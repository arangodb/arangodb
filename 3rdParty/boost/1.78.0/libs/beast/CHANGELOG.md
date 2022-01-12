Version 322:

* Fix typo in `_experimental::test::basic_stream` documentation.

--------------------------------------------------------------------------------

Version 321:

* Remove test framework's dependency on RTTI.
* Fix CVE-2016-9840 in zlib implementation.
* Fix TLS SNI handling in websocket_client_async_ssl example.
* Fix reuse of sliding window in WebSocket permessage_deflate.
* Fix accept error handling in http_server_async example.
* Move library-specific docca configuration to Beast.
* Remove dependency on RTTI in `test::stream`.

--------------------------------------------------------------------------------

Version 320:

* Fix missing includes in `stream_state`.
* Update GitHub Actions CI.

--------------------------------------------------------------------------------

Version 319:

* Update release notes for Boost 1.77.

--------------------------------------------------------------------------------

Version 318:

* Add a Boost-friendly subproject case to CMakeLists.
* Remove use of POSIX-only constant.

--------------------------------------------------------------------------------

Version 317:

* Remove travis CI.
* Fix Drone CI script.
* Add example of reading large response body.

--------------------------------------------------------------------------------

Version 316:

* Disable GHA CI for clang-9.
* Update example root certificates.

--------------------------------------------------------------------------------

Version 315:

* Documentation uses docca jam module.

--------------------------------------------------------------------------------

Version 314:

* WebSocket test is deterministic.
* Add missing compilers to GHA script.

--------------------------------------------------------------------------------

Version 313:

* Fix incorrect websocket test ordering.
* Fix missing check for error code after header is parsed.
* Fix case where inflated content is larger than the out buffer.

--------------------------------------------------------------------------------

Version 312:

* Enable Github Actions CI.

--------------------------------------------------------------------------------

Version 311:

* Fix warning in http-server-fast.
* Parenthesise all uses of min() and max().
* Extend Drone MSVC tests.
* Fix MSVC build error.

--------------------------------------------------------------------------------

Version 310:

* Update Release Notes.

--------------------------------------------------------------------------------

Version 309:

* Extra logic error detection in http parser.
* Move Windows CI to Drone.

--------------------------------------------------------------------------------

Version 308:

* Fix compiler warning in WebSSocket async shutdown.
* Remove Travis CI status.
* Repoint B2 refs to new non-boostorg home.
* Limit async_write instantiations in websocket ops.
* Add Drone CI status.

--------------------------------------------------------------------------------

Version 307:

* Add executor rebind to test::stream.
* Remove floating point arithmetic requirement.
* Add `cxxstd` to json field.

--------------------------------------------------------------------------------

Version 306:

* Revert removal of lowest_layer_type from test stream.

--------------------------------------------------------------------------------

Version 305:

* Fix documentation build.

--------------------------------------------------------------------------------

Version 304:

* Fix C++20 coroutine tests.
* Fix links to Bishop Fox Security Assessment.

--------------------------------------------------------------------------------

Version 303:

* Add Bishop Fox Security Assessment.

--------------------------------------------------------------------------------

Version 302:

* Fix assert when basic_stream used as underlying of ssl::stream with zero-length write.
* Add Sec-* HTTP headers.
* Fix nullptr implicit cast on `fields::set()`.

--------------------------------------------------------------------------------

Version 301:

* Fix Travis CI bug.
* Fix erroneous error when HTTP `body_limit` is `none`.
* Fix unreachable code warning with MSVC.
* Fix logic error in advance_server_flex.
* Fix file open with append_existing flag on posix.
* Websocket SSL `teardown` also tears down underlying TCP.
* Update WebSocket examples to set TLS SNI.
* Add handler tracking locations to websocket.
* Add handler tracking locations to tcp teardown.
* Eliminate spurious uninitialised variable warning in detect_ssl.
* Add handler tracking locations to flat_stream.
* Add handler tracking locations to detect_ssl.
* Add handler tracking locations to icy_stream.
* Add handler tracking locations to basic_stream.
* Add handler tracking locations to http operations.

API Changes:

* Previously, `teardown` and `async_teardown` of a websocket connection over SSL
  would only shut down the SSL layer, leaving the TCP layer connected. Now the 
  SSL teardown will tear down both the SSL and TCP layers, cleanly shutting down 
  the TCP connection and closing the socket.
  Should the client expect the TCP socket to remain connected, users will need to 
  re-engineer their code.

--------------------------------------------------------------------------------

Version 300:

* Fix compile errors under Clang 3.4
* Fix portability bug in websocket server sync example.

--------------------------------------------------------------------------------

Version 299:

* Fix race in http-crawl example.

--------------------------------------------------------------------------------

Version 298:

* Support BOOST_ASIO_NO_TS_EXECUTORS.
* Use strand<> rather than legacy executor io_context::strand.
* Use dispatch/post free functions to be independent of executor concept.
* New name for polymorphic executor. Trait for detecting new executors.
* Handler invoke and allocation hooks are deprecated.

--------------------------------------------------------------------------------

Version 297:

* iless and iequal take part in Heterogeneous Lookup
* Fix `max` compile error
* Deprecate `string_param` (API Change)

API Changes:

* `string_param`, which was previously the argument type when setting field values 
   has been replaced by `string_view`. Because of this, it is no longer possible to 
   set message field values directly as integrals. 

   Users are required to convert numeric arguments to a string type prior to calling 
   `fields::set` et. al.
   
   Beast provides the non-allocating `to_static_string()` function for this purpose.
   
   To set Content-Length field manually, call `message::content_length`.

--------------------------------------------------------------------------------

Version 296:

* Remove buffers_adapter.hpp (API Change)
* Remove zlib error `invalid_code_lenths`(sic) (API Change)
* Remove `core/type_traits.hpp` (API Change)
* Remove `reset` function from `flat_static_buffer` (API Change)
* Remove `mutable_data_type` from Dyanmic Buffers (API Change)
* Remove deprecated lowest_layer from test::stream
* Remove handler_pointer (API Change)
* Remove websocket::role_type (API Change)
* Remove accept_ex and handshake_ex variants (API Change)
* Rename to BOOST_BEAST_ALLOW_DEPRECATED (API Change)

API Changes:

* The file `core/buffers_adapter.hpp` has been removed along with the deprecated
  alias typename `buffers_adapter`. Affected programs should use 
  `core/buffers_adapator.hpp` and the type `buffers_adaptor`.

* The error code enum `invalid_code_lenths` was a synonym of `invalid_code_lengths`.
  Affected programs should be modified to use `invalid_code_lengths`.

* The `core/type_traits.hpp` public header has been removed and along with it
  the type trait `is_completion_handler`. Beast uses the CompletionHandler correctness
  checks provided by Asio. In a c++20 environment, these convert to concept checks.

* The `reset` function has been removed from `flat_static_buffer`. Use the 
  `clear` function instead.

* Code that depends on `mutable_data_type` should be refactored to use
  `mutable_buffers_type`. Classes affected are:
  - `buffers_adaptor`
  - `flat_buffer`
  - `flat_static_buffer`
  - `multi_buffer`
  - `static_buffer`

* handler_ptr has been removed. Users should use net::bind_handler and/or 
bind_front_handler instead.

* websocket::role_type has been removed. Users should use beast::role_type instead.

* The following deprecated functions have been removed:
  - websocket::async_accept_ex
  - websocket::async_handshake_ex
  - websocket::accept_ex
  - websocket::handshake_ex

Programs still using these names should be refactored to use the `decorator` feature and
the remaining handshake and accept functions.

* The macro BOOST_BEAST_NO_DEPRECATED will no longer be noticed by Beast. The only way to
enable deprecated functionality is now the macro BOOST_BEAST_ALLOW_DEPRECATED which is 
undefined by default. That is, all deprecated behaviour is disabled by default.

--------------------------------------------------------------------------------

Version 295:

* Parser body_limit is optional (API Change)
* Fix basic_stream expires_after (API Change)
* Fix FILE namespace qualification

API Changes:

* The signature of basic_parser<>::body_limit(n) has changed. It now accepts an
optional std::uint64_t. The caller may indicate that no body limit is required
by calling body_limit(boost::none). The default limits remain in place in order
to maintain 'safe by default' behaviour.

* basic_stream::expires_after used to take a nanosecond duration type argument. This
required users on systems where the steady_clock::duration_type was less accurate
to explicity duration_cast when calling this function, making code non-portable.
The duration type is now that of the embedded steady_clock.

--------------------------------------------------------------------------------

Version 294:

* Fix FILE namespace qualification
* Fix http read bytes_transferred (API Change)

API Changes:

The following functions did not previously report the number of bytes consumed
by the HTTP parser:

- http::read
- http::read_header
- http::read_some
- http::async_read
- http::async_read_header
- http::async_read_some

As of now, the `bytes_transferred` return value will indicate the number of bytes 
consumed by the parser when parsing an http message.

Actions Required:

- Modify calling code which depends on the value returned from these functions.

--------------------------------------------------------------------------------

Version 293:

* Fix async_connect documentation
* Fix assert in websocket
* No automatic User-Agent in WebSocket

Behaviour Changes:

* No automatic User-Agent in WebSocket
  Beast websocket streams will no longer automatically set the
  User-Agent HTTP header during client handshake. This header is not required
  by the WebSocket standard. If this field is required, user code may set it
  in the decorator

--------------------------------------------------------------------------------

Version 292:

* Fix compile errors on Visual Studio with /std:c++latest
* Fix standalone compilation error with std::string_view 
* OpenSSL 1.0.2 or later is required
* Fix c++20 deprecation warning in span_body

--------------------------------------------------------------------------------

Version 291:

* Test websocket with use_awaitable
* Test http write with use_awaitable
* Test http read with use_awaitable
* Fix use buffered_read_stream with use_awaitable
* Implement is_completion_token_for trait
* Test use_awaitable with basic_stream
* Fix async_detect_ssl with use_awaitable
* Add clang coroutines-ts to circleci config

--------------------------------------------------------------------------------

Version 290:

* Travis build host now bionic
* Update Release Notes

Version 289:

* Fix Host header in websocket examples

--------------------------------------------------------------------------------

Version 288:

* Fix Content-Length parsing
* Update credits

--------------------------------------------------------------------------------

Version 287:

* Remove CODE_OF_CONDUCT.md
* Refactor websocket read
* Correct buffer_bytes documentation
* Fix examples to dispatch to strand
* Ensure basic_stream::close will not throw

--------------------------------------------------------------------------------

Version 286:

* Refactor multi_buffer
* Refactor buffers_adapter
* Refactor static_buffer
* Refactor flat_buffer
* Refactor flat_static_buffer
* Fix missing include in sha1.hpp
* Fix ostream warning
* Field digest is endian-independent
* update broken links in README
* Fix ostream flush

API Changes:

* Nested const and mutable buffer types for all
  Beast dynamic buffers are refactored. Affected types:
  - buffers_adapter
  - flat_buffer
  - flat_static_buffer
  - multi_buffer
  - static_buffer
  
* Nested mutable_data_type in Beast dynamic buffers is deprecated:

Changes Required:

* Use nested mutable_buffers_type instead of mutable_data_type,
  or define BOOST_BEAST_ALLOW_DEPRECATED

--------------------------------------------------------------------------------

Version 285:

* Translate some win32 errors to net error codes
* enable circleci integration
* flat_buffer shrink_to_fit is noexcept
* moved-from dynamic buffers do not clear if different allocator
* fix erase field

--------------------------------------------------------------------------------

Version 284:

* fix compilation macro documentation
* examples use strands correctly
* update root certificates in examples
* clarify end-of-file behaviour in File::read docs
* file_body returns short_read on eof during read
* fix bug in win32 file_body

--------------------------------------------------------------------------------

Version 283:

* ostream_buffer satisfies preconditions of DynamicBuffer_v1::commit
* Add accessor function to File member of basic_file_body

Version 282:

* Use superproject docca
* Fix release build of docs
* file_win32 supports UTF-8 paths
* file_stdio supports unicode paths

--------------------------------------------------------------------------------

Version 281:

* Travis builds docs
* Fix echo-op test
* file_win32 bodies respect http::serializer::split

--------------------------------------------------------------------------------

Version 280:

* Fix non-msvc cmake
* Use docca master branch

--------------------------------------------------------------------------------

Version 279:

* Use regular throw in test
* Fix pragma warning

--------------------------------------------------------------------------------

Version 278:

* Use regular throw in test

--------------------------------------------------------------------------------

Version 277:

* Update release notes

--------------------------------------------------------------------------------

Version 276:

* https_get example sends the Host header
* Fix async_close error code when async_read times out
* Refactor zlib tests and fix enum typo

--------------------------------------------------------------------------------

Version 275:

* Async init-fns use the executor's default token
* Add basic_stream::rebind_executor
* Use automatically deduced return types for all async operations
* Support Concepts for completion token params

--------------------------------------------------------------------------------

Version 274:

* Fix leftovers in basic_parser corner case

--------------------------------------------------------------------------------

Version 273:

* Squelch spurious websocket timer assert
* Use the executor type in basic_stream timer

--------------------------------------------------------------------------------

Version 272:

* Add BEAST_THROWS
* Add zlib tests and fixes

--------------------------------------------------------------------------------

Version 271:

* Add HTTP async client with system_executor example
* Add WebSocket async client with system_executor example
* Fix data race in HTTP server examples
* Fix data race in WebSocket examples

--------------------------------------------------------------------------------

Version 270:

* Silence unused variables
* Fix typo

--------------------------------------------------------------------------------

Version 269:

* Fix /permissive- missing include
* Add test

--------------------------------------------------------------------------------

Version 268:

* root_certificates.hpp is not for production

--------------------------------------------------------------------------------

Version 267:

* Add package for Travis config
* Fix signed/unsigned mismatch in file_stdio::seek
* basic_stream dtor cannot throw
* cmake: check policy first
* Add default dtors to satisfy -Wnon-virtual-dtor
* Multiple I/O of the same type is not supported

--------------------------------------------------------------------------------

Version 266:

* Fix some missing deduced return types in the docs

--------------------------------------------------------------------------------

Version 265:

* Fix outgoing websocket message compression

--------------------------------------------------------------------------------

Version 264:

* Handle overflow in max size calculation in `basic_dynamic_body`
* Fix unused variable warnings in tests
* Fix missing initializer warning in `basic_fields`
* Remove unused functions in `impl/static_string.hpp`
* Fix unused variable warning in `multi_buffer`
* Fix header-only compilation errors in some configurations
* Workaround for miscompilation in MSVC 14.2

--------------------------------------------------------------------------------

Version 263:

* Update documentation

--------------------------------------------------------------------------------

Version 262:

* Fix deallocate in multi_buffer

--------------------------------------------------------------------------------

Version 261:

* Deduplicate `websocket::read_size_hint` definition
* Fix UB in websocket read tests
* Remove redundant includes in websocket
* Simplify websocket::detail::prng
* Don't over-allocate in http::basic_fields
* Fix multi_buffer allocation alignment
* Tidy up buffers_range

--------------------------------------------------------------------------------

Version 260:

* More split compilation in rfc7230.hpp
* Qualify calls to `beast::iequals` in basic_parser.ipp
* More split compilation in websocket/detail/mask.hpp
* Cleanup transitive includes in beast/core/detail/type_traits.hpp
* Simplify generation of sec-websocket-key
* Move detail::base64 helpers to tests
* Remove redundant includes in core

--------------------------------------------------------------------------------

Version 259:

* Reduce the number of instantiations of filter_token_list
* Remove the use of `static_string` from `http::fields`
* Add gcc-9 to AzP CI test matrix
* Enable split compilation in http::basic_fields
* Remove redundant instation of `static_string` in websocket
* Remove redundant use of `asio::coroutine` in `flat_stream`
* Remove unused includes from `test::stream`
* Move `char_buffer` into a separate file
* Fix coverage collection in AzP CI
* Improve performance of `http::string_to_verb`
* Replace uses of `net::coroutine` with `asio::coroutine`
* Replace uses of `net::spawn` with `asio::spawn`
* Use `beast::read_size` in `detail::read`

--------------------------------------------------------------------------------

Version 258:

* Fix separate compilation in CI
* Fix clang inititalization warning in websocket
* Remove redundant use of `yield_to` in parser tests
* Add VS 2019 AzP CI matrix item
* Clean up typo in chat websocket javascript client

--------------------------------------------------------------------------------

Version 257:

* Add b2 features for compile-time options used in testing
* Remove redundant dependencies in http/server/fast example
* Remove experimental/unit_test/thread.hpp
* Use `if` statement in basic_stream::transfer_op
* Fix flat_buffer copy members

--------------------------------------------------------------------------------

Version 256:

* Remove uses of the deprecated `buffers` function
* Remove uses of deprecated methods in websocket tests
* Remove redundant use of `static_string`
* Remove redundant template in service_base
* Expand CI matrix using Azure Pipelines
* Make chat websocket javascript client more user friendly
* `allocator_traits::construct` is used for user-defined types
* Add 1-element specialization for `buffers_cat`
* Fix `buffers_cat` iterator tests
* Don't pessimize-move
* Use steady_timer type
* Preserve operation_aborted on partial message

--------------------------------------------------------------------------------

Version 255:

* Add idle ping suspend test
* Fix moved-from executor in idle ping timeout

--------------------------------------------------------------------------------

Version 254:

* Fix data race in test::stream::connect
* Fix UB in websocket close tests
* Fix uninitalized memory use in deflate_stream
* Fix gcc-8 warning in websocket::stream

--------------------------------------------------------------------------------

Version 253:

* Fix async_detect_ssl handler type
* member get_executor const-correctness
* Fix min/max on MSVC
* Relax requirements for vector_body

--------------------------------------------------------------------------------

Version 252:

* More std::string_view fixes
* CI copies to libs/beast

--------------------------------------------------------------------------------

Version 251:

* Clean up CI scripts
* detect_ssl uses bool
* launder pointers
* Fix compilation on MSVC with std::string_view
* Replace static_string in parser

--------------------------------------------------------------------------------

Version 250:

* Use SaxonHE in reference generation
* Cleanup endianness conversions
* Set parser status and flags even if body_limit_ has been reached

--------------------------------------------------------------------------------

Version 249:

* Move friend function template definition

--------------------------------------------------------------------------------

Version 248:

* Don't use a moved-from handler

--------------------------------------------------------------------------------

Version 247:

* Fix async_base immediate completion

--------------------------------------------------------------------------------

Version 246:

* decorator ctor is explicit

--------------------------------------------------------------------------------

Version 245:

* decorator constructor is constrained

--------------------------------------------------------------------------------

Version 244:

* Tidy up declval in some traits
* Fix websocket keep-alive ping expiration

--------------------------------------------------------------------------------

Version 243:

* Fix some typos
* Tidy up file_stdio for VS2015
* Fix http::message constructor constraint

--------------------------------------------------------------------------------

Version 242:

* test::stream has deprecated lowest_layer for ssl
* MSVC uses ::fopen_s
* Fix http::message constructor constraint
* Check defined(BOOST_MSVC)

--------------------------------------------------------------------------------

Version 241:

* Tidy up a doc code snippet
* basic_parser::content_length is stable (API Change)

--------------------------------------------------------------------------------

Version 240:

* Fix ssl_stream teardown

--------------------------------------------------------------------------------

Version 239:

* More split compilation in HTTP

--------------------------------------------------------------------------------

Version 238:

* Refactor Jamfiles to work with release layout

--------------------------------------------------------------------------------

Version 237:

* cmake: Use static libs to speed up builds

--------------------------------------------------------------------------------

Version 236:

* root_certificates.hpp: brought in the server certificate

--------------------------------------------------------------------------------

Version 235:

* Fix self-assignment warning in buffer test
* Jamfile cleanup

--------------------------------------------------------------------------------

Version 234:

* Don't link to OpenSSL needlessly (bjam)
* HTTPS URLs in README.md

--------------------------------------------------------------------------------

Version 233:

* Check __ANDROID__ instead
* Use secure TLS/SSL versions

--------------------------------------------------------------------------------

Version 232:

* Fix close_socket for net::basic_socket changes
* Fix file_win32_write_op async initiation
* Fix basic_stream lowest_layer for ssl

--------------------------------------------------------------------------------

Version 231:

* Doc section names are stable
* Add missing include
* Constrain to_static_string to integers

--------------------------------------------------------------------------------

Version 230:

* Don't use dynamic_buffer_ref
* Remove dynamic_buffer_ref
* Fix completion handler invocation signatures

--------------------------------------------------------------------------------

Version 229:

* Rename to buffer_bytes
* Tidy up examples
* detect_ssl returns a bool
* Fix stable_async_base example

API Changes:

* handler_ptr is deprecated

Actions Required:

* Replace use of `handler_ptr` with `stable_async_base`
  and `allocate_stable`.

--------------------------------------------------------------------------------

Version 228:

* Fix UB in decorator:
* Sync up convenience headers
* The Fields concept is deprecated (API Change)
* Fix includes.xsl for newer doxygen
* Tidy up quick reference
* SSL teardowns are in an associated namespace

--------------------------------------------------------------------------------

Version 227:

* Fix decorator for certain sizes

--------------------------------------------------------------------------------

Version 226:

* Support -fno-exceptions
* make_strand is in net::
* Fix HTTP parser static string calculation
* Move parser definitions to .ipp
* Appveyor uses msvc-14.0

--------------------------------------------------------------------------------

Version 225:

* Tidy up an unused function
* Fix wsload jamfile
* Examples use flat_buffer
* Remove session_alloc (API Change)

Actions Required:

* Don't use session_alloc

--------------------------------------------------------------------------------

Version 224:

* Remove extraneous error check in advanced-server-flex
* Advanced servers use HTTP parser interfaces for reading
* Reusing an HTTP parser returns an error

--------------------------------------------------------------------------------

Version 223:

* Add test::stream::service
* Add websocket service
* Pausation abandoning test
* Destroy abandoned websocket ops on shutdown

--------------------------------------------------------------------------------

Version 222:

* stream_base::timeout::suggested is a nested function

--------------------------------------------------------------------------------

Version 221:

* Rename to async_base, stable_async_base
* role_type is in boost/beast/core/role.hpp (API Change)
* Cleanup in test::stream internals
* Pass references as pointers to async_initiate

Actions Required:

* Include <boost/beast/core/role.hpp> or
  define BOOST_BEAST_ALLOW_DEPRECATED=1

--------------------------------------------------------------------------------

Version 220:

* Documentation and release notes

--------------------------------------------------------------------------------

Version 219:

* More split definitions in test::stream
* Visual Studio 2017 minimum requirement for Windows
* Better treatment of SSL short reads
* ssl_stream is a public interface
* basic_parser is abstract, not CRTP (API Change)
* OpenSSL is required for tests and examples

--------------------------------------------------------------------------------

Version 218:

* detect_ssl, async_detect_ssl are public interfaces
* Add OpenSSL installation/setup instructions
* Enable split Beast compilation for tests

--------------------------------------------------------------------------------

Version 217:

* websocket idle pings
* RatePolicy documentation
* Pass strand to async_accept
* Fix file_body_win32
* Use async_initiate
* Check BOOST_NO_CXX11_THREAD_LOCAL
* Fast prng is pcg

--------------------------------------------------------------------------------

Version 216:

* Refactor websocket::stream operations
* Add websocket::stream timeouts
* Use suggested timeouts in Websocket examples
* Add make_strand
* Add RatePolicy to basic_stream
* Use async_initiate in basic_stream
* basic_stream connects are members
* Beast supports latest Asio changes (API Change)
* WebSocket Decorator is a socket option (API Change)
* Overloads of the following functions which accept a Decorator
  are deprecated:
  - accept, accept_ex
  - handshake, handshake_ex
  - async_accept, async_accept_ex
  - async_handshake, async_handshake_ex

Actions Required:

* Code which passes decorator to any `websocket::stream` member
  function should call `stream::set_option` instead with a newly
  constructed `stream_base::decorator` object containing the
  decorator. Alternatively, the macro `BOOST_BEAST_ALLOW_DEPRECATED`
  may be defined to 1.

* Fix compilation errors required by Asio changes

--------------------------------------------------------------------------------

Version 215:

* basic_stream uses boost::shared_ptr
* Remove bind_back_handler
* bind_front_handler works with member functions
* Examples use bind_front_handler
* Add experimental test/handler.hpp
* Rename to async_op_base::invoke_now
* Add async_op_base::invoke
* Remove CppCon2018 example
* Examples use ssl_stream

--------------------------------------------------------------------------------

Version 214:

* Handler binders use the associated allocator
* Add detail::bind_continuation
* Rewrite the echo-op example

--------------------------------------------------------------------------------

Version 213:

* Fix posix_file::close handling of EINTR
* basic_stream subsumes stranded_stream:
* Use timeouts in HTTP server examples
* Use timeouts in HTTP client examples
* Use tcp_stream in WebSocket client examples
* Use tcp_stream in WebSocket server examples
* Use tcp_stream, HTTP timeouts in advanced servers

--------------------------------------------------------------------------------

Version 212:

* dynamic_buffer_ref tests and tidy
* flat_stream tests and tidy
* stranded_socket tests and tidy
* buffers_front tests
* Improved websocket stream documentation

--------------------------------------------------------------------------------

Version 211:

* close_socket is in stream_traits.hpp
* Improvements to test::stream
* Add stranded_stream
* Add flat_stream
* flat_buffer::clear preserves capacity
* multi_buffer::clear preserves capacity
* Fixes to rfc7230

--------------------------------------------------------------------------------

Version 210:

* Tidy up read implementation
* Fix stable_async_op_base javadoc
* Better handling of stream timeouts
* Add stream_traits.hpp
* Add executor_type trait
* Fix hexadecimal string conversion table
* is_completion_handler, type_traits.hpp are deprecated
* Fixes to test::stream::async_read

API Changes:

* Stream traits are now in stream_traits.hpp
* `is_file` is now in file_base.hpp
* is_completion_handler is deprecated

Actions Required:

* Include stream_traits.hpp as needed
* Include file_base.hpp as needed
* Use std::is_invocable instead of is_completion_handler

--------------------------------------------------------------------------------

Version 209:

* Faster http::string_to_field
* async_echo supports move-only handlers
* test::stream maintains a handler work guard
* Qualify buffer_copy, don't qualify buffer_size
* Add dynamic_buffer_ref
* Refactor quickref.xml
* Add buffer_size

--------------------------------------------------------------------------------

Version 208:

* Add get_lowest_layer free function
* Add lowest_layer_type metafunction
* Add close_socket, beast_close_socket customization
* Doc work

--------------------------------------------------------------------------------

Version 207

* Send from the strand
* Pass the correct handler in basic_timeout_stream

API Changes:

* lowest_layer is removed

Actions Required:

* Remove lowest_layer and lowest_layer_type from user-defined streams.
* Use the get_lowest_layer free function and the lowest_layer_type trait
  as needed.

--------------------------------------------------------------------------------

Version 206

* Clear error codes idiomatically
* websocket stream uses shared_ptr<impl_type>
* Add websocket-chat-multi example

--------------------------------------------------------------------------------

Version 205

* Doc work
* Add detail/soft_mutex.hpp
* Add detail/prng.hpp

--------------------------------------------------------------------------------

Version 204

* Add basic_timeout_stream
* Unit test macros use the global suite
* Doc work

--------------------------------------------------------------------------------

Version 203

* Update networking refresher doc
* Include error code in call to set_option
* saved_handler is a public interface
* Use new saved_handler in websocket
* session_alloc is thread-safe
* examples use flat_buffer
* parse_until is not static
* Boost.System is header-only

--------------------------------------------------------------------------------

Version 202

* Use cxxstd instead of cxxflags
* Update coverage badge images
* Tidy up basic_stream_socket docs
* Refactor async_op_base
* Use async_op_base
* async_op_base is a public interface
* Add tests for bind_back_handler
* Add tests for async_op_base

--------------------------------------------------------------------------------

Version 201

* Decay bound arguments in handler wrapper parameters
* Add bind_back_handler
* Tidy up default-constructed iterators
* Add core errors and conditions
* New basic_stream_socket

--------------------------------------------------------------------------------

Version 200

* Don't include OpenSSL for core snippets
* Tidy up msvc-14 workaround in multi_buffer
* buffers_cat fixes and coverage
* Refactor buffers_adaptor
* Refactor buffers_range
* Fix and refactor buffers_cat
* Refactor buffers_prefix
* Add const and mutable buffer sequence traits
* Add buffers_iterator_type trait
* Use new buffer traits, remove old unused traits
* Optimize for size on buffers_cat preconditions
* Refactor buffers_suffix
* Tidy up flat_buffer tests
* Fix ostream prepare calculation for low limits
* Tidy up flat_static_buffer tests
* Add more tests for dynamic buffers
* Tidy up multi_buffer
* Refactor ostream
* Refactor static_buffer
* HTTP tidying
* Adjust static_asio lib options in Jamfile
* Add type_traits tests
* Add buffers_range_ref (replaces reference_wrapper parameter)

API Changes:

* buffers_adaptor replaces buffers_adapter (rename)
* make_printable replaces buffers (rename)
* Remove file_mode::append_new

Actions Required:

* Replace buffers_adapter.hpp with buffers_adaptor.hpp, and
  replace buffers_adapter with buffers_adaptor. Or, define
  BOOST_BEAST_ALLOW_DEPRECATED

* Replace call sites to use make_printable instead of buffers,
  and also include make_printable.hpp instead of ostream.hpp.

* Replace file_mode::append_new with file_mode::append
  or file_mode::append_existing instead of file_mode::append_new

--------------------------------------------------------------------------------

Version 199:

* Workarounds for msvc-14
* Fix Appveyor badge links

--------------------------------------------------------------------------------

Version 198:

* flat_buffer improvements
* multi_buffer improvements
* static_buffer improvements
* flat_static_buffer_improvements
* saved_handler maintains a work_guard (websocket)
* Add buffer_traits.hpp, buffers_type
* Tidy up experimental files
* Tidy up core files
* Fix bind_handler, bind_front_handler
* Improved handler bind wrapper tests

API Changes:

* Files return errc::bad_file_descriptor
* flat_static_buffer::reset is deprecated

Actions Required:

* Callers checking for errc::invalid_argument from calls to
  file APIs should check for errc::bad_file_descriptor instead.

* Replace calls to flat_static_buffer::reset with
  flat_static_buffer::clear

--------------------------------------------------------------------------------

Version 197:

* Improvements to echo-op example
* Crawler example clears the response before each read
* Use a struct instead of a pair in flat_stream (experimental)

API Changes:

* Refactor HTTP operations

Actions Required:

* Callers depending on the return value of http::read or
  http::async_read overloads should adjust the usage of
  the returned value as needed.

--------------------------------------------------------------------------------

Version 196:

* Tidy up calls to placement new
* Remove unused type_traits
* Simplify handler_ptr

--------------------------------------------------------------------------------

Version 195:

* net is a namespace alias for boost::asio
* Simplify multi_buffer and static_buffer sequences
* Documentation work

--------------------------------------------------------------------------------

Version 194:

* http::async_read returns the right byte count on error
* Add net namespace alias
* Don't use-after-free in test
* Tidy up ssl_stream (experimental)
* Dynamic buffer improvements
* Saved handlers are dispatched

--------------------------------------------------------------------------------

Version 193:

* Update ssl_stream signatures for networking changes
* Fix test::stream async_result transformation
* Tidy up test::stream
* Enable explicit instantiation of websocket::stream

--------------------------------------------------------------------------------

Version 192:

* Use mp11::integer_sequence
* Tidy up warnings and deprecated usage
* http::message is not-a boost::empty_value
* Fix link in docs
* Fixes to timeout services (experimental)

--------------------------------------------------------------------------------

Version 191:

* Add bind_front_handler
* Use bind_front_handler
* Simplify some type traits
* Use lean_tuple in buffers_cat
* Use lean_tuple in bind_handler, bind_front_handler
* Use mp11 in detail::variant
* Fix buffers_cat uninitialized warning
* Fix static_string uninitialized warning
* Fix warning in is_ssl_handshake

--------------------------------------------------------------------------------

Version 190:

* Add missing includes to convenience headers
* Unit test framework is experimental
* Add buffers_range
* Rename experimental directory
* Improve compilation of tests for continuous integration
* Fix visibility warnings in test

--------------------------------------------------------------------------------

Version 189-hf1:

* Fix broken doc link
* example/cppcon2018 only requires C++11

--------------------------------------------------------------------------------

Version 189:

* Add CppCon2018 chat server example and video

--------------------------------------------------------------------------------

Version 188:

* Remove extraneous strand from example
* Add missing include in http/read.ipp
* Test for gcc warning bug
* Fix a spurious gcc warning

--------------------------------------------------------------------------------

Version 187:

* Add experimental timeout_socket
* Fix warning in file tests
* Fix uninitialized comparison in buffers iterator
* Partial support for BOOST_NO_EXCEPTIONS
* Fix a spurious gcc warning
* Test for gcc warning bug
* Add missing include
* Remove extraneous strand from example

--------------------------------------------------------------------------------

Version 186:

* basic_fields uses intrusive base hooks
* Fix parsing of out-of-bounds hex values

--------------------------------------------------------------------------------

Version 185:

* Remove extraneous function
* Fix some typos
* Add BOOST_BEAST_USE_STD_STRING_VIEW
* Fix timer on websocket upgrade in examples

--------------------------------------------------------------------------------

Version 183:

* Fix a rare case of failed UTF8 validation
* Verify certificates in client examples
* Use boost::empty_value
* Workaround for http-server-fast and libstdc++

--------------------------------------------------------------------------------

Version 182:

* Silence ubsan false positive

--------------------------------------------------------------------------------

Version 181:

* Fix parse_dec algorithm
* Add parse_dec tests

--------------------------------------------------------------------------------

Version 180:

* Fix http_server_stackless_ssl.cpp example

--------------------------------------------------------------------------------

Version 179:

* Use the exchange() idiom in move constructors
* Most members of std::allocate are deprecated in C++17
* Remove some unused variables

--------------------------------------------------------------------------------

Version 178:

* Use static_cast instead

--------------------------------------------------------------------------------

Version 177:

* Add test for issue #1188
* Set /permissive-
* Check error in example set_option

--------------------------------------------------------------------------------

Version 176:

* Tidy up Quick Reference
* Fix array end calculation in utf8 assertion
* WebSocket masks use secure PRNG by default

--------------------------------------------------------------------------------

Version 175:

* Fix initialization warning

--------------------------------------------------------------------------------

Version 174:

* Fix Fields, FieldsWriter concept docs
* Fix BodyReader constructor requirements doc

--------------------------------------------------------------------------------

Version 173:

* Remove Autobahn testsuite doc note
* Fix buffers_adapter iterator value type
* Fix buffers_adapter max_size
* Fix buffers_prefix iterator decrement
* buffers_adapter improvements
* Add icy_stream Shoutcast stream filter

--------------------------------------------------------------------------------

Version 172:

* Tidy up websocket stream javadocs
* Fix move-only arguments in bind_handler
* Fix http::parser constructor javadoc
* Tidy up test::stream javadocs
* Tidy up composed operation doc

--------------------------------------------------------------------------------

Version 171:

* Add handler_ptr::has_value
* Remove spurious assert
* Fix unused variable warning

--------------------------------------------------------------------------------

Version 170:

* Add flat_stream to experimental
* Add ssl_stream to experimental
* Add test::error to experimental
* Add test::fail_count to experimental
* Add test::stream to experimental
* Use a shared string for example HTTP server doc roots
* Remove deprecated serializer::reader_impl()
* Remove deprecated Body reader and writer ctor signatures
* Add is_mutable_body_writer metafunction
* Add const and non-const overloads for message based http writes
* Use the root certificate which matches the fingerprint

--------------------------------------------------------------------------------

Version 169:

* Use buffers_to_string in tests
* Use boost::void_t
* Refactor HTTP write_op implementation
* Use fully qualified namespace in BOOST_BEAST_HANDLER_INIT
* New flat_stream example class
* Use flat_stream in ssl_stream example code

--------------------------------------------------------------------------------

Version 168:

* Use executor_work_guard in composed operations
* Revert verb.ipp change which caused spurious warnings
* Fix race in advanced server examples

--------------------------------------------------------------------------------

Version 167:

* Revert: Tidy up calls to post()

--------------------------------------------------------------------------------

Version 166:

* Use boost::is_convertible as a workaround

--------------------------------------------------------------------------------

Version 165:

* Fix BOOST_NO_CXX11_ALLOCATOR check

--------------------------------------------------------------------------------

Version 164:

* Fix masking on continuation frames
* Add Access-Control-Expose-Headers field constant

--------------------------------------------------------------------------------

Version 163:

* Tidy up calls to post()
* Fix narrowing warnings

--------------------------------------------------------------------------------

Version 162:

* Add asio_handler_invoke overloads for stream algorithms
* Improve websocket::stream::control_callback javadoc

--------------------------------------------------------------------------------

Version 161:

* Don't copy the handler in write_some_op
* Add move-only handler tests
* Fix handler parameter javadocs

--------------------------------------------------------------------------------

Version 160:

* Examples clear the HTTP message before reading

--------------------------------------------------------------------------------

Version 159:

* Fix typo in release notes
* Safe treatment of zero-length string arguments in basic_fields
* Some basic_fields operations now give the strong exception guarantee

--------------------------------------------------------------------------------

Version 158:

* Tidy up end_of_stream javadoc
* Tidy up websocket docs
* Examples set reuse_address(true)
* Advanced servers support clean shutdown via SIGINT or SIGTERM
* DynamicBuffer input areas are not mutable
* Tidy up some documentation

API Changes:

* get_lowest_layer is a type alias

Actions required:

* Replace instances of `typename get_lowest_layer<T>::type`
  with `get_lowest_layer<T>`.

--------------------------------------------------------------------------------

Version 157:

* Fix teardown for TIME_WAIT
* Fix big-endian websocket masking

--------------------------------------------------------------------------------

Version 156:

* Don't use typeid
* Add release notes to documentation
* Fix stale link for void-or-deduced

--------------------------------------------------------------------------------

Version 155:

* Fix memory leak in advanced server examples
* Fix soft-mutex assert in websocket stream
* Fix fallthrough warnings
* Tidy up bind_handler doc

--------------------------------------------------------------------------------

Version 154:

* Type check completion handlers
* bind_handler doc update
* bind_handler works with boost placeholders

--------------------------------------------------------------------------------

Version 153:

* Remove BOOST_VERSION checks
* Use make_error_code for setting an error_code from errc
* Use boost::winapi::GetLastError() consistently
* Update README.md for branches
* Avoid string_view::clear
* Fix iterator version of basic_fields::erase
* Fix use-after-move in example request handlers
* Add Bishop Fox interview media

--------------------------------------------------------------------------------

Version 152:

* Refactor detect_ssl_op
* Disable gdb on Travis for Meltdown

WebSocket:

* Redistribute the read tests in the translation units
* Refactor error headers
* Add WebSocket error conditions

API Changes:

* Refactor WebSocket errors (API Change):

Actions Required:

* Code which explicitly compares error_code values against the
  constant `websocket::error::handshake_failed` should compare
  against `websocket::condition::handshake_failed` instead.

* Code which explicitly compares error_code values against the
  constant `websocket::error::failed` should compare
  against `websocket::condition::protocol_violation` instead.

--------------------------------------------------------------------------------

Version 151:

* Sanitizer failures are errors
* Depend on container_hash
* Fix high-ASCII in source file

WebSocket:

* Control callback is invoked on the execution context
* Add stream_fwd.hpp
* Remove unnecessary include

API Changes:

* http::parser is not MoveConstructible
* permessage-deflate is a compile-time feature

--------------------------------------------------------------------------------

Version 150:

* handler_ptr tests
* Documentation

API Changes:

* serializer::reader_impl is deprecated

Actions Required:

* Call serializer::writer_impl instead of reader_impl

--------------------------------------------------------------------------------

Version 149:

* built-in r-value return values can't be assigned
* Tidy up ssl_stream special members
* Fix CMakeLists.txt variable
* Protect calls from macros
* pausation always allocates
* Don't copy completion handlers
* handler_ptr is move-only
* Fix Travis memory utilization

API Changes:

* handler_ptr gives the strong exception guarantee

Actions Required:

* Change the constructor signature for state objects
  used with handler_ptr to receive a const reference to
  the handler.

--------------------------------------------------------------------------------

Version 148:

* Install codecov on codecov CI targets only
* Update reports for hybrid assessment
* Handle invalid deflate frames

--------------------------------------------------------------------------------

Version 147:

* Don't use boost::string_ref
* Use iterator wrapper in detail::buffers_range

HTTP:

* Tidy up basic_fields exception specifiers

WebSocket:

* control callback is copied or moved
* Send idle pings in advanced servers

--------------------------------------------------------------------------------

Version 146:

* Fix some typos
* Faster ascii_tolower
* Documentation tidying
* Fix typo in examples documentation
* Add detail::aligned_union and tidy up
* Use variant in buffers_cat_view

API Changes:

* Remove unintended public members of handler_ptr

--------------------------------------------------------------------------------

Version 145:

* Rename some detail functions
* Refactor basic_fields allocator internals
* Refactor HTTP async read composed operations
* null_buffers is deprecated
* Version 124 works with Boost 1.65.1 and earlier
* basic_fields does not support fancy pointers

--------------------------------------------------------------------------------

Version 144-hf1:

* Update reports for hybrid assessment
* Handle invalid deflate frames
* Install codecov on codecov CI targets only

--------------------------------------------------------------------------------

Version 144:

* Fix websocket permessage-deflate negotiation

--------------------------------------------------------------------------------

Version 143:

* Fix autobahn report link

--------------------------------------------------------------------------------

Version 142:

* Warn about upcoming API changes to certain concepts
* Fix name typo in http write test

--------------------------------------------------------------------------------

Version 141:

* Tidy up some documentation

--------------------------------------------------------------------------------

Version 140:

* Fix some integer warnings in 64-bit builds
* Fix utf8_checker test failures
* Fix signature for async_read_some, and tests
* Tidy up basic_parser
* Some basic_fields special members are protected

--------------------------------------------------------------------------------

Version 139:

* Revisit boost library min/max guidance

--------------------------------------------------------------------------------

Version 138:

* Tidy up some documentation

--------------------------------------------------------------------------------

Version 137:

* ConstBufferSequence mandates pointer equivalence
* Add FieldsWriter constructor requirement
* Tidy up some documented constructor syntax

--------------------------------------------------------------------------------

Version 136:

* Tidy up message doc image

--------------------------------------------------------------------------------

Version 135:

* Fix typo in example server help text

--------------------------------------------------------------------------------

Version 134:

* Add static_buffer_base default constructor definition

--------------------------------------------------------------------------------

Version 133:

* Remove const&& overload of message::body

--------------------------------------------------------------------------------

Version 132:

* Tidy up project folders in CMakeLists.txt
* Rename Cmake variables for clarity
* Add ref-qualified overloads for message::body
* Tidy up FieldsReader doc

API Changes:

* Fields::writer replaces Fields::reader
* BodyReader and BodyWriter names are swapped

Actions Required:

* Rename reader to writer for user defined Fields
* Swap the reader and writer names for user defined Body types
* Swap use of is_body_reader and is_body_writer

--------------------------------------------------------------------------------

Version 131:

* basic_fields returns const values
* Set SNI hostname in example SSL clients

--------------------------------------------------------------------------------

Version 130:

* Tidy up fallthrough warning
* Remove cxx11_sfinae_expr build requirement from tests

--------------------------------------------------------------------------------

Version 129:

* Add autobahn test report to doc
* Documentation tidying
* Fix prepare_payload: chunked is HTTP/1.1

--------------------------------------------------------------------------------

Version 128:

* Update doc links
* Add explicit-failures-markup.xml

HTTP:

* Add message::need_eof
* Use message::need_eof in example servers
* Use synchronous writes in chunk output example

WebSocket:

* Fix utf8 validation for autobahn
* Temporarily disable utf8 validation tests
* Tidy up fast websocket server host names

API Changes:

* Remove serializer::keep_alive
* Remove serializer::chunked
* Add has_content_length_impl to Fields
* Add message::has_content_length
* Rename some basic_parser observers

Actions Required:

* Call message::keep_alive instead of serializer::keep_alive
* Call serializer::get::chunked instead of serializer::chunked
* Implement has_content_length_impl for user-defined Fields
* Remove the "is_" prefix from call sites invoking certain basic_parser members

--------------------------------------------------------------------------------

Version 127:

* Add BOOST_BEAST_NO_POSIX_FADVISE
* Version command line option for HTTP client examples
* More Jamfile compiler requirements for tests

--------------------------------------------------------------------------------

Version 126:

* Add CppCon2017 presentation link
* Update README.md
* Update stream write documentation for end of stream changes
* Tidy up unused variable warnings
* Don't return end_of_stream on win32 file body writes
* Fix doc typo
* Fix shadowing in session_alloc
* Fix executor type compilation
* Add Travis tests with the default compilers
* Update Boost.WinAPI usage to the new location and namespace.
* Fix buffered_read_stream async_read_some signature

--------------------------------------------------------------------------------

Version 125:

API Changes:

* Update for Net-TS Asio

Actions Required:

* Use BOOST_ASIO_HANDLER_TYPE instead of handler_type
* Use BOOST_ASIO_INITFN_RESULT_TYPE instead of async_result
* Use boost::asio::async_completion
* Use boost::asio::is_dynamic_buffer
* Use boost::asio::is_const_buffer_sequence
* Use boost::asio::is_mutable_buffer_sequence
* boost::asio::associated_allocator_t replaces handler_alloc

--------------------------------------------------------------------------------

Version 124:

* Fix for a test matrix compiler
* Fix basic_fields javadoc

API Changes:

* http write returns success on connection close

Actions Required:

* Add code to servers to close the connection after successfully
  writing a message where `message::keep_alive()` would return `false`.

--------------------------------------------------------------------------------

Version 123:

* Use unit-test subtree
* Fix spurious race in websocket close test
* Check compiler feature in Jamfile
* Clear previous message fields in parser

--------------------------------------------------------------------------------

Version 122:

* Add test for issue 807
* assert on empty buffer in websocket read
* Fix zlib symbol conflicts
* CMake 3.5.1 is required
* config.hpp is not a public header
* Add missing case in error test

--------------------------------------------------------------------------------

Version 121:

* Add test for issue 802
* Fix enum and non-enum in conditional exp. warning

--------------------------------------------------------------------------------

Version 120:

* Fix spurious strand_ in advanced_server_flex
* OpenSSL targets are optional (CMake)

--------------------------------------------------------------------------------

Version 119:

* Tidy up doc badge links

--------------------------------------------------------------------------------

Version 118:

* file_win32 opens for read-only in shared mode
* Remove unused strands in server examples
* Update build instructions
* Doc root is at index.html

HTTP:

* Fix writing header into std::ostream

--------------------------------------------------------------------------------

Version 117:

* Add buffers_to_string

API Changes:

* buffers_suffix replaces consuming_buffers
* buffers_prefix replaces buffer_prefix
* buffers_prefix_view replaces buffer_prefix_view
* buffers_front replaces buffer_front
* buffers_cat replaces buffer_cat
* buffers_cat_view replaces buffer_cat_view
* Remove buffers_suffix::get

Actions Required:

* Use buffers_suffix instead of consuming_buffers
* Use buffers_prefix instead of buffer_prefix
* Use buffers_prefix_view instead of buffer_prefix_view
* Use buffers_front instead of buffer_front
* Use buffers_cat instead of buffer_cat
* Use buffers_cat_view instead of buffer_cat_view
* Avoid calling buffers_suffix::get

--------------------------------------------------------------------------------

Version 116:

API Changes:

* message::body is a member function
* message::version is a member function

Actions Required:

* Call member function message::body instead of accessing
  the data member at call sites.

* Call member function message::version instead of accessing
  the version member at call sites.

--------------------------------------------------------------------------------

Version 115:

* Update README.md master doc link

--------------------------------------------------------------------------------

Version 114:

(No changes)

--------------------------------------------------------------------------------

Version 113:

* Fix handler signature in async_read_header
* Remove field_range copy constructor

--------------------------------------------------------------------------------

Version 112:

* Update websocket notes

API Changes:

* WebSocket writes return the bytes transferred

* HTTP reads and writes return bytes transferred

Actions Required:

* Modify websocket write completion handlers to receive
  the extra std::size_t bytes_transferred parameter.

* Modify HTTP read and/or write completion handlers to
  receive the extra std::size_t bytes_transferred parameter.

--------------------------------------------------------------------------------

Version 111:

WebSocket:

* Fix utf8 check split code point at buffer end
* Refactor stream operations and tests plus coverage
* Suspended ops special members

--------------------------------------------------------------------------------

Version 110:

* Refactor stream open state variable
* Refactor websocket stream members
* Refactor websocket stream: fixes and tests
* Add test::stream::lowest_layer
* Fix invalid iterator in test reporter

--------------------------------------------------------------------------------

Version 109:

* refactor test::stream

WebSocket:

* Fix async_read_some handler signature
* websocket close fixes and tests
* websocket handshake uses coroutine
* websocket ping tests
* Fix websocket close_op resume state
* websocket write tests
* split up websocket tests
* websocket read tests

API Changes:

* websocket accept refactoring

Actions Required:

* Do not call websocket accept overloads which take
  both a message and a buffer sequence, as it is
  illegal per rfc6455.

--------------------------------------------------------------------------------

Version 108:

* Fix argument parsing in HTTP examples
* Don't use async_write_msg in examples

--------------------------------------------------------------------------------

Version 107:

* Use test::stream

WebSocket

* Fix done state for WebSocket reads
* Fix utf8 check for compressed frames
* Rename frame and header buffer types

--------------------------------------------------------------------------------

Version 106:

* Dynamic buffer input areas are mutable
* Add flat_static_buffer::reset

HTTP:

* Fix for basic_parser::skip(true) and docs

WebSocket:

* websocket test improvements
* Remove obsolete write_op
* Refactor write_op
* Refactor ping_op
* Refactor fail_op
* Refactor read_op
* Refactor close_op
* Refactor read_op + fail_op
* Websocket close will automatically drain
* Autobahn|Testsuite fixes
* Tidy up utf8_checker and tests

--------------------------------------------------------------------------------

Version 105:

* Fix compile error in websocket snippet
* Tidy up Jamfile and tests

--------------------------------------------------------------------------------

Version 104:

* Remove unused include
* Use #error in config.hpp
* Only set -std=c++11 on Travis
* Only set /permissive- on Appveyor
* Tidy up some test warnings
* tools/ renamed from build/

WebSocket:

* Fix pausation::save

--------------------------------------------------------------------------------

Version 103:

* Boost test matrix fixes
* Tidy up allocator usage
* Example HTTP server fixes

--------------------------------------------------------------------------------

Version 102:

* Section headings in examples

--------------------------------------------------------------------------------

Version 101:

* Refactor all examples

--------------------------------------------------------------------------------

Version 100:

* Fix doc convenience includes
* Fix doc includes
* Remove unused test header
* Rename test macros
* Reorder define test macro params
* vcxproj workaround for include symlinks
* Add variadic min/max

WebSocket:

* Remove obsolete frame tests
* Refactor fail/clode code
* Call do_fail from read_some
* eof on accept returns error::closed
* Fix stream::read_size_hint calculation
* Documentation

API Changes:

* Calls to stream::close and stream::async_close will
  automatically perform the required read operations

* drain_buffer is removed

* role_type replaces teardown_tag

Actions Required:

* Remove calling code which drains the connection after
  calling stream::close or stream::async_close

* Replace code which uses drain_buffer. For websocket::stream,
  it is no longer necessary to manually drain the connection
  after closing.

* Modify signatures of teardown and async_teardown to use
  role_type instead of teardown_tag

* Change calls to teardown and async_teardown to pass the
  correct role_type, client or server, depending on context.

--------------------------------------------------------------------------------

Version 99:

* Log the value of LIB_DIR for doc builds (debug)
* Use correct handler signature in fail_op
* Fix doc typo

--------------------------------------------------------------------------------

Version 98:

* basic_fields::key_compare is noexcept
* Fix bench-zlib cmake
* Use unique names Jam projects
* Trim utf8 checker test

--------------------------------------------------------------------------------

Version 97:

* Update redirect

--------------------------------------------------------------------------------

Version 96:

* Move bench/ to test/
* Move extras/ to test/
* Use <valgrind> property
* Rename wsload compile target
* Fix library.json
* Update boostorg links
* Add bench-zlib
* Faster zlib tests
* Less compression on websocket test

--------------------------------------------------------------------------------

Version 95:

* Tidy up Travis build scripts
* Move scripts to build/
* Fix race in test::pipe
* close on test::pipe teardown
* Add test::stream
* Tidy up static_buffer braced init
* Fix html redirect
* Add valgrind variant, fix false positives

--------------------------------------------------------------------------------

Version 94:

* Use off-site Quick Start link temporarily

--------------------------------------------------------------------------------

Version 93:

* Unset BOOST_COROUTINES_NO_DEPRECATION_WARNING

--------------------------------------------------------------------------------

Version 92:

* Fix typo in test/CMakeLists.txt
* basic_fields::value_type is not copyable
* Update repository links in source comments
* Ignore Content-Length in some cases
* Tidy up doc snippet paths
* Use off-site doc link

--------------------------------------------------------------------------------

Version 91:

* Adjust redirect html
* Don't build pre-C++11
* source.dox is path-independent
* Tidy up namespace qualifiers
* Tidy up MSVC CMakeLists.txt
* Optimize buffered_read_stream
* constexpr in derived buffers
* Set BOOST_ASIO_NO_DEPRECATED
* Use Asio array optimization in static_buffer_base
* Rename wstest source file
* Use fopen_s on Windows
* Fix Appveyor script
* Update project metadata
* Move benchmarks to bench/
* Fix doc title
* Build stand-alone doc
* Update doc copyrights
* Refactor test build scripts

WebSocket:

* Tidy up websocket javadocs
* Refactor accept, handshake ops
* Use read buffer instead of buffered stream

API Changes

* control frame callbacks are non-const references

Actions Required:

* Modify calls to set the control frame callback, to
  pass non-const reference instances, and manage the
  lifetime of the instance.

--------------------------------------------------------------------------------

Version 90:

* Fix websocket read of zero length message
* Fix typo in equal_range
* Output to integrated documentation

--------------------------------------------------------------------------------

Version 89:

* Fix CONTRIBUTING.md links

--------------------------------------------------------------------------------

Version 88:

* Update doc links in README.md
* Refactor tests Jamfile
* Don't use program_options

WebSocket:

* Fix uninitialized frame done

--------------------------------------------------------------------------------

Version 87:

* Update appveyor for Boost branch
* Rename to BEAST_EXPECT
* variant fixes and tests
* Update README, add CONTRIBUTING.md and CODE_OF_CONDUCT.md

--------------------------------------------------------------------------------

Version 86:

* Boost prep
* Remove use of lexical_cast
* Use custom variant
* Update README.md
* Add local-travis.sh
* Use winapi
* Update CMakeLists.txt for boost
* Update documentation for boost
* Update copyrights
* Remove spurious declaration
* Tidy up Jamfile
* Normalize doc build scripts
* Use configured doxygen and xsltproc
* Fix Deferred Body Type Example Documentation
* Add library metadata

API Changes:

* websocket read returns the number of bytes inserted

Actions Required:

* Change the signature of completion handlers used with
  websocket::stream::async_read to void(error_code, std::size_t)

--------------------------------------------------------------------------------

Version 85:

* Fix test failure
* Tidy up test warning

--------------------------------------------------------------------------------

Version 84:

* Tidy up buffer_front
* static_buffer::consume improvement
* multi_buffer is type-check friendly
* bind_handler allows placeholders
* Add consuming_buffers::get

WebSocket:

* WebSocket read optimization

API Changes:

* websocket::stream::read_buffer_size is removed

Actions Required:

* Remove calls websocket::stream::read_buffer_size
* Use read_some and write_some instead of read_frame and write_frame

--------------------------------------------------------------------------------

Version 83:

* Add flat_static_buffer::mutable_data
* Add buffer_front
* Add wstest compression option
* Turn some flat_static_buffer_tests on
* Documentation work
* read_some, async_read_some return bytes used
* Fix private timer::clock_type

WebSocket

* Add wstest compression option
* Fix buffer lifetime in websocket write

API Changes:

* Add static_buffer

Actions Required:

* Callers who depend on static_buffer returning sequences of
  exactly length one should switch to flat_static_buffer.

--------------------------------------------------------------------------------

Version 82:

* Documentation tidying
* is_invocable works with move-only types
* Use std::function and reference wrapper
* Add session_alloc to example/common
* Silence spurious warning

HTTP

* Fix extraneous argument in async_write_header

WebSocket

* stream tidying
* Add rd_close_ to websocket stream state
* stream uses flat_buffer
* accept requires a message
* Add wstest benchmark tool

API Changes:

* Rename to flat_static_buffer and flat_static_buffer_base

Actions Required:

* Rename static_buffer to flat_static_buffer_base
* Rename static_buffer_n to flat_static_buffer

--------------------------------------------------------------------------------

Version 81:

* Pass string_view by value
* better is_final on empty_base_optimization
* Suppress incorrect GCC warning
* multi_buffer ctor is explicit
* File is not copy-constructible

API Changes:

* Refactor basic_parser, chunk parsing:

Actions Required:

* Adjust signatures for required members of user-defined
  subclasses of basic_parser

* Use the new basic_parser chunk callbacks for accessing
  chunk extensions and chunk bodies.

--------------------------------------------------------------------------------

Version 80:

* Javadoc tidying
* Add basic_dynamic_body.hpp
* Shrink buffer_prefix_view
* Remove unused file_path
* Add basic_file_body.hpp
* buffers_ref is Assignable

HTTP

* Shrink chunk header buffer sequence size

API Changes:

* Refactor chunked-encoding serialization

Actions Required:

* Remove references to ChunkDecorators. Use the new chunk-encoding
  buffer sequences to manually produce a chunked payload body in
  the case where control over the chunk-extensions and/or trailers
  is required.

--------------------------------------------------------------------------------

Version 79:

* Remove spurious fallthrough guidance

--------------------------------------------------------------------------------

Version 78:

* Add span
* Documentation work
* Use make_unique_noinit
* Fix warning in zlib
* Header file tidying
* Tidy up FieldsReader doc
* Add Boost.Locale utf8 benchmark comparison
* Tidy up dstream for existing Boost versions
* Tidy up file_posix unused variable
* Fix warning in root ca declaration

HTTP:

* Tidy up basic_string_body
* Add vector_body
* span, string, vector bodies are public
* Fix spurious uninitialized warning
* fields temp string uses allocator

API Changes:

* Add message::keep_alive()
* Add message::chunked() and message::content_length()
* Remove string_view_body

Actions Required:

* Change user defined implementations of Fields and
  FieldsReader to meet the new requirements.

* Use span_body<char> instead of string_view_body

--------------------------------------------------------------------------------

Version 77:

* file_posix works without large file support

--------------------------------------------------------------------------------

Version 76:

* Always go through write_some
* Use Boost.Config
* BodyReader may construct from a non-const message
* Add serializer::get
* Add serializer::chunked
* Serializer members are not const
* serializing file_body is not const
* Add file_body_win32
* Fix parse illegal characters in obs-fold
* Disable SSE4.2 optimizations

API Changes:

* Rename to serializer::keep_alive
* BodyReader, BodyWriter use two-phase init

Actions Required:

* Use serializer::keep_alive instead of serializer::close and
  take the logical NOT of the return value.

* Modify instances of user-defined BodyReader and BodyWriter
  types to perform two-phase initialization, as per the
  updated documented type requirements.

--------------------------------------------------------------------------------

Version 75:

* Use file_body for valid requests, string_body otherwise.
* Construct buffer_prefix_view in-place
* Shrink serializer buffers using buffers_ref
* Tidy up BEAST_NO_BIG_VARIANTS
* Shrink serializer buffers using buffers_ref
* Add serializer::limit
* file_body tests
* Using SSE4.2 intrinsics in basic_parser if available

--------------------------------------------------------------------------------

Version 74:

* Add file_stdio and File concept
* Add file_win32
* Add file_body
* Remove common/file_body.hpp
* Add file_posix
* Fix Beast include directories for cmake targets
* remove redundant flush() from example

--------------------------------------------------------------------------------

Version 73:

* Jamroot tweak
* Verify certificates in SSL clients
* Adjust benchmarks
* Initialize local variable in basic_parser
* Fixes for gcc-4.8

HTTP:

* basic_parser optimizations
* Add basic_parser tests

API Changes:

* Refactor header and message constructors
* serializer::next replaces serializer::get

Actions Required:

* Evaluate each message constructor call site and
  adjust the constructor argument list as needed.

* Use serializer::next instead of serializer::get at call sites

--------------------------------------------------------------------------------

Version 72:

HTTP:

* Tidy up set payload in http-server-fast
* Refine Body::size specification
* Newly constructed responses have a 200 OK result
* Refactor file_body for best practices
* Add http-server-threaded example
* Documentation tidying
* Various improvements to http_server_fast.cpp

WebSocket:

* Add websocket-server-async example

--------------------------------------------------------------------------------

Version 71:

* Fix extra ; warning
* Documentation revision
* Fix spurious on_chunk invocation
* Call prepare_payload in HTTP example
* Check trailers in test
* Fix buffer overflow handling for string_body and mutable_body
* Concept check in basic_dynamic_body
* Tidy up http_sync_port error check
* Tidy up Jamroot /permissive-

WebSockets:

* Fine tune websocket op asserts
* Refactor websocket composed ops
* Allow close, ping, and write to happen concurrently
* Fix race in websocket read op
* Fix websocket write op
* Add cmake options for examples and tests

API Changes:

* Return `std::size_t` from `Body::writer::put`

Actions Required:

* Return the number of bytes actually transferred from the
  input buffers in user defined `Body::writer::put` functions.

--------------------------------------------------------------------------------

Version 70:

* Serialize in one step when possible
* Add basic_parser header and body limits
* Add parser::on_header to set a callback
* Fix BEAST_FALLTHROUGH
* Fix HEAD response in file_service

API Changes:

* Rename to message::base
* basic_parser default limits are now 1MB/8MB

Actions Required:

* Change calls to message::header_part() with message::base()

* Call body_limit and/or header_limit as needed to adjust the
  limits to suitable values if the defaults are insufficient.

--------------------------------------------------------------------------------

Version 69:

* basic_parser optimizations
* Use BEAST_FALLTHROUGH to silence warnings
* Add /permissive- to msvc toolchain

--------------------------------------------------------------------------------

Version 68:

* Split common tests to a new project
* Small speed up in fields comparisons
* Adjust buffer size in fast server
* Use string_ref in older Boost versions
* Optimize field lookups
* Add const_body, mutable_body to examples
* Link statically on cmake MSVC

API Changes:

* Change BodyReader, BodyWriter requirements
* Remove BodyReader::is_deferred
* http::error::bad_target replaces bad_path

Actions Required:

* Change user defined instances of BodyReader and BodyWriter
  to meet the new requirements.

* Replace references to http::error::bad_path with http::error::bad_target

--------------------------------------------------------------------------------

Version 67:

* Fix doc example link
* Add http-server-small example
* Merge stream_base to stream and tidy
* Use boost::string_view
* Rename to http-server-fast
* Appveyor use Boost 1.64.0
* Group common example headers

API Changes:

* control_callback replaces ping_callback

Actions Required:

* Change calls to websocket::stream::ping_callback to use
  websocket::stream::control_callback

* Change user defined ping callbacks to have the new
  signature and adjust the callback definition appropriately.

--------------------------------------------------------------------------------

Version 66:

* string_param optimizations
* Add serializer request/response aliases
* Make consuming_buffers smaller
* Fix costly potential value-init in parser
* Fix unused parameter warning
* Handle bad_alloc in parser
* Tidy up message piecewise ctors
* Add header aliases
* basic_fields optimizations
* Add http-server example
* Squelch spurious warning on gcc

--------------------------------------------------------------------------------

Version 65:

* Enable narrowing warning on msvc cmake
* Fix integer types in deflate_stream::bi_reverse
* Fix narrowing in static_ostream
* Fix narrowing in ostream
* Fix narrowing in inflate_stream
* Fix narrowing in deflate_stream
* Fix integer warnings
* Enable unused variable warning on msvc cmake

--------------------------------------------------------------------------------

Version 64:

* Simplify buffered_read_stream composed op
* Simplify ssl teardown composed op
* Simplify websocket write_op
* Exemplars are compiled code
* Better User-Agent in examples
* async_write requires a non-const message
* Doc tidying
* Add link_directories to cmake

API Changes:

* Remove make_serializer

Actions Required:

* Replace calls to make_serializer with variable declarations

--------------------------------------------------------------------------------

Version 63:

* Use std::to_string instead of lexical_cast
* Don't use cached Boost
* Put num_jobs back up on Travis
* Only build and run tests in variant=coverage
* Move benchmarks to a separate project
* Only run the tests under ubasan
* Tidy up CMakeLists.txt
* Tidy up Jamfiles
* Control running with valgrind explicitly

--------------------------------------------------------------------------------

Version 62:

* Remove libssl-dev from a Travis matrix item
* Increase detail::static_ostream coverage
* Add server-framework tests
* Doc fixes and tidy
* Tidy up namespaces in examples
* Clear the error faster
* Avoid explicit operator bool for error
* Add http::is_fields trait
* Squelch harmless not_connected errors
* Put slow tests back for coverage builds

API Changes:

* parser requires basic_fields
* Refine FieldsReader concept
* message::prepare_payload replaces message::prepare

Actions Required:

* Callers using `parser` with Fields types other than basic_fields
  will need to create their own subclass of basic_parser to work
  with their custom fields type.

* Implement chunked() and keep_alive() for user defined FieldsReader types.

* Change calls to msg.prepare to msg.prepare_payload. For messages
  with a user-defined Fields, provide the function prepare_payload_impl
  in the fields type according to the Fields requirements.

--------------------------------------------------------------------------------

Version 61:

* Remove Spirit dependency
* Use generic_cateogry for errno
* Reorganize SSL examples
* Tidy up some integer conversion warnings
* Add message::header_part()
* Tidy up names in error categories
* Flush the output stream in the example
* Clean close in Secure WebSocket client
* Add server-framework SSL HTTP and WebSocket ports
* Fix shadowing warnings
* Tidy up http-crawl example
* Add multi_port to server-framework
* Tidy up resolver calls
* Use one job on CI
* Don't run slow tests on certain targets

API Changes:

* header::version is unsigned
* status-codes is unsigned

--------------------------------------------------------------------------------

Version 60:

* String comparisons are public interfaces
* Fix response message type in async websocket accept
* New server-framework, full featured server example

--------------------------------------------------------------------------------

Version 59:

* Integrated Beast INTERFACE (cmake)
* Fix base64 alphabet
* Remove obsolete doc/README.md

API Changes:

* Change Body::size signature (API Change):

Actions Required:

* For any user-defined models of Body, change the function signature
  to accept `value_type const&` and modify the function definition
  accordingly.

--------------------------------------------------------------------------------

Version 58:

* Fix unaligned reads in utf8-checker
* Qualify size_t in message template
* Reorganize examples
* Specification for http read
* Avoid `std::string` in websocket
* Fix basic_fields insert ordering
* basic_fields::set optimization
* basic_parser::put doc
* Use static string in basic_fields::reader
* Remove redundant code
* Fix parsing chunk size with leading zeroes
* Better message formal parameter names

API Changes:

* `basic_fields::set` renamed from `basic_fields::replace`

Actions Required:

* Rename calls to `basic_fields::replace` to `basic_fields::set`

--------------------------------------------------------------------------------

Version 57:

* Fix message.hpp javadocs
* Fix warning in basic_parser.cpp
* Integrate docca for documentation and tidy

--------------------------------------------------------------------------------

Version 56:

* Add provisional IANA header field names
* Add string_view_body
* Call on_chunk when the extension is empty
* HTTP/1.1 is the default version
* Try harder to find Boost (cmake)
* Reset error codes
* More basic_parser tests
* Add an INTERFACE cmake target
* Convert buffer in range loops

--------------------------------------------------------------------------------

Version 55:

* Don't allocate memory to handle obs-fold
* Avoid a parser allocation using non-flat buffer
* read_size replaces read_size_helper

--------------------------------------------------------------------------------

Version 54:

* static_buffer coverage
* flat_buffer coverage
* multi_buffer coverage
* consuming_buffers members and coverage
* basic_fields members and coverage
* Add string_param
* Retain ownership when reading using a message
* Fix incorrect use of [[fallthrough]]

API Changes:

* basic_fields refactor

--------------------------------------------------------------------------------

Version 53:

* Fix basic_parser::maybe_flatten
* Fix read_size_helper usage

--------------------------------------------------------------------------------

Version 52:

* flat_buffer is an AllocatorAwareContainer
* Add drain_buffer class

API Changes:

* `auto_fragment` is a member of `stream`
* `binary`, `text` are members of `stream`
* read_buffer_size is a member of `stream`
* read_message_max is a member of `stream`
* `write_buffer_size` is a member of `stream`
* `ping_callback` is a member of stream
* Remove `opcode` from `read`, `async_read`
* `read_frame` returns `bool` fin
* `opcode` is private
* finish(error_code&) is a BodyReader requirement

Actions Required:

* Change call sites which use `auto_fragment` with `set_option`
  to call `stream::auto_fragment` instead.

* Change call sites which use message_type with `set_option`
  to call `stream::binary` or `stream::text` instead.

* Change call sites which use `read_buffer_size` with `set_option` to
  call `stream::read_buffer_size` instead.

* Change call sites which use `read_message_max` with `set_option` to
  call `stream::read_message_max` instead.

* Change call sites which use `write_buffer_size` with `set_option` to
  call `stream::write_buffer_size` instead.

* Change call sites which use `ping_callback1 with `set_option` to
  call `stream::ping_callback` instead.

* Remove the `opcode` reference parameter from calls to synchronous
  and asynchronous read functions, replace the logic with calls to
  `stream::got_binary` and `stream::got_text` instead.

* Remove the `frame_info` parameter from all read frame call sites

* Check the return value 'fin' for calls to `read_frame`

* Change ReadHandlers passed to `async_read_frame` to have
  the signature `void(error_code, bool fin)`, use the `bool`
  to indicate if the frame is the last frame.

* Remove all occurrences of the `opcode` enum at call sites

--------------------------------------------------------------------------------

Version 51

* Fix operator<< for header
* Tidy up file_body
* Fix file_body::get() not setting the more flag correctly
* Use BOOST_FALLTHROUGH
* Use BOOST_STRINGIZE
* DynamicBuffer benchmarks
* Add construct, destroy to handler_alloc
* Fix infinite loop in basic_parser

API Changes:

* Tune up static_buffer
* multi_buffer implementation change

Actions Required:

* Call sites passing a number to multi_buffer's constructor
  will need to be adjusted, see the corresponding commit message.

--------------------------------------------------------------------------------

Version 50

* parser is constructible from other body types
* Add field enumeration
* Use allocator more in basic_fields
* Fix basic_fields allocator awareness
* Use field in basic_fields and call sites
* Use field in basic_parser
* Tidy up basic_fields, header, and field concepts
* Fields concept work
* Body documentation work
* Add missing handler_alloc nested types
* Fix chunk delimiter parsing
* Fix test::pipe read_size
* Fix chunk header parsing

API Changes:

* Remove header_parser
* Add verb to on_request for parsers
* Refactor prepare
* Protect basic_fields special members
* Remove message connection settings
* Remove message free functions
* Remove obsolete serializer allocator
* http read_some, async_read_some don't return bytes

--------------------------------------------------------------------------------

Version 49

* Use <iosfwd> instead of <ostream>

HTTP:

* Add HEAD request example

API Changes:

* Refactor method and verb
* Canonicalize string_view parameter types
* Tidy up empty_body writer error
* Refactor header status, reason, and target

--------------------------------------------------------------------------------

Version 48

* Make buffer_prefix_view public
* Remove detail::sync_ostream
* Tidy up core type traits

API Changes:

* Tidy up chunk decorator
* Rename to buffer_cat_view
* Consolidate parsers to parser.hpp
* Rename to parser

--------------------------------------------------------------------------------

Version 47

* Disable operator<< for buffer_body
* buffer_size overload for basic_multi_buffer::const_buffers_type
* Fix undefined behavior in pausation
* Fix leak in basic_flat_buffer

API Changes:

* Refactor treatment of request-method
* Refactor treatment of status code and obsolete reason
* Refactor HTTP serialization and parsing

--------------------------------------------------------------------------------

Version 46

* Add test::pipe
* Documentation work

API Changes:

* Remove HTTP header aliases
* Refactor HTTP serialization
* Refactor type traits

--------------------------------------------------------------------------------

Version 45

* Workaround for boost::asio::basic_streambuf type check
* Fix message doc image
* Better test::enable_yield_to
* Fix header::reason
* Documentation work
* buffer_view skips empty buffer sequences
* Disable reverse_iterator buffer_view test

--------------------------------------------------------------------------------

Version 44

* Use BOOST_THROW_EXCEPTION
* Tidy up read_size_helper and dynamic buffers
* Require Boost 1.58.0 or later
* Tidy up and make get_lowest_layer public
* Use BOOST_STATIC_ASSERT
* Fix async return values in docs
* Fix README websocket example
* Add buffers_adapter regression test
* Tidy up is_dynamic_buffer traits test
* Make buffers_adapter meet requirements

--------------------------------------------------------------------------------

Version 43

* Require Boost 1.64.0
* Fix strict aliasing warnings in buffers_view
* Tidy up buffer_prefix overloads and test
* Add write limit to test::string_ostream
* Additional constructors for consuming_buffers

--------------------------------------------------------------------------------

Version 42

* Fix javadoc typo
* Add formal review notes
* Make buffers_view a public interface

--------------------------------------------------------------------------------

Version 41

* Trim Appveyor matrix rows
* Concept revision and documentation
* Remove coveralls integration
* Tidy up formal parameter names

WebSocket

* Tidy up websocket::close_code enum and constructors

API Changes

* Return http::error::end_of_stream on HTTP read eof
* Remove placeholders
* Rename prepare_buffer(s) to buffer_prefix
* Remove handler helpers, tidy up hook invocations

--------------------------------------------------------------------------------

Version 40

* Add to_static_string
* Consolidate get_lowest_layer in type_traits.hpp
* Fix basic_streambuf movable trait
* Tidy up .travis.yml

--------------------------------------------------------------------------------

Version 39

Beast versions are now identified by a single integer which
is incremented on each merge. The macro BEAST_VERSION
identifies the version number, currently at 39. A version
setting commit will always be at the tip of the master
and develop branches.

* Use beast::string_view alias
* Fixed braced-init error with older gcc

HTTP

* Tidy up basic_parser javadocs

WebSocket:

* Add websocket async echo ssl server test:
* Fix eof error on ssl::stream shutdown

API Changes:

* Refactor http::header contents
* New ostream() returns dynamic buffer output stream
* New buffers() replaces to_string()
* Rename to multi_buffer, basic_multi_buffer
* Rename to flat_buffer, basic_flat_buffer
* Rename to static_buffer, static_buffer_n
* Rename to buffered_read_stream
* Harmonize concepts and identifiers with net-ts
* Tidy up HTTP reason_string

--------------------------------------------------------------------------------

1.0.0-b38

* Refactor static_string
* Refactor base64
* Use static_string for WebSocket handshakes
* Simplify get_lowest_layer test
* Add test_allocator to extras/test
* More flat_streambuf tests
* WebSocket doc work
* Prevent basic_fields operator[] assignment

API Changes:

* Refactor WebSocket error codes
* Remove websocket::keep_alive option

--------------------------------------------------------------------------------

1.0.0-b37

* CMake hide command lines in .vcxproj Output windows"
* Rename to detail::is_invocable
* Rename project to http-bench
* Fix flat_streambuf
* Add ub sanitizer blacklist
* Add -funsigned-char to asan build target
* Fix narrowing warning in table constants

WebSocket:

* Add is_upgrade() free function
* Document websocket::stream thread safety
* Rename to websocket::detail::pausation

API Changes:

* Provide websocket::stream accept() overloads
* Refactor websocket decorators
* Move everything in basic_fields.hpp to fields.hpp
* Rename to http::dynamic_body, consolidate header

--------------------------------------------------------------------------------

1.0.0-b36

* Update README.md

--------------------------------------------------------------------------------

1.0.0-b35

* Add Appveyor build scripts and badge
* Tidy up MSVC CMake configuration
* Make close_code a proper enum
* Add flat_streambuf
* Rename to BEAST_DOXYGEN
* Update .gitignore for VS2017
* Fix README.md CMake instructions

API Changes:

* New HTTP interfaces
* Remove http::empty_body

--------------------------------------------------------------------------------

1.0.0-b34

* Fix and tidy up CMake build scripts

--------------------------------------------------------------------------------

1.0.0-b33

* Require Visual Studio 2015 Update 3 or later

HTTP

* Use fwrite return value in file_body

WebSocket

* Set internal state correctly when writing frames
* Add decorator unit test
* Add write_frames unit test

--------------------------------------------------------------------------------

1.0.0-b32

* Add io_service completion invariants test
* Update CMake scripts for finding packages

API Changes:

* Remove http Writer suspend and resume feature

--------------------------------------------------------------------------------

1.0.0-b31

* Tidy up build settings
* Add missing dynabuf_readstream member

WebSocket

* Move the handler, don't copy it

--------------------------------------------------------------------------------

1.0.0-b30

WebSocket

* Fix race in pings during reads
* Fix race in close frames during reads
* Fix race when write suspends
* Allow concurrent websocket async ping and writes

--------------------------------------------------------------------------------

1.0.0-b29

* Fix compilation error in non-template class
* Document type-pun in buffer_cat
* Correctly check ostream modifier (/extras)

HTTP

* Fix Body requirements doc
* Fix illegal HTTP characters accepted as hex zero
* Fix Writer return value documentation

WebSocket

* Fix race in writes during reads
* Fix doc link typo

--------------------------------------------------------------------------------

1.0.0-b28

* Split out and rename test stream classes
* Restyle async result constructions
* Fix HTTP split parse edge case

--------------------------------------------------------------------------------

1.0.0-b27

* Tidy up tests and docs
* Add documentation building instructions

API Changes:

* Invoke callback on pings and pongs
* Move basic_streambuf to streambuf.hpp

--------------------------------------------------------------------------------

1.0.0-b26

* Tidy up warnings and tests

--------------------------------------------------------------------------------

1.0.0-b25

* Fixes for WebSocket echo server
* Fix 32-bit arm7 warnings
* Remove unnecessary include
* WebSocket server examples and test tidying
* Fix deflate setup bug

API Changes:

* Better handler_ptr

--------------------------------------------------------------------------------

1.0.0-b24

* bjam use clang on MACOSX
* Simplify Travis package install specification
* Add optional yield_to arguments
* Make decorator copyable
* Add WebSocket permessage-deflate extension support

--------------------------------------------------------------------------------

1.0.0-b23

* Tune websocket echo server for performance
* Add file and line number to thrown exceptions
* Better logging in async echo server
* Add copy special members
* Fix message constructor and special members
* Travis CI improvements

--------------------------------------------------------------------------------

1.0.0-b22

* Fix broken Intellisense
* Implement the Asio deallocation-before-invocation guarantee
* Add handler helpers
* Avoid copies in handler_alloc
* Update README.md example programs
* Fix websocket stream read documentation
* Disable Boost.Coroutine deprecation warning
* Update documentation examples

--------------------------------------------------------------------------------

1.0.0-b21

* Remove extraneous includes

--------------------------------------------------------------------------------

1.0.0-b20

ZLib

* Add ZLib module

API Changes:

* Rename HTTP identifiers

--------------------------------------------------------------------------------

1.0.0-b19

* Boost library min/max guidance
* Improvements to code coverage
* Use boost::lexical_cast instead of std::to_string
* Fix prepare_buffers value_type
* Fix consuming_buffers value_type
* Better buffer_cat

HTTP

* Make chunk_encode public
* Add write, async_write, operator<< for message_headers
* Add read, async_read for message_headers
* Fix with_body example

WebSocket

* Optimize utf8 validation
* Optimize mask operations

API Changes:

* Refactor message and message_headers declarations
* prepared_buffers is private
* consume_buffers is removed

--------------------------------------------------------------------------------

1.0.0-b18

* Increase optimization settings for MSVC builds

HTTP

* Check invariants in parse_op:
* Clean up message docs

WebSocket

* Write buffer option does not change capacity
* Close connection during async_read on close frame
* Add pong, async pong to stream

Core

* Meet DynamicBuffer requirements for static_streambuf
* Fix write_frame masking and auto-fragment handling

Extras

* unit_test::suite fixes:
  - New overload of fail() specifies file and line
  - BEAST_EXPECTS only evaluates the reason string on a failure
* Add zlib module

--------------------------------------------------------------------------------

1.0.0-b17

* Change implicit to default value in example
* Tidy up some declarations
* Fix basic_streambuf::capacity
* Add basic_streambuf::alloc_size
* Parser callbacks may not throw
* Fix Reader concept doc typo
* Add is_Reader trait
* Tidy up basic_headers for documentation
* Tidy up documentation
* Add basic_parser_v1::reset
* Fix handling of body_what::pause in basic_parser_v1
* Add headers_parser
* Engaged invokable is destructible
* Improve websocket example in README.md
* Refactor read_size_helper

API Changes:

* Added init() to Reader requirements
* Reader must be nothrow constructible
* Reader is now constructed right before reading the body
  - The message passed on construction is filled in
* Rework HTTP concepts:
  - Writer uses write instead of operator()
  - Refactor traits to use void_t
  - Remove is_ReadableBody, is_WritableBody
  - Add has_reader, has_writer, is_Reader, is_Writer
  - More friendly compile errors on failed concept checks
* basic_parser_v1 requires all callbacks present
* on_headers parser callback now returns void
* on_body_what is a new required parser callback returning body_what

--------------------------------------------------------------------------------

1.0.0-b16

* Make value optional in param-list
* Frame processing routines are member functions
* Fix on_headers called twice from basic_parser_v1
* Constrain parser_v1 constructor
* Improve first line serialization
* Add pause option to on_headers interface
* Refactor base_parser_v1 callback traits:
* Refine Parser concept
* Relax ForwardIterator requirements in FieldSequence
* Fix websocket failure testing
* Refine Writer concept and fix exemplar in documentation

API Changes:

* Rename mask_buffer_size to write_buffer_size
* Make auto_fragment a boolean option

The message class hierarchy is refactored (breaking change):

* One message class now models both HTTP/1 and HTTP/2 messages
* message_v1, request_v1, response_v1 removed
* New classes basic_request and basic_response model
  messages without the body.

    Error resolution: Callers should use message, request,
    and response instead of message_v1, request_v1, and
    response_v1 respectively.

--------------------------------------------------------------------------------

1.0.0-b15

* rfc7230 section 3.3.2 compliance
* Add HTTPS example
* Add Secure WebSocket example
* Fix message_v1 constructor
* Tidy up DynamicBuffer requirements
* Tidy up error types and headers
* Fix handling empty HTTP headers in parser_v1

--------------------------------------------------------------------------------

1.0.0-b14

* Add missing rebind to handler_alloc
* Fix error handling in http server examples
* Fix CMake scripts for MinGW
* Use BOOST_ASSERT
* Better WebSocket decorator
* Update and tidy documentation

--------------------------------------------------------------------------------

1.0.0-b13

* dstream improvements
* Remove bin and bin64 directories
* Tidy up .vcxproj file groupings

--------------------------------------------------------------------------------

1.0.0-b12

* Use -p to print suites from unit test main.
* BEAST_EXPECTS to add a reason string to test failures
* Fix unit test runner to output all case names
* Update README for build requirements
* Rename to CHANGELOG.md

--------------------------------------------------------------------------------

1.0.0-b11

* Set URI in generated WebSocket Upgrade requests
* Rename echo server class and file names
* Rename to DynamicBuffer in some code and documentation
* Fix integer warnings in Windows builds
* Add 32 and 64 bit Windows build support
* Update README for build instructions and more
* Add repository and documentation banners

--------------------------------------------------------------------------------

1.0.0-b10

* Fix compilation warnings
* Add websocketpp comparison to HTML documentation

--------------------------------------------------------------------------------

1.0.0-b9

* Fix CMakeLists.txt

--------------------------------------------------------------------------------

1.0.0-b8

* Fix include in example code
* Fix basic_headers rfc2616 Section 4.2 compliance

--------------------------------------------------------------------------------

1.0.0-b7

* Fix prepare by calling init. prepare() can throw depending on the
  implementation of Writer. Publicly provided beast::http writers never throw.
* Fixes to example HTTP server
* Fully qualify ambiguous calls to read and parse
* Remove deprecated http::stream wrapper
* Example HTTP server now calculates the MIME-type
* Fixes and documentation for teardown and use with SSL:
* Add example code to rfc7230 javadocs
* Remove extraneous header file <beast/http/status.hpp>
* Add skip_body parser option

--------------------------------------------------------------------------------

1.0.0-b6

* Use SFINAE on return values
* Use beast::error_code instead of nested types
* Tidy up use of GENERATING_DOCS
* Remove obsolete RFC2616 functions
* Add message swap members and free functions
* Add HTTP field value parser containers: ext_list, param_list, token_list
* Fixes for some corner cases in basic_parser_v1
* Configurable limits on headers and body sizes in basic_parser_v1

API Changes:

* ci_equal is moved to beast::http namespace, in rfc7230.hpp

* "DynamicBuffer","dynabuf" renamed from "Streambuf", "streambuf". See:
  http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4478.html#requirements.dynamic_buffers

* basic_parser_v1 adheres to rfc7230 as strictly as possible

--------------------------------------------------------------------------------
