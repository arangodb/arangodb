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

#include "Basics/signals.h"
#include "Basics/operating-system.h"

#include <atomic>
#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>

namespace arangodb::signals {

/// @brief find out what impact a signal will have to the process we send it.
SignalType signalType(int signal) noexcept {
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
                                     //                 we say this is
                                     //                 non-deadly since we
                                     //                 should do a logrotate.
    case SIGINT:  //    2       Term    Interrupt from keyboard
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
    case SIGPOLL:               //            Term    Pollable event (Sys V).
      return SignalType::term;  //         Synonym for SIGIO
    case SIGPROF:               //  27,27,29  Term    Profiling timer expired
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
                     // case SIGIO:     //  23,29,22  Term    I/O now possible
                     // (4.2BSD)
    case SIGPWR:     //  29,30,19  Term    Power failure (System V)
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
}

/// @brief whether or not the signal is deadly
bool isDeadly(int signal) noexcept {
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
  // well windows... always deadly.
  return true;
}

/// @brief return the name for a signal
std::string_view name(int signal) noexcept {
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

std::string_view subtypeName(int signal, int subCode) noexcept {
  // the following meanings are taken from /usr/include/asm-generic/siginfo.h
  switch (signal) {
    case SIGILL: {
      switch (subCode) {
#ifdef ILL_ILLOPC
        case ILL_ILLOPC:
          return "ILL_ILLOPC: illegal opcode";
#endif
#ifdef ILL_ILLOPN
        case ILL_ILLOPN:
          return "ILL_ILLOPN: illegal operand";
#endif
#ifdef ILL_ILLADR
        case ILL_ILLADR:
          return "ILL_ILLADR: illegal addressing mode";
#endif
#ifdef ILL_ILLTRP
        case ILL_ILLTRP:
          return "ILL_ILLTRP: illegal trap";
#endif
#ifdef ILL_PRVOPC
        case ILL_PRVOPC:
          return "ILL_PRVOPC: privileged opcode";
#endif
#ifdef ILL_PRVREG
        case ILL_PRVREG:
          return "ILL_PRVREG: privileged register";
#endif
#ifdef ILL_COPROC
        case ILL_COPROC:
          return "ILL_COPROC: coprocessor error";
#endif
#ifdef ILL_BADSTK
        case ILL_BADSTK:
          return "ILL_BADSTK: internal stack error";
#endif
#ifdef ILL_BADIADDR
        case ILL_BADIADDR:
          return "ILL_BADIADDR: unimplemented instruction address";
#endif
        default:
          break;
      }
      break;
    }
    case SIGFPE: {
      switch (subCode) {
#ifdef FPE_INTDIV
        case FPE_INTDIV:
          return "FPE_INTDIV: integer divide by zero";
#endif
#ifdef FPE_INTOVF
        case FPE_INTOVF:
          return "FPE_INTOVF: integer overflow";
#endif
#ifdef FPE_FLTDIV
        case FPE_FLTDIV:
          return "FPE_FLTDIV: floating point divide by zero";
#endif
#ifdef FPE_FLTOVF
        case FPE_FLTOVF:
          return "FPE_FLTOVF: floating point overflow";
#endif
#ifdef FPE_FLTUND
        case FPE_FLTUND:
          return "FPE_FLTUND: floating point underflow";
#endif
#ifdef FPE_FLTRES
        case FPE_FLTRES:
          return "FPE_FLTRES: floating point inexact result";
#endif
#ifdef FPE_FLTINV
        case FPE_FLTINV:
          return "FPE_FLTINV: floating point invalid operation";
#endif
#ifdef FPE_FLTSUB
        case FPE_FLTSUB:
          return "FPE_FLTSUB: subscript out of range";
#endif
#ifdef FPE_FLTUNK
        case FPE_FLTUNK:
          return "FPE_FLTUNK: undiagnosed floating-point exception";
#endif
#ifdef FPE_CONDTRAP
        case FPE_CONDTRAP:
          return "FPE_CONDTRAP: trap on condition";
#endif
        default:
          break;
      }
      break;
    }
    case SIGSEGV: {
      switch (subCode) {
#ifdef SEGV_MAPERR
        case SEGV_MAPERR:
          return "SEGV_MAPERR: address not mapped to object";
#endif
#ifdef SEGV_ACCERR
        case SEGV_ACCERR:
          return "SEGV_ACCERR: invalid permissions for mapped object";
#endif
#ifdef SEGV_PKUERR
        case SEGV_PKUERR:
          return "SEGV_PKUERR: failed protection key checks";
#endif
#ifdef SEGV_ACCADI
        case SEGV_ACCADI:
          return "SEGV_ACCADI: ADI not enabled for mapped object";
#endif
#ifdef SEGV_ADIDERR
        case SEGV_ADIDERR:
          return "SEGV_ADIDERR: Disrupting MCD error";
#endif
#ifdef SEGV_ADIPERR
        case SEGV_ADIPERR:
          return "SEGV_ADIPERR: Precise MCD exception";
#endif
#ifdef SEGV_MTEAERR
        case SEGV_MTEAERR:
          return "SEGV_MTEAERR: Asynchronous ARM MTE error";
#endif
#ifdef SEGV_MTESERR
        case SEGV_MTESERR:
          return "SEGV_MTESERR: Synchronous ARM MTE exception";
#endif
        default:
          break;
      }
      break;
    }
    case SIGBUS: {
      switch (subCode) {
#ifdef BUS_ADRALN
        case BUS_ADRALN:
          return "BUS_ADRALN: invalid address alignment";
#endif
#ifdef BUS_ADRERR
        case BUS_ADRERR:
          return "BUS_ADRERR: non-existent physical address";
#endif
#ifdef BUS_OBJERR
        case BUS_OBJERR:
          return "BUS_OBJERR: object specific hardware error";
#endif
#ifdef BUS_MCEERR_AR
        case BUS_MCEERR_AR:
          return "BUS_MCEERR_AR: hardware memory error consumed on a machine "
                 "check";
#endif
#ifdef BUS_MCEERR_AO
        case BUS_MCEERR_AO:
          return "BUS_MCEERR_AO: hardware memory error "
                 "detected in process but not consumed";
#endif
        default:
          break;
      }
      break;
    }
    case SIGTRAP: {
      switch (subCode) {
#ifdef TRAP_BRKPT
        case TRAP_BRKPT:
          return "TRAP_BRKPT: process breakpoint";
#endif
#ifdef TRAP_TRACE
        case TRAP_TRACE:
          return "TRAP_TRACE: process trace trap";
#endif
#ifdef TRAP_BRANCH
        case TRAP_BRANCH:
          return "TRAP_BRANCH: process taken branch trap";
#endif
#ifdef TRAP_HWBKPT
        case TRAP_HWBKPT:
          return "TRAP_HWBKPT: hardware breakpoint/watchpoint";
#endif
#ifdef TRAP_UNK
        case TRAP_UNK:
          return "TRAP_UNK: undiagnosed trap";
#endif
#ifdef TRAP_PERF
        case TRAP_PERF:
          return "TRAP_PERF: perf event with sigtrap=1";
#endif
        default:
          break;
      }
      break;
    }

    default:
      break;
  }
  return "unknown";
}

std::atomic<bool> isServer = true;

void maskAllSignalsServer() {
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

void maskAllSignalsClient() {
  isServer.store(false);
#ifdef TRI_HAVE_POSIX_THREADS
  sigset_t all;
  sigfillset(&all);
  sigdelset(&all, SIGSEGV);
  sigdelset(&all, SIGBUS);
  sigdelset(&all, SIGILL);
  sigdelset(&all, SIGFPE);
  sigdelset(&all, SIGABRT);
  sigdelset(&all, SIGINT);
  pthread_sigmask(SIG_SETMASK, &all, nullptr);
#endif
}

void maskAllSignals() {
  if (isServer.load()) {
    maskAllSignalsServer();
  } else {
    maskAllSignalsClient();
  }
}

void unmaskAllSignals() {
#ifdef TRI_HAVE_POSIX_THREADS
  sigset_t all;
  sigfillset(&all);
  pthread_sigmask(SIG_UNBLOCK, &all, nullptr);
#endif
}

}  // namespace arangodb::signals
