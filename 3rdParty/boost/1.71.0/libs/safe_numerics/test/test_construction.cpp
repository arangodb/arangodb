//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// test constructors

#include <iostream>

#include <boost/safe_numerics/safe_compare.hpp>
#include <boost/safe_numerics/safe_integer.hpp>

template<class T2, class T1>
bool test_construction(T1 t1, const char *t2_name, const char *t1_name){
    using namespace boost::safe_numerics;
    std::cout
        << "testing constructions to " << t2_name << " from " << t1_name
        << std::endl;
    {
        /* (1) test construction of safe<T1> from T1 type */
        try{
            safe<T1> s1(t1);
            // should always arrive here!
        }
        catch(const std::exception &){
            // should never, ever arrive here
            std::cout
                << "erroneously detected error in construction "
                << "safe<" << t1_name << "> (" << t1_name << ")"
                << std::endl;
            try{
                safe<T1> s1(t1); // try again for debugging
            }
            catch(const std::exception &){}
            return false;
        }
    }
    {
        /* (2) test construction of safe<T2> from T1 type */
        T2 t2;
        try{
            t2 = safe<T2>(t1);
            if(! safe_compare::equal(t2 , t1)){
                std::cout
                    << "failed to detect error in construction "
                    << "safe<" << t2_name << "> (" << t1_name << ")"
                    << std::endl;
                safe<T2> s2x(t1);
                return false;
            }
        }
        catch(const std::exception &){
            if(safe_compare::equal(t2, t1)){
                std::cout
                    << "erroneously detected error in construction "
                    << "safe<" << t2_name << "> (" << t1_name << ")"
                    << std::endl;
                try{
                    safe<T2> sx2(t1); // try again for debugging
                }
                catch(const std::exception &){}
                return false;
            }
        }
    }
    {
        /* (3) test construction of safe<T1> from safe<T1> type */
        safe<T1> s1x(t1);
        try{
            safe<T1> s1(s1x);
            if(! (s1 == s1x)){
                std::cout
                    << "copy constructor altered value "
                    << "safe<" << t1_name << "> (safe<" << t1_name << ">(" << t1 << "))"
                    << std::endl;
                //safe<T1> s1(s1x);
                return false;
            }
        }
        catch(const std::exception &){
            // should never arrive here
            std::cout
                << "erroneously detected error in construction "
                << "safe<" << t1_name << "> (safe<" << t1_name << ">(" << t1 << "))"
                << std::endl;
            try{
                safe<T1> s1(t1);
            }
            catch(const std::exception &){}
            return false;
        }
    }
    {
        /* (4) test construction of safe<T2> from safe<T1> type */
        T2 t2;
        try{
            safe<T1> s1(t1);
            safe<T2> s2(s1);
            t2 = static_cast<T2>(s2);
            if(! (safe_compare::equal(t1, t2))){
                std::cout
                    << "failed to detect error in construction "
                    << "safe<" << t1_name << "> (safe<" << t2_name << ">(" << t1 << "))"
                    << std::endl;
                safe<T2> s1x(t1);
                return false;
            }
        }
        catch(const std::exception &){
            if(safe_compare::equal(t1, t2)){
                std::cout
                    << "erroneously detected error in construction "
                    << "safe<" << t2_name << "> (safe<" << t1_name << ">(" << t1 << "))"
                    << std::endl;
                try{
                    safe<T2> s1(t1);
                }
                catch(const std::exception &){}
                return false;
            }
        }
    }
    return true;
}

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
        m_error &= test_construction<T1>(
            v2,
            boost::core::demangle(typeid(T1).name()).c_str(),
            boost::core::demangle(typeid(T2).name()).c_str()
        );
    }
};

template<typename T>
using extract_value_type = typename T::value_type;
using test_types = mp_unique<
    mp_transform<
        extract_value_type,
        test_values
    >
>;

int main(){
    test rval(true);

    mp_for_each<
        mp_product<mp_list, test_types, test_values>
    >(rval);

    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
