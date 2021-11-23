// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/result.hpp>
#endif

#include "visibility_test_lib.hpp"
#include "lightweight_test.hpp"
#include <sstream>

namespace leaf = boost::leaf;

leaf::result<void> hidden_result();
void hidden_throw();

int
main()
    {
        {
            int r = leaf::try_handle_all(
                []() -> leaf::result<int>
                {
                    BOOST_LEAF_CHECK(hidden_result());
                    return 0;
                },
                []( my_info<1> x1, my_info<2> x2, leaf::diagnostic_info const & info, leaf::verbose_diagnostic_info const & vinfo )
                {
                    BOOST_TEST_EQ(x1.value, 1);
                    BOOST_TEST_EQ(x2.value, 2);
                    if( BOOST_LEAF_DIAGNOSTICS )
                    {
                        std::stringstream ss; ss << info;
                        BOOST_TEST_NE(ss.str().find("1 attempt to communicate an unexpected error object"), std::string::npos);
                    }
                    if( BOOST_LEAF_DIAGNOSTICS )
                    {
                        std::stringstream ss; ss << vinfo;
                        BOOST_TEST_NE(ss.str().find("Test my_info<3>::value = 3"), std::string::npos);
                    }
                    return 1;
                },
                []
                {
                    return 2;
                } );
            BOOST_TEST_EQ(r, 1);
        }

#ifndef BOOST_LEAF_NO_EXCEPTIONS
        {
            int r = leaf::try_catch(
                []
                {
                    hidden_throw();
                    return 0;
                },
                []( my_info<1> x1, my_info<2> x2, leaf::diagnostic_info const & info, leaf::verbose_diagnostic_info const & vinfo )
                {
                    BOOST_TEST_EQ(x1.value, 1);
                    BOOST_TEST_EQ(x2.value, 2);
                    if( BOOST_LEAF_DIAGNOSTICS )
                    {
                        std::stringstream ss; ss << info;
                        BOOST_TEST_NE(ss.str().find("1 attempt to communicate an unexpected error object"), std::string::npos);
                    }
                    if( BOOST_LEAF_DIAGNOSTICS )
                    {
                        std::stringstream ss; ss << vinfo;
                        BOOST_TEST_NE(ss.str().find("Test my_info<3>::value = 3"), std::string::npos);
                    }
                    return 1;
                },
                []
                {
                    return 2;
                } );
            BOOST_TEST_EQ(r, 1);
        }
        {
            try
            {
                hidden_throw();
                BOOST_ERROR("hidden_throw() failed to throw");
            }
            catch( leaf::error_id const & )
            {
            }
            catch(...)
            {
                BOOST_ERROR("Failed to catch leaf::error_id");
            }
        }
#endif

    return boost::report_errors();
    }
