# Efficient and performance-portable vector software

[//]: # (placeholder, do not remove)

Highway is a C++ library that provides portable SIMD/vector intrinsics.

[Documentation](https://google.github.io/highway/en/master/)

## Why

We are passionate about high-performance software. We see major untapped
potential in CPUs (servers, mobile, desktops). Highway is for engineers who want
to reliably and economically push the boundaries of what is possible in
software.

## How

CPUs provide SIMD/vector instructions that apply the same operation to multiple
data items. This can reduce energy usage e.g. *fivefold* because fewer
instructions are executed. We also often see *5-10x* speedups.

Highway makes SIMD/vector programming practical and workable according to these
guiding principles:

**Does what you expect**: Highway is a C++ library with carefully-chosen
functions that map well to CPU instructions without extensive compiler
transformations. The resulting code is more predictable and robust to code
changes/compiler updates than autovectorization.

**Works on widely-used platforms**: Highway supports four architectures; the
same application code can target eight instruction sets, including those with
'scalable' vectors (size unknown at compile time). Highway only requires C++11
and supports four families of compilers. If you would like to use Highway on
other platforms, please raise an issue.

**Flexible to deploy**: Applications using Highway can run on heterogeneous
clouds or client devices, choosing the best available instruction set at
runtime. Alternatively, developers may choose to target a single instruction set
without any runtime overhead. In both cases, the application code is the same
except for swapping `HWY_STATIC_DISPATCH` with `HWY_DYNAMIC_DISPATCH` plus one
line of code.

**Suitable for a variety of domains**: Highway provides an extensive set of
operations, used for image processing (floating-point), compression, video
analysis, linear algebra, cryptography, sorting and random generation. We
recognise that new use-cases may require additional ops and are happy to add
them where it makes sense (e.g. no performance cliffs on some architectures). If
you would like to discuss, please file an issue.

**Rewards data-parallel design**: Highway provides tools such as Gather,
MaskedLoad, and FixedTag to enable speedups for legacy data structures. However,
the biggest gains are unlocked by designing algorithms and data structures for
scalable vectors. Helpful techniques include batching, structure-of-array
layouts, and aligned/padded allocations.

## Examples

Online demos using Compiler Explorer:

-   [multiple targets with dynamic dispatch](https://gcc.godbolt.org/z/zP7MYe9Yf)
    (recommended)
-   [single target using -m flags](https://gcc.godbolt.org/z/rGnjMevKG)

We observe that Highway is referenced in the following open source projects,
found via sourcegraph.com. Most are Github repositories. If you would like to
add your project or link to it directly, feel free to raise an issue or contact
us via the below email.

*   Browsers: Chromium (+Vivaldi), Firefox (+floorp / foxhound / librewolf / Waterfox)
*   Cryptography: google/distributed_point_functions
*   Image codecs: eustas/2im, [Grok JPEG 2000](https://github.com/GrokImageCompression/grok), [JPEG XL](https://github.com/libjxl/libjxl), OpenHTJ2K
*   Image processing: cloudinary/ssimulacra2, m-ab-s/media-autobuild_suite
*   Image viewers: AlienCowEatCake/ImageViewer, mirillis/jpegxl-wic,
    [Lux panorama/image viewer](https://bitbucket.org/kfj/pv/)
*   Information retrieval: [iresearch database index](https://github.com/iresearch-toolkit/iresearch/blob/e7638e7a4b99136ca41f82be6edccf01351a7223/core/utils/simd_utils.hpp), michaeljclark/zvec

Other

*   [zimt](https://github.com/kfjahnke/zimt): C++11 template library to process n-dimensional arrays with multi-threaded SIMD code
*   [vectorized Quicksort](https://github.com/google/highway/tree/master/hwy/contrib/sort) ([paper](https://arxiv.org/abs/2205.05982))

If you'd like to get Highway, in addition to cloning from this Github repository
or using it as a Git submodule, you can also find it in the following package
managers or repositories: alpinelinux, conan-io, conda-forge, DragonFlyBSD,
freebsd, ghostbsd, microsoft/vcpkg, MidnightBSD, NetBSD, openSUSE, opnsense,
Xilinx/Vitis_Libraries.

## Current status

### Targets

Highway supports 19 targets, listed in alphabetical order of platform:

-   Any: `EMU128`, `SCALAR`;
-   Arm: `NEON` (Armv7+), `SVE`, `SVE2`, `SVE_256`, `SVE2_128`;
-   POWER: `PPC8` (v2.07), `PPC9` (v3.0), `PPC10` (v3.1B, not yet supported
    due to compiler bugs, see #1207; also requires QEMU 7.2);
-   RISC-V: `RVV` (1.0);
-   WebAssembly: `WASM`, `WASM_EMU256` (a 2x unrolled version of wasm128,
    enabled if `HWY_WANT_WASM2` is defined. This will remain supported until it
    is potentially superseded by a future version of WASM.);
-   x86:
    -   `SSE2`
    -   `SSSE3` (~Intel Core)
    -   `SSE4` (~Nehalem, also includes AES + CLMUL).
    -   `AVX2` (~Haswell, also includes BMI2 + F16 + FMA)
    -   `AVX3` (~Skylake, AVX-512F/BW/CD/DQ/VL)
    -   `AVX3_DL` (~Icelake, includes BitAlg + CLMUL + GFNI + VAES + VBMI +
        VBMI2 + VNNI + VPOPCNT; requires opt-in by defining `HWY_WANT_AVX3_DL`
        unless compiling for static dispatch),
    -   `AVX3_ZEN4` (like AVX3_DL but optimized for AMD Zen4; requires opt-in by
        defining `HWY_WANT_AVX3_ZEN4` if compiling for static dispatch)

SVE was initially tested using farm_sve (see acknowledgments).

### Versioning

Highway releases aim to follow the semver.org system (MAJOR.MINOR.PATCH),
incrementing MINOR after backward-compatible additions and PATCH after
backward-compatible fixes. We recommend using releases (rather than the Git tip)
because they are tested more extensively, see below.

The current version 1.0 signals an increased focus on backwards compatibility.
Applications using documented functionality will remain compatible with future
updates that have the same major version number.

### Testing

Continuous integration tests build with a recent version of Clang (running on
native x86, or QEMU for RISC-V and Arm) and MSVC 2019 (v19.28, running on native
x86).

Before releases, we also test on x86 with Clang and GCC, and Armv7/8 via GCC
cross-compile. See the [testing process](g3doc/release_testing_process.md) for
details.

### Related modules

The `contrib` directory contains SIMD-related utilities: an image class with
aligned rows, a math library (16 functions already implemented, mostly
trigonometry), and functions for computing dot products and sorting.

### Other libraries

If you only require x86 support, you may also use Agner Fog's
[VCL vector class library](https://github.com/vectorclass). It includes many
functions including a complete math library.

If you have existing code using x86/NEON intrinsics, you may be interested in
[SIMDe](https://github.com/simd-everywhere/simde), which emulates those
intrinsics using other platforms' intrinsics or autovectorization.

## Installation

This project uses CMake to generate and build. In a Debian-based system you can
install it via:

```bash
sudo apt install cmake
```

Highway's unit tests use [googletest](https://github.com/google/googletest).
By default, Highway's CMake downloads this dependency at configuration time.
You can disable this by setting the `HWY_SYSTEM_GTEST` CMake variable to ON and
installing gtest separately:

```bash
sudo apt install libgtest-dev
```

Running cross-compiled tests requires support from the OS, which on Debian is
provided by the `qemu-user-binfmt` package.

To build Highway as a shared or static library (depending on BUILD_SHARED_LIBS),
the standard CMake workflow can be used:

```bash
mkdir -p build && cd build
cmake ..
make -j && make test
```

Or you can run `run_tests.sh` (`run_tests.bat` on Windows).

Bazel is also supported for building, but it is not as widely used/tested.

When building for Armv7, a limitation of current compilers requires you to add
`-DHWY_CMAKE_ARM7:BOOL=ON` to the CMake command line; see #834 and #1032. We
understand that work is underway to remove this limitation.

Building on 32-bit x86 is not officially supported, and AVX2/3 are disabled by
default there. Note that johnplatts has successfully built and run the Highway
tests on 32-bit x86, including AVX2/3, on GCC 7/8 and Clang 8/11/12. On Ubuntu
22.04, Clang 11 and 12, but not later versions, require extra compiler flags
`-m32 -isystem /usr/i686-linux-gnu/include`. Clang 10 and earlier require the
above plus `-isystem /usr/i686-linux-gnu/include/c++/12/i686-linux-gnu`. See
#1279.

## Quick start

You can use the `benchmark` inside examples/ as a starting point.

A [quick-reference page](g3doc/quick_reference.md) briefly lists all operations
and their parameters, and the [instruction_matrix](g3doc/instruction_matrix.pdf)
indicates the number of instructions per operation.

The [FAQ](g3doc/faq.md) answers questions about portability, API design and
where to find more information.

We recommend using full SIMD vectors whenever possible for maximum performance
portability. To obtain them, pass a `ScalableTag<float>` (or equivalently
`HWY_FULL(float)`) tag to functions such as `Zero/Set/Load`. There are two
alternatives for use-cases requiring an upper bound on the lanes:

-   For up to `N` lanes, specify `CappedTag<T, N>` or the equivalent
    `HWY_CAPPED(T, N)`. The actual number of lanes will be `N` rounded down to
    the nearest power of two, such as 4 if `N` is 5, or 8 if `N` is 8. This is
    useful for data structures such as a narrow matrix. A loop is still required
    because vectors may actually have fewer than `N` lanes.

-   For exactly a power of two `N` lanes, specify `FixedTag<T, N>`. The largest
    supported `N` depends on the target, but is guaranteed to be at least
    `16/sizeof(T)`.

Due to ADL restrictions, user code calling Highway ops must either:
*   Reside inside `namespace hwy { namespace HWY_NAMESPACE {`; or
*   prefix each op with an alias such as `namespace hn = hwy::HWY_NAMESPACE;
    hn::Add()`; or
*   add using-declarations for each op used: `using hwy::HWY_NAMESPACE::Add;`.

Additionally, each function that calls Highway ops (such as `Load`) must either
be prefixed with `HWY_ATTR`, OR reside between `HWY_BEFORE_NAMESPACE()` and
`HWY_AFTER_NAMESPACE()`. Lambda functions currently require `HWY_ATTR` before
their opening brace.

The entry points into code using Highway differ slightly depending on whether
they use static or dynamic dispatch.

*   For static dispatch, `HWY_TARGET` will be the best available target among
    `HWY_BASELINE_TARGETS`, i.e. those allowed for use by the compiler (see
    [quick-reference](g3doc/quick_reference.md)). Functions inside
    `HWY_NAMESPACE` can be called using `HWY_STATIC_DISPATCH(func)(args)` within
    the same module they are defined in. You can call the function from other
    modules by wrapping it in a regular function and declaring the regular
    function in a header.

*   For dynamic dispatch, a table of function pointers is generated via the
    `HWY_EXPORT` macro that is used by `HWY_DYNAMIC_DISPATCH(func)(args)` to
    call the best function pointer for the current CPU's supported targets. A
    module is automatically compiled for each target in `HWY_TARGETS` (see
    [quick-reference](g3doc/quick_reference.md)) if `HWY_TARGET_INCLUDE` is
    defined and `foreach_target.h` is included.

When using dynamic dispatch, `foreach_target.h` is included from translation
units (.cc files), not headers. Headers containing vector code shared between
several translation units require a special include guard, for example the
following taken from `examples/skeleton-inl.h`:

```
#if defined(HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#undef HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#else
#define HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#endif

#include "hwy/highway.h"
// Your vector code
#endif
```

By convention, we name such headers `-inl.h` because their contents (often
function templates) are usually inlined.

## Compiler flags

Applications should be compiled with optimizations enabled - without inlining,
SIMD code may slow down by factors of 10 to 100. For clang and GCC, `-O2` is
generally sufficient.

For MSVC, we recommend compiling with `/Gv` to allow non-inlined functions to
pass vector arguments in registers. If intending to use the AVX2 target together
with half-width vectors (e.g. for `PromoteTo`), it is also important to compile
with `/arch:AVX2`. This seems to be the only way to generate VEX-encoded SSE4
instructions on MSVC. Otherwise, mixing VEX-encoded AVX2 instructions and
non-VEX SSE4 may cause severe performance degradation. Unfortunately, the
resulting binary will then require AVX2. Note that no such flag is needed for
clang and GCC because they support target-specific attributes, which we use to
ensure proper VEX code generation for AVX2 targets.

## Strip-mining loops

When vectorizing a loop, an important question is whether and how to deal with
a number of iterations ('trip count', denoted `count`) that does not evenly
divide the vector size `N = Lanes(d)`. For example, it may be necessary to avoid
writing past the end of an array.

In this section, let `T` denote the element type and `d = ScalableTag<T>`.
Assume the loop body is given as a function `template<bool partial, class D>
void LoopBody(D d, size_t index, size_t max_n)`.

"Strip-mining" is a technique for vectorizing a loop by transforming it into an
outer loop and inner loop, such that the number of iterations in the inner loop
matches the vector width. Then, the inner loop is replaced with vector
operations.

Highway offers several strategies for loop vectorization:

*   Ensure all inputs/outputs are padded. Then the (outer) loop is simply

    ```
    for (size_t i = 0; i < count; i += N) LoopBody<false>(d, i, 0);
    ```
    Here, the template parameter and second function argument are not needed.

    This is the preferred option, unless `N` is in the thousands and vector
    operations are pipelined with long latencies. This was the case for
    supercomputers in the 90s, but nowadays ALUs are cheap and we see most
    implementations split vectors into 1, 2 or 4 parts, so there is little cost
    to processing entire vectors even if we do not need all their lanes. Indeed
    this avoids the (potentially large) cost of predication or partial
    loads/stores on older targets, and does not duplicate code.

*   Process whole vectors and include previously processed elements
    in the last vector:
    ```
    for (size_t i = 0; i < count; i += N) LoopBody<false>(d, HWY_MIN(i, count - N), 0);
    ```

    This is the second preferred option provided that `count >= N`
    and `LoopBody` is idempotent. Some elements might be processed twice, but
    a single code path and full vectorization is usually worth it. Even if
    `count < N`, it usually makes sense to pad inputs/outputs up to `N`.

*   Use the `Transform*` functions in hwy/contrib/algo/transform-inl.h. This
    takes care of the loop and remainder handling and you simply define a
    generic lambda function (C++14) or functor which receives the current vector
    from the input/output array, plus optionally vectors from up to two extra
    input arrays, and returns the value to write to the input/output array.

    Here is an example implementing the BLAS function SAXPY (`alpha * x + y`):

    ```
    Transform1(d, x, n, y, [](auto d, const auto v, const auto v1) HWY_ATTR {
      return MulAdd(Set(d, alpha), v, v1);
    });
    ```

*   Process whole vectors as above, followed by a scalar loop:

    ```
    size_t i = 0;
    for (; i + N <= count; i += N) LoopBody<false>(d, i, 0);
    for (; i < count; ++i) LoopBody<false>(CappedTag<T, 1>(), i, 0);
    ```
    The template parameter and second function arguments are again not needed.

    This avoids duplicating code, and is reasonable if `count` is large.
    If `count` is small, the second loop may be slower than the next option.

*   Process whole vectors as above, followed by a single call to a modified
    `LoopBody` with masking:

    ```
    size_t i = 0;
    for (; i + N <= count; i += N) {
      LoopBody<false>(d, i, 0);
    }
    if (i < count) {
      LoopBody<true>(d, i, count - i);
    }
    ```
    Now the template parameter and third function argument can be used inside
    `LoopBody` to non-atomically 'blend' the first `num_remaining` lanes of `v`
    with the previous contents of memory at subsequent locations:
    `BlendedStore(v, FirstN(d, num_remaining), d, pointer);`. Similarly,
    `MaskedLoad(FirstN(d, num_remaining), d, pointer)` loads the first
    `num_remaining` elements and returns zero in other lanes.

    This is a good default when it is infeasible to ensure vectors are padded,
    but is only safe `#if !HWY_MEM_OPS_MIGHT_FAULT`!
    In contrast to the scalar loop, only a single final iteration is needed.
    The increased code size from two loop bodies is expected to be worthwhile
    because it avoids the cost of masking in all but the final iteration.

## Additional resources

*   [Highway introduction (slides)](g3doc/highway_intro.pdf)
*   [Overview of instructions per operation on different architectures](g3doc/instruction_matrix.pdf)
*   [Design philosophy and comparison](g3doc/design_philosophy.md)
*   [Implementation details](g3doc/impl_details.md)

## Acknowledgments

We have used [farm-sve](https://gitlab.inria.fr/bramas/farm-sve) by Berenger
Bramas; it has proved useful for checking the SVE port on an x86 development
machine.

This is not an officially supported Google product.
Contact: janwas@google.com
