# Vectorized and performance-portable Quicksort

## Introduction

As of 2022-06-07 this sorts large arrays of built-in types about ten times as
fast as `std::sort`. See also our
[blog post](https://opensource.googleblog.com/2022/06/Vectorized%20and%20performance%20portable%20Quicksort.html)
and [paper](https://arxiv.org/abs/2205.05982).

## Instructions

Here are instructions for reproducing our results with cross-platform CMake,
Linux, or AWS (SVE, NEON).

### CMake, any platform

Please first ensure that Clang (tested with 13.0.1 and 15.0.6) is installed, and
if it is not the default compiler, point the CC and CXX environment variables to
it, e.g.

```
export CC=clang-15
export CXX=clang++-15
```

Then run the usual CMake workflow, also documented in the Highway README, e.g.:

```
mkdir -p build && cd build && cmake .. && make -j
taskset -c 2 tests/bench_sort
```

The optional `taskset -c 2` part reduces the variability of measurements by
preventing the OS from migrating the benchmark between cores.

### Linux

Please first ensure golang, and Clang (tested with 13.0.1) are installed via
your system's package manager.

```
go install github.com/bazelbuild/bazelisk@latest
git clone https://github.com/google/highway
cd highway
CC=clang CXX=clang++ ~/go/bin/bazelisk build -c opt hwy/contrib/sort:all
bazel-bin/hwy/contrib/sort/sort_test
bazel-bin/hwy/contrib/sort/bench_sort
```

### AWS Graviton3

Instance config: amazon linux 5.10 arm64, c7g.8xlarge (largest allowed config is
32 vCPU). Initial launch will fail. Wait a few minutes for an email saying the
config is verified, then re-launch. See IPv4 hostname in list of instances.

`ssh -i /path/key.pem ec2-user@hostname`

Note that the AWS CMake package is too old for llvm, so we build it first:
```
wget https://cmake.org/files/v3.23/cmake-3.23.2.tar.gz
tar -xvzf cmake-3.23.2.tar.gz && cd cmake-3.23.2/
./bootstrap -- -DCMAKE_USE_OPENSSL=OFF
make -j8 && sudo make install
cd ..
```

AWS clang is at version 11.1, which generates unnecessary `AND` instructions
which slow down the sort by 1.15x. We tested with clang trunk as of June 13
(which reports Git hash 8f6512fea000c3a0d394864bb94e524bee375069). To build:

```
git clone --depth 1 https://github.com/llvm/llvm-project.git
cd llvm-project
mkdir -p build && cd build
/usr/local/bin/cmake ../llvm -DLLVM_ENABLE_PROJECTS="clang" -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" -DCMAKE_BUILD_TYPE=Release
make -j32 && sudo make install
```

```
sudo yum install go
go install github.com/bazelbuild/bazelisk@latest
git clone https://github.com/google/highway
cd highway
CC=/usr/local/bin/clang CXX=/usr/local/bin/clang++ ~/go/bin/bazelisk build -c opt --copt=-march=armv8.2-a+sve hwy/contrib/sort:all
bazel-bin/hwy/contrib/sort/sort_test
bazel-bin/hwy/contrib/sort/bench_sort
```

The above command line enables SVE, which is currently only available on
Graviton 3. You can also test NEON on the same processor, or other Arm CPUs, by
changing the `-march=` option to `--copt=-march=armv8.2-a+crypto`. Note that
such flags will be unnecessary once Clang supports `#pragma target` for NEON and
SVE intrinsics, as it does for x86.

## Results

`bench_sort` outputs the instruction set (AVX3 refers to AVX-512), the sort
algorithm (std for `std::sort`, vq for our vqsort), the type of keys being
sorted (f32 is float), the distribution of keys (uniform32 for uniform random
with range 0-2^32), the number of keys, then the throughput of sorted keys (i.e.
number of key bytes output per second).

Example excerpt from Xeon 6154 (Skylake-X) CPU clocked at 3 GHz:

```
[ RUN      ] BenchSortGroup/BenchSort.BenchAllSort/AVX3
      AVX3:          std:     f32: uniform32: 1.00E+06   54 MB/s ( 1 threads)
      AVX3:           vq:     f32: uniform32: 1.00E+06 1143 MB/s ( 1 threads)
```
