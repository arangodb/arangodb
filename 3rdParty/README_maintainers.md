3rd Party components and what to do on update
=============================================

## asio

https://think-async.com/Asio/

## boost

https://www.boost.org/

(we don't ship the upstream documentation!)


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
- thirdparty.inc needs to be adjusted to use the snappy we specify

## s2geometry

http://s2geometry.io/

## snappy

Compression library
https://github.com/google/snappy

## snowball

http://snowball.tartarus.org/ stemming for IResearch. We use the latest provided cmake which we maintain.

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
