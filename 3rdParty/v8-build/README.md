# V8 integration for maintainers

ArangoDB uses the Google V8 JavaScript engine for executing JavaScript code
inside the arangod server process and inside the ArangoShell (arangosh).

arangod and all client tools also rely on the ICU library (International
components for Unicode) that is currently built as part of V8.

The V8 build is mainly driven by the CMakeLists.txt file in this directory.
The CMake file will make a call to gyp, which will create Makefiles for V8
that can later be built with GNU make on Linux and macOS, or with msbuild
on Windows.

ArangoDB is currently tied to a specific version of V8. If the V8 version
does not need an upgrade, there is no need for action and this document
does not need to be read further. Only continue if you plan on upgrading
the V8 version inside ArangoDB.

## The V8 build system

ArangoDB builds using CMake, but V8 doesn't come with a CMakeLists.txt file.
There are two ways for building V8, which both have pros and cons:

### Building with gyp

gyp is the build system used by nodejs to build nodejs and its dependencies
(of which the largest one is V8). ArangoDB has also traditionally used gyp
for building V8 and continues to do so.

Building with gyp has the advantage that the V8 library can be built as
part of a normal ArangoDB build, so we can use a fixed compiler version
and flags to built with. This is also ideal for static executables.

### Building the Google way

Google, as the primary maintainer of the V8 engine, does not recommend using 
gyp to build V8, but recommends a totally different procedure 
(from https://v8.dev/docs/embed).

They basically recommend to use `depot_tools` and `gclient` to fetch the
current dependencies of V8, and then build the V8 static libraries with a
fully custom toolchain (including generate-ninja, ninja and custom compiler
versions that are downloaded for the build).

While this is the official way to build V8, the prerequisites are many,
and the generated libraries may have dependencies on the properties of the
host system (e.g. libc version used when compiling).
This way also requires to have the full toolchain available and working
for all our target platforms (Linux x86/ARM, Windows x86, macOS x86/M1).

### Build variant used in ArangoDB

We currently go with the gyp variant, and use gyp files which were largely
inspired by the gyp files in previous versions of ArangoDB and nodejs.

The integration of V8 and gyp into our build system is not trivial, mainly
because it is has to work for different platforms and there are various
parts to consider.

## Description of subdirectories

The following subdirectories currently exist inside `3rdParty/v8-build`:

* gyp: this is the source of gyp (from github.com:nodejs/gyp-next). We
  use the gyp version from commit `379257d4eb433096fa8a77b5ffc9971b0fd23af1`
  from that repository.
  Note that some adjustments had to be made the Windows and macOS parts
  of these files because otherwise gyp did not get all required build
  parameters through.
* gypfiles: these are gyp "build instructions" that are required to build
  v8 with gyp. They have been taken from a previous version of ArangoDB
  and have been largely inspired by the gyp files in node.js.
* v8: the actual v8 source code, including the required V8 dependencies.
  This is a git submodule.

## Description of V8 dependencies

For building ArangoDB 3.12, we rely on a minimal set of V8 dependencies.
The following subdirectories were already included in V8's git repository
when checking out git tag `12.1.165`:
* colorama
* glibc 
* googletest 
* jsoncpp 
* v8 
* cpu_features 
* google_benchmark 
* inspector_protocol 
* test262-harness 
* wasm-api

The following subdirectories were added afterwards to make V8 compile:
* icu (from https://chromium.googlesource.com/chromium/deps/icu.git)
* jinja2 (https://chromium.googlesource.com/chromium/src/third_party/jinja2.git)
* markupsafe (https://chromium.googlesource.com/chromium/src/third_party/markupsafe.git)

The full source code of `googletest` and `googlemock` also needed to be
added to the existing `googletest` directory in order to make V8 compile.

When we build ArangoDB with V8, we intentionally turn off the zlib support
in V8 so that it has less dependencies (and conflicts with our own version
of zlib).

## Upgrading our version of V8

To upgrade our version of V8, the source code of V8 must be upgraded in
the submodule `3rdParty/v8-build/v8`.
This may not be as trivial as it sounds, because new versions of V8 may 
require different dependencies, or making the new version compile and/or link
may require changes to the gyp build system.

Before the files in the `v8` submodule are updated, it is better to first 
check out v8 in a standalone directory using the way that Google recommends.
That way it can more easily be checked with dependencies are needed and in
what version.

So try the following in a new directory, outside of ArangoDB:

### Get the V8 source code

According to https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up, the
source code of V8 should not be checked out from Github directly, but should
be fetched using `depot_tools`.

`depot_tools` can be fetched by cloning this git repository:
```
 git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```
Afterwards, the `depot_tools` must be added to the path as follows:
```
export PATH=/path/to/depot_tools:$PATH
```

Afterwards the `depot_tools` should be updated by calling `gclient`.

Once the `depot_tools` are ready, the V8 repository can be checked out via
```
mkdir ~/v8
cd ~/v8
fetch v8
cd v8
```
Note that whenever the V8 repository moves forward, both the V8 source code
as well as its dependencies may change.
For ArangoDB 3.12, we have checked out git tag `12.1.165` of the V8 repo.
If you want to upgrade to a newer version, checkout whatever git tag you
are interesting in. You can then run `gclient sync` to fetch the current 
dependencies for that git tag.

Note that this will download all dependencies of V8 into the `third_party`
subdirectory in the `v8` checkout directory, regardless of whether they
are needed for building ArangoDB with V8 or not.
Most dependencies are actually not needed.

### Patching V8

After checking out the standalone version of V8 the Google way, it is required
to patch 2 files to prevent V8 from crashing and to reduce its dependency
surface.

The following patch is needed to turn off some memory protection functionality
that will make V8 crash on some Linux kernels:
```
diff --git a/src/base/build_config.h b/src/base/build_config.h
index 673330236ce..30e752bc843 100644
--- a/src/base/build_config.h
+++ b/src/base/build_config.h
@@ -36,7 +36,7 @@
 #endif

 #if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_X64)
-#define V8_HAS_PKU_JIT_WRITE_PROTECT 1
+#define V8_HAS_PKU_JIT_WRITE_PROTECT 0
 #else
 #define V8_HAS_PKU_JIT_WRITE_PROTECT 0
 #endif
```

The following patch is needed to make V8 not rely on the abseil library, which
allows ArangoDB to use its own version of the abseil library without conflicting
with the abseil version in V8.

At least in V8 version 12.1.165, the abseil library is not really needed when
building V8 and the one place of usage can easily be worked around by using the
standard library containers:
```
diff --git a/src/zone/zone-containers.h b/src/zone/zone-containers.h
index 1238ab2b..1dba0409 100644
--- a/src/zone/zone-containers.h
+++ b/src/zone/zone-containers.h
@@ -17,8 +17,8 @@
 #include <unordered_map>
 #include <unordered_set>

-#include "absl/container/flat_hash_map.h"
-#include "absl/container/flat_hash_set.h"
+//#include "absl/container/flat_hash_map.h"
+//#include "absl/container/flat_hash_set.h"
 #include "src/base/functional.h"
 #include "src/base/intrusive-set.h"
 #include "src/base/small-map.h"
@@ -785,6 +785,14 @@ class SmallZoneMap
             ZoneMapInit<ZoneMap<K, V, Compare>>(zone)) {}
 };

+template <typename K, typename V, typename Hash = base::hash<K>,
+          typename KeyEqual = std::equal_to<K>>
+using ZoneAbslFlatHashMap = ZoneUnorderedMap<K, V, Hash, KeyEqual>;
+
+template <typename K, typename Hash = base::hash<K>,
+          typename KeyEqual = std::equal_to<K>>
+using ZoneAbslFlatHashSet = ZoneUnorderedSet<K, Hash, KeyEqual>;
+#if 0
 // A wrapper subclass for absl::flat_hash_map to make it easy to construct one
 // that uses a zone allocator. If you want to use a user-defined type as key
 // (K), you'll need to define a AbslHashValue function for it (see
@@ -819,6 +827,7 @@ class ZoneAbslFlatHashSet
       : absl::flat_hash_set<K, Hash, KeyEqual, ZoneAllocator<K>>(
             bucket_count, Hash(), KeyEqual(), ZoneAllocator<K>(zone)) {}
 };
+#endif

 // Typedefs to shorten commonly used vectors.
 using IntVector = ZoneVector<int>;
```

If abseil is used more in future versions of V8, we may need to inject the path
to our own abseil version when building V8, because otherwise the linker may
either see duplicate abseil symbols, or worse, contradicting versions of the
abseil library implementations of the same name inside V8 and ArangoDB's own
3rdParty directory.

### Building V8

To build the standalone V8, follow the examples from https://v8.dev/docs/embed#run-the-example.

First run `tools/dev/v8gen.py x64.release.sample`.

Afterwards, run `gn args out.gn/x64.release.sample`. We have last built
with the following switches there:
```
dcheck_always_on = false
is_component_build = false
is_debug = false
target_cpu = "x64"
use_custom_libcxx = false
v8_monolithic = true
v8_use_external_startup_data = false
v8_optimized_debug = true
v8_enable_webassembly = false
```

The command to build the V8 static libraries then is 
`ninja -C out.gn/x64.release.sample v8_monolith`.

The static libraries will be placed somewhere in `out.gn`.

### Upgrading the source code and dependencies

Once the new V8 version builds ok as a standalone library, it is time to update
the V8 library source code in ArangoDB in the submodule `3rdParty/v8-build/v8`.

Note that you may need to add more required dependencies into the `v8/third_party`
directory for the new version. Please be mindful when adding new dependencies,
and only add the ones that are actually needed.

It may be necessary to make adjustments to gyp files in `3rdParty/v8-build/gypfiles`.

Any change to the ICU source code in `3rdParty/v8-build/v8/third_party/icu` may
require changes to the gyp files in the same directory.

### V8-specific build flags

When building V8, we have turned off support for web assembly (WASM).
This was done because we are not using web assembly ourselves and to reduce the 
scope of V8 and its attack surface.

We have also turned off pointer compression in V8. This can save a lot of memory
in the V8 engine, because it would only allow addressing 4GB of memory from it.
This would not be compatible with previous versions of ArangoDB.

When starting up V8 in our source code, we also turn off the RegExp peephole
optimization in V8, because we have seen it running into assertion failures and
segfaulting.
