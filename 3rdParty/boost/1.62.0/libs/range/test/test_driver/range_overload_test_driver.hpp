    //  Copyright Neil Groves 2013. Use, modification and 
    //  distribution is subject to the Boost Software License, Version 
    //  1.0. (See accompanying file LICENSE_1_0.txt or copy at 
    //  http://www.boost.org/LICENSE_1_0.txt) 
    // 
    // 
    // For more information, see http://www.boost.org/libs/range/ 
    // 
    // Acknowledgments:
    // Implemented by Andy in response to Ticket 6888 - unique fix
    //
    #ifndef BOOST_RANGE_TEST_TEST_DRIVER_RANGE_OVERLOAD_TEST_DRIVER_HPP_INCLUDED 
    #define BOOST_RANGE_TEST_TEST_DRIVER_RANGE_OVERLOAD_TEST_DRIVER_HPP_INCLUDED 
     
    #include "range_return_test_driver.hpp" 
    #include <boost/assert.hpp> 
    #include <boost/test/test_tools.hpp> 
    #include <boost/test/unit_test.hpp> 
     
    namespace boost 
    { 
        namespace range_test 
        { 
             
            // A test driver to exercise a test through range_return_test_driver 
            // plus the overload that determines the return_type by overload 
            // 
            // The test driver also contains the code required to check the 
            // return value correctness. 
            // 
            // The TestPolicy needs to implement all those required by  
            // range_return_test_driver, and additionally 
            // 
            // - perform the boost range version of the algorithm that determines 
            //   the return_type by overload 
            class range_overload_test_driver : range_return_test_driver 
            { 
            public: 
                template< class Container, 
                          class TestPolicy > 
                void operator()(Container& cont, TestPolicy policy) 
                { 
                    range_return_test_driver::operator()(cont, policy); 
                    test_range_overload<Container, TestPolicy>()(cont, policy); 
                } 
     
            private: 
                template< class Container, class TestPolicy > 
                struct test_range_overload 
                { 
                    void operator()(Container& cont, TestPolicy policy) 
                    { 
                        typedef BOOST_DEDUCED_TYPENAME range_iterator<Container>::type iterator_t; 
                        typedef BOOST_DEDUCED_TYPENAME TestPolicy::template test_range_overload<Container> test_range_overload_t; 
                        const range_return_value result_type = test_range_overload_t::result_type;  
                        typedef BOOST_DEDUCED_TYPENAME range_return<Container, result_type>::type range_return_t; 
     
                        Container reference(cont); 
                        Container test_cont(cont); 
     
                        test_range_overload_t test_range_overload_fn; 
                        range_return_t range_result = test_range_overload_fn(policy, test_cont); 
     
                        iterator_t reference_it = policy.reference(reference); 
     
                        check_results<result_type>::test(test_cont, reference, 
                                                         range_result, reference_it); 
                    } 
                }; 
            }; 
        } 
    } 
     
    #endif // include guard 