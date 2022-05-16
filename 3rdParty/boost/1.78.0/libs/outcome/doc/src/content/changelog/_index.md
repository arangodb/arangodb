+++
title = "Changelog"
weight = 80
+++

---
## v2.2.2 ??? (Boost 1.78) [[release]](https://github.com/ned14/outcome/releases/tag/v2.2.2)

### Enhancements:

[#255](https://github.com/ned14/outcome/issues/255)
: Restore Experimental Outcome constexpr compatibility in C++ 20 which was an undocumented
property of the Outcome v2.1 series, and which had been dropped in the v2.2 series.

GCC Coroutines support
: Coroutine support in GCCs after 10 is now correctly detected.

### Bug fixes:


---
## v2.2.1 13th August 2021 (Boost 1.77) [[release]](https://github.com/ned14/outcome/releases/tag/v2.2.1)

### Bug fixes:

[#251](https://github.com/ned14/outcome/issues/251)
: Fix failure to compile Boost.Outcome when the compiler declares support for C++ modules.

- Don't use `[[likely]]` in GCCs before 9.

[#251](https://github.com/ned14/outcome/issues/253)
: Make support for C++ modules opt-in.

---
## v2.2.0 16th April 2021 (Boost 1.76) [[release]](https://github.com/ned14/outcome/releases/tag/v2.2.0)

BREAKING CHANGE As announced for a year and three Boost releases, Outcome v2.2 became the default, replacing v2.1.
: All v2.1 Outcome code will need to be upgraded as described in [the v2.1 => v2.2 upgrade guide]({{% relref "/changelog/upgrade_v21_v22" %}}).
This branch has a number of major breaking changes to Outcome v2.1, see
[the list of v2.2 major changes]({{% relref "/changelog/v22" %}}).

### Enhancements:

VS2019.8 compatibility
: VS2019.8 changed how to enable Coroutines, which caused Outcome to not compile on that compiler.

[#237](https://github.com/ned14/outcome/issues/237)
: If on C++ 20, we now use C++ 20 `[[likely]]` instead of compiler-specific markup to indicate
when TRY has likely success or failure.

BREAKING CHANGE [#247](https://github.com/ned14/outcome/issues/247)
: Previously the value of {{% api "spare_storage(const basic_result|basic_outcome *) noexcept" %}} was
not propagated over `BOOST_OUTCOME_TRY`, which causes things like stack backtraces captured at the point of
construction of an errored result to get dropped at every `TRY` point. This has been fixed by adding
an optional `spare_storage` to {{% api "success_type<T>" %}} and {{% api "failure_type<T>" %}}, as well
as to {{% api "auto success(T &&, ...)" %}} and {{% api "auto failure(T &&, ...)" %}}.

    You should not notice this in your code, except that where before spare storage values did not
    propagate through TRY, now they do, which is a breaking change.

### Bug fixes:

BREAKING CHANGE [#244](https://github.com/ned14/outcome/issues/244)
: It came as a shock to learn that `BOOST_OUTCOME_TRY` had been broken since the inception of this
library for certain corner case code:

    ```c++
    outcome::result<Foo>    get_foo();
    outcome::result<Foo>    filter1(outcome::result<Foo> &&);
    outcome::result<Foo> && filter2(outcome::result<Foo> &&);

    // This works fine, and always has
    BOOST_OUTCOME_TRY(auto v, filter1(get_foo()))

    // This causes UB due to result<Foo> being destructed before move of value into v
    BOOST_OUTCOME_TRY(auto v, filter2(get_foo()))
    ```

    Whilst reference passthrough filter functions are not common, they can turn up in highly generic
    code, where destruction before copy/move is not helpful.

    The cause is that TRY used to work by binding the result of the expression to an `auto &&unique`,
    testing if that unique if successful or not, and if successful then moving from `unique.value()`
    into the user's output variable. If the expression returned is a prvalue, the Result's lifetime is
    extended by the bound reference to outside of the statement, and all is good. If the expression
    returned is an xvalue or lvalue, then the lifetime extension does not exceed that of the statement,
    and the Result is destructed after the semicolon succeeding the assignment to `auto &&unique`.

    This bug has been fixed by TRY deducing the [value category](https://en.cppreference.com/w/cpp/language/value_category)
    of its input expression as follows:

    - prvalues => `auto unique = (expr)`   (breaking change)
    - xvalue => `auto unique = (expr)`     (breaking change)
    - lvalue => `auto unique = (expr)`     (breaking change)

    This ensures that xvalue and lvalue inputs do not cause unhelpfully early lifetime end, though it
    does silently change the behaviour of existing code which relied on rvalues and lvalues being passed
    through, as a new construct-move-destruct or construct-copy-destruct cycle is introduced to where
    there was none before. Also, before C++ 17, there is now an added copy/move for prvalue inputs,
    which does not affect runtime codegen due to Return Value Optimisation (RVO), but does cause
    Results containing non-copying non-moving types to fail to compile, which is a breaking change
    from beforehand.

    If one wishes rvalues or lvalues to be passed through, one can avail of a new TRY syntax based on
    preprocessor overloading:

    - `BOOST_OUTCOME_TRY((refspec, varname), expr)`
    - `BOOST_OUTCOME_TRYV2(refspec, expr)`

    Here `refspec` is the storage to be used for **both** the internal temporary unique, AND `varname`.
    So if you write:

    ```c++
    Foo &&foo;
    BOOST_OUTCOME_TRY((auto &&, v), filter2(foo))
    ```
    ... then the internal unique is declared as `auto &&unique = (filter2(foo))`, and the output variable
    is declared as `auto &&v = std::move(unique).assume_value()`. This passes through the rvalue referencing,
    and completely avoids copies and moves of `Foo`. If you wish to not extract the value but also
    specify unique storage, there is a new `BOOST_OUTCOME_TRYV2(refspec, expr)`.

    My thanks to KamilCuk from https://stackoverflow.com/questions/66069152/token-detection-within-a-c-preprocessor-macro-argument
    for all their help in designing the new overloaded TRY syntax. My thanks also to vasama for reporting this
    issue and working through how best to fix it with me.

[#249](https://github.com/ned14/outcome/issues/249)
: The preprocessor logic for choosing when to use `bool` with `concept` on GCC was yet again refactored.
This should fix those choices of GCC configuration which caused failure due to the wrong combination
being chosen.

---
## v2.1.5 11th December 2020 (Boost 1.75) [[release]](https://github.com/ned14/outcome/releases/tag/v2.1.5)

### Enhancements:

[The ADL discovered event hooks]({{% relref "/tutorial/advanced/hooks" %}}) have been replaced with policy-specified event hooks instead
: This is due to brittleness (where hooks would quietly
self-disable if somebody changed something), compiler bugs (a difference in compiler settings causes
the wrong hooks, or some but not all hooks, to get discovered), and end user difficulty in using
them at all. The policy-specified event hooks can be told to default to ADL discovered hooks for
backwards compatibility: set {{% api "BOOST_OUTCOME_ENABLE_LEGACY_SUPPORT_FOR" %}} to less than `220` to
enable emulation.

Improve configuring `BOOST_OUTCOME_GCC6_CONCEPT_BOOL`
: Older GCCs had boolean based concepts syntax, whereas newer GCCs are standards conforming.
However the precise logic of when to use legacy and conforming syntax was not well understood,
which caused Outcome to fail to compile depending on what options you pass to GCC. The new logic
always uses the legacy syntax if on GCC 8 or older, otherwise we use conforming syntax if and
only if GCC is in C++ 20 mode or later. This hopefully will resolve the corner case build
failures on GCC.

### Bug fixes:

Boost.Outcome should now compile with `BOOST_NO_EXCEPTIONS` defined
: Thanks to Emil, maintainer of Boost.Exception, making a change for me, Boost.Outcome
should now compile with C++ exceptions globally disabled. You won't be able to use
`boost::exception_ptr` as it can't be included if C++ exceptions are globally disabled.

[#236](https://github.com/ned14/outcome/issues/236)
: In the Coroutine support the `final_suspend()` was not `noexcept`, despite being required
to be so in the C++ 20 standard. This has been fixed, but only if your compiler implements
`noop_coroutine`. Additionally, if `noop_coroutine` is available, we use the much more
efficient coroutine handle returning variant of `await_suspend()` which should significantly
improve codegen and context switching performance.

---
## v2.1.4 14th August 2020 (Boost 1.74) [[release]](https://github.com/ned14/outcome/releases/tag/v2.1.4)

### Enhancements:

BREAKING CHANGE `void` results and outcomes no longer default construct types during explicit construction
: Previously if you explicitly constructed a `result<T>` from a non-errored
`result<void>`, it default constructed `T`. This was found to cause unhelpful
surprise, so it has been disabled.

New macro `BOOST_OUTCOME_ENABLE_LEGACY_SUPPORT_FOR`
: The macro {{% api "BOOST_OUTCOME_ENABLE_LEGACY_SUPPORT_FOR" %}} can be used to
enable aliasing of older naming and features to newer naming and features when
using a newer version of Outcome.

Concepts now have snake case style naming instead of camel case style
: When Outcome was first implemented, it was thought that C++ 20 concepts were
going to have camel case style. This was changed before the C++ 20 release, and
Outcome's concepts have been renamed similarly. This won't break any code in
Outcome v2.1, as compatibility aliases are provided. However code compiled
against Outcome v2.2 will need to be upgraded, unless `BOOST_OUTCOME_ENABLE_LEGACY_SUPPORT_FOR`
is set to less than `220`.

Concepts now live in `BOOST_OUTCOME_V2_NAMESPACE::concepts` namespace
: Previously concepts lived in the `convert` namespace, now they live in their
own namespace.

New concepts {{% api "basic_result<T>" %}} and {{% api "basic_outcome<T>" %}} added
: End users were finding an unhelpful gap in between {{% api "is_basic_result<T>" %}}
and {{% api "value_or_error<T>" %}} where they wanted a concept that matched
types which were `basic_result`, but not exactly one of those. Concepts filling
that gap were added.

Operation `TRY` works differently from Outcome v2.2 onwards
: This is a severely code breaking change which change the syntax of how one uses
`BOOST_OUTCOME_TRY()`. A regular expression suitable for upgrading code can be found in
the list of changes between Outcome v2.1 and v2.2.

### Bug fixes:

[#224](https://github.com/ned14/outcome/issues/224)
: The clang Apple ships in Xcode 11.4 (currently the latest) has not been patched
with the fixes to LLVM clang that fix `noexcept(std::is_constructible<T, void>)`
failing to compile which I originally submitted years ago. So give up waiting on
Apple to fix their clang, add a workaround to Outcome.

Use of `void` in `T` or `E` caused `noexcept(false)`
: Direct traits examination of `void` was causing nothrow detection to return false,
fixed.

Spare storage could not be used from within no-value policy classes
: Due to an obvious brain fart when writing the code at the time, the spare storage
APIs had the wrong prototype which prevented them working from within policy classes.
Sorry.

---
## v2.1.3 29th April 2020 (Boost 1.73) [[release]](https://github.com/ned14/outcome/releases/tag/v2.1.3)

### Enhancements:

Performance of Outcome-based code compiled by clang has been greatly improved
: The previous implementation of Outcome's status bitfield confused clang's
optimiser, which caused low quality codegen. Unlike most codegen issues, this was
noticeably in empirical benchmarks of real world code, as was shown by
[P1886 *Error speed benchmarking*](https://wg21.link/P1886).

    The safe part of the [`better_optimisation`](https://github.com/ned14/outcome/tree/better_optimisation)
Outcome v2.2.0 future branch was merged to Outcome v2.1.3 which includes a new
status bitfield implementation. This appears to not confuse clang's optimiser,
and clang 9 produces code which routinely beats GCC 9's code for various canned
use cases.

Precompiled headers are automatically enabled on new enough cmake's for standalone Outcome
: If on cmake 3.16 or later, its new precompiled headers build support is used
to tell consumers of the `outcome::hl` cmake target to precompile Outcome, **if
and only if** `PROJECT_IS_DEPENDENCY` is false. `PROJECT_IS_DEPENDENCY` is set
by Outcome's CMakeLists.txt if it detects that it was included using
`add_subdirectory()`, so for the vast majority of Outcome end users, the use
of precompiled headers will NOT be enabled.

    Exported targets do NOT request precompilation of headers, as it is
assumed that importers of the Outcome cmake targets will configure their own
precompiled headers which incorporate Outcome.

Installability is now CI tested per commit
: Due to installability of standalone Outcome (e.g. `make install`) breaking
itself rather more frequently than is ideal, installability is now tested on CI
per commit.

Coroutines support has been documented
: The coroutines support added in v2.1.2 has now been properly documented.

### Bug fixes:

[#214](https://github.com/ned14/outcome/issues/214)
: Newer Concepts implementing compilers were unhappy with the early check for
destructibility of `T` and `E`, so removed template constraints, falling back
to static assert which runs later in the type instantiation sequence.

[#215](https://github.com/ned14/outcome/issues/215)
: For standalone Outcome, `CMAKE_TOOLCHAIN_FILE` is now passed through during
dependency superbuild. This should solve build issues for some embedded toolchain
users.

[#220](https://github.com/ned14/outcome/issues/220)
: A false positive undefined behaviour sanitiser failure in some use cases of
Experimental Outcome was worked around to avoid the failure message.

[#221](https://github.com/ned14/outcome/issues/221)
: Restored compatibility with x86 on Windows, which was failing with link errors.
It was quite surprising that this bug was not reported sooner, but obviously
almost nobody is using Outcome with x86 on Windows.

[#223](https://github.com/ned14/outcome/issues/223)
: Fix a segfault in Debug builds only when cloning a `status_code_ptr` in
Experimental.Outcome only.

---
## v2.1.2 11th December 2019 (Boost 1.72) [[release]](https://github.com/ned14/outcome/releases/tag/v2.1.2)

### Enhancements:

Improved compatibility with cmake tooling
: Standalone outcome is now `make install`-able, and cmake `find_package()` can find it.
Note that you must separately install and `find_package()` Outcome's dependency, quickcpplib,
else `find_package()` of Outcome will fail.

Non-permissive parsing is now default in Visual Studio
: The default targets in standalone Outcome's cmake now enable non-permissive parsing.
This was required partially because VS2019 16.3's quite buggy Concepts implementation is
unusuable in permissive parsing mode. Even then, lazy ADL two phase lookup is broken
in VS2019 16.3 with `/std:latest`, you may wish to use an earlier language standard.

**Breaking change!**
: The git submodule mechanism used by standalone Outcome of specifying dependent libraries
has been replaced with a cmake superbuild of dependencies mechanism instead. Upon cmake
configure, an internal copy of quickcpplib will be git cloned, built and installed into the
build directory from where an internal `find_package()` uses it. This breaks the use of
the unconfigured Outcome repo as an implementation of Outcome, one must now do one of:
 1. Add Outcome as subdirectory to cmake build.
 2. Use cmake superbuild (i.e. `ExternalProject_Add()`) to build and install Outcome into
 a local installation.
 3. Use one of the single header editions.

**Breaking change!**
: For standalone Outcome, the current compiler is now checked for whether it will compile
code containing C++ Concepts, and if it does, all cmake consumers of Outcome will enable
C++ Concepts. Set the cmake variable `BOOST_OUTCOME_C_CONCEPTS_FLAGS` to an empty string to prevent
auto detection and enabling of C++ Concepts support occurring.

`BOOST_OUTCOME_TRY` operation now hints to the compiler that operation will be successful
: [P1886 *Error speed benchmarking*](https://wg21.link/P1886) showed that there is
considerable gain in very small functions by hinting to the compiler whether the expression
is expected to be successful or not. `BOOST_OUTCOME_TRY` previously did not hint to the compiler
at all, but now it does. A new suite of macros `BOOST_OUTCOME_TRY_FAILURE_LIKELY` hint to the
compiler that failure is expected. If you wish to return to the previously unhinted
behaviour, define `BOOST_OUTCOME_TRY_LIKELY(expr)` to `(!!expr)`.

[#199](https://github.com/ned14/outcome/issues/199)
: Support for C++ Coroutines has been added. This comes in two parts, firstly there is
now an `BOOST_OUTCOME_CO_TRY()` operation suitable for performing the `TRY` operation from
within a C++ Coroutine. Secondly, in the header `outcome/coroutine_support.hpp` there are
implementations of `eager<OutcomeType>` and `lazy<OutcomeType>` which let you more
naturally and efficiently use `basic_result` or `basic_outcome` from within C++
Coroutines -- specifically, if the result or outcome will construct from an exception
pointer, exceptions thrown in the coroutine return an errored or excepted result with
the thrown exception instead of throwing the exception through the coroutine machinery
(which in current compilers, has a high likelihood of blowing up the program). Both
`eager<T>` and `lazy<T>` can accept any `T` as well. Both have been tested and found
working on VS2019 and clang 9.

[#210](https://github.com/ned14/outcome/issues/210)
: `make_error_code()` and `make_exception_ptr()` are now additionally considered for
compatible copy and move conversions for `basic_result<>`. This lets you construct
a `basic_result<T, E>` into a `basic_result<T, error_code>`, where `E` is a
custom type which has implemented the ADL discovered free function
`error_code make_error_code(E)`, but is otherwise unrelated to `error_code`.
The same availability applies for `exception_ptr` with `make_exception_ptr()` being
the ADL discovered free function. `basic_outcome<>` has less support for this than
`basic_result<>` in order to keep constructor count down, but it will accept via
this mechanism conversions from `basic_result<>` and `failure_type<>`.

### Bug fixes:

[#184](https://github.com/ned14/outcome/issues/207)
: The detection of `[[nodiscard]]` support in the compiler was very mildly broken.

---
## v2.1.1 19th August 2019 (Boost 1.71) [[release]](https://github.com/ned14/outcome/releases/tag/v2.1.1)

### Enhancements:

[#184](https://github.com/ned14/outcome/issues/184)
: As per request from Boost release managers, relocated `version.hpp` and
`revision.hpp` into detail, and added the Boost licence boilerplate to the top
of every source file which was missing one (I think). Also took the opportunity
to run the licence restamping script over all Outcome, so copyright dates are now
up to date.

[#185](https://github.com/ned14/outcome/issues/185)
: Add FAQ item explaining issue #185, and why we will do nothing to
fix it right now.

[#189](https://github.com/ned14/outcome/issues/189)
: Refactored the `BOOST_OUTCOME_TRY` implementation to use more clarified
customisation points capable of accepting very foreign inputs. Removed the
`std::experimental::expected<T, E>` specialisations, as those are no longer
necessary. Fixed the documentation for the customisation points which
previously claimed that they are ADL discovered, which they are not. Added
a recipe describing how to add in support for foreign input types.

[#183](https://github.com/ned14/outcome/issues/183)
: Added a separate `motivation/plug_error_code` specifically for Boost.

### Bug fixes:

-
: `BOOST_OUTCOME_VERSION_MINOR` hadn't been updated to 1.

[#181](https://github.com/ned14/outcome/issues/181)
: Fix issue #181 where Outcome didn't actually implement the strong swap guarantee,
despite being documented as doing so.

[#190](https://github.com/ned14/outcome/issues/190)
: Fix issue #190 in Boost edition where unit test suite was not runnable from
the Boost release distro.

[#182](https://github.com/ned14/outcome/issues/182)
: Fix issue #182 where `trait::is_exception_ptr_available<T>` was always true,
thus causing much weirdness, like not printing diagnostics and trying to feed
everything to `make_exception_ptr()`.

[#194](https://github.com/ned14/outcome/issues/192)
: Fix issue #192 where the `std::basic_outcome_failure_exception_from_error()`
was being defined twice for translation units which combine standalone and
Boost Outcome's.

---
## v2.1 12th Apr 2019 (Boost 1.70) [[release]](https://github.com/ned14/outcome/releases/tag/v2.1)

- [#180](https://github.com/ned14/outcome/issues/180)
    - `success()` and `failure()` now produce types marked `[[nodiscard]]`.

- `include/outcome/outcome.natvis` is now namespace permuted like the rest of
Outcome, so debugging Outcome based code in Visual Studio should look much
prettier than before.

- [#162](https://github.com/ned14/outcome/issues/162)
    - `.has_failure()` was returning false at times when it should have returned true.

- [#152](https://github.com/ned14/outcome/issues/152)
    - GCC 5 no longer can compile Outcome at all due to [https://stackoverflow.com/questions/45607450/gcc5-nested-variable-template-is-not-a-function-template](https://stackoverflow.com/questions/45607450/gcc5-nested-variable-template-is-not-a-function-template).
Added explicit version trap for GCC 5 to say it can not work. Note this is not a
breaking change, GCC 5 was never supported officially in any v2 Outcome.

- [#150](https://github.com/ned14/outcome/issues/150)
    - **BREAKING CHANGE** `result<T, E>`, `boost_result<T, E>` and `std_result<T, E>`
no longer implement hard UB on fetching a value from a valueless instance if `E` is
a UDT, they now fail to compile with a useful error message. If you wish hard UB,
use `unchecked<T, E>`, `boost_unchecked<T, E>` or `std_unchecked<T, E>` instead.

- [#140](https://github.com/ned14/outcome/issues/140)
    - Fixed a nasty corner case bug where value type's without a copy constructor
but with a move constructor would indicate via traits that copy construction
was available. Thanks to Microsoft's compiler team for reporting this issue.

- Added experimental `status_result` and `status_outcome` based on experimental
`status_code`.

- Boost edition is now 100% Boost, so defaults for `result` and `outcome` are
`boost::system::error_code::errc_t` and `boost::exception_ptr`. Moreover,
the test suite in the Boost edition now exclusively tests the Boost edition.
One can, of course, freely use the standalone edition with Boost, and the Boost
edition with `std` types.

- Renamed ADL discovered customisation point `throw_as_system_error_with_payload()`
to `outcome_throw_as_system_error_with_payload()`.

- [#135](https://github.com/ned14/outcome/issues/135)
    - Added much clearer compile failure when user tries `result<T, T>` or `outcome`
    where two or more types are identical. Thanks to Andrzej Krzemieński
    for suggesting a technique which combines SFINAE correctness with
    the remaining ability for `result<T, T>` etc to be a valid type, but
    not constructible.

- [#67](https://github.com/ned14/outcome/issues/67)
    - Fixed one of the oldest long open bugs in Outcome, that the noexcept
unit tests failed on OS X for an unknown reason.

- [#115](https://github.com/ned14/outcome/issues/115)
    - Outcome did not construct correctly from `failure_type`.

- Inexplicably outcome's error + exception constructor had been removed.
Nobody noticed during the Boost peer review, which is worrying seeing as that
constructor is needed for one of the main advertised features to Boost!

- [#107](https://github.com/ned14/outcome/issues/107) and [#116](https://github.com/ned14/outcome/issues/116)
    - `operator==` and `operator!=` now become disabled if the value, error and
    exception types do not implement the same operator.
    - Relatedly, both comparison operators simply didn't work right. Fixed.

- [#109](https://github.com/ned14/outcome/issues/109)
    - `swap()` now has correct `noexcept` calculation and now correctly orders
    the swaps to be whichever is the throwing swap first.

- Added reference dump of v2.1 ABI so we can check if ABI breakage detection
works in the next set of changes, plus Travis job to check ABI and API compatibility
per commit.

- [#124](https://github.com/ned14/outcome/issues/124)
    - `BOOST_OUTCOME_TRY` is now overloaded and selects `void` or `auto` edition
    according to input parameter count.

- [#120](https://github.com/ned14/outcome/issues/120)
    - Fix generation of double underscored temporary variables in
    `BOOST_OUTCOME_UNIQUE_NAME`, which is UB.

- [#110](https://github.com/ned14/outcome/issues/110)
    - Separated `result` from its hard coded dependency on the `<system_error>` header.
    - Renamed `result` and `outcome` to `basic_result` and `basic_outcome`.
    - Renamed `result.hpp` into `basic_result.hpp`.
    - Moved `<system_error>` and `<exception>` dependent code into new
    `std_result.hpp` and `std_outcome.hpp` header files.
    - Added `boost_result.hpp` and `boost_outcome.hpp` which use Boost.System
    and Boost.Exception (these are `result.hpp` and `outcome.hpp` in the Boost edition).

---
## v2.0 18th Jan 2018 [[release]](https://github.com/ned14/outcome/releases/tag/v2.0-boost-peer-review)

- Boost peer review edition. This is what was reviewed.
- Changelog from v1 can be found in the release notes for this release.
