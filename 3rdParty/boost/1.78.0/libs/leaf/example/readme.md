# Example Programs Using LEAF to Handle Errors

* [print_file](./print_file): The complete example from the [Five Minute Introduction](https://boostorg.github.io/leaf/#introduction). This directory contains several versions of the same program, each using a different error handling style.

* [capture_in_result.cpp](https://github.com/boostorg/leaf/blob/master/example/capture_in_result.cpp?ts=4): Shows how to transport error objects between threads in a `leaf::result<T>` object.
* [capture_in_exception.cpp](https://github.com/boostorg/leaf/blob/master/example/capture_in_exception.cpp?ts=4): Shows how to transport error objects between threads in an exception object.
* [lua_callback_result.cpp](https://github.com/boostorg/leaf/blob/master/example/lua_callback_result.cpp?ts=4): Transporting arbitrary error objects through an uncooperative C API.
* [lua_callback_eh.cpp](https://github.com/boostorg/leaf/blob/master/example/lua_callback_eh.cpp?ts=4): Transporting arbitrary error objects through an uncooperative exception-safe API.
* [exception_to_result.cpp](https://github.com/boostorg/leaf/blob/master/example/exception_to_result.cpp?ts=4): Demonstrates how to transport exceptions through a `noexcept` layer in the program.
* [exception_error_log.cpp](https://github.com/boostorg/leaf/blob/master/example/error_log.cpp?ts=4): Using `accumulate` to produce an error log.
* [exception_error_trace.cpp](https://github.com/boostorg/leaf/blob/master/example/error_trace.cpp?ts=4): Same as above, but the log is recorded in a `std::deque` rather than just printed.
* [print_half.cpp](https://github.com/boostorg/leaf/blob/master/example/print_half.cpp?ts=4): This is a Boost Outcome example adapted to LEAF, demonstrating the use of `try_handle_some` to handle some errors, forwarding any other error to the caller.
* [asio_beast_leaf_rpc.cpp](https://github.com/boostorg/leaf/blob/master/example/asio_beast_leaf_rpc.cpp?ts=4): A simple RPC calculator implemented with Beast+ASIO+LEAF.
