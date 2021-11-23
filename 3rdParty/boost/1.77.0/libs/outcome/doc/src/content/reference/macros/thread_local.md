+++
title = "`BOOST_OUTCOME_THREAD_LOCAL`"
description = "How to mark variables as having thread local storage duration."
+++

Compiler-specific markup used to mark variables as having thread local storage duration.

{{% notice note %}}
This isn't used inside Outcome, but is used by its unit test suite.
{{% /notice %}}

*Overridable*: Define before inclusion.

*Default*: To `thread_local` if the compiler implements C++ 11 `thread_local`, else `__thread` for the one supported compiler (older Mac OS XCode) which does not.

*Header*: `<boost/outcome/config.hpp>`