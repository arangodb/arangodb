+++
title = "Worked example: Custom domain"
weight = 80
+++

Here follows a worked example of use of Experimental Outcome. It presents
the same sample program I sent to the San Diego 2018 WG21 standards meeting
after I was asked by the committee to demonstrate how P1095 implements P0709
in a working code example they could study and discuss.

We will walk through this worked example, step by step, explaining how each
part works in detail. This will help you implement your own code based on
Experimental Outcome.

You may find it useful to open now in a separate browser tab the reference API
documentation for proposed `<system_error2>` at https://ned14.github.io/status-code/
(scroll half way down). The references in the comments to P1028 are to
[P1028 *SG14 status_code and standard error object for P0709 Zero-overhead
deterministic exceptions*](http://wg21.link/P1028), which is the WG21 proposal
paper for potential `<system_error2>`.

### Goal of this section

We are going to define a simple custom code domain which defines that
the status code's payload will consist of a POSIX error code, and the
`__FILE__` and `__LINE__` where the failure occurred. This custom status
code will have an implicit conversion to type erased `error` defined, which dynamically
allocates memory for the original status code, and outputs an `error`
which manages that dynamic allocation, indirecting all queries etc
to the erased custom status code type such that the `error` instance
quacks as if just like the original. This demonstrates that `error` could
just as equally convey a `std::exception_ptr`, for example, or indeed
manage the lifetime of any pointer.
