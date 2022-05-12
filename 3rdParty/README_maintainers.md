3rd Party components and what to do on update
=============================================

---

**Do not forget to update `../LICENSES-OTHER-COMPONENTS.md`!**

---

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

http://s2geometry.io/

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
