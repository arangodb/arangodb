// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/handle_errors.hpp>
#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

#ifdef BOOST_LEAF_NO_EXCEPTIONS

int main()
{
    int r = leaf::try_catch(
        []
        {
            return 42;
        },
        []
        {
            return 1;
        } );
    BOOST_TEST_EQ(r, 42);

    return boost::report_errors();
}

#else

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/pred.hpp>
#endif

template <int> struct info { int value; };

struct error1: std::exception { };
struct error2: std::exception { };
struct error3: std::exception { };

struct exc_val: std::exception { int value; explicit exc_val(int v): value(v) { } };

template <class R,class Ex>
R failing( Ex && ex )
{
    throw leaf::exception(std::move(ex), info<1>{1}, info<2>{2}, info<3>{3});
}

template <class R>
R succeeding()
{
    return R(42);
}

int main()
{
    // void, try_catch (success)
    {
        int c=0;
        leaf::try_catch(
            [&c]
            {
                c = succeeding<int>();
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 1;
            } );
        BOOST_TEST_EQ(c, 42);
    }

    // void, try_catch (failure), match_enum (single enum value)
    {
        int c=0;
        leaf::try_catch(
            [&c]
            {
                c = failing<int>(error1());
            },
            [&c]( error2 const & )
            {
                BOOST_TEST_EQ(c, 0);
                c = 1;
            },
            [&c]( error1 const &, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    // void, try_catch (failure), match_enum (multiple enum values)
    {
        int c=0;
        leaf::try_catch(
            [&c]
            {
                c = failing<int>(error1());
            },
            [&c]( error2 const & )
            {
                BOOST_TEST_EQ(c, 0);
                c = 1;
            },
            [&c]( leaf::catch_<error2,error1>, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    // void, try_catch (failure), match_value (single value)
    {
        int c=0;
        leaf::try_catch(
            [&c]
            {
                c = failing<int>(error1());
            },
            [&c]( error2 const & )
            {
                BOOST_TEST_EQ(c, 0);
                c = 1;
            },
            [&c]( error1 const &, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    // void, try_catch (failure), match_value (multiple values)
    {
        int c=0;
        leaf::try_catch(
            [&c]
            {
                c = failing<int>(error1());
            },
            [&c]( error2 const & )
            {
                BOOST_TEST_EQ(c, 0);
                c = 1;
            },
            [&c]( leaf::catch_<error2,error1>, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    //////////////////////////////////////

    // void, handle_some (failure, initially not matched), match_enum (single enum value)
    {
        int c=0;
        leaf::try_catch(
            [&c]
            {
                leaf::try_catch(
                    [&c]
                    {
                        c = failing<int>(error1());
                    },
                    [&c]( error2 const & )
                    {
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(false);
            },
            [&c]( error1 const &, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    // void, handle_some (failure, initially not matched), match_enum (multiple enum values)
    {
        int c=0;
        leaf::try_catch(
            [&c]
            {
                leaf::try_catch(
                    [&c]
                    {
                        c = failing<int>(error1());
                    },
                    [&c]( error2 const & )
                    {
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(false);
            },
            [&c]( leaf::catch_<error2,error1>, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    // void, handle_some (failure, initially matched), match_enum (single enum value)
    {
        int c=0;
        leaf::try_catch(
            [&c]
            {
                leaf::try_catch(
                    [&c]
                    {
                        c = failing<int>(error1());
                    },
                    [&c]( error1 const &, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
            },
            [&c]( error2 const & )
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 1);
    }

    // void, handle_some (failure, initially matched), match_enum (multiple enum values)
    {
        int c=0;
        leaf::try_catch(
            [&c]
            {
                leaf::try_catch(
                    [&c]
                    {
                        c = failing<int>(error1());
                    },
                    [&c]( leaf::catch_<error2,error1>, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
            },
            [&c]( error2 const & )
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 1);
    }

    //////////////////////////////////////

    // int, try_catch (success)
    {
        int r = leaf::try_catch(
            []
            {
                return succeeding<int>();
            },
            []
            {
                return 1;
            } );
        BOOST_TEST_EQ(r, 42);
    }

    // int, try_catch (failure), match_enum (single enum value)
    {
        int r = leaf::try_catch(
            []
            {
                return failing<int>(error1());
            },
            []( error2 const & )
            {
                return 1;
            },
            []( error1 const &, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    // int, try_catch (failure), match_enum (multiple enum values)
    {
        int r = leaf::try_catch(
            []
            {
                return failing<int>(error1());
            },
            []( error2 const & )
            {
                return 1;
            },
            []( leaf::catch_<error2,error1>, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    //////////////////////////////////////

    // int, handle_some (failure, matched), match_enum (single enum value)
    {
        int r = leaf::try_catch(
            []
            {
                return failing<int>(error1());
            },
            []( error2 const & )
            {
                return 1;
            },
            []( error1 const &, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    // int, handle_some (failure, matched), match_enum (multiple enum values)
    {
        int r = leaf::try_catch(
            []
            {
                return failing<int>(error1());
            },
            []( error2 const & )
            {
                return 1;
            },
            []( leaf::catch_<error2,error1>, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    // int, handle_some (failure, initially not matched), match_enum (single enum value)
    {
        int r = leaf::try_catch(
            []
            {
                int r = leaf::try_catch(
                    []
                    {
                        return failing<int>(error1());
                    },
                    []( error2 const & )
                    {
                        return 1;
                    } );
                BOOST_TEST(false);
                return r;
            },
            []( error1 const &, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    // int, handle_some (failure, initially not matched), match_enum (multiple enum values)
    {
        int r = leaf::try_catch(
            []
            {
                int r = leaf::try_catch(
                    []
                    {
                        return failing<int>(error1());
                    },
                    []( error2 const & )
                    {
                        return 1;
                    } );
                BOOST_TEST(false);
                return r;
            },
            []( leaf::catch_<error2,error1>, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    // int, handle_some (failure, initially matched), match_enum (single enum value)
    {
        int r = leaf::try_catch(
            []
            {
                int r = leaf::try_catch(
                    []
                    {
                        return failing<int>(error1());
                    },
                    []( error1 const &, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        return 1;
                    } );
                BOOST_TEST_EQ(r, 1);
                return r;
            },
            []( error1 const & )
            {
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    // int, handle_some (failure, initially matched), match_enum (multiple enum values)
    {
        int r = leaf::try_catch(
            []
            {
                int r = leaf::try_catch(
                    []
                    {
                        return failing<int>(error1());
                    },
                    []( leaf::catch_<error2,error1>, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        return 1;
                    } );
                BOOST_TEST_EQ(r, 1);
                return r;
            },
            []( error1 const & )
            {
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    //////////////////////////////////////

    // match<> with exceptions
    {
        int r = leaf::try_catch(
            []
            {
                throw leaf::exception(exc_val{42});
                return 0;
            },
            []( leaf::match_value<exc_val, 42> )
            {
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }
    {
        int r = leaf::try_catch(
            []
            {
                throw leaf::exception(exc_val{42});
                return 0;
            },
            []( leaf::match_value<exc_val, 41> )
            {
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 2);
    }
    {
        int r = leaf::try_catch(
            []
            {
                throw exc_val{42};
                return 0;
            },
            []( leaf::match_value<exc_val, 42> )
            {
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }
    {
        int r = leaf::try_catch(
            []
            {
                throw exc_val{42};
                return 0;
            },
            []( leaf::match_value<exc_val, 41> )
            {
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    //////////////////////////////////////

    return boost::report_errors();
}

#endif
