/* tests for using boost::hash with boost::array
 * (C) Copyright Marshall Clow 2012
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <string>
#include <iostream>
#include <boost/array.hpp>
#include <algorithm>
#include <boost/functional/hash.hpp>

#include <boost/core/lightweight_test_trait.hpp>

namespace {

    template< class T >
    void    RunTests()
    {
    //    std::size_t hash0 = boost::hash<boost::array<T,0> > () ( boost::array<T, 0> ());
    //    std::size_t hash1 = boost::hash<boost::array<T,1> > () ( boost::array<T, 1> ());
    
        typedef boost::array< T, 5 >    barr;
        typedef T arr[5];
        barr           test_barr =   {{ 1, 1, 2, 3, 5 }};
        arr            test_arr  =    { 1, 1, 2, 3, 5 };
    
        std::size_t bhash = boost::hash<barr> () ( test_barr );
        std::size_t ahash = boost::hash<arr>  () ( test_arr );
        BOOST_TEST ( ahash == bhash );
    }

}

int main()
{
    RunTests< int >();
    RunTests< long >();
    RunTests< long double >();

    return boost::report_errors();
}

