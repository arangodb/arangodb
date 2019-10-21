+++
title = "`BOOST_OUTCOME_NODISCARD`"
description = "How to tell the compiler than the return value of a function should not be discarded without examining it."
+++

Compiler-specific markup used to tell the compiler than the return value of a function should not be discarded without examining it.

*Overridable*: Define before inclusion.

*Default*: To `[[nodiscard]]` if on C++ 17 or higher, `__attribute__((warn_unused_result))` if on clang, SAL `_Must_inspect_result_` if on MSVC, otherwise nothing.

*Header*: `<boost/outcome/config.hpp>`