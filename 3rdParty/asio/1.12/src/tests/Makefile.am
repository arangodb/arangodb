AUTOMAKE_OPTIONS = subdir-objects

if SEPARATE_COMPILATION
noinst_LIBRARIES = libasio.a
libasio_a_SOURCES = ../asio.cpp
if HAVE_OPENSSL
libasio_a_SOURCES += ../asio_ssl.cpp
endif
LDADD = libasio.a
endif

check_PROGRAMS = \
	unit/associated_allocator \
	unit/associated_executor \
	unit/async_result \
	unit/basic_datagram_socket \
	unit/basic_deadline_timer \
	unit/basic_raw_socket \
	unit/basic_seq_packet_socket \
	unit/basic_serial_port \
	unit/basic_signal_set \
	unit/basic_socket_acceptor \
	unit/basic_stream_socket \
	unit/basic_streambuf \
	unit/basic_waitable_timer \
	unit/bind_executor \
	unit/buffered_read_stream \
	unit/buffered_stream \
	unit/buffered_write_stream \
	unit/buffer \
	unit/buffers_iterator \
	unit/completion_condition \
	unit/connect \
	unit/coroutine \
	unit/datagram_socket_service \
	unit/deadline_timer_service \
	unit/deadline_timer \
	unit/defer \
	unit/dispatch \
	unit/error \
	unit/execution_context \
	unit/executor \
	unit/executor_work_guard \
	unit/generic/basic_endpoint \
	unit/generic/datagram_protocol \
	unit/generic/raw_protocol \
	unit/generic/seq_packet_protocol \
	unit/generic/stream_protocol \
	unit/high_resolution_timer \
	unit/io_context \
	unit/ip/address \
	unit/ip/address_v4 \
	unit/ip/address_v4_iterator \
	unit/ip/address_v4_range \
	unit/ip/address_v6 \
	unit/ip/address_v6_iterator \
	unit/ip/address_v6_range \
	unit/ip/basic_endpoint \
	unit/ip/basic_resolver \
	unit/ip/basic_resolver_entry \
	unit/ip/basic_resolver_iterator \
	unit/ip/basic_resolver_query \
	unit/ip/host_name \
	unit/ip/icmp \
	unit/ip/multicast \
	unit/ip/network_v4 \
	unit/ip/network_v6 \
	unit/ip/resolver_query_base \
	unit/ip/resolver_service \
	unit/ip/tcp \
	unit/ip/udp \
	unit/ip/unicast \
	unit/ip/v6_only \
	unit/is_read_buffered \
	unit/is_write_buffered \
	unit/local/basic_endpoint \
	unit/local/connect_pair \
	unit/local/datagram_protocol \
	unit/local/stream_protocol \
	unit/packaged_task \
	unit/placeholders \
	unit/posix/basic_descriptor \
	unit/posix/basic_stream_descriptor \
	unit/posix/descriptor \
	unit/posix/descriptor_base \
	unit/posix/stream_descriptor \
	unit/posix/stream_descriptor_service \
	unit/post \
	unit/raw_socket_service \
	unit/read \
	unit/read_at \
	unit/read_until \
	unit/seq_packet_socket_service \
	unit/serial_port \
	unit/serial_port_base \
	unit/serial_port_service \
	unit/signal_set \
	unit/signal_set_service \
	unit/socket_acceptor_service \
	unit/socket_base \
	unit/steady_timer \
	unit/strand \
	unit/stream_socket_service \
	unit/streambuf \
	unit/system_context \
	unit/system_executor \
	unit/system_timer \
	unit/thread \
	unit/time_traits \
	unit/ts/buffer \
	unit/ts/executor \
	unit/ts/internet \
	unit/ts/io_context \
	unit/ts/net \
	unit/ts/netfwd \
	unit/ts/socket \
	unit/ts/timer \
	unit/use_future \
	unit/uses_executor \
	unit/wait_traits \
	unit/waitable_timer_service \
	unit/windows/basic_handle \
	unit/windows/basic_object_handle \
	unit/windows/basic_random_access_handle \
	unit/windows/basic_stream_handle \
	unit/windows/object_handle \
	unit/windows/object_handle_service \
	unit/windows/overlapped_handle \
	unit/windows/overlapped_ptr \
	unit/windows/random_access_handle \
	unit/windows/random_access_handle_service \
	unit/windows/stream_handle \
	unit/windows/stream_handle_service \
	unit/write \
	unit/write_at

if !STANDALONE
noinst_PROGRAMS = \
	latency/tcp_client \
	latency/tcp_server \
	latency/udp_client \
	latency/udp_server \
	performance/client \
	performance/server
endif

if HAVE_OPENSSL
check_PROGRAMS += \
	unit/ssl/context_base \
	unit/ssl/context \
	unit/ssl/error \
	unit/ssl/rfc2818_verification \
	unit/ssl/stream_base \
	unit/ssl/stream
endif

TESTS = \
	unit/associated_allocator \
	unit/associated_executor \
	unit/async_result \
	unit/basic_datagram_socket \
	unit/basic_deadline_timer \
	unit/basic_raw_socket \
	unit/basic_seq_packet_socket \
	unit/basic_serial_port \
	unit/basic_signal_set \
	unit/basic_socket_acceptor \
	unit/basic_stream_socket \
	unit/basic_streambuf \
	unit/basic_waitable_timer \
	unit/bind_executor \
	unit/buffered_read_stream \
	unit/buffered_stream \
	unit/buffered_write_stream \
	unit/buffer \
	unit/buffers_iterator \
	unit/completion_condition \
	unit/connect \
	unit/datagram_socket_service \
	unit/deadline_timer_service \
	unit/deadline_timer \
	unit/defer \
	unit/dispatch \
	unit/error \
	unit/execution_context \
	unit/executor \
	unit/executor_work_guard \
	unit/high_resolution_timer \
	unit/io_context \
	unit/ip/address \
	unit/ip/address_v4 \
	unit/ip/address_v4_iterator \
	unit/ip/address_v4_range \
	unit/ip/address_v6 \
	unit/ip/address_v6_iterator \
	unit/ip/address_v6_range \
	unit/ip/basic_endpoint \
	unit/ip/basic_resolver \
	unit/ip/basic_resolver_entry \
	unit/ip/basic_resolver_iterator \
	unit/ip/basic_resolver_query \
	unit/ip/host_name \
	unit/ip/icmp \
	unit/ip/multicast \
	unit/ip/network_v4 \
	unit/ip/network_v6 \
	unit/ip/resolver_query_base \
	unit/ip/resolver_service \
	unit/ip/tcp \
	unit/ip/udp \
	unit/ip/unicast \
	unit/ip/v6_only \
	unit/is_read_buffered \
	unit/is_write_buffered \
	unit/local/basic_endpoint \
	unit/local/connect_pair \
	unit/local/datagram_protocol \
	unit/local/stream_protocol \
	unit/packaged_task \
	unit/placeholders \
	unit/posix/basic_descriptor \
	unit/posix/basic_stream_descriptor \
	unit/posix/descriptor\
	unit/posix/descriptor_base \
	unit/posix/stream_descriptor \
	unit/posix/stream_descriptor_service \
	unit/post \
	unit/raw_socket_service \
	unit/read \
	unit/read_at \
	unit/read_until \
	unit/seq_packet_socket_service \
	unit/serial_port \
	unit/serial_port_base \
	unit/serial_port_service \
	unit/signal_set \
	unit/signal_set_service \
	unit/socket_acceptor_service \
	unit/socket_base \
	unit/steady_timer \
	unit/strand \
	unit/stream_socket_service \
	unit/streambuf \
	unit/system_context \
	unit/system_executor \
	unit/system_timer \
	unit/thread \
	unit/time_traits \
	unit/ts/buffer \
	unit/ts/executor \
	unit/ts/internet \
	unit/ts/io_context \
	unit/ts/net \
	unit/ts/netfwd \
	unit/ts/socket \
	unit/ts/timer \
	unit/use_future \
	unit/uses_executor \
	unit/wait_traits \
	unit/waitable_timer_service \
	unit/windows/basic_handle \
	unit/windows/basic_object_handle \
	unit/windows/basic_random_access_handle \
	unit/windows/basic_stream_handle \
	unit/windows/object_handle \
	unit/windows/object_handle_service \
	unit/windows/overlapped_handle \
	unit/windows/overlapped_ptr \
	unit/windows/random_access_handle \
	unit/windows/random_access_handle_service \
	unit/windows/stream_handle \
	unit/windows/stream_handle_service \
	unit/write \
	unit/write_at

if HAVE_OPENSSL
TESTS += \
	unit/ssl/context_base \
	unit/ssl/context \
	unit/ssl/error \
	unit/ssl/rfc2818_verification \
	unit/ssl/stream_base \
	unit/ssl/stream
endif

noinst_HEADERS = \
	latency/high_res_clock.hpp \
	unit/unit_test.hpp

AM_CXXFLAGS = -I$(srcdir)/../../include

if !STANDALONE
latency_tcp_client_SOURCES = latency/tcp_client.cpp
latency_tcp_server_SOURCES = latency/tcp_server.cpp
latency_udp_client_SOURCES = latency/udp_client.cpp
latency_udp_server_SOURCES = latency/udp_server.cpp
performance_client_SOURCES = performance/client.cpp
performance_server_SOURCES = performance/server.cpp
endif

unit_associated_allocator_SOURCES = unit/associated_allocator.cpp
unit_associated_executor_SOURCES = unit/associated_executor.cpp
unit_async_result_SOURCES = unit/async_result.cpp
unit_basic_datagram_socket_SOURCES = unit/basic_datagram_socket.cpp
unit_basic_deadline_timer_SOURCES = unit/basic_deadline_timer.cpp
unit_basic_raw_socket_SOURCES = unit/basic_raw_socket.cpp
unit_basic_seq_packet_socket_SOURCES = unit/basic_seq_packet_socket.cpp
unit_basic_serial_port_SOURCES = unit/basic_serial_port.cpp
unit_basic_signal_set_SOURCES = unit/basic_signal_set.cpp
unit_basic_socket_acceptor_SOURCES = unit/basic_socket_acceptor.cpp
unit_basic_stream_socket_SOURCES = unit/basic_stream_socket.cpp
unit_basic_streambuf_SOURCES = unit/basic_streambuf.cpp
unit_basic_waitable_timer_SOURCES = unit/basic_waitable_timer.cpp
unit_bind_executor_SOURCES = unit/bind_executor.cpp
unit_buffer_SOURCES = unit/buffer.cpp
unit_buffers_iterator_SOURCES = unit/buffers_iterator.cpp
unit_buffered_read_stream_SOURCES = unit/buffered_read_stream.cpp
unit_buffered_stream_SOURCES = unit/buffered_stream.cpp
unit_buffered_write_stream_SOURCES = unit/buffered_write_stream.cpp
unit_completion_condition_SOURCES = unit/completion_condition.cpp
unit_connect_SOURCES = unit/connect.cpp
unit_coroutine_SOURCES = unit/coroutine.cpp
unit_datagram_socket_service_SOURCES = unit/datagram_socket_service.cpp
unit_deadline_timer_service_SOURCES = unit/deadline_timer_service.cpp
unit_deadline_timer_SOURCES = unit/deadline_timer.cpp
unit_defer_SOURCES = unit/defer.cpp
unit_dispatch_SOURCES = unit/dispatch.cpp
unit_error_SOURCES = unit/error.cpp
unit_execution_context_SOURCES = unit/execution_context.cpp
unit_executor_SOURCES = unit/executor.cpp
unit_executor_work_guard_SOURCES = unit/executor_work_guard.cpp
unit_generic_basic_endpoint_SOURCES = unit/generic/basic_endpoint.cpp
unit_generic_datagram_protocol_SOURCES = unit/generic/datagram_protocol.cpp
unit_generic_raw_protocol_SOURCES = unit/generic/raw_protocol.cpp
unit_generic_seq_packet_protocol_SOURCES = unit/generic/seq_packet_protocol.cpp
unit_generic_stream_protocol_SOURCES = unit/generic/stream_protocol.cpp
unit_high_resolution_timer_SOURCES = unit/high_resolution_timer.cpp
unit_io_context_SOURCES = unit/io_context.cpp
unit_ip_address_SOURCES = unit/ip/address.cpp
unit_ip_address_v4_SOURCES = unit/ip/address_v4.cpp
unit_ip_address_v4_iterator_SOURCES = unit/ip/address_v4_iterator.cpp
unit_ip_address_v4_range_SOURCES = unit/ip/address_v4_range.cpp
unit_ip_address_v6_SOURCES = unit/ip/address_v6.cpp
unit_ip_address_v6_iterator_SOURCES = unit/ip/address_v6_iterator.cpp
unit_ip_address_v6_range_SOURCES = unit/ip/address_v6_range.cpp
unit_ip_basic_endpoint_SOURCES = unit/ip/basic_endpoint.cpp
unit_ip_basic_resolver_SOURCES = unit/ip/basic_resolver.cpp
unit_ip_basic_resolver_entry_SOURCES = unit/ip/basic_resolver_entry.cpp
unit_ip_basic_resolver_iterator_SOURCES = unit/ip/basic_resolver_iterator.cpp
unit_ip_basic_resolver_query_SOURCES = unit/ip/basic_resolver_query.cpp
unit_ip_host_name_SOURCES = unit/ip/host_name.cpp
unit_ip_icmp_SOURCES = unit/ip/icmp.cpp
unit_ip_multicast_SOURCES = unit/ip/multicast.cpp
unit_ip_network_v4_SOURCES = unit/ip/network_v4.cpp
unit_ip_network_v6_SOURCES = unit/ip/network_v6.cpp
unit_ip_resolver_query_base_SOURCES = unit/ip/resolver_query_base.cpp
unit_ip_resolver_service_SOURCES = unit/ip/resolver_service.cpp
unit_ip_tcp_SOURCES = unit/ip/tcp.cpp
unit_ip_udp_SOURCES = unit/ip/udp.cpp
unit_ip_unicast_SOURCES = unit/ip/unicast.cpp
unit_ip_v6_only_SOURCES = unit/ip/v6_only.cpp
unit_is_read_buffered_SOURCES = unit/is_read_buffered.cpp
unit_is_write_buffered_SOURCES = unit/is_write_buffered.cpp
unit_local_basic_endpoint_SOURCES = unit/local/basic_endpoint.cpp
unit_local_connect_pair_SOURCES = unit/local/connect_pair.cpp
unit_local_datagram_protocol_SOURCES = unit/local/datagram_protocol.cpp
unit_local_stream_protocol_SOURCES = unit/local/stream_protocol.cpp
unit_packaged_task_SOURCES = unit/packaged_task.cpp
unit_placeholders_SOURCES = unit/placeholders.cpp
unit_posix_basic_descriptor_SOURCES = unit/posix/basic_descriptor.cpp
unit_posix_basic_stream_descriptor_SOURCES = unit/posix/basic_stream_descriptor.cpp
unit_posix_descriptor_SOURCES = unit/posix/descriptor.cpp
unit_posix_descriptor_base_SOURCES = unit/posix/descriptor_base.cpp
unit_posix_stream_descriptor_SOURCES = unit/posix/stream_descriptor.cpp
unit_posix_stream_descriptor_service_SOURCES = unit/posix/stream_descriptor_service.cpp
unit_post_SOURCES = unit/post.cpp
unit_raw_socket_service_SOURCES = unit/raw_socket_service.cpp
unit_read_SOURCES = unit/read.cpp
unit_read_at_SOURCES = unit/read_at.cpp
unit_read_until_SOURCES = unit/read_until.cpp
unit_seq_packet_socket_service_SOURCES = unit/seq_packet_socket_service.cpp
unit_serial_port_SOURCES = unit/serial_port.cpp
unit_serial_port_base_SOURCES = unit/serial_port_base.cpp
unit_serial_port_service_SOURCES = unit/serial_port_service.cpp
unit_signal_set_SOURCES = unit/signal_set.cpp
unit_signal_set_service_SOURCES = unit/signal_set_service.cpp
unit_socket_acceptor_service_SOURCES = unit/socket_acceptor_service.cpp
unit_socket_base_SOURCES = unit/socket_base.cpp
unit_steady_timer_SOURCES = unit/steady_timer.cpp
unit_strand_SOURCES = unit/strand.cpp
unit_stream_socket_service_SOURCES = unit/stream_socket_service.cpp
unit_streambuf_SOURCES = unit/streambuf.cpp
unit_system_context_SOURCES = unit/system_context.cpp
unit_system_executor_SOURCES = unit/system_executor.cpp
unit_system_timer_SOURCES = unit/system_timer.cpp
unit_thread_SOURCES = unit/thread.cpp
unit_time_traits_SOURCES = unit/time_traits.cpp
unit_ts_buffer_SOURCES = unit/ts/buffer.cpp
unit_ts_executor_SOURCES = unit/ts/executor.cpp
unit_ts_internet_SOURCES = unit/ts/internet.cpp
unit_ts_io_context_SOURCES = unit/ts/io_context.cpp
unit_ts_net_SOURCES = unit/ts/net.cpp
unit_ts_netfwd_SOURCES = unit/ts/netfwd.cpp
unit_ts_socket_SOURCES = unit/ts/socket.cpp
unit_ts_timer_SOURCES = unit/ts/timer.cpp
unit_use_future_SOURCES = unit/use_future.cpp
unit_uses_executor_SOURCES = unit/uses_executor.cpp
unit_wait_traits_SOURCES = unit/wait_traits.cpp
unit_waitable_timer_service_SOURCES = unit/waitable_timer_service.cpp
unit_windows_basic_handle_SOURCES = unit/windows/basic_handle.cpp
unit_windows_basic_object_handle_SOURCES = unit/windows/basic_object_handle.cpp
unit_windows_basic_random_access_handle_SOURCES = unit/windows/basic_random_access_handle.cpp
unit_windows_basic_stream_handle_SOURCES = unit/windows/basic_stream_handle.cpp
unit_windows_object_handle_SOURCES = unit/windows/object_handle.cpp
unit_windows_object_handle_service_SOURCES = unit/windows/object_handle_service.cpp
unit_windows_overlapped_handle_SOURCES = unit/windows/overlapped_handle.cpp
unit_windows_overlapped_ptr_SOURCES = unit/windows/overlapped_ptr.cpp
unit_windows_random_access_handle_SOURCES = unit/windows/random_access_handle.cpp
unit_windows_random_access_handle_service_SOURCES = unit/windows/random_access_handle_service.cpp
unit_windows_stream_handle_SOURCES = unit/windows/stream_handle.cpp
unit_windows_stream_handle_service_SOURCES = unit/windows/stream_handle_service.cpp
unit_write_SOURCES = unit/write.cpp
unit_write_at_SOURCES = unit/write_at.cpp

if HAVE_OPENSSL
unit_ssl_context_base_SOURCES = unit/ssl/context_base.cpp
unit_ssl_context_SOURCES = unit/ssl/context.cpp
unit_ssl_error_SOURCES = unit/ssl/error.cpp
unit_ssl_stream_base_SOURCES = unit/ssl/stream_base.cpp
unit_ssl_rfc2818_verification_SOURCES = unit/ssl/rfc2818_verification.cpp
unit_ssl_stream_SOURCES = unit/ssl/stream.cpp
endif

EXTRA_DIST = \
	latency/allocator.hpp \
	performance/handler_allocator.hpp \
	unit/archetypes/async_ops.hpp \
	unit/archetypes/async_result.hpp \
	unit/archetypes/deprecated_async_result.hpp \
	unit/archetypes/deprecated_async_ops.hpp \
	unit/archetypes/gettable_socket_option.hpp \
	unit/archetypes/io_control_command.hpp \
	unit/archetypes/settable_socket_option.hpp

MAINTAINERCLEANFILES = \
	$(srcdir)/Makefile.in
