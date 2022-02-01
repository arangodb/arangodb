# Efficient and performance-portable SIMD

Highway is a C++ library for SIMD (Single Instruction, Multiple Data), i.e.
applying the same operation to multiple 'lanes' using a single CPU instruction.

## Why Highway?

- more portable (same source code) than platform-specific intrinsics,
- works on a wider range of compilers than compiler-specific vector extensions,
- more dependable than autovectorization,
- easier to write/maintain than assembly language,
- supports **runtime dispatch**,
- supports **variable-length vector** architectures.

## Current status

Supported targets: scalar, S-SSE3, SSE4, AVX2, AVX-512, AVX3_DL (~Icelake,
requires opt-in by defining `HWY_WANT_AVX3_DL`), NEON (ARMv7 and v8), SVE,
WASM SIMD.

SVE is tested using farm_sve (see acknowledgments). SVE2 is implemented but not
yet validated. A subset of RVV is implemented and tested with GCC and QEMU.
Work is underway to compile using LLVM, which has different intrinsics with AVL.

Version 0.11 is considered stable enough to use in other projects, and is
expected to remain backwards compatible unless serious issues are discovered
while finishing the RVV target. After that, Highway will reach version 1.0.

Continuous integration tests build with a recent version of Clang (running on
x86 and QEMU for ARM) and MSVC from VS2015 (running on x86).

Before releases, we also test on x86 with Clang and GCC, and ARMv7/8 via
GCC cross-compile and QEMU. See the
[testing process](g3doc/release_testing_process.md) for details.

The `contrib` directory contains SIMD-related utilities: an image class with
aligned rows, and a math library (16 functions already implemented, mostly
trigonometry).

## Installation

This project uses cmake to generate and build. In a Debian-based system you can
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

To build and test the library the standard cmake workflow can be used:

```bash
mkdir -p build && cd build
cmake ..
make -j && make test
```

Or you can run `run_tests.sh` (`run_tests.bat` on Windows).

Bazel is also supported for building, but it is not as widely used/tested.

## Quick start

You can use the `benchmark` inside examples/ as a starting point.

A [quick-reference page](g3doc/quick_reference.md) briefly lists all operations
and their parameters, and the [instruction_matrix](g3doc/instruction_matrix.pdf)
indicates the number of instructions per operation.

We recommend using full SIMD vectors whenever possible for maximum performance
portability. To obtain them, pass a `HWY_FULL(float)` tag to functions such as
`Zero/Set/Load`. There is also the option of a vector of up to `N` (a power of
two <= 16/sizeof(T)) lanes of type `T`: `HWY_CAPPED(T, N)`. If `HWY_TARGET ==
HWY_SCALAR`, the vector always has one lane. For all other targets, up to
128-bit vectors are guaranteed to be available.

Functions using Highway must be inside `namespace HWY_NAMESPACE {`
(possibly nested in one or more other namespaces defined by the project), and
additionally either prefixed with `HWY_ATTR`, or residing between
`HWY_BEFORE_NAMESPACE()` and `HWY_AFTER_NAMESPACE()`.

*   For static dispatch, `HWY_TARGET` will be the best available target among
    `HWY_BASELINE_TARGETS`, i.e. those allowed for use by the compiler (see
    [quick-reference](g3doc/quick_reference.md)). Functions inside `HWY_NAMESPACE`
    can be called using `HWY_STATIC_DISPATCH(func)(args)` within the same module
    they are defined in. You can call the function from other modules by
    wrapping it in a regular function and declaring the regular function in a
    header.

*   For dynamic dispatch, a table of function pointers is generated via the
    `HWY_EXPORT` macro that is used by `HWY_DYNAMIC_DISPATCH(func)(args)` to
    call the best function pointer for the current CPU's supported targets. A
    module is automatically compiled for each target in `HWY_TARGETS` (see
    [quick-reference](g3doc/quick_reference.md)) if `HWY_TARGET_INCLUDE` is
    defined and foreach_target.h is included.

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

To vectorize a loop, "strip-mining" transforms it into an outer loop and inner
loop with number of iterations matching the preferred vector width.

In this section, let `T` denote the element type, `d = HWY_FULL(T)`, `count` the
number of elements to process, and `N = Lanes(d)` the number of lanes in a full
vector. Assume the loop body is given as a function `template<bool partial,
class D> void LoopBody(D d, size_t max_n)`.

Highway offers several ways to express loops where `N` need not divide `count`:

*   Ensure all inputs/outputs are padded. Then the loop is simply

    ```
    for (size_t i = 0; i < count; i += N) LoopBody<false>(d, 0);
    ```
    Here, the template parameter and second function argument are not needed.

    This is the preferred option, unless `N` is in the thousands and vector
    operations are pipelined with long latencies. This was the case for
    supercomputers in the 90s, but nowadays ALUs are cheap and we see most
    implementations split vectors into 1, 2 or 4 parts, so there is little cost
    to processing entire vectors even if we do not need all their lanes. Indeed
    this avoids the (potentially large) cost of predication or partial
    loads/stores on older targets, and does not duplicate code.

*   Process whole vectors as above, followed by a scalar loop:

    ```
    size_t i = 0;
    for (; i + N <= count; i += N) LoopBody<false>(d, 0);
    for (; i < count; ++i) LoopBody<false>(HWY_CAPPED(T, 1)(), 0);
    ```
    The template parameter and second function arguments are again not needed.

    This avoids duplicating code, and is reasonable if `count` is large.
    If `count` is small, the second loop may be slower than the next option.

*   Process whole vectors as above, followed by a single call to a modified
    `LoopBody` with masking:

    ```
    size_t i = 0;
    for (; i + N <= count; i += N) {
      LoopBody<false>(d, 0);
    }
    if (i < count) {
      LoopBody<true>(d, count - i);
    }
    ```
    Now the template parameter and second function argument can be used inside
    `LoopBody` to 'blend' the new partial vector with previous memory contents:
    `Store(IfThenElse(FirstN(d, N), partial, prev_full), d, aligned_pointer);`.

    This is a good default when it is infeasible to ensure vectors are padded.
    In contrast to the scalar loop, only a single final iteration is needed.

## Additional resources

*   [Highway introduction (slides)](g3doc/highway_intro.pdf)
*   [Overview of instructions per operation on different architectures](g3doc/instruction_matrix.pdf)
*   [Design philosophy and comparison](g3doc/design_philosophy.md)

## Acknowledgments

We have used [farm-sve](https://gitlab.inria.fr/bramas/farm-sve) by Berenger
Bramas; it has proved useful for checking the SVE port on an x86 development
machine.

This is not an officially supported Google product.
Contact: janwas@google.com
