////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/signals.h"
#include "Basics/operating-system.h"

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>

namespace arangodb {
namespace signals {

#ifndef _WIN32
/// @brief find out what impact a signal will have to the process we send it.
SignalType signalType(int signal) {
  // Some platforms don't have these. To keep our table clean
  // we just define them here:
#ifndef SIGPOLL
#define SIGPOLL 23
#endif
#ifndef SIGSTKFLT
#define SIGSTKFLT 255
#endif
#ifndef SIGPWR
#define SIGPWR 29
#endif

  //     Signal       Value     Action   Comment
  //     ────────────────────────────────────────────────────────────────────
  switch (signal) {
    case SIGHUP:  //    1       Term    Hangup detected on controlling terminal
      return SignalType::logrotate;  // or death of controlling process
                         //                 we say this is non-deadly since we
                         //                 should do a logrotate.
    case SIGINT:         //    2       Term    Interrupt from keyboard
      return SignalType::term;
    case SIGQUIT:  //    3       Core    Quit from keyboard
    case SIGILL:   //    4       Core    Illegal Instruction
    case SIGABRT:  //    6       Core    Abort signal from abort(3)
    case SIGFPE:   //    8       Core    Floating-point exception
    case SIGSEGV:  //   11       Core    Invalid memory reference
      return SignalType::core;
    case SIGKILL:  //    9       Term    Kill signal
    case SIGPIPE:  //   13       Term    Broken pipe: write to pipe with no
                   //                   readers; see pipe(7)
    case SIGALRM:  //   14       Term    Timer signal from alarm(2)
    case SIGTERM:  //   15       Term    Termination signal
    case SIGUSR1:  // 30,10,16   Term    User-defined signal 1
    case SIGUSR2:  // 31,12,17   Term    User-defined signal 2
      return SignalType::term;
    case SIGCHLD:  // 20,17,18   Ign     Child stopped or terminated
      return SignalType::ign;
    case SIGCONT:  // 19,18,25   Cont    Continue if stopped
      return SignalType::cont;
    case SIGSTOP:  // 17,19,23   Stop    Stop process
    case SIGTSTP:  // 18,20,24   Stop    Stop typed at terminal
    case SIGTTIN:  // 21,21,26   Stop    Terminal input for background process
    case SIGTTOU:  // 22,22,27   Stop    Terminal output for background process
      return SignalType::stop;
    case SIGBUS:  //  10,7,10   Core    Bus error (bad memory access)
      return SignalType::core;
    case SIGPOLL:   //            Term    Pollable event (Sys V).
      return SignalType::term; //         Synonym for SIGIO
    case SIGPROF:   //  27,27,29  Term    Profiling timer expired
      return SignalType::term;
    case SIGSYS:   //  12,31,12  Core    Bad system call (SVr4);
                   //                     see also seccomp(2)
    case SIGTRAP:  //     5      Core    Trace/breakpoint trap
      return SignalType::core;
    case SIGURG:  //  16,23,21  Ign     Urgent condition on socket (4.2BSD)
      return SignalType::ign;
    case SIGVTALRM:  //  26,26,28  Term    Virtual alarm clock (4.2BSD)
      return SignalType::term;
    case SIGXCPU:  //  24,24,30  Core    CPU time limit exceeded (4.2BSD);
                   //                    see setrlimit(2)
    case SIGXFSZ:  //  25,25,31  Core    File size limit exceeded (4.2BSD);
                   //                     see setrlimit(2)
      // case SIGIOT:    //     6      Core    IOT trap. A synonym for SIGABRT
      return SignalType::core;
      // case SIGEMT:    //   7,-,7    Term    Emulator trap
    case SIGSTKFLT:  //   -,16,-   Term    Stack fault on coprocessor (unused)
                     // case SIGIO:     //  23,29,22  Term    I/O now possible (4.2BSD)
    case SIGPWR:  //  29,30,19  Term    Power failure (System V)
                  // case SIGINFO:   //   29,-,-           A synonym for SIGPWR
      // case SIGLOST:   //   -,-,-    Term    File lock lost (unused)
      return SignalType::term;
      // case SIGCLD:    //   -,-,18   Ign     A synonym for SIGCHLD
    case SIGWINCH:  //  28,28,20  Ign     Window resize signal (4.3BSD, Sun)
      return SignalType::ign;
      // case SIGUNUSED: //   -,31,-   Core    Synonymous with SIGSYS
      //  return core;
    default:
      return SignalType::user;
  }
  return SignalType::term;
}
#endif

/// @brief whether or not the signal is deadly
bool isDeadly(int signal) {
#ifndef _WIN32
  switch (signalType(signal)) {
    case SignalType::term:
      return true;
    case SignalType::core:
      return true;
    case SignalType::cont:
      return false;
    case SignalType::ign:
      return false;
    case SignalType::logrotate:
      return false;
    case SignalType::stop:
      return false;
    case SignalType::user:  // user signals aren't supposed to be deadly.
      return false;
  }
#endif
  // well windows... always deadly.
  return true;
}

/// @brief return the name for a signal
char const* name(int signal) {
  if (signal >= 128) {
    signal -= 128;
  }
  switch (signal) {
#ifdef SIGHUP
    case SIGHUP:
      return "SIGHUP";
#endif
#ifdef SIGINT
    case SIGINT:
      return "SIGINT";
#endif
#ifdef SIGQUIT
    case SIGQUIT:
      return "SIGQUIT";
#endif
#ifdef SIGKILL
    case SIGILL:
      return "SIGILL";
#endif
#ifdef SIGTRAP
    case SIGTRAP:
      return "SIGTRAP";
#endif
#ifdef SIGABRT
    case SIGABRT:
      return "SIGABRT";
#endif
#ifdef SIGBUS
    case SIGBUS:
      return "SIGBUS";
#endif
#ifdef SIGFPE
    case SIGFPE:
      return "SIGFPE";
#endif
#ifdef SIGKILL
    case SIGKILL:
      return "SIGKILL";
#endif
#ifdef SIGSEGV
    case SIGSEGV:
      return "SIGSEGV";
#endif
#ifdef SIGPIPE
    case SIGPIPE:
      return "SIGPIPE";
#endif
#ifdef SIGTERM
    case SIGTERM:
      return "SIGTERM";
#endif
#ifdef SIGCONT
    case SIGCONT:
      return "SIGCONT";
#endif
#ifdef SIGSTOP
    case SIGSTOP:
      return "SIGSTOP";
#endif
    default:
      return "unknown";
  }
}

void maskAllSignals() {
#ifdef TRI_HAVE_POSIX_THREADS
  sigset_t all;
  sigfillset(&all);
  sigdelset(&all, SIGSEGV);
  sigdelset(&all, SIGBUS);
  sigdelset(&all, SIGILL);
  sigdelset(&all, SIGFPE);
  sigdelset(&all, SIGABRT);
  pthread_sigmask(SIG_SETMASK, &all, nullptr);
#endif
}

void unmaskAllSignals() {
#ifdef TRI_HAVE_POSIX_THREADS
  sigset_t all;
  sigfillset(&all);
  pthread_sigmask(SIG_UNBLOCK, &all, nullptr);
#endif
}

}  // namespace signals
}  // namespace arangodb
