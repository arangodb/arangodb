boost.coroutine2
===============

boost.coroutine2 provides templates for generalized subroutines which allow multiple entry points for
suspending and resuming execution at certain locations. It preserves the local state of execution and 
allows re-entering subroutines more than once (useful if state must be kept across function calls).

Coroutines can be viewed as a language-level construct providing a special kind of control flow.

In contrast to threads, which are pre-emptive, coroutines switches are cooperative (programmer controls 
when a switch will happen). The kernel is not involved in the coroutine switches.

boost.coroutine2 requires C++11!
Note that boost.coroutine2 is the successor of the deprectated boost.coroutine.
