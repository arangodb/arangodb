
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

#include <boost/cstdint.hpp>
#include <boost/preprocessor.hpp>

#include "../clock.hpp"

#define CREATE(z, n, _) \
    std::thread BOOST_PP_CAT(t,n) (worker);
#define JOIN(z, n, _) \
    BOOST_PP_CAT(t,n) .join();

void worker() {}

duration_type measure10( duration_type overhead)
{
    std::thread( worker).join();

    BOOST_PP_REPEAT_FROM_TO(1, 10, CREATE, _);

    time_point_type start( clock_type::now() );
    BOOST_PP_REPEAT_FROM_TO(1, 10, JOIN, _);
    duration_type total = clock_type::now() - start;

    total -= overhead_clock(); // overhead of measurement
    total /= 10;  // loops

    return total;
}

duration_type measure50( duration_type overhead)
{
    std::thread( worker).join();

    BOOST_PP_REPEAT_FROM_TO(1, 50, CREATE, _);

    time_point_type start( clock_type::now() );
    BOOST_PP_REPEAT_FROM_TO(1, 50, JOIN, _);
    duration_type total = clock_type::now() - start;

    total -= overhead_clock(); // overhead of measurement
    total /= 50;  // loops

    return total;
}

duration_type measure100( duration_type overhead)
{
    std::thread( worker).join();

    BOOST_PP_REPEAT_FROM_TO(1, 100, CREATE, _);

    time_point_type start( clock_type::now() );
    BOOST_PP_REPEAT_FROM_TO(1, 100, JOIN, _);
    duration_type total = clock_type::now() - start;

    total -= overhead_clock(); // overhead of measurement
    total /= 100;  // loops

    return total;
}

duration_type measure500( duration_type overhead)
{
    std::thread( worker).join();

#include "thread_create_500.ipp"

    time_point_type start( clock_type::now() );
#include "thread_join_500.ipp"
    duration_type total = clock_type::now() - start;

    total -= overhead_clock(); // overhead of measurement
    total /= 500;  // loops

    return total;
}

duration_type measure1000( duration_type overhead)
{
    std::thread( worker).join();

#include "thread_create_1000.ipp"

    time_point_type start( clock_type::now() );
#include "thread_join_1000.ipp"
    duration_type total = clock_type::now() - start;

    total -= overhead_clock(); // overhead of measurement
    total /= 1000;  // loops

    return total;
}

duration_type measure5000( duration_type overhead)
{
    std::thread( worker).join();

#include "thread_create_5000.ipp"

    time_point_type start( clock_type::now() );
#include "thread_join_5000.ipp"
    duration_type total = clock_type::now() - start;

    total -= overhead_clock(); // overhead of measurement
    total /= 5000;  // loops

    return total;
}

duration_type measure10000( duration_type overhead)
{
    std::thread( worker).join();

#include "thread_create_10000.ipp"

    time_point_type start( clock_type::now() );
#include "thread_join_10000.ipp"
    duration_type total = clock_type::now() - start;

    total -= overhead_clock(); // overhead of measurement
    total /= 10000;  // loops

    return total;
}

int main( int argc, char * argv[])
{
    try
    {
        duration_type overhead = overhead_clock();

        boost::uint64_t res = measure10( overhead).count();
        std::cout << "10 jobs: average of " << res << " nano seconds" << std::endl;
        res = measure50( overhead).count();
        std::cout << "50 jobs: average of " << res << " nano seconds" << std::endl;
        res = measure100( overhead).count();
        std::cout << "100 jobs: average of " << res << " nano seconds" << std::endl;
        res = measure500( overhead).count();
        std::cout << "500 jobs: average of " << res << " nano seconds" << std::endl;
        res = measure1000( overhead).count();
        std::cout << "1000 jobs: average of " << res << " nano seconds" << std::endl;
        res = measure5000( overhead).count();
        std::cout << "5000 jobs: average of " << res << " nano seconds" << std::endl;
        res = measure10000( overhead).count();
        std::cout << "10000 jobs: average of " << res << " nano seconds" << std::endl;

        return EXIT_SUCCESS;
    }
    catch ( std::exception const& e)
    { std::cerr << "exception: " << e.what() << std::endl; }
    catch (...)
    { std::cerr << "unhandled exception" << std::endl; }
    return EXIT_FAILURE;
}
