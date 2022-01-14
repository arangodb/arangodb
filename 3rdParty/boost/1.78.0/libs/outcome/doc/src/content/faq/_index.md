+++
title = "Frequently asked questions"
weight = 30
+++

{{% toc %}}

## Is Outcome safe to use in extern APIs?

Outcome is specifically designed for use in the public interfaces of multi-million
line codebases. `result`'s layout is hard coded to:

```c++
struct trivially_copyable_result_layout {
  union {
    value_type value;
    error_type error;
  };
  unsigned int flags;
};
```

... if both `value_type` and `error_type` are `TriviallyCopyable`, otherwise:

```c++
struct non_trivially_copyable_result_layout {
  value_type value;
  unsigned int flags;
  error_type error;
};
```

This is C-compatible if `value_type` and `error_type` are C-compatible. {{% api "std::error_code" %}}
is *probably* C-compatible, but its layout is not standardised (though there is a
normative note in the standard about its layout). Hence Outcome cannot provide a
C macro API for standard Outcome, but we can for [Experimental Outcome]({{< relref "/experimental/c-api" >}}).


## Does Outcome implement over-alignment?

Outcome propagates any over-alignment of the types you supply to it as according
to the layout specified above. Therefore the ordinary alignment and padding rules
for your compiler are used.


## Does Outcome implement the no-fail, strong or basic exception guarantee?

([You can read about the meaning of these guarantees at cppreference.com](https://en.cppreference.com/w/cpp/language/exceptions#Exception_safety))

If for the following operations:

- Construction
- Assignment
- Swap

... the corresponding operation in **all** of `value_type`, `error_type` (and
`exception_type` for `outcome`) is `noexcept(true)`, then `result` and
`outcome`'s operation is `noexcept(true)`. This propagates the no-fail exception
guarantee of the underlying types. Otherwise the basic guarantee applies for all
but Swap, under the same rules as for the `struct` layout type given above e.g.
value would be constructed first, then the flags, then the error. If the error
throws, value and status bits would be as if the failure had not occurred, same
as for aborting the construction of any `struct` type.

It is recognised that these weak guarantees may be unsuitable for some people,
so Outcome implements `swap()` with much stronger guarantees, as one can locally refine,
without too much work, one's own custom classes from `result` and `outcome` implementing
stronger guarantees for construction and assignment using `swap()` as the primitive
building block.

The core ADL discovered implementation of strong guarantee swap is {{% api "strong_swap(bool &all_good, T &a, T &b)" %}}.
This can be overloaded by third party code with custom strong guarantee swap
implementations, same as for `std::swap()`. Because strong guarantee swap may fail
when trying to restore input state during handling of failure to swap, the
`all_good` boolean becomes false if restoration fails, at which point both
results/outcomes get marked as tainted via {{% api "has_lost_consistency()" %}}.

It is **up to you** to check this flag to see if known good state has been lost,
as Outcome never does so on your behalf. The simple solution to avoiding having
to deal with this situation is to always choose your value, error and exception
types to have non-throwing move constructors and move assignments. This causes
the strong swap implementation to no longer be used, as it is no longer required,
and standard swap is used instead.


## Does Outcome have a stable ABI and API?

The layout changed for all trivially copyable types between Outcome v2.1 and v2.2,
as union based storage was introduced. From v2.2 onwards, the layout is not
expected to change again.

If v2.2 proves to be unchanging for 24 months, Outcome's ABI and API will be
formally fixed as **the** v2 interface and written into stone forever. Thereafter
the [ABI compliance checker](https://lvc.github.io/abi-compliance-checker/)
will be run per-commit to ensure Outcome's ABI and API remains stable. This is
currently expected to occur in 2022.

Note that the stable ABI and API guarantee will only apply to standalone
Outcome, not to Boost.Outcome. Boost.Outcome has dependencies on other
parts of Boost which are not stable across releases.

Note also that the types you configure a `result` or `outcome` with also need
to be ABI stable if `result` or `outcome` is to be ABI stable.


## Can I use `result<T, EC>` across DLL/shared object boundaries?

A known problem with using Windows DLLs (and to smaller extent POSIX shared libraries) is that global
objects may get duplicated: one instance in the executable and one in the DLL. This
behaviour is not incorrect according to the C++ Standard, as the Standard does not
recognize the existence of DLLs or shared libraries. Therefore, program designs that
depend on globals having unique addresses may become compromised when used in a program
using DLLs.

Nothing in Outcome depends on the addresses of globals, plus the guaranteed fixed data
layout (see answer above) means that different versions of Outcome can be used in
different DLLs, and it probably will work okay (it is still not advised that you do that
as that is an ODR violation).
However, one of the most likely candidate for `EC` -- `std::error_code` -- **does** depend
on the addresses of globals for correct functioning.

The standard library is required to implement globally unique addresses for the standard library
provided {{% api "std::error_category" %}} implementations e.g. `std::system_category()`.
User defined error code categories may **not** have unique global addresses, and thus
introduce misoperation.

`boost::system::error_code`, since version 1.69 does offer an *opt-in* guarantee
that it does not depend on the addresses of globals **if** the user defined error code
category *opts-in* to the 64-bit comparison mechanism. This can be seen in the specification of
`error_category::operator==` in
[Boost.System synopsis](https://www.boost.org/doc/libs/1_69_0/libs/system/doc/html/system.html#ref_synopsis).

Alternatively, the `status_code` in [Experimental Outcome](({{< relref "/experimental/differences" >}})),
due to its more modern design, does not suffer from any problems from being used in shared
libraries in any configuration.


## Why two types `result<>`  and `outcome<>`, rather than just one?

`result` is the simple, success OR failure type.

`outcome` extends `result` with a third state to transport, conventionally (but not necessarily) some sort of "abort" or "exceptional" state which a function can return to indicate that not only did the operation fail, but it did so *catastrophically* i.e. please abort any attempt to retry the operation.

A perfect alternative to using `outcome` is to throw a C++ exception for the abort code path, and indeed most programs ought to do exactly that instead of using `outcome`. However there are a number of use cases where choosing `outcome` shines:

1. Where C++ exceptions or RTTI is not available, but the ability to fail catastrophically without terminating the program is important.
2. Where deterministic behaviour is required even in the catastrophic failure situation.
3. In unit test suites of code using Outcome it is extremely convenient to accumulate test failures into an `outcome` for later reporting. A similar convenience applies to RPC situations, where C++ exception throws need to be accumulated for reporting back to the initiating endpoint.
4. Where a function is "dual use deterministic" i.e. it can be used deterministically, in which case one switches control flow based on `.error()`, or it can be used non-deterministically by throwing an exception perhaps carrying a custom payload.


## How badly will including Outcome in my public interface affect compile times?

The quick answer is that it depends on how much convenience you want.

The convenience header `<result.hpp>` is dependent on `<system_error>` or Boost.System, which unfortunately includes `<string>` and thus
drags in quite a lot of other slow-to-parse stuff. If your public interface already includes `<string>`,
then the impact of additionally including Outcome will be low. If you do not include `<string>`,
unfortunately impact may be relatively quite high, depending on the total impact of your
public interface files.

If you've been extremely careful to avoid ever including the most of the STL headers
into your interfaces in order to maximise build performance, then `<basic_result.hpp>`
can have as few dependencies as:

1. `<cstdint>`
2. `<initializer_list>`
3. `<iosfwd>`
4. `<new>`
5. `<type_traits>`
6. `<cstdio>`
7. `<cstdlib>`
8. `<cassert>`

These, apart from `<iosfwd>`, tend to be very low build time impact in most standard
library implementations. If you include only `<basic_result.hpp>`, and manually configure
`basic_result<>` by hand, compile time impact will be minimised.

(See reference documentation for {{% api "basic_result<T, E, NoValuePolicy>" %}} for more detail.


## Is Outcome suitable for fixed latency/predictable execution coding such as for high frequency trading or audio?

Great care has been taken to ensure that Outcome never unexpectedly executes anything
with unbounded execution times such as `malloc()`, `dynamic_cast<>()` or `throw`.
Outcome works perfectly with C++ exceptions and RTTI globally disabled.

Outcome's entire design premise is that its users are happy to exchange a small, predictable constant overhead
during successful code paths, in exchange for predictable failure code paths.

In contrast, table-based exception handling gives zero run time overhead for the
successful code path, and completely unpredictable (and very expensive) overhead
for failure code paths.

For code where predictability of execution, no matter the code path, is paramount,
writing all your code to use Outcome is not a bad place to start. Obviously enough,
do choose a non-throwing policy when configuring `outcome` or `result` such as
{{% api "all_narrow" %}} to guarantee that exceptions can never be thrown by Outcome
(or use the convenience typedef for `result`, {{% api "unchecked<T, E = varies>" %}} which uses `policy::all_narrow`).


## What kind of runtime performance impact will using Outcome in my code introduce?

It is very hard to say anything definitive about performance impacts in codebases one
has never seen. Each codebase is unique. However to come up with some form of measure,
we timed traversing ten stack frames via each of the main mechanisms, including the
"do nothing" (null) case.

A stack frame is defined to be something called by the compiler whilst
unwinding the stack between the point of return in the ultimate callee and the base
caller, so for example ten stack allocated objects might be destructed, or ten levels
of stack depth might be unwound. This is not a particularly realistic test, but it
should at least give one an idea of the performance impact of returning Outcome's
`result` or `outcome` over say returning a plain integer, or throwing an exception.

The following figures are for Outcome v2.1.0 with GCC 7.4, clang 8.0 and Visual
Studio 2017.9. Figures for newer Outcomes with newer compilers can be found at
https://github.com/ned14/outcome/tree/develop/benchmark.

### High end CPU: Intel Skylake x64

This is a high end CPU with very significant ability to cache, predict, parallelise
and execute out-of-order such that tight, repeated loops perform very well. It has
a large μop cache able to wholly contain the test loop, meaning that these results
are a **best case** performance.

{{% figure src="/faq/results_skylake_log.png" title="Log graph comparing GCC 7.4, clang 8.0 and Visual Studio 2017.9 on x64, for exceptions-globally-disabled, ordinary and link-time-optimised build configurations." %}}

As you can see, throwing and catching an exception is
expensive on table-based exception handling implementations such as these, anywhere
between 26,000 and 43,000 CPU cycles. And this is the *hot path* situation, this
benchmark is a loop around hot cached code. If the tables are paged out onto storage,
you are talking about **millions** of CPU cycles.

Simple integer returns (i.e. do nothing null case)
are always going to be the fastest as they do the least work, and that costs 80 to 90
CPU cycles on this Intel Skylake CPU.

Note that returning a `result<int, std::error_code>` with a "success (error code)"
is no more than 5% added runtime overhead over returning a naked int on GCC and clang. On MSVC
it costs an extra 20% or so, mainly due to poor code optimisation in the VS2017.9 compiler. Note that "success
(experimental status code)" optimises much better, and has almost no overhead over a
naked int.

Returning a `result<int, std::error_code>` with a "failure (error code)"
is less than 5% runtime overhead over returning a success on GCC, clang and MSVC.

You might wonder what happens if type `E` has a non-trivial destructor, thus making the
`result<T, E>` have a non-trivial destructor? We tested `E = std::exception_ptr` and
found less than a 5% overhead to `E = std::error_code` for returning success. Returning a failure
was obviously much slower at anywhere between 300 and 1,100 CPU cycles, due to the
dynamic memory allocation and free of the exception ptr, plus at least two atomic operations per stack frame, but that is
still two orders of magnitude better than throwing and catching an exception.

We conclude that if failure is anything but extremely rare in your C++ codebase,
using Outcome instead of throwing and catching exceptions ought to be quicker overall:

- Experimental Outcome is statistically indistinguishable from the null case on this
high end CPU, for both returning success and failure, on all compilers.
- Standard Outcome is less than 5%
worse than the null case for returning successes on GCC and clang, and less than 10% worse than
the null case for returning failures on GCC and clang.
- Standard Outcome optimises
poorly on VS2017.9, indeed markedly worse than on previous point releases, so let's
hope that Microsoft fix that soon. It currently has a less than 20% overhead on the null case.

### Mid tier CPU: ARM Cortex A72

This is a four year old mid tier CPU used in many high end mobile phones and tablets
of its day, with good ability to cache, predict, parallelise
and execute out-of-order such that tight, repeated loops perform very well. It has
a μop cache able to wholly contain the test loop, meaning that these results
are a **best case** performance.

{{% figure src="/faq/results_arm_a72_log.png" title="Log graph comparing GCC 7.3 and clang 7.3 on ARM64, for exceptions-globally-disabled, ordinary and link-time-optimised build configurations." %}}

This ARM chip is a very consistent performer -- null case, success, or failure, all take
almost exactly the same CPU cycles. Choosing Outcome, in any configuration, makes no
difference to not using Outcome at all. Throwing and catching a C++ exception costs
about 90,000 CPU cycles, whereas the null case/Outcome costs about 130 - 140 CPU cycles.

There is very little to say about this CPU, other than Outcome is zero overhead on it. The same
applied to the ARM Cortex A15 incidentally, which I test cased extensively when
deciding on the Outcome v2 design back after the first peer review. The v2 design
was chosen partially because of such consistent performance on ARM.

### Low end CPUs: Intel Silvermont x64 and ARM Cortex A53

These are low end CPUs with a mostly or wholly in-order execution core. They have a small
or no μop cache, meaning that the CPU must always decode the instruction stream.
These results represent an execution environment more typical of CPUs two decades
ago, back when table-based EH created a big performance win if you never threw
an exception.

{{% figure src="/faq/results_silvermont_log.png" title="Log graph comparing GCC 7.3 and clang 7.3 on x64, for exceptions-globally-disabled, ordinary and link-time-optimised build configurations." %}}
{{% figure src="/faq/results_arm_a53_log.png" title="Log graph comparing GCC 7.3 and clang 7.3 on ARM64, for exceptions-globally-disabled, ordinary and link-time-optimised build configurations." %}}

The first thing to mention is that clang generates very high performance code for
in-order cores, far better than GCC. It is said that this is due to a very large investment by
Apple in clang/LLVM for their devices sustained over many years. In any case, if you're
targeting in-order CPUs, don't use GCC if you can use clang instead!

For the null case, Silvermont and Cortex A53 are quite similar in terms of CPU clock cycles. Ditto
for throwing and catching a C++ exception (approx 150,000 CPU cycles). However the Cortex
A53 does far better with Outcome than Silvermont, a 15% versus 100% overhead for Standard
Outcome, and a 4% versus 20% overhead for Experimental Outcome.

Much of this large difference is in fact due to calling convention differences. x64 permits up to 8 bytes
to be returned from functions by CPU register. `result<int>` consumes 24 bytes, so on x64
the compiler writes the return value to the stack. However ARM64 permits up to 64 bytes
to be returned in registers, so `result<int>` is returned via CPU registers on ARM64.

On higher end CPUs, memory is read and written in cache lines (32 or 64 bytes), and
reads and writes are coalesced and batched together by the out-of-order execution core. On these
low end CPUs, memory is read and written sequentially per assembler instruction,
so only one load or one store to L1
cache can occur at a time. This makes writing the stack particularly slow on in-order
CPUs. Memory operations which "disappear" on higher end CPUs take considerable time
on low end CPUs. This particularly punishes Silvermont in a way which does not punish
the Cortex A53, because of having to write multiple values to the stack to create the
24 byte object to be returned.

The conclusion to take away from this is that if you are targeting a low end CPU,
table-based EH still delivers significant performance improvements for the success
code path. Unless determinism in failure is critically important, you should not
use Outcome on in-order execution CPUs.


## Why is implicit default construction disabled?

This was one of the more interesting points of discussion during the peer review of
Outcome v1. v1 had a formal empty state. This came with many advantages, but it
was not felt to be STL idiomatic as `std::optional<result<T>>` is what was meant, so
v2 has eliminated any legal possibility of being empty.

The `expected<T, E>` proposal of that time (May 2017) did permit default construction
if its `T` type allowed default construction. This was specifically done to make
`expected<T, E>` more useful in STL containers as one can say resize a vector without
having to supply an `expected<T, E>` instance to fill the new items with. However
there was some unease with that design choice, because it may cause programmers to
use some type `T` whose default constructed state is overloaded with additional meaning,
typically "to be filled" i.e. a de facto empty state via choosing a magic value.

For the v2 redesign, the various arguments during the v1 review were considered.
Unlike `expected<T, E>` which is intended to be a general purpose Either monad
vocabulary type, Outcome's types are meant primarily for returning success or failure
from functions. The API should therefore encourage the programmer to not overload
the successful type with additional meaning of "to be filled" e.g. `result<std::optional<T>>`.
The decision was therefore taken to disable *implicit* default construction, but
still permit *explicit* default construction by making the programmer spell out their
intention with extra typing.

To therefore explicitly default construct a `result<T>` or `outcome<T>`, use one
of these forms as is the most appropriate for the use case:

1. Construct with just `in_place_type<T>` e.g. `result<T>(in_place_type<T>)`.
2. Construct via `success()` e.g. `outcome<T>(success())`.
3. Construct from a `void` form e.g. `result<T>(result<void>(in_place_type<void>))`.


## How far away from the proposed `std::expected<T, E>` is Outcome's `checked<T, E>`?

Not far, in fact after the first Boost.Outcome peer review in May 2017, Expected moved
much closer to Outcome, and Outcome deliberately provides {{% api "checked<T, E = varies>" %}}
as a semantic equivalent.

Here are the remaining differences which represent the
divergence of consensus opinion between the Boost peer review and WG21 on the proper
design for this object:

1. `checked<T, E>` has no default constructor. Expected has a default constructor if
`T` has a default constructor.
2. `checked<T, E>` uses the same constructor design as `std::variant<...>`. Expected
uses the constructor design of `std::optional<T>`.
3. `checked<T, E>` cannot be modified after construction except by assignment.
Expected provides an `.emplace()` modifier.
4. `checked<T, E>` permits implicit construction from both `T` and `E` when
unambiguous. Expected permits implicit construction from `T` alone.
5. `checked<T, E>` does not permit `T` and `E` to be the same, and becomes annoying
to use if they are constructible into one another (implicit construction self-disables).
Expected permits `T` and `E` to be the same.
6. `checked<T, E>` throws `bad_result_access_with<E>` instead of Expected's
`bad_expected_access<E>`.
7. `checked<T, E>` models `std::variant<...>`. Expected models `std::optional<T>`. Thus:
   - `checked<T, E>` does not provide `operator*()` nor `operator->`
   - `checked<T, E>` `.error()` is wide (i.e. throws on no-value) like `.value()`.
   Expected's `.error()` is narrow (UB on no-error). [`checked<T, E>` provides
   `.assume_value()` and `.assume_error()` for narrow (UB causing) observers].
8. `checked<T, E>` uses `success<T>` and `failure<E>` type sugars for disambiguation.
Expected uses `unexpected<E>` only.
9. `checked<T, E>` does not implement (prone to unintended misoperation) comparison
operators which permit implicit conversion e.g. `checked<T> == T` will fail to compile.
Instead write unambiguous code e.g. `checked<T> == success(T)` or `checked<T> == failure(T)`.
10. `checked<T, E>` defaults `E` to `std::error_code` or `boost::system::error_code`.
Expected does not default `E`.

In fact, the two are sufficiently close in design that a highly conforming `expected<T, E>`
can be implemented by wrapping up `checked<T, E>` with the differing functionality:

{{% snippet "expected_implementation.cpp" "expected_implementation" %}}


## Why doesn't Outcome duplicate `std::expected<T, E>`'s design?

There are a number of reasons:

1. Outcome is not aimed at the same audience as Expected. We target developers
and users who would be happy to use Boost. Expected targets the standard library user.

2. Outcome believes that the monadic use case isn't as important as Expected does.
Specifically, we think that 99% of use of Expected in the real world will be to
return failure from functions, and not as some sort of enhanced or "rich" Optional.
Outcome therefore models a subset of Variant, whereas Expected models an extended Optional.

3. Outcome believes that if you are thinking about using something like Outcome,
then for you writing failure code will be in the same proportion as writing success code,
and thus in Outcome writing for failure is exactly the same as writing for success.
Expected assumes that success will be more common than failure, and makes you type
more when writing for failure.

4. Outcome goes to considerable effort to help the end user type fewer characters
during use. This results in tighter, less verbose, more succinct code. The cost of this is a steeper
learning curve and more complex mental model than when programming with Expected.

5. Outcome has facilities to make easier interoperation between multiple third
party libraries each using incommensurate Outcome (or Expected) configurations. Expected does
not do any of this, but subsequent WG21 papers do propose various interoperation
mechanisms, [one of which](https://wg21.link/P0786) Outcome implements so code using Expected will seamlessly
interoperate with code using Outcome.

6. Outcome was designed with the benefit of hindsight after Optional and Expected,
where how those do implicit conversions have been found to be prone to writing
unintentionally buggy code. Outcome simultaneously permits more implicit conversions
for ease of use and convenience, where those are unambigiously safe, and prevents
other implicit conversions which the Boost peer review reported as dangerous.


## Is Outcome riddled with undefined behaviour for const, const-containing and reference-containing types?

The short answer is not any more in C++ 20 and after, thanks to changes made to
C++ 20 at the Belfast WG21 meeting in November 2019.

The longer answer is that before C++ 20, use of placement
new on types containing `const` member types where the resulting pointer was
thrown away is undefined behaviour. As of the resolution of a national body
comment, this is no longer the case, and now Outcome is free of this particular
UB for C++ 20 onwards.

This still affects C++ before 20, though no major compiler is affected. Still,
if you wish to avoid UB, don't use `const` types within Outcome types (or any
`optional<T>`, or `vector<T>` or any STL container type for that matter).

### More detail

Before the C++ 14 standard, placement new into storage which used to contain
a const type was straight out always undefined behaviour, period. Thus all use of
placement new within a `result<const_containing_type>`, or indeed an `optional<const_containing_type>`, is always
undefined behaviour before C++ 14. From `[basic.life]` for the C++ 11 standard:

> Creating a new object at the storage location that a const object with static, 
> thread, or automatic storage duration occupies or, at the storage location
> that such a const object used to occupy before its lifetime ended results
> in undefined behavior. 

This being excessively restrictive, from C++ 14 onwards, `[basic_life]` now states:

> If, after the lifetime of an object has ended and before the storage which
> the object occupied is reused or released, a new object is created at the
> storage location which the original object occupied, a pointer that
> pointed to the original object, a reference that referred to the original
> object, or the name of the original object will automatically refer to the
> new object and, once the lifetime of the new object has started, can be
> used to manipulate the new object, if:
>
>   — the storage for the new object exactly overlays the storage location which
>     the original object occupied, and
>
>   — the new object is of the same type as the original object (ignoring the
>     top-level cv-qualifiers), and
>
>   — the type of the original object is not const-qualified, and, if a class type,
>     does not contain any non-static data member whose type is const-qualified
>     or a reference type, and
>
>   — neither the original object nor the new object is a potentially-overlapping
>     subobject

Leaving aside my personal objections to giving placement new of non-const
non-reference types magical pointer renaming powers, the upshot is that if
you want defined behaviour for placement new of types containing const types
or references, you must store the pointer returned by placement new, and use
that pointer for all further reference to the newly created object. This
obviously adds eight bytes of storage to a `result<const_containing_type>`, which is highly
undesirable given all the care and attention paid to keeping it small. The alternative
is to use {{% api "std::launder" %}}, which was added in C++ 17, to 'launder'
the storage into which we placement new before each and every use of that
storage. This forces the compiler to reload the object stored by placement
new on every occasion, and not assume it can be constant propagated, which
impacts codegen quality.

As mentioned above, this issue (in so far as it applies to types containing
user supplied `T` which might be `const`) has been resolved as of C++ 20 onwards,
and it is extremely unlikely that any C++ compiler will act on any UB here in
C++ 17 or 14 given how much of STL containers would break.
