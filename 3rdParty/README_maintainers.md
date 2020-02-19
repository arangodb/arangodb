3rd Party components and what to do on update
=============================================

** EDIT ../LICENSES-OTHER-COMPONENTS.md **

## boost

https://www.boost.org/

(we don't ship the upstream documentation!)
To remove some unused doc files, you can run something as follows:

    cd 3rdParty/boost/1.69.0
    for i in `find -type d -name "doc"`; do git rm -r "$i"; done


## cmake

Custom boost locator

## curl

HTTP client library https://curl.haxx.se/

We apply several build system patches to avoid unneccessary recompiles, bugfixes.

For example, we commented out curl's check for nroff to avoid documentation
building (and complaining about it on Windows).

We also deactivated some of curl's install CMake commands, as we don't need
anything installed (apart from that the vanilla curl install commands don't work
when compiled as part of ArangoDB).

We also disabled adding the OpenSSL libraries via the following command:

   list(APPEND CURL_LIBS OpenSSL::SSL OpenSSL::Crypto)

and instead are using the command
 
   list(APPEND CURL_LIBS ${OPENSSL_LIBRARIES})

from the previous version of curl's CMake file. When not applying this change,
the static builds try to look for libssl.so, which will not work.

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

## linenoise-ng

Our maintained fork of linenoise
https://github.com/arangodb/linenoise-ng

We may want to switch to replxx (uniting our & other forks):
https://github.com/AmokHuginnsson/replxx


## lz4

https://github.com/lz4/lz4

## rocksdb

our branch is maintained at:
https://github.com/arangodb-helper/rocksdb

Most changes can be ported upstream:
https://github.com/facebook/rocksdb

On Upgrade:
- `./thirdparty.inc``needs to be adjusted to use the snappy we specify. This can be
   adjusted by commenting out the section that sets Snappy-related CMake variables:

    -set(SNAPPY_HOME $ENV{THIRDPARTY_HOME}/Snappy.Library)
    -set(SNAPPY_INCLUDE ${SNAPPY_HOME}/build/native/inc/inc)
    -set(SNAPPY_LIB_DEBUG ${SNAPPY_HOME}/lib/native/debug/amd64/snappy.lib)
    -set(SNAPPY_LIB_RELEASE ${SNAPPY_HOME}/lib/native/retail/amd64/snappy.lib)
    +#set(SNAPPY_HOME $ENV{THIRDPARTY_HOME}/Snappy.Library)
    +#set(SNAPPY_INCLUDE ${SNAPPY_HOME}/build/native/inc/inc)
    +#set(SNAPPY_LIB_DEBUG ${SNAPPY_HOME}/lib/native/debug/amd64/snappy.lib)
    +#set(SNAPPY_LIB_RELEASE ${SNAPPY_HOME}/lib/native/retail/amd64/snappy.lib)

- fix timestamp in `./CMakeLists.txt` to avoid recompilation

    -string(TIMESTAMP GIT_DATE_TIME "%Y/%m/%d %H:%M:%S" UTC)
    +string(TIMESTAMP TS "%Y/%m/%d %H:%M:%S" UTC )
    +set(GIT_DATE_TIME "${TS}" CACHE STRING "the time we first built rocksdb")


## s2geometry

http://s2geometry.io/

## snappy

Compression library
https://github.com/google/snappy

We change the target `snappy` to `snapy-dyn` so cmake doesn't interfere targets with the static library (that we need)

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

To verify the changes were applied correctly, start ArangoDB and
open the ArangoDB Rest API documentation in the ArangoDB web interface.
Routes can be executed by clicking on them to expand their documentation,
clicking the _Try it out_ button, filling out any required fields and clicking
the _Execute_ button.

* The Swagger top bar containing an URL field should NOT appear.

  This confirms that the change hiding the top bar was applied correctly.

* The API documentation should appear, NOT an error message.

  This confirms that the change inferring the URL was applied correctly.

* All sections of the API documentation should be expanded to show all
  routes but NOT fully expanded to reveal descriptions and examples.

  This confirms the change to `docExpansion` was applied correctly.

* When using the `POST /_api/cursor` route with a query the authenticated
  user is authorized to execute, the response should not indicate an
  ArangoDB authentication error.

  This confirms the `requestInterceptor`-related changes were applied correctly.

* All text in the API documentation should use readable color combinations.
  The API documentation should NOT look obviously "broken" or "ugly".

  This indicates the stylistic CSS changes were applied correctly.

Note that to account for changes introduced by new versions of swagger-ui,
the stylistic CSS changes may need to be adjusted manually even when
applied correctly.

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
