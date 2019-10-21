+++
title = "Before we begin"
description = "Essential information before you begin the tutorial."
weight = 5
tags = ["namespace", "playpen"]
+++

## Outcome v2 namespace

It is recommended that you refer to entities from this Outcome v2 via the following namespace alias:

{{% snippet "using_result.cpp" "namespace" %}}

On standalone Outcome only, as patches and modifications are applied to this library,
namespaces get permuted in order to not to cause binary incompatibility. At some point
namespace `outcome_v2` will be defined, and this will be the preferred namespace.
Until then `BOOST_OUTCOME_V2_NAMESPACE` denotes the most recently
updated version, getting closer to `outcome_v2`.

On Boost.Outcome only, as Boost provides no binary compatibility across releases,
`BOOST_OUTCOME_V2_NAMESPACE` always expands into `boost::outcome_v2`.

## Online compilers

If you've never used them before, you will find
[Godbolt](https://godbolt.org/) and [Wandbox](https://wandbox.org/) invaluable.
These let you play with C++ inside your web browser.

Most of the source code snippets in Outcome have a link in their top right to
the original source code on github. You can copy and paste this source code into
Godbolt (if you wish to study the assembler generated) or Wandbox (if you
wish to run the program).

### Godbolt

Godbolt is invaluable for visualising online the assembler generated for a
piece of C++, for all the major compilers and CPU architectures.

Standalone Outcome is built into Godbolt! In the right hand pane toolbar, click the
libraries dropdown (currently third from the right, looks like a book), find
Outcome and choose the version you want.

After this is selected, you can `#include` any of these editions of Outcome:

<dl>
  <dt><code>&lt;outcome-basic.hpp&gt;</code></dt>
  <dd>An inclusion of <code>basic_outcome.hpp</code> + <code>try.hpp</code> which includes as few
  system headers as possible in order to give an absolute minimum compile time
  impact edition of Outcome. See <a href="https://github.com/ned14/stl-header-heft">https://github.com/ned14/stl-header-heft</a>.
  </dd>
  <dt><code>&lt;outcome-experimental.hpp&gt;</code></dt>
  <dd>An inclusion of <code>experimental/status_outcome.hpp</code> + <code>try.hpp</code> which
  is the low compile time impact of the basic edition combined with
  <code>status_code</code> from <a href="https://ned14.github.io/status-code/">https://ned14.github.io/status-code/</a>. If you are on an
  embedded system where binary bloat must be absolutely avoided, and don't
  mind the potentially unstable <code>status_code</code>, this is definitely the edition
  for you.
  </dd>
  <dt><code>&lt;outcome.hpp&gt;</code></dt>
  <dd>An inclusion of <code>outcome.hpp</code> which brings in all the specialisations
  for the <code>std</code> STL types, plus iostreams support. If you don't know which
  edition to use, you should use this one, it ought to "just work".</dd>
</dl>

Here is the first tutorial topic's source code loaded into Godbolt: https://godbolt.org/z/p-NAho

### Wandbox

Wandbox lets you place a third party header into a separate tab. It also
comes with a recent Boost libraries. Either technique can be used to
explore Outcome.

Here is the first tutorial topic's source code loaded into Wandbox: https://wandbox.org/permlink/sJoeKHXSyCU5Avft
