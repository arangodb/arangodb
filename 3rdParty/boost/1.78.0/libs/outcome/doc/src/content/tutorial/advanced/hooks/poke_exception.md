+++
title = "Custom exception ptr"
description = ""
weight = 40
+++

If you merely want `result` to capture stack backtraces without calling a memory allocator
and retaining any triviality of copy which is important for optimisation,
you already have everything you need.

But let's keep going by intercepting any
construction of our localised `outcome` from our localised `result`, retrieving any
stored backtrace and using it to synthesise an exception ptr with a message text
including the backtrace. Firstly let us look at the function which synthesises
the exception ptr:

{{% snippet "error_code_extended.cpp" "error_code_extended4" %}}

If the localised `outcome` being constructed is errored, try fetching the TLS slot
for the unique 16-bit value in its spare storage. If that is valid, symbolise the
stack backtrace into a string and make an exception ptr with a runtime error with
that string. Finally, override the payload/exception member in our just-copy-constructed
localised `outcome` with the new exception ptr.

<hr>

As the reference documentation for {{% api "void override_outcome_exception(basic_outcome<T, EC, EP, NoValuePolicy> *, U &&) noexcept" %}}
points out, you *almost certainly* never want to use this function if there is any
other alternative. It is worth explaining what is meant by this.

In this section, we *always* synthesise an exception ptr from the stored state and
error code at the exact point of transition from `result` based APIs to `outcome`
based APIs. This is acceptable only because we know that our code enforces that
discipline.

If one were designing a library facility, one could not assume such discipline in the
library user. One would probably be better off making the exception ptr synthesis
*lazy* via a custom no-value policy which generates the stacktrace-containing error
message only on demand e.g. `.exception()` observation, or a `.value()` observation
where no value is available.

Such a design is however more indeterminate than the design presented in this section,
because the indeterminacy is less predictable than in this design. Ultimately which
strategy you adopt depends on how important absolute determinism is to your Outcome-based
application.
