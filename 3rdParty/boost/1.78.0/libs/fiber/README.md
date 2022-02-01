Boost.fiber
===========


Boost.fiber provides a framework for micro-/userland-threads (fibers) scheduled cooperatively. The API contains classes and functions to manage and synchronize fibers similar to boost.thread.

A fiber is able to store the current execution state, including all registers and CPU flags, the instruction pointer, and the stack pointer and later restore this state. The idea is to have multiple execution paths running on a single thread using a sort of cooperative scheduling (threads are preemptively scheduled) - the running fiber decides explicitly when it yields to allow another fiber to run (context switching).

A context switch between threads costs usually thousands of CPU cycles on x86 compared to a fiber switch with less than 100 cycles. A fiber can only run on a single thread at any point in time.

Boost.fiber requires C++11!
