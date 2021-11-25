#ifndef BOOST_TEST_ASSIGNMENT_HPP
#define BOOST_TEST_ASSIGNMENT_HPP

//  Copyright (c) 2015 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <boost/core/demangle.hpp>
#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/range_value.hpp>

template<class T1, class T2>
bool test_assignment(
    T1 t1,
    T2 t2,
    const char *at1,
    const char *at2,
    char expected_result
){
    std::cout << "testing " << std::endl;
    {
        std::cout << "safe<" << at1 << "> = " << at2 << std::endl;
        // std::cout << boost::core::demangle(typeid(t1).name()) << " = " << at2 << std::endl;
        safe_t<T2> s2(t2);

        static_assert(
            boost::safe_numerics::is_safe<safe_t<T1> >::value,
            "safe_t not safe!"
        );
        try{
            t1 = s2;
            if(expected_result == 'x'){
                    std::cout
                        << "failed to detect error in assignment "
                        << boost::core::demangle(typeid(t1).name())
                        << " = "
                        << at2
                        << std::endl;
                t1 = s2;
                return false;
            }
        }
        catch(const std::exception &){
            if(expected_result == '.'){
                    std::cout
                        << "erroneously detected error in assignment "
                        << boost::core::demangle(typeid(t1).name())
                        << " = "
                        << at2
                        << std::endl;
                try{
                    t1 = s2;
                }
                catch(const std::exception &){}
                return false;
            }
        }
    }
    {
        safe_t<T1> s1(t1);
        safe_t<T2> s2(t2);
        std::cout
            << "safe<" << boost::core::demangle(typeid(t1).name()) << ">"
            << " = "
            << "safe<" << at2 << '>' << std::endl;
        try{
            s1 = s2;
            if(expected_result == 'x'){
                std::cout
                    << "failed to detect error in assignment "
                    << "safe<" << boost::core::demangle(typeid(t1).name()) << ">"
                    << " = "
                    << at2
                    << std::endl;
                s1 = s2;
                return false;
            }
        }
        catch(const std::exception &){
            if(expected_result == '.'){
                std::cout
                    << "erroneously detected error in assignment "
                    << "safe<" << boost::core::demangle(typeid(t1).name()) << ">"
                    << " = "
                    << at2
                    << std::endl;
                try{
                    s1 = t2;
                }
                catch(const std::exception &){}
                return false;
            }
        }
    }
    return true; // correct result
}

#endif // BOOST_TEST_ASSIGNMENT_HPP
