+++
title = "`operator co_await` as TRY operator"
weight = 40
tags = [ "coroutines", "co_await" ]
+++

Many people have requested that `operator co_await` be overloaded to
behave as a TRY operator when supplied with an Outcome type.

Outcome does not implement that extension, nor will we accept PRs
contributing support for this. We think you should use `BOOST_OUTCOME_CO_TRY()`
as this will lead to more maintainable and future proof code.

However, we deliberately do not get in the way of you implementing
that overload yourself in your own Outcome-based code. Just be sure
that you document what you are doing loudly and clearly, and be
aware that future C++ standards may have a proper `operator try`
overload mechanism.
