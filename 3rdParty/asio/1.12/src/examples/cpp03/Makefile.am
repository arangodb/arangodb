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
	http/client/async_client \
	http/client/sync_client \
	http/server/http_server \
	http/server2/http_server \
	http/server3/http_server \
	http/server4/http_server \
	icmp/ping \
	invocation/prioritised_handlers \
	iostreams/daytime_client \
	iostreams/daytime_server \
	iostreams/http_client \
	multicast/receiver \
	multicast/sender \
	nonblocking/third_party_lib \
	porthopper/client \
	porthopper/server \
	services/daytime_client \
	socks4/sync_client \
	timeouts/async_tcp_client \
	timeouts/blocking_tcp_client \
	timeouts/blocking_udp_client \
	timeouts/server \
	timers/time_t_timer \
	tutorial/timer1/timer \
	tutorial/timer2/timer \
	tutorial/timer3/timer \
	tutorial/timer4/timer \
	tutorial/timer5/timer \
	tutorial/daytime1/client \
	tutorial/daytime2/server \
	tutorial/daytime3/server \
	tutorial/daytime4/client \
	tutorial/daytime5/server \
	tutorial/daytime6/server \
	tutorial/daytime7/server

if !WINDOWS_TARGET
noinst_PROGRAMS += \
	chat/posix_chat_client \
	fork/daemon \
	fork/process_per_connection \
	local/connect_pair \
	local/iostream_client \
	local/stream_server \
	local/stream_client
endif

if WINDOWS_TARGET
noinst_PROGRAMS += \
	windows/transmit_file
endif

if HAVE_OPENSSL
noinst_PROGRAMS += \
	ssl/client \
	ssl/server
endif

if HAVE_BOOST_COROUTINE
noinst_PROGRAMS += \
  spawn/echo_server \
  spawn/parallel_grep
endif

noinst_HEADERS = \
	chat/chat_message.hpp \
	services/basic_logger.hpp \
	services/logger.hpp \
	services/logger_service.hpp

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
http_client_async_client_SOURCES = http/client/async_client.cpp
http_client_sync_client_SOURCES = http/client/sync_client.cpp
http_server_http_server_SOURCES = \
	http/server/connection.cpp \
	http/server/connection_manager.cpp \
	http/server/main.cpp \
	http/server/mime_types.cpp \
	http/server/reply.cpp \
	http/server/request_handler.cpp \
	http/server/request_parser.cpp \
	http/server/server.cpp
http_server2_http_server_SOURCES = \
	http/server2/connection.cpp \
	http/server2/io_context_pool.cpp \
	http/server2/main.cpp \
	http/server2/mime_types.cpp \
	http/server2/reply.cpp \
	http/server2/request_handler.cpp \
	http/server2/request_parser.cpp \
	http/server2/server.cpp
http_server3_http_server_SOURCES = \
	http/server3/connection.cpp \
	http/server3/main.cpp \
	http/server3/mime_types.cpp \
	http/server3/reply.cpp \
	http/server3/request_handler.cpp \
	http/server3/request_parser.cpp \
	http/server3/server.cpp
http_server4_http_server_SOURCES = \
	http/server4/file_handler.cpp \
	http/server4/main.cpp \
	http/server4/mime_types.cpp \
	http/server4/reply.cpp \
	http/server4/request_parser.cpp \
	http/server4/server.cpp
icmp_ping_SOURCES = icmp/ping.cpp
invocation_prioritised_handlers_SOURCES = invocation/prioritised_handlers.cpp
iostreams_daytime_client_SOURCES = iostreams/daytime_client.cpp
iostreams_daytime_server_SOURCES = iostreams/daytime_server.cpp
iostreams_http_client_SOURCES = iostreams/http_client.cpp
multicast_receiver_SOURCES = multicast/receiver.cpp
multicast_sender_SOURCES = multicast/sender.cpp
nonblocking_third_party_lib_SOURCES = nonblocking/third_party_lib.cpp
porthopper_client_SOURCES = porthopper/client.cpp
porthopper_server_SOURCES = porthopper/server.cpp
services_daytime_client_SOURCES = \
	services/daytime_client.cpp \
	services/logger_service.cpp
socks4_sync_client_SOURCES = socks4/sync_client.cpp
timeouts_async_tcp_client_SOURCES = timeouts/async_tcp_client.cpp
timeouts_blocking_tcp_client_SOURCES = timeouts/blocking_tcp_client.cpp
timeouts_blocking_udp_client_SOURCES = timeouts/blocking_udp_client.cpp
timeouts_server_SOURCES = timeouts/server.cpp
timers_time_t_timer_SOURCES = timers/time_t_timer.cpp
tutorial_timer1_timer_SOURCES = tutorial/timer1/timer.cpp
tutorial_timer2_timer_SOURCES = tutorial/timer2/timer.cpp
tutorial_timer3_timer_SOURCES = tutorial/timer3/timer.cpp
tutorial_timer4_timer_SOURCES = tutorial/timer4/timer.cpp
tutorial_timer5_timer_SOURCES = tutorial/timer5/timer.cpp
tutorial_daytime1_client_SOURCES = tutorial/daytime1/client.cpp
tutorial_daytime2_server_SOURCES = tutorial/daytime2/server.cpp
tutorial_daytime3_server_SOURCES = tutorial/daytime3/server.cpp
tutorial_daytime4_client_SOURCES = tutorial/daytime4/client.cpp
tutorial_daytime5_server_SOURCES = tutorial/daytime5/server.cpp
tutorial_daytime6_server_SOURCES = tutorial/daytime6/server.cpp
tutorial_daytime7_server_SOURCES = tutorial/daytime7/server.cpp

if !WINDOWS_TARGET
chat_posix_chat_client_SOURCES = chat/posix_chat_client.cpp
fork_daemon_SOURCES = fork/daemon.cpp
fork_process_per_connection_SOURCES = fork/process_per_connection.cpp
local_connect_pair_SOURCES = local/connect_pair.cpp
local_iostream_client_SOURCES = local/iostream_client.cpp
local_stream_server_SOURCES = local/stream_server.cpp
local_stream_client_SOURCES = local/stream_client.cpp
endif

if WINDOWS_TARGET
windows_transmit_file_SOURCES = windows/transmit_file.cpp
endif

if HAVE_OPENSSL
ssl_client_SOURCES = ssl/client.cpp
ssl_server_SOURCES = ssl/server.cpp
endif

if HAVE_BOOST_COROUTINE
spawn_echo_server_SOURCES = spawn/echo_server.cpp
spawn_echo_server_LDADD = $(LDADD) -lboost_coroutine -lboost_context -lboost_thread -lboost_chrono -lboost_system
spawn_parallel_grep_SOURCES = spawn/parallel_grep.cpp
spawn_parallel_grep_LDADD = $(LDADD) -lboost_coroutine -lboost_context -lboost_thread -lboost_chrono -lboost_system
endif

EXTRA_DIST = \
	http/server/connection.hpp \
	http/server/connection_manager.hpp \
	http/server/header.hpp \
	http/server/mime_types.hpp \
	http/server/reply.hpp \
	http/server/request.hpp \
	http/server/request_handler.hpp \
	http/server/request_parser.hpp \
	http/server/server.hpp \
	http/server2/connection.hpp \
	http/server2/io_context_pool.hpp \
	http/server2/header.hpp \
	http/server2/mime_types.hpp \
	http/server2/reply.hpp \
	http/server2/request.hpp \
	http/server2/request_handler.hpp \
	http/server2/request_parser.hpp \
	http/server2/server.hpp \
	http/server3/connection.hpp \
	http/server3/header.hpp \
	http/server3/mime_types.hpp \
	http/server3/reply.hpp \
	http/server3/request.hpp \
	http/server3/request_handler.hpp \
	http/server3/request_parser.hpp \
	http/server3/server.hpp \
	http/server4/file_handler.hpp \
	http/server4/header.hpp \
	http/server4/mime_types.hpp \
	http/server4/reply.hpp \
	http/server4/request.hpp \
	http/server4/request_parser.hpp \
	http/server4/server.hpp \
	icmp/icmp_header.hpp \
	icmp/ipv4_header.hpp \
	porthopper/protocol.hpp \
	serialization/client.cpp \
	serialization/server.cpp \
	serialization/connection.hpp \
	serialization/stock.hpp \
	services/basic_logger.hpp \
	services/logger.hpp \
	services/logger_service.hpp \
	socks4/socks4.hpp \
	ssl/README \
	ssl/ca.pem \
	ssl/server.pem \
	ssl/dh2048.pem \
	timers/tick_count_timer.cpp

MAINTAINERCLEANFILES = \
	$(srcdir)/Makefile.in
