Compling and Installing the VPack library
=========================================

Prerequisites
-------------

The VPack library can be built on Linux, MacOS and Windows. Other platforms may
work too, but are untested.

The VPack library is implemented in C++. To build it, a C++11-enabled compiler
is required. Recent versions of g++, clang++ and Visual Studio are known to work.

VPack uses [CMake](https://cmake.org/download/) for building. Therefore a recent
version of CMake is required too.


Building the VPack library
--------------------------

*Note: the following build instructions are for Linux and MacOS.*

Building the VPack library is straightforward with `cmake`. Simply execute the
following commands to create an out-of-source build:

```bash
mkdir -p build
(cd build && cmake .. && make)
```

This will build a static library `libvelocypack.a` in the `build` directory
in *Release* mode.

By default a few example programs and tools will also be built. These can
be found in the `build` directory when the build is complete.

To install the library and tools, run the following command:

```bash
(cd build && cmake .. && sudo make install)
```

This will install everything in a platform-dependent location. On Linux,
the default base directory will be `/usr/local`, so the library will be
installed in `/usr/local/lib` and the binaries in `/usr/local/bin`. 
To adjust the target install directory, change the `DESTDIR` variable to
the desired target directory when invoking `make install`, e.g.:

```bash
(cd build && cmake .. && sudo make DESTDIR=/usr install)
```

To create the Visual Studio project files for *Visual Studio for Windows*, 
execute the appropriate command in the `build` subdirectory:

* 32 bit: `cmake -G "Visual Studio 14 2015" -DCMAKE_BUILD_TYPE=Release ..`
* 64 bit: `cmake -G "Visual Studio 14 2015 Win64" -DCMAKE_BUILD_TYPE=Release ..`

After that, open the generated file `velocypack.sln` from the `build`
subdirectory with Visual Studio.


Build Options
-------------

The following options can be set when building VPack:

* `-DCMAKE_BUILD_TYPE=Release`: builds the VPack library in release mode. This
  does not build debug symbols and turns on all optimizations. Use this mode for
  production.
* `-DCMAKE_BUILD_TYPE=Debug`: builds the VPack library in debug mode. This
  adds debug symbols and turns off optimizations. Use this mode for development,
  but not for production or performance testing.
* `-DHashType`: sets the hash function internally used by VelocyPack. Valid values
  for this option are `xxhash` and `fasthash`.
* `-DBuildTools`: controls whether some tool binaries for VPack values should be
  built. These tools can be used to inspect VPack values contained in files or to
  convert JSON files to VPack. They are not needed when VPack is used as a library  
  only.
* `-DBuildBench`: controls whether the benchmark suite should be built. The
  default is `OFF`, meaning the suite will not be built. Set the option to `ON` to
  build it. Building the benchmark suite requires the subdirectory *rapidjson* to
  be present (see below) and the `BuildTools` option to be set to `ON`.
* `-DBuildVelocyPackExamples`: controls whether VPack's examples should be built. The
  examples are not needed when VPack is used as a library only.
* `-DBuildTests`: controls whether VPack's own test suite should be built. The
  default is `OFF`. Set the option to `ON` for building the tests. This requires
  the subdirectory *googletest* to be present (see below).
* `-DEnableSSE`: controls whether SSE4.2 optimizations are compiled into the
  library. The default is either `ON` or `OFF`, depending on the detected SSE4.2
  support of the host platform. Note that this option should be turned off when
  running VPack under Valgrind, as Valgrind does not seem to support all SSE4
  operations used in VPack.
* `-DCoverage`: needs to be set to `ON` for coverage tests. Setting this option
  will automatically turn the build into a debug build. The option is currently
  supported for g++ only.


Running the tests
-----------------

Building VPack's own test suite requires the [googletest framework](https://github.com/google/googletest)
to be built. To build the tests, run cmake with the option `-DBuildTests=ON`:

```bash
mkdir -p build
(cd build && cmake -DBuildTests=ON .. && make)
```

Afterwards you can run all tests via:

```bash
(cd build/tests && ctest -V)
```

Running coverage tests
----------------------

Running the test suite with coverage collection requires g++ and *lcov* to be
installed. To run the coverage tests, a debug build needs to be created as
follows:

```bash
(cd build && cmake -DCoverage=ON -DBuildBench=OFF -DBuildVelocyPackExamples=OFF -DBuildTests=ON -DBuildLargeTests=OFF -DBuildTools=OFF .. && make)
(cd build && lcov --zerocounters --directory . && lcov --capture --initial --directory . --output-file app)
(cd build/tests && ctest)
(cd build && lcov --no-checksum --directory . --capture --output-file app.info && genhtml app.info)
```

This will create coverage info and HTML coverage reports in the `build` subdirectory.

