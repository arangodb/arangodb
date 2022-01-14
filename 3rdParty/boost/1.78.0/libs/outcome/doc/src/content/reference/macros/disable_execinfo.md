+++
title = "`BOOST_OUTCOME_DISABLE_EXECINFO`"
description = "If defined, disables the use of the `<execinfo.h>` header (or the win32 emulation)."
+++

If defined, disables the use of the `<execinfo.h>` header (or the win32 emulation).

Some embedded Linux toolchains do not define `<execinfo.h>`, thus disabling C++ exceptions on those toolchains produces a failure to find this file. Avoid that problem by defining this macro to disable stack backtrace support entirely.

*Overridable*: Define before inclusion.

*Default*: Defined if `__ANDROID__` is defined, else undefined.

*Header*: `<boost/outcome/config.hpp>`