////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Basics/operating-system.h"

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <algorithm>
#include <atomic>
#include <exception>
#include <thread>

#include <boost/core/demangle.hpp>

#include "CrashHandler.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/files.h"
#include "Basics/operating-system.h"
#include "Basics/process-utils.h"
#include "Basics/signals.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/Version.h"

#ifdef ARANGODB_HAVE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#ifdef __linux__
#include <sys/auxv.h>
#include <elf.h>
#endif

namespace {

#ifdef _WIN32

// forward-declare a pseudo struct so the following code compiles
struct siginfo_t;

#else

// memory reserved for the signal handler stack
std::unique_ptr<char[]> alternativeStackMemory;

/// @brief an atomic that makes sure there are no races inside the signal
/// handler callback
std::atomic<bool> crashHandlerInvoked(false);
#endif

/// @brief an atomic that controls whether we will log backtraces 
/// (default: yes on Linux, false elsewhere) or not
std::atomic<bool> enableStacktraces(true);

/// @brief kill process hard using SIGKILL, in order to circumvent core
/// file generation etc.
std::atomic<bool> killHard(false);

/// @brief kills the process with the given signal
[[noreturn]] void killProcess(int signal) {
#ifdef _WIN32
  if (::killHard.load(std::memory_order_relaxed)) {
    auto hSelf = GetCurrentProcess();
    TerminateProcess(hSelf, -999);
    // TerminateProcess is async, alright wait here for selfdestruct (we will never exit wait)
    WaitForSingleObject(hSelf, INFINITE);
  } else {
    exit(255 + signal);
  }
#else
  if (::killHard.load(std::memory_order_relaxed)) {
    kill(getpid(), SIGKILL);  //to kill the complete process tree.
    std::this_thread::sleep_for(std::chrono::seconds(5));
  } else {
    // restore default signal action, so that we can write a core dump and crash "properly"
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_RESETHAND | (alternativeStackMemory != nullptr ? SA_ONSTACK : 0);
    act.sa_handler = SIG_DFL;
    sigaction(signal, &act, nullptr);

    // resend signal to ourselves to invoke default action for the signal (e.g. coredump)
    kill(::getpid(), signal);
  }
#endif
  
  std::abort();
}

/// @brief appends null-terminated string src to dst,
/// advances dst pointer by length of src
void appendNullTerminatedString(char const* src, char*& dst) {
  size_t len = strlen(src);
  memcpy(static_cast<void*>(dst), src, len);
  dst += len;
  *dst = '\0';
}

void appendNullTerminatedString(char const* src, size_t maxLength, char*& dst) {
  size_t len = std::min(strlen(src), maxLength);
  memcpy(static_cast<void*>(dst), src, len);
  dst += len;
  *dst = '\0';
}

/// @brief appends null-terminated hex string value to dst
/// advances dst pointer by at most len * 2.
/// if stripLeadingZeros is true, omits all leading zero characters. 
/// If the value is 0x0 itself, prints one zero character.
void appendHexValue(unsigned char const* src, size_t len, char*& dst, bool stripLeadingZeros) {
  char chars[] = "0123456789abcdef";
  unsigned char const* e = src + len;
  while (--e >= src) {
    unsigned char c = *e;
    if (!stripLeadingZeros || (c >> 4U) != 0) {
      *dst++ = chars[c >> 4U];
      stripLeadingZeros = false; 
    }
    if (!stripLeadingZeros || (c & 0xfU) != 0) {
      *dst++ = chars[c & 0xfU];
      stripLeadingZeros = false; 
    }
  }
  if (stripLeadingZeros) {
    *dst++ = '0';
  }
}

#ifdef ARANGODB_HAVE_LIBUNWIND
void appendAddress(unw_word_t pc, long base, char*& dst) {
  if (base == 0) {
    // absolute address of pc
    appendNullTerminatedString(" [$0x", dst);
    appendHexValue(reinterpret_cast<unsigned char const*>(&pc), sizeof(decltype(pc)), dst, false);
  } else {
    // relative offset of pc
    appendNullTerminatedString(" [+0x", dst);
    decltype(pc) relative = pc - base; 
    unsigned char const* s = reinterpret_cast<unsigned char const*>(&relative);
    appendHexValue(s, sizeof(relative), dst, false);
  }
  appendNullTerminatedString("] ", dst);
}
#endif

/// @brief builds a log message to be logged to the logfile later
/// does not allocate any memory, so should be safe to call even
/// in context of SIGSEGV, with a broken heap etc.
/// Assumes that the buffer pointed to by s has enough space to
/// hold the thread id, the thread name and the signal name
/// (4096 bytes should be more than enough).
size_t buildLogMessage(char* s, char const* context, int signal, siginfo_t const* info, void* ucontext) {
  // build a crash message
  char* p = s;
  appendNullTerminatedString("ArangoDB ", p);
  appendNullTerminatedString(ARANGODB_VERSION_FULL, p);
  appendNullTerminatedString(", thread ", p);
  p += arangodb::basics::StringUtils::itoa(uint64_t(arangodb::Thread::currentThreadNumber()), p);
  
#ifdef __linux__
  char const* name = arangodb::Thread::currentThreadName();
#else
  char const* name = nullptr;
#endif
  if (name != nullptr && *name != '\0') {
    appendNullTerminatedString(" [", p);
    appendNullTerminatedString(name, p);
    appendNullTerminatedString("]", p);
  }

  appendNullTerminatedString(" caught unexpected signal ", p);
  p += arangodb::basics::StringUtils::itoa(uint64_t(signal), p);
  appendNullTerminatedString(" (", p);
  appendNullTerminatedString(arangodb::signals::name(signal), p);
  appendNullTerminatedString(")", p);
  
#ifndef _WIN32
  if (info != nullptr && 
      (signal == SIGSEGV || signal == SIGBUS)) {
    // dump address that was accessed when the failure occurred (this is somewhat likely
    // a nullptr)
    appendNullTerminatedString(" accessing address 0x", p);
    unsigned char const* x = reinterpret_cast<unsigned char const*>(info->si_addr);
    unsigned char const* s = reinterpret_cast<unsigned char const*>(&x);
    appendHexValue(s, sizeof(unsigned char const*), p, false);
  }
#endif
  
  appendNullTerminatedString(": ", p);
  appendNullTerminatedString(context, p);

#ifdef __linux__
  {
    // AT_PHDR points to the program header, which is located after the ELF header.
    // This allows us to calculate the base address of the executable.
    auto baseAddr = getauxval(AT_PHDR) - sizeof(Elf64_Ehdr);
    appendNullTerminatedString(" - image base address: 0x", p);
    unsigned char const* x = reinterpret_cast<unsigned char const*>(baseAddr);
    unsigned char const* s = reinterpret_cast<unsigned char const*>(&x);
    appendHexValue(s, sizeof(unsigned char const*), p, false);
  }

  auto ctx = static_cast<ucontext_t*>(ucontext);
  if (ctx) {
    auto appendRegister = [ctx, &p](const char* prefix, int reg) {
      appendNullTerminatedString(prefix, p);
      unsigned char const* s = reinterpret_cast<unsigned char const*>(&ctx->uc_mcontext.gregs[reg]);
      appendHexValue(s, sizeof(greg_t), p, false);
    };
    appendNullTerminatedString(" - CPU context:", p);
    appendRegister(" rip: 0x", REG_RIP);
    appendRegister(", rsp: 0x", REG_RSP);
    appendRegister(", efl: 0x", REG_EFL);
    appendRegister(", rbp: 0x", REG_RBP);
    appendRegister(", rsi: 0x", REG_RSI);
    appendRegister(", rdi: 0x", REG_RDI);
    appendRegister(", rax: 0x", REG_RAX);
    appendRegister(", rbx: 0x", REG_RBX);
    appendRegister(", rcx: 0x", REG_RCX);
    appendRegister(", rdx: 0x", REG_RDX);    
    appendRegister(", r8: 0x", REG_R8);
    appendRegister(", r9: 0x", REG_R9);
    appendRegister(", r10: 0x", REG_R10);
    appendRegister(", r11: 0x", REG_R11);
    appendRegister(", r12: 0x", REG_R12);
    appendRegister(", r13: 0x", REG_R13);
    appendRegister(", r14: 0x", REG_R14);
    appendRegister(", r15: 0x", REG_R15);
  }
#endif

  return p - s;
}

void logBacktrace(char const* context, int signal, siginfo_t* info, void* ucontext) try {
  // buffer for constructing temporary log messages (to avoid malloc)
  char buffer[4096];
  memset(&buffer[0], 0, sizeof(buffer));

  char* p = &buffer[0];
  size_t length = buildLogMessage(p, context, signal, info, ucontext);
  // note: LOG_TOPIC() can allocate memory
  LOG_TOPIC("a7902", FATAL, arangodb::Logger::CRASH) << arangodb::Logger::CHARS(&buffer[0], length);

  if (!enableStacktraces.load(std::memory_order_relaxed)) {
    return;
  }
   
#ifdef ARANGODB_HAVE_LIBUNWIND
  // log backtrace, of up to maxFrames depth 
  { 
#ifdef __linux__
  // The address of the program headers of the executable.
    long base = getauxval(AT_PHDR) - sizeof(Elf64_Ehdr);
#else
    long base = 0;
#endif

    unw_cursor_t cursor;
    // unw_word_t ip, sp;
    unw_context_t uc;

    // the following call is recommended by libunwind's manual when the
    // calling function is known to be a signal handler. but on x86_64,
    // the code in libunwind is no different if the flag is set or not.
    //  unw_init_local2(&cursor, &uc, UNW_INIT_SIGNAL_FRAME);
    if (unw_getcontext(&uc) == 0 && unw_init_local(&cursor, &uc) == 0) {
      // unwind frames one by one, going up the frame stack.
    
      // number of frames to skip in backtrace output
      static constexpr int skipFrames = 1;
      // maximum number of stack traces to show
      static constexpr int maxFrames = 50;

      // current frame counter
      int frame = 0;

      do {
        unw_word_t pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0) {
          break;
        }

        if (frame >= skipFrames) {
          // this is a stack frame we want to display
          memset(&buffer[0], 0, sizeof(buffer));
          p = &buffer[0];
          appendNullTerminatedString("frame ", p);
          if (frame < 10) {
            // pad frame id to 2 digits length
            *p++ = ' ';
          }
          p += arangodb::basics::StringUtils::itoa(uint64_t(frame), p);

          appendAddress(pc, base, p);

          char mangled[512];
          memset(&mangled[0], 0, sizeof(mangled));
          
          // get symbol information (in mangled format)
          unw_word_t offset = 0;
          if (unw_get_proc_name(&cursor, &mangled[0], sizeof(mangled) - 1, &offset) == 0) {
            // "mangled" buffer must have been null-terminated before, but it doesn't
            // harm if we double-check it is null-terminated
            mangled[sizeof(mangled) - 1] = '\0';
          
            boost::core::scoped_demangled_name demangled(&mangled[0]);

            if (demangled.get()) {
              appendNullTerminatedString(demangled.get(), p); 
            } else {
              // demangling has failed.
              // in this case, append mangled version. still better than nothing
              appendNullTerminatedString(mangled, p);
            }
            // print offset into function
            appendNullTerminatedString(" (+0x", p); 
            appendHexValue(reinterpret_cast<unsigned char const*>(&offset), sizeof(decltype(offset)), p, true);
            appendNullTerminatedString(")", p); 
          } else {
            // unable to retrieve symbol information
            appendNullTerminatedString("*no symbol name available for this frame", p); 
          }

          length = p - &buffer[0];
          LOG_TOPIC("308c3", INFO, arangodb::Logger::CRASH) << arangodb::Logger::CHARS(&buffer[0], length);
        }
      } while (++frame < (maxFrames + skipFrames) && unw_step(&cursor) > 0);
      // flush logs as early as possible
      arangodb::Logger::flush();
    }
  }
#endif

  // log info about the current process
  {
    auto processInfo = TRI_ProcessInfoSelf();
    memset(&buffer[0], 0, sizeof(buffer));
    p = &buffer[0];
    appendNullTerminatedString("available physical memory: ", p);
    p += arangodb::basics::StringUtils::itoa(arangodb::PhysicalMemory::getValue(), p);
    appendNullTerminatedString(", rss usage: ", p);
    p += arangodb::basics::StringUtils::itoa(uint64_t(processInfo._residentSize), p);
    appendNullTerminatedString(", vsz usage: ", p);
    p += arangodb::basics::StringUtils::itoa(uint64_t(processInfo._virtualSize), p);
    appendNullTerminatedString(", threads: ", p);
    p += arangodb::basics::StringUtils::itoa(uint64_t(processInfo._numberThreads), p);

    LOG_TOPIC("ded81", INFO, arangodb::Logger::CRASH) << arangodb::Logger::CHARS(&buffer[0], p - &buffer[0]);
  }
} catch (...) {
  // we better not throw an exception from inside a signal handler
}

/// @brief Logs the reception of a signal to the logfile.
/// this is the actual function that is invoked for a deadly signal
/// (i.e. SIGSEGV, SIGBUS, SIGILL, SIGFPE...)
///
/// the following assumptions are made for this crash handler:
/// - it is invoked in fatal situations only, that we need as much information as possible
///   about. thus we try logging some information into the ArangoDB logfile. Our logger is
///   not async-safe right now, but everything in our own log message-building routine
///   should be async-safe.
///   in case of a corrupted heap/stack all this will fall apart. However, it is better to
///   try using our logger than doing nothing, or writing somewhere else nobody will see
///   the information later.
/// - the interesting signals are delivered from the same thread that caused them. Thus we 
///   will have a few stack frames of the offending thread available.
/// - it is not possible to generate the stack traces from other threads without substantial
///   efforts, so we are not even trying this.
/// - Windows and macOS are currently not supported.
#ifndef _WIN32
void crashHandlerSignalHandler(int signal, siginfo_t* info, void* ucontext) {
  if (!::crashHandlerInvoked.exchange(true)) {
    ::logBacktrace("signal handler invoked", signal, info, ucontext);
    arangodb::Logger::flush();
    arangodb::Logger::shutdown();
  } else {
    // signal handler was already entered by another thread...
    // there is not so much we can do here except waiting and then finally let it crash
    
    // alternatively, we can get if the current thread has received the signal, invoked the 
    // signal handler and while being in there, caught yet another signal.
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }

  killProcess(signal);
}
#endif

} // namespace

namespace arangodb {

/// @brief logs a fatal message and crashes the program
void CrashHandler::crash(char const* context) {
  ::logBacktrace(context, SIGABRT, /*no signal*/ nullptr, /*no context*/ nullptr);
  Logger::flush();
  Logger::shutdown();

  // crash from here
  ::killProcess(SIGABRT);
}

/// @brief logs an assertion failure and crashes the program
void CrashHandler::assertionFailure(char const* file, int line, char const* context) {
  // assemble an "assertion failured in file:line: message" string
  char buffer[4096];
  memset(&buffer[0], 0, sizeof(buffer));
  
  char* p = &buffer[0];
  appendNullTerminatedString("assertion failed in ", p);
  appendNullTerminatedString((file == nullptr ? "unknown file" : file), 128, p);
  appendNullTerminatedString(":", p);
  p += arangodb::basics::StringUtils::itoa(uint64_t(line), p);
  appendNullTerminatedString(": ", p);
  appendNullTerminatedString(context, 256, p);

  crash(&buffer[0]);
}

/// @brief set flag to kill process hard using SIGKILL, in order to circumvent core
/// file generation etc.
void CrashHandler::setHardKill() {
  ::killHard.store(true, std::memory_order_relaxed);
}

/// @brief disable printing of backtraces
void CrashHandler::disableBacktraces() {
  ::enableStacktraces.store(false, std::memory_order_relaxed);
}

/// @brief installs the crash handler globally
void CrashHandler::installCrashHandler() {
  // read environment variable that can be used to toggle the
  // crash handler
  std::string value;
  if (TRI_GETENV("ARANGODB_OVERRIDE_CRASH_HANDLER", value)) {
    bool toggle = arangodb::basics::StringUtils::boolean(value);
    if (!toggle) {
      // crash handler backtraces turned off
      disableBacktraces();
      // additionally, do not install signal handler nor the
      // handler for std::terminate()
      return;
    }
  }

#ifndef _WIN32
  try {
    constexpr size_t stackSize = std::max<size_t>(
        128 * 1024, 
        std::max<size_t>(
          MINSIGSTKSZ, 
          SIGSTKSZ
        )
      );

    ::alternativeStackMemory = std::make_unique<char[]>(stackSize);
    
    stack_t altstack;
    altstack.ss_sp = static_cast<void*>(::alternativeStackMemory.get());
    altstack.ss_size = stackSize;
    altstack.ss_flags = 0;
    if (sigaltstack(&altstack, nullptr) != 0) {
      ::alternativeStackMemory.release();
    }
  } catch (...) {
    // could not allocate memory for alternative stack.
    // in this case we must not modify the stack for the signal handler
    // as we have no way of signaling failure here.
    ::alternativeStackMemory.release();
  }

  // install signal handlers for the following signals
  struct sigaction act;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO | (::alternativeStackMemory != nullptr ? SA_ONSTACK : 0);
  act.sa_sigaction = crashHandlerSignalHandler;
  sigaction(SIGSEGV, &act,nullptr);
  sigaction(SIGBUS, &act, nullptr);
  sigaction(SIGILL, &act, nullptr);
  sigaction(SIGFPE, &act, nullptr);
  sigaction(SIGABRT, &act,nullptr);
#endif

  // install handler for std::terminate()
  std::set_terminate([]() {
    char buffer[256];
    memset(&buffer[0], 0, sizeof(buffer));
    char* p = &buffer[0];

    if (auto ex = std::current_exception()) {
      // there is an active exception going on...
      try {
        // rethrow so we can get the exception type and its message
        std::rethrow_exception(ex);
      } catch (std::exception const& ex) {
        char const* msg = "handler for std::terminate() invoked with an std::exception: ";
        appendNullTerminatedString(msg, p);
        char const* e = ex.what();
        if (e != nullptr) {
          if (strlen(e) > 100) {
            memcpy(p, e, 100);
            p += 100;
            appendNullTerminatedString(" (truncated)", p);
          } else {
            appendNullTerminatedString(e, p);
          }
        }
      } catch (...) {
        char const* msg = "handler for std::terminate() invoked with an unknown exception";
        appendNullTerminatedString(msg, p);
      }
    } else {
      char const* msg = "handler for std::terminate() invoked without active exception";
      appendNullTerminatedString(msg, p);
    }

    CrashHandler::crash(&buffer[0]);
  });
}

}  // namespace arangodb
