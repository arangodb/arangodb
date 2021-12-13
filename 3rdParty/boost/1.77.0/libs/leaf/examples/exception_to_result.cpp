// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This example demonstrates how to transport exceptions thrown by a low level
// function through an intermediate scopes that are not exception-safe, to be
// handled in a high level function which may or may not be exception-safe.

#include <boost/leaf.hpp>
#include <iostream>

namespace leaf = boost::leaf;


class error_base: public virtual std::exception { };
class error_a: public virtual error_base { };
class error_b: public virtual error_base { };
class error_c: public virtual error_base { };


// Lower-level library function which throws exceptions.
int compute_answer_throws()
{
    switch( rand()%4 )
    {
        default: return 42;
        case 1: throw error_a();
        case 2: throw error_b();
        case 3: throw error_c();
    }
}


// Call compute_answer_throws, switch to result<int> for error handling.
leaf::result<int> compute_answer() noexcept
{
    // Convert exceptions of types error_a and error_b to be communicated by
    // leaf::result. Any other exception will be communicated as a
    // std::exception_ptr.
    return leaf::exception_to_result<error_a, error_b>(
        []
        {
            return compute_answer_throws();
        } );
}


// Print the answer if the call to compute_answer is successful.
leaf::result<void> print_answer() noexcept
{
    BOOST_LEAF_AUTO( answer, compute_answer());
    std::cout << "Answer: " << answer << std::endl;
    return { };
}


int main()
{
    // Exercise print_answer a few times and handle errors. Note that the
    // exception objects that compute_answer_throws throws are not handled as
    // exceptions, but as regular LEAF error error objects...
    for( int i=0; i!=42; ++i )
    {
        leaf::try_handle_all(
            []() -> leaf::result<void>
            {
                BOOST_LEAF_CHECK(print_answer());
                return { };
            },

            []( error_a const & e )
            {
                std::cerr << "Error A!" << std::endl;
            },

            []( error_b const & e )
            {
                std::cerr << "Error B!" << std::endl;
            },

            // ...except for error_c errors, which (for demonstration) are
            // captured as exceptions into std::exception_ptr as "unknown"
            // exceptions. Presumably this should not happen, therefore at this
            // point we treat this situation as a logic error: we print
            // diagnostic information and bail out.
            []( std::exception_ptr const * ep )
            {
                std::cerr << "Got unknown error!" << std::endl;

                // Above, why do we take ep as a pointer? Because handle_all
                // requires that the last handler matches any error and, taken
                // as a pointer, if there isn't a std::exception_ptr associated
                // with the error, the handler will still be matched (with 0
                // passed for ep). Had we taken it by value or by const &, the
                // program would not have compiled.
                if( ep )
                    leaf::try_catch(
                        [&]
                        {
                            std::rethrow_exception(*ep);
                        },
                        []( leaf::error_info const & unmatched )
                        {
                            std::cerr << unmatched;
                        } );
            } );
    }

    return 0;
}
