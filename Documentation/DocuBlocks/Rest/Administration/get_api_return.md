
@startDocuBlock get_api_return
@brief returns the server version number

@RESTHEADER{GET /_api/version, Return server version, RestVersionHandler}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{details,boolean,optional}
If set to *true*, the response will contain a *details* attribute with
additional information about included components and their versions. The
attribute names and internals of the *details* object may vary depending on
platform and ArangoDB version.

@RESTDESCRIPTION
Returns the server name and version number. The response is a JSON object
with the following attributes:

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned in all cases.

@RESTREPLYBODY{server,string,required,string}
will always contain *arango*

@RESTREPLYBODY{version,string,required,string}
the server version string. The string has the format
"*major*.*minor*.*sub*". *major* and *minor* will be numeric, and *sub*
may contain a number or a textual version.

@RESTREPLYBODY{details,object,optional,version_details_struct}
an optional JSON object with additional details. This is
returned only if the *details* query parameter is set to *true* in the
request.

@RESTSTRUCT{architecture,version_details_struct,string,optional,}
The CPU architecture, i.e. *64bit*

@RESTSTRUCT{arm,version_details_struct,string,optional,}
*false* - this is not running on an ARM cpu

@RESTSTRUCT{asan,version_details_struct,string,optional,}
has this been compiled with the asan address sanitizer turned on? (should be false)

@RESTSTRUCT{asm-crc32,version_details_struct,string,optional,}
do we have assembler implemented CRC functions?

@RESTSTRUCT{assertions,version_details_struct,string,optional,}
do we have assertions compiled in (=> developer version)

@RESTSTRUCT{boost-version,version_details_struct,string,optional,}
which boost version do we bind

@RESTSTRUCT{build-date,version_details_struct,string,optional,}
the date when this binary was created

@RESTSTRUCT{build-repository,version_details_struct,string,optional,}
reference to the git-ID this was compiled from

@RESTSTRUCT{compiler,version_details_struct,string,optional,}
which compiler did we use

@RESTSTRUCT{cplusplus,version_details_struct,string,optional,}
C++ standards version

@RESTSTRUCT{debug,version_details_struct,string,optional,}
*false* for production binaries

@RESTSTRUCT{endianness,version_details_struct,string,optional,}
currently only *little* is supported

@RESTSTRUCT{failure-tests,version_details_struct,string,optional,}
*false* for production binaries (the facility to invoke fatal errors is disabled)

@RESTSTRUCT{fd-client-event-handler,version_details_struct,string,optional,}
which method do we use to handle fd-sets, *poll* should be here on linux.

@RESTSTRUCT{fd-setsize,version_details_struct,string,optional,}
if not *poll* the fd setsize is valid for the maximum number of filedescriptors

@RESTSTRUCT{full-version-string,version_details_struct,string,optional,}
The full version string

@RESTSTRUCT{icu-version,version_details_struct,string,optional,}
Which version of ICU do we bundle

@RESTSTRUCT{jemalloc,version_details_struct,string,optional,}
*true* if we use jemalloc

@RESTSTRUCT{maintainer-mode,version_details_struct,string,optional,}
*false* if this is a production binary

@RESTSTRUCT{openssl-version,version_details_struct,string,optional,}
which openssl version do we link?

@RESTSTRUCT{platform,version_details_struct,string,optional,}
the host os - *linux*, *windows* or *darwin*

@RESTSTRUCT{reactor-type,version_details_struct,string,optional,}
*epoll* TODO

@RESTSTRUCT{rocksdb-version,version_details_struct,string,optional,}
the rocksdb version this release bundles

@RESTSTRUCT{server-version,version_details_struct,string,optional,}
the ArangoDB release version

@RESTSTRUCT{sizeof int,version_details_struct,string,optional,}
number of bytes for *integers*

@RESTSTRUCT{sizeof void*,version_details_struct,string,optional,}
number of bytes for *void pointers*

@RESTSTRUCT{sse42,version_details_struct,string,optional,}
do we have a SSE 4.2 enabled cpu?

@RESTSTRUCT{unaligned-access,version_details_struct,string,optional,}
does this system support unaligned memory access?

@RESTSTRUCT{v8-version,version_details_struct,string,optional,}
the bundled V8 javascript engine version

@RESTSTRUCT{vpack-version,version_details_struct,string,optional,}
the version of the used velocypack implementation

@RESTSTRUCT{zlib-version,version_details_struct,string,optional,}
the version of the bundled zlib

@RESTSTRUCT{mode,version_details_struct,string,optional,}
the mode we're runnig as - one of [*server*, *console*, *script*]

@RESTSTRUCT{host,version_details_struct,string,optional,}
the host ID

@EXAMPLES

Return the version information

@EXAMPLE_ARANGOSH_RUN{RestVersion}
    var response = logCurlRequest('GET', '/_api/version');

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Return the version information with details

@EXAMPLE_ARANGOSH_RUN{RestVersionDetails}
    var response = logCurlRequest('GET', '/_api/version?details=true');

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
