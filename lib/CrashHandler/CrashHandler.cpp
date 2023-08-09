////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#ifdef _WIN32
#include <DbgHelp.h>
#endif

#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <string_view>
#include <thread>

#include <boost/core/demangle.hpp>

#include "CrashHandler.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/SizeLimitedString.h"
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

using SmallString = arangodb::SizeLimitedString<4096>;

#ifdef _WIN32

// forward-declare a pseudo struct so the following code compiles
struct siginfo_t;

void createMiniDump(EXCEPTION_POINTERS* pointers);

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

/// @brief string with server state information. must be null-terminated
std::atomic<char const*> stateString = nullptr;

/// @brief kills the process with the given signal
[[noreturn]] void killProcess(int signal) {
#ifdef _WIN32
  if (::killHard.load(std::memory_order_relaxed)) {
    auto hSelf = GetCurrentProcess();
    TerminateProcess(hSelf, -999);
    // TerminateProcess is async, alright wait here for selfdestruct (we will
    // never exit wait)
    WaitForSingleObject(hSelf, INFINITE);
  } else {
    // exit will not trigger dump creation. So do this manually.
    if (SIGABRT == signal) {
      SetUnhandledExceptionFilter(NULL);
      // produce intentional segfault to trigger WER (and attached debugger if
      // any)
      *static_cast<volatile int*>(nullptr) = 1;
    }
    exit(255 + signal);
  }
#else
  if (::killHard.load(std::memory_order_relaxed)) {
    kill(getpid(), SIGKILL);  // to kill the complete process tree.
    std::this_thread::sleep_for(std::chrono::seconds(5));
  } else {
    // restore default signal action, so that we can write a core dump and crash
    // "properly"
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_RESETHAND |
                   (alternativeStackMemory != nullptr ? SA_ONSTACK : 0);
    act.sa_handler = SIG_DFL;
    sigaction(signal, &act, nullptr);

    // resend signal to ourselves to invoke default action for the signal (e.g.
    // coredump)
    kill(::getpid(), signal);
  }
#endif

  std::abort();
}

#ifdef ARANGODB_HAVE_LIBUNWIND
void appendAddress(SmallString& dst, unw_word_t pc, long base) {
  if (base == 0) {
    // absolute address of pc
    dst.append(" [$0x").appendHexValue(pc, false).append("] ");
  } else {
    // relative offset of pc
    decltype(pc) relative = pc - base;
    dst.append(" [+0x").appendHexValue(relative, false).append("] ");
  }
}
#endif

/// @brief builds a log message to be logged to the logfile later
/// does not allocate any memory, so should be safe to call even
/// in context of SIGSEGV, with a broken heap etc.
void buildLogMessage(SmallString& buffer, std::string_view context, int signal,
                     siginfo_t const* info, void* ucontext) {
  // build a crash message
  buffer.append("💥 ArangoDB ").append(ARANGODB_VERSION_FULL);
  buffer.append(", thread ")
      .appendUInt64(uint64_t(arangodb::Thread::currentThreadNumber()));

#ifdef __linux__
  arangodb::ThreadNameFetcher nameFetcher;
  buffer.append(" [").append(nameFetcher.get()).append("]");
#endif

  bool printed = false;
  buffer.append(" caught unexpected signal ").appendUInt64(uint64_t(signal));
  buffer.append(" (").append(arangodb::signals::name(signal));
#ifndef _WIN32
  if (info != nullptr) {
    buffer.append(") from pid ").appendUInt64(uint64_t(info->si_pid));
    printed = true;
  }
#endif
  if (!printed) {
    buffer.append(")");
  }

  if (char const* ss = ::stateString.load(); ss != nullptr) {
    buffer.append(" in state \"").append(ss).append("\"");
  }

#ifndef _WIN32
  if (info != nullptr && (signal == SIGSEGV || signal == SIGBUS)) {
    // dump address that was accessed when the failure occurred (this is
    // somewhat likely a nullptr)
    buffer.append(" accessing address 0x").appendHexValue(info->si_addr, false);
  }
#endif

  buffer.append(": ").append(context);

#ifdef __linux__
  {
    // AT_PHDR points to the program header, which is located after the ELF
    // header. This allows us to calculate the base address of the executable.
    auto baseAddr = getauxval(AT_PHDR) - sizeof(Elf64_Ehdr);
    buffer.append(" - image base address: 0x").appendHexValue(baseAddr, false);
  }
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  // FIXME: implement ARM64 context output
  buffer.append(" ARM64 CPU context: is not available ");
#else
  if (auto ctx = static_cast<ucontext_t*>(ucontext); ctx) {
    auto appendRegister = [ctx, &buffer](char const* prefix, int reg) {
      buffer.append(prefix).appendHexValue(ctx->uc_mcontext.gregs[reg], false);
    };
    buffer.append(" - CPU context:");
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
#endif
}

void logCrashInfo(std::string_view context, int signal, siginfo_t* info,
                  void* ucontext) try {
  // fixed buffer for constructing temporary log messages (to avoid malloc)
  SmallString buffer;

  buildLogMessage(buffer, context, signal, info, ucontext);
  // note: LOG_TOPIC() can allocate memory
  LOG_TOPIC("a7902", FATAL, arangodb::Logger::CRASH) << buffer.view();
} catch (...) {
  // we better not throw an exception from inside a signal handler
}

void logBacktrace() try {
  if (!enableStacktraces.load(std::memory_order_relaxed)) {
    return;
  }

  arangodb::ThreadNameFetcher nameFetcher;
  std::string_view currentThreadName = nameFetcher.get();
  if (currentThreadName == arangodb::Logger::logThreadName) {
    // we must not log a backtrace from the logging thread itself. if we would
    // do, we may cause a deadlock
    return;
  }

#ifdef ARANGODB_HAVE_LIBUNWIND
  // fixed buffer for constructing temporary log messages (to avoid malloc)
  SmallString buffer;

  buffer.append("Backtrace of thread ");
  buffer.appendUInt64(arangodb::Thread::currentThreadNumber());
  buffer.append(" [").append(currentThreadName).append("]");

  LOG_TOPIC("c962b", INFO, arangodb::Logger::CRASH) << buffer.view();

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

        if (frame == maxFrames + skipFrames) {
          buffer.clear();
          buffer.append("..reached maximum frame display depth (");
          buffer.appendUInt64(maxFrames);
          buffer.append("). stopping backtrace");

          LOG_TOPIC("bbb04", INFO, arangodb::Logger::CRASH) << buffer.view();
          break;
        }

        if (frame >= skipFrames) {
          // this is a stack frame we want to display
          buffer.clear();
          buffer.append("frame ");
          if (frame < 10) {
            // pad frame id to 2 digits length
            buffer.push_back(' ');
          }
          buffer.appendUInt64(uint64_t(frame));

          appendAddress(buffer, pc, base);

          char mangled[512];
          memset(&mangled[0], 0, sizeof(mangled));

          // get symbol information (in mangled format)
          unw_word_t offset = 0;
          if (unw_get_proc_name(&cursor, &mangled[0], sizeof(mangled) - 1,
                                &offset) == 0) {
            // "mangled" buffer must have been null-terminated before, but it
            // doesn't harm if we double-check it is null-terminated
            mangled[sizeof(mangled) - 1] = '\0';

            boost::core::scoped_demangled_name demangled(&mangled[0]);

            if (demangled.get()) {
              buffer.append(demangled.get());
            } else {
              // demangling has failed.
              // in this case, append mangled version. still better than nothing
              buffer.append(mangled);
            }
            // print offset into function
            buffer.append(" (+0x").appendHexValue(offset, true).append(")");
          } else {
            // unable to retrieve symbol information
            buffer.append("*no symbol name available for this frame");
          }

          LOG_TOPIC("308c3", INFO, arangodb::Logger::CRASH) << buffer.view();
        }
      } while (++frame < (maxFrames + skipFrames + 1) && unw_step(&cursor) > 0);
      // flush logs as early as possible
      arangodb::Logger::flush();
    }
  }

#endif
} catch (...) {
  // we better not throw an exception from inside a signal handler
}

// log info about the current process
void logProcessInfo() {
  auto processInfo = TRI_ProcessInfoSelf();

  // fixed buffer for constructing temporary log messages (to avoid malloc)
  SmallString buffer;
  buffer.append("available physical memory: ")
      .appendUInt64(uint64_t(arangodb::PhysicalMemory::getValue()));
  buffer.append(", rss usage: ")
      .appendUInt64(uint64_t(processInfo._residentSize));
  buffer.append(", vsz usage: ")
      .appendUInt64(uint64_t(processInfo._virtualSize));
  buffer.append(", threads: ")
      .appendUInt64(uint64_t(processInfo._numberThreads));

  LOG_TOPIC("ded81", INFO, arangodb::Logger::CRASH) << buffer.view();
}

/// @brief Logs the reception of a signal to the logfile.
/// this is the actual function that is invoked for a deadly signal
/// (i.e. SIGSEGV, SIGBUS, SIGILL, SIGFPE...)
///
/// the following assumptions are made for this crash handler:
/// - it is invoked in fatal situations only, that we need as much information
/// as possible
///   about. thus we try logging some information into the ArangoDB logfile. Our
///   logger is not async-safe right now, but everything in our own log
///   message-building routine should be async-safe. in case of a corrupted
///   heap/stack all this will fall apart. However, it is better to try using
///   our logger than doing nothing, or writing somewhere else nobody will see
///   the information later.
/// - the interesting signals are delivered from the same thread that caused
/// them. Thus we
///   will have a few stack frames of the offending thread available.
/// - it is not possible to generate the stack traces from other threads without
/// substantial
///   efforts, so we are not even trying this.
/// - Windows and macOS are currently not supported.
#ifndef _WIN32
void crashHandlerSignalHandler(int signal, siginfo_t* info, void* ucontext) {
  if (!::crashHandlerInvoked.exchange(true)) {
    logCrashInfo("signal handler invoked", signal, info, ucontext);
    logBacktrace();
    logProcessInfo();
    arangodb::Logger::flush();
    arangodb::Logger::shutdown();
  } else {
    // signal handler was already entered by another thread...
    // there is not so much we can do here except waiting and then finally let
    // it crash

    // alternatively, we can get if the current thread has received the signal,
    // invoked the signal handler and while being in there, caught yet another
    // signal.
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }

  killProcess(signal);
}
#endif

#ifdef _WIN32

static std::string miniDumpDirectory = "C:\\temp";
static std::mutex miniDumpLock;

// TODO - ATM if these are put inside the function MSVC complains that they
// are not captured in the lambda, but a lamba with captures cannot convert
// to a function pointer. Since these are constexpr is should not be necessary
// to capture them, and with C++20 enabled MSVC no longer complains.
// So once we have upgraded to C++20 these should be move into the function.
constexpr DWORD64 blockSize = 1024;  // 1kb
constexpr DWORD64 maxStackAddrs = 2048;
constexpr DWORD maxNumAddrs = 160000;
constexpr DWORD numRegs = 16;

void createMiniDump(EXCEPTION_POINTERS* pointers) {
  // we have to serialize calls to MiniDumpWriteDump
  std::lock_guard<std::mutex> lock(miniDumpLock);

  SmallString buffer;

  time_t rawtime;
  time(&rawtime);
  struct tm timeinfo;
  _gmtime64_s(&timeinfo, &rawtime);
  char time[20];
  strftime(time, sizeof(time), "%Y-%m-%dT%H-%M-%S", &timeinfo);

  char filename[MAX_PATH];
  _snprintf_s(filename, sizeof(filename), "%s\\%s_%d_%d.dmp",
              miniDumpDirectory.c_str(), time, GetCurrentProcessId(),
              GetCurrentThreadId());
  HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile == INVALID_HANDLE_VALUE) {
    buffer.append("Could not open minidump file: ");
    buffer.appendUInt64(GetLastError());
    LOG_TOPIC("ba80e", WARN, arangodb::Logger::CRASH) << buffer.view();
    return;
  }

  MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
  exceptionInfo.ThreadId = GetCurrentThreadId();
  exceptionInfo.ExceptionPointers = pointers;
  exceptionInfo.ClientPointers = FALSE;

  // We try to gather some additional information from referenced memory
  // In total we gather up to 16000 memory blocks of 1kb each.
  // We consider only addresses that reference some memory block that can
  // actually be read (-> !IsBadReadPtr).

  // we want to have enough addresses to cover all 16 registers plus all
  // indirections and all maxStackAddrs stack addresses
  static_assert(maxNumAddrs >
                maxStackAddrs + numRegs * (blockSize / sizeof(void*)));

  DWORD64 addrs[maxNumAddrs];
  DWORD numAddrs = 0;

  if (pointers) {
    auto addAddr = [&addrs, &numAddrs](DWORD64 reg) {
      auto base = reg & ~(blockSize - 1);
      if (base == 0 || IsBadReadPtr((void*)base, blockSize) ||
          numAddrs >= maxNumAddrs) {
        return;
      }
      for (DWORD i = 0; i < numAddrs; ++i) {
        if (addrs[i] == base) {
          return;
        }
      }
      addrs[numAddrs++] = base;
    };

    // we take the values of all general purpose registers
    auto& ctx = *pointers->ContextRecord;
    addAddr(ctx.Rax);
    addAddr(ctx.Rcx);
    addAddr(ctx.Rdx);
    addAddr(ctx.Rbx);
    addAddr(ctx.Rsp);
    addAddr(ctx.Rbp);
    addAddr(ctx.Rsi);
    addAddr(ctx.Rdi);
    addAddr(ctx.R8);
    addAddr(ctx.R9);
    addAddr(ctx.R10);
    addAddr(ctx.R11);
    addAddr(ctx.R12);
    addAddr(ctx.R13);
    addAddr(ctx.R14);
    addAddr(ctx.R15);
    TRI_ASSERT(numAddrs <= numRegs);

    // Take the first 2048 pointers from the stack and add them to the address
    // list. We use the thread information block (TIB) to get the base address
    // of the stack to handle the (unlikely) cases where the stack has less than
    // 2048 items.
    auto* tib = (PNT_TIB)NtCurrentTeb();
    auto numStackAddrs = std::min(
        ((DWORD64)tib->StackBase - ctx.Rsp) / sizeof(void*), maxStackAddrs);
    void** p = (void**)ctx.Rsp;
    for (DWORD64 i = 0; i < numStackAddrs; ++i) {
      addAddr((DWORD64)p[i]);
    }

    // Now we take all the addresses we gathered so far and add all indirect
    // addresses, i.e., we take each 1024 byte block and add all 128 potential
    // addresses from that block (as long as we don't exceed our limit). That
    // way can follow at least one level of indirection when analyzing the dump.
    DWORD idx = numAddrs;
    do {
      --idx;
      void** p = (void**)addrs[idx];
      for (DWORD i = 0; i < blockSize / sizeof(void*) && numAddrs < maxNumAddrs;
           ++i) {
        auto base = (DWORD64)p[i] & ~(blockSize - 1);
        if (base != 0) {
          addrs[numAddrs++] = base;
        }
      }
    } while (idx != 0 && numAddrs < maxNumAddrs);
  }

  struct CallbackParam {
    DWORD64* addrs;
    DWORD idx;
    DWORD numAddrs;
  };
  CallbackParam param{addrs, 0, numAddrs};

  auto callback = [](PVOID callbackParam,
                     PMINIDUMP_CALLBACK_INPUT callbackInput,
                     PMINIDUMP_CALLBACK_OUTPUT callbackOutput) -> BOOL {
    auto* param = static_cast<CallbackParam*>(callbackParam);
    if (callbackInput->CallbackType == MemoryCallback &&
        param->idx < param->numAddrs) {
      callbackOutput->MemoryBase = param->addrs[param->idx];
      callbackOutput->MemorySize = blockSize;
      ++param->idx;
    }
    return TRUE;
  };

  MINIDUMP_CALLBACK_INFORMATION callbackInfo{callback, &param};

  buffer.clear();
  if (MiniDumpWriteDump(
          GetCurrentProcess(), GetCurrentProcessId(), hFile,
          MINIDUMP_TYPE(MiniDumpNormal | MiniDumpWithProcessThreadData |
                        MiniDumpWithDataSegs |
                        MiniDumpIgnoreInaccessibleMemory),
          pointers ? &exceptionInfo : nullptr, nullptr,
          pointers ? &callbackInfo : nullptr)) {
    buffer.append("Wrote minidump: ");
    buffer.append(filename);
    LOG_TOPIC("93315", INFO, arangodb::Logger::CRASH) << buffer.view();
  } else {
    buffer.append("Failed to write minidump: ");
    buffer.appendUInt64(GetLastError());
    LOG_TOPIC("af06b", WARN, arangodb::Logger::CRASH) << buffer.view();
  }

  CloseHandle(hFile);
}

LONG CALLBACK unhandledExceptionFilter(EXCEPTION_POINTERS* pointers) {
  TRI_ASSERT(pointers && pointers->ExceptionRecord);

  SmallString buffer;
  buffer.append("Unhandled exception: ");
  buffer.appendHexValue(pointers->ExceptionRecord->ExceptionCode, false);
  buffer.append(" at address ");
  buffer.appendHexValue(pointers->ContextRecord->Rip, false);
  buffer.append(" in thread ");
  buffer.appendHexValue(GetCurrentThreadId(), true);

  LOG_TOPIC("87ff4", INFO, arangodb::Logger::CRASH) << buffer.view();

  createMiniDump(pointers);
  return EXCEPTION_CONTINUE_SEARCH;
}
#endif

}  // namespace

namespace arangodb {

void CrashHandler::logBacktrace() {
  ::logBacktrace();
  Logger::flush();
}

/// @brief logs a fatal message and crashes the program
void CrashHandler::crash(std::string_view context) {
  ::logCrashInfo(context, SIGABRT, /*no signal*/ nullptr,
                 /*no context*/ nullptr);
  ::logBacktrace();
  ::logProcessInfo();
  Logger::flush();
  Logger::shutdown();

  // crash from here
  ::killProcess(SIGABRT);
}

void CrashHandler::setState(std::string_view state) {
  ::stateString.store(state.data());
}

/// @brief logs an assertion failure and crashes the program
[[noreturn]] void CrashHandler::assertionFailure(char const* file, int line,
                                                 char const* func,
                                                 char const* context,
                                                 char const* message) {
  // assemble an "assertion failured in file:line: message" string
  SmallString buffer;

  buffer.append("assertion failed in ")
      .append(file == nullptr ? "unknown file" : file)
      .append(":");
  buffer.appendUInt64(uint64_t(line));
  if (func != nullptr) {
    buffer.append(" [").append(func).append("]");
  }
  buffer.append(": ");
  buffer.append(context);
  if (message != nullptr) {
    buffer.append(" ; ").append(message);
  }

  crash(buffer.view());
}

/// @brief set flag to kill process hard using SIGKILL, in order to circumvent
/// core file generation etc.
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
    size_t const stackSize =
        std::max<size_t>(128 * 1024, std::max<size_t>(MINSIGSTKSZ, SIGSTKSZ));

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
  act.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO |
                 (::alternativeStackMemory != nullptr ? SA_ONSTACK : 0);
  act.sa_sigaction = crashHandlerSignalHandler;
  sigaction(SIGSEGV, &act, nullptr);
  sigaction(SIGBUS, &act, nullptr);
  sigaction(SIGILL, &act, nullptr);
  sigaction(SIGFPE, &act, nullptr);
  sigaction(SIGABRT, &act, nullptr);
#else  // _WIN32
  SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif

  // install handler for std::terminate()
  std::set_terminate([]() {
    SmallString buffer;

    if (auto ex = std::current_exception()) {
      // there is an active exception going on...
      try {
        // rethrow so we can get the exception type and its message
        std::rethrow_exception(ex);
      } catch (std::exception const& ex) {
        buffer.append(
            "handler for std::terminate() invoked with an std::exception: ");
        char const* e = ex.what();
        if (e != nullptr) {
          buffer.append(e);
        }
      } catch (...) {
        buffer.append(
            "handler for std::terminate() invoked with an unknown exception");
      }
    } else {
      buffer.append(
          "handler for std::terminate() invoked without active exception");
    }

    CrashHandler::crash(buffer.view());
  });
}

#ifdef _WIN32
void CrashHandler::setMiniDumpDirectory(std::string path) {
  ::miniDumpDirectory = std::move(path);
}
#endif

}  // namespace arangodb
