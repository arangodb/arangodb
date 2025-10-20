////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include <chrono>
#include <exception>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>

#include <boost/core/demangle.hpp>

#include "CrashHandler.h"
#include "BuildId/BuildId.h"
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

#include <sys/auxv.h>
#include <elf.h>

namespace {

// The CrashHandler is a singleton in the process. We collect all its state
// in this anonymous namespace. It needs to be static for use in the signal
// handlers and the std::terminate handler. There is a single class
// CrashHandler instantiated in `main` on the stack, but this only controls
// initialization and destruction. There is an additional destruction
// pathway in an atexit and an at_quick_exit handler, since in some situations
// we exit the process in this way, for example if the command line option
// --version was given.
// The crash handler runs a dedicated thread, which is started on
// initialization and joined on shutdown. This is to allow for more
// complex crash dumping things, which are not allowed in signal handlers.
// We use C++20 atomic wait/notify_all to communicate between signal handlers
// and the dedicated CrashHandler thread.

using SmallString = arangodb::SizeLimitedString<4096>;

// memory reserved for the signal handler stack
std::unique_ptr<char[]> alternativeStackMemory;

// preallocated buffer for storing backtrace data from signal handler
std::unique_ptr<char[]> backtraceBuffer;
constexpr size_t backtraceBufferSize = 1024 * 1024;  // 1MB
std::atomic<size_t> backtraceBufferUsed(0);

/// @brief an atomic that makes sure there are no races inside the signal
/// handler callback
std::atomic<bool> crashHandlerInvoked(false);

/// @brief an atomic that controls whether we will log backtraces
/// (default: yes on Linux, false elsewhere) or not
std::atomic<bool> enableStacktraces(true);

/// @brief kill process hard using SIGKILL, in order to circumvent core
/// file generation etc.
std::atomic<bool> killHard(false);

/// @brief string with server state information. must be null-terminated
std::atomic<char const*> stateString = nullptr;

/// @brief atomic variable for coordinating between signal handler and crash
/// handler thread
std::atomic<arangodb::CrashHandlerState> crashHandlerState(
    arangodb::CrashHandlerState::IDLE);

/// @brief dedicated crash handler thread
std::unique_ptr<std::jthread> crashHandlerThread = nullptr;

/// @brief mutex to protect thread creation/destruction
std::mutex crashHandlerThreadMutex;

// Static variables to store crash data for the dedicated crash handler thread
/// @brief stores the crash context/reason
std::string_view crashContext;

/// @brief stores the signal number
int crashSignal = 0;

/// @brief stores the thread id (pthread id)
uint64_t crashThreadId = 0;

/// @brief stores the thread number (arangodb Thread number)
uint64_t crashThreadNumber = 0;

/// @brief stores a pointer to the signal info
siginfo_t* crashSiginfo = nullptr;

/// @brief stores a pointer to the user context
void* crashUcontext = nullptr;

/// @brief stores crash data for later use by the crash handler thread
void storeCrashData(std::string_view context, int signal, uint64_t threadId,
                    uint64_t threadNumber, siginfo_t* info, void* ucontext) {
  // We intentionally do not protect this by a mutex. After all, acquiring
  // a mutex in a signal handler is not allowed. Rather, this is protected
  // by the exchange method used on ::crashHandlerInvoked, which can only
  // be successful for one crashing thread. And then, there is actual
  // synchronization between the signal handler and the dedicated
  // CrashHandler thread through atomics, so this is in fact safe.
  crashContext = context;
  crashSignal = signal;
  crashThreadId = threadId;
  crashThreadNumber = threadNumber;
  crashSiginfo = info;
  crashUcontext = ucontext;
}

/// @brief kills the process with the given signal
[[noreturn]] void killProcess(int signal) {
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
                     uint64_t threadId, uint64_t threadNumber,
                     siginfo_t const* info, void* ucontext) {
  // build a crash message
  buffer.append("💥 ArangoDB ").append(ARANGODB_VERSION_FULL);

  if constexpr (arangodb::build_id::supportsBuildIdReader()) {
    // get build-id by reference, so we can avoid a copy here
    std::string const& buildId = arangodb::rest::Version::getBuildId();
    if (!buildId.empty()) {
      buffer.append(", build-id ").append(buildId);
    }
  }

  // append thread id
  buffer.append(", thread ").appendUInt64(threadNumber);

  // append thread name
  arangodb::ThreadNameFetcher nameFetcher(threadId);
  buffer.append(" [").append(nameFetcher.get()).append("]");

  // append signal number and name
  buffer.append(" caught unexpected signal ").appendUInt64(uint64_t(signal));
  buffer.append(" (").append(arangodb::signals::name(signal));
  if (info != nullptr) {
    // signal sub type, if available
    std::string_view subType =
        arangodb::signals::subtypeName(signal, info->si_code);
    if (!subType.empty()) {
      buffer.append(", sub type ");
      buffer.append(subType);
    }
    // pid that sent the signal
    buffer.append(") from pid ").appendUInt64(uint64_t(info->si_pid));
  } else {
    buffer.append(")");
  }

  if (char const* ss = ::stateString.load(); ss != nullptr) {
    // append application server state
    buffer.append(" in state \"").append(ss).append("\"");
  }

  {
    // append current working directory
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
      buffer.append(" in directory \"").append(cwd).append("\"");
    }
  }

  if (info != nullptr && (signal == SIGSEGV || signal == SIGBUS)) {
    // dump address that was accessed when the failure occurred (this is
    // somewhat likely a nullptr)
    buffer.append(" accessing address 0x").appendHexValue(info->si_addr, false);
  }

  buffer.append(": ").append(context);

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
}

void logCrashInfo(std::string_view context, int signal, uint64_t threadId,
                  uint64_t threadNumber, siginfo_t* info, void* ucontext) try {
  // Note that this uses the Logger and should thus not be called
  // directly in a signal handler. The idea is that this is actually
  // executed in the dedicated CrashHandler thread.

  // fixed buffer for constructing temporary log messages (to avoid malloc):
  SmallString buffer;

  buildLogMessage(buffer, context, signal, threadId, threadNumber, info,
                  ucontext);
  // note: LOG_TOPIC() can allocate memory
  LOG_TOPIC("a7902", FATAL, arangodb::Logger::CRASH) << buffer.view();
} catch (...) {
  // we better not throw an exception from inside a signal handler
}

/// @brief acquire backtrace data and write it to the provided buffer
/// @param buffer pointer to buffer to write backtrace data to
/// @param bufferSize size of the buffer
/// @return number of bytes written to buffer
/// Note that it is safe to call this in a signal handler, provided
/// the buffer has been allocated beforehand!
size_t acquireBacktrace(char* buffer, size_t bufferSize) try {
  if (!enableStacktraces.load(std::memory_order_relaxed)) {
    return 0;
  }

  if (buffer == nullptr || bufferSize == 0) {
    return 0;
  }

  arangodb::ThreadNameFetcher nameFetcher;
  std::string_view currentThreadName = nameFetcher.get();
  if (currentThreadName == arangodb::Logger::logThreadName) {
    // we must not log a backtrace from the logging thread itself. if we would
    // do, we may cause a deadlock
    return 0;
  }

#ifdef ARANGODB_HAVE_LIBUNWIND
  size_t totalWritten = 0;

  // Helper lambda to safely append to buffer
  auto safeAppend = [&](std::string_view text) -> bool {
    if (totalWritten + text.size() < bufferSize) {
      memcpy(buffer + totalWritten, text.data(), text.size());
      totalWritten += text.size();
      return true;
    }
    return false;
  };

  // Write header
  {
    SmallString headerBuffer;
    headerBuffer.append("Backtrace of thread ");
    headerBuffer.appendUInt64(arangodb::Thread::currentThreadNumber());
    headerBuffer.append(" [").append(currentThreadName).append("]\n");

    if (!safeAppend(headerBuffer.view())) {
      return totalWritten;
    }
  }

  // The address of the program headers of the executable.
  long base = getauxval(AT_PHDR) - sizeof(Elf64_Ehdr);

  unw_cursor_t cursor;
  unw_context_t uc;

  if (unw_getcontext(&uc) == 0 && unw_init_local(&cursor, &uc) == 0) {
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
        SmallString frameBuffer;
        frameBuffer.append("..reached maximum frame display depth (");
        frameBuffer.appendUInt64(maxFrames);
        frameBuffer.append("). stopping backtrace\n");
        safeAppend(frameBuffer.view());
        break;
      }

      if (frame >= skipFrames) {
        // this is a stack frame we want to display
        SmallString frameBuffer;
        frameBuffer.append("frame ");
        if (frame < 10) {
          // pad frame id to 2 digits length
          frameBuffer.push_back(' ');
        }
        frameBuffer.appendUInt64(uint64_t(frame));

        appendAddress(frameBuffer, pc, base);

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
            frameBuffer.append(demangled.get());
          } else {
            // demangling has failed.
            // in this case, append mangled version. still better than nothing
            frameBuffer.append(mangled);
          }
          // print offset into function
          frameBuffer.append(" (+0x").appendHexValue(offset, true).append(")");
        } else {
          // unable to retrieve symbol information
          frameBuffer.append("*no symbol name available for this frame");
        }

        frameBuffer.append("\n");

        if (!safeAppend(frameBuffer.view())) {
          break;  // Buffer is full
        }
      }
    } while (++frame < (maxFrames + skipFrames + 1) && unw_step(&cursor) > 0);
  }

  return totalWritten;
#else
  return 0;
#endif
} catch (...) {
  // we better not throw an exception from inside a signal handler
  return 0;
}

/// @brief log a previously acquired backtrace from a buffer
/// Note that this uses the Logger and thus should not be called directly
/// in a signal handler. Use `acquireBacktrace` above to get the backtrace
/// in the signal handler and then this function in the dedicated
/// CrashHandler thread to do the actual logging!
void logAcquiredBacktrace(std::string_view buffer) try {
  if (buffer.empty()) {
    return;
  }

  // Split buffer content by lines and log each line
  size_t pos = 0;
  size_t lineStart = 0;

  while (pos <= buffer.size()) {
    if (pos == buffer.size() || buffer[pos] == '\n') {
      if (pos > lineStart) {
        std::string_view line = buffer.substr(lineStart, pos - lineStart);
        if (lineStart == 0) {
          // First line is the header
          LOG_TOPIC("c962b", INFO, arangodb::Logger::CRASH) << line;
        } else if (line.find("..reached maximum frame display depth") !=
                   std::string_view::npos) {
          // Max depth message
          LOG_TOPIC("bbb04", INFO, arangodb::Logger::CRASH) << line;
        } else {
          // Regular frame
          LOG_TOPIC("308c3", INFO, arangodb::Logger::CRASH) << line;
        }
      }
      lineStart = pos + 1;
    }
    pos++;
  }

  // flush logs as early as possible
  arangodb::Logger::flush();
} catch (...) {
}

void logBacktrace() try {
  // Allocate a temporary buffer for this backtrace
  constexpr size_t tempBufferSize =
      1024 * 1024;  // 1MB should be enough for most backtraces
  auto tempBuffer = std::make_unique<char[]>(tempBufferSize);

  size_t bytesWritten = acquireBacktrace(tempBuffer.get(), tempBufferSize);
  if (bytesWritten > 0) {
    logAcquiredBacktrace(std::string_view(tempBuffer.get(), bytesWritten));
  }
} catch (...) {
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

void actuallyDumpCrashInfo() {
  // Handle the crash logging in this dedicated thread
  // We can safely do all the work here since we're not in a signal
  // handler
  try {
    // Log crash information using the stored data
    logCrashInfo(crashContext, crashSignal, crashThreadId, crashThreadNumber,
                 crashSiginfo, crashUcontext);
    SmallString buffer;
    buffer.append(
        "💥 Hello, this is the dedicated crash handler thread, I will "
        "make this unfortunate crash experience as agreeable as "
        "possible for you...");
    LOG_TOPIC("a7903", FATAL, arangodb::Logger::CRASH) << buffer.view();

    // Log process information
    logProcessInfo();

    // Log the backtrace that was acquired by the signal handler
    size_t bytesUsed = ::backtraceBufferUsed.load(std::memory_order_acquire);
    if (::backtraceBuffer != nullptr && bytesUsed > 0) {
      logAcquiredBacktrace(
          std::string_view(::backtraceBuffer.get(), bytesUsed));
    }

    // Flush logs
    arangodb::Logger::flush();

  } catch (...) {
    // Ignore exceptions in crash handling
  }
}

/// @brief dedicated thread function that monitors for crashes and handles
/// logging
void crashHandlerThreadFunction() {
  while (true) {
    // Wait for the state to change from IDLE using C++20 atomic wait
    arangodb::CrashHandlerState expectedState =
        arangodb::CrashHandlerState::IDLE;
    crashHandlerState.wait(expectedState, std::memory_order_acquire);

    arangodb::CrashHandlerState state =
        crashHandlerState.load(std::memory_order_acquire);

    switch (state) {
      case arangodb::CrashHandlerState::IDLE:
        // Spurious wakeup or race condition, continue waiting
        break;

      case arangodb::CrashHandlerState::CRASH_DETECTED: {
        actuallyDumpCrashInfo();

        // Signal that we're done with crash handling and notify waiters
        crashHandlerState.store(arangodb::CrashHandlerState::HANDLING_COMPLETE,
                                std::memory_order_release);
        crashHandlerState
            .notify_all();  // tell the crashed thread to continue crashing
        return;
      }

      case arangodb::CrashHandlerState::SHUTDOWN:
        // shutdown requested
        return;

      default: {
        // Unknown state, sleep briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        break;
      }
    }
  }
}

/// @brief This is the actual function that is invoked for a deadly signal
/// (i.e. SIGSEGV, SIGBUS, SIGILL, SIGFPE...)
///
/// the following assumptions are made for this crash handler:
/// - it is invoked in fatal situations only, that we need as much information
///   as possible about. thus we try logging some information into the
///   ArangoDB logfile. Our logger is not async-safe right now, but
///   everything in our own log message-building routine should be
///   async-safe. Therefore we collect data here and then trigger
///   execution of the actual logging in a dedicated CrashHandler thread,
///   which keeps running even in case we catch a signal.
/// - The interesting signals are delivered from the same thread that caused
///   them. Thus we will have a few stack frames of the offending thread
///   available.
/// - It is not possible to generate the stack traces from other threads
///   without substantial efforts, so we are not even trying this.
/// - Windows and macOS are not supported.

void crashHandlerSignalHandler(int signal, siginfo_t* info, void* ucontext) {
  if (!::crashHandlerInvoked.exchange(true)) {
    // Now let's check we are not the Logging thread:
    arangodb::ThreadNameFetcher fetcher;
    auto threadName = fetcher.get();
    if (threadName == "Logging") {
      // We cannot really log in here, since most likely we will be holding
      // a mutex in the Logger, so directly kill the process.
      killProcess(signal);
    }

    // Store crash data for the dedicated crash handler thread
    storeCrashData("signal handler invoked", signal,
                   arangodb::Thread::currentThreadId(),
                   arangodb::Thread::currentThreadNumber(), info, ucontext);

    // Now acquire the backtrace from the crashed thread (only the crashed
    // thread can do this) and store it in the preallocated buffer
    if (::backtraceBuffer != nullptr) {
      size_t bytesWritten =
          acquireBacktrace(::backtraceBuffer.get(), ::backtraceBufferSize);
      ::backtraceBufferUsed.store(bytesWritten, std::memory_order_release);
    }

    if (std::this_thread::get_id() == ::crashHandlerThread->get_id()) {
      // In this case we cannot delegate to ourselves to do the actual
      // dumping. So let's just do it ourselves:
      actuallyDumpCrashInfo();
    } else {
      static_assert(decltype(::crashHandlerState)::is_always_lock_free);
      // Signal the dedicated crash handler thread (force trigger even if not
      // idle), due to this static assertion we know that it is allowed to
      // use `notify_all` here in the signal handler.
      arangodb::CrashHandler::triggerCrashHandler();

      // Busy wait for the dedicated thread to complete its work
      arangodb::CrashHandler::waitForCrashHandlerCompletion();
    }

    // Final cleanup
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

}  // namespace

namespace arangodb {

std::atomic<CrashHandler*> CrashHandler::_theCrashHandler;

void CrashHandler::triggerCrashHandler() {
  ::crashHandlerState.store(arangodb::CrashHandlerState::CRASH_DETECTED,
                            std::memory_order_release);
  ::crashHandlerState.notify_all();
}

void CrashHandler::waitForCrashHandlerCompletion() {
  while (true) {
    arangodb::CrashHandlerState currentState =
        ::crashHandlerState.load(std::memory_order_acquire);
    if (currentState == arangodb::CrashHandlerState::CRASH_DETECTED) {
      // Wait for the state to change from the current state
      ::crashHandlerState.wait(currentState, std::memory_order_acquire);
    } else {
      return;
    }
  }
}

void CrashHandler::logBacktrace() {
  ::logBacktrace();
  Logger::flush();
}

/// @brief logs a fatal message and crashes the program
void CrashHandler::crash(std::string_view context) {
  // Store crash data for the dedicated crash handler thread
  ::storeCrashData(context.data(), SIGABRT, arangodb::Thread::currentThreadId(),
                   arangodb::Thread::currentThreadNumber(), /*no info*/ nullptr,
                   /*no context*/ nullptr);

  // Now acquire the backtrace from the crashed thread (only the crashed
  // thread can do this) and store it in the preallocated buffer
  if (::backtraceBuffer != nullptr) {
    size_t bytesWritten =
        acquireBacktrace(::backtraceBuffer.get(), ::backtraceBufferSize);
    ::backtraceBufferUsed.store(bytesWritten, std::memory_order_release);
  }

  // Signal the dedicated crash handler thread if it exists
  CrashHandler::triggerCrashHandler();

  // Wait for the dedicated thread to complete its work
  CrashHandler::waitForCrashHandlerCompletion();

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

  // Start the dedicated crash handler thread
  {
    std::lock_guard<std::mutex> lock(::crashHandlerThreadMutex);
    if (::crashHandlerThread == nullptr) {
      ::crashHandlerState.store(arangodb::CrashHandlerState::IDLE,
                                std::memory_order_relaxed);
      ::crashHandlerThread =
          std::make_unique<std::jthread>(::crashHandlerThreadFunction);
#ifdef TRI_HAVE_SYS_PRCTL_H
      pthread_setname_np(::crashHandlerThread->native_handle(), "CrashHandler");
#endif
    }
  }

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

  try {
    // Allocate the preallocated backtrace buffer
    ::backtraceBuffer = std::make_unique<char[]>(::backtraceBufferSize);
  } catch (...) {
    // could not allocate memory for backtrace buffer.
    // this is not critical - we can still work without it
    ::backtraceBuffer.release();
    LOG_TOPIC("25162", WARN, Logger::CRASH)
        << "Could not allocate static memory for crash stack traces!";
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

  std::atexit([]() {
    auto* ch = CrashHandler::_theCrashHandler.exchange(nullptr);
    if (ch != nullptr) {
      ch->shutdownCrashHandler();
    }
  });

  std::at_quick_exit([]() {
    auto* ch = CrashHandler::_theCrashHandler.exchange(nullptr);
    if (ch != nullptr) {
      ch->shutdownCrashHandler();
    }
  });
}

/// @brief shuts down the crash handler thread
void CrashHandler::shutdownCrashHandler() {
  std::lock_guard<std::mutex> lock(::crashHandlerThreadMutex);

  if (::crashHandlerThread != nullptr) {
    // Signal the thread to shutdown and notify it if it's idle. If it is
    // already handling a crash (or has finished handling a crash) it will stop
    // afterwards (and of course the crash will terminate the process after the
    // crash handler is done).
    // We must not change the state if it is CRASH_DETECTED, to not interfere
    // with the signal handler waiting upon completion of the crash handler
    // thread. Note that the signal handler must not finish before the crash
    // handler, due to object lifetimes.
    auto expected = arangodb::CrashHandlerState::IDLE;
    if (::crashHandlerState.compare_exchange_strong(
            expected, arangodb::CrashHandlerState::SHUTDOWN,
            std::memory_order_relaxed, std::memory_order_relaxed)) {
      ::crashHandlerState.notify_all();
    } else {
      TRI_ASSERT(expected == arangodb::CrashHandlerState::CRASH_DETECTED ||
                 expected == arangodb::CrashHandlerState::HANDLING_COMPLETE);
    }

    // Wait for the thread to finish
    if (::crashHandlerThread->joinable()) {
      ::crashHandlerThread->join();
    }

    // Clean up
    ::crashHandlerThread.reset();
    ::crashHandlerState.store(arangodb::CrashHandlerState::IDLE,
                              std::memory_order_relaxed);
  }

  ::backtraceBuffer.reset();  // release memory
}

}  // namespace arangodb
