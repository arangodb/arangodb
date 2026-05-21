# Frequently Asked Questions

[[TOC]]

## Getting started

Q0.0: How do I **get the Highway library**?

A: Highway is available in numerous package managers, e.g. under the name
libhwy-dev. After installing, you can add it to your CMake-based build via
`find_package(HWY 1.0.4)` and `target_link_libraries(your_project PRIVATE hwy)`.

Alternatively, if using Git for version control, you can use Highway as a
'submodule' by adding the following to .gitmodules:

```
[submodule "highway"]
    path = highway
    url = https://github.com/google/highway
```

Then, anyone who runs `git clone --recursive` on your repository will also get
Highway. If not using Git, you can also manually download the
[Highway code](https://github.com/google/highway/releases) and add it to your
source tree.

For building Highway yourself, the two best-supported build systems are CMake
and Bazel. For the former, insert `add_subdirectory(highway)` into your
CMakeLists.txt. For the latter, we provide a BUILD file and your project can
reference it by adding `deps = ["//path/highway:hwy"]`.

If you use another build system, add `hwy/per_target.cc` and `hwy/targets.cc` to
your list of files to compile and link. As of writing, all other files are
headers typically included via highway.h.

If you are interested in a single-header version of Highway, please raise an
issue so we can understand your use-case.

Q0.1: What's the **easiest way to start using Highway**?

A: Copy an existing file such as `hwy/examples/benchmark.cc` or `skeleton.cc` or
another source already using Highway.
This ensures that the 'boilerplate' (namespaces, include order) are correct.

Then, in the function `RunBenchmarks` (for benchmark.cc) or `FloorLog2` (for
skeleton.cc), you can insert your own code. For starters it can be written
entirely in normal C++. This can still be beneficial because your code will be
compiled with the appropriate flags for SIMD, which may allow the compiler to
autovectorize your C++ code especially if it is straightforward integer code
without conditional statements/branches.

Next, you can wrap your code in `#if HWY_TARGET == HWY_SCALAR || HWY_TARGET ==
HWY_EMU128` and into the `#else` branch, put a vectorized version of your code
using the Highway intrinsics (see Documentation section below). If you also
create a test by copying one of the source files in `hwy/tests/`, the Highway
infrastructure will run your test for all supported targets. Because one of the
targets SCALAR or EMU128 are always supported, this will ensure that your vector
code behaves the same as your original code.

## Documentation

Q1.1: How do I **find the Highway op name** corresponding to an existing
intrinsic?

A: Search for the intrinsic in (for example)
[x86_128-inl.h](https://github.com/google/highway/blob/master/hwy/ops/x86_128-inl.h).
The Highway op is typically the name of the function that calls the intrinsic.
See also the
[quick reference](https://github.com/google/highway/blob/master/g3doc/quick_reference.md)
which lists all of the Highway ops.

Q1.2: Are there **examples of porting intrinsics to Highway**?

A: See cl/448957386 and cl/450480902.

Q1.3: Where do I find documentation for each **platform's intrinsics**?

A: See [Intel](https://www.intel.com/content/www/us/en/docs/intrinsics-guide),
[Arm NEON and SVE](https://developer.arm.com/architectures/instruction-sets/intrinsics),
[RISC-V V](https://github.com/riscv/riscv-v-spec/blob/master/v-spec.adoc),
[WebAssembly](https://nemequ.github.io/waspr/intrinsics), PPC
[ISA](https://openpowerfoundation.org/specifications/isa/) and
[intrinsics](https://openpowerfoundation.org/specifications/vectorintrinsicprogrammingreference/).

Q1.4: Where do I find **instruction latency/throughput**?

A: For x86, a combination of [uops.info](https://www.uops.info/table.html) and
https://agner.org/optimize/, plus Intel's above intrinsics guide and
[AMD's sheet (zip file)](https://www.amd.com/system/files/TechDocs/56665.zip).
For Arm, the
[Software_Optimization_Guide](https://developer.arm.com/documentation/pjdoc466751330-9685/latest/)
for Neoverse V1 etc. For RISC-V, the vendor's tables (typically not publicly
available).

Q1.5: Where can I find **inspiration for SIMD-friendly algorithms**? A:

-   [Algorithms for Modern Hardware online book](https://en.algorithmica.org/hpc/)
-   [SIMD for C++ developers](http://const.me/articles/simd/simd.pdf)
-   [Bit twiddling collection](https://graphics.stanford.edu/~seander/bithacks.html)
-   [SIMD-within-a-register](http://aggregate.org/MAGIC/)
-   Hacker's Delight book, which has a huge collection of bitwise identities,
    but is written for hypothetical RISC CPUs, which differ in some ways from
    the SIMD capabilities of current CPUs.

Q1.6: How do I **predict performance**?

A: The best approach by far is end-to-end application benchmarking. Typical
microbenchmarks are subject to numerous pitfalls including unrealistic cache and
branch predictor hit rates (unless the benchmark randomizes its behavior). But
sometimes we would like a quick indication of whether a short piece of code runs
efficiently on a given CPU. Intel's IACA used to serve this purpose but has been
discontinued. We now recommend llvm-mca,
[integrated into Compiler Explorer](https://gcc.godbolt.org/z/n-KcQ-). This
shows the predicted throughput and the pressure on the various functional units,
but does not cover dynamic behavior including frontend and cache. For a bit more
detail, see
[https://en.algorithmica.org/hpc/profiling/mca/](https://en.algorithmica.org/hpc/profiling/mca/).
chriselrod mentioned the recently published [uica](https://uica.uops.info/),
which is reportedly more accurate
([paper](https://arxiv.org/pdf/2107.14210.pdf)).

## Correctness

Q2.1: **Which targets are covered** by my tests?

A: Tests execute for every target supported by the current CPU. The CPU may vary
across runs in a cloud environment, so you may want to specify constraints to
ensure the CPU is as recent as possible.

Q2.2: Why do **floating-point results differ** on some platforms?

A: It is commonly believed that floating-point reproducibility across platforms
is infeasible. That is somewhat pessimistic, but not entirely wrong. Although
IEEE-754 guarantees certain properties, including the rounding of each
operation, commonly used compiler flags can invalidate them. In particular,
clang/GCC -ffp-contract and MSVC /fp:contract can change results of anything
involving multiply followed by add. This is usually helpful (fusing both
operations into a single FMA, with only a single rounding), but depending on the
computation typically changes the end results by around 10^-5. Using Highway's
`MulAdd` op can have the same effect: SSE4, NEON and WASM may not support FMA,
but most other platforms do. A common workaround is to use a tolerance when
comparing expected values. For robustness across both large and small values, we
recommend both a relative and absolute (L1 norm) tolerance. The -ffast-math flag
can have more subtle and dangerous effects. It allows reordering operations
(which can also change results), but also removes guarantees about NaN, thus we
do not recommend using it.

Q2.3: How do I make my code **safe for asan and msan**?

A: The main challenge is dealing with the remainders in arrays not divisible by
the vector length. Using `LoadU`, or even `MaskedLoad` with the mask set to
`FirstN(d, remaining_lanes)`, may trigger page faults or asan errors. We instead
recommend using `hwy/contrib/algo/transform-inl.h`. Rather than having to write
a loop plus remainder handling, you simply define a templated (lambda) function
implementing one loop iteration. The `Generate` or `Transform*` functions then
take care of remainder handling.

## API design

Q3.1: Are the **`d` arguments optimized out**?

A: Yes, `d` is an lvalue of the zero-sized type `Simd<>`, typically obtained via
`ScalableTag<T>`. These only serve to select overloaded functions and do not
occupy any storage at runtime.

Q3.2: Why do **only some functions have a `d` argument**?

A: Ops which receive and return vectors typically do not require a `d` argument
because the type information on vectors (either built-in or wrappers) is
sufficient for C++ overload resolution. The `d` argument is required for:

```
-   Influencing the number of lanes loaded/stored from/to memory. The
    arguments to `Simd<>` include an upper bound `N`, and a shift count
    `kPow2` to divide the actual number of lanes by a power of two.
-   Indicating the desired vector or mask type to return from 'factory'
    functions such as `Set` or `FirstN`, `BitCast`, or conversions such as
    `PromoteTo`.
-   Disambiguating the argument type to ops such as `VecFromMask` or
    `AllTrue`, because masks may be generic types shared between multiple
    lane types.
-   Determining the actual number of lanes for certain ops, in particular
    those defined in terms of the upper half of a vector (`UpperHalf`, but
    also `Combine` or `ConcatUpperLower`) and reductions such as
    `MaxOfLanes`.
```

Q3.3: What's the policy for **adding new ops**?

A: Please reach out, we are happy to discuss via Github issue. The general
guideline is that there should be concrete plans to use the op, and it should be
efficiently implementable on all platforms without major performance cliffs. In
particular, each implementation should be at least as efficient as what is
achievable on any platform using portable code without the op. See also the
[wishlist for ops](op_wishlist.md).

Q3.4: `auto` is discouraged, **what vector type** should we use?

A: You can use `Vec<D>` or `Mask<D>`, where `D` is the type of `d` (in fact we
often use `decltype(d)` for that). To keep code short, you can define
typedefs/aliases, for example `using V = Vec<decltype(d)>`. Note that the
Highway implementation uses `VFromD<D>`, which is equivalent but currently
necessary because `Vec` is defined after the Highway implementations in
hwy/ops/*.

Q3.5: **Why is base.h separate** from highway.h?

A: It can be useful for files that just want compiler-dependent macros, for
example `HWY_RESTRICT` in public headers. This avoids the expense of including
the full `highway.h`, which can be large because some platform headers declare
thousands of intrinsics.

## Portability

Q4.1: How do I **only generate code for a single instruction set** (static
dispatch)?

A: Suppose we know that all target CPUs support a given baseline (for example
SSE4). Then we can reduce binary size and compilation time by only generating
code for its instruction set. This is actually the default for Highway code that
does not use foreach_target.h. Highway detects via predefined macros which
instruction sets the compiler is allowed to use, which is governed by compiler
flags. This [example](https://gcc.godbolt.org/z/rGnjMevKG) documents which flags
are required on x86.

Q4.2: Why does my working x86 code **not compile on SVE or RISC-V**?

A: Assuming the code uses only documented identifiers (not, for example, the
AVX2-specific `Vec256`), the problem is likely due to compiler limitations
related to sizeless vectors. Code that works on x86 or NEON but not other
platforms is likely breaking one of the following rules:

-   Use functions (Eq, Lt) instead of overloaded operators (`==`, `<`);
-   Prefix Highway ops with `hwy::HWY_NAMESPACE`, or an alias (`hn::Load`) or
    ensure your code resides inside `namespace hwy::HWY_NAMESPACE`;
-   Avoid arrays of vectors and static/thread_local/member vectors; instead use
    arrays of the lane type (T).
-   Avoid pointer arithmetic on vectors; instead increment pointers to lanes by
    the vector length (`Lanes(d)`).

Q4.3: Why are **class members not allowed**?

A: This is a limitation of clang and GCC, which disallow sizeless types
(including SVE and RISC-V vectors) as members. This is because it is not known
at compile time how large the vectors are. MSVC does not yet support SVE nor
RISC-V V, so the issue has not yet come up there.

Q4.4: Why are **overloaded operators not allowed**?

A: C++ disallows overloading functions for built-in types, and vectors on some
platforms (SVE, RISC-V) are indeed built-in types precisely due to the above
limitation. Discussions are ongoing whether the compiler could add builtin
`operator<(unspecified_vector, unspecified_vector)`. When(if) that becomes
widely supported, this limitation can be lifted.

Q4.5: Can I declare **arrays of lanes on the stack**?

A: This mostly works, but is not necessarily safe nor portable. On RISC-V,
vectors can be quite large (64 KiB for LMUL=8), which can exceed the stack size.
It is better to use `hwy::AllocateAligned<T>(Lanes(d))`.

## Boilerplate

Q5.1: What is **boilerplate**?

A: We use this to refer to reusable infrastructure which mostly serves to
support runtime dispatch. We strongly recommend starting a SIMD project by
copying from an existing one, because the ordering of code matters and the
vector-specific boilerplate may be unfamiliar. For static dispatch, see
cl/408632990. For dynamic dispatch, see hwy/examples/skeleton.cc or
cl/376150733.

Q5.2: What's the difference between **`HWY_BEFORE_NAMESPACE` and `HWY_ATTR`**?

A: Both are ways of enabling SIMD code generation in clang/gcc. The former is a
pragma that applies to all subsequent namespace-scope and member functions, but
not lambda functions. It can be more convenient than specifying `HWY_ATTR` for
every function. However, `HWY_ATTR` is still necessary for lambda functions that
use SIMD.

Q5.3: **Why use `HWY_NAMESPACE`**?

A: This is only required when using foreach_target.h to generate code for
multiple targets and dispatch to the best one at runtime. The namespace name
changes for each target to avoid ODR violations. This would not be necessary for
binaries built for a single target instruction set. However, we recommend
placing your code in a `HWY_NAMESPACE` namespace (nested under your project's
namespace) regardless so that it will be ready for runtime dispatch if you want
that later.

Q5.4: What are these **unusual include guards**?

A: Suppose you want to share vector code between several translation units, and
ensure it is inlined. With normal code we would use a header. However,
foreach_target.h wants to re-compile (via repeated preprocessor `#include`) a
translation unit once per target. A conventional include guard would strip out
the header contents after the first target. By convention, we use header files
named *-inl.h with a special include guard of the form:

```
#if defined(MYPROJECT_FILE_INL_H_TARGET) == defined(HWY_TARGET_TOGGLE)
#ifdef MYPROJECT_FILE_INL_H_TARGET
#undef MYPROJECT_FILE_INL_H_TARGET
#else
#define MYPROJECT_FILE_INL_H_TARGET
#endif
```

Highway takes care of defining and un-defining `HWY_TARGET_TOGGLE` after each
recompilation such that the guarded header is included exactly once per target.
Again, this effort is only necessary when using foreach_target.h. However, we
recommend using the special include guards already so your code is ready for
runtime dispatch.

Q5.5: How do I **prevent lint warnings for the include guard**?

A: The linter wishes to see a normal include guard at the start of the file. We
can simply insert an empty guard, followed by our per-target guard.

```
// Start of file: empty include guard to avoid lint errors
#ifndef MYPROJECT_FILE_INL_H_
#define MYPROJECT_FILE_INL_H_
#endif
// Followed by the actual per-target guard as above
```

## Efficiency

Q6.1: I heard that modern CPUs support unaligned loads efficiently. Why does
Highway **differentiate unaligned and aligned loads/stores**?

A: It is true that Intel CPUs since Haswell have greatly reduced the penalty for
unaligned loads. Indeed the `LDDQU` instruction intended to reduce their
performance penalty is no longer necessary because normal loads (`MOVDQU`) now
behave in the same way, splitting unaligned loads into two aligned loads.
However, this comes at a cost: using two (both) load ports per cycle. This can
slow down low-arithmetic-intensity algorithms such as dot products that mainly
load without performing much arithmetic. Also, unaligned stores are typically
more expensive on any platform. Thus we recommend using aligned stores where
possible, and testing your code on x86 (which may raise faults if your pointers
are actually unaligned). Note that the more specialized memory operations apart
from Load/Store (e.g. `CompressStore` or `BlendedStore`) are not specialized for
aligned pointers; this is to avoid doubling the number of memory ops.

Q6.2: **When does `Prefetch` help**?

A: Prefetching reduces apparent memory latency by starting the process of
loading from cache or DRAM before the data is actually required. In some cases,
this can be a 10-20% improvement if the application is indeed latency sensitive.
However, the CPU may already be triggering prefetches by analyzing your access
patterns. Depending on the platform, one or two separate instances of continuous
forward or backward scans are usually automatically detected. If so, then
additional prefetches may actually degrade performance. Also, applications will
not see much benefit if they are bottlenecked by something else such as vector
execution resources. Finally, a prefetch only helps if it comes sufficiently
before the subsequent load, but not so far ahead that it again falls out of the
cache. Thus prefetches are typically applied to future loop iterations.
Unfortunately, the prefetch distance (gap between current position and where we
want to prefetch) is highly platform- and microarchitecture dependent, so it can
be difficult to choose a value appropriate for all platforms.

Q6.3: Is **CPU clock throttling** really an issue?

A: Early Intel implementations of AVX2 and especially AVX-512 reduced their
clock frequency once certain instructions are executed. A
[microbenchmark](https://lemire.me/blog/2018/08/15/the-dangers-of-avx-512-throttling-a-3-impact/)
specifically designed to reveal the worst case (with only few AVX-512
instructions) shows a 3-4% slowdown on Skylake. Note that this is for a single
core; the effect depends on the number of cores using SIMD, and the CPU type
(Bronze/Silver are more heavily affected than Gold/Platinum). However, the
throttling is defined relative to an arbitrary base frequency; what actually
matters is the measured performance. Because throttling or SIMD usage can affect
the entire system, it is important to measure end-to-end application performance
rather than rely on microbenchmarks. In practice, we find the speedup from
sustained SIMD usage (not just sporadic instructions amid mostly scalar code) is
much larger than the impact of throttling. For JPEG XL image decompression and
vectorized Quicksort, we observe a 1.4-1.6x end to end speedup from AVX-512 vs
AVX2, even on multiple cores of a Xeon Gold. Note that throttling is
[no longer a concern on recent Intel](https://travisdowns.github.io/blog/2020/08/19/icl-avx512-freq.html#summary)
implementations of AVX-512 (Icelake and Rocket Lake client), and AMD CPUs do not
throttle AVX2 or AVX-512.

Q6.4: Why does my CPU sometimes only execute **one vector instruction per
cycle** even though the specs say it could do 2-4?

A: CPUs and fast food restaurants assume there will be a mix of
instructions/food types. If everyone orders only french fries, that unit will be
the bottleneck. Instructions such as permutes/swizzles and comparisons are
assumed to be less common, and thus can typically only execute one per cycle.
Check the platform's optimization guide for the per-instruction "throughput".
For example, Intel Skylake executes swizzles on port 5, and thus only one per
cycle. Similarly, Arm V1 can only execute one predicate-setting instruction
(including comparisons) per cycle. As a workaround, consider replacing equality
comparisons with the OR-sum of XOR differences.

Q6.5: How **expensive are Gather/Scatter**?

A: Platforms that support it typically process one lane per cycle. This can be
far slower than normal Load/Store (which can typically handle two or even three
entire *vectors* per cycle), so avoid them where possible. However, some
algorithms such as rANS entropy coding and hash tables require gathers, and it
is still usually better to use them than to avoid vectorization entirely.
