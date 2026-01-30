# Highway implementation details

[TOC]

## Introduction

This doc explains some of the Highway implementation details; understanding them
is mainly useful for extending the library. Bear in mind that Highway is a thin
wrapper over 'intrinsic functions' provided by the compiler.

## Vectors vs. tags

The key to understanding Highway is to differentiate between vectors and
zero-sized tag arguments. The former store actual data and are mapped by the
compiler to vector registers. The latter (`Simd<>` and `SizeTag<>`) are only
used to select among the various overloads of functions such as `Set`. This
allows Highway to use builtin vector types without a class wrapper.

Class wrappers are problematic for SVE and RVV because LLVM (or at least Clang)
does not allow member variables whose type is 'sizeless' (in particular,
built-in vectors). To our knowledge, Highway is the only C++ vector library that
supports SVE and RISC-V without compiler flags that indicate what the runtime
vector length will be. Such flags allow the compiler to convert the previously
sizeless vectors to known-size vector types, which can then be wrapped in
classes, but this only makes sense for use-cases where the exact hardware is
known and rarely changes (e.g. supercomputers). By contrast, Highway can run on
unknown hardware such as heterogeneous clouds or client devices without
requiring a recompile, nor multiple binaries.

Note that Highway does use class wrappers where possible, in particular NEON,
WASM and x86. The wrappers (e.g. Vec128) are in fact required on some platforms
(x86 and perhaps WASM) because Highway assumes the vector arguments passed e.g.
to `Add` provide sufficient type information to identify the appropriate
intrinsic. By contrast, x86's loosely typed `__m128i` built-in type could
actually refer to any integer lane type. Because some targets use wrappers and
others do not, incorrect user code may compile on some platforms but not others.
This is because passing class wrappers as arguments triggers argument-dependent
lookup, which would find the `Add` function even without namespace qualifiers
because it resides in the same namespace as the wrapper. Correct user code
qualifies each call to a Highway op, e.g. with a namespace alias `hn`, so
`hn::Add`. This works for both wrappers and built-in vector types.

## Adding a new target

Adding a target requires updating about ten locations: adding a macro constant
to identify it, hooking it into static and dynamic dispatch, detecting support
at runtime, and identifying the target name. The easiest and safest way to do
this is to search for one of the target identifiers such as `HWY_AVX3_DL`, and
add corresponding logic for your new target. Note the upper limits on the number
of targets per platform imposed by `HWY_MAX_DYNAMIC_TARGETS`.

## When to use -inl.h

By convention, files whose name ends with `-inl.h` contain vector code in the
form of inlined function templates. In order to support the multiple compilation
required for dynamic dispatch on platforms which provide several targets, such
files generally begin with a 'per-target include guard' of the form:

```
#if defined(HWY_PATH_NAME_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HWY_PATH_NAME_INL_H_
#undef HWY_PATH_NAME_INL_H_
#else
#define HWY_PATH_NAME_INL_H_
#endif
// contents to include once per target
#endif  // HWY_PATH_NAME_INL_H_
```

This toggles the include guard between defined and undefined, which is
sufficient to 'reset' the include guard when beginning a new 'compilation pass'
for the next target. This is accomplished by simply re-#including the user's
translation unit, which may in turn `#include` one or more `-inl.h` files. As an
exception, `hwy/ops/*-inl.h` do not require include guards because they are all
included from highway.h, which takes care of this in a single location. Note
that platforms such as RISC-V which currently only offer a single target do not
require multiple compilation, but the same mechanism is used without actually
re-#including. For both of those platforms, it is possible that additional
targets will later be added, which means this mechanism will then be required.

Instead of a -inl.h file, you can also use a normal .cc/.h component, where the
vector code is hidden inside the .cc file, and the header only declares a normal
non-template function whose implementation does `HWY_DYNAMIC_DISPATCH` into the
vector code. For an example of this, see
[vqsort.cc](../hwy/contrib/sort/vqsort.cc).

Considerations for choosing between these alternatives are similar to those for
regular headers. Inlining and thus `-inl.h` makes sense for short functions, or
when the function must support many input types and is defined as a template.
Conversely, non-inline `.cc` files make sense when the function is very long
(such that call overhead does not matter), and/or is only required for a small
set of input types. [Math functions](../hwy/contrib/math/math-inl.h)
can fall into either case, hence we provide both inline functions and `Call*`
wrappers.

## Use of macros

Highway ops are implemented for up to 12 lane types, which can make for
considerable repetition - even more so for RISC-V, which can have seven times as
many variants (one per LMUL in `[1/8, 8]`). The various backends
(implementations of one or more targets) differ in their strategies for handling
this, in increasing order of macro complexity:

*   `x86_*` and `wasm_*` simply write out all the overloads, which is
    straightforward but results in 4K-6K line files.

*   [arm_sve-inl.h](../hwy/ops/arm_sve-inl.h) defines 'type list'
    macros `HWY_SVE_FOREACH*` to define all overloads for most ops in a single
    line. Such an approach makes sense because SVE ops are quite orthogonal
    (i.e. generally defined for all types and consistent).

*   [arm_neon-inl.h](../hwy/ops/arm_neon-inl.h) also uses type list
    macros, but with a more general 'function builder' which helps to define
    custom function templates required for 'unusual' ops such as `ShiftLeft`.

*   [rvv-inl.h](../hwy/ops/rvv-inl.h) has the most complex system
    because it deals with both type lists and LMUL, plus support for widening or
    narrowing operations. The type lists thus have additional arguments, and
    there are also additional lists for LMUL which can be extended or truncated.

## Code reuse across targets

The set of Highway ops is carefully chosen such that most of them map to a
single platform-specific intrinsic. However, there are some important functions
such as `AESRound` which may require emulation, and are non-trivial enough that
we don't want to copy them into each target's implementation. Instead, we
implement such functions in
[generic_ops-inl.h](../hwy/ops/generic_ops-inl.h), which is included
into every backend. To allow some targets to override these functions, we use
the same per-target include guard mechanism, e.g. `HWY_NATIVE_AES`.

The functions there are typically templated on the vector and/or tag types. This
is necessary because the vector type depends on the target. Although `Vec128` is
available on most targets, `HWY_SCALAR`, `HWY_RVV` and `HWY_SVE*` lack this
type. To enable specialized overloads (e.g. only for signed integers), we use
the `HWY_IF` SFINAE helpers. Example: `template <class V, class D = DFromV<V>,
HWY_IF_SIGNED_D(D)>`. Note that there is a limited set of `HWY_IF` that work
directly with vectors, identified by their `_V` suffix. However, the functions
likely use a `D` type anyway, thus it is convenient to obtain one in the
template arguments and also use that for `HWY_IF_*_D`.

For x86, we also avoid some duplication by implementing only once the functions
which are shared between all targets. They reside in
[x86_128-inl.h](../hwy/ops/x86_128-inl.h) and are also templated on the
vector type.

## Adding a new op

Adding an op consists of three steps, listed below. As an example, consider
https://github.com/google/highway/commit/6c285d64ae50e0f48866072ed3a476fc12df5ab6.

1) Document the new op in `g3doc/quick_reference.md` with its function signature
and a description of what the op does.

2) Implement the op in each `ops/*-inl.h` header. There are two exceptions,
detailed in the previous section: first, `generic_ops-inl.h` is not changed in
the common case where the op has a unique definition for every target. Second,
if the op's definition would be duplicated in `x86_256-inl.h` and
`x86_512-inl.h`, it may be expressed as a template in `x86_128-inl.h` with a
`class V` template argument, e.g. `TableLookupBytesOr0`.

3) Pick the appropriate `hwy/tests/*_test.cc` and add a test. This is also a
three step process: first define a functor that implements the test logic (e.g.
`TestPlusMinus`), then a function (e.g. `TestAllPlusMinus`) that invokes this
functor for all lane types the op supports, and finally a line near the end of
the file that invokes the function for all targets:
`HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllPlusMinus);`. Note the naming
convention that the function has the same name as the functor except for the
`TestAll` prefix.

## Reducing the number of overloads via templates

Most ops are supported for many types. Often it is possible to reuse the same
implementation. When this works for every possible type, we simply use a
template. C++ provides several mechanisms for constraining the types:

*   We can extend templates with SFINAE. Highway provides some internal-only
    `HWY_IF_*` macros for this, e.g. `template <typename T, HWY_IF_FLOAT(T)>
    bool IsFiniteT(T t) {`. Variants of these with `_D` and `_V` suffixes exist
    for when the argument is a tag or vector type. Although convenient and
    fairly readable, this style sometimes encounters limits in compiler support,
    especially with older MSVC.

*   When the implementation is lengthy and only a few types are supported, it
    can make sense to move the implementation into namespace detail and provide
    one non-template overload for each type; each calls the implementation.

*   When the implementation only depends on the size in bits of the lane type
    (instead of whether it is signed/float), we sometimes add overloads with an
    additional `SizeTag` argument to namespace detail, and call those from the
    user-visible template. This may avoid compiler limitations relating to the
    otherwise equivalent `HWY_IF_T_SIZE(T, 1)`.

## Deducing the Simd<> argument type

For functions that take a `d` argument such as `Load`, we usually deduce it as a
`class D` template argument rather than deducing the individual `T`, `N`,
`kPow2` arguments to `Simd`. To obtain `T` e.g. for the pointer argument to
`Load`, use `TFromD<D>`. Rather than `N`, e.g. for stack-allocated arrays on
targets where `!HWY_HAVE_SCALABLE`, use `MaxLanes(d)`, or where no `d` lvalue is
available, `HWY_MAX_LANES_D(D)`.

When there are constraints, such as "only enable when the `D` is exactly 128
bits", be careful not to use `Full128<T>` as the function argument type, because
this will not match `Simd<T, 8 / sizeof(T), 1>`, i.e. twice a half-vector.
Instead use `HWY_IF_V_SIZE_D(D, 16)`.

We could perhaps skip the `HWY_IF_V_SIZE_D` if fixed-size vector or mask
arguments are present, because they already have the same effect of overload
resolution. For example, when the arguments are `Vec256` the overload defined in
x86_256 will be selected. However, also verifying the `D` matches the other
arguments helps prevent erroneous or questionable code from compiling. For
example, passing a different `D` to `Store` than the one used to create the
vector argument might point to an error; both should match.

For functions that accept multiple vector types (these are mainly in x86_128,
and avoid duplicating those functions in x86_256 and x86_512), we use
`VFrom<D>`.

## Documentation of platform-specific intrinsics

When adding a new op, it is often necessary to consult the reference for each
platform's intrinsics.

For x86 targets `HWY_SSE2`, `HWY_SSSE3`, `HWY_SSE4`, `HWY_AVX2`, `HWY_AVX3`,
`HWY_AVX3_DL`, `HWY_AVX3_ZEN4` Intel provides a
[searchable reference](https://www.intel.com/content/www/us/en/docs/intrinsics-guide).

For Arm targets `HWY_NEON`, `HWY_NEON_WITHOUT_AES`, `HWY_SVE` (plus its
specialization for 256-bit vectors `HWY_SVE_256`), `HWY_SVE2` (plus its
specialization for 128-bit vectors `HWY_SVE2_128`), Arm provides a
[searchable reference](https://developer.arm.com/architectures/instruction-sets/intrinsics).

For RISC-V target `HWY_RVV`, we refer to the assembly language
[specification](https://github.com/riscv/riscv-v-spec/blob/master/v-spec.adoc)
plus the separate
[intrinsics specification](https://github.com/riscv-non-isa/rvv-intrinsic-doc).

For WebAssembly target `HWY_WASM`, we recommend consulting the
[intrinsics header](https://github.com/llvm/llvm-project/blob/main/clang/lib/Headers/wasm_simd128.h).
There is also an unofficial
[searchable list of intrinsics](https://nemequ.github.io/waspr/intrinsics).

For POWER targets `HWY_PPC8`, `HWY_PPC9`, `HWY_PPC10`, there is
[documentation of intrinsics](https://files.openpower.foundation/s/9nRDmJgfjM8MpR7),
the [ISA](https://files.openpower.foundation/s/dAYSdGzTfW4j2r2), plus a
[searchable reference](https://www.ibm.com/docs/en/openxl-c-and-cpp-aix/17.1.1?).

## Why scalar target

There can be various reasons to avoid using vector intrinsics:

*   The current CPU may not support any instruction sets generated by Highway
    (on x86, we only target S-SSE3 or newer because its predecessor SSE3 was
    introduced in 2004 and it seems unlikely that many users will want to
    support such old CPUs);
*   The compiler may crash or emit incorrect code for certain intrinsics or
    instruction sets;
*   We may want to estimate the speedup from the vector implementation compared
    to scalar code.

Highway provides either the `HWY_SCALAR` or the `HWY_EMU128` target for such
use-cases. Both implement ops using standard C++ instead of intrinsics. They
differ in the vector size: the former always uses single-lane vectors and thus
cannot implement ops such as `AESRound` or `TableLookupBytes`. The latter
guarantees 16-byte vectors are available like all other Highway targets, and
supports all ops. Both of these alternatives are slower than native vector code,
but they allow testing your code even when actual vectors are unavailable.

One of the above targets is used if the CPU does not support any actual SIMD
target. To avoid compiling any intrinsics, define `HWY_COMPILE_ONLY_EMU128`.

`HWY_SCALAR` is only enabled/used `#ifdef HWY_COMPILE_ONLY_SCALAR` (or `#if
HWY_BROKEN_EMU128`). Projects that intend to use it may require `#if HWY_TARGET
!= HWY_SCALAR` around the ops it does not support to prevent compile errors.
