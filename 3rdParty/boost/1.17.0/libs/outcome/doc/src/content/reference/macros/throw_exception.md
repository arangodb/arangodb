+++
title = "`BOOST_OUTCOME_THROW_EXCEPTION(expr)`"
description = "How to throw a C++ exception, or equivalent thereof."
+++

Throws a C++ exception, or equivalent thereof.

*Overridable*: Define before inclusion.

*Default*:<dl>
<dt>Standalone Outcome (C++ exceptions enabled):
<dd>To `throw expr`
<dt>Standalone Outcome (C++ exceptions disabled):
<dd>To `BOOST_OUTCOME_V2_NAMESPACE::detail::do_fatal_exit(#expr)` which is a function which prints a useful error message including a stack backtrace (where available) to `stderr` before calling `abort()`.
<dt>Boost.Outcome:
<dd>To `BOOST_THROW_EXCEPTION(expr)`.
</dl>

*Header*: `<boost/outcome/config.hpp>`