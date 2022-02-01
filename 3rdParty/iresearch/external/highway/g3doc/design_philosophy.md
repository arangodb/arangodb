# Design philosophy

*   Performance is important but not the sole consideration. Anyone who goes to
    the trouble of using SIMD clearly cares about speed. However, portability,
    maintainability and readability also matter, otherwise we would write in
    assembly. We aim for performance within 10-20% of a hand-written assembly
    implementation on the development platform. There is no performance gap vs.
    intrinsics: Highway code can do anything they can. If necessary, you can use
    platform-specific instructions inside `#if HWY_TARGET == HWY_NEON` etc.

*   The guiding principles of C++ are "pay only for what you use" and "leave no
    room for a lower-level language below C++". We apply these by defining a
    SIMD API that ensures operation costs are visible, predictable and minimal.

*   Performance portability is important, i.e. the API should be efficient on
    all target platforms. Unfortunately, common idioms for one platform can be
    inefficient on others. For example: summing lanes horizontally versus
    shuffling. Documenting which operations are expensive does not prevent their
    use, as evidenced by widespread use of `HADDPS`. Performance acceptance
    tests may detect large regressions, but do not help choose the approach
    during initial development. Analysis tools can warn about some potential
    inefficiencies, but likely not all. We instead provide [a carefully chosen
    set of vector types and operations that are efficient on all target
    platforms](instruction_matrix.pdf) (PPC8, SSE4/AVX2+, ARMv8).

*   Future SIMD hardware features are difficult to predict. For example, AVX2
    came with surprising semantics (almost no interaction between 128-bit
    blocks) and AVX-512 added two kinds of predicates (writemask and zeromask).
    To ensure the API reflects hardware realities, we suggest a flexible
    approach that adds new operations as they become commonly available, with
    fallback implementations where necessary.

*   Masking/predication differs between platforms, and it is not clear how
    important the use cases are beyond the ternary operator `IfThenElse`.
    AVX-512/ARM SVE zeromasks are useful, but not supported by P0214R5. We
    provide `IfThen[Zero]Else[Zero]` variants.

*   "Width-agnostic" SIMD is more future-proof than user-specified fixed sizes.
    For example, valarray-like code can iterate over a 1D array with a
    library-specified vector width. This will result in better code when vector
    sizes increase, and matches the direction taken by
    [ARM SVE](https://alastairreid.github.io/papers/sve-ieee-micro-2017.pdf) and
    RiscV V as well as Agner Fog's
    [ForwardCom instruction set proposal](https://goo.gl/CFizWu). However, some
    applications may require fixed sizes, so we also guarantee support for <=
    128-bit vectors in each instruction set.

*   The API and its implementation should be usable and efficient with commonly
    used compilers, including MSVC. For example, we write `ShiftLeft<3>(v)`
    instead of `v << 3` because MSVC 2017 (ARM64) does not propagate the literal
    (https://godbolt.org/g/rKx5Ga). Highway requires function-specific target
    attributes, supported by GCC 4.9 / Clang 3.9 / MSVC 2015.

*   Efficient and safe runtime dispatch is important. Modules such as image or
    video codecs are typically embedded into larger applications such as
    browsers, so they cannot require separate binaries for each CPU. Libraries
    also cannot predict whether the application already uses AVX2 (and pays the
    frequency throttling cost), so this decision must be left to the
    application. Using only the lowest-common denominator instructions
    sacrifices too much performance. Therefore, we provide code paths for
    multiple instruction sets and choose the most suitable at runtime. To reduce
    overhead, dispatch should be hoisted to higher layers instead of checking
    inside every low-level function. Highway supports inlining functions in the
    same file or in `*-inl.h` headers. We generate all code paths from the same
    source to reduce implementation- and debugging cost.

*   Not every CPU need be supported. For example, pre-SSSE3 CPUs are
    increasingly rare and the AVX instruction set is limited to floating-point
    operations. To reduce code size and compile time, we provide specializations
    for S-SSE3, SSE4, AVX2 and AVX-512 instruction sets on x86, plus a scalar
    fallback.

*   Access to platform-specific intrinsics is necessary for acceptance in
    performance-critical projects. We provide conversions to and from intrinsics
    to allow utilizing specialized platform-specific functionality, and simplify
    incremental porting of existing code.

*   The core API should be compact and easy to learn; we provide a [concise
    reference](quick_reference.md).

## Prior API designs

The author has been writing SIMD code since 2002: first via assembly language,
then intrinsics, later Intel's `F32vec4` wrapper, followed by three generations
of custom vector classes. The first used macros to generate the classes, which
reduces duplication but also readability. The second used templates instead.
The third (used in highwayhash and PIK) added support for AVX2 and runtime
dispatch. The current design (used in JPEG XL) enables code generation for
multiple platforms and/or instruction sets from the same source, and improves
runtime dispatch.

## Overloaded function API

Most C++ vector APIs rely on class templates. However, the ARM SVE vector
type is sizeless and cannot be wrapped in a class. We instead rely on overloaded
functions. Overloading based on vector types is also undesirable because SVE
vectors cannot be default-constructed. We instead use a dedicated 'descriptor'
type `Simd` for overloading, abbreviated to `D` for template arguments and
`d` in lvalues.

Note that generic function templates are possible (see generic_ops-inl.h).

## Masks

AVX-512 introduced a major change to the SIMD interface: special mask registers
(one bit per lane) that serve as predicates. It would be expensive to force
AVX-512 implementations to conform to the prior model of full vectors with lanes
set to all one or all zero bits. We instead provide a Mask type that emulates
a subset of this functionality on other platforms at zero cost.

Masks are returned by comparisons and `TestBit`; they serve as the input to
`IfThen*`. We provide conversions between masks and vector lanes. For clarity
and safety, we use FF..FF as the definition of true. To also benefit from
x86 instructions that only require the sign bit of floating-point inputs to be
set, we provide a special `ZeroIfNegative` function.

## Differences vs. [P0214R5](https://goo.gl/zKW4SA) / std::experimental::simd

1.  Allowing the use of built-in vector types by relying on non-member
    functions. By contrast, P0214R5 requires a wrapper class, which does not
    work for sizeless vector types currently used by ARM SVE and Risc-V.

1.  Adding widely used and portable operations such as `AndNot`, `AverageRound`,
    bit-shift by immediates and `IfThenElse`.

1.  Designing the API to avoid or minimize overhead on AVX2/AVX-512 caused by
    crossing 128-bit 'block' boundaries.

1.  Avoiding the need for non-native vectors. By contrast, P0214R5's `simd_cast`
    returns `fixed_size<>` vectors which are more expensive to access because
    they reside on the stack. We can avoid this plus additional overhead on
    ARM/AVX2 by defining width-expanding operations as functions of a vector
    part, e.g. promoting half a vector of `uint8_t` lanes to one full vector of
    `uint16_t`, or demoting full vectors to half vectors with half-width lanes.

1.  Guaranteeing access to the underlying intrinsic vector type. This ensures
    all platform-specific capabilities can be used. P0214R5 instead only
    'encourages' implementations to provide access.

1.  Enabling safe runtime dispatch and inlining in the same binary. P0214R5 is
    based on the Vc library, which does not provide assistance for linking
    multiple instruction sets into the same binary. The Vc documentation
    suggests compiling separate executables for each instruction set or using
    GCC's ifunc (indirect functions). The latter is compiler-specific and risks
    crashes due to ODR violations when compiling the same function with
    different compiler flags. We solve this problem via target-specific
    namespaces and attributes (see HOWTO section below). We also permit a mix of
    static target selection and runtime dispatch for hotspots that may benefit
    from newer instruction sets if available.

1.  Omitting inefficient or non-performance-portable operations such as `hmax`,
    `operator[]`, and unsupported integer comparisons. Applications can often
    replace these operations at lower cost than emulating that exact behavior.

1.  Omitting `long double` types: these are not commonly available in hardware.

1.  Ensuring signed integer overflow has well-defined semantics (wraparound).

1.  Simple header-only implementation and a fraction of the size of the
    Vc library from which P0214 was derived (39K, vs. 92K lines in
    https://github.com/VcDevel/Vc according to the gloc Chrome extension).

1.  Avoiding hidden performance costs. P0214R5 allows implicit conversions from
    integer to float, which costs 3-4 cycles on x86. We make these conversions
    explicit to ensure their cost is visible.

## Other related work

*   [Neat SIMD](http://ieeexplore.ieee.org/stamp/stamp.jsp?arnumber=7568423)
    adopts a similar approach with interchangeable vector/scalar types and
    a compact interface. It allows access to the underlying intrinsics, but
    does not appear to be designed for other platforms than x86.

*   UME::SIMD ([code](https://goo.gl/yPeVZx), [paper](https://goo.gl/2xpZrk))
    also adopts an explicit vectorization model with vector classes.
    However, it exposes the union of all platform capabilities, which makes the
    API harder to learn (209-page spec) and implement (the estimated LOC count
    is [500K](https://goo.gl/1THFRi)). The API is less performance-portable
    because it allows applications to use operations that are inefficient on
    other platforms.

*   Inastemp ([code](https://goo.gl/hg3USM), [paper](https://goo.gl/YcTU7S))
    is a vector library for scientific computing with some innovative features:
    automatic FLOPS counting, and "if/else branches" using lambda functions.
    It supports IBM Power8, but only provides float and double types and does
    not support SVE without assuming the runtime vector size.
