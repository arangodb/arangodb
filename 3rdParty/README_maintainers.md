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

Forward port of C++20 date/time class. We have patched this slightly to
avoid a totally unnecessary user database lookup which poses problems
with static glibc builds and nsswitch.

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

Updated to the head of the dev branch to make it possible to compile
ArangoDB with clang 15.0.7 in an 3.18 Alpine container.

The following change has been made to jemalloc compared to upstream commit
e4817c8d89a2a413e835c4adeab5c5c4412f9235:

```diff
diff --git a/3rdParty/jemalloc/jemalloc/src/pages.c b/3rdParty/jemalloc/jemalloc/src/pages.c
index 8cf2fd9f876..11489b3f03d 100644
--- a/3rdParty/jemalloc/jemalloc/src/pages.c
+++ b/3rdParty/jemalloc/jemalloc/src/pages.c
@@ -37,7 +37,7 @@ size_t        os_page;
 
 #ifndef _WIN32
 #  define PAGES_PROT_COMMIT (PROT_READ | PROT_WRITE)
-#  define PAGES_PROT_DECOMMIT (PROT_NONE)
+#  define PAGES_PROT_DECOMMIT (PROT_READ | PROT_WRITE)
 static int     mmap_flags;
 #endif
 static bool    os_overcommits;
```

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
index 4044cd90a00..f22895951e7 100644
--- a/3rdParty/s2geometry/master/CMakeLists.txt
+++ b/3rdParty/s2geometry/master/CMakeLists.txt
@@ -260,13 +260,14 @@ endif()
 # list(APPEND CMAKE_MODULE_PATH "<path_to_s2geometry_dir>/third_party/cmake")
 # add_subdirectory(<path_to_s2geometry_dir> s2geometry)
 # target_link_libraries(<target_name> s2)
-target_include_directories(s2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
+target_include_directories(s2 SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
 
 # Add version information to the target
 set_target_properties(s2 PROPERTIES
     SOVERSION ${PROJECT_VERSION_MAJOR}
     VERSION ${PROJECT_VERSION})
 
+#[[
 # We don't need to install all headers, only those
 # transitively included by s2 headers we are exporting.
 install(FILES src/s2/_fp_contract_off.h
@@ -438,6 +439,7 @@ install(TARGETS ${S2_TARGETS}
         RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
         ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
         LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
+]]
 
 message("GTEST_ROOT: ${GTEST_ROOT}")
 if (GTEST_ROOT)
diff --git a/3rdParty/s2geometry/master/src/s2/s2polygon.h b/3rdParty/s2geometry/master/src/s2/s2polygon.h
index 9981ae0ff89..eb8ca50e8fd 100644
--- a/3rdParty/s2geometry/master/src/s2/s2polygon.h
+++ b/3rdParty/s2geometry/master/src/s2/s2polygon.h
@@ -701,6 +701,7 @@ class S2Polygon final : public S2Region {
   S2Polygon* Clone() const override;
   S2Cap GetCapBound() const override;  // Cap surrounding rect bound.
   S2LatLngRect GetRectBound() const override { return bound_; }
+  S2LatLngRect GetSubRegionBound() const { return subregion_bound_; }
   void GetCellUnionBound(std::vector<S2CellId> *cell_ids) const override;
 
   bool Contains(const S2Cell& cell) const override;
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

Our copy of swagger-ui resides at `js/server/assets/swagger`. The `index.css`
and `swagger-initializer.js` files contain a few tweaks to make swagger-ui look
a little nicer and make it work with the web interface.

To upgrade to a newer version:

1. Copy the files `js/server/assets/swagger/index.css` and
   `js/server/assets/swagger/swagger-initializer.js`
   to a safe location and open them in an editor
2. Delete all existing files inside `js/server/assets/swagger`
3. Download the release bundle of swagger-ui you want to upgrade to
4. Copy all files from the bundle's `dist` folder into `js/server/assets/swagger`,
   but delete the unnecessary `*es-bundle*` and non-bundle files (`swagger-ui.*`)
5. Open the new `js/server/assets/swagger/index.css` file in an editor
6. Apply the style adjustments from the old copy to the new file, indicated by
   code comments in the following format:
   `/* #region ArangoDB-specific changes */` and `/* #endregion */`
7. Open the new `js/server/assets/swagger/swagger-initializer.js` file in an editor
8. Add a comment to the start of the file indicating the release version number,
   e.g.  `// Version: swagger-ui 5.6.7`
9. Apply all code changes from the old copy to the new file,
   indicated by code comments in the following format:
   `#region ArangoDB-specific changes` and `#endregion`
10. Verify the changes were applied correctly and discard the old copies of
    `index.css` and `swagger-initializer.js`
11. Update the information in `LICENSES-OTHER-COMPONENTS.md` for swagger-ui

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

  This confirms the `requestInterceptor`-related changes for authentication
  were applied correctly.

* When using the `POST /_api/index#persistent` endpoint with any collection name,
  the response URL should contain `?collection=<collection-name>` but not contain
  `#persistent` anywhere.

  This confirms the `requestInterceptor`-related changes for removing
  fragment identifiers used for disambiguation in OpenAPI were applied correctly.

* All text in the API documentation should use readable color combinations.
  The API documentation should NOT look obviously "broken" or "ugly".

  Text should NOT partially have a font size of 12px or smaller but 14px.

  Inline code should be black, NOT purple, and the background should only have
  little padding that only slightly overlaps with other inline code in the
  above or below line.

  Code blocks should have a background, but NOT the individual lines of it.
  The font weight should be normal, NOT bold.

  Models should NOT have a background, expandible nested models should only have
  a slightly larger font size than the properties. Property descriptions should
  NOT use a monospace but a sans-serif font.

  This indicates the stylistic CSS changes were applied correctly and that the
  HTML IDs and classes are unchanged.

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
