// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/detail/config.hpp>
#ifdef BOOST_LEAF_NO_EXCEPTIONS

#include <iostream>

int main()
{
    std::cout << "Unit test not applicable." << std::endl;
    return 0;
}

#else

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/pred.hpp>
#   include <boost/leaf/result.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

template <int> struct info { int value; };

struct my_exception: std::exception
{
    int value;

    my_exception():
        value(0)
    {
    }

    my_exception(int v):
        value(v)
    {
    }
};

int main()
{
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                return 42;
            },
            []
            {
                return 1;
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(r.value(), 42);
    }
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                throw leaf::exception( my_exception(), info<1>{1} );
            },
            []( my_exception const &, info<1> const & x )
            {
                BOOST_TEST_EQ(x.value, 1);
                return 1;
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(r.value(), 1);
    }
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                throw leaf::exception( info<1>{1} );
            },
            []( info<1> const & x )
            {
                BOOST_TEST_EQ(x.value, 1);
                return 1;
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(r.value(), 1);
    }
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                return leaf::new_error( info<1>{1} );
            },
            []( info<1> const & x )
            {
                BOOST_TEST_EQ(x.value, 1);
                return 1;
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(r.value(), 1);
    }

    ///////////////////////////

    {
        auto error_handlers = std::make_tuple(
            []( my_exception const &, info<1> const & x ) -> leaf::result<int>
            {
                BOOST_TEST_EQ(x.value, 1);
                return 1;
            },
            []( info<1> const & x ) -> leaf::result<int>
            {
                BOOST_TEST_EQ(x.value, 1);
                return 2;
            } );
        {
            leaf::result<int> r = leaf::try_handle_some(
                []() -> leaf::result<int>
                {
                    return 42;
                },
                error_handlers );
            BOOST_TEST(r);
            BOOST_TEST_EQ(r.value(), 42);
        }
        {
            leaf::result<int> r = leaf::try_handle_some(
                []() -> leaf::result<int>
                {
                    throw leaf::exception( my_exception(), info<1>{1} );
                },
                error_handlers );
            BOOST_TEST(r);
            BOOST_TEST_EQ(r.value(), 1);
        }
        {
            leaf::result<int> r = leaf::try_handle_some(
                []() -> leaf::result<int>
                {
                    throw leaf::exception( info<1>{1} );
                },
                error_handlers );
            BOOST_TEST(r);
            BOOST_TEST_EQ(r.value(), 2);
        }
        {
            leaf::result<int> r = leaf::try_handle_some(
                []() -> leaf::result<int>
                {
                    return leaf::new_error( info<1>{1} );
                },
                error_handlers );
            BOOST_TEST(r);
            BOOST_TEST_EQ(r.value(), 2);
        }
    }

    ///////////////////////////

    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return 42;
            },
            []
            {
                return 1;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                throw leaf::exception( my_exception(), info<1>{1} );
            },
            []( my_exception const &, info<1> const & x )
            {
                BOOST_TEST_EQ(x.value, 1);
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                throw leaf::exception( info<1>{1} );
            },
            []( info<1> const & x )
            {
                BOOST_TEST_EQ(x.value, 1);
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::new_error( info<1>{1} );
            },
            []( info<1> const & x )
            {
                BOOST_TEST_EQ(x.value, 1);
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    ///////////////////////////

    {
        auto error_handlers = std::make_tuple(
            []( my_exception const &, info<1> const & x )
            {
                BOOST_TEST_EQ(x.value, 1);
                return 1;
            },
            []( info<1> const & x )
            {
                BOOST_TEST_EQ(x.value, 1);
                return 2;
            },
            []
            {
                return 1;
            } );
        {
            int r = leaf::try_handle_all(
                []() -> leaf::result<int>
                {
                    return 42;
                },
                error_handlers );
            BOOST_TEST_EQ(r, 42);
        }
        {
            int r = leaf::try_handle_all(
                []() -> leaf::result<int>
                {
                    throw leaf::exception( my_exception(), info<1>{1} );
                },
                error_handlers );
            BOOST_TEST_EQ(r, 1);
        }
        {
            int r = leaf::try_handle_all(
                []() -> leaf::result<int>
                {
                    throw leaf::exception( info<1>{1} );
                },
                error_handlers );
            BOOST_TEST_EQ(r, 2);
        }
        {
            int r = leaf::try_handle_all(
                []() -> leaf::result<int>
                {
                    return leaf::new_error( info<1>{1} );
                },
                error_handlers );
            BOOST_TEST_EQ(r, 2);
        }
    }

    ///////////////////////////

    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::try_handle_all(
                    []() -> leaf::result<int>
                    {
                        return leaf::new_error( info<1>{1} );
                    },
                    []( info<1> const & ) -> int
                    {
                        BOOST_LEAF_THROW_EXCEPTION(my_exception());
                    },
                    []
                    {
                        return 1;
                    } );
            },
            []( my_exception const &, info<1> )
            {
                return 2;
            },
            []( my_exception const & )
            {
                return 3;
            },
            []
            {
                return 4;
            } );

        BOOST_TEST_EQ(r, 3);
    }
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::try_handle_all(
                    []() -> leaf::result<int>
                    {
                        return leaf::new_error( info<1>{1} );
                    },
                    []( info<1> const & x ) -> int
                    {
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_LEAF_THROW_EXCEPTION();
                    },
                    []
                    {
                        return 1;
                    } );
            },
            []( my_exception const &, info<1> )
            {
                return 2;
            },
            []( my_exception const & )
            {
                return 3;
            },
            []
            {
                return 4;
            } );

        BOOST_TEST_EQ(r, 4);
    }

    ///////////////////////////

    {
        auto error_handlers = std::make_tuple(
            []( info<1> const & x ) -> int
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_LEAF_THROW_EXCEPTION(my_exception());
            },
            []
            {
                return 1;
            } );
        int r = leaf::try_handle_all(
            [&]() -> leaf::result<int>
            {
                return leaf::try_handle_all(
                    [&]() -> leaf::result<int>
                    {
                        return leaf::new_error( info<1>{1} );
                    },
                    error_handlers );
            },
            []( my_exception const &, info<1> )
            {
                return 2;
            },
            []( my_exception const & )
            {
                return 3;
            },
            []
            {
                return 4;
            } );

        BOOST_TEST_EQ(r, 3);
    }
    {
        auto error_handlers = std::make_tuple(
            []( info<1> const & x ) -> int
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_LEAF_THROW_EXCEPTION();
            },
            []
            {
                return 1;
            } );
        int r = leaf::try_handle_all(
            [&]() -> leaf::result<int>
            {
                return leaf::try_handle_all(
                    [&]() -> leaf::result<int>
                    {
                        return leaf::new_error( info<1>{1} );
                    },
                    error_handlers );
            },
            []( my_exception const &, info<1> )
            {
                return 2;
            },
            []( my_exception const & )
            {
                return 3;
            },
            []
            {
                return 4;
            } );

        BOOST_TEST_EQ(r, 4);
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
                    []( info<1> const & x ) -> int
                    {
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_LEAF_THROW_EXCEPTION(my_exception());
                    },
                    []
                    {
                        return 1;
                    } );
            },
            []( my_exception const &, info<1> )
            {
                return 3;
            },
            []( my_exception const & )
            {
                return 4;
            },
            []
            {
                return 5;
            } );

        BOOST_TEST_EQ(r, 4);
    }
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        return leaf::new_error( info<1>{1} );
                    },
                    []( info<1> const & x ) -> int
                    {
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_LEAF_THROW_EXCEPTION();
                    },
                    []
                    {
                        return 1;
                    } );
            },
            []( my_exception const &, info<1> )
            {
                return 3;
            },
            []( my_exception const & )
            {
                return 4;
            },
            []
            {
                return 5;
            } );

        BOOST_TEST_EQ(r, 5);
    }

    ///////////////////////////

    {
        auto error_handlers = std::make_tuple(
            []( info<1> const & x ) -> leaf::result<int>
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_LEAF_THROW_EXCEPTION(my_exception());
            },
            []() -> leaf::result<int>
            {
                return 1;
            } );
        int r = leaf::try_handle_all(
            [&]() -> leaf::result<int>
            {
                return leaf::try_handle_some(
                    [&]() -> leaf::result<int>
                    {
                        return leaf::new_error( info<1>{1} );
                    },
                    error_handlers );
            },
            []( my_exception const &, info<1> )
            {
                return 3;
            },
            []( my_exception const & )
            {
                return 4;
            },
            []
            {
                return 5;
            } );

        BOOST_TEST_EQ(r, 4);
    }
    {
        auto error_handlers = std::make_tuple(
            []( info<1> const & x ) -> leaf::result<int>
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_LEAF_THROW_EXCEPTION();
            },
            []() -> leaf::result<int>
            {
                return 1;
            } );
        int r = leaf::try_handle_all(
            [&]() -> leaf::result<int>
            {
                return leaf::try_handle_some(
                    [&]() -> leaf::result<int>
                    {
                        return leaf::new_error( info<1>{1} );
                    },
                    error_handlers );
            },
            []( my_exception const &, info<1> )
            {
                return 3;
            },
            []( my_exception const & )
            {
                return 4;
            },
            []
            {
                return 5;
            } );

        BOOST_TEST_EQ(r, 5);
    }

    //////////////////////////////////////

    // match_value<> with exceptions, try_handle_some
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                throw leaf::exception( my_exception(42) );
            },
            []( leaf::match_value<my_exception, 42> m )
            {
                return m.matched.value;
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(r.value(), 42);
    }
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                throw my_exception(42);
            },
            []( leaf::match_value<my_exception, 42> m )
            {
                return m.matched.value;
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(r.value(), 42);
    }
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                throw leaf::exception( my_exception(42) );
            },
            []( leaf::match_value<my_exception, 41> m )
            {
                return m.matched.value;
            },
            []( leaf::error_info const & unmatched )
            {
                return unmatched.error();
            } );
        BOOST_TEST(!r);
    }
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                throw my_exception(42);
            },
            []( leaf::match_value<my_exception, 41> m )
            {
                return m.matched.value;
            },
            []( leaf::error_info const & unmatched )
            {
                return unmatched.error();
            } );
        BOOST_TEST(!r);
    }

    //////////////////////////////////////

    // match_value<> with exceptions, try_handle_all
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                throw leaf::exception( my_exception(42) );
            },
            []( leaf::match_value<my_exception, 42> m )
            {
                return m.matched.value;
            },
            []
            {
                return -1;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                throw my_exception(42);
            },
            []( leaf::match_value<my_exception, 42> m )
            {
                return m.matched.value;
            },
            []
            {
                return -1;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                throw leaf::exception( my_exception(42) );
            },
            []( leaf::match_value<my_exception, 41> m )
            {
                return m.matched.value;
            },
            []
            {
                return -1;
            } );
        BOOST_TEST_EQ(r, -1);
    }
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                throw my_exception(42);
            },
            []( leaf::match_value<my_exception, 41> m )
            {
                return m.matched.value;
            },
            []
            {
                return -1;
            } );
        BOOST_TEST_EQ(r, -1);
    }

    return boost::report_errors();
}

#endif
