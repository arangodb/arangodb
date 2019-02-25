//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// test constructors

#include <iostream>
#include <exception>

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
        catch(std::exception){
            // should never, ever arrive here
            std::cout
                << "erroneously detected error in construction "
                << "safe<" << t1_name << "> (" << t1_name << ")"
                << std::endl;
            try{
                safe<T1> s1(t1); // try again for debugging
            }
            catch(std::exception){}
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
        catch(std::exception){
            if(safe_compare::equal(t2, t1)){
                std::cout
                    << "erroneously detected error in construction "
                    << "safe<" << t2_name << "> (" << t1_name << ")"
                    << std::endl;
                try{
                    safe<T2> sx2(t1); // try again for debugging
                }
                catch(std::exception){}
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
        catch(std::exception){
            // should never arrive here
            std::cout
                << "erroneously detected error in construction "
                << "safe<" << t1_name << "> (safe<" << t1_name << ">(" << t1 << "))"
                << std::endl;
            try{
                safe<T1> s1(t1);
            }
            catch(std::exception){}
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
        catch(std::exception){
            if(safe_compare::equal(t1, t2)){
                std::cout
                    << "erroneously detected error in construction "
                    << "safe<" << t2_name << "> (safe<" << t1_name << ">(" << t1 << "))"
                    << std::endl;
                try{
                    safe<T2> s1(t1);
                }
                catch(std::exception){}
                return false;
            }
        }
    }
    return true;
}

#include "test.hpp"
#include "test_types.hpp"
#include "test_values.hpp"

#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/array/elem.hpp>
#include <boost/preprocessor/array/size.hpp>
#include <boost/preprocessor/stringize.hpp>

#define TEST_CONSTRUCTION(T1, v)      \
    rval &= test_construction<T1>(    \
        v,                            \
        BOOST_PP_STRINGIZE(T1),       \
        BOOST_PP_STRINGIZE(v)         \
    );
/**/

#define EACH_VALUE(z, value_index, type_index)     \
    TEST_CONSTRUCTION(                             \
        BOOST_PP_ARRAY_ELEM(type_index, TYPES),    \
        BOOST_PP_ARRAY_ELEM(value_index, VALUES)   \
    )                                              \
/**/

#define EACH_TYPE1(z, type_index, nothing)         \
    BOOST_PP_REPEAT(                               \
        BOOST_PP_ARRAY_SIZE(VALUES),               \
        EACH_VALUE,                                \
        type_index                                 \
    )                                              \
/**/
int main(int, char *[]){
    bool rval = true;
    BOOST_PP_REPEAT(
        BOOST_PP_ARRAY_SIZE(TYPES),
        EACH_TYPE1,
        nothing
    )
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}

