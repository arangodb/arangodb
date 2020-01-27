#ifndef BOOST_TEST_DIVIDE_HPP
#define BOOST_TEST_DIVIDE_HPP

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
bool test_divide(
    T1 v1,
    T2 v2,
    const char *av1,
    const char *av2,
    char expected_result
){
    {
        safe_t<T1> t1 = v1;
        using result_type = decltype(t1 / v2);
        std::cout
            << "safe<" << av1 << "> / " << av2 << " -> "
            << boost::core::demangle(typeid(result_type).name()) << '\n';
        static_assert(
            boost::safe_numerics::is_safe<safe_t<T1> >::value,
            "safe_t not safe!"
        );
        static_assert(
            boost::safe_numerics::is_safe<result_type>::value,
            "Expression failed to return safe type"
        );

        try{
            // use auto to avoid checking assignment.
            auto result = t1 / v2;
            std::cout << make_result_display(result);
            if(expected_result == 'x'){
                std::cout << " *** failed to detect error in division" << std::endl;
                t1 / v2;
                return false;
            }
            std::cout << std::endl;
        }
        catch(const std::exception &){
            if(expected_result == '.'){
                std::cout << " *** erroneously detected error in division" << std::endl;
                try{
                    t1 / v2;
                }
                catch(const std::exception &){}
                return false;
            }
        }
    }
    {
        safe_t<T2> t2 = v2;
        using result_type = decltype(v1 / t2);
        std::cout
            << "safe<" << av1 << "> / " << av2 << " -> "
            << boost::core::demangle(typeid(result_type).name()) << '\n';

        static_assert(
            boost::safe_numerics::is_safe<safe_t<T2> >::value,
            "safe_t not safe!"
        );
        static_assert(
            boost::safe_numerics::is_safe<result_type>::value,
            "Expression failed to return safe type"
        );
        try{
            // use auto to avoid checking assignment.
            auto result = v1 / t2;
            std::cout << make_result_display(result);
            if(expected_result == 'x'){
                std::cout << " *** failed to detect error in division " << std::endl;
                v1 / t2;
                return false;
            }
            std::cout << std::endl;
        }
        catch(const std::exception &){
            if(expected_result == '.'){
                std::cout << "*** erroneously detected error in division " << std::endl;
                try{
                    v1 / t2;
                }
                catch(const std::exception &){}
                // assert(result == unsafe_result);
                return false;
            }
        }
    }
    {
        safe_t<T1> t1 = v1;
        safe_t<T2> t2 = v2;
        using result_type = decltype(t1 / t2);
        std::cout
            << "testing  safe<" << av1 << "> / safe<" << av2 << "> -> "
            << boost::core::demangle(typeid(result_type).name()) << '\n';

        static_assert(
            boost::safe_numerics::is_safe<result_type>::value,
            "Expression failed to return safe type"
        );

        try{
            // use auto to avoid checking assignment.
            auto result = t1 / t2;
            std::cout << make_result_display(result);
            if(expected_result == 'x'){
                std::cout << " *** failed to detect error in division" << std::endl;
                t1 / t2;
                return false;
            }
            std::cout << std::endl;
        }
        catch(const std::exception &){
            if(expected_result == '.'){
                std::cout << " *** erroneously detected error in division \\" << std::endl;
                try{
                    t1 / t2;
                }
                catch(const std::exception &){}
                return false;
            }
        }
    }
    return true;
}

#endif // BOOST_TEST_DIVIDE
