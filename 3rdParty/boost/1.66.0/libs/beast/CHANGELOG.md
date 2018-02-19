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
  types to perfrom two-phase initialization, as per the
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
* Add repository and documention banners

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
