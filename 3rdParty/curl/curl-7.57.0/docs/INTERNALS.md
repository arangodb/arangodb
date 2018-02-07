curl internals
==============

 - [Intro](#intro)
 - [git](#git)
 - [Portability](#Portability)
 - [Windows vs Unix](#winvsunix)
 - [Library](#Library)
   - [`Curl_connect`](#Curl_connect)
   - [`Curl_do`](#Curl_do)
   - [`Curl_readwrite`](#Curl_readwrite)
   - [`Curl_done`](#Curl_done)
   - [`Curl_disconnect`](#Curl_disconnect)
 - [HTTP(S)](#http)
 - [FTP](#ftp)
   - [Kerberos](#kerberos)
 - [TELNET](#telnet)
 - [FILE](#file)
 - [SMB](#smb)
 - [LDAP](#ldap)
 - [E-mail](#email)
 - [General](#general)
 - [Persistent Connections](#persistent)
 - [multi interface/non-blocking](#multi)
 - [SSL libraries](#ssl)
 - [Library Symbols](#symbols)
 - [Return Codes and Informationals](#returncodes)
 - [AP/ABI](#abi)
 - [Client](#client)
 - [Memory Debugging](#memorydebug)
 - [Test Suite](#test)
 - [Asynchronous name resolves](#asyncdns)
   - [c-ares](#cares)
 - [`curl_off_t`](#curl_off_t)
 - [curlx](#curlx)
 - [Content Encoding](#contentencoding)
 - [hostip.c explained](#hostip)
 - [Track Down Memory Leaks](#memoryleak)
 - [`multi_socket`](#multi_socket)
 - [Structs in libcurl](#structs)

<a name="intro"></a>
Intro
=====

 This project is split in two. The library and the client. The client part
 uses the library, but the library is designed to allow other applications to
 use it.

 The largest amount of code and complexity is in the library part.


<a name="git"></a>
git
===

 All changes to the sources are committed to the git repository as soon as
 they're somewhat verified to work. Changes shall be committed as independently
 as possible so that individual changes can be easily spotted and tracked
 afterwards.

 Tagging shall be used extensively, and by the time we release new archives we
 should tag the sources with a name similar to the released version number.

<a name="Portability"></a>
Portability
===========

 We write curl and libcurl to compile with C89 compilers.  On 32bit and up
 machines. Most of libcurl assumes more or less POSIX compliance but that's
 not a requirement.

 We write libcurl to build and work with lots of third party tools, and we
 want it to remain functional and buildable with these and later versions
 (older versions may still work but is not what we work hard to maintain):

Dependencies
------------

 - OpenSSL      0.9.7
 - GnuTLS       1.2
 - zlib         1.1.4
 - libssh2      0.16
 - c-ares       1.6.0
 - libidn2      2.0.0
 - cyassl       2.0.0
 - openldap     2.0
 - MIT Kerberos 1.2.4
 - GSKit        V5R3M0
 - NSS          3.14.x
 - axTLS        2.1.0
 - PolarSSL     1.3.0
 - Heimdal      ?
 - nghttp2      1.0.0

Operating Systems
-----------------

 On systems where configure runs, we aim at working on them all - if they have
 a suitable C compiler. On systems that don't run configure, we strive to keep
 curl running correctly on:

 - Windows      98
 - AS/400       V5R3M0
 - Symbian      9.1
 - Windows CE   ?
 - TPF          ?

Build tools
-----------

 When writing code (mostly for generating stuff included in release tarballs)
 we use a few "build tools" and we make sure that we remain functional with
 these versions:

 - GNU Libtool  1.4.2
 - GNU Autoconf 2.57
 - GNU Automake 1.7
 - GNU M4       1.4
 - perl         5.004
 - roffit       0.5
 - groff        ? (any version that supports "groff -Tps -man [in] [out]")
 - ps2pdf (gs)  ?

<a name="winvsunix"></a>
Windows vs Unix
===============

 There are a few differences in how to program curl the Unix way compared to
 the Windows way. Perhaps the four most notable details are:

 1. Different function names for socket operations.

   In curl, this is solved with defines and macros, so that the source looks
   the same in all places except for the header file that defines them. The
   macros in use are sclose(), sread() and swrite().

 2. Windows requires a couple of init calls for the socket stuff.

   That's taken care of by the `curl_global_init()` call, but if other libs
   also do it etc there might be reasons for applications to alter that
   behaviour.

 3. The file descriptors for network communication and file operations are
    not as easily interchangeable as in Unix.

   We avoid this by not trying any funny tricks on file descriptors.

 4. When writing data to stdout, Windows makes end-of-lines the DOS way, thus
    destroying binary data, although you do want that conversion if it is
    text coming through... (sigh)

   We set stdout to binary under windows

 Inside the source code, We make an effort to avoid `#ifdef [Your OS]`. All
 conditionals that deal with features *should* instead be in the format
 `#ifdef HAVE_THAT_WEIRD_FUNCTION`. Since Windows can't run configure scripts,
 we maintain a `curl_config-win32.h` file in lib directory that is supposed to
 look exactly like a `curl_config.h` file would have looked like on a Windows
 machine!

 Generally speaking: always remember that this will be compiled on dozens of
 operating systems. Don't walk on the edge!

<a name="Library"></a>
Library
=======

 (See [Structs in libcurl](#structs) for the separate section describing all
 major internal structs and their purposes.)

 There are plenty of entry points to the library, namely each publicly defined
 function that libcurl offers to applications. All of those functions are
 rather small and easy-to-follow. All the ones prefixed with `curl_easy` are
 put in the lib/easy.c file.

 `curl_global_init()` and `curl_global_cleanup()` should be called by the
 application to initialize and clean up global stuff in the library. As of
 today, it can handle the global SSL initing if SSL is enabled and it can init
 the socket layer on windows machines. libcurl itself has no "global" scope.

 All printf()-style functions use the supplied clones in lib/mprintf.c. This
 makes sure we stay absolutely platform independent.

 [ `curl_easy_init()`][2] allocates an internal struct and makes some
 initializations.  The returned handle does not reveal internals. This is the
 `Curl_easy` struct which works as an "anchor" struct for all `curl_easy`
 functions. All connections performed will get connect-specific data allocated
 that should be used for things related to particular connections/requests.

 [`curl_easy_setopt()`][1] takes three arguments, where the option stuff must
 be passed in pairs: the parameter-ID and the parameter-value. The list of
 options is documented in the man page. This function mainly sets things in
 the `Curl_easy` struct.

 `curl_easy_perform()` is just a wrapper function that makes use of the multi
 API.  It basically calls `curl_multi_init()`, `curl_multi_add_handle()`,
 `curl_multi_wait()`, and `curl_multi_perform()` until the transfer is done
 and then returns.

 Some of the most important key functions in url.c are called from multi.c
 when certain key steps are to be made in the transfer operation.

<a name="Curl_connect"></a>
Curl_connect()
--------------

   Analyzes the URL, it separates the different components and connects to the
   remote host. This may involve using a proxy and/or using SSL. The
   `Curl_resolv()` function in lib/hostip.c is used for looking up host names
   (it does then use the proper underlying method, which may vary between
   platforms and builds).

   When `Curl_connect` is done, we are connected to the remote site. Then it
   is time to tell the server to get a document/file. `Curl_do()` arranges
   this.

   This function makes sure there's an allocated and initiated 'connectdata'
   struct that is used for this particular connection only (although there may
   be several requests performed on the same connect). A bunch of things are
   inited/inherited from the `Curl_easy` struct.

<a name="Curl_do"></a>
Curl_do()
---------

   `Curl_do()` makes sure the proper protocol-specific function is called. The
   functions are named after the protocols they handle.

   The protocol-specific functions of course deal with protocol-specific
   negotiations and setup. They have access to the `Curl_sendf()` (from
   lib/sendf.c) function to send printf-style formatted data to the remote
   host and when they're ready to make the actual file transfer they call the
   `Curl_Transfer()` function (in lib/transfer.c) to setup the transfer and
   returns.

   If this DO function fails and the connection is being re-used, libcurl will
   then close this connection, setup a new connection and re-issue the DO
   request on that. This is because there is no way to be perfectly sure that
   we have discovered a dead connection before the DO function and thus we
   might wrongly be re-using a connection that was closed by the remote peer.

   Some time during the DO function, the `Curl_setup_transfer()` function must
   be called with some basic info about the upcoming transfer: what socket(s)
   to read/write and the expected file transfer sizes (if known).

<a name="Curl_readwrite"></a>
Curl_readwrite()
----------------

   Called during the transfer of the actual protocol payload.

   During transfer, the progress functions in lib/progress.c are called at
   frequent intervals (or at the user's choice, a specified callback might get
   called). The speedcheck functions in lib/speedcheck.c are also used to
   verify that the transfer is as fast as required.

<a name="Curl_done"></a>
Curl_done()
-----------

   Called after a transfer is done. This function takes care of everything
   that has to be done after a transfer. This function attempts to leave
   matters in a state so that `Curl_do()` should be possible to call again on
   the same connection (in a persistent connection case). It might also soon
   be closed with `Curl_disconnect()`.

<a name="Curl_disconnect"></a>
Curl_disconnect()
-----------------

   When doing normal connections and transfers, no one ever tries to close any
   connections so this is not normally called when `curl_easy_perform()` is
   used. This function is only used when we are certain that no more transfers
   are going to be made on the connection. It can be also closed by force, or
   it can be called to make sure that libcurl doesn't keep too many
   connections alive at the same time.

   This function cleans up all resources that are associated with a single
   connection.

<a name="http"></a>
HTTP(S)
=======

 HTTP offers a lot and is the protocol in curl that uses the most lines of
 code. There is a special file (lib/formdata.c) that offers all the multipart
 post functions.

 base64-functions for user+password stuff (and more) is in (lib/base64.c) and
 all functions for parsing and sending cookies are found in (lib/cookie.c).

 HTTPS uses in almost every case the same procedure as HTTP, with only two
 exceptions: the connect procedure is different and the function used to read
 or write from the socket is different, although the latter fact is hidden in
 the source by the use of `Curl_read()` for reading and `Curl_write()` for
 writing data to the remote server.

 `http_chunks.c` contains functions that understands HTTP 1.1 chunked transfer
 encoding.

 An interesting detail with the HTTP(S) request, is the `Curl_add_buffer()`
 series of functions we use. They append data to one single buffer, and when
 the building is finished the entire request is sent off in one single write. This is done this way to overcome problems with flawed firewalls and lame servers.

<a name="ftp"></a>
FTP
===

 The `Curl_if2ip()` function can be used for getting the IP number of a
 specified network interface, and it resides in lib/if2ip.c.

 `Curl_ftpsendf()` is used for sending FTP commands to the remote server. It
 was made a separate function to prevent us programmers from forgetting that
 they must be CRLF terminated. They must also be sent in one single write() to
 make firewalls and similar happy.

<a name="kerberos"></a>
Kerberos
--------

 Kerberos support is mainly in lib/krb5.c and lib/security.c but also
 `curl_sasl_sspi.c` and `curl_sasl_gssapi.c` for the email protocols and
 `socks_gssapi.c` and `socks_sspi.c` for SOCKS5 proxy specifics.

<a name="telnet"></a>
TELNET
======

 Telnet is implemented in lib/telnet.c.

<a name="file"></a>
FILE
====

 The file:// protocol is dealt with in lib/file.c.

<a name="smb"></a>
SMB
===

 The smb:// protocol is dealt with in lib/smb.c.

<a name="ldap"></a>
LDAP
====

 Everything LDAP is in lib/ldap.c and lib/openldap.c

<a name="email"></a>
E-mail
======

 The e-mail related source code is in lib/imap.c, lib/pop3.c and lib/smtp.c.

<a name="general"></a>
General
=======

 URL encoding and decoding, called escaping and unescaping in the source code,
 is found in lib/escape.c.

 While transferring data in Transfer() a few functions might get used.
 `curl_getdate()` in lib/parsedate.c is for HTTP date comparisons (and more).

 lib/getenv.c offers `curl_getenv()` which is for reading environment
 variables in a neat platform independent way. That's used in the client, but
 also in lib/url.c when checking the proxy environment variables. Note that
 contrary to the normal unix getenv(), this returns an allocated buffer that
 must be free()ed after use.

 lib/netrc.c holds the .netrc parser

 lib/timeval.c features replacement functions for systems that don't have
 gettimeofday() and a few support functions for timeval conversions.

 A function named `curl_version()` that returns the full curl version string
 is found in lib/version.c.

<a name="persistent"></a>
Persistent Connections
======================

 The persistent connection support in libcurl requires some considerations on
 how to do things inside of the library.

 - The `Curl_easy` struct returned in the [`curl_easy_init()`][2] call
   must never hold connection-oriented data. It is meant to hold the root data
   as well as all the options etc that the library-user may choose.

 - The `Curl_easy` struct holds the "connection cache" (an array of
   pointers to 'connectdata' structs).

 - This enables the 'curl handle' to be reused on subsequent transfers.

 - When libcurl is told to perform a transfer, it first checks for an already
   existing connection in the cache that we can use. Otherwise it creates a
   new one and adds that to the cache. If the cache is full already when a new
   connection is added, it will first close the oldest unused one.

 - When the transfer operation is complete, the connection is left
   open. Particular options may tell libcurl not to, and protocols may signal
   closure on connections and then they won't be kept open, of course.

 - When `curl_easy_cleanup()` is called, we close all still opened connections,
   unless of course the multi interface "owns" the connections.

 The curl handle must be re-used in order for the persistent connections to
 work.

<a name="multi"></a>
multi interface/non-blocking
============================

 The multi interface is a non-blocking interface to the library. To make that
 interface work as well as possible, no low-level functions within libcurl
 must be written to work in a blocking manner. (There are still a few spots
 violating this rule.)

 One of the primary reasons we introduced c-ares support was to allow the name
 resolve phase to be perfectly non-blocking as well.

 The FTP and the SFTP/SCP protocols are examples of how we adapt and adjust
 the code to allow non-blocking operations even on multi-stage command-
 response protocols. They are built around state machines that return when
 they would otherwise block waiting for data.  The DICT, LDAP and TELNET
 protocols are crappy examples and they are subject for rewrite in the future
 to better fit the libcurl protocol family.

<a name="ssl"></a>
SSL libraries
=============

 Originally libcurl supported SSLeay for SSL/TLS transports, but that was then
 extended to its successor OpenSSL but has since also been extended to several
 other SSL/TLS libraries and we expect and hope to further extend the support
 in future libcurl versions.

 To deal with this internally in the best way possible, we have a generic SSL
 function API as provided by the vtls/vtls.[ch] system, and they are the only
 SSL functions we must use from within libcurl. vtls is then crafted to use
 the appropriate lower-level function calls to whatever SSL library that is in
 use. For example vtls/openssl.[ch] for the OpenSSL library.

<a name="symbols"></a>
Library Symbols
===============

 All symbols used internally in libcurl must use a `Curl_` prefix if they're
 used in more than a single file. Single-file symbols must be made static.
 Public ("exported") symbols must use a `curl_` prefix. (There are exceptions,
 but they are to be changed to follow this pattern in future versions.) Public
 API functions are marked with `CURL_EXTERN` in the public header files so
 that all others can be hidden on platforms where this is possible.

<a name="returncodes"></a>
Return Codes and Informationals
===============================

 I've made things simple. Almost every function in libcurl returns a CURLcode,
 that must be `CURLE_OK` if everything is OK or otherwise a suitable error
 code as the curl/curl.h include file defines. The very spot that detects an
 error must use the `Curl_failf()` function to set the human-readable error
 description.

 In aiding the user to understand what's happening and to debug curl usage, we
 must supply a fair number of informational messages by using the
 `Curl_infof()` function. Those messages are only displayed when the user
 explicitly asks for them. They are best used when revealing information that
 isn't otherwise obvious.

<a name="abi"></a>
API/ABI
=======

 We make an effort to not export or show internals or how internals work, as
 that makes it easier to keep a solid API/ABI over time. See docs/libcurl/ABI
 for our promise to users.

<a name="client"></a>
Client
======

 main() resides in `src/tool_main.c`.

 `src/tool_hugehelp.c` is automatically generated by the mkhelp.pl perl script
 to display the complete "manual" and the `src/tool_urlglob.c` file holds the
 functions used for the URL-"globbing" support. Globbing in the sense that the
 {} and [] expansion stuff is there.

 The client mostly sets up its 'config' struct properly, then
 it calls the `curl_easy_*()` functions of the library and when it gets back
 control after the `curl_easy_perform()` it cleans up the library, checks
 status and exits.

 When the operation is done, the ourWriteOut() function in src/writeout.c may
 be called to report about the operation. That function is using the
 `curl_easy_getinfo()` function to extract useful information from the curl
 session.

 It may loop and do all this several times if many URLs were specified on the
 command line or config file.

<a name="memorydebug"></a>
Memory Debugging
================

 The file lib/memdebug.c contains debug-versions of a few functions. Functions
 such as malloc, free, fopen, fclose, etc that somehow deal with resources
 that might give us problems if we "leak" them. The functions in the memdebug
 system do nothing fancy, they do their normal function and then log
 information about what they just did. The logged data can then be analyzed
 after a complete session,

 memanalyze.pl is the perl script present in tests/ that analyzes a log file
 generated by the memory tracking system. It detects if resources are
 allocated but never freed and other kinds of errors related to resource
 management.

 Internally, definition of preprocessor symbol DEBUGBUILD restricts code which
 is only compiled for debug enabled builds. And symbol CURLDEBUG is used to
 differentiate code which is _only_ used for memory tracking/debugging.

 Use -DCURLDEBUG when compiling to enable memory debugging, this is also
 switched on by running configure with --enable-curldebug. Use -DDEBUGBUILD
 when compiling to enable a debug build or run configure with --enable-debug.

 curl --version will list 'Debug' feature for debug enabled builds, and
 will list 'TrackMemory' feature for curl debug memory tracking capable
 builds. These features are independent and can be controlled when running
 the configure script. When --enable-debug is given both features will be
 enabled, unless some restriction prevents memory tracking from being used.

<a name="test"></a>
Test Suite
==========

 The test suite is placed in its own subdirectory directly off the root in the
 curl archive tree, and it contains a bunch of scripts and a lot of test case
 data.

 The main test script is runtests.pl that will invoke test servers like
 httpserver.pl and ftpserver.pl before all the test cases are performed. The
 test suite currently only runs on Unix-like platforms.

 You'll find a description of the test suite in the tests/README file, and the
 test case data files in the tests/FILEFORMAT file.

 The test suite automatically detects if curl was built with the memory
 debugging enabled, and if it was, it will detect memory leaks, too.

<a name="asyncdns"></a>
Asynchronous name resolves
==========================

 libcurl can be built to do name resolves asynchronously, using either the
 normal resolver in a threaded manner or by using c-ares.

<a name="cares"></a>
[c-ares][3]
------

### Build libcurl to use a c-ares

1. ./configure --enable-ares=/path/to/ares/install
2. make

### c-ares on win32

 First I compiled c-ares. I changed the default C runtime library to be the
 single-threaded rather than the multi-threaded (this seems to be required to
 prevent linking errors later on). Then I simply build the areslib project
 (the other projects adig/ahost seem to fail under MSVC).

 Next was libcurl. I opened lib/config-win32.h and I added a:
 `#define USE_ARES 1`

 Next thing I did was I added the path for the ares includes to the include
 path, and the libares.lib to the libraries.

 Lastly, I also changed libcurl to be single-threaded rather than
 multi-threaded, again this was to prevent some duplicate symbol errors. I'm
 not sure why I needed to change everything to single-threaded, but when I
 didn't I got redefinition errors for several CRT functions (malloc, stricmp,
 etc.)

<a name="curl_off_t"></a>
`curl_off_t`
==========

 `curl_off_t` is a data type provided by the external libcurl include
 headers. It is the type meant to be used for the [`curl_easy_setopt()`][1]
 options that end with LARGE. The type is 64bit large on most modern
 platforms.

curlx
=====

 The libcurl source code offers a few functions by source only. They are not
 part of the official libcurl API, but the source files might be useful for
 others so apps can optionally compile/build with these sources to gain
 additional functions.

 We provide them through a single header file for easy access for apps:
 "curlx.h"

`curlx_strtoofft()`
-------------------
   A macro that converts a string containing a number to a `curl_off_t` number.
   This might use the `curlx_strtoll()` function which is provided as source
   code in strtoofft.c. Note that the function is only provided if no
   strtoll() (or equivalent) function exist on your platform. If `curl_off_t`
   is only a 32 bit number on your platform, this macro uses strtol().

Future
------

 Several functions will be removed from the public `curl_` name space in a
 future libcurl release. They will then only become available as `curlx_`
 functions instead. To make the transition easier, we already today provide
 these functions with the `curlx_` prefix to allow sources to be built
 properly with the new function names. The concerned functions are:

 - `curlx_getenv`
 - `curlx_strequal`
 - `curlx_strnequal`
 - `curlx_mvsnprintf`
 - `curlx_msnprintf`
 - `curlx_maprintf`
 - `curlx_mvaprintf`
 - `curlx_msprintf`
 - `curlx_mprintf`
 - `curlx_mfprintf`
 - `curlx_mvsprintf`
 - `curlx_mvprintf`
 - `curlx_mvfprintf`

<a name="contentencoding"></a>
Content Encoding
================

## About content encodings

 [HTTP/1.1][4] specifies that a client may request that a server encode its
 response. This is usually used to compress a response using one (or more)
 encodings from a set of commonly available compression techniques. These
 schemes include 'deflate' (the zlib algorithm), 'gzip' 'br' (brotli) and
 'compress'. A client requests that the server perform an encoding by including
 an Accept-Encoding header in the request document. The value of the header
 should be one of the recognized tokens 'deflate', ... (there's a way to
 register new schemes/tokens, see sec 3.5 of the spec). A server MAY honor
 the client's encoding request. When a response is encoded, the server
 includes a Content-Encoding header in the response. The value of the
 Content-Encoding header indicates which encodings were used to encode the
 data, in the order in which they were applied.

 It's also possible for a client to attach priorities to different schemes so
 that the server knows which it prefers. See sec 14.3 of RFC 2616 for more
 information on the Accept-Encoding header. See sec [3.1.2.2 of RFC 7231][15]
 for more information on the Content-Encoding header.

## Supported content encodings

 The 'deflate', 'gzip' and 'br' content encodings are supported by libcurl.
 Both regular and chunked transfers work fine.  The zlib library is required
 for the 'deflate' and 'gzip' encodings, while the brotli decoding library is
 for the 'br' encoding.

## The libcurl interface

 To cause libcurl to request a content encoding use:

  [`curl_easy_setopt`][1](curl, [`CURLOPT_ACCEPT_ENCODING`][5], string)

 where string is the intended value of the Accept-Encoding header.

 Currently, libcurl does support multiple encodings but only
 understands how to process responses that use the "deflate", "gzip" and/or
 "br" content encodings, so the only values for [`CURLOPT_ACCEPT_ENCODING`][5]
 that will work (besides "identity," which does nothing) are "deflate",
 "gzip" and "br". If a response is encoded using the "compress" or methods,
 libcurl will return an error indicating that the response could
 not be decoded.  If <string> is NULL no Accept-Encoding header is generated.
 If <string> is a zero-length string, then an Accept-Encoding header
 containing all supported encodings will be generated.

 The [`CURLOPT_ACCEPT_ENCODING`][5] must be set to any non-NULL value for
 content to be automatically decoded.  If it is not set and the server still
 sends encoded content (despite not having been asked), the data is returned
 in its raw form and the Content-Encoding type is not checked.

## The curl interface

 Use the [--compressed][6] option with curl to cause it to ask servers to
 compress responses using any format supported by curl.

<a name="hostip"></a>
hostip.c explained
==================

 The main compile-time defines to keep in mind when reading the host*.c source
 file are these:

## `CURLRES_IPV6`

 this host has getaddrinfo() and family, and thus we use that. The host may
 not be able to resolve IPv6, but we don't really have to take that into
 account. Hosts that aren't IPv6-enabled have `CURLRES_IPV4` defined.

## `CURLRES_ARES`

 is defined if libcurl is built to use c-ares for asynchronous name
 resolves. This can be Windows or *nix.

## `CURLRES_THREADED`

 is defined if libcurl is built to use threading for asynchronous name
 resolves. The name resolve will be done in a new thread, and the supported
 asynch API will be the same as for ares-builds. This is the default under
 (native) Windows.

 If any of the two previous are defined, `CURLRES_ASYNCH` is defined too. If
 libcurl is not built to use an asynchronous resolver, `CURLRES_SYNCH` is
 defined.

## host*.c sources

 The host*.c sources files are split up like this:

 - hostip.c      - method-independent resolver functions and utility functions
 - hostasyn.c    - functions for asynchronous name resolves
 - hostsyn.c     - functions for synchronous name resolves
 - asyn-ares.c   - functions for asynchronous name resolves using c-ares
 - asyn-thread.c - functions for asynchronous name resolves using threads
 - hostip4.c     - IPv4 specific functions
 - hostip6.c     - IPv6 specific functions

 The hostip.h is the single united header file for all this. It defines the
 `CURLRES_*` defines based on the config*.h and `curl_setup.h` defines.

<a name="memoryleak"></a>
Track Down Memory Leaks
=======================

## Single-threaded

  Please note that this memory leak system is not adjusted to work in more
  than one thread. If you want/need to use it in a multi-threaded app. Please
  adjust accordingly.


## Build

  Rebuild libcurl with -DCURLDEBUG (usually, rerunning configure with
  --enable-debug fixes this). 'make clean' first, then 'make' so that all
  files are actually rebuilt properly. It will also make sense to build
  libcurl with the debug option (usually -g to the compiler) so that debugging
  it will be easier if you actually do find a leak in the library.

  This will create a library that has memory debugging enabled.

## Modify Your Application

  Add a line in your application code:

       `curl_memdebug("dump");`

  This will make the malloc debug system output a full trace of all resource
  using functions to the given file name. Make sure you rebuild your program
  and that you link with the same libcurl you built for this purpose as
  described above.

## Run Your Application

  Run your program as usual. Watch the specified memory trace file grow.

  Make your program exit and use the proper libcurl cleanup functions etc. So
  that all non-leaks are returned/freed properly.

## Analyze the Flow

  Use the tests/memanalyze.pl perl script to analyze the dump file:

    tests/memanalyze.pl dump

  This now outputs a report on what resources that were allocated but never
  freed etc. This report is very fine for posting to the list!

  If this doesn't produce any output, no leak was detected in libcurl. Then
  the leak is mostly likely to be in your code.

<a name="multi_socket"></a>
`multi_socket`
==============

 Implementation of the `curl_multi_socket` API

  The main ideas of this API are simply:

   1 - The application can use whatever event system it likes as it gets info
       from libcurl about what file descriptors libcurl waits for what action
       on. (The previous API returns `fd_sets` which is very select()-centric).

   2 - When the application discovers action on a single socket, it calls
       libcurl and informs that there was action on this particular socket and
       libcurl can then act on that socket/transfer only and not care about
       any other transfers. (The previous API always had to scan through all
       the existing transfers.)

  The idea is that [`curl_multi_socket_action()`][7] calls a given callback
  with information about what socket to wait for what action on, and the
  callback only gets called if the status of that socket has changed.

  We also added a timer callback that makes libcurl call the application when
  the timeout value changes, and you set that with [`curl_multi_setopt()`][9]
  and the [`CURLMOPT_TIMERFUNCTION`][10] option. To get this to work,
  Internally, there's an added struct to each easy handle in which we store
  an "expire time" (if any). The structs are then "splay sorted" so that we
  can add and remove times from the linked list and yet somewhat swiftly
  figure out both how long there is until the next nearest timer expires
  and which timer (handle) we should take care of now. Of course, the upside
  of all this is that we get a [`curl_multi_timeout()`][8] that should also
  work with old-style applications that use [`curl_multi_perform()`][11].

  We created an internal "socket to easy handles" hash table that given
  a socket (file descriptor) returns the easy handle that waits for action on
  that socket.  This hash is made using the already existing hash code
  (previously only used for the DNS cache).

  To make libcurl able to report plain sockets in the socket callback, we had
  to re-organize the internals of the [`curl_multi_fdset()`][12] etc so that
  the conversion from sockets to `fd_sets` for that function is only done in
  the last step before the data is returned. I also had to extend c-ares to
  get a function that can return plain sockets, as that library too returned
  only `fd_sets` and that is no longer good enough. The changes done to c-ares
  are available in c-ares 1.3.1 and later.

<a name="structs"></a>
Structs in libcurl
==================

This section should cover 7.32.0 pretty accurately, but will make sense even
for older and later versions as things don't change drastically that often.

## Curl_easy

  The `Curl_easy` struct is the one returned to the outside in the external API
  as a "CURL *". This is usually known as an easy handle in API documentations
  and examples.

  Information and state that is related to the actual connection is in the
  'connectdata' struct. When a transfer is about to be made, libcurl will
  either create a new connection or re-use an existing one. The particular
  connectdata that is used by this handle is pointed out by
  `Curl_easy->easy_conn`.

  Data and information that regard this particular single transfer is put in
  the SingleRequest sub-struct.

  When the `Curl_easy` struct is added to a multi handle, as it must be in
  order to do any transfer, the ->multi member will point to the `Curl_multi`
  struct it belongs to. The ->prev and ->next members will then be used by the
  multi code to keep a linked list of `Curl_easy` structs that are added to
  that same multi handle. libcurl always uses multi so ->multi *will* point to
  a `Curl_multi` when a transfer is in progress.

  ->mstate is the multi state of this particular `Curl_easy`. When
  `multi_runsingle()` is called, it will act on this handle according to which
  state it is in. The mstate is also what tells which sockets to return for a
  specific `Curl_easy` when [`curl_multi_fdset()`][12] is called etc.

  The libcurl source code generally use the name 'data' for the variable that
  points to the `Curl_easy`.

  When doing multiplexed HTTP/2 transfers, each `Curl_easy` is associated with
  an individual stream, sharing the same connectdata struct. Multiplexing
  makes it even more important to keep things associated with the right thing!

## connectdata

  A general idea in libcurl is to keep connections around in a connection
  "cache" after they have been used in case they will be used again and then
  re-use an existing one instead of creating a new as it creates a significant
  performance boost.

  Each 'connectdata' identifies a single physical connection to a server. If
  the connection can't be kept alive, the connection will be closed after use
  and then this struct can be removed from the cache and freed.

  Thus, the same `Curl_easy` can be used multiple times and each time select
  another connectdata struct to use for the connection. Keep this in mind, as
  it is then important to consider if options or choices are based on the
  connection or the `Curl_easy`.

  Functions in libcurl will assume that connectdata->data points to the
  `Curl_easy` that uses this connection (for the moment).

  As a special complexity, some protocols supported by libcurl require a
  special disconnect procedure that is more than just shutting down the
  socket. It can involve sending one or more commands to the server before
  doing so. Since connections are kept in the connection cache after use, the
  original `Curl_easy` may no longer be around when the time comes to shut down
  a particular connection. For this purpose, libcurl holds a special dummy
  `closure_handle` `Curl_easy` in the `Curl_multi` struct to use when needed.

  FTP uses two TCP connections for a typical transfer but it keeps both in
  this single struct and thus can be considered a single connection for most
  internal concerns.

  The libcurl source code generally use the name 'conn' for the variable that
  points to the connectdata.

## Curl_multi

  Internally, the easy interface is implemented as a wrapper around multi
  interface functions. This makes everything multi interface.

  `Curl_multi` is the multi handle struct exposed as "CURLM *" in external
  APIs.

  This struct holds a list of `Curl_easy` structs that have been added to this
  handle with [`curl_multi_add_handle()`][13]. The start of the list is
  `->easyp` and `->num_easy` is a counter of added `Curl_easy`s.

  `->msglist` is a linked list of messages to send back when
  [`curl_multi_info_read()`][14] is called. Basically a node is added to that
  list when an individual `Curl_easy`'s transfer has completed.

  `->hostcache` points to the name cache. It is a hash table for looking up
  name to IP. The nodes have a limited life time in there and this cache is
  meant to reduce the time for when the same name is wanted within a short
  period of time.

  `->timetree` points to a tree of `Curl_easy`s, sorted by the remaining time
  until it should be checked - normally some sort of timeout. Each `Curl_easy`
  has one node in the tree.

  `->sockhash` is a hash table to allow fast lookups of socket descriptor for
  which `Curl_easy` uses that descriptor. This is necessary for the
  `multi_socket` API.

  `->conn_cache` points to the connection cache. It keeps track of all
  connections that are kept after use. The cache has a maximum size.

  `->closure_handle` is described in the 'connectdata' section.

  The libcurl source code generally use the name 'multi' for the variable that
  points to the `Curl_multi` struct.

## Curl_handler

  Each unique protocol that is supported by libcurl needs to provide at least
  one `Curl_handler` struct. It defines what the protocol is called and what
  functions the main code should call to deal with protocol specific issues.
  In general, there's a source file named [protocol].c in which there's a
  "struct `Curl_handler` `Curl_handler_[protocol]`" declared. In url.c there's
  then the main array with all individual `Curl_handler` structs pointed to
  from a single array which is scanned through when a URL is given to libcurl
  to work with.

  `->scheme` is the URL scheme name, usually spelled out in uppercase. That's
  "HTTP" or "FTP" etc. SSL versions of the protocol need their own `Curl_handler` setup so HTTPS separate from HTTP.

  `->setup_connection` is called to allow the protocol code to allocate
  protocol specific data that then gets associated with that `Curl_easy` for
  the rest of this transfer. It gets freed again at the end of the transfer.
  It will be called before the 'connectdata' for the transfer has been
  selected/created. Most protocols will allocate its private
  'struct [PROTOCOL]' here and assign `Curl_easy->req.protop` to point to it.

  `->connect_it` allows a protocol to do some specific actions after the TCP
  connect is done, that can still be considered part of the connection phase.

  Some protocols will alter the `connectdata->recv[]` and
  `connectdata->send[]` function pointers in this function.

  `->connecting` is similarly a function that keeps getting called as long as
  the protocol considers itself still in the connecting phase.

  `->do_it` is the function called to issue the transfer request. What we call
  the DO action internally. If the DO is not enough and things need to be kept
  getting done for the entire DO sequence to complete, `->doing` is then
  usually also provided. Each protocol that needs to do multiple commands or
  similar for do/doing need to implement their own state machines (see SCP,
  SFTP, FTP). Some protocols (only FTP and only due to historical reasons) has
  a separate piece of the DO state called `DO_MORE`.

  `->doing` keeps getting called while issuing the transfer request command(s)

  `->done` gets called when the transfer is complete and DONE. That's after the
  main data has been transferred.

  `->do_more` gets called during the `DO_MORE` state. The FTP protocol uses
  this state when setting up the second connection.

  ->`proto_getsock`
  ->`doing_getsock`
  ->`domore_getsock`
  ->`perform_getsock`
  Functions that return socket information. Which socket(s) to wait for which
  action(s) during the particular multi state.

  ->disconnect is called immediately before the TCP connection is shutdown.

  ->readwrite gets called during transfer to allow the protocol to do extra
  reads/writes

  ->defport is the default report TCP or UDP port this protocol uses

  ->protocol is one or more bits in the `CURLPROTO_*` set. The SSL versions
  have their "base" protocol set and then the SSL variation. Like
  "HTTP|HTTPS".

  ->flags is a bitmask with additional information about the protocol that will
  make it get treated differently by the generic engine:

  - `PROTOPT_SSL` - will make it connect and negotiate SSL

  - `PROTOPT_DUAL` - this protocol uses two connections

  - `PROTOPT_CLOSEACTION` - this protocol has actions to do before closing the
    connection. This flag is no longer used by code, yet still set for a bunch
    of protocol handlers.

  - `PROTOPT_DIRLOCK` - "direction lock". The SSH protocols set this bit to
    limit which "direction" of socket actions that the main engine will
    concern itself with.

  - `PROTOPT_NONETWORK` - a protocol that doesn't use network (read file:)

  - `PROTOPT_NEEDSPWD` - this protocol needs a password and will use a default
    one unless one is provided

  - `PROTOPT_NOURLQUERY` - this protocol can't handle a query part on the URL
    (?foo=bar)

## conncache

  Is a hash table with connections for later re-use. Each `Curl_easy` has a
  pointer to its connection cache. Each multi handle sets up a connection
  cache that all added `Curl_easy`s share by default.

## Curl_share

  The libcurl share API allocates a `Curl_share` struct, exposed to the
  external API as "CURLSH *".

  The idea is that the struct can have a set of its own versions of caches and
  pools and then by providing this struct in the `CURLOPT_SHARE` option, those
  specific `Curl_easy`s will use the caches/pools that this share handle
  holds.

  Then individual `Curl_easy` structs can be made to share specific things
  that they otherwise wouldn't, such as cookies.

  The `Curl_share` struct can currently hold cookies, DNS cache and the SSL
  session cache.

## CookieInfo

  This is the main cookie struct. It holds all known cookies and related
  information. Each `Curl_easy` has its own private CookieInfo even when
  they are added to a multi handle. They can be made to share cookies by using
  the share API.


[1]: https://curl.haxx.se/libcurl/c/curl_easy_setopt.html
[2]: https://curl.haxx.se/libcurl/c/curl_easy_init.html
[3]: https://c-ares.haxx.se/
[4]: https://tools.ietf.org/html/rfc7230 "RFC 7230"
[5]: https://curl.haxx.se/libcurl/c/CURLOPT_ACCEPT_ENCODING.html
[6]: https://curl.haxx.se/docs/manpage.html#--compressed
[7]: https://curl.haxx.se/libcurl/c/curl_multi_socket_action.html
[8]: https://curl.haxx.se/libcurl/c/curl_multi_timeout.html
[9]: https://curl.haxx.se/libcurl/c/curl_multi_setopt.html
[10]: https://curl.haxx.se/libcurl/c/CURLMOPT_TIMERFUNCTION.html
[11]: https://curl.haxx.se/libcurl/c/curl_multi_perform.html
[12]: https://curl.haxx.se/libcurl/c/curl_multi_fdset.html
[13]: https://curl.haxx.se/libcurl/c/curl_multi_add_handle.html
[14]: https://curl.haxx.se/libcurl/c/curl_multi_info_read.html
[15]: https://tools.ietf.org/html/rfc7231#section-3.1.2.2
