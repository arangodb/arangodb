+++
title = "`BOOST_OUTCOME_SYMBOL_VISIBLE`"
description = "How to mark throwable types as always having default ELF symbol visibility."
+++

Compiler-specific markup used to mark throwable types as always having default ELF symbol visibility, without which it will be impossible to catch throws of such types across shared library boundaries on ELF only.

*Overridable*: Define before inclusion.

*Default*:<dl>
<dt>Standalone Outcome:
<dd>To `__attribute__((visibility("default"))` on GCC and clang when targeting ELF, otherwise nothing.
<dt>Boost.Outcome:
<dd>To `BOOST_SYMBOL_VISIBLE`.
</dl>

*Header*: `<boost/outcome/config.hpp>`