+++
title = "The main advantages"
weight = 5
+++

The main advantages of choosing `<boost/outcome/experimental>` over default Outcome:

1. Codegen is tighter and less verbose[^1], sometimes remarkably so.

2. Build time impact is markedly lower, barely above the inclusion of naked
`<basic_result.hpp>`, as the STL allocator machinery and `std::string` et al
is not dragged into inclusion by including `<system_error>`. Note that
`<boost/outcome/experimental/status_outcome.hpp>` bring in `<exception>`,
however `<boost/outcome/experimental/status_result.hpp>` brings in no extra system
headers.

3. More discipline is imposed on your use of Outcome, leading to
less ambiguous code which is easier to optimise by the compiler,
lower cost to maintain, and lower cognitive load to audit code based on
experimental Outcome for correctness.

4. Code written to `<boost/outcome/experimental>` can be fairly easily dual
targeted, with just a few switching type aliases, to default Outcome.
This author has several Outcome-based libraries with identical source code which
can target either configuration of Outcome. The experimental Outcome
build regularly beats the default Outcome build in benchmarks by 2-3%,
and the dual target source code, being written to tighter discipline,
is faster and more deterministic in the default target than it was before
the (non-trivial) port to `<boost/outcome/experimental>`.


If you are building a codebase on top of Outcome expecting long term
maintenance, the author's personal recommendation is that you design, write, test and
optimise it for `<boost/outcome/experimental>`. What you ship to your customers
ought to be targeted at default Outcome however, so employ type aliases and
macros as appropriate to switch the build configuration for production releases.
This is what the Outcome author does himself, to date with great success,
despite the fundamental architectural differences between `<system_error>`
and proposed `<system_error2>`.



[^1]: Boost.System's `error_code` has incorporated some of the design improvements of experimental `status_code`, and produces codegen somewhere in between experimental `status_code` and `std::error_code`.