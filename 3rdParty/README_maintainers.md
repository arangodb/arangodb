3rd Party components and what to do on update
=============================================

## boost

https://www.boost.org/

(we don't ship the upstream documentation!)
To remove some unused doc files, you can run something as follows:

    cd 3rdParty/boost/1.69.0
    for i in `find -type d -name "doc"`; do git rm -r "$i"; done


## catch

https://github.com/catchorg/Catch2

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
- `./thirdparty.inc` needs to be adjusted to use the snappy we specify see old commit

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

## snowball

http://snowball.tartarus.org/ stemming for IResearch. We use the latest provided cmake which we maintain.

## swagger-ui

https://github.com/swagger-api/swagger-ui/releases

Copy contents of `dist` folder in the release bundle to `js/assets/swagger`.

Our `index.html` contains a few tweaks to make swagger-ui work with the web interface.
Generally:

1. CSS: some changes to match our look & feel better
2. CSS: hiding the `.topbar` because we don't need it
3. JS: some logic to infer the `url` of the `swagger.json`
4. JS: some logic to inject the `jwt` as fallback auth via `requestInterceptor`
5. An HTML comment at the start of the file indicating the swagger-ui version

To determine which exact changes you need to make to `index.html`
compare the file you're replacing against the upstream version it was based on.

Note that the changes we made need to be re-applied manually as details like
CSS class names may change between different versions of swagger-ui.

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
