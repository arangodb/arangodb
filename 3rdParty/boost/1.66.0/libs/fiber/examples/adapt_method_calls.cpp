//          Copyright Nat Goodspeed 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/fiber/all.hpp>
#include <memory>                   // std::shared_ptr
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>
#include <exception>
#include <cassert>

/*****************************************************************************
*   example async API
*****************************************************************************/
// introduce class-scope typedef
struct AsyncAPIBase {
    // error callback accepts an int error code; 0 == success
    typedef int errorcode;
};

//[Response
// every async operation receives a subclass instance of this abstract base
// class through which to communicate its result
struct Response {
    typedef std::shared_ptr< Response > ptr;

    // called if the operation succeeds
    virtual void success( std::string const& data) = 0;

    // called if the operation fails
    virtual void error( AsyncAPIBase::errorcode ec) = 0;
};
//]

// the actual async API
class AsyncAPI: public AsyncAPIBase {
public:
    // constructor acquires some resource that can be read
    AsyncAPI( std::string const& data);

//[method_init_read
    // derive Response subclass, instantiate, pass Response::ptr
    void init_read( Response::ptr);
//]

    // ... other operations ...
    void inject_error( errorcode ec);

private:
    std::string data_;
    errorcode   injected_;
};

/*****************************************************************************
*   fake AsyncAPI implementation... pay no attention to the little man behind
*   the curtain...
*****************************************************************************/
AsyncAPI::AsyncAPI( std::string const& data) :
    data_( data),
    injected_( 0) {
}

void AsyncAPI::inject_error( errorcode ec) {
    injected_ = ec;
}

void AsyncAPI::init_read( Response::ptr response) {
    // make a local copy of injected_
    errorcode injected( injected_);
    // reset it synchronously with caller
    injected_ = 0;
    // local copy of data_ so we can capture in lambda
    std::string data( data_);
    // Simulate an asynchronous I/O operation by launching a detached thread
    // that sleeps a bit before calling either completion method.
    std::thread( [injected, response, data](){
                     std::this_thread::sleep_for( std::chrono::milliseconds(100) );
                     if ( ! injected) {
                         // no error, call success()
                         response->success( data);
                     } else {
                         // injected error, call error()
                         response->error( injected);
                     }            
                 }).detach();
}

/*****************************************************************************
*   adapters
*****************************************************************************/
// helper function
std::runtime_error make_exception( std::string const& desc, AsyncAPI::errorcode);

//[PromiseResponse
class PromiseResponse: public Response {
public:
    // called if the operation succeeds
    virtual void success( std::string const& data) {
        promise_.set_value( data);
    }

    // called if the operation fails
    virtual void error( AsyncAPIBase::errorcode ec) {
        promise_.set_exception(
                std::make_exception_ptr(
                    make_exception("read", ec) ) );
    }

    boost::fibers::future< std::string > get_future() {
        return promise_.get_future();
    }

private:
    boost::fibers::promise< std::string >   promise_;
};
//]

//[method_read
std::string read( AsyncAPI & api) {
    // Because init_read() requires a shared_ptr, we must allocate our
    // ResponsePromise on the heap, even though we know its lifespan.
    auto promisep( std::make_shared< PromiseResponse >() );
    boost::fibers::future< std::string > future( promisep->get_future() );
    // Both 'promisep' and 'future' will survive until our lambda has been
    // called.
    api.init_read( promisep);
    return future.get();
}
//]

/*****************************************************************************
*   helpers
*****************************************************************************/
std::runtime_error make_exception( std::string const& desc, AsyncAPI::errorcode ec) {
    std::ostringstream buffer;
    buffer << "Error in AsyncAPI::" << desc << "(): " << ec;
    return std::runtime_error( buffer.str() );
}

/*****************************************************************************
*   driving logic
*****************************************************************************/
int main(int argc, char *argv[]) {
    // prime AsyncAPI with some data
    AsyncAPI api("abcd");

    // successful read(): retrieve it
    std::string data( read( api) );
    assert(data == "abcd");

    // read() with error
    std::string thrown;
    api.inject_error(1);
    try {
        data = read( api);
    } catch ( std::exception const& e) {
        thrown = e.what();
    }
    assert(thrown == make_exception("read", 1).what() );

    std::cout << "done." << std::endl;

    return EXIT_SUCCESS;
}
