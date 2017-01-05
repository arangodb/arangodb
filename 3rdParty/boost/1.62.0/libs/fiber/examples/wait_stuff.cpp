//          Copyright Nat Goodspeed 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/fiber/all.hpp>
#include <boost/noncopyable.hpp>
#include <boost/variant/variant.hpp>
#include <boost/variant/get.hpp>

// These are wait_something() functions rather than when_something()
// functions. A big part of the point of the Fiber library is to model
// sequencing using the processor's instruction pointer rather than chains of
// callbacks. The future-oriented when_all() / when_any() functions are still
// based on chains of callbacks. With Fiber, we can do better.

/*****************************************************************************
*   Verbose
*****************************************************************************/
class Verbose: boost::noncopyable {
public:
    Verbose( std::string const& d):
        desc( d) {
        std::cout << desc << " start" << std::endl;
    }

    ~Verbose() {
        std::cout << desc << " stop" << std::endl;
    }

private:
    const std::string desc;
};

/*****************************************************************************
*   Runner and Example
*****************************************************************************/
// collect and ultimately run every Example
class Runner {
    typedef std::vector< std::pair< std::string, std::function< void() > > >    function_list;

public:
    void add( std::string const& desc, std::function< void() > const& func) {
        functions_.push_back( function_list::value_type( desc, func) );
    }

    void run() {
        for ( function_list::value_type const& pair : functions_) {
            Verbose v( pair.first);
            pair.second();
        }
    }

private:
    function_list   functions_;
};

Runner runner;

// Example allows us to embed Runner::add() calls at module scope
struct Example {
    Example( Runner & runner, std::string const& desc, std::function< void() > const& func) {
        runner.add( desc, func);
    }
};

/*****************************************************************************
*   example task functions
*****************************************************************************/
//[wait_sleeper
template< typename T >
T sleeper_impl( T item, int ms, bool thrw = false) {
    std::ostringstream descb, funcb;
    descb << item;
    std::string desc( descb.str() );
    funcb << "  sleeper(" << item << ")";
    Verbose v( funcb.str() );

    boost::this_fiber::sleep_for( std::chrono::milliseconds( ms) );
    if ( thrw) {
        throw std::runtime_error( desc);
    }
    return item;
}
//]

inline
std::string sleeper( std::string const& item, int ms, bool thrw = false) {
    return sleeper_impl( item, ms, thrw);
}

inline
double sleeper( double item, int ms, bool thrw = false) {
    return sleeper_impl( item, ms, thrw);
}

inline
int sleeper(int item, int ms, bool thrw = false) {
    return sleeper_impl( item, ms, thrw);
}

/*****************************************************************************
*   Done
*****************************************************************************/
//[wait_done
// Wrap canonical pattern for condition_variable + bool flag
struct Done {
private:
    boost::fibers::condition_variable   cond;
    boost::fibers::mutex                mutex;
    bool                                ready = false;

public:
    typedef std::shared_ptr< Done >     ptr;

    void wait() {
        std::unique_lock< boost::fibers::mutex > lock( mutex);
        cond.wait( lock, [this](){ return ready; });
    }

    void notify() {
        {
            std::unique_lock< boost::fibers::mutex > lock( mutex);
            ready = true;
        } // release mutex
        cond.notify_one();
    }
};
//]

/*****************************************************************************
*   when_any, simple completion
*****************************************************************************/
//[wait_first_simple_impl
// Degenerate case: when there are no functions to wait for, return
// immediately.
void wait_first_simple_impl( Done::ptr) {
}

// When there's at least one function to wait for, launch it and recur to
// process the rest.
template< typename Fn, typename ... Fns >
void wait_first_simple_impl( Done::ptr done, Fn && function, Fns && ... functions) {
    boost::fibers::fiber( [done, function](){
                              function();
                              done->notify();
                          }).detach();
    wait_first_simple_impl( done, std::forward< Fns >( functions) ... );
}
//]

// interface function: instantiate Done, launch tasks, wait for Done
//[wait_first_simple
template< typename ... Fns >
void wait_first_simple( Fns && ... functions) {
    // Use shared_ptr because each function's fiber will bind it separately,
    // and we're going to return before the last of them completes.
    auto done( std::make_shared< Done >() );
    wait_first_simple_impl( done, std::forward< Fns >( functions) ... );
    done->wait();
}
//]

// example usage
Example wfs( runner, "wait_first_simple()", [](){
//[wait_first_simple_ex
    wait_first_simple(
            [](){ sleeper("wfs_long",   150); },
            [](){ sleeper("wfs_medium", 100); },
            [](){ sleeper("wfs_short",   50); });
//]
});

/*****************************************************************************
*   when_any, return value
*****************************************************************************/
// When there's only one function, call this overload
//[wait_first_value_impl
template< typename T, typename Fn >
void wait_first_value_impl( std::shared_ptr< boost::fibers::unbounded_channel< T > > channel,
                            Fn && function) {
    boost::fibers::fiber( [channel, function](){
                              // Ignore channel_op_status returned by push():
                              // might be closed; we simply don't care.
                              channel->push( function() );
                          }).detach();
}
//]

// When there are two or more functions, call this overload
template< typename T, typename Fn0, typename Fn1, typename ... Fns >
void wait_first_value_impl( std::shared_ptr< boost::fibers::unbounded_channel< T > > channel,
                            Fn0 && function0,
                            Fn1 && function1,
                            Fns && ... functions) {
    // process the first function using the single-function overload
    wait_first_value_impl< T >( channel,
                                std::forward< Fn0 >( function0) );
    // then recur to process the rest
    wait_first_value_impl< T >( channel,
                                std::forward< Fn1 >( function1),
                                std::forward< Fns >( functions) ... );
}

//[wait_first_value
// Assume that all passed functions have the same return type. The return type
// of wait_first_value() is the return type of the first passed function. It is
// simply invalid to pass NO functions.
template< typename Fn, typename ... Fns >
typename std::result_of< Fn() >::type
wait_first_value( Fn && function, Fns && ... functions) {
    typedef typename std::result_of< Fn() >::type return_t;
    typedef boost::fibers::unbounded_channel< return_t > channel_t;
    auto channelp( std::make_shared< channel_t >() );
    // launch all the relevant fibers
    wait_first_value_impl< return_t >( channelp,
                                       std::forward< Fn >( function),
                                       std::forward< Fns >( functions) ... );
    // retrieve the first value
    return_t value( channelp->value_pop() );
    // close the channel: no subsequent push() has to succeed
    channelp->close();
    return value;
}
//]

// example usage
Example wfv( runner, "wait_first_value()", [](){
//[wait_first_value_ex
    std::string result = wait_first_value(
            [](){ return sleeper("wfv_third",  150); },
            [](){ return sleeper("wfv_second", 100); },
            [](){ return sleeper("wfv_first",   50); });
    std::cout << "wait_first_value() => " << result << std::endl;
    assert(result == "wfv_first");
//]
});

/*****************************************************************************
*   when_any, produce first outcome, whether result or exception
*****************************************************************************/
// When there's only one function, call this overload.
//[wait_first_outcome_impl
template< typename T, typename CHANNELP, typename Fn >
void wait_first_outcome_impl( CHANNELP channel, Fn && function) {
    boost::fibers::fiber(
        // Use std::bind() here for C++11 compatibility. C++11 lambda capture
        // can't move a move-only Fn type, but bind() can. Let bind() move the
        // channel pointer and the function into the bound object, passing
        // references into the lambda.
        std::bind(
            []( CHANNELP & channel,
                typename std::decay< Fn >::type & function) {
                // Instantiate a packaged_task to capture any exception thrown by
                // function.
                boost::fibers::packaged_task< T() > task( function);
                // Immediately run this packaged_task on same fiber. We want
                // function() to have completed BEFORE we push the future.
                task();
                // Pass the corresponding future to consumer. Ignore
                // channel_op_status returned by push(): might be closed; we
                // simply don't care.
                channel->push( task.get_future() );
            },
            channel,
            std::forward< Fn >( function)
        )).detach();
}
//]

// When there are two or more functions, call this overload
template< typename T, typename CHANNELP, typename Fn0, typename Fn1, typename ... Fns >
void wait_first_outcome_impl( CHANNELP channel,
                              Fn0 && function0,
                              Fn1 && function1,
                              Fns && ... functions) {
    // process the first function using the single-function overload
    wait_first_outcome_impl< T >( channel,
                                  std::forward< Fn0 >( function0) );
    // then recur to process the rest
    wait_first_outcome_impl< T >( channel,
                                  std::forward< Fn1 >( function1),
                                  std::forward< Fns >( functions) ... );
}

// Assume that all passed functions have the same return type. The return type
// of wait_first_outcome() is the return type of the first passed function. It is
// simply invalid to pass NO functions.
//[wait_first_outcome
template< typename Fn, typename ... Fns >
typename std::result_of< Fn() >::type
wait_first_outcome( Fn && function, Fns && ... functions) {
    // In this case, the value we pass through the channel is actually a
    // future -- which is already ready. future can carry either a value or an
    // exception.
    typedef typename std::result_of< Fn() >::type return_t;
    typedef boost::fibers::future< return_t > future_t;
    typedef boost::fibers::unbounded_channel< future_t > channel_t;
    auto channelp(std::make_shared< channel_t >() );
    // launch all the relevant fibers
    wait_first_outcome_impl< return_t >( channelp,
                                         std::forward< Fn >( function),
                                         std::forward< Fns >( functions) ... );
    // retrieve the first future
    future_t future( channelp->value_pop() );
    // close the channel: no subsequent push() has to succeed
    channelp->close();
    // either return value or throw exception
    return future.get();
}
//]

// example usage
Example wfo( runner, "wait_first_outcome()", [](){
//[wait_first_outcome_ex
    std::string result = wait_first_outcome(
            [](){ return sleeper("wfos_first",   50); },
            [](){ return sleeper("wfos_second", 100); },
            [](){ return sleeper("wfos_third",  150); });
    std::cout << "wait_first_outcome(success) => " << result << std::endl;
    assert(result == "wfos_first");

    std::string thrown;
    try {
        result = wait_first_outcome(
                [](){ return sleeper("wfof_first",   50, true); },
                [](){ return sleeper("wfof_second", 100); },
                [](){ return sleeper("wfof_third",  150); });
    } catch ( std::exception const& e) {
        thrown = e.what();
    }
    std::cout << "wait_first_outcome(fail) threw '" << thrown
              << "'" << std::endl;
    assert(thrown == "wfof_first");
//]
});

/*****************************************************************************
*   when_any, collect exceptions until success; throw exception_list if no
*   success
*****************************************************************************/
// define an exception to aggregate exception_ptrs; prefer
// std::exception_list (N4407 et al.) once that becomes available
//[exception_list
class exception_list : public std::runtime_error {
public:
    exception_list( std::string const& what) :
        std::runtime_error( what) {
    }

    typedef std::vector< std::exception_ptr >   bundle_t;

    // N4407 proposed std::exception_list API
    typedef bundle_t::const_iterator iterator;

    std::size_t size() const noexcept {
        return bundle_.size();
    }

    iterator begin() const noexcept {
        return bundle_.begin();
    }

    iterator end() const noexcept {
        return bundle_.end();
    }

    // extension to populate
    void add( std::exception_ptr ep) {
        bundle_.push_back( ep);
    }

private:
    bundle_t bundle_;
};
//]

// Assume that all passed functions have the same return type. The return type
// of wait_first_success() is the return type of the first passed function. It is
// simply invalid to pass NO functions.
//[wait_first_success
template< typename Fn, typename ... Fns >
typename std::result_of< Fn() >::type
wait_first_success( Fn && function, Fns && ... functions) {
    std::size_t count( 1 + sizeof ... ( functions) );
    // In this case, the value we pass through the channel is actually a
    // future -- which is already ready. future can carry either a value or an
    // exception.
    typedef typename std::result_of< typename std::decay< Fn >::type() >::type return_t;
    typedef boost::fibers::future< return_t > future_t;
    typedef boost::fibers::unbounded_channel< future_t > channel_t;
    auto channelp( std::make_shared< channel_t >() );
    // launch all the relevant fibers
    wait_first_outcome_impl< return_t >( channelp,
                                         std::forward< Fn >( function),
                                         std::forward< Fns >( functions) ... );
    // instantiate exception_list, just in case
    exception_list exceptions("wait_first_success() produced only errors");
    // retrieve up to 'count' results -- but stop there!
    for ( std::size_t i = 0; i < count; ++i) {
        // retrieve the next future
        future_t future( channelp->value_pop() );
        // retrieve exception_ptr if any
        std::exception_ptr error( future.get_exception_ptr() );
        // if no error, then yay, return value
        if ( ! error) {
            // close the channel: no subsequent push() has to succeed
            channelp->close();
            // show caller the value we got
            return future.get();
        }

        // error is non-null: collect
        exceptions.add( error);
    }
    // We only arrive here when every passed function threw an exception.
    // Throw our collection to inform caller.
    throw exceptions;
}
//]

// example usage
Example wfss( runner, "wait_first_success()", [](){
//[wait_first_success_ex
    std::string result = wait_first_success(
                [](){ return sleeper("wfss_first",   50, true); },
                [](){ return sleeper("wfss_second", 100); },
                [](){ return sleeper("wfss_third",  150); });
    std::cout << "wait_first_success(success) => " << result << std::endl;
    assert(result == "wfss_second");
//]

    std::string thrown;
    std::size_t count = 0;
    try {
        result = wait_first_success(
                [](){ return sleeper("wfsf_first",   50, true); },
                [](){ return sleeper("wfsf_second", 100, true); },
                [](){ return sleeper("wfsf_third",  150, true); });
    } catch ( exception_list const& e) {
        thrown = e.what();
        count = e.size();
    } catch ( std::exception const& e) {
        thrown = e.what();
    }
    std::cout << "wait_first_success(fail) threw '" << thrown << "': "
              << count << " errors" << std::endl;
    assert(thrown == "wait_first_success() produced only errors");
    assert(count == 3);
});

/*****************************************************************************
*   when_any, heterogeneous
*****************************************************************************/
//[wait_first_value_het
// No need to break out the first Fn for interface function: let the compiler
// complain if empty.
// Our functions have different return types, and we might have to return any
// of them. Use a variant, expanding std::result_of<Fn()>::type for each Fn in
// parameter pack.
template< typename ... Fns >
boost::variant< typename std::result_of< Fns() >::type ... >
wait_first_value_het( Fns && ... functions) {
    // Use unbounded_channel<boost::variant<T1, T2, ...>>; see remarks above.
    typedef boost::variant< typename std::result_of< Fns() >::type ... > return_t;
    typedef boost::fibers::unbounded_channel< return_t > channel_t;
    auto channelp( std::make_shared< channel_t >() );
    // launch all the relevant fibers
    wait_first_value_impl< return_t >( channelp,
                                       std::forward< Fns >( functions) ... );
    // retrieve the first value
    return_t value( channelp->value_pop() );
    // close the channel: no subsequent push() has to succeed
    channelp->close();
    return value;
}
//]

// example usage
Example wfvh( runner, "wait_first_value_het()", [](){
//[wait_first_value_het_ex
    boost::variant< std::string, double, int > result =
        wait_first_value_het(
                [](){ return sleeper("wfvh_third",  150); },
                [](){ return sleeper(3.14,          100); },
                [](){ return sleeper(17,             50); });
    std::cout << "wait_first_value_het() => " << result << std::endl;
    assert(boost::get< int >( result) == 17);
//]
});

/*****************************************************************************
*   when_all, simple completion
*****************************************************************************/
// Degenerate case: when there are no functions to wait for, return
// immediately.
void wait_all_simple_impl( std::shared_ptr< boost::fibers::barrier >) {
}

// When there's at least one function to wait for, launch it and recur to
// process the rest.
//[wait_all_simple_impl
template< typename Fn, typename ... Fns >
void wait_all_simple_impl( std::shared_ptr< boost::fibers::barrier > barrier,
                           Fn && function, Fns && ... functions) {
    boost::fibers::fiber(
            std::bind(
                []( std::shared_ptr< boost::fibers::barrier > & barrier,
                    typename std::decay< Fn >::type & function) mutable {
                        function();
                        barrier->wait();
                },
                barrier,
                std::forward< Fn >( function)
            )).detach();
    wait_all_simple_impl( barrier, std::forward< Fns >( functions) ... );
}
//]

// interface function: instantiate barrier, launch tasks, wait for barrier
//[wait_all_simple
template< typename ... Fns >
void wait_all_simple( Fns && ... functions) {
    std::size_t count( sizeof ... ( functions) );
    // Initialize a barrier(count+1) because we'll immediately wait on it. We
    // don't want to wake up until 'count' more fibers wait on it. Even though
    // we'll stick around until the last of them completes, use shared_ptr
    // anyway because it's easier to be confident about lifespan issues.
    auto barrier( std::make_shared< boost::fibers::barrier >( count + 1) );
    wait_all_simple_impl( barrier, std::forward< Fns >( functions) ... );
    barrier->wait();
}
//]

// example usage
Example was( runner, "wait_all_simple()", [](){
//[wait_all_simple_ex
    wait_all_simple(
            [](){ sleeper("was_long",   150); },
            [](){ sleeper("was_medium", 100); },
            [](){ sleeper("was_short",   50); });
//]
});

/*****************************************************************************
*   when_all, return values
*****************************************************************************/
//[wait_nchannel
// Introduce a channel facade that closes the channel once a specific number
// of items has been pushed. This allows an arbitrary consumer to read until
// 'closed' without itself having to count items.
template< typename T >
class nchannel {
public:
    nchannel( std::shared_ptr< boost::fibers::unbounded_channel< T > > cp,
              std::size_t lm):
        channel_( cp),
        limit_( lm) {
        assert(channel_);
        if ( 0 == limit_) {
            channel_->close();
        }
    }

    boost::fibers::channel_op_status push( T && va) {
        boost::fibers::channel_op_status ok =
            channel_->push( std::forward< T >( va) );
        if ( ok == boost::fibers::channel_op_status::success &&
             --limit_ == 0) {
            // after the 'limit_'th successful push, close the channel
            channel_->close();
        }
        return ok;
    }

private:
    std::shared_ptr< boost::fibers::unbounded_channel< T > >    channel_;
    std::size_t                                                 limit_;
};
//]

// When there's only one function, call this overload
//[wait_all_values_impl
template< typename T, typename Fn >
void wait_all_values_impl( std::shared_ptr< nchannel< T > > channel,
                           Fn && function) {
    boost::fibers::fiber( [channel, function](){
                              channel->push(function());
                          }).detach();
}
//]

// When there are two or more functions, call this overload
template< typename T, typename Fn0, typename Fn1, typename ... Fns >
void wait_all_values_impl( std::shared_ptr< nchannel< T > > channel,
                           Fn0 && function0,
                           Fn1 && function1,
                           Fns && ... functions) {
    // process the first function using the single-function overload
    wait_all_values_impl< T >( channel, std::forward< Fn0 >( function0) );
    // then recur to process the rest
    wait_all_values_impl< T >( channel,
                               std::forward< Fn1 >( function1),
                               std::forward< Fns >( functions) ... );
}

//[wait_all_values_source
// Return a shared_ptr<unbounded_channel<T>> from which the caller can
// retrieve each new result as it arrives, until 'closed'.
template< typename Fn, typename ... Fns >
std::shared_ptr< boost::fibers::unbounded_channel< typename std::result_of< Fn() >::type > >
wait_all_values_source( Fn && function, Fns && ... functions) {
    std::size_t count( 1 + sizeof ... ( functions) );
    typedef typename std::result_of< Fn() >::type return_t;
    typedef boost::fibers::unbounded_channel< return_t > channel_t;
    // make the channel
    auto channelp( std::make_shared< channel_t >() );
    // and make an nchannel facade to close it after 'count' items
    auto ncp( std::make_shared< nchannel< return_t > >( channelp, count) );
    // pass that nchannel facade to all the relevant fibers
    wait_all_values_impl< return_t >( ncp,
                                      std::forward< Fn >( function),
                                      std::forward< Fns >( functions) ... );
    // then return the channel for consumer
    return channelp;
}
//]

// When all passed functions have completed, return vector<T> containing
// collected results. Assume that all passed functions have the same return
// type. It is simply invalid to pass NO functions.
//[wait_all_values
template< typename Fn, typename ... Fns >
std::vector< typename std::result_of< Fn() >::type >
wait_all_values( Fn && function, Fns && ... functions) {
    std::size_t count( 1 + sizeof ... ( functions) );
    typedef typename std::result_of< Fn() >::type return_t;
    typedef std::vector< return_t > vector_t;
    vector_t results;
    results.reserve( count);

    // get channel
    std::shared_ptr< boost::fibers::unbounded_channel< return_t > > channel =
        wait_all_values_source( std::forward< Fn >( function),
                                std::forward< Fns >( functions) ... );
    // fill results vector
    return_t value;
    while ( boost::fibers::channel_op_status::success == channel->pop(value) ) {
        results.push_back( value);
    }
    // return vector to caller
    return results;
}
//]

Example wav( runner, "wait_all_values()", [](){
//[wait_all_values_source_ex
    std::shared_ptr< boost::fibers::unbounded_channel< std::string > > channel =
        wait_all_values_source(
                [](){ return sleeper("wavs_third",  150); },
                [](){ return sleeper("wavs_second", 100); },
                [](){ return sleeper("wavs_first",   50); });
    std::string value;
    while ( boost::fibers::channel_op_status::success == channel->pop(value) ) {
        std::cout << "wait_all_values_source() => '" << value
                  << "'" << std::endl;
    }
//]

//[wait_all_values_ex
    std::vector< std::string > values =
        wait_all_values(
                [](){ return sleeper("wav_late",   150); },
                [](){ return sleeper("wav_middle", 100); },
                [](){ return sleeper("wav_early",   50); });
//]
    std::cout << "wait_all_values() =>";
    for ( std::string const& v : values) {
        std::cout << " '" << v << "'";
    }
    std::cout << std::endl;
});

/*****************************************************************************
*   when_all, throw first exception
*****************************************************************************/
//[wait_all_until_error_source
// Return a shared_ptr<unbounded_channel<future<T>>> from which the caller can
// get() each new result as it arrives, until 'closed'.
template< typename Fn, typename ... Fns >
std::shared_ptr<
    boost::fibers::unbounded_channel<
        boost::fibers::future<
            typename std::result_of< Fn() >::type > > >
wait_all_until_error_source( Fn && function, Fns && ... functions) {
    std::size_t count( 1 + sizeof ... ( functions) );
    typedef typename std::result_of< Fn() >::type return_t;
    typedef boost::fibers::future< return_t > future_t;
    typedef boost::fibers::unbounded_channel< future_t > channel_t;
    // make the channel
    auto channelp( std::make_shared< channel_t >() );
    // and make an nchannel facade to close it after 'count' items
    auto ncp( std::make_shared< nchannel< future_t > >( channelp, count) );
    // pass that nchannel facade to all the relevant fibers
    wait_first_outcome_impl< return_t >( ncp,
                                         std::forward< Fn >( function),
                                         std::forward< Fns >( functions) ... );
    // then return the channel for consumer
    return channelp;
}
//]

// When all passed functions have completed, return vector<T> containing
// collected results, or throw the first exception thrown by any of the passed
// functions. Assume that all passed functions have the same return type. It
// is simply invalid to pass NO functions.
//[wait_all_until_error
template< typename Fn, typename ... Fns >
std::vector< typename std::result_of< Fn() >::type >
wait_all_until_error( Fn && function, Fns && ... functions) {
    std::size_t count( 1 + sizeof ... ( functions) );
    typedef typename std::result_of< Fn() >::type return_t;
    typedef typename boost::fibers::future< return_t > future_t;
    typedef std::vector< return_t > vector_t;
    vector_t results;
    results.reserve( count);

    // get channel
    std::shared_ptr<
        boost::fibers::unbounded_channel< future_t > > channel(
            wait_all_until_error_source( std::forward< Fn >( function),
                                         std::forward< Fns >( functions) ... ) );
    // fill results vector
    future_t future;
    while ( boost::fibers::channel_op_status::success == channel->pop( future) ) {
        results.push_back( future.get() );
    }
    // return vector to caller
    return results;
}
//]

Example waue( runner, "wait_all_until_error()", [](){
//[wait_all_until_error_source_ex
    typedef boost::fibers::future< std::string > future_t;
    std::shared_ptr< boost::fibers::unbounded_channel< future_t > > channel =
        wait_all_until_error_source(
                [](){ return sleeper("wauess_third",  150); },
                [](){ return sleeper("wauess_second", 100); },
                [](){ return sleeper("wauess_first",   50); });
    future_t future;
    while ( boost::fibers::channel_op_status::success == channel->pop( future) ) {
        std::string value( future.get() );
        std::cout << "wait_all_until_error_source(success) => '" << value
                  << "'" << std::endl;
    }
//]

    channel = wait_all_until_error_source(
            [](){ return sleeper("wauesf_third",  150); },
            [](){ return sleeper("wauesf_second", 100, true); },
            [](){ return sleeper("wauesf_first",   50); });
//[wait_all_until_error_ex
    std::string thrown;
//<-
    try {
        while ( boost::fibers::channel_op_status::success == channel->pop( future) ) {
            std::string value( future.get() );
            std::cout << "wait_all_until_error_source(fail) => '" << value
                      << "'" << std::endl;
        }
    } catch ( std::exception const& e) {
        thrown = e.what();
    }
    std::cout << "wait_all_until_error_source(fail) threw '" << thrown
              << "'" << std::endl;

    thrown.clear();
//->
    try {
        std::vector< std::string > values = wait_all_until_error(
                [](){ return sleeper("waue_late",   150); },
                [](){ return sleeper("waue_middle", 100, true); },
                [](){ return sleeper("waue_early",   50); });
//<-
        std::cout << "wait_all_until_error(fail) =>";
        for ( std::string const& v : values) {
            std::cout << " '" << v << "'";
        }
        std::cout << std::endl;
//->
    } catch ( std::exception const& e) {
        thrown = e.what();
    }
    std::cout << "wait_all_until_error(fail) threw '" << thrown
              << "'" << std::endl;
//]
});

/*****************************************************************************
*   when_all, collect exceptions
*****************************************************************************/
// When all passed functions have succeeded, return vector<T> containing
// collected results, or throw exception_list containing all exceptions thrown
// by any of the passed functions. Assume that all passed functions have the
// same return type. It is simply invalid to pass NO functions.
//[wait_all_collect_errors
template< typename Fn, typename ... Fns >
std::vector< typename std::result_of< Fn() >::type >
wait_all_collect_errors( Fn && function, Fns && ... functions) {
    std::size_t count( 1 + sizeof ... ( functions) );
    typedef typename std::result_of< Fn() >::type return_t;
    typedef typename boost::fibers::future< return_t > future_t;
    typedef std::vector< return_t > vector_t;
    vector_t results;
    results.reserve( count);
    exception_list exceptions("wait_all_collect_errors() exceptions");

    // get channel
    std::shared_ptr<
        boost::fibers::unbounded_channel< future_t > > channel(
            wait_all_until_error_source( std::forward< Fn >( function),
                                         std::forward< Fns >( functions) ... ) );
    // fill results and/or exceptions vectors
    future_t future;
    while ( boost::fibers::channel_op_status::success == channel->pop( future) ) {
        std::exception_ptr exp = future.get_exception_ptr();
        if ( ! exp) {
            results.push_back( future.get() );
        } else {
            exceptions.add( exp);
        }
    }
    // if there were any exceptions, throw
    if ( exceptions.size() ) {
        throw exceptions;
    }
    // no exceptions: return vector to caller
    return results;
}
//]

Example wace( runner, "wait_all_collect_errors()", [](){
    std::vector< std::string > values = wait_all_collect_errors(
            [](){ return sleeper("waces_late",   150); },
            [](){ return sleeper("waces_middle", 100); },
            [](){ return sleeper("waces_early",   50); });
    std::cout << "wait_all_collect_errors(success) =>";
    for ( std::string const& v : values) {
        std::cout << " '" << v << "'";
    }
    std::cout << std::endl;

    std::string thrown;
    std::size_t errors = 0;
    try {
        values = wait_all_collect_errors(
                [](){ return sleeper("wacef_late",   150, true); },
                [](){ return sleeper("wacef_middle", 100, true); },
                [](){ return sleeper("wacef_early",   50); });
        std::cout << "wait_all_collect_errors(fail) =>";
        for ( std::string const& v : values) {
            std::cout << " '" << v << "'";
        }
        std::cout << std::endl;
    } catch ( exception_list const& e) {
        thrown = e.what();
        errors = e.size();
    } catch ( std::exception const& e) {
        thrown = e.what();
    }
    std::cout << "wait_all_collect_errors(fail) threw '" << thrown
              << "': " << errors << " errors" << std::endl;
});

/*****************************************************************************
*   when_all, heterogeneous
*****************************************************************************/
//[wait_all_members_get
template< typename Result, typename ... Futures >
Result wait_all_members_get( Futures && ... futures) {
    // Fetch the results from the passed futures into Result's initializer
    // list. It's true that the get() calls here will block the implicit
    // iteration over futures -- but that doesn't matter because we won't be
    // done until the slowest of them finishes anyway. As results are
    // processed in argument-list order rather than order of completion, the
    // leftmost get() to throw an exception will cause that exception to
    // propagate to the caller.
    return Result{ futures.get() ... };
}
//]

//[wait_all_members
// Explicitly pass Result. This can be any type capable of being initialized
// from the results of the passed functions, such as a struct.
template< typename Result, typename ... Fns >
Result wait_all_members( Fns && ... functions) {
    // Run each of the passed functions on a separate fiber, passing all their
    // futures to helper function for processing.
    return wait_all_members_get< Result >(
            boost::fibers::async( std::forward< Fns >( functions) ) ... );
}
//]

// used by following example
//[wait_Data
struct Data {
    std::string str;
    double      inexact;
    int         exact;

    friend std::ostream& operator<<( std::ostream& out, Data const& data)/*=;
    ...*/
//<-
    {
        return out << "Data{str='" << data.str << "', inexact=" << data.inexact
                   << ", exact=" << data.exact << "}";
    }
//->
};
//]

// example usage
Example wam( runner, "wait_all_members()", [](){
//[wait_all_members_data_ex
    Data data = wait_all_members< Data >(
            [](){ return sleeper("wams_left", 100); },
            [](){ return sleeper(3.14,        150); },
            [](){ return sleeper(17,          50); });
    std::cout << "wait_all_members<Data>(success) => " << data << std::endl;
//]

    std::string thrown;
    try {
        data = wait_all_members< Data >(
                [](){ return sleeper("wamf_left", 100, true); },
                [](){ return sleeper(3.14,        150); },
                [](){ return sleeper(17,          50, true); });
        std::cout << "wait_all_members<Data>(fail) => " << data << std::endl;
    } catch ( std::exception const& e) {
        thrown = e.what();
    }
    std::cout << "wait_all_members<Data>(fail) threw '" << thrown
              << '"' << std::endl;

//[wait_all_members_vector_ex
    // If we don't care about obtaining results as soon as they arrive, and we
    // prefer a result vector in passed argument order rather than completion
    // order, wait_all_members() is another possible implementation of
    // wait_all_until_error().
    auto strings = wait_all_members< std::vector< std::string > >(
            [](){ return sleeper("wamv_left",   150); },
            [](){ return sleeper("wamv_middle", 100); },
            [](){ return sleeper("wamv_right",   50); });
    std::cout << "wait_all_members<vector>() =>";
    for ( std::string const& str : strings) {
        std::cout << " '" << str << "'";
    }
    std::cout << std::endl;
//]
});


/*****************************************************************************
*   main()
*****************************************************************************/
int main( int argc, char *argv[]) {
    runner.run();
    std::cout << "done." << std::endl;
    return EXIT_SUCCESS;
}
