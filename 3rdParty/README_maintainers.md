3rd Party components and what to do on update
=============================================

---

**Do not forget to update `../LICENSES-OTHER-COMPONENTS.md`!**

---
## abseil

Just update code and directory name (commit hash) and apply patch

```
diff --git a/3rdParty/abseil-cpp/7f850b3167fb38e6b4a9ce1824e6fabd733b5d62/absl/flags/commandlineflag.h b/3rdParty/abseil-cpp/7f850b3167fb38e6b4a9ce1824e6fabd733b5d62/absl/flags/commandlineflag.h
index f2fa08977fd..8e97fdb0ca4 100644
--- a/3rdParty/abseil-cpp/7f850b3167fb38e6b4a9ce1824e6fabd733b5d62/absl/flags/commandlineflag.h
+++ b/3rdParty/abseil-cpp/7f850b3167fb38e6b4a9ce1824e6fabd733b5d62/absl/flags/commandlineflag.h
@@ -153,7 +153,7 @@ class CommandLineFlag {
   bool ParseFrom(absl::string_view value, std::string* error);
 
  protected:
-  ~CommandLineFlag() = default;
+  virtual ~CommandLineFlag() = default;
 
  private:
   friend class flags_internal::PrivateHandleAccessor;
diff --git a/3rdParty/abseil-cpp/7f850b3167fb38e6b4a9ce1824e6fabd733b5d62/absl/strings/numbers.h b/3rdParty/abseil-cpp/7f850b3167fb38e6b4a9ce1824e6fabd733b5d62/absl/strings/numbers.h
index 3ed24669280..ca0f37aa8d1 100644
--- a/3rdParty/abseil-cpp/7f850b3167fb38e6b4a9ce1824e6fabd733b5d62/absl/strings/numbers.h
+++ b/3rdParty/abseil-cpp/7f850b3167fb38e6b4a9ce1824e6fabd733b5d62/absl/strings/numbers.h
@@ -24,7 +24,26 @@
 #define ABSL_STRINGS_NUMBERS_H_
 
 #ifdef __SSE4_2__
+#if defined(_MSC_VER)
+/* Microsoft C/C++-compatible compiler */
+#include <intrin.h>
+#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
+/* GCC-compatible compiler, targeting x86/x86-64 */
 #include <x86intrin.h>
+#elif defined(__GNUC__) && defined(__ARM_NEON__)
+/* GCC-compatible compiler, targeting ARM with NEON */
+#include <arm_neon.h>
+#elif defined(__GNUC__) && defined(__IWMMXT__)
+/* GCC-compatible compiler, targeting ARM with WMMX */
+#include <mmintrin.h>
+#elif (defined(__GNUC__) || defined(__xlC__)) && \
+    (defined(__VEC__) || defined(__ALTIVEC__))
+/* XLC or GCC-compatible compiler, targeting PowerPC with VMX/VSX */
+#include <altivec.h>
+#elif defined(__GNUC__) && defined(__SPE__)
+/* GCC-compatible compiler, targeting PowerPC with SPE */
+#include <spe.h>
+#endif
 #endif
 
 #include <cstddef>
```

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

Our C++ Client driver capable of velocystream. Maintained at https://github.com/arangodb/fuerte

## iresearch

This contains the iresearch library and its sub-components, ICU is used from V8.

## iresearch.build

This contains statically generated files for the IResearch folder, and replaces them.

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

(6.26.0, upstream commit f72fd5856585774063ac3fc8926f70626963d488)

Our branch is maintained at:
https://github.com/arangodb-helper/rocksdb

Most changes can be ported upstream:
https://github.com/facebook/rocksdb

On Upgrade:
- `./thirdparty.inc` needs to be adjusted to use the snappy we specify. This can be
   adjusted by commenting out the section that sets Snappy-related CMake variables:

    -set(SNAPPY_HOME $ENV{THIRDPARTY_HOME}/Snappy.Library)
    -set(SNAPPY_INCLUDE ${SNAPPY_HOME}/build/native/inc/inc)
    -set(SNAPPY_LIB_DEBUG ${SNAPPY_HOME}/lib/native/debug/amd64/snappy.lib)
    -set(SNAPPY_LIB_RELEASE ${SNAPPY_HOME}/lib/native/retail/amd64/snappy.lib)
    +#set(SNAPPY_HOME $ENV{THIRDPARTY_HOME}/Snappy.Library)
    +#set(SNAPPY_INCLUDE ${SNAPPY_HOME}/build/native/inc/inc)
    +#set(SNAPPY_LIB_DEBUG ${SNAPPY_HOME}/lib/native/debug/amd64/snappy.lib)
    +#set(SNAPPY_LIB_RELEASE ${SNAPPY_HOME}/lib/native/retail/amd64/snappy.lib)

The following other change has been made to CMakeLists.txt to allow compiling on ARM:
```
diff --git a/arangod/RocksDBEngine/CMakeLists.txt b/arangod/RocksDBEngine/CMakeLists.txt
index 58205d5ca90..cb3f2c276d9 100644
--- a/arangod/RocksDBEngine/CMakeLists.txt
+++ b/arangod/RocksDBEngine/CMakeLists.txt
@@ -9,11 +9,6 @@ if(CMAKE_SYSTEM_NAME MATCHES "Cygwin")
   add_definitions(-fno-builtin-memcmp -DCYGWIN)
 elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
   add_definitions(-DOS_MACOSX)
-  if(CMAKE_SYSTEM_PROCESSOR MATCHES arm)
-    add_definitions(-DIOS_CROSS_COMPILE -DROCKSDB_LITE)
-    # no debug info for IOS, that will make our library big
-    add_definitions(-DNDEBUG)
-  endif()
 elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
   add_definitions(-DOS_LINUX)
 elseif(CMAKE_SYSTEM_NAME MATCHES "SunOS")
```

We also made the following modification to gtest (included in a subdirectory of
RocksDB):
```
diff --git a/3rdParty/rocksdb/6.8/third-party/gtest-1.8.1/fused-src/gtest/gtest.h b/3rdParty/rocksdb/6.8/third-party/gtest-1.8.1/fused-src/gtest/gtest.h
index ebb16db7b09..10188c93a8c 100644
--- a/3rdParty/rocksdb/6.8/third-party/gtest-1.8.1/fused-src/gtest/gtest.h
+++ b/3rdParty/rocksdb/6.8/third-party/gtest-1.8.1/fused-src/gtest/gtest.h
@@ -19371,7 +19371,7 @@ INSTANTIATE_TYPED_TEST_CASE_P(My, FooTest, MyTypes);
    private:                                                                   \
     typedef CaseName<gtest_TypeParam_> TestFixture;                           \
     typedef gtest_TypeParam_ TypeParam;                                       \
-    virtual void TestBody();                                                  \
+    virtual void TestBody() override;                                         \
   };                                                                          \
   static bool gtest_##CaseName##_##TestName##_registered_                     \
         GTEST_ATTRIBUTE_UNUSED_ =                                             \
```
This change suppresses compile warnings about missing override specifiers, and
hopefully can be removed once RocksDB upgrades their version of gtest.

We also applied a small patch on top of `db/wal_manager.cc` which makes it handle
expected errors (errors that occur when moving WAL files around while they are
tailed) gracefully:
```
diff --git a/3rdParty/rocksdb/6.18/db/wal_manager.cc b/3rdParty/rocksdb/6.18/db/wal_manager.cc
index 7e77e03618..c5bf3ee1b1 100644
--- a/3rdParty/rocksdb/6.18/db/wal_manager.cc
+++ b/3rdParty/rocksdb/6.18/db/wal_manager.cc
@@ -328,6 +339,15 @@ Status WalManager::GetSortedWalsOfType(const std::string& path,
         }
       }
       if (!s.ok()) {
+        if (log_type == kArchivedLogFile &&
+            (s.IsNotFound() ||
+             (s.IsIOError() && env_->FileExists(ArchivedLogFileName(path, number)).IsNotFound()))) {
+          // It may happen that the iteration performed by GetChildren() found
+          // a logfile in the archive, but that this file has been deleted by
+          // another thread in the meantime. In this case just ignore it.
+          s = Status::OK();
+          continue;
+        }
         return s;
       }

```

## s2geometry

Just update code and directory name (commit hash) and apply patch

```
diff --git a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/CMakeLists.txt b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/CMakeLists.txt
index 67d81cd394f..7d67c8a7c17 100644
--- a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/CMakeLists.txt
+++ b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/CMakeLists.txt
@@ -19,7 +19,7 @@ endif()
 # undefined symbol errors since ABSL_HAVE_STD_STRING_VIEW etc will
 # end up defined differently.  There is probably a better way to achieve
 # this than assuming what absl used.
-set(CMAKE_CXX_STANDARD 11)
+set(CMAKE_CXX_STANDARD 17)
 set(CMAKE_CXX_STANDARD_REQUIRED ON)
 # No compiler-specific extensions, i.e. -std=c++11, not -std=gnu++11.
 set(CMAKE_CXX_EXTENSIONS OFF)
@@ -67,7 +67,6 @@ if (WITH_GFLAGS)
     add_definitions(-DS2_USE_GFLAGS)
 endif()
 
-find_package(absl REQUIRED)
 find_package(OpenSSL REQUIRED)
 # pthreads isn't used directly, but this is still required for std::thread.
 find_package(Threads REQUIRED)
@@ -208,11 +207,15 @@ add_library(s2
             src/s2/util/math/exactfloat/exactfloat.cc
             src/s2/util/math/mathutil.cc
             src/s2/util/units/length-units.cc)
-add_library(s2testing STATIC
-            src/s2/s2builderutil_testing.cc
-            src/s2/s2shapeutil_testing.cc
-            src/s2/s2testing.cc
-            src/s2/thread_testing.cc)
+
+if (GTEST_ROOT)
+  add_library(s2testing STATIC
+              src/s2/s2builderutil_testing.cc
+              src/s2/s2shapeutil_testing.cc
+              src/s2/s2testing.cc
+              src/s2/thread_testing.cc)
+endif()
+
 target_link_libraries(
     s2
     ${GFLAGS_LIBRARIES} ${GLOG_LIBRARIES} ${OPENSSL_LIBRARIES}
@@ -236,11 +239,14 @@ target_link_libraries(
     absl::type_traits
     absl::utility
     ${CMAKE_THREAD_LIBS_INIT})
-target_link_libraries(
-    s2testing
-    ${GFLAGS_LIBRARIES} ${GLOG_LIBRARIES}
-    absl::memory
-    absl::strings)
+
+if (GTEST_ROOT)
+    target_link_libraries(
+      s2testing
+      ${GFLAGS_LIBRARIES} ${GLOG_LIBRARIES}
+      absl::memory
+      absl::strings)
+endif()
 
 # Allow other CMake projects to use this one with:
 # list(APPEND CMAKE_MODULE_PATH "<path_to_s2geometry_dir>/third_party/cmake")
@@ -409,7 +415,14 @@ install(FILES src/s2/util/math/exactfloat/exactfloat.h
 install(FILES src/s2/util/units/length-units.h
               src/s2/util/units/physical-units.h
         DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/s2/util/units")
-install(TARGETS s2 s2testing
+
+if (GTEST_ROOT)
+    set(S2_TARGETS s2 s2testing)
+else()
+    set(S2_TARGETS s2)
+endif()
+
+install(TARGETS ${S2_TARGETS}
         RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
         ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
         LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
@@ -548,7 +561,7 @@ if (GTEST_ROOT)
   endforeach()
 endif()
 
-if (BUILD_EXAMPLES)
+if (BUILD_EXAMPLES AND TARGET s2testing)
   add_subdirectory("doc/examples" examples)
 endif()
 
diff --git a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/base/logging.h b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/base/logging.h
index b5a26715441..5136b297c43 100644
--- a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/base/logging.h
+++ b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/base/logging.h
@@ -62,7 +62,7 @@ class S2LogMessage {
     : severity_(severity), stream_(stream) {
     if (enabled()) {
       stream_ << file << ":" << line << " "
-              << absl::LogSeverityName(severity) << " ";
+              << absl::LogSeverityName(severity_) << " ";
     }
   }
   ~S2LogMessage() { if (enabled()) stream_ << std::endl; }
diff --git a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/base/port.h b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/base/port.h
index 0efaba84248..328393d2ffd 100644
--- a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/base/port.h
+++ b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/base/port.h
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
diff --git a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2edge_crossings_internal.h b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2edge_crossings_internal.h
index 24877542d54..2d7584389f2 100644
--- a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2edge_crossings_internal.h
+++ b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2edge_crossings_internal.h
@@ -104,18 +104,17 @@ class S2Point_PointerRep {
   const S2Point* p_;
 };
 
-class S2Point_ValueRep {
+class S2Point_ValueRep : public S2Point {
  public:
   using T = const S2Point &;
-  S2Point_ValueRep() : p_() {}
-  explicit S2Point_ValueRep(const S2Point& p) : p_(p) {}
-  S2Point_ValueRep& operator=(const S2Point& p) { p_ = p; return *this; }
-  operator const S2Point&() const { return p_; }  // Conversion operator.
-  const S2Point& operator*() const { return p_; }
-  const S2Point* operator->() const { return &p_; }
-
- private:
-  S2Point p_;
+  S2Point_ValueRep() : S2Point{} {}
+  explicit S2Point_ValueRep(const S2Point& p) : S2Point{p} {}
+  S2Point_ValueRep& operator=(const S2Point& p) {
+    static_cast<S2Point&>(*this) = p;
+    return *this;
+  }
+  const S2Point& operator*() const { return *this; }
+  const S2Point* operator->() const { return this; }
 };
 
 }  // namespace internal
diff --git a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2loop_measures.h b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2loop_measures.h
index cdc8e87cd8d..fb1df468a10 100644
--- a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2loop_measures.h
+++ b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2loop_measures.h
@@ -226,7 +226,7 @@ T GetSurfaceIntegral(S2PointLoopSpan loop,
   if (loop.size() < 3) return sum;
 
   S2Point origin = loop[0];
-  for (int i = 1; i + 1 < loop.size(); ++i) {
+  for (int i = 1; i + 1 < static_cast<int>(loop.size()); ++i) {
     // Let V_i be loop[i], let O be the current origin, and let length(A, B)
     // be the length of edge (A, B).  At the start of each loop iteration, the
     // "leading edge" of the triangle fan is (O, V_i), and we want to extend
diff --git a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2polygon.h b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2polygon.h
index 6ce2ebf8c39..61935099d27 100644
--- a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2polygon.h
+++ b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2polygon.h
@@ -1028,10 +1028,10 @@ inline S2Shape::ChainPosition S2Polygon::Shape::chain_position(int e) const {
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
diff --git a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2region_coverer.h b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2region_coverer.h
index 575f74c3ad4..1969b320cb7 100644
--- a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2region_coverer.h
+++ b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/s2region_coverer.h
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
diff --git a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/util/coding/coder.h b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/util/coding/coder.h
index f6556774c11..9b0d959d2f9 100644
--- a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/util/coding/coder.h
+++ b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/util/coding/coder.h
@@ -134,7 +134,7 @@ class Encoder {
 
   // REQUIRES: length() >= N
   // Removes the last N bytes out of the encoded buffer
-  void RemoveLast(size_t N) { writer().skip(-N); }
+  void RemoveLast(size_t N) { writer().skip(-static_cast<ptrdiff_t>(N)); }
 
   // REQUIRES: length() >= N
   // Removes the last length()-N bytes to make the encoded buffer have length N
@@ -495,7 +495,7 @@ inline void DecoderExtensions::FillArray(Decoder* array, int num_decoders) {
                 "Decoder must be trivially copy-assignable");
   static_assert(absl::is_trivially_destructible<Decoder>::value,
                 "Decoder must be trivially destructible");
-  std::memset(array, 0, num_decoders * sizeof(Decoder));
+  std::memset(reinterpret_cast<char*>(array), 0, num_decoders * sizeof(Decoder));
 }
 
 inline unsigned char Decoder::get8() {
diff --git a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/util/math/vector.h b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/util/math/vector.h
index d9b7043a39c..2718c16e5a4 100644
--- a/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/util/math/vector.h
+++ b/3rdParty/s2geometry/0e7b146184fbf119e60ceaf176c2b580c9d2cef4/src/s2/util/math/vector.h
@@ -50,6 +50,7 @@ namespace internal_vector {
 // CRTP base class for all Vector templates.
 template <template <typename> class VecTemplate, typename T, std::size_t N>
 class BasicVector {
+ public:
   using D = VecTemplate<T>;
 
  protected:
@@ -97,12 +98,6 @@ class BasicVector {
     return static_cast<const D&>(*this).Data()[b];
   }
 
-  // TODO(user): Relationals should be nonmembers.
-  bool operator==(const D& b) const {
-    const T* ap = static_cast<const D&>(*this).Data();
-    return std::equal(ap, ap + this->Size(), b.Data());
-  }
-  bool operator!=(const D& b) const { return !(AsD() == b); }
   bool operator<(const D& b) const {
     const T* ap = static_cast<const D&>(*this).Data();
     const T* bp = b.Data();
@@ -334,6 +329,23 @@ class BasicVector {
   }
 };
 
+template<typename T, template<typename> class VecTemplate, std::size_t N>
+bool operator==(const BasicVector<VecTemplate, T, N>& lhs,
+                const BasicVector<VecTemplate, T, N>& rhs) {
+  auto& vector1 =
+      static_cast<const typename BasicVector<VecTemplate, T, N>::D&>(lhs);
+  auto& vector2 =
+      static_cast<const typename BasicVector<VecTemplate, T, N>::D&>(rhs);
+  return std::equal(vector1.Data(), vector1.Data() + vector1.Size(),
+                    vector2.Data(), vector2.Data() + vector2.Size());
+}
+
+template<typename T, template<typename> class VecTemplate, std::size_t N>
+bool operator!=(const BasicVector<VecTemplate, T, N>& lhs,
+                const BasicVector<VecTemplate, T, N>& rhs) {
+  return !(lhs == rhs);
+}
+
 // These templates must be defined outside of BasicVector so that the
 // template specialization match algorithm must deduce 'a'. See the review
 // of cl/119944115.
```

## snappy

Compression library
https://github.com/google/snappy

## snowball

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
