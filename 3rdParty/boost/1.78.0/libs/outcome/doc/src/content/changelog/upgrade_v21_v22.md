+++
title = "Upgrade guide v2.1 => v2.2"
weight = 20
+++

In the start of 2020, after a year of listening to user feedback since
entering Boost, Outcome v2.2 was published with a number of breaking source
changes from Outcome v2.1 The full year of 2020 (three Boost releases) was given to
announcing those upcoming changes, and testing the v2.2 branch in
production. In late December 2020, Outcome v2.2 became the default Outcome,
and all Outcome v2.1 code shall need to be upgraded to work with v2.2.

To upgrade an Outcome v2.1 based codebase to Outcome v2.2 is very easy:

1. You will need a tool capable of finding regular expressions in all source
files in a directory tree and replacing them -- most IDEs such as Visual Studio
have such a tool with GUI, on POSIX a shell script such as this ought to work:

        find /path/to/project -type f -name "*.hpp" | xargs sed -i "s/_TRY\(([^(]*?),(.*?)\);/_TRY((auto &&, \1),\2);/g"
        find /path/to/project -type f -name "*.cpp" | xargs sed -i "s/_TRY\(([^(]*?),(.*?)\);/_TRY((auto &&, \1),\2);/g"
        find /path/to/project -type f -name "*.hpp" | xargs sed -i "s/_TRY\(([^(]*?)\);/_TRYV2(auto &&, \1);/g"
        find /path/to/project -type f -name "*.cpp" | xargs sed -i "s/_TRY\(([^(]*?)\);/_TRYV2(auto &&, \1);/g"

    The transformation needed are the regular expressions `_TRY\(([^(]*?),(.*?)\);` =>
    `_TRY((auto &&, \1),\2);` and `TRY\(([^(]*?)\);` => `_TRYV2(auto &&, \1);`.
    This is because in Outcome v2.2 onwards, `BOOST_OUTCOME_TRY(var, expr)`
    no longer implicitly declares the variable created as `auto&&` on your behalf,
    now you must specify the storage of the variable. It also declares the internal
    uniquely named temporary as a value rather than as a reference, the initial
    brackets overrides this to force the use of a rvalue reference for the internal
    uniquely named temporary instead.
    
    This makes use of Outcome's [new TRY syntax]({{% relref "/tutorial/essential/result/try_ref" %}})
    to tell the TRY operation to use references rather than values for the internal
    uniquely named temporary, thus avoiding any copies and moves. The only way to
    override the storage of the internal uniquely named temporary for non-value
    outputting TRY is via the new `BOOST_OUTCOME_TRYV2()` which takes the storage specifier
    you desire as its first parameter.

    The principle advantage of this change is that you can now assign to
    existing variables the successful results of expressions, instead of being
    forced to TRY into a new variable, and move that variable into the destination
    you intended. Also, because you can now specify storage, you can now assign
    the result of a TRYied operation into static or thread local storage.

2. The find regex and replace rule above is to preserve exact semantics with
Outcome v2.1 whereby the internal uniquely named temporary and the variable for
the value are both rvalue references. If you're feeling like more work, it is
safer if you convert as many `BOOST_OUTCOME_TRY((auto &&, v), expr)` to
`BOOST_OUTCOME_TRY(auto &&v, expr)` as possible. This will mean that TRY 'consumes'
`expr` i.e. moves it into the internal uniquely named temporary, if expr is
an rvalue reference. Usually this does not affect existing code, but occasionally
it can, generally a bit of code reordering will fix it.

3. If your code uses [the ADL discovered event hooks]({{% relref "/tutorial/advanced/hooks" %}})
to intercept when `basic_result` and `basic_outcome` is constructed, copies or
moved, you will need to either define the macro {{% api "BOOST_OUTCOME_ENABLE_LEGACY_SUPPORT_FOR" %}}
to less than `220` to enable emulation, or upgrade the code to use the new mechanism.

    The hooks themselves have identical signature, [only the name and location has
    changed]({{% relref "/tutorial/advanced/hooks" %}}). Therefore upgrade is usually
    a case of copy-pasting the hook implementation into a custom `NoValuePolicy`
    implementation, and changing the ADL free function's name from `hook_*` to `on_*`.

    You are recommended to upgrade if possible, as the ADL discovered hooks were
    found in real world code usage to be brittle and surprising.

4. Any usage of CamelCase named concepts from Outcome must be replaced with snake_case
named concepts instead:

    - `concepts::ValueOrError<T>` => `concepts::value_or_error<T>`
    - `concepts::ValueOrNone<T>` => `concepts::value_or_none<T>`

    The CamelCase naming is aliased to the snake_case naming if the macro
    {{% api "BOOST_OUTCOME_ENABLE_LEGACY_SUPPORT_FOR" %}} is defined to less than `220`.
    Nevertheless you ought to upgrade here is possible, as due to a late change
    in C++ 20 all standard concepts are now snake_case named.

5. Finally, despite that Outcome does not currently offer a stable ABI guarantee
(hoped to begin in 2022), v2.1 had a stable storage layout for `basic_result` and
`basic_outcome`. In v2.2 that storage layout has changed, so the ABIs generated by
use of v2.1 and v2.2 are incompatible i.e. you will need to recompile everything
using Outcome after you upgrade to v2.2.
