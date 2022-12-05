3rd Party components and what to do on update
=============================================

---

**Do not forget to update `../LICENSES-OTHER-COMPONENTS.md`!**

---
## abseil-cpp

13708db87b1ab69f4f2b3214f3f51e986546f282
abseil-cpp is pulled in as a submodule - the exact commit can be found there.

The submodule repository is located at https://github.com/arangodb/abseil-cpp

We have some changes for usage in arangodb and iresearch, so we maintain our own branch with
these changes called "master".
To update to a new version pull from upstream (https://github.com/google/abseil-cpp)
and rebase onto the new version the our "master" branch.

## boost

https://www.boost.org/

(we don't ship the upstream documentation!)
To remove some unused doc files, you can run something as follows:

    cd 3rdParty/boost/1.71.0
    for i in `find -type d -name "doc"`; do git rm -r "$i"; done


## cmake

Custom boost locator

## date

Forward port of C++20 date/time class

## fakeit

Mocking library. https://github.com/eranpeer/FakeIt

## fuerte

Our C++ Client driver capable of velocystream.

## iresearch

This contains the iresearch library and its sub-components, ICU is used from V8.

## iresearch.build

This contains statically generated files for the IResearch folder, and replaces them.

When you update Snowball, you should update `modules.h` in this directory.
Simply run the Perl script in the `snowball` source directory:

```bash
libstemmer/mkmodules.pl temp_modules.h . libstemmer/modules.txt libstemmer/mkinc.mak
```

Then copy `temp_modules.h` to `modules.h`, and fix the paths.

## jemalloc

Only used on Linux/Mac, still uses autofoo.

## libunwind

Only used on Linux, still uses autofoo. The "aux" directory has been removed from the
libtool source because there must not be directories named "aux" on Windows.

In order to update to new versions, run `autogen.sh` and then remove the unnecessary
generated files, e.g.:
```
rm -rf v1.5/aux v1.5/autom4te.cache/ v1.5/config.log v1.5/config.status v1.5/Makefile v1.5/libtool
```

Note: some files normally excluded by .gitignore need to be added to the build:
```
git add -f v1.5/config v1.5/configure v1.5/Makefile.in
```

## linenoise-ng

Our maintained fork of linenoise
https://github.com/arangodb/linenoise-ng

We may want to switch to replxx (uniting our & other forks):
https://github.com/AmokHuginnsson/replxx

## lz4

https://github.com/lz4/lz4

## rocksdb

v7.2.0
RocksDB is pulled in as a submodule - the exact commit can be found there.

The submodule repository is located at https://github.com/arangodb/rocksdb

We have some changes for usage in arangodb, so we maintain our own branch with
these changes called "arango-dev".
To update to a new version pull from upstream (https://github.com/facebook/rocksdb)
and merge the new version into the "arango-dev" branch.

## s2geometry

Just update code and directory name (commit hash) and apply patch

```
diff --git a/3rdParty/s2geometry/master/CMakeLists.txt b/3rdParty/s2geometry/master/CMakeLists.txt
index 2cbbf68c7d7..27f57896054 100644
--- a/3rdParty/s2geometry/master/CMakeLists.txt
+++ b/3rdParty/s2geometry/master/CMakeLists.txt
@@ -68,7 +68,6 @@ if (WITH_GFLAGS)
     add_definitions(-DS2_USE_GFLAGS)
 endif()
 
-find_package(absl REQUIRED)
 find_package(OpenSSL REQUIRED)
 # pthreads isn't used directly, but this is still required for std::thread.
 find_package(Threads REQUIRED)
diff --git a/3rdParty/s2geometry/master/src/s2/base/logging.h b/3rdParty/s2geometry/master/src/s2/base/logging.h
index b5a26715441..5136b297c43 100644
--- a/3rdParty/s2geometry/master/src/s2/base/logging.h
+++ b/3rdParty/s2geometry/master/src/s2/base/logging.h
@@ -62,7 +62,7 @@ class S2LogMessage {
     : severity_(severity), stream_(stream) {
     if (enabled()) {
       stream_ << file << ":" << line << " "
-              << absl::LogSeverityName(severity) << " ";
+              << absl::LogSeverityName(severity_) << " ";
     }
   }
   ~S2LogMessage() { if (enabled()) stream_ << std::endl; }
diff --git a/3rdParty/s2geometry/master/src/s2/s2loop_measures.h b/3rdParty/s2geometry/master/src/s2/s2loop_measures.h
index cdc8e87cd8d..fb1df468a10 100644
--- a/3rdParty/s2geometry/master/src/s2/s2loop_measures.h
+++ b/3rdParty/s2geometry/master/src/s2/s2loop_measures.h
@@ -226,7 +226,7 @@ T GetSurfaceIntegral(S2PointLoopSpan loop,
   if (loop.size() < 3) return sum;
 
   S2Point origin = loop[0];
-  for (int i = 1; i + 1 < loop.size(); ++i) {
+  for (int i = 1; i + 1 < static_cast<int>(loop.size()); ++i) {
     // Let V_i be loop[i], let O be the current origin, and let length(A, B)
     // be the length of edge (A, B).  At the start of each loop iteration, the
     // "leading edge" of the triangle fan is (O, V_i), and we want to extend
diff --git a/3rdParty/s2geometry/master/src/s2/s2polygon.h b/3rdParty/s2geometry/master/src/s2/s2polygon.h
index 9981ae0ff89..e77f0c96714 100644
--- a/3rdParty/s2geometry/master/src/s2/s2polygon.h
+++ b/3rdParty/s2geometry/master/src/s2/s2polygon.h
@@ -1009,10 +1009,10 @@ inline S2Shape::ChainPosition S2Polygon::Shape::chain_position(int e) const {
     }
   } else {
     i = prev_loop_.load(std::memory_order_relaxed);
-    if (e >= start[i] && e < start[i + 1]) {
+    if (e >= static_cast<int>(start[i]) && e < static_cast<int>(start[i + 1])) {
       // This edge belongs to the same loop as the previous call.
     } else {
-      if (e == start[i + 1]) {
+      if (e == static_cast<int>(start[i + 1])) {
         // This edge immediately follows the loop from the previous call.
         // Note that S2Polygon does not allow empty loops.
         ++i;
diff --git a/3rdParty/s2geometry/master/src/s2/s2region_coverer.h b/3rdParty/s2geometry/master/src/s2/s2region_coverer.h
index 575f74c3ad4..1969b320cb7 100644
--- a/3rdParty/s2geometry/master/src/s2/s2region_coverer.h
+++ b/3rdParty/s2geometry/master/src/s2/s2region_coverer.h
@@ -269,7 +269,14 @@ class S2RegionCoverer {
     S2Cell cell;
     bool is_terminal;        // Cell should not be expanded further.
     int num_children = 0;    // Number of children that intersect the region.
+#ifdef _MSC_VER
+#pragma warning(push)
+#pragma warning(disable:4200)
+#endif
     Candidate* children[0];  // Actual size may be 0, 4, 16, or 64 elements.
+#ifdef _MSC_VER
+#pragma warning(pop)
+#endif
   };
 
   // If the cell intersects the given region, return a new candidate with no
diff --git a/3rdParty/s2geometry/master/src/s2/util/coding/coder.h b/3rdParty/s2geometry/master/src/s2/util/coding/coder.h
index df633a2c45d..4e230ea4789 100644
--- a/3rdParty/s2geometry/master/src/s2/util/coding/coder.h
+++ b/3rdParty/s2geometry/master/src/s2/util/coding/coder.h
@@ -495,7 +495,7 @@ inline void DecoderExtensions::FillArray(Decoder* array, int num_decoders) {
                 "Decoder must be trivially copy-assignable");
   static_assert(absl::is_trivially_destructible<Decoder>::value,
                 "Decoder must be trivially destructible");
-  std::memset(array, 0, num_decoders * sizeof(Decoder));
+  std::memset(static_cast<void*>(array), 0, num_decoders * sizeof(Decoder));
 }
 
 inline unsigned char Decoder::get8() {
diff --git a/3rdParty/s2geometry/master/src/s2/util/bits/bits.h b/3rdParty/s2geometry/master/src/s2/util/bits/bits.h
index f1d5d26cef3..ae063557aa3 100644
--- a/3rdParty/s2geometry/master/src/s2/util/bits/bits.h
+++ b/3rdParty/s2geometry/master/src/s2/util/bits/bits.h
@@ -39,29 +39,29 @@ namespace Bits {
 
 inline int FindLSBSetNonZero(uint32 n) {
   S2_ASSUME(n != 0);
-  return absl::countr_zero(n);
+  return static_cast<int>(absl::countr_zero(n));
 }
 
 inline int FindLSBSetNonZero64(uint64 n) {
   S2_ASSUME(n != 0);
-  return absl::countr_zero(n);
+  return static_cast<int>(absl::countr_zero(n));
 }
 
 inline int Log2FloorNonZero(uint32 n) {
   S2_ASSUME(n != 0);
-  return absl::bit_width(n) - 1;
+  return static_cast<int>(absl::bit_width(n)) - 1;
 }
 
 inline int Log2FloorNonZero64(uint64 n) {
   S2_ASSUME(n != 0);
-  return absl::bit_width(n) - 1;
+  return static_cast<int>(absl::bit_width(n)) - 1;
 }
 
 inline int FindMSBSetNonZero(uint32 n) { return Log2FloorNonZero(n); }
 inline int FindMSBSetNonZero64(uint64 n) { return Log2FloorNonZero64(n); }
 
 inline int Log2Ceiling(uint32 n) {
-  int floor = absl::bit_width(n) - 1;
+  int floor = static_cast<int>(absl::bit_width(n)) - 1;
   if ((n & (n - 1)) == 0) {  // zero or a power of two
     return floor;
   } else {
diff --git a/3rdParty/s2geometry/master/src/s2/base/port.h b/3rdParty/s2geometry/master/src/s2/base/port.h
index 0efaba84248..328393d2ffd 100644
--- a/3rdParty/s2geometry/master/src/s2/base/port.h
+++ b/3rdParty/s2geometry/master/src/s2/base/port.h
@@ -59,6 +59,15 @@
 #undef ERROR
 #undef DELETE
 #undef DIFFERENCE
+#undef S_IRUSR
+#undef S_IWUSR
+#undef S_IXUSR
+#undef S_IRGRP
+#undef S_IWGRP
+#undef S_IXGRP
+#undef S_IROTH
+#undef S_IWOTH
+#undef S_IXOTH
 #define STDIN_FILENO 0
 #define STDOUT_FILENO 1
 #define STDERR_FILENO 2
diff --git a/3rdParty/s2geometry/master/src/s2/util/gtl/compact_array.h b/3rdParty/s2geometry/master/src/s2/util/gtl/compact_array.h
index cbc4fe9aa46..9de05c61d73 100644
--- a/3rdParty/s2geometry/master/src/s2/util/gtl/compact_array.h
+++ b/3rdParty/s2geometry/master/src/s2/util/gtl/compact_array.h
@@ -413,7 +413,9 @@ class compact_array_base {
     value_allocator_type allocator;
 
     T* new_ptr = allocator.allocate(capacity());
-    memcpy(new_ptr, Array(), old_capacity * sizeof(T));
+    if (old_capacity != 0) {
+      memcpy(new_ptr, Array(), old_capacity * sizeof(T));
+    }
     allocator.deallocate(Array(), old_capacity);
 
     SetArray(new_ptr);
```

## snappy

Compression library
https://github.com/google/snappy

We need the following modification to snappy's cmake config to ensure that on
Windows the compiler does not use instructions which are not supported by our
target architecture:
```
diff --git a/3rdParty/snappy/snappy-1.1.9/CMakeLists.txt b/3rdParty/snappy/snappy-1.1.9/CMakeLists.txt
index 672561e62fc..d6341fd1d7a 100644
--- a/3rdParty/snappy/snappy-1.1.9/CMakeLists.txt
+++ b/3rdParty/snappy/snappy-1.1.9/CMakeLists.txt
@@ -160,6 +160,7 @@ int main() {
   return zero();
 }" HAVE_ATTRIBUTE_ALWAYS_INLINE)

+if (NOT WINDOWS)
 check_cxx_source_compiles("
 #include <tmmintrin.h>

@@ -177,6 +178,7 @@ check_cxx_source_compiles("
 int main() {
   return _bzhi_u32(0, 1);
 }" SNAPPY_HAVE_BMI2)
+endif()

 include(CheckSymbolExists)
 check_symbol_exists("mmap" "sys/mman.h" HAVE_FUNC_MMAP)
 ```

and the following patch to enable building with RTTI, because mixing RTTI and non-RTTI code
leads to unpredictable problems.

``` 
diff --git a/3rdParty/snappy/snappy-1.1.9/CMakeLists.txt b/3rdParty/snappy/snappy-1.1.9/CMakeLists.txt
index 55c7bc88a10..5c3cf68f879 100644
--- a/3rdParty/snappy/snappy-1.1.9/CMakeLists.txt
+++ b/3rdParty/snappy/snappy-1.1.9/CMakeLists.txt
@@ -53,8 +53,8 @@ if(MSVC)
   add_definitions(-D_HAS_EXCEPTIONS=0)
 
   # Disable RTTI.
-  string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
-  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
+  #string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
+  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
 else(MSVC)
   # Use -Wall for clang and gcc.
   if(NOT CMAKE_CXX_FLAGS MATCHES "-Wall")
@@ -78,8 +78,8 @@ else(MSVC)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
 
   # Disable RTTI.
-  string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
-  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
+  #string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
+  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
 endif(MSVC)
 
 # BUILD_SHARED_LIBS is a standard CMake variable, but we declare it here to make
```

## snowball

Don't forget to update `iresearch.build/modules.h`, see [iresearch.build](#iresearchbuild)!

http://snowball.tartarus.org/ stemming for IResearch. We use the latest provided cmake which we maintain.

## swagger-ui

https://github.com/swagger-api/swagger-ui/releases

Our copy of swagger-ui resides at `js/assets/swagger`. The `index.html`
contains a few tweaks to make swagger-ui work with the web interface.

To upgrade to a newer version:

1. Copy the file `js/assets/swagger/index.html` to a safe location and open it in an editor
2. Delete all existing files inside `js/assets/swagger` including `index.html`
3. Download the release bundle of swagger-ui you want to upgrade to
4. Copy all files from the bundle's `dist` folder into `js/assets/swagger`
5. Open the new `js/assets/swagger/index.html` in an editor
6. Add an HTML comment to the start of the file indicating the release version number,
   e.g.  `<!-- swagger-ui 1.2.3 -->`
7. Apply all changes from the old copy to the new file,
   these are indicated by code comments in the following format:
   `#region ArangoDB-specific changes` and `#endregion`
8. Verify the changes were applied correctly and discard the old copy of `index.html`

To verify the changes were applied correctly, start the ArangoDB server and
open the _Rest API_ documentation (_Support_ tab) in the ArangoDB web interface.
Routes can be executed by clicking on them to expand their documentation,
clicking the _Try it out_ button, filling out any required fields and clicking
the _Execute_ button.

* The Swagger top bar containing an URL field should NOT appear.

  This confirms that the change hiding the top bar was applied correctly.

* The API documentation should appear, NOT an error message.

  This confirms that the change inferring the URL was applied correctly.

* The sections should be collapsed, NOT showing the individual routes.

  This confirms the `docExpansion` changes work correctly for the
  server API documentation.

* When using the `POST /_api/cursor` route with a query the authenticated
  user is authorized to execute, the response should not indicate an
  ArangoDB authentication error.

  This confirms the `requestInterceptor`-related changes were applied correctly.

* All text in the API documentation should use readable color combinations.
  The API documentation should NOT look obviously "broken" or "ugly".

  This indicates the stylistic CSS changes were applied correctly.

* Scroll to the very end of the page and check the bottom right corner.
  There should be NO badge reading _INVALID_.

  This confirms that validation is disabled correctly (`validatorUrl`).

Note that to account for changes introduced by new versions of swagger-ui,
the stylistic CSS changes may need to be adjusted manually even when
applied correctly.

To verify the `docExpansion` changes work correctly in Foxx, navigate to the
_Services_ tab, reveal system services via the menu in the gear icon, open the
service `/_api/foxx` and navigate to the _API_ tab within that service.

* All sections of the API documentation should be expanded to show all
  routes but NOT fully expanded to reveal descriptions and examples.

  This confirms the `docExpansion` changes work correctly for Foxx services.

## taocpp::json

Json Parser library
Contains TaoCpp PEGTL - PEG Parsing library

Upstream is: https://github.com/taocpp/json

- On upgrade do not add unnecessary files (e.g. src, tests, contrib)
  and update the commit hash in `./taocpp-json.version`.
  
## tzdata

IANA time zone database / Olson database
Contains information about the world's time zones

Upstream is: https://www.iana.org/time-zones (Data Only Distribution)

Windows builds require windowsZones.xml from the Unicode CLDR project:
https://github.com/unicode-org/cldr/blob/master/common/supplemental/windowsZones.xml

invoke `Installation/fetch_tz_database.sh` to do this.
Fix CMakeLists.txt with new zone files if neccessary.

## V8

Javascript interpreter.
This is maintained via https://github.com/arangodb-helper/v8

Upstream is: https://chromium.googlesource.com/v8/v8.git

- On upgrade the ICU data file(s) need to be replaced with ICU upstream, 
  since the V8 copy doesn't contain all locales (known as full-icu, ~25 MB icudt*.dat).

## velocypack

our fast and compact format for serialization and storage

Maintained at:
https://github.com/arangodb/velocypack/

## zlib

ZLib compression library https://zlib.net/

Compared to the original zlib 1.2.12, we have made changes to the CMakeLists.txt
as can be found in the patch file

    3rdParty/zlib/zlib-1.2.12.patch
