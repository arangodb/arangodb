//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <cstdlib> // EXIT_SUCCESS

#include <boost/safe_numerics/safe_compare.hpp>
#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/range_value.hpp>

using namespace boost::safe_numerics;

// works for both GCC and clang
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"

// test conversion to T2 from different literal types
template<class T2, class T1>
bool test_cast(T1 v1, const char *t2_name, const char *t1_name){
    std::cout
        << "testing static_cast<safe<" << t2_name << ">>(" << t1_name << ")"
        << std::endl;
    {
        /* test conversion constructor to safe<T2> from v1 */
        try{
            // use auto to avoid checking assignment.
            auto result = static_cast<safe<T2>>(v1);
            std::cout << make_result_display(result) << " <- " << v1
                << std::endl;
            if(! safe_compare::equal(base_value(result), v1)){
                std::cout
                    << ' ' << t2_name << "<-" << t1_name
                    << " failed to detect error in construction"
                    << std::endl;
                static_cast<safe<T2> >(v1);
                return false;
            }
        }
        catch(const std::exception &){
            if( safe_compare::equal(static_cast<T2>(v1), v1)){
                std::cout
                    << ' ' << t1_name << "<-" << t2_name
                    << " erroneously emitted error "
                    << std::endl;
                try{
                    static_cast<safe<T2> >(v1);
                }
                catch(const std::exception &){}
                return false;
            }
        }
    }
    {
        /* test conversion to T2 from safe<T1>(v1) */
        safe<T1> s1(v1);
        try{
            auto result = static_cast<T2>(s1);
            std::cout << make_result_display(result) << " <- " << v1
                << std::endl;
            if(! safe_compare::equal(result, v1)){
                std::cout
                    << ' ' << t2_name << "<-" << t1_name
                    << " failed to detect error in construction"
                    << std::endl;
                static_cast<T2>(s1);
                return false;
            }
        }
        catch(const std::exception &){
            if(safe_compare::equal(static_cast<T2>(v1), v1)){
                std::cout
                    << ' ' << t1_name << "<-" << t2_name
                    << " erroneously emitted error"
                    << std::endl;
                try{
                    static_cast<T2>(s1);
                }
                catch(const std::exception &){}
                return false;
            }
        }
    }
    return true; // passed test
}
#pragma GCC diagnostic pop

#include <boost/mp11/algorithm.hpp>
#include <boost/core/demangle.hpp>
#include "test_values.hpp"

using namespace boost::mp11;

struct test {
    bool m_error;
    test(bool b = true) : m_error(b) {}
    operator bool(){
        return m_error;
    }
    template<typename T>
    void operator()(const T &){
        static_assert(mp_is_list<T>(), "must be a list of two types");
        using T1 = mp_first<T>; // first element is a type
        // second element is an integral constant
        using T2 = typename mp_second<T>::value_type; // get it's type
        constexpr T2 v2 = mp_second<T>(); // get it's value
        m_error &= test_cast<T1>(
            v2,
            boost::core::demangle(typeid(T1).name()).c_str(),
            boost::core::demangle(typeid(T2).name()).c_str()
        );
    }
};

int main(){
    test rval(true);

    mp_for_each<
        mp_product<mp_list, test_types, test_values>
    >(rval);

    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
