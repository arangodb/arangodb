//          Copyright Nat Goodspeed 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cassert>
#include <chrono>
#include <exception>
#include <iostream>
#include <sstream>
#include <thread>
#include <tuple>                    // std::tie()

#include <boost/context/detail/apply.hpp>
#include <boost/fiber/all.hpp>

#if defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
/*****************************************************************************
*   helper code to help callback 
*****************************************************************************/
template< typename Fn, typename ... Args >
class helper {
private:
    typename std::decay< Fn >::type                     fn_;
    std::tuple< typename std::decay< Args >::type ... > args_;

public:
    helper( Fn && fn, Args && ... args) :
        fn_( std::forward< Fn >( fn) ),
        args_( std::make_tuple( std::forward< Args >( args) ... ) ) {
    }

    helper( helper && other) = default;
    helper & operator=( helper && other) = default;

    helper( helper const&) = default;
    helper & operator=( helper const&) = default;

    void operator()() {
        boost::context::detail::apply( fn_, args_);
    }
};

template< typename Fn, typename ... Args  >
helper< Fn, Args ... > help( Fn && fn, Args && ... args) {
    return helper< Fn, Args ... >( std::forward< Fn >( fn), std::forward< Args >( args) ... );
}
#endif

/*****************************************************************************
*   example async API
*****************************************************************************/
//[AsyncAPI
class AsyncAPI {
public:
    // constructor acquires some resource that can be read and written
    AsyncAPI();

    // callbacks accept an int error code; 0 == success
    typedef int errorcode;

    // write callback only needs to indicate success or failure
    template< typename Fn >
    void init_write( std::string const& data, Fn && callback);

    // read callback needs to accept both errorcode and data
    template< typename Fn >
    void init_read( Fn && callback);

    // ... other operations ...
//<-
    void inject_error( errorcode ec);

private:
    std::string data_;
    errorcode   injected_;
//->
};
//]

/*****************************************************************************
*   fake AsyncAPI implementation... pay no attention to the little man behind
*   the curtain...
*****************************************************************************/
AsyncAPI::AsyncAPI() :
    injected_( 0) {
}

void AsyncAPI::inject_error( errorcode ec) {
    injected_ = ec;
}

template< typename Fn >
void AsyncAPI::init_write( std::string const& data, Fn && callback) {
    // make a local copy of injected_
    errorcode injected( injected_);
    // reset it synchronously with caller
    injected_ = 0;
    // update data_ (this might be an echo service)
    if ( ! injected) {
        data_ = data;
    }
    // Simulate an asynchronous I/O operation by launching a detached thread
    // that sleeps a bit before calling completion callback. Echo back to
    // caller any previously-injected errorcode.
#if ! defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    std::thread( [injected,callback=std::forward< Fn >( callback)]() mutable {
        std::this_thread::sleep_for( std::chrono::milliseconds(100) );
        callback( injected);
    }).detach();
#else
    std::thread(
        std::move(
            help( std::forward< Fn >( callback), injected) ) ).detach();
#endif
}

template< typename Fn >
void AsyncAPI::init_read( Fn && callback) {
    // make a local copy of injected_
    errorcode injected( injected_);
    // reset it synchronously with caller
    injected_ = 0;
    // local copy of data_ so we can capture in lambda
    std::string data( data_);
    // Simulate an asynchronous I/O operation by launching a detached thread
    // that sleeps a bit before calling completion callback. Echo back to
    // caller any previously-injected errorcode.
#if ! defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    std::thread( [injected,callback=std::forward< Fn >( callback),data]() mutable {
        std::this_thread::sleep_for( std::chrono::milliseconds(100) );
        callback( injected, data);
    }).detach();
#else
    std::thread(
        std::move(
            help( std::forward< Fn >( callback), injected, data) ) ).detach();
#endif
}

/*****************************************************************************
*   adapters
*****************************************************************************/
// helper function used in a couple of the adapters
std::runtime_error make_exception( std::string const& desc, AsyncAPI::errorcode);

//[callbacks_write_ec
AsyncAPI::errorcode write_ec( AsyncAPI & api, std::string const& data) {
    boost::fibers::promise< AsyncAPI::errorcode > promise;
    boost::fibers::future< AsyncAPI::errorcode > future( promise.get_future() );
    // In general, even though we block waiting for future::get() and therefore
    // won't destroy 'promise' until promise::set_value() has been called, we
    // are advised that with threads it's possible for ~promise() to be
    // entered before promise::set_value() has returned. While that shouldn't
    // happen with fibers::promise, a robust way to deal with the lifespan
    // issue is to bind 'promise' into our lambda. Since promise is move-only,
    // use initialization capture.
#if ! defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    api.init_write(
        data,
        [promise=std::move( promise)]( AsyncAPI::errorcode ec) mutable {
                            promise.set_value( ec);
                  });

#else // defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    api.init_write(
        data,
        std::bind([](boost::fibers::promise< AsyncAPI::errorcode > & promise,
                     AsyncAPI::errorcode ec) {
            promise.set_value( ec);
        },
        std::move( promise),
        std::placeholders::_1) );
#endif // BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES

    return future.get();
}
//]

//[callbacks_write
void write( AsyncAPI & api, std::string const& data) {
    AsyncAPI::errorcode ec = write_ec( api, data);
    if ( ec) {
        throw make_exception("write", ec);
    }
}
//]

//[callbacks_read_ec
std::pair< AsyncAPI::errorcode, std::string > read_ec( AsyncAPI & api) {
    typedef std::pair< AsyncAPI::errorcode, std::string > result_pair;
    boost::fibers::promise< result_pair > promise;
    boost::fibers::future< result_pair > future( promise.get_future() );
    // We promise that both 'promise' and 'future' will survive until our
    // lambda has been called.
#if ! defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    api.init_read([promise=std::move( promise)]( AsyncAPI::errorcode ec, std::string const& data) mutable {
                            promise.set_value( result_pair( ec, data) );
                  });
#else // defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    api.init_read(
            std::bind([]( boost::fibers::promise< result_pair > & promise,
                          AsyncAPI::errorcode ec, std::string const& data) mutable {
                            promise.set_value( result_pair( ec, data) );
                  },
                  std::move( promise),
                  std::placeholders::_1,
                  std::placeholders::_2) );
#endif // BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES
    return future.get();
}
//]

//[callbacks_read
std::string read( AsyncAPI & api) {
    boost::fibers::promise< std::string > promise;
    boost::fibers::future< std::string > future( promise.get_future() );
    // Both 'promise' and 'future' will survive until our lambda has been
    // called.
#if ! defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    api.init_read([&promise]( AsyncAPI::errorcode ec, std::string const& data) mutable {
                           if ( ! ec) {
                               promise.set_value( data);
                           } else {
                               promise.set_exception(
                                       std::make_exception_ptr(
                                           make_exception("read", ec) ) );
                           }
                  });
#else // defined(BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES)
    api.init_read(
            std::bind([]( boost::fibers::promise< std::string > & promise,
                          AsyncAPI::errorcode ec, std::string const& data) mutable {
                           if ( ! ec) {
                               promise.set_value( data);
                           } else {
                               promise.set_exception(
                                       std::make_exception_ptr(
                                           make_exception("read", ec) ) );
                           }
                  },
                  std::move( promise),
                  std::placeholders::_1,
                  std::placeholders::_2) );
#endif // BOOST_NO_CXX14_INITIALIZED_LAMBDA_CAPTURES
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
int main( int argc, char *argv[]) {
    AsyncAPI api;

    // successful write(): prime AsyncAPI with some data
    write( api, "abcd");
    // successful read(): retrieve it
    std::string data( read( api) );
    assert( data == "abcd");

    // successful write_ec()
    AsyncAPI::errorcode ec( write_ec( api, "efgh") );
    assert( ec == 0);

    // write_ec() with error
    api.inject_error(1);
    ec = write_ec( api, "ijkl");
    assert( ec == 1);

    // write() with error
    std::string thrown;
    api.inject_error(2);
    try {
        write(api, "mnop");
    } catch ( std::exception const& e) {
        thrown = e.what();
    }
    assert( thrown == make_exception("write", 2).what() );

    // successful read_ec()
//[callbacks_read_ec_call
    std::tie( ec, data) = read_ec( api);
//]
    assert( ! ec);
    assert( data == "efgh");         // last successful write_ec()

    // read_ec() with error
    api.inject_error(3);
    std::tie( ec, data) = read_ec( api);
    assert( ec == 3);
    // 'data' in unspecified state, don't test

    // read() with error
    thrown.clear();
    api.inject_error(4);
    try {
        data = read(api);
    } catch ( std::exception const& e) {
        thrown = e.what();
    }
    assert( thrown == make_exception("read", 4).what() );

    std::cout << "done." << std::endl;

    return EXIT_SUCCESS;
}
