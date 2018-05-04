AUTOMAKE_OPTIONS = subdir-objects

if SEPARATE_COMPILATION
noinst_LIBRARIES = libasio.a
libasio_a_SOURCES = ../../asio.cpp
if HAVE_OPENSSL
libasio_a_SOURCES += ../../asio_ssl.cpp
endif
LDADD = libasio.a
endif

noinst_PROGRAMS = \
	allocation/server \
	buffers/reference_counted \
	chat/chat_client \
	chat/chat_server \
	echo/async_tcp_echo_server \
	echo/async_udp_echo_server \
	echo/blocking_tcp_echo_client \
	echo/blocking_tcp_echo_server \
	echo/blocking_udp_echo_client \
	echo/blocking_udp_echo_server \
	executors/actor \
	executors/bank_account_1 \
	executors/bank_account_2 \
	executors/fork_join \
	executors/pipeline \
	executors/priority_scheduler \
	futures/daytime_client \
	http/server/http_server \
	invocation/prioritised_handlers \
	iostreams/http_client \
	multicast/receiver \
	multicast/sender \
	nonblocking/third_party_lib

if !WINDOWS_TARGET
noinst_PROGRAMS += \
	fork/daemon \
	fork/process_per_connection \
	local/connect_pair \
	local/iostream_client \
	local/stream_server \
	local/stream_client
endif

if HAVE_BOOST_COROUTINE
noinst_PROGRAMS += \
  spawn/echo_server \
  spawn/parallel_grep
endif

noinst_HEADERS = \
	chat/chat_message.hpp

AM_CXXFLAGS = -I$(srcdir)/../../../include

allocation_server_SOURCES = allocation/server.cpp
buffers_reference_counted_SOURCES = buffers/reference_counted.cpp
chat_chat_client_SOURCES = chat/chat_client.cpp
chat_chat_server_SOURCES = chat/chat_server.cpp
echo_async_tcp_echo_server_SOURCES = echo/async_tcp_echo_server.cpp
echo_async_udp_echo_server_SOURCES = echo/async_udp_echo_server.cpp
echo_blocking_tcp_echo_client_SOURCES = echo/blocking_tcp_echo_client.cpp
echo_blocking_tcp_echo_server_SOURCES = echo/blocking_tcp_echo_server.cpp
echo_blocking_udp_echo_client_SOURCES = echo/blocking_udp_echo_client.cpp
echo_blocking_udp_echo_server_SOURCES = echo/blocking_udp_echo_server.cpp
executors_actor_SOURCES = executors/actor.cpp
executors_bank_account_1_SOURCES = executors/bank_account_1.cpp
executors_bank_account_2_SOURCES = executors/bank_account_2.cpp
executors_fork_join_SOURCES = executors/fork_join.cpp
executors_pipeline_SOURCES = executors/pipeline.cpp
executors_priority_scheduler_SOURCES = executors/priority_scheduler.cpp
futures_daytime_client_SOURCES = futures/daytime_client.cpp
http_server_http_server_SOURCES = \
	http/server/connection.cpp \
	http/server/connection_manager.cpp \
	http/server/main.cpp \
	http/server/mime_types.cpp \
	http/server/reply.cpp \
	http/server/request_handler.cpp \
	http/server/request_parser.cpp \
	http/server/server.cpp
invocation_prioritised_handlers_SOURCES = invocation/prioritised_handlers.cpp
iostreams_http_client_SOURCES = iostreams/http_client.cpp
multicast_receiver_SOURCES = multicast/receiver.cpp
multicast_sender_SOURCES = multicast/sender.cpp
nonblocking_third_party_lib_SOURCES = nonblocking/third_party_lib.cpp

if !WINDOWS_TARGET
fork_daemon_SOURCES = fork/daemon.cpp
fork_process_per_connection_SOURCES = fork/process_per_connection.cpp
local_connect_pair_SOURCES = local/connect_pair.cpp
local_iostream_client_SOURCES = local/iostream_client.cpp
local_stream_server_SOURCES = local/stream_server.cpp
local_stream_client_SOURCES = local/stream_client.cpp
endif

if HAVE_BOOST_COROUTINE
spawn_echo_server_SOURCES = spawn/echo_server.cpp
spawn_echo_server_LDADD = $(LDADD) -lboost_coroutine -lboost_context -lboost_thread -lboost_chrono -lboost_system
spawn_parallel_grep_SOURCES = spawn/parallel_grep.cpp
spawn_parallel_grep_LDADD = $(LDADD) -lboost_coroutine -lboost_context -lboost_thread -lboost_chrono -lboost_system
endif

EXTRA_DIST = \
	handler_tracking/custom_tracking.hpp \
	http/server/connection.hpp \
	http/server/connection_manager.hpp \
	http/server/header.hpp \
	http/server/mime_types.hpp \
	http/server/reply.hpp \
	http/server/request.hpp \
	http/server/request_handler.hpp \
	http/server/request_parser.hpp \
	http/server/server.hpp

MAINTAINERCLEANFILES = \
	$(srcdir)/Makefile.in
