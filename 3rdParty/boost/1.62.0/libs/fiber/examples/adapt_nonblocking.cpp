//          Copyright Nat Goodspeed 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/fiber/all.hpp>
#include <iostream>
#include <sstream>
#include <exception>
#include <string>
#include <algorithm>                // std::min()
#include <errno.h>                  // EWOULDBLOCK
#include <cassert>

/*****************************************************************************
*   example nonblocking API
*****************************************************************************/
//[NonblockingAPI
class NonblockingAPI {
public:
    NonblockingAPI();

    // nonblocking operation: may return EWOULDBLOCK
    int read( std::string & data, std::size_t desired);

/*=    ...*/
//<-
    // for simulating a real nonblocking API
    void set_data( std::string const& data, std::size_t chunksize);
    void inject_error( int ec);

private:
    std::string data_;
    int         injected_;
    unsigned    tries_;
    std::size_t chunksize_;
//->
};
//]

/*****************************************************************************
*   fake NonblockingAPI implementation... pay no attention to the little man
*   behind the curtain...
*****************************************************************************/
NonblockingAPI::NonblockingAPI() :
    injected_( 0),
    tries_( 0),
    chunksize_( 9999) {
}

void NonblockingAPI::set_data( std::string const& data, std::size_t chunksize) {
    data_ = data;
    chunksize_ = chunksize;
    // This delimits the start of a new test. Reset state.
    injected_ = 0;
    tries_ = 0;
}

void NonblockingAPI::inject_error( int ec) {
    injected_ = ec;
}

int NonblockingAPI::read( std::string & data, std::size_t desired) {
    // in case of error
    data.clear();

    if ( injected_) {
        // copy injected_ because we're about to reset it
        auto injected( injected_);
        injected_ = 0;
        // after an error situation, restart success count
        tries_ = 0;
        return injected;
    }

    if ( ++tries_ < 5) {
        // no injected error, but the resource isn't yet ready
        return EWOULDBLOCK;
    }

    // tell caller there's nothing left
    if ( data_.empty() ) {
        return EOF;
    }

    // okay, finally have some data
    // but return minimum of desired and chunksize_
    std::size_t size( ( std::min)( desired, chunksize_) );
    data = data_.substr( 0, size);
    // strip off what we just returned
    data_ = data_.substr( size);
    // reset I/O retries count for next time
    tries_ = 0;
    // success
    return 0;
}

/*****************************************************************************
*   adapters
*****************************************************************************/
//[nonblocking_read_chunk
// guaranteed not to return EWOULDBLOCK
int read_chunk( NonblockingAPI & api, std::string & data, std::size_t desired) {
    int error;
    while ( EWOULDBLOCK == ( error = api.read( data, desired) ) ) {
        // not ready yet -- try again on the next iteration of the
        // application's main loop
        boost::this_fiber::yield();
    }
    return error;
}
//]

//[nonblocking_read_desired
// keep reading until desired length, EOF or error
// may return both partial data and nonzero error
int read_desired( NonblockingAPI & api, std::string & data, std::size_t desired) {
    // we're going to accumulate results into 'data'
    data.clear();
    std::string chunk;
    int error = 0;
    while ( data.length() < desired &&
           ( error = read_chunk( api, chunk, desired - data.length() ) ) == 0) {
        data.append( chunk);
    }
    return error;
}
//]

//[nonblocking_IncompleteRead
// exception class augmented with both partially-read data and errorcode
class IncompleteRead : public std::runtime_error {
public:
    IncompleteRead( std::string const& what, std::string const& partial, int ec) :
        std::runtime_error( what),
        partial_( partial),
        ec_( ec) {
    }

    std::string get_partial() const {
        return partial_;
    }

    int get_errorcode() const {
        return ec_;
    }

private:
    std::string partial_;
    int         ec_;
};
//]

//[nonblocking_read
// read all desired data or throw IncompleteRead
std::string read( NonblockingAPI & api, std::size_t desired) {
    std::string data;
    int ec( read_desired( api, data, desired) );

    // for present purposes, EOF isn't a failure
    if ( 0 == ec || EOF == ec) {
        return data;
    }

    // oh oh, partial read
    std::ostringstream msg;
    msg << "NonblockingAPI::read() error " << ec << " after "
        << data.length() << " of " << desired << " characters";
    throw IncompleteRead( msg.str(), data, ec);
}
//]

int main( int argc, char *argv[]) {
    NonblockingAPI api;
    const std::string sample_data("abcdefghijklmnopqrstuvwxyz");

    // Try just reading directly from NonblockingAPI
    api.set_data( sample_data, 5);
    std::string data;
    int ec = api.read( data, 13);
    // whoops, underlying resource not ready
    assert(ec == EWOULDBLOCK);
    assert(data.empty());

    // successful read()
    api.set_data( sample_data, 5);
    data = read( api, 13);
    assert(data == "abcdefghijklm");

    // read() with error
    api.set_data( sample_data, 5);
    // don't accidentally pick either EOF or EWOULDBLOCK
    assert(EOF != 1);
    assert(EWOULDBLOCK != 1);
    api.inject_error(1);
    int thrown = 0;
    try {
        data = read( api, 13);
    } catch ( IncompleteRead const& e) {
        thrown = e.get_errorcode();
    }
    assert(thrown == 1);

    std::cout << "done." << std::endl;

    return EXIT_SUCCESS;
}
