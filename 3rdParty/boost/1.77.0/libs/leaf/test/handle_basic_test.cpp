// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/detail/config.hpp>
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/pred.hpp>
#   include <boost/leaf/result.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

template <int Tag> struct info { int value;};

enum class error_code
{
    error1=1,
    error2,
    error3
};

struct error1_tag { };
struct error2_tag { };
struct error3_tag { };

leaf::result<int> compute_answer( int what_to_do ) noexcept
{
    switch( what_to_do )
    {
    case 0:
        return 42;
    case 1:
        return leaf::new_error(error_code::error1);
    case 2:
        return leaf::new_error(error_code::error2);
    case 3:
        return leaf::new_error(error_code::error3);
    case 4:
        return leaf::new_error(error1_tag{}, error_code::error1);
    case 5:
        return leaf::new_error(error2_tag{}, error_code::error2);
    default:
        BOOST_LEAF_ASSERT(what_to_do==6);
        return leaf::new_error(error3_tag{}, error_code::error3);
    }
}

leaf::result<int> handle_some_errors( int what_to_do )
{
    return leaf::try_handle_some(
        [=]
        {
            return compute_answer(what_to_do);
        },
        []( error1_tag, leaf::match<error_code, error_code::error1> )
        {
            return -1;
        },
        []( leaf::match<error_code, error_code::error1> )
        {
            return -2;
        } );
}

leaf::result<float> handle_some_errors_float( int what_to_do )
{
    return leaf::try_handle_some(
        [=]() -> leaf::result<float>
        {
            return compute_answer(what_to_do);
        },
        []( error2_tag, leaf::match<error_code, error_code::error2> )
        {
            return -1.0f;
        },
        []( leaf::match<error_code, error_code::error2> )
        {
            return -2.0f;
        } );
}

leaf::result<void> handle_some_errors_void( int what_to_do )
{
    return leaf::try_handle_some(
        [=]() -> leaf::result<void>
        {
            BOOST_LEAF_AUTO(answer, compute_answer(what_to_do));
            (void) answer;
            return { };
        },
        []( leaf::match<error_code, error_code::error3>  )
        {
        } );
}

int main()
{
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        return leaf::try_handle_some(
                            []() -> leaf::result<int>
                            {
                                return leaf::new_error(40);
                            },
                            []( leaf::error_info const & ei, int & v )
                            {
                                ++v;
                                return ei.error();
                            });
                    },
                    []( leaf::error_info const & ei, int & v )
                    {
                        ++v;
                        return ei.error();
                    });
            },
            []( int v )
            {
                BOOST_TEST_EQ(v, 42);
                return 1;
            },
            []
            {
                return 2;
            });
        BOOST_TEST_EQ(r, 1);
    }
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        return leaf::try_handle_some(
                            []() -> leaf::result<int>
                            {
                                return leaf::new_error(40);
                            },
                            []( leaf::error_info const & ei, int * v )
                            {
                                ++*v;
                                return ei.error();
                            });
                    },
                    []( leaf::error_info const & ei, int * v )
                    {
                        ++*v;
                        return ei.error();
                    });
            },
            []( int v )
            {
                BOOST_TEST_EQ(v, 42);
                return 1;
            },
            []
            {
                return 2;
            });
        BOOST_TEST_EQ(r, 1);
    }

    ///////////////////////////

    BOOST_TEST_EQ(handle_some_errors(0).value(), 42);
    BOOST_TEST_EQ(handle_some_errors(1).value(), -2);
    BOOST_TEST_EQ(handle_some_errors(4).value(), -1);
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                BOOST_LEAF_AUTO(answer, handle_some_errors(3));
                (void) answer;
                return 0;
            },
            []( leaf::match<error_code, error_code::error3> )
            {
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    ///////////////////////////

    BOOST_TEST_EQ(handle_some_errors_float(0).value(), 42.0f);
    BOOST_TEST_EQ(handle_some_errors_float(2).value(), -2.0f);
    BOOST_TEST_EQ(handle_some_errors_float(5).value(), -1.0f);
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                BOOST_LEAF_AUTO(answer, handle_some_errors_float(1));
                (void) answer;
                return 0;
            },
            []( leaf::match<error_code, error_code::error1> )
            {
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    ///////////////////////////

    BOOST_TEST(handle_some_errors_void(0));
    BOOST_TEST(handle_some_errors_void(3));
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                BOOST_LEAF_CHECK(handle_some_errors_void(2));
                return 0;
            },
            []( leaf::match<error_code, error_code::error2> )
            {
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    ///////////////////////////

#ifndef BOOST_LEAF_NO_EXCEPTIONS
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                BOOST_LEAF_CHECK(handle_some_errors_void(2));
                return 0;
            },
            []( std::exception const & )
            {
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 2);
    }
#endif

    ///////////////////////////

    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::new_error( info<1>{42} );
            },
            []( info<1> const & i1 )
            {
                BOOST_TEST_EQ(i1.value, 42);
                int r = leaf::try_handle_all(
                    []() -> leaf::result<int>
                    {
                        return leaf::new_error( info<1>{43} );
                    },
                    []()
                    {
                        return -1;
                    } );
                BOOST_TEST_EQ(r, -1);
                BOOST_TEST_EQ(i1.value, 42);
                return 0;
            },
            []()
            {
                return -1;
            } );
        BOOST_TEST_EQ(r, 0);
    }

    ///////////////////////////

    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::new_error( info<1>{42} );
            },
            []( info<1> const & i1 )
            {
                BOOST_TEST_EQ(i1.value, 42);
                int r = leaf::try_handle_all(
                    []() -> leaf::result<int>
                    {
                        return leaf::new_error( info<1>{43} );
                    },
                    []( info<1> const & i1 )
                    {
                        BOOST_TEST_EQ(i1.value, 43);
                        return -1;
                    },
                    []()
                    {
                        return -2;
                    } );
                BOOST_TEST_EQ(r, -1);
                BOOST_TEST_EQ(i1.value, 42);
                return 0;
            },
            []()
            {
                return -1;
            } );
        BOOST_TEST_EQ(r, 0);
    }

    ///////////////////////////

    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        return leaf::new_error( info<1>{1} );
                    },
                    []( leaf::error_info const & err, info<1> const & i1, info<2> const * i2 )
                    {
                        //We have space for info<2> in the context but i2 is null.
                        BOOST_TEST_EQ(i1.value, 1);
                        BOOST_TEST(!i2);
                        return err.error().load(info<2>{2});
                    } );
            },
            []( info<1> const & i1, info<2> const & i2 )
            {
                BOOST_TEST_EQ(i1.value, 1);
                BOOST_TEST_EQ(i2.value, 2);
                return 0;
            },
            []()
            {
                return -1;
            } );
        BOOST_TEST_EQ(r, 0);
    }

    ///////////////////////////

    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        return leaf::new_error( info<1>{1}, info<2>{-2} );
                    },
                    []( leaf::error_info const & err, info<1> const & i1, info<2> const & i2 )
                    {
                        BOOST_TEST_EQ(i1.value, 1);
                        BOOST_TEST_EQ(i2.value, -2);
                        return err.error().load(info<2>{2});
                    } );
            },
            []( info<1> const & i1, info<2> const & i2 )
            {
                BOOST_TEST_EQ(i1.value, 1);
                BOOST_TEST_EQ(i2.value, 2);
                return 0;
            },
            []()
            {
                return -1;
            } );
        BOOST_TEST_EQ(r, 0);
    }

    ///////////////////////////

    return boost::report_errors();
}
