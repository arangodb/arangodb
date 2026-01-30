# API synopsis / quick reference

[[_TOC_]]

## High-level overview

Highway is a collection of 'ops': platform-agnostic pure functions that operate
on tuples (multiple values of the same type). These functions are implemented
using platform-specific intrinsics, which map to SIMD/vector instructions.
`hwy/contrib` also includes higher-level algorithms such as `FindIf` or `Sorter`
implemented using these ops.

Highway can use dynamic dispatch, which chooses the best available
implementation at runtime, or static dispatch which has no runtime overhead.
Dynamic dispatch works by compiling your code once per target CPU and then
selecting (via indirect call) at runtime.

Examples of both are provided in examples/. Dynamic dispatch uses the same
source code as static, plus `#define HWY_TARGET_INCLUDE`, `#include
"third_party/highway/hwy/foreach_target.h"` (which must come before any
inclusion of highway.h) and `HWY_DYNAMIC_DISPATCH`.

## Headers

The public headers are:

*   hwy/highway.h: main header, included from source AND/OR header files that
    use vector types. Note that including in headers may increase compile time,
    but allows declaring functions implemented out of line.

*   hwy/base.h: included from headers that only need compiler/platform-dependent
    definitions (e.g. `PopCount`) without the full highway.h.

*   hwy/foreach_target.h: re-includes the translation unit (specified by
    `HWY_TARGET_INCLUDE`) once per enabled target to generate code from the same
    source code. highway.h must still be included.

*   hwy/aligned_allocator.h: defines functions for allocating memory with
    alignment suitable for `Load`/`Store`.

*   hwy/cache_control.h: defines stand-alone functions to control caching (e.g.
    prefetching), independent of actual SIMD.

*   hwy/nanobenchmark.h: library for precisely measuring elapsed time (under
    varying inputs) for benchmarking small/medium regions of code.

*   hwy/print-inl.h: defines Print() for writing vector lanes to stderr.

*   hwy/tests/test_util-inl.h: defines macros for invoking tests on all
    available targets, plus per-target functions useful in tests.

Highway provides helper macros to simplify your vector code and ensure support
for dynamic dispatch. To use these, add the following to the start and end of
any vector code:

```
#include "hwy/highway.h"
HWY_BEFORE_NAMESPACE();  // at file scope
namespace project {  // optional
namespace HWY_NAMESPACE {

// implementation

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace project - optional
HWY_AFTER_NAMESPACE();
```

If you choose not to use the `BEFORE/AFTER` lines, you must prefix any function
that calls Highway ops such as `Load` with `HWY_ATTR`. You can omit the
`HWY_NAMESPACE` lines if not using dynamic dispatch.

## Notation in this doc

*   `T` denotes the type of a vector lane (integer or floating-point);
*   `N` is a size_t value that governs (but is not necessarily identical to) the
    number of lanes;
*   `D` is shorthand for a zero-sized tag type `Simd<T, N, kPow2>`, used to
    select the desired overloaded function (see next section). Use aliases such
    as `ScalableTag` instead of referring to this type directly;
*   `d` is an lvalue of type `D`, passed as a function argument e.g. to Zero;
*   `V` is the type of a vector, which may be a class or built-in type.
*   `v[i]` is analogous to C++ array notation, with zero-based index `i` from
    the starting address of the vector `v`.

## Vector and tag types

Highway vectors consist of one or more 'lanes' of the same built-in type
`uint##_t, int##_t` for `## = 8, 16, 32, 64`, plus `float##_t` for `## = 16, 32,
64` and `bfloat16_t`.

Beware that `char` may differ from these types, and is not supported directly.
If your code loads from/stores to `char*`, use `T=uint8_t` for Highway's `d`
tags (see below) or `T=int8_t` (which may enable faster less-than/greater-than
comparisons), and cast your `char*` pointers to your `T*`.

In Highway, `float16_t` (an IEEE binary16 half-float) and `bfloat16_t` (the
upper 16 bits of an IEEE binary32 float) only support load, store, and
conversion to/from `float32_t`. The behavior of infinity and NaN in `float16_t`
is implementation-defined due to Armv7.

On RVV/SVE, vectors are sizeless and cannot be wrapped inside a class. The
Highway API allows using built-in types as vectors because operations are
expressed as overloaded functions. Instead of constructors, overloaded
initialization functions such as `Set` take a zero-sized tag argument called `d`
of type `D` and return an actual vector of unspecified type.

`T` is one of the lane types above, and may be retrieved via `TFromD<D>`.

The actual lane count (used to increment loop counters etc.) can be obtained via
`Lanes(d)`. This value might not be known at compile time, thus storage for
vectors should be dynamically allocated, e.g. via `AllocateAligned(Lanes(d))`.

Note that `Lanes(d)` could potentially change at runtime. This is currently
unlikely, and will not be initiated by Highway without user action, but could
still happen in other circumstances:

*   upon user request in future via special CPU instructions (switching to
    'streaming SVE' mode for Arm SME), or
*   via system software (`prctl(PR_SVE_SET_VL` on Linux for Arm SVE). When the
    vector length is changed using this mechanism, all but the lower 128 bits of
    vector registers are invalidated.

Thus we discourage caching the result; it is typically used inside a function or
basic block. If the application anticipates that one of the above circumstances
could happen, it should ensure by some out-of-band mechanism that such changes
will not happen during the critical section (the vector code which uses the
result of the previously obtained `Lanes(d)`).

`MaxLanes(d)` returns a (potentially loose) upper bound on `Lanes(d)`, and is
implemented as a constexpr function.

The actual lane count is guaranteed to be a power of two, even on SVE hardware
where vectors can be a multiple of 128 bits (there, the extra lanes remain
unused). This simplifies alignment: remainders can be computed as `count &
(Lanes(d) - 1)` instead of an expensive modulo. It also ensures loop trip counts
that are a large power of two (at least `MaxLanes`) are evenly divisible by the
lane count, thus avoiding the need for a second loop to handle remainders.

`d` lvalues (a tag, NOT actual vector) are obtained using aliases:

*   Most common: `ScalableTag<T[, kPow2=0]> d;` or the macro form `HWY_FULL(T[,
    LMUL=1]) d;`. With the default value of the second argument, these both
    select full vectors which utilize all available lanes.

    Only for targets (e.g. RVV) that support register groups, the kPow2 (-3..3)
    and LMUL argument (1, 2, 4, 8) specify `LMUL`, the number of registers in
    the group. This effectively multiplies the lane count in each operation by
    `LMUL`, or left-shifts by `kPow2` (negative values are understood as
    right-shifting by the absolute value). These arguments will eventually be
    optional hints that may improve performance on 1-2 wide machines (at the
    cost of reducing the effective number of registers), but RVV target does not
    yet support fractional `LMUL`. Thus, mixed-precision code (e.g. demoting
    float to uint8_t) currently requires `LMUL` to be at least the ratio of the
    sizes of the largest and smallest type, and smaller `d` to be obtained via
    `Half<DLarger>`.

    For other targets, `kPow2` must lie within [HWY_MIN_POW2, HWY_MAX_POW2]. The
    `*Tag` aliases clamp to the upper bound but your code should ensure the
    lower bound is not exceeded, typically by specializing compile-time
    recursions for `kPow2` = `HWY_MIN_POW2` (this avoids compile errors when
    `kPow2` is low enough that it is no longer a valid shift count).

*   Less common: `CappedTag<T, kCap> d` or the macro form `HWY_CAPPED(T, kCap)
    d;`. These select vectors or masks where *no more than* the largest power of
    two not exceeding `kCap` lanes have observable effects such as
    loading/storing to memory, or being counted by `CountTrue`. The number of
    lanes may also be less; for the `HWY_SCALAR` target, vectors always have a
    single lane. For example, `CappedTag<T, 3>` will use up to two lanes.

*   For applications that require fixed-size vectors: `FixedTag<T, kCount> d;`
    will select vectors where exactly `kCount` lanes have observable effects.
    These may be implemented using full vectors plus additional runtime cost for
    masking in `Load` etc. `kCount` must be a power of two not exceeding
    `HWY_LANES(T)`, which is one for `HWY_SCALAR`. This tag can be used when the
    `HWY_SCALAR` target is anyway disabled (superseded by a higher baseline) or
    unusable (due to use of ops such as `TableLookupBytes`). As a convenience,
    we also provide `Full128<T>`, `Full64<T>` and `Full32<T>` aliases which are
    equivalent to `FixedTag<T, 16 / sizeof(T)>`, `FixedTag<T, 8 / sizeof(T)>`
    and `FixedTag<T, 4 / sizeof(T)>`.

*   The result of `UpperHalf`/`LowerHalf` has half the lanes. To obtain a
    corresponding `d`, use `Half<decltype(d)>`; the opposite is `Twice<>`.

User-specified lane counts or tuples of vectors could cause spills on targets
with fewer or smaller vectors. By contrast, Highway encourages vector-length
agnostic code, which is more performance-portable.

For mixed-precision code (e.g. `uint8_t` lanes promoted to `float`), tags for
the smaller types must be obtained from those of the larger type (e.g. via
`Rebind<uint8_t, ScalableTag<float>>`).

## Using unspecified vector types

Vector types are unspecified and depend on the target. Your code could define
vector variables using `auto`, but it is more readable (due to making the type
visible) to use an alias such as `Vec<D>`, or `decltype(Zero(d))`. Similarly,
the mask type can be obtained via `Mask<D>`. Often your code will first define a
`d` lvalue using `ScalableTag<T>`. You may wish to define an alias for your
vector types such as `using VecT = Vec<decltype(d)>`. Do not use undocumented
types such as `Vec128`; these may work on most targets, but not all (e.g. SVE).

Vectors are sizeless types on RVV/SVE. Therefore, vectors must not be used in
arrays/STL containers (use the lane type `T` instead), class members,
static/thread_local variables, new-expressions (use `AllocateAligned` instead),
and sizeof/pointer arithmetic (increment `T*` by `Lanes(d)` instead).

Initializing constants requires a tag type `D`, or an lvalue `d` of that type.
The `D` can be passed as a template argument or obtained from a vector type `V`
via `DFromV<V>`. `TFromV<V>` is equivalent to `TFromD<DFromV<V>>`.

**Note**: Let `DV = DFromV<V>`. For builtin `V` (currently necessary on
RVV/SVE), `DV` might not be the same as the `D` used to create `V`. In
particular, `DV` must not be passed to `Load/Store` functions because it may
lack the limit on `N` established by the original `D`. However, `Vec<DV>` is the
same as `V`.

Thus a template argument `V` suffices for generic functions that do not load
from/store to memory: `template<class V> V Mul4(V v) { return Mul(v,
Set(DFromV<V>(), 4)); }`.

Example of mixing partial vectors with generic functions:

```
CappedTag<int16_t, 2> d2;
auto v = Mul4(Set(d2, 2));
Store(v, d2, ptr);  // Use d2, NOT DFromV<decltype(v)>()
```

## Targets

Let `Target` denote an instruction set, one of `SCALAR/EMU128`, `RVV`,
`SSE2/SSSE3/SSE4/AVX2/AVX3/AVX3_DL/AVX3_ZEN4` (x86), `PPC8/PPC9/PPC10` (POWER),
`NEON/SVE/SVE2/SVE_256/SVE2_128` (Arm), `WASM/WASM_EMU256`.

Note that x86 CPUs are segmented into dozens of feature flags and capabilities,
which are often used together because they were introduced in the same CPU
(example: AVX2 and FMA). To keep the number of targets and thus compile time and
code size manageable, we define targets as 'clusters' of related features. To
use `HWY_AVX2`, it is therefore insufficient to pass -mavx2. For definitions of
the clusters, see `kGroup*` in `targets.cc`. The corresponding Clang/GCC
compiler options to enable them (without -m prefix) are defined by
`HWY_TARGET_STR*` in `set_macros-inl.h`, and also listed as comments in
https://gcc.godbolt.org/z/rGnjMevKG.

Targets are only used if enabled (i.e. not broken nor disabled). Baseline
targets are those for which the compiler is unconditionally allowed to generate
instructions (implying the target CPU must support them).

*   `HWY_STATIC_TARGET` is the best enabled baseline `HWY_Target`, and matches
    `HWY_TARGET` in static dispatch mode. This is useful even in dynamic
    dispatch mode for deducing and printing the compiler flags.

*   `HWY_TARGETS` indicates which targets to generate for dynamic dispatch, and
    which headers to include. It is determined by configuration macros and
    always includes `HWY_STATIC_TARGET`.

*   `HWY_SUPPORTED_TARGETS` is the set of targets available at runtime. Expands
    to a literal if only a single target is enabled, or SupportedTargets().

*   `HWY_TARGET`: which `HWY_Target` is currently being compiled. This is
    initially identical to `HWY_STATIC_TARGET` and remains so in static dispatch
    mode. For dynamic dispatch, this changes before each re-inclusion and
    finally reverts to `HWY_STATIC_TARGET`. Can be used in `#if` expressions to
    provide an alternative to functions which are not supported by `HWY_SCALAR`.

    In particular, for x86 we sometimes wish to specialize functions for AVX-512
    because it provides many new instructions. This can be accomplished via `#if
    HWY_TARGET <= HWY_AVX3`, which means AVX-512 or better (e.g. `HWY_AVX3_DL`).
    This is because numerically lower targets are better, and no other platform
    has targets numerically less than those of x86.

*   `HWY_WANT_SSSE3`, `HWY_WANT_SSE4`: add SSSE3 and SSE4 to the baseline even
    if they are not marked as available by the compiler. On MSVC, the only ways
    to enable SSSE3 and SSE4 are defining these, or enabling AVX.

*   `HWY_WANT_AVX3_DL`: opt-in for dynamic dispatch to `HWY_AVX3_DL`. This is
    unnecessary if the baseline already includes AVX3_DL.

You can detect and influence the set of supported targets:

*   `TargetName(t)` returns a string literal identifying the single target `t`,
    where `t` is typically `HWY_TARGET`.

*   `SupportedTargets()` returns an int64_t bitfield of enabled targets that are
    supported on this CPU. The return value may change after calling
    `DisableTargets`, but will never be zero.

*   `HWY_SUPPORTED_TARGETS` is equivalent to `SupportedTargets()` but more
    efficient if only a single target is enabled.

*   `DisableTargets(b)` causes subsequent `SupportedTargets()` to not return
    target(s) whose bits are set in `b`. This is useful for disabling specific
    targets if they are unhelpful or undesirable, e.g. due to memory bandwidth
    limitations. The effect is not cumulative; each call overrides the effect of
    all previous calls. Calling with `b == 0` restores the original behavior.
    Use `SetSupportedTargetsForTest` instead of this function for iteratively
    enabling specific targets for testing.

*   `SetSupportedTargetsForTest(b)` causes subsequent `SupportedTargets` to
    return `b`, minus those disabled via `DisableTargets`. `b` is typically
    derived from a subset of `SupportedTargets()`, e.g. each individual bit in
    order to test each supported target. Calling with `b == 0` restores the
    normal `SupportedTargets` behavior.

## Operations

In the following, the argument or return type `V` denotes a vector with `N`
lanes, and `M` a mask. Operations limited to certain vector types begin with a
constraint of the form `V`: `{prefixes}[{bits}]`. The prefixes `u,i,f` denote
unsigned, signed, and floating-point types, and bits indicates the number of
bits per lane: 8, 16, 32, or 64. Any combination of the specified prefixes and
bits are allowed. Abbreviations of the form `u32 = {u}{32}` may also be used.

Note that Highway functions reside in `hwy::HWY_NAMESPACE`, whereas user-defined
functions reside in `project::[nested]::HWY_NAMESPACE`. Highway functions
generally take either a `D` or vector/mask argument. For targets where vectors
and masks are defined in namespace `hwy`, the functions will be found via
Argument-Dependent Lookup. However, this does not work for function templates,
and RVV and SVE both use builtin vectors. There are three options for portable
code, in descending order of preference:

-   `namespace hn = hwy::HWY_NAMESPACE;` alias used to prefix ops, e.g.
    `hn::LoadDup128(..)`;
-   `using hwy::HWY_NAMESPACE::LoadDup128;` declarations for each op used;
-   `using hwy::HWY_NAMESPACE;` directive. This is generally discouraged,
    especially for SIMD code residing in a header.

Note that overloaded operators are not yet supported on RVV and SVE. Until that
is resolved, code that wishes to run on all targets must use the corresponding
equivalents mentioned in the description of each overloaded operator, for
example `Lt` instead of `operator<`.

### Initialization

*   <code>V **Zero**(D)</code>: returns N-lane vector with all bits set to 0.
*   <code>V **Set**(D, T)</code>: returns N-lane vector with all lanes equal to
    the given value of type `T`.
*   <code>V **Undefined**(D)</code>: returns uninitialized N-lane vector, e.g.
    for use as an output parameter.
*   <code>V **Iota**(D, T)</code>: returns N-lane vector where the lane with
    index `i` has the given value of type `T` plus `i`. The least significant
    lane has index 0. This is useful in tests for detecting lane-crossing bugs.
*   <code>V **SignBit**(D, T)</code>: returns N-lane vector with all lanes set
    to a value whose representation has only the most-significant bit set.

### Getting/setting lanes

*   <code>T **GetLane**(V)</code>: returns lane 0 within `V`. This is useful for
    extracting `SumOfLanes` results.

The following may be slow on some platforms (e.g. x86) and should not be used in
time-critical code:

*   <code>T **ExtractLane**(V, size_t i)</code>: returns lane `i` within `V`.
    `i` must be in `[0, Lanes(DFromV<V>()))`. Potentially slow, it may be better
    to store an entire vector to an array and then operate on its elements.

*   <code>V **InsertLane**(V, size_t i, T t)</code>: returns a copy of V whose
    lane `i` is set to `t`. `i` must be in `[0, Lanes(DFromV<V>()))`.
    Potentially slow, it may be better set all elements of an aligned array and
    then `Load` it.

### Printing

*   <code>V **Print**(D, const char* caption, V [, size_t lane][, size_t
    max_lanes])</code>: prints `caption` followed by up to `max_lanes`
    comma-separated lanes from the vector argument, starting at index `lane`.
    Defined in hwy/print-inl.h, also available if hwy/tests/test_util-inl.h has
    been included.

### Tuples

As a partial workaround to the "no vectors as class members" compiler limitation
mentioned in "Using unspecified vector types", we provide special types able to
carry 2, 3 or 4 vectors, denoted `Tuple{2-4}` below. Their type is unspecified,
potentially built-in, so use the aliases `Vec{2-4}&lt;D&gt;`. These can (only)
be passed as arguments or returned from functions, and created/accessed using
the functions in this section.

*   <code>Tuple2 **Create2**(D, V v0, V v1)</code>: returns tuple such that
    `Get2<1>(tuple)` returns `v1`.
*   <code>Tuple3 **Create3**(D, V v0, V v1, V v2)</code>: returns tuple such
    that `Get3<2>(tuple)` returns `v2`.
*   <code>Tuple4 **Create4**(D, V v0, V v1, V v2, V v3)</code>: returns tuple
    such that `Get4<3>(tuple)` returns `v3`.

The following take a `size_t` template argument indicating the zero-based index,
from left to right, of the arguments passed to `Create{2-4}`.

*   <code>V **Get2&lt;size_t&gt;**(Tuple2)</code>: returns the i-th vector
    passed to `Create2`.
*   <code>V **Get3&lt;size_t&gt;**(Tuple3)</code>: returns the i-th vector
    passed to `Create3`.
*   <code>V **Get4&lt;size_t&gt;**(Tuple4)</code>: returns the i-th vector
    passed to `Create4`.

### Arithmetic

*   <code>V **operator+**(V a, V b)</code>: returns `a[i] + b[i]` (mod 2^bits).
    Currently unavailable on SVE/RVV; use the equivalent `Add` instead.
*   <code>V **operator-**(V a, V b)</code>: returns `a[i] - b[i]` (mod 2^bits).
    Currently unavailable on SVE/RVV; use the equivalent `Sub` instead.

*   `V`: `{i,f}` \
    <code>V **Neg**(V a)</code>: returns `-a[i]`.

*   `V`: `{i,f}` \
    <code>V **Abs**(V a)</code> returns the absolute value of `a[i]`; for
    integers, `LimitsMin()` maps to `LimitsMax() + 1`.

*   `V`: `{u,i}{8,16,32,64},f32` \
    <code>V **AbsDiff**(V a, V b)</code>: returns `|a[i] - b[i]|` in each lane.

*   `V`: `u8` \
    <code>VU64 **SumsOf8**(V v)</code> returns the sums of 8 consecutive u8
    lanes, zero-extending each sum into a u64 lane. This is slower on RVV/WASM.

*   `V`: `u8` \
    <code>VU64 **SumsOf8AbsDiff**(V a, V b)</code> returns the same result as
    `SumsOf8(AbsDiff(a, b))` but `SumsOf8AbsDiff(a, b)` is more efficient than
    `SumsOf8(AbsDiff(a, b))` on SSSE3/SSE4/AVX2/AVX3.

*   `V`: `{u,i}{8,16}` \
    <code>V **SaturatedAdd**(V a, V b)</code> returns `a[i] + b[i]` saturated to
    the minimum/maximum representable value.

*   `V`: `{u,i}{8,16}` \
    <code>V **SaturatedSub**(V a, V b)</code> returns `a[i] - b[i]` saturated to
    the minimum/maximum representable value.

*   `V`: `{u}{8,16}` \
    <code>V **AverageRound**(V a, V b)</code> returns `(a[i] + b[i] + 1) / 2`.

*   <code>V **Clamp**(V a, V lo, V hi)</code>: returns `a[i]` clamped to
    `[lo[i], hi[i]]`.

*   `V`: `{f}` \
    <code>V **operator/**(V a, V b)</code>: returns `a[i] / b[i]` in each lane.
    Currently unavailable on SVE/RVV; use the equivalent `Div` instead.

*   `V`: `{f}` \
    <code>V **Sqrt**(V a)</code>: returns `sqrt(a[i])`.

*   `V`: `f32` \
    <code>V **ApproximateReciprocalSqrt**(V a)</code>: returns an approximation
    of `1.0 / sqrt(a[i])`. `sqrt(a) ~= ApproximateReciprocalSqrt(a) * a`. x86
    and PPC provide 12-bit approximations but the error on Arm is closer to 1%.

*   `V`: `f32` \
    <code>V **ApproximateReciprocal**(V a)</code>: returns an approximation of
    `1.0 / a[i]`.

#### Min/Max

**Note**: Min/Max corner cases are target-specific and may change. If either
argument is qNaN, x86 SIMD returns the second argument, Armv7 Neon returns NaN,
Wasm is supposed to return NaN but does not always, but other targets actually
uphold IEEE 754-2019 minimumNumber: returning the other argument if exactly one
is qNaN, and NaN if both are.

*   <code>V **Min**(V a, V b)</code>: returns `min(a[i], b[i])`.

*   <code>V **Max**(V a, V b)</code>: returns `max(a[i], b[i])`.

All other ops in this section are only available if `HWY_TARGET != HWY_SCALAR`:

*   `V`: `u64` \
    <code>V **Min128**(D, V a, V b)</code>: returns the minimum of unsigned
    128-bit values, each stored as an adjacent pair of 64-bit lanes (e.g.
    indices 1 and 0, where 0 is the least-significant 64-bits).

*   `V`: `u64` \
    <code>V **Max128**(D, V a, V b)</code>: returns the maximum of unsigned
    128-bit values, each stored as an adjacent pair of 64-bit lanes (e.g.
    indices 1 and 0, where 0 is the least-significant 64-bits).

*   `V`: `u64` \
    <code>V **Min128Upper**(D, V a, V b)</code>: for each 128-bit key-value
    pair, returns `a` if it is considered less than `b` by Lt128Upper, else `b`.

*   `V`: `u64` \
    <code>V **Max128Upper**(D, V a, V b)</code>: for each 128-bit key-value
    pair, returns `a` if it is considered > `b` by Lt128Upper, else `b`.

#### Multiply

*   <code>V <b>operator*</b>(V a, V b)</code>: returns `r[i] = a[i] * b[i]`,
    truncating it to the lower half for integer inputs. Currently unavailable on
    SVE/RVV; use the equivalent `Mul` instead.

*   `V`: `i16` \
    <code>V **MulHigh**(V a, V b)</code>: returns the upper half of `a[i] *
    b[i]` in each lane.

*   `V`: `i16` \
    <code>V **MulFixedPoint15**(V a, V b)</code>: returns the result of
    multiplying two Q1.15 fixed-point numbers. This corresponds to doubling the
    multiplication result and storing the upper half. Results are
    implementation-defined iff both inputs are -32768.

*   `V`: `{u,i}{32},u64` \
    <code>V2 **MulEven**(V a, V b)</code>: returns double-wide result of `a[i] *
    b[i]` for every even `i`, in lanes `i` (lower) and `i + 1` (upper). `V2` is
    a vector with double-width lanes, or the same as `V` for 64-bit inputs
    (which are only supported if `HWY_TARGET != HWY_SCALAR`).

*   `V`: `u64` \
    <code>V **MulOdd**(V a, V b)</code>: returns double-wide result of `a[i] *
    b[i]` for every odd `i`, in lanes `i - 1` (lower) and `i` (upper). Only
    supported if `HWY_TARGET != HWY_SCALAR`.

*   `V`: `{bf,i}16`, `D`: `RepartitionToWide<DFromV<V>>`, `VW`: `Vec<D>` \
    <code>VW **ReorderWidenMulAccumulate**(D d, V a, V b, VW sum0, VW&
    sum1)</code>: widens `a` and `b` to `TFromD<D>`, then adds `a[i] * b[i]` to
    either `sum1[j]` or lane `j` of the return value, where `j = P(i)` and `P`
    is a permutation. The only guarantee is that `SumOfLanes(d,
    Add(return_value, sum1))` is the sum of all `a[i] * b[i]`. This is useful
    for computing dot products and the L2 norm. The initial value of `sum1`
    before any call to `ReorderWidenMulAccumulate` must be zero (because it is
    unused on some platforms). It is safe to set the initial value of `sum0` to
    any vector `v`; this has the effect of increasing the total sum by
    `GetLane(SumOfLanes(d, v))` and may be slightly more efficient than later
    adding `v` to `sum0`.

*   `VW`: `{f,i}32` \
    <code>VW **RearrangeToOddPlusEven**(VW sum0, VW sum1)</code>: returns in
    each 32-bit lane with index `i` `a[2*i+1]*b[2*i+1] + a[2*i+0]*b[2*i+0]`.
    `sum0` must be the return value of a prior `ReorderWidenMulAccumulate`, and
    `sum1` must be its last (output) argument. In other words, this strengthens
    the invariant of `ReorderWidenMulAccumulate` such that each 32-bit lane is
    the sum of the widened products whose 16-bit inputs came from the top and
    bottom halves of the 32-bit lane. This is typically called after a series of
    calls to `ReorderWidenMulAccumulate`, as opposed to after each one.
    Exception: if `HWY_TARGET == HWY_SCALAR`, returns `a[0]*b[0]`. Note that the
    initial value of `sum1` must be zero, see `ReorderWidenMulAccumulate`.

#### Fused multiply-add

When implemented using special instructions, these functions are more precise
and faster than separate multiplication followed by addition. The `*Sub`
variants are somewhat slower on Arm, and unavailable for integer inputs; if the
`c` argument is a constant, it would be better to negate it and use `MulAdd`.

*   <code>V **MulAdd**(V a, V b, V c)</code>: returns `a[i] * b[i] + c[i]`.

*   <code>V **NegMulAdd**(V a, V b, V c)</code>: returns `-a[i] * b[i] + c[i]`.

*   `V`: `{f}` \
    <code>V **MulSub**(V a, V b, V c)</code>: returns `a[i] * b[i] - c[i]`.

*   `V`: `{f}` \
    <code>V **NegMulSub**(V a, V b, V c)</code>: returns `-a[i] * b[i] - c[i]`.

#### Shifts

**Note**: Counts not in `[0, sizeof(T)*8)` yield implementation-defined results.
Left-shifting signed `T` and right-shifting positive signed `T` is the same as
shifting `MakeUnsigned<T>` and casting to `T`. Right-shifting negative signed
`T` is the same as an unsigned shift, except that 1-bits are shifted in.

Compile-time constant shifts: the amount must be in [0, sizeof(T)*8). Generally
the most efficient variant, but 8-bit shifts are potentially slower than other
lane sizes, and `RotateRight` is often emulated with shifts:

*   `V`: `{u,i}` \
    <code>V **ShiftLeft**&lt;int&gt;(V a)</code> returns `a[i] << int`.

*   `V`: `{u,i}` \
    <code>V **ShiftRight**&lt;int&gt;(V a)</code> returns `a[i] >> int`.

*   `V`: `{u}` \
    <code>V **RotateRight**&lt;int&gt;(V a)</code> returns `(a[i] >> int) |
    (a[i] << (sizeof(T)*8 - int))`.

Shift all lanes by the same (not necessarily compile-time constant) amount:

*   `V`: `{u,i}` \
    <code>V **ShiftLeftSame**(V a, int bits)</code> returns `a[i] << bits`.

*   `V`: `{u,i}` \
    <code>V **ShiftRightSame**(V a, int bits)</code> returns `a[i] >> bits`.

Per-lane variable shifts (slow if SSSE3/SSE4, or 16-bit, or Shr i64 on AVX2):

*   `V`: `{u}, {i}{16,32,64}` \
    <code>V **operator<<**(V a, V b)</code> returns `a[i] << b[i]`. Currently
    unavailable on SVE/RVV; use the equivalent `Shl` instead.

*   `V`: `{u}, {i}{16,32,64}` \
    <code>V **operator>>**(V a, V b)</code> returns `a[i] >> b[i]`. Currently
    unavailable on SVE/RVV; use the equivalent `Shr` instead.

#### Floating-point rounding

*   `V`: `{f}` \
    <code>V **Round**(V v)</code>: returns `v[i]` rounded towards the nearest
    integer, with ties to even.

*   `V`: `{f}` \
    <code>V **Trunc**(V v)</code>: returns `v[i]` rounded towards zero
    (truncate).

*   `V`: `{f}` \
    <code>V **Ceil**(V v)</code>: returns `v[i]` rounded towards positive
    infinity (ceiling).

*   `V`: `{f}` \
    <code>V **Floor**(V v)</code>: returns `v[i]` rounded towards negative
    infinity.

#### Floating-point classification

*   `V`: `{f}` \
    <code>M **IsNaN**(V v)</code>: returns mask indicating whether `v[i]` is
    "not a number" (unordered).

*   `V`: `{f}` \
    <code>M **IsInf**(V v)</code>: returns mask indicating whether `v[i]` is
    positive or negative infinity.

*   `V`: `{f}` \
    <code>M **IsFinite**(V v)</code>: returns mask indicating whether `v[i]` is
    neither NaN nor infinity, i.e. normal, subnormal or zero. Equivalent to
    `Not(Or(IsNaN(v), IsInf(v)))`.

### Logical

*   `V`: `{u,i}` \
    <code>V **PopulationCount**(V a)</code>: returns the number of 1-bits in
    each lane, i.e. `PopCount(a[i])`.

*   `V`: `{u,i}` \
    <code>V **LeadingZeroCount**(V a)</code>: returns the number of
    leading zeros in each lane. For any lanes where ```a[i]``` is zero,
    ```sizeof(TFromV<V>) * 8``` is returned in the corresponding result lanes.

*   `V`: `{u,i}` \
    <code>V **TrailingZeroCount**(V a)</code>: returns the number of
    trailing zeros in each lane. For any lanes where ```a[i]``` is zero,
    ```sizeof(TFromV<V>) * 8``` is returned in the corresponding result lanes.

*   `V`: `{u,i}` \
    <code>V **HighestSetBitIndex**(V a)</code>: returns the index of
    the highest set bit of each lane. For any lanes of a signed vector type
    where ```a[i]``` is zero, an unspecified negative value is returned in the
    corresponding result lanes. For any lanes of an unsigned vector type
    where ```a[i]``` is zero, an unspecified value that is greater than
    ```HighestValue<MakeSigned<TFromV<V>>>()``` is returned in the
    corresponding result lanes.

The following operate on individual bits within each lane. Note that the
non-operator functions (`And` instead of `&`) must be used for floating-point
types, and on SVE/RVV.

*   `V`: `{u,i}` \
    <code>V **operator&**(V a, V b)</code>: returns `a[i] & b[i]`. Currently
    unavailable on SVE/RVV; use the equivalent `And` instead.

*   `V`: `{u,i}` \
    <code>V **operator|**(V a, V b)</code>: returns `a[i] | b[i]`. Currently
    unavailable on SVE/RVV; use the equivalent `Or` instead.

*   `V`: `{u,i}` \
    <code>V **operator^**(V a, V b)</code>: returns `a[i] ^ b[i]`. Currently
    unavailable on SVE/RVV; use the equivalent `Xor` instead.

*   `V`: `{u,i}` \
    <code>V **Not**(V v)</code>: returns `~v[i]`.

*   <code>V **AndNot**(V a, V b)</code>: returns `~a[i] & b[i]`.

The following three-argument functions may be more efficient than assembling
them from 2-argument functions:

*   <code>V **Xor3**(V x1, V x2, V x3)</code>: returns `x1[i] ^ x2[i] ^ x3[i]`.
    This is more efficient than `Or3` on some targets. When inputs are disjoint
    (no bit is set in more than one argument), `Xor3` and `Or3` are equivalent
    and you should use the former.
*   <code>V **Or3**(V o1, V o2, V o3)</code>: returns `o1[i] | o2[i] | o3[i]`.
    This is less efficient than `Xor3` on some targets; use that where possible.
*   <code>V **OrAnd**(V o, V a1, V a2)</code>: returns `o[i] | (a1[i] & a2[i])`.
*   <code>V **BitwiseIfThenElse**(V mask, V yes, V no)</code>: returns
    `((mask[i] & yes[i]) | (~mask[i] & no[i]))`. `BitwiseIfThenElse` is more
    efficient than `Or(And(mask, yes), AndNot(mask, no))` on some targets.

    `BitwiseIfThenElse(mask, yes, no)` is guaranteed to return the same result
    as `Or(And(mask, yes), AndNot(mask, no))` on all targets

Special functions for signed types:

*   `V`: `{f}` \
    <code>V **CopySign**(V a, V b)</code>: returns the number with the magnitude
    of `a` and sign of `b`.

*   `V`: `{f}` \
    <code>V **CopySignToAbs**(V a, V b)</code>: as above, but potentially
    slightly more efficient; requires the first argument to be non-negative.

*   `V`: `i32/64` \
    <code>V **BroadcastSignBit**(V a)</code> returns `a[i] < 0 ? -1 : 0`.

*   `V`: `{f}` \
    <code>V **ZeroIfNegative**(V v)</code>: returns `v[i] < 0 ? 0 : v[i]`.

*   `V`: `{i,f}` \
    <code>V **IfNegativeThenElse**(V v, V yes, V no)</code>: returns `v[i] < 0 ?
    yes[i] : no[i]`. This may be more efficient than `IfThenElse(Lt..)`.

### Masks

Let `M` denote a mask capable of storing a logical true/false for each lane (the
encoding depends on the platform).

#### Creation

*   <code>M **FirstN**(D, size_t N)</code>: returns mask with the first `N`
    lanes (those with index `< N`) true. `N >= Lanes(D())` results in an
    all-true mask. `N` must not exceed
    `LimitsMax<SignedFromSize<HWY_MIN(sizeof(size_t), sizeof(TFromD<D>))>>()`.
    Useful for implementing "masked" stores by loading `prev` followed by
    `IfThenElse(FirstN(d, N), what_to_store, prev)`.

*   <code>M **MaskFromVec**(V v)</code>: returns false in lane `i` if `v[i] ==
    0`, or true if `v[i]` has all bits set. The result is
    *implementation-defined* if `v[i]` is neither zero nor all bits set.

*   <code>M **LoadMaskBits**(D, const uint8_t* p)</code>: returns a mask
    indicating whether the i-th bit in the array is set. Loads bytes and bits in
    ascending order of address and index. At least 8 bytes of `p` must be
    readable, but only `(Lanes(D()) + 7) / 8` need be initialized. Any unused
    bits (happens if `Lanes(D()) < 8`) are treated as if they were zero.

#### Conversion

*   <code>M1 **RebindMask**(D, M2 m)</code>: returns same mask bits as `m`, but
    reinterpreted as a mask for lanes of type `TFromD<D>`. `M1` and `M2` must
    have the same number of lanes.

*   <code>V **VecFromMask**(D, M m)</code>: returns 0 in lane `i` if `m[i] ==
    false`, otherwise all bits set.

*   <code>size_t **StoreMaskBits**(D, M m, uint8_t* p)</code>: stores a bit
    array indicating whether `m[i]` is true, in ascending order of `i`, filling
    the bits of each byte from least to most significant, then proceeding to the
    next byte. Returns the number of bytes written: `(Lanes(D()) + 7) / 8`. At
    least 8 bytes of `p` must be writable.

#### Testing

*   <code>bool **AllTrue**(D, M m)</code>: returns whether all `m[i]` are true.

*   <code>bool **AllFalse**(D, M m)</code>: returns whether all `m[i]` are
    false.

*   <code>size_t **CountTrue**(D, M m)</code>: returns how many of `m[i]` are
    true [0, N]. This is typically more expensive than AllTrue/False.

*   <code>intptr_t **FindFirstTrue**(D, M m)</code>: returns the index of the
    first (i.e. lowest index) `m[i]` that is true, or -1 if none are.

*   <code>size_t **FindKnownFirstTrue**(D, M m)</code>: returns the index of the
    first (i.e. lowest index) `m[i]` that is true. Requires `!AllFalse(d, m)`,
    otherwise results are undefined. This is typically more efficient than
    `FindFirstTrue`.

*   <code>intptr_t **FindLastTrue**(D, M m)</code>: returns the index of the
    last (i.e. highest index) `m[i]` that is true, or -1 if none are.

*   <code>size_t **FindKnownLastTrue**(D, M m)</code>: returns the index of the
    last (i.e. highest index) `m[i]` that is true. Requires `!AllFalse(d, m)`,
    otherwise results are undefined. This is typically more efficient than
    `FindLastTrue`.

#### Ternary operator

For `IfThen*`, masks must adhere to the invariant established by `MaskFromVec`:
false is zero, true has all bits set:

*   <code>V **IfThenElse**(M mask, V yes, V no)</code>: returns `mask[i] ?
    yes[i] : no[i]`.

*   <code>V **IfThenElseZero**(M mask, V yes)</code>: returns `mask[i] ?
    yes[i] : 0`.

*   <code>V **IfThenZeroElse**(M mask, V no)</code>: returns `mask[i] ? 0 :
    no[i]`.

*   <code>V **IfVecThenElse**(V mask, V yes, V no)</code>: equivalent to and
    possibly faster than `IfVecThenElse(MaskFromVec(mask), yes, no)`. The result
    is *implementation-defined* if `mask[i]` is neither zero nor all bits set.

#### Logical

*   <code>M **Not**(M m)</code>: returns mask of elements indicating whether the
    input mask element was false.

*   <code>M **And**(M a, M b)</code>: returns mask of elements indicating
    whether both input mask elements were true.

*   <code>M **AndNot**(M not_a, M b)</code>: returns mask of elements indicating
    whether `not_a` is false and `b` is true.

*   <code>M **Or**(M a, M b)</code>: returns mask of elements indicating whether
    either input mask element was true.

*   <code>M **Xor**(M a, M b)</code>: returns mask of elements indicating
    whether exactly one input mask element was true.

*   <code>M **ExclusiveNeither**(M a, M b)</code>: returns mask of elements
    indicating `a` is false and `b` is false. Undefined if both are true. We
    choose not to provide NotOr/NotXor because x86 and SVE only define one of
    these operations. This op is for situations where the inputs are known to be
    mutually exclusive.

#### Compress

*   <code>V **Compress**(V v, M m)</code>: returns `r` such that `r[n]` is
    `v[i]`, with `i` the n-th lane index (starting from 0) where `m[i]` is true.
    Compacts lanes whose mask is true into the lower lanes. For targets and lane
    type `T` where `CompressIsPartition<T>::value` is true, the upper lanes are
    those whose mask is false (thus `Compress` corresponds to partitioning
    according to the mask). Otherwise, the upper lanes are
    implementation-defined. Potentially slow with 8 and 16-bit lanes. Use this
    form when the input is already a mask, e.g. returned by a comparison.

*   <code>V **CompressNot**(V v, M m)</code>: equivalent to `Compress(v,
    Not(m))` but possibly faster if `CompressIsPartition<T>::value` is true.

*   `V`: `u64` \
    <code>V **CompressBlocksNot**(V v, M m)</code>: equivalent to
    `CompressNot(v, m)` when `m` is structured as adjacent pairs (both true or
    false), e.g. as returned by `Lt128`. This is a no-op for 128 bit vectors.
    Unavailable if `HWY_TARGET == HWY_SCALAR`.

*   <code>size_t **CompressStore**(V v, M m, D d, T* p)</code>: writes lanes
    whose mask `m` is true into `p`, starting from lane 0. Returns `CountTrue(d,
    m)`, the number of valid lanes. May be implemented as `Compress` followed by
    `StoreU`; lanes after the valid ones may still be overwritten! Potentially
    slow with 8 and 16-bit lanes.

*   <code>size_t **CompressBlendedStore**(V v, M m, D d, T* p)</code>: writes
    only lanes whose mask `m` is true into `p`, starting from lane 0. Returns
    `CountTrue(d, m)`, the number of lanes written. Does not modify subsequent
    lanes, but there is no guarantee of atomicity because this may be
    implemented as `Compress, LoadU, IfThenElse(FirstN), StoreU`.

*   <code>V **CompressBits**(V v, const uint8_t* HWY_RESTRICT bits)</code>:
    Equivalent to, but often faster than `Compress(v, LoadMaskBits(d, bits))`.
    `bits` is as specified for `LoadMaskBits`. If called multiple times, the
    `bits` pointer passed to this function must also be marked `HWY_RESTRICT` to
    avoid repeated work. Note that if the vector has less than 8 elements,
    incrementing `bits` will not work as intended for packed bit arrays. As with
    `Compress`, `CompressIsPartition` indicates the mask=false lanes are moved
    to the upper lanes. Potentially slow with 8 and 16-bit lanes.

*   <code>size_t **CompressBitsStore**(V v, const uint8_t* HWY_RESTRICT bits, D
    d, T* p)</code>: combination of `CompressStore` and `CompressBits`, see
    remarks there.

#### Expand

*   <code>V **Expand**(V v, M m)</code>: returns `r` such that `r[i]` is zero
    where `m[i]` is false, and otherwise `v[s]`, where `s` is the number of
    `m[0, i)` which are true. Scatters inputs in ascending index order to the
    lanes whose mask is true and zeros all other lanes. Potentially slow with 8
    and 16-bit lanes.

*   <code>V **LoadExpand**(M m, D d, const T* p)</code>: returns `r` such that
    `r[i]` is zero where `m[i]` is false, and otherwise `p[s]`, where `s` is the
    number of `m[0, i)` which are true. May be implemented as `LoadU` followed
    by `Expand`. Potentially slow with 8 and 16-bit lanes.

### Comparisons

These return a mask (see above) indicating whether the condition is true.

*   <code>M **operator==**(V a, V b)</code>: returns `a[i] == b[i]`. Currently
    unavailable on SVE/RVV; use the equivalent `Eq` instead.
*   <code>M **operator!=**(V a, V b)</code>: returns `a[i] != b[i]`. Currently
    unavailable on SVE/RVV; use the equivalent `Ne` instead.

*   <code>M **operator&lt;**(V a, V b)</code>: returns `a[i] < b[i]`. Currently
    unavailable on SVE/RVV; use the equivalent `Lt` instead.

*   <code>M **operator&gt;**(V a, V b)</code>: returns `a[i] > b[i]`. Currently
    unavailable on SVE/RVV; use the equivalent `Gt` instead.

*   <code>M **operator&lt;=**(V a, V b)</code>: returns `a[i] <= b[i]`.
    Currently unavailable on SVE/RVV; use the equivalent `Le` instead.

*   <code>M **operator&gt;=**(V a, V b)</code>: returns `a[i] >= b[i]`.
    Currently unavailable on SVE/RVV; use the equivalent `Ge` instead.

*   `V`: `{u,i}` \
    <code>M **TestBit**(V v, V bit)</code>: returns `(v[i] & bit[i]) == bit[i]`.
    `bit[i]` must have exactly one bit set.

*   `V`: `u64` \
    <code>M **Lt128**(D, V a, V b)</code>: for each adjacent pair of 64-bit
    lanes (e.g. indices 1,0), returns whether `a[1]:a[0]` concatenated to an
    unsigned 128-bit integer (least significant bits in `a[0]`) is less than
    `b[1]:b[0]`. For each pair, the mask lanes are either both true or both
    false. Unavailable if `HWY_TARGET == HWY_SCALAR`.

*   `V`: `u64` \
    <code>M **Lt128Upper**(D, V a, V b)</code>: for each adjacent pair of 64-bit
    lanes (e.g. indices 1,0), returns whether `a[1]` is less than `b[1]`. For
    each pair, the mask lanes are either both true or both false. This is useful
    for comparing 64-bit keys alongside 64-bit values. Only available if
    `HWY_TARGET != HWY_SCALAR`.

*   `V`: `u64` \
    <code>M **Eq128**(D, V a, V b)</code>: for each adjacent pair of 64-bit
    lanes (e.g. indices 1,0), returns whether `a[1]:a[0]` concatenated to an
    unsigned 128-bit integer (least significant bits in `a[0]`) equals
    `b[1]:b[0]`. For each pair, the mask lanes are either both true or both
    false. Unavailable if `HWY_TARGET == HWY_SCALAR`.

*   `V`: `u64` \
    <code>M **Ne128**(D, V a, V b)</code>: for each adjacent pair of 64-bit
    lanes (e.g. indices 1,0), returns whether `a[1]:a[0]` concatenated to an
    unsigned 128-bit integer (least significant bits in `a[0]`) differs from
    `b[1]:b[0]`. For each pair, the mask lanes are either both true or both
    false. Unavailable if `HWY_TARGET == HWY_SCALAR`.

*   `V`: `u64` \
    <code>M **Eq128Upper**(D, V a, V b)</code>: for each adjacent pair of 64-bit
    lanes (e.g. indices 1,0), returns whether `a[1]` equals `b[1]`. For each
    pair, the mask lanes are either both true or both false. This is useful for
    comparing 64-bit keys alongside 64-bit values. Only available if `HWY_TARGET
    != HWY_SCALAR`.

*   `V`: `u64` \
    <code>M **Ne128Upper**(D, V a, V b)</code>: for each adjacent pair of 64-bit
    lanes (e.g. indices 1,0), returns whether `a[1]` differs from `b[1]`. For
    each pair, the mask lanes are either both true or both false. This is useful
    for comparing 64-bit keys alongside 64-bit values. Only available if
    `HWY_TARGET != HWY_SCALAR`.

### Memory

Memory operands are little-endian, otherwise their order would depend on the
lane configuration. Pointers are the addresses of `N` consecutive `T` values,
either `aligned` (address is a multiple of the vector size) or possibly
unaligned (denoted `p`).

Even unaligned addresses must still be a multiple of `sizeof(T)`, otherwise
`StoreU` may crash on some platforms (e.g. RVV and Armv7). Note that C++ ensures
automatic (stack) and dynamically allocated (via `new` or `malloc`) variables of
type `T` are aligned to `sizeof(T)`, hence such addresses are suitable for
`StoreU`. However, casting pointers to `char*` and adding arbitrary offsets (not
a multiple of `sizeof(T)`) can violate this requirement.

**Note**: computations with low arithmetic intensity (FLOP/s per memory traffic
bytes), e.g. dot product, can be *1.5 times as fast* when the memory operands
are aligned to the vector size. An unaligned access may require two load ports.

#### Load

*   <code>Vec&lt;D&gt; **Load**(D, const T* aligned)</code>: returns
    `aligned[i]`. May fault if the pointer is not aligned to the vector size
    (using aligned_allocator.h is safe). Using this whenever possible improves
    codegen on SSSE3/SSE4: unlike `LoadU`, `Load` can be fused into a memory
    operand, which reduces register pressure.

Requires only *element-aligned* vectors (e.g. from malloc/std::vector, or
aligned memory at indices which are not a multiple of the vector length):

*   <code>Vec&lt;D&gt; **LoadU**(D, const T* p)</code>: returns `p[i]`.

*   <code>Vec&lt;D&gt; **LoadDup128**(D, const T* p)</code>: returns one 128-bit
    block loaded from `p` and broadcasted into all 128-bit block\[s\]. This may
    be faster than broadcasting single values, and is more convenient than
    preparing constants for the actual vector length. Only available if
    `HWY_TARGET != HWY_SCALAR`.

*   <code>Vec&lt;D&gt; **MaskedLoadOr**(V no, M mask, D, const T* p)</code>:
    returns `mask[i] ? p[i] : no[i]`. May fault even where `mask` is false `#if
    HWY_MEM_OPS_MIGHT_FAULT`. If `p` is aligned, faults cannot happen unless the
    entire vector is inaccessible. Assuming no faults, this is equivalent to,
    and potentially more efficient than, `IfThenElse(mask, LoadU(D(), p), no)`.

*   <code>Vec&lt;D&gt; **MaskedLoad**(M mask, D d, const T* p)</code>:
    equivalent to `MaskedLoadOr(Zero(d), mask, d, p)`, but potentially slightly
    more efficient.

#### Store

*   <code>void **Store**(Vec&lt;D&gt; v, D, T* aligned)</code>: copies `v[i]`
    into `aligned[i]`, which must be aligned to the vector size. Writes exactly
    `N * sizeof(T)` bytes.

*   <code>void **StoreU**(Vec&lt;D&gt; v, D, T* p)</code>: as `Store`, but the
    alignment requirement is relaxed to element-aligned (multiple of
    `sizeof(T)`).

*   <code>void **BlendedStore**(Vec&lt;D&gt; v, M m, D d, T* p)</code>: as
    `StoreU`, but only updates `p` where `m` is true. May fault even where
    `mask` is false `#if HWY_MEM_OPS_MIGHT_FAULT`. If `p` is aligned, faults
    cannot happen unless the entire vector is inaccessible. Equivalent to, and
    potentially more efficient than, `StoreU(IfThenElse(m, v, LoadU(d, p)), d,
    p)`. "Blended" indicates this may not be atomic; other threads must not
    concurrently update `[p, p + Lanes(d))` without synchronization.

*   <code>void **SafeFillN**(size_t num, T value, D d, T* HWY_RESTRICT
    to)</code>: Sets `to[0, num)` to `value`. If `num` exceeds `Lanes(d)`, the
    behavior is target-dependent (either filling all, or no more than one
    vector). Potentially more efficient than a scalar loop, but will not fault,
    unlike `BlendedStore`. No alignment requirement. Potentially non-atomic,
    like `BlendedStore`.

*   <code>void **SafeCopyN**(size_t num, D d, const T* HWY_RESTRICT from, T*
    HWY_RESTRICT to)</code>: Copies `from[0, num)` to `to`. If `num` exceeds
    `Lanes(d)`, the behavior is target-dependent (either copying all, or no more
    than one vector). Potentially more efficient than a scalar loop, but will
    not fault, unlike `BlendedStore`. No alignment requirement. Potentially
    non-atomic, like `BlendedStore`.

#### Interleaved

*   <code>void **LoadInterleaved2**(D, const T* p, Vec&lt;D&gt;&amp; v0,
    Vec&lt;D&gt;&amp; v1)</code>: equivalent to `LoadU` into `v0, v1` followed
    by shuffling, such that `v0[0] == p[0], v1[0] == p[1]`.

*   <code>void **LoadInterleaved3**(D, const T* p, Vec&lt;D&gt;&amp; v0,
    Vec&lt;D&gt;&amp; v1, Vec&lt;D&gt;&amp; v2)</code>: as above, but for three
    vectors (e.g. RGB samples).

*   <code>void **LoadInterleaved4**(D, const T* p, Vec&lt;D&gt;&amp; v0,
    Vec&lt;D&gt;&amp; v1, Vec&lt;D&gt;&amp; v2, Vec&lt;D&gt;&amp; v3)</code>: as
    above, but for four vectors (e.g. RGBA).

*   <code>void **StoreInterleaved2**(Vec&lt;D&gt; v0, Vec&lt;D&gt; v1, D, T*
    p)</code>: equivalent to shuffling `v0, v1` followed by two `StoreU()`, such
    that `p[0] == v0[0], p[1] == v1[0]`.

*   <code>void **StoreInterleaved3**(Vec&lt;D&gt; v0, Vec&lt;D&gt; v1,
    Vec&lt;D&gt; v2, D, T* p)</code>: as above, but for three vectors (e.g. RGB
    samples).

*   <code>void **StoreInterleaved4**(Vec&lt;D&gt; v0, Vec&lt;D&gt; v1,
    Vec&lt;D&gt; v2, Vec&lt;D&gt; v3, D, T* p)</code>: as above, but for four
    vectors (e.g. RGBA samples).

#### Scatter/Gather

**Note**: Offsets/indices are of type `VI = Vec<RebindToSigned<D>>` and need not
be unique. The results are implementation-defined if any are negative.

**Note**: Where possible, applications should `Load/Store/TableLookup*` entire
vectors, which is much faster than `Scatter/Gather`. Otherwise, code of the form
`dst[tbl[i]] = F(src[i])` should when possible be transformed to `dst[i] =
F(src[tbl[i]])` because `Scatter` is more expensive than `Gather`.

*   `D`: `{u,i,f}{32,64}` \
    <code>void **ScatterOffset**(Vec&lt;D&gt; v, D, const T* base, VI
    offsets)</code>: stores `v[i]` to the base address plus *byte* `offsets[i]`.

*   `D`: `{u,i,f}{32,64}` \
    <code>void **ScatterIndex**(Vec&lt;D&gt; v, D, const T* base, VI
    indices)</code>: stores `v[i]` to `base[indices[i]]`.

*   `D`: `{u,i,f}{32,64}` \
    <code>Vec&lt;D&gt; **GatherOffset**(D, const T* base, VI offsets)</code>:
    returns elements of base selected by *byte* `offsets[i]`.

*   `D`: `{u,i,f}{32,64}` \
    <code>Vec&lt;D&gt; **GatherIndex**(D, const T* base, VI indices)</code>:
    returns vector of `base[indices[i]]`.

### Cache control

All functions except `Stream` are defined in cache_control.h.

*   <code>void **Stream**(Vec&lt;D&gt; a, D d, const T* aligned)</code>: copies
    `a[i]` into `aligned[i]` with non-temporal hint if available (useful for
    write-only data; avoids cache pollution). May be implemented using a
    CPU-internal buffer. To avoid partial flushes and unpredictable interactions
    with atomics (for example, see Intel SDM Vol 4, Sec. 8.1.2.2), call this
    consecutively for an entire cache line (typically 64 bytes, aligned to its
    size). Each call may write a multiple of `HWY_STREAM_MULTIPLE` bytes, which
    can exceed `Lanes(d) * sizeof(T)`. The new contents of `aligned` may not be
    visible until `FlushStream` is called.

*   <code>void **FlushStream**()</code>: ensures values written by previous
    `Stream` calls are visible on the current core. This is NOT sufficient for
    synchronizing across cores; when `Stream` outputs are to be consumed by
    other core(s), the producer must publish availability (e.g. via mutex or
    atomic_flag) after `FlushStream`.

*   <code>void **FlushCacheline**(const void* p)</code>: invalidates and flushes
    the cache line containing "p", if possible.

*   <code>void **Prefetch**(const T* p)</code>: optionally begins loading the
    cache line containing "p" to reduce latency of subsequent actual loads.

*   <code>void **Pause**()</code>: when called inside a spin-loop, may reduce
    power consumption.

### Type conversion

*   <code>Vec&lt;D&gt; **BitCast**(D, V)</code>: returns the bits of `V`
    reinterpreted as type `Vec<D>`.

*   <code>Vec&lt;D&gt; **ResizeBitCast**(D, V)</code>: resizes `V` to a vector
    of `Lanes(D()) * sizeof(TFromD<D>)` bytes, and then returns the bits of the
    resized vector reinterpreted as type `Vec<D>`.

    If `Vec<D>` is a larger vector than `V`, then the contents of any bytes
    past the first `Lanes(DFromV<V>()) * sizeof(TFromV<V>)` bytes of the result
    vector is unspecified.

*   <code>Vec&lt;DTo&gt; **ZeroExtendResizeBitCast**(DTo, DFrom, V)</code>:
    resizes `V`, which is a vector of type `Vec<DFrom>`, to a vector of
    `Lanes(D()) * sizeof(TFromD<D>)` bytes, and then returns the bits of the
    resized vector reinterpreted as type `Vec<DTo>`.

    If `Lanes(DTo()) * sizeof(TFromD<DTo>)` is greater than
    `Lanes(DFrom()) * sizeof(TFromD<DFrom>)`, then any bytes past the first
    `Lanes(DFrom()) * sizeof(TFromD<DFrom>)` bytes of the result vector are
    zeroed out.

*   `V`,`V8`: (`u32,u8`) \
    <code>V8 **U8FromU32**(V)</code>: special-case `u32` to `u8` conversion when
    all lanes of `V` are already clamped to `[0, 256)`.

*   `V`,`D`: (`i32`,`f32`), (`i64`,`f64`) \
    <code>Vec&lt;D&gt; **ConvertTo**(D, V)</code>: converts an integer value to
    same-sized floating point.

*   `V`,`D`: (`f32`,`i32`), (`f64`,`i64`) \
    <code>Vec&lt;D&gt; **ConvertTo**(D, V)</code>: rounds floating point towards
    zero and converts the value to same-sized integer. Returns the closest
    representable value if the input exceeds the destination range.

*   `V`: `f32`; `Ret`: `i32` \
    <code>Ret **NearestInt**(V a)</code>: returns the integer nearest to `a[i]`;
    results are undefined for NaN.

#### Single vector demotion

These functions demote a full vector (or parts thereof) into a vector of half
the size. Use `Rebind<MakeNarrow<T>, D>` or `Half<RepartitionToNarrow<D>>` to
obtain the `D` that describes the return type.

*   `V`,`D`: (`u64,u32`), (`u64,u16`), (`u64,u8`), (`u32,u16`), (`u32,u8`),
    (`u16,u8`) \
    <code>Vec&lt;D&gt; **TruncateTo**(D, V v)</code>: returns `v[i]` truncated
    to the smaller type indicated by `T = TFromD<D>`, with the same result as if
    the more-significant input bits that do not fit in `T` had been zero.
    Example: `ScalableTag<uint32_t> du32; Rebind<uint8_t> du8; TruncateTo(du8,
    Set(du32, 0xF08F))` is the same as `Set(du8, 0x8F)`.

*   `V`,`D`: (`i16,i8`), (`i32,i8`), (`i64,i8`), (`i32,i16`), (`i64,i16`),
    (`i64,i32`), (`u16,i8`), (`u32,i8`), (`u64,i8`), (`u32,i16`), (`u64,i16`),
    (`u64,i32`), (`i16,u8`), (`i32,u8`), (`i64,u8`), (`i32,u16`), (`i64,u16`),
    (`i64,u32`), (`u16,u8`), (`u32,u8`), (`u64,u8`), (`u32,u16`), (`u64,u16`),
    (`u64,u32`), (`f64,f32`) \
    <code>Vec&lt;D&gt; **DemoteTo**(D, V v)</code>: returns `v[i]` after packing
    with signed/unsigned saturation to `MakeNarrow<T>`.

*   `V`,`D`: `f64,i32` \
    <code>Vec&lt;D&gt; **DemoteTo**(D, V v)</code>: rounds floating point
    towards zero and converts the value to 32-bit integers. Returns the closest
    representable value if the input exceeds the destination range.

*   `V`,`D`: (`f32,f16`), (`f32,bf16`) \
    <code>Vec&lt;D&gt; **DemoteTo**(D, V v)</code>: narrows float to half (for
    bf16, it is unspecified whether this truncates or rounds).

#### Single vector promotion

These functions promote a half vector to a full vector. To obtain halves, use
`LowerHalf` or `UpperHalf`, or load them using a half-sized `D`.

*   Unsigned `V` to wider signed/unsigned `D`; signed to wider signed, `f16` to
    `f32`, `bf16` to `f32`, `f32` to `f64` \
    <code>Vec&lt;D&gt; **PromoteTo**(D, V part)</code>: returns `part[i]` zero-
    or sign-extended to the integer type `MakeWide<T>`, or widened to the
    floating-point type `MakeWide<T>`.

The following may be more convenient or efficient than also calling `LowerHalf`
/ `UpperHalf`:

*   `V`,`D`: (`bf16,f32`) \
    <code>Vec&lt;D&gt; **PromoteLowerTo**(D, V v)</code>: returns `v[i]` widened
    to `MakeWide<T>`, for i in `[0, Lanes(D()))`. Note that `V` has twice as
    many lanes as `D` and the return value.

*   `V`,`D`: (`bf16,f32`) \
    <code>Vec&lt;D&gt; **PromoteUpperTo**(D, V v)</code>: returns `v[i]` widened
    to `MakeWide<T>`, for i in `[Lanes(D()), 2 * Lanes(D()))`. Note that `V` has
    twice as many lanes as `D` and the return value.

#### Two-vector demotion

*   `V`,`D`: (`i16,i8`), (`i32,i16`), (`i64,i32`), (`u16,i8`), (`u32,i16`),
    (`u64,i32`), (`i16,u8`), (`i32,u16`), (`i64,u32`), (`u16,u8`), (`u32,u16`),
    (`u64,u32`), (`f32,bf16`) \
    <code>Vec&lt;D&gt; **ReorderDemote2To**(D, V a, V b)</code>: as above, but
    converts two inputs, `D` and the output have twice as many lanes as `V`, and
    the output order is some permutation of the inputs. Only available if
    `HWY_TARGET != HWY_SCALAR`.

*   `V`,`D`: (`i16,i8`), (`i32,i16`), (`i64,i32`), (`u16,i8`), (`u32,i16`),
    (`u64,i32`), (`i16,u8`), (`i32,u16`), (`i64,u32`), (`u16,u8`), (`u32,u16`),
    (`u64,u32`), (`f32,bf16`) \
    <code>Vec&lt;D&gt; **OrderedDemote2To**(D d, V a, V b)</code>: as above, but
    converts two inputs, `D` and the output have twice as many lanes as `V`, and
    the output order is the result of demoting the elements of `a` in the lower
    half of the result followed by the result of demoting the elements of `b` in
    the upper half of the result. `OrderedDemote2To(d, a, b)` is equivalent to
    `Combine(d, DemoteTo(Half<D>(), b), DemoteTo(Half<D>(), a))`, but
    `OrderedDemote2To(d, a, b)` is typically more efficient than `Combine(d,
    DemoteTo(Half<D>(), b), DemoteTo(Half<D>(), a))`. Only available if
    `HWY_TARGET != HWY_SCALAR`.

*   `V`,`D`: (`u16,u8`), (`u32,u16`), (`u64,u32`), \
    <code>Vec&lt;D&gt; **OrderedTruncate2To**(D d, V a, V b)</code>: as above,
    but converts two inputs, `D` and the output have twice as many lanes as `V`,
    and the output order is the result of truncating the elements of `a` in the
    lower half of the result followed by the result of truncating the elements
    of `b` in the upper half of the result. `OrderedTruncate2To(d, a, b)` is
    equivalent to `Combine(d, TruncateTo(Half<D>(), b), TruncateTo(Half<D>(),
    a))`, but `OrderedTruncate2To(d, a, b)` is typically more efficient than
    `Combine(d, TruncateTo(Half<D>(), b), TruncateTo(Half<D>(), a))`. Only
    available if `HWY_TARGET != HWY_SCALAR`.

### Combine

*   <code>V2 **LowerHalf**([D, ] V)</code>: returns the lower half of the vector
    `V`. The optional `D` (provided for consistency with `UpperHalf`) is
    `Half<DFromV<V>>`.

All other ops in this section are only available if `HWY_TARGET != HWY_SCALAR`:

*   <code>V2 **UpperHalf**(D, V)</code>: returns upper half of the vector `V`,
    where `D` is `Half<DFromV<V>>`.

*   <code>V **ZeroExtendVector**(D, V2)</code>: returns vector whose `UpperHalf`
    is zero and whose `LowerHalf` is the argument; `D` is `Twice<DFromV<V2>>`.

*   <code>V **Combine**(D, V2, V2)</code>: returns vector whose `UpperHalf` is
    the first argument and whose `LowerHalf` is the second argument; `D` is
    `Twice<DFromV<V2>>`.

**Note**: the following operations cross block boundaries, which is typically
more expensive on AVX2/AVX-512 than per-block operations.

*   <code>V **ConcatLowerLower**(D, V hi, V lo)</code>: returns the
    concatenation of the lower halves of `hi` and `lo` without splitting into
    blocks. `D` is `DFromV<V>`.

*   <code>V **ConcatUpperUpper**(D, V hi, V lo)</code>: returns the
    concatenation of the upper halves of `hi` and `lo` without splitting into
    blocks. `D` is `DFromV<V>`.

*   <code>V **ConcatLowerUpper**(D, V hi, V lo)</code>: returns the inner half
    of the concatenation of `hi` and `lo` without splitting into blocks. Useful
    for swapping the two blocks in 256-bit vectors. `D` is `DFromV<V>`.

*   <code>V **ConcatUpperLower**(D, V hi, V lo)</code>: returns the outer
    quarters of the concatenation of `hi` and `lo` without splitting into
    blocks. Unlike the other variants, this does not incur a block-crossing
    penalty on AVX2/3. `D` is `DFromV<V>`.

*   <code>V **ConcatOdd**(D, V hi, V lo)</code>: returns the concatenation of
    the odd lanes of `hi` and the odd lanes of `lo`.

*   <code>V **ConcatEven**(D, V hi, V lo)</code>: returns the concatenation of
    the even lanes of `hi` and the even lanes of `lo`.

### Blockwise

**Note**: if vectors are larger than 128 bits, the following operations split
their operands into independently processed 128-bit *blocks*.

*   `V`: `{u,i}{16,32,64}, {f}` \
    <code>V **Broadcast**&lt;int i&gt;(V)</code>: returns individual *blocks*,
    each with lanes set to `input_block[i]`, `i = [0, 16/sizeof(T))`.

All other ops in this section are only available if `HWY_TARGET != HWY_SCALAR`:

*   `V`: `{u,i}` \
    <code>VI **TableLookupBytes**(V bytes, VI indices)</code>: returns
    `bytes[indices[i]]`. Uses byte lanes regardless of the actual vector types.
    Results are implementation-defined if `indices[i] < 0` or `indices[i] >=
    HWY_MIN(Lanes(DFromV<V>()), 16)`. `VI` are integers, possibly of a different
    type than those in `V`. The number of lanes in `V` and `VI` may differ, e.g.
    a full-length table vector loaded via `LoadDup128`, plus partial vector `VI`
    of 4-bit indices.

*   `V`: `{u,i}` \
    <code>VI **TableLookupBytesOr0**(V bytes, VI indices)</code>: returns
    `bytes[indices[i]]`, or 0 if `indices[i] & 0x80`. Uses byte lanes regardless
    of the actual vector types. Results are implementation-defined for
    `indices[i] < 0` or in `[HWY_MIN(Lanes(DFromV<V>()), 16), 0x80)`. The
    zeroing behavior has zero cost on x86 and Arm. For vectors of >= 256 bytes
    (can happen on SVE and RVV), this will set all lanes after the first 128
    to 0. `VI` are integers, possibly of a different type than those in `V`. The
    number of lanes in `V` and `VI` may differ.

#### Interleave

Ops in this section are only available if `HWY_TARGET != HWY_SCALAR`:

*   <code>V **InterleaveLower**([D, ] V a, V b)</code>: returns *blocks* with
    alternating lanes from the lower halves of `a` and `b` (`a[0]` in the
    least-significant lane). The optional `D` (provided for consistency with
    `InterleaveUpper`) is `DFromV<V>`.

*   <code>V **InterleaveUpper**(D, V a, V b)</code>: returns *blocks* with
    alternating lanes from the upper halves of `a` and `b` (`a[N/2]` in the
    least-significant lane). `D` is `DFromV<V>`.

#### Zip

*   `Ret`: `MakeWide<T>`; `V`: `{u,i}{8,16,32}` \
    <code>Ret **ZipLower**([DW, ] V a, V b)</code>: returns the same bits as
    `InterleaveLower`, but repartitioned into double-width lanes (required in
    order to use this operation with scalars). The optional `DW` (provided for
    consistency with `ZipUpper`) is `RepartitionToWide<DFromV<V>>`.

*   `Ret`: `MakeWide<T>`; `V`: `{u,i}{8,16,32}` \
    <code>Ret **ZipUpper**(DW, V a, V b)</code>: returns the same bits as
    `InterleaveUpper`, but repartitioned into double-width lanes (required in
    order to use this operation with scalars). `DW` is
    `RepartitionToWide<DFromV<V>>`. Only available if `HWY_TARGET !=
    HWY_SCALAR`.

#### Shift

Ops in this section are only available if `HWY_TARGET != HWY_SCALAR`:

*   `V`: `{u,i}` \
    <code>V **ShiftLeftBytes**&lt;int&gt;([D, ] V)</code>: returns the result of
    shifting independent *blocks* left by `int` bytes \[1, 15\]. The optional
    `D` (provided for consistency with `ShiftRightBytes`) is `DFromV<V>`.

*   <code>V **ShiftLeftLanes**&lt;int&gt;([D, ] V)</code>: returns the result of
    shifting independent *blocks* left by `int` lanes. The optional `D`
    (provided for consistency with `ShiftRightLanes`) is `DFromV<V>`.

*   `V`: `{u,i}` \
    <code>V **ShiftRightBytes**&lt;int&gt;(D, V)</code>: returns the result of
    shifting independent *blocks* right by `int` bytes \[1, 15\], shifting in
    zeros even for partial vectors. `D` is `DFromV<V>`.

*   <code>V **ShiftRightLanes**&lt;int&gt;(D, V)</code>: returns the result of
    shifting independent *blocks* right by `int` lanes, shifting in zeros even
    for partial vectors. `D` is `DFromV<V>`.

*   `V`: `{u,i}` \
    <code>V **CombineShiftRightBytes**&lt;int&gt;(D, V hi, V lo)</code>: returns
    a vector of *blocks* each the result of shifting two concatenated *blocks*
    `hi[i] || lo[i]` right by `int` bytes \[1, 16). `D` is `DFromV<V>`.

*   <code>V **CombineShiftRightLanes**&lt;int&gt;(D, V hi, V lo)</code>: returns
    a vector of *blocks* each the result of shifting two concatenated *blocks*
    `hi[i] || lo[i]` right by `int` lanes \[1, 16/sizeof(T)). `D` is
    `DFromV<V>`.

#### Shuffle

Ops in this section are only available if `HWY_TARGET != HWY_SCALAR`:

*   `V`: `{u,i,f}{32}` \
    <code>V **Shuffle1032**(V)</code>: returns *blocks* with 64-bit halves
    swapped.

*   `V`: `{u,i,f}{32}` \
    <code>V **Shuffle0321**(V)</code>: returns *blocks* rotated right (toward
    the lower end) by 32 bits.

*   `V`: `{u,i,f}{32}` \
    <code>V **Shuffle2103**(V)</code>: returns *blocks* rotated left (toward the
    upper end) by 32 bits.

The following are equivalent to `Reverse2` or `Reverse4`, which should be used
instead because they are more general:

*   `V`: `{u,i,f}{32}` \
    <code>V **Shuffle2301**(V)</code>: returns *blocks* with 32-bit halves
    swapped inside 64-bit halves.

*   `V`: `{u,i,f}{64}` \
    <code>V **Shuffle01**(V)</code>: returns *blocks* with 64-bit halves
    swapped.

*   `V`: `{u,i,f}{32}` \
    <code>V **Shuffle0123**(V)</code>: returns *blocks* with lanes in reverse
    order.

### Swizzle

*   <code>V **OddEven**(V a, V b)</code>: returns a vector whose odd lanes are
    taken from `a` and the even lanes from `b`.

*   <code>V **OddEvenBlocks**(V a, V b)</code>: returns a vector whose odd
    blocks are taken from `a` and the even blocks from `b`. Returns `b` if the
    vector has no more than one block (i.e. is 128 bits or scalar).

*   `V`: `{u,i,f}{32,64}` \
    <code>V **DupEven**(V v)</code>: returns `r`, the result of copying even
    lanes to the next higher-indexed lane. For each even lane index `i`,
    `r[i] == v[i]` and `r[i + 1] == v[i]`.

*   <code>V **ReverseBlocks**(V v)</code>: returns a vector with blocks in
    reversed order.

*   <code>V **TableLookupLanes**(V a, unspecified)</code> returns a vector
    of `a[indices[i]]`, where `unspecified` is the return value of
    `SetTableIndices(D, &indices[0])` or `IndicesFromVec`. The indices are
    not limited to blocks, hence this is slower than `TableLookupBytes*` on
    AVX2/AVX-512. Results are implementation-defined unless `0 <= indices[i]
    < Lanes(D())`. `indices` are always integers, even if `V` is a
    floating-point type.

*   <code>V **TwoTablesLookupLanes**(D d, V a, V b, unspecified)</code>
    returns a vector of `indices[i] < N ? a[indices[i]] : b[indices[i] - N]`,
    where  `unspecified` is the return value of
    `SetTableIndices(d, &indices[0])` or `IndicesFromVec` and `N` is equal to
    `Lanes(d)`. The indices are not limited to blocks. Results are
    implementation-defined unless `0 <= indices[i] < 2 * Lanes(d)`. `indices`
    are always integers, even if `V` is a floating-point type.

*   <code>V **TwoTablesLookupLanes**(V a, V b, unspecified)</code>
    returns a vector of `indices[i] < N ? a[indices[i]] : b[indices[i] - N]`,
    where  `unspecified` is the return value of
    `SetTableIndices(D, &indices[0])` or `IndicesFromVec` and `N` is equal to
    `Lanes(DFromV<V>())`.  The indices are not limited to blocks. Results are
    implementation-defined unless `0 <= indices[i] < 2 * Lanes(DFromV<V>())`.
    `indices` are always integers, even if `V` is a floating-point type.

    ```TwoTablesLookupLanes(a, b, indices)``` is equivalent to
    ```TwoTablesLookupLanes(DFromV<V>(), a, b, indices)```.

    The results of ```TwoTablesLookupLanes(d, a, b, indices)``` can differ
    from ```TwoTablesLookupLanes(a, b, indices)``` on RVV/SVE if
    ```Lanes(d)``` is less than ```Lanes(DFromV<V>())```.

*   <code>unspecified **IndicesFromVec**(D d, V idx)</code> prepares for
    `TableLookupLanes` with integer indices in `idx`, which must be the same bit
    width as `TFromD<D>` and in the range `[0, 2 * Lanes(d))`, but need not be
    unique.

*   <code>unspecified **SetTableIndices**(D d, TI* idx)</code> prepares for
    `TableLookupLanes` by loading `Lanes(d)` integer indices from `idx`, which
    must be in the range `[0, 2 * Lanes(d))` but need not be unique. The index
    type `TI` must be an integer of the same size as `TFromD<D>`.

*   <code>V **Reverse**(D, V a)</code> returns a vector with lanes in reversed
    order (`out[i] == a[Lanes(D()) - 1 - i]`).

*   `V`: `{u,i}{16,32,64}` \
    <code>V **ReverseLaneBytes**(V a)</code> returns a vector where the bytes of
    each lane are swapped.

*   `V`: `{u,i}{8,16,32,64}` \
    <code>V **ReverseBits**(V a)</code> returns a vector where the bits of each
    lane are reversed.

The following `ReverseN` must not be called if `Lanes(D()) < N`:

*   <code>V **Reverse2**(D, V a)</code> returns a vector with each group of 2
    contiguous lanes in reversed order (`out[i] == a[i ^ 1]`).

*   <code>V **Reverse4**(D, V a)</code> returns a vector with each group of 4
    contiguous lanes in reversed order (`out[i] == a[i ^ 3]`).

*   <code>V **Reverse8**(D, V a)</code> returns a vector with each group of 8
    contiguous lanes in reversed order (`out[i] == a[i ^ 7]`).

All other ops in this section are only available if `HWY_TARGET != HWY_SCALAR`:

*   `V`: `{u,i,f}{32,64}` \
    <code>V **DupOdd**(V v)</code>: returns `r`, the result of copying odd lanes
    to the previous lower-indexed lane. For each odd lane index `i`, `r[i] ==
    v[i]` and `r[i - 1] == v[i]`.

*   <code>V **SwapAdjacentBlocks**(V v)</code>: returns a vector where blocks of
    index `2*i` and `2*i+1` are swapped. Results are undefined for vectors with
    less than two blocks; callers must first check that via `Lanes`.

### Reductions

**Note**: these 'reduce' all lanes to a single result (e.g. sum), which is
broadcasted to all lanes. To obtain a scalar, you can call `GetLane`.

Being a horizontal operation (across lanes of the same vector), these are slower
than normal SIMD operations and are typically used outside critical loops.

There are additional `{u,i}{8}` implementations on SSE4.1+ and NEON.

*   `V`: `{u,i,f}{32,64},{u,i}{16}` \
    <code>V **SumOfLanes**(D, V v)</code>: returns the sum of all lanes in each
    lane.

*   `T`: `{u,i,f}{32,64},{u,i}{16}` \
    <code>T **ReduceSum**(D, V v)</code>: returns the sum of all lanes.

*   `V`: `{u,i,f}{32,64},{u,i}{16}` \
    <code>V **MinOfLanes**(D, V v)</code>: returns the minimum-valued lane in
    each lane.

*   `V`: `{u,i,f}{32,64},{u,i}{16}` \
    <code>V **MaxOfLanes**(D, V v)</code>: returns the maximum-valued lane in
    each lane.

### Crypto

Ops in this section are only available if `HWY_TARGET != HWY_SCALAR`:

*   `V`: `u8` \
    <code>V **AESRound**(V state, V round_key)</code>: one round of AES
    encryption: `MixColumns(SubBytes(ShiftRows(state))) ^ round_key`. This
    matches x86 AES-NI. The latency is independent of the input values.

*   `V`: `u8` \
    <code>V **AESLastRound**(V state, V round_key)</code>: the last round of AES
    encryption: `SubBytes(ShiftRows(state)) ^ round_key`. This matches x86
    AES-NI. The latency is independent of the input values.

*   `V`: `u8` \
    <code>V **AESRoundInv**(V state, V round_key)</code>: one round of AES
    decryption using the AES Equivalent Inverse Cipher:
    `InvMixColumns(InvShiftRows(InvSubBytes(state))) ^ round_key`. This matches
    x86 AES-NI. The latency is independent of the input values.

*   `V`: `u8` \
    <code>V **AESLastRoundInv**(V state, V round_key)</code>: the last round of
    AES decryption: `InvShiftRows(InvSubBytes(state)) ^ round_key`. This matches
    x86 AES-NI. The latency is independent of the input values.

*   `V`: `u8` \
    <code>V **AESInvMixColumns**(V state)</code>: the InvMixColumns operation of
    the AES decryption algorithm. AESInvMixColumns is used in the key expansion
    step of the AES Equivalent Inverse Cipher algorithm. The latency is
    independent of the input values.

*   `V`: `u8` \
    <code>V **AESKeyGenAssist**&lt;uint8_t kRcon&gt;(V v)</code>: AES key
    generation assist operation

    The AESKeyGenAssist operation is equivalent to doing the following, which
    matches the behavior of the x86 AES-NI AESKEYGENASSIST instruction:
    *  Applying the AES SubBytes operation to each byte of `v`.
    *  Doing a TableLookupBytes operation on each 128-bit block of the
       result of the `SubBytes(v)` operation with the following indices
       (which is broadcast to each 128-bit block in the case of vectors with 32
       or more lanes):
       `{4, 5, 6, 7, 5, 6, 7, 4, 12, 13, 14, 15, 13, 14, 15, 12}`
    *  Doing a bitwise XOR operation with the following vector (where `kRcon`
       is the rounding constant that is the first template argument of the
       AESKeyGenAssist function and where the below vector is broadcasted to
       each 128-bit block in the case of vectors with 32 or more lanes):
       `{0, 0, 0, 0, kRcon, 0, 0, 0, 0, 0, 0, 0, kRcon, 0, 0, 0}`

*   `V`: `u64` \
    <code>V **CLMulLower**(V a, V b)</code>: carryless multiplication of the
    lower 64 bits of each 128-bit block into a 128-bit product. The latency is
    independent of the input values (assuming that is true of normal integer
    multiplication) so this can safely be used in crypto. Applications that wish
    to multiply upper with lower halves can `Shuffle01` one of the operands; on
    x86 that is expected to be latency-neutral.

*   `V`: `u64` \
    <code>V **CLMulUpper**(V a, V b)</code>: as CLMulLower, but multiplies the
    upper 64 bits of each 128-bit block.

## Preprocessor macros

*   `HWY_ALIGN`: Prefix for stack-allocated (i.e. automatic storage duration)
    arrays to ensure they have suitable alignment for Load()/Store(). This is
    specific to `HWY_TARGET` and should only be used inside `HWY_NAMESPACE`.

    Arrays should also only be used for partial (<= 128-bit) vectors, or
    `LoadDup128`, because full vectors may be too large for the stack and should
    be heap-allocated instead (see aligned_allocator.h).

    Example: `HWY_ALIGN float lanes[4];`

*   `HWY_ALIGN_MAX`: as `HWY_ALIGN`, but independent of `HWY_TARGET` and may be
    used outside `HWY_NAMESPACE`.

## Advanced macros

*   `HWY_IDE` is 0 except when parsed by IDEs; adding it to conditions such as
    `#if HWY_TARGET != HWY_SCALAR || HWY_IDE` avoids code appearing greyed out.

The following indicate support for certain lane types and expand to 1 or 0:

*   `HWY_HAVE_INTEGER64`: support for 64-bit signed/unsigned integer lanes.
*   `HWY_HAVE_FLOAT16`: support for 16-bit floating-point lanes.
*   `HWY_HAVE_FLOAT64`: support for double-precision floating-point lanes.

The above were previously known as `HWY_CAP_INTEGER64`, `HWY_CAP_FLOAT16`, and
`HWY_CAP_FLOAT64`, respectively. Those `HWY_CAP_*` names are DEPRECATED.

*   `HWY_HAVE_SCALABLE` indicates vector sizes are unknown at compile time, and
    determined by the CPU.

*   `HWY_HAVE_TUPLE` indicates `Vec{2-4}`, `Create{2-4}` and `Get{2-4}` are
    usable. This is already true `#if !HWY_HAVE_SCALABLE`, and for SVE targets,
    and the RVV target when using Clang 16. We anticipate it will also become,
    and then remain, true starting with GCC 14.

*   `HWY_MEM_OPS_MIGHT_FAULT` is 1 iff `MaskedLoad` may trigger a (page) fault
    when attempting to load lanes from unmapped memory, even if the
    corresponding mask element is false. This is the case on ASAN/MSAN builds,
    AMD x86 prior to AVX-512, and Arm NEON. If so, users can prevent faults by
    ensuring memory addresses are aligned to the vector size or at least padded
    (allocation size increased by at least `Lanes(d)`.

*   `HWY_NATIVE_FMA` expands to 1 if the `MulAdd` etc. ops use native fused
    multiply-add for floating-point inputs. Otherwise, `MulAdd(f, m, a)` is
    implemented as `Add(Mul(f, m), a)`. Checking this can be useful for
    increasing the tolerance of expected results (around 1E-5 or 1E-6).

*   `HWY_IS_LITTLE_ENDIAN` expands to 1 on little-endian targets and to 0 on
    big-endian targets.

*   `HWY_IS_BIG_ENDIAN` expands to 0 on big-endian targets and to 1 on
    little-endian targets.

The following were used to signal the maximum number of lanes for certain
operations, but this is no longer necessary (nor possible on SVE/RVV), so they
are DEPRECATED:

*   `HWY_CAP_GE256`: the current target supports vectors of >= 256 bits.
*   `HWY_CAP_GE512`: the current target supports vectors of >= 512 bits.

## Detecting supported targets

`SupportedTargets()` returns a non-cached (re-initialized on each call) bitfield
of the targets supported on the current CPU, detected using CPUID on x86 or
equivalent. This may include targets that are not in `HWY_TARGETS`, and vice
versa. If there is no overlap the binary will likely crash. This can only happen
if:

*   the specified baseline is not supported by the current CPU, which
    contradicts the definition of baseline, so the configuration is invalid; or
*   the baseline does not include the enabled/attainable target(s), which are
    also not supported by the current CPU, and baseline targets (in particular
    `HWY_SCALAR`) were explicitly disabled.

## Advanced configuration macros

The following macros govern which targets to generate. Unless specified
otherwise, they may be defined per translation unit, e.g. to disable >128 bit
vectors in modules that do not benefit from them (if bandwidth-limited or only
called occasionally). This is safe because `HWY_TARGETS` always includes at
least one baseline target which `HWY_EXPORT` can use.

*   `HWY_DISABLE_CACHE_CONTROL` makes the cache-control functions no-ops.
*   `HWY_DISABLE_BMI2_FMA` prevents emitting BMI/BMI2/FMA instructions. This
    allows using AVX2 in VMs that do not support the other instructions, but
    only if defined for all translation units.

The following `*_TARGETS` are zero or more `HWY_Target` bits and can be defined
as an expression, e.g. `-DHWY_DISABLED_TARGETS=(HWY_SSE4|HWY_AVX3)`.

*   `HWY_BROKEN_TARGETS` defaults to a blocklist of known compiler bugs.
    Defining to 0 disables the blocklist.

*   `HWY_DISABLED_TARGETS` defaults to zero. This allows explicitly disabling
    targets without interfering with the blocklist.

*   `HWY_BASELINE_TARGETS` defaults to the set whose predefined macros are
    defined (i.e. those for which the corresponding flag, e.g. -mavx2, was
    passed to the compiler). If specified, this should be the same for all
    translation units, otherwise the safety check in SupportedTargets (that all
    enabled baseline targets are supported) may be inaccurate.

Zero or one of the following macros may be defined to replace the default
policy for selecting `HWY_TARGETS`:

*   `HWY_COMPILE_ONLY_EMU128` selects only `HWY_EMU128`, which avoids intrinsics
    but implements all ops using standard C++.
*   `HWY_COMPILE_ONLY_SCALAR` selects only `HWY_SCALAR`, which implements
    single-lane-only ops using standard C++.
*   `HWY_COMPILE_ONLY_STATIC` selects only `HWY_STATIC_TARGET`, which
    effectively disables dynamic dispatch.
*   `HWY_COMPILE_ALL_ATTAINABLE` selects all attainable targets (i.e. enabled
    and permitted by the compiler, independently of autovectorization), which
    maximizes coverage in tests.

At most one `HWY_COMPILE_ONLY_*` may be defined. `HWY_COMPILE_ALL_ATTAINABLE`
may also be defined even if one of `HWY_COMPILE_ONLY_*` is, but will then be
ignored.

If none are defined, but `HWY_IS_TEST` is defined, the default is
`HWY_COMPILE_ALL_ATTAINABLE`. Otherwise, the default is to select all attainable
targets except any non-best baseline (typically `HWY_SCALAR`), which reduces
code size.

## Compiler support

Clang and GCC require e.g. -mavx2 flags in order to use SIMD intrinsics.
However, this enables AVX2 instructions in the entire translation unit, which
may violate the one-definition rule and cause crashes. Instead, we use
target-specific attributes introduced via #pragma. Function using SIMD must
reside between `HWY_BEFORE_NAMESPACE` and `HWY_AFTER_NAMESPACE`. Alternatively,
individual functions or lambdas may be prefixed with `HWY_ATTR`.

If you know the SVE vector width and are using static dispatch, you can specify
`-march=armv9-a+sve2-aes -msve-vector-bits=128` and Highway will then use
`HWY_SVE2_128` as the baseline. Similarly, `-march=armv8.2-a+sve
-msve-vector-bits=256` enables the `HWY_SVE_256` specialization for Neoverse V1.
Note that these flags are unnecessary when using dynamic dispatch. Highway will
automatically detect and dispatch to the best available target, including
`HWY_SVE2_128` or `HWY_SVE_256`.

Immediates (compile-time constants) are specified as template arguments to avoid
constant-propagation issues with Clang on Arm.

## Type traits

*   `IsFloat<T>()` returns true if the `T` is a floating-point type.
*   `IsSigned<T>()` returns true if the `T` is a signed or floating-point type.
*   `LimitsMin/Max<T>()` return the smallest/largest value representable in
    integer `T`.
*   `SizeTag<N>` is an empty struct, used to select overloaded functions
    appropriate for `N` bytes.

*   `MakeUnsigned<T>` is an alias for an unsigned type of the same size as `T`.

*   `MakeSigned<T>` is an alias for a signed type of the same size as `T`.

*   `MakeFloat<T>` is an alias for a floating-point type of the same size as
    `T`.

*   `MakeWide<T>` is an alias for a type with twice the size of `T` and the same
    category (unsigned/signed/float).

*   `MakeNarrow<T>` is an alias for a type with half the size of `T` and the
    same category (unsigned/signed/float).

## Memory allocation

`AllocateAligned<T>(items)` returns a unique pointer to newly allocated memory
for `items` elements of POD type `T`. The start address is aligned as required
by `Load/Store`. Furthermore, successive allocations are not congruent modulo a
platform-specific alignment. This helps prevent false dependencies or cache
conflicts. The memory allocation is analogous to using `malloc()` and `free()`
with a `std::unique_ptr` since the returned items are *not* initialized or
default constructed and it is released using `FreeAlignedBytes()` without
calling `~T()`.

`MakeUniqueAligned<T>(Args&&... args)` creates a single object in newly
allocated aligned memory as above but constructed passing the `args` argument to
`T`'s constructor and returning a unique pointer to it. This is analogous to
using `std::make_unique` with `new` but for aligned memory since the object is
constructed and later destructed when the unique pointer is deleted. Typically
this type `T` is a struct containing multiple members with `HWY_ALIGN` or
`HWY_ALIGN_MAX`, or arrays whose lengths are known to be a multiple of the
vector size.

`MakeUniqueAlignedArray<T>(size_t items, Args&&... args)` creates an array of
objects in newly allocated aligned memory as above and constructs every element
of the new array using the passed constructor parameters, returning a unique
pointer to the array. Note that only the first element is guaranteed to be
aligned to the vector size; because there is no padding between elements,
the alignment of the remaining elements depends on the size of `T`.

## Speeding up code for older x86 platforms

Thanks to @dzaima for inspiring this section.

It is possible to improve the performance of your code on older x86 CPUs while
remaining portable to all platforms. These older CPUs might indeed be the ones
for which optimization is most impactful, because modern CPUs are usually faster
and thus likelier to meet performance expectations.

For those without AVX3, preferably avoid `Scatter*`; some algorithms can be
reformulated to use `Gather*` instead. For pre-AVX2, it is also important to
avoid `Gather*`.

It is typically much more efficient to pad arrays and use `Load` instead of
`MaskedLoad` and `Store` instead of `BlendedStore`.

If possible, use signed 8..32 bit types instead of unsigned types for
comparisons and `Min`/`Max`.

Other ops which are considerably more expensive especially on SSSE3, and
preferably avoided if possible: `MulEven`, i32 `Mul`, `Shl`/`Shr`,
`Round`/`Trunc`/`Ceil`/`Floor`, float16 `PromoteTo`/`DemoteTo`, `AESRound`.

Ops which are moderately more expensive on older CPUs: 64-bit
`Abs`/`ShiftRight`/`ConvertTo`, i32->u16 `DemoteTo`, u32->f32 `ConvertTo`,
`Not`, `IfThenElse`, `RotateRight`, `OddEven`, `BroadcastSignBit`.

It is likely difficult to avoid all of these ops (about a fifth of the total).
Apps usually also cannot more efficiently achieve the same result as any op
without using it - this is an explicit design goal of Highway. However,
sometimes it is possible to restructure your code to avoid `Not`, e.g. by
hoisting it outside the SIMD code, or fusing with `AndNot` or `CompressNot`.
