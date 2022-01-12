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
#   include <boost/leaf/exception.hpp>
#   include <boost/leaf/on_error.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

struct info { int value; };

struct abstract_base_exception
{
    virtual ~abstract_base_exception() { }
    virtual int get_val() const = 0;
};

struct my_exception:
    std::exception,
    abstract_base_exception
{
    int val;;
    explicit my_exception(int val): val{val} { }
    int get_val() const { return val; }
};

int get_val( abstract_base_exception const & ex )
{
    return ex.get_val();
}

int get_val( my_exception const & ex )
{
    return ex.val;
}

int get_val( leaf::catch_<abstract_base_exception> const & ex )
{
    return ex.matched.get_val();
}

int get_val( leaf::catch_<my_exception> const & ex )
{
    return ex.matched.val;
}

template <class Ex, class F>
int test( F && f )
{
    return leaf::try_catch(
        [&]() -> int
        {
            f();
            return 0;
        },

        []( Ex ex, leaf::match_value<info,42>, leaf::e_source_location )
        {
            BOOST_TEST_EQ(get_val(ex), 42);
            return 20;
        },
        []( Ex ex, leaf::match_value<info,42>, info x )
        {
            BOOST_TEST_EQ(get_val(ex), 42);
            return 21;
        },
        []( Ex ex, leaf::e_source_location )
        {
            BOOST_TEST_EQ(get_val(ex), 42);
            return 22;
        },
        []( Ex ex )
        {
            BOOST_TEST_EQ(get_val(ex), 42);
            return 23;
        },
        []( leaf::match_value<info,42>, leaf::e_source_location )
        {
            return 40;
        },
        []( leaf::match_value<info,42>, info x )
        {
            return 41;
        },
        []( leaf::e_source_location )
        {
            return 42;
        },
        []
        {
            return 43;
        } );
}

int main()
{
    BOOST_TEST_EQ(20, test<leaf::catch_<my_exception>>([]{ BOOST_LEAF_THROW_EXCEPTION(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(20, test<leaf::catch_<my_exception>>([]{ throw BOOST_LEAF_EXCEPTION(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(21, test<leaf::catch_<my_exception>>([]{ throw leaf::exception(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(21, test<leaf::catch_<my_exception>>([]{ my_exception exc(42); throw leaf::exception(exc, info{42}); }));
    BOOST_TEST_EQ(21, test<leaf::catch_<my_exception>>([]{ my_exception const exc(42); throw leaf::exception(exc, info{42}); }));
    BOOST_TEST_EQ(22, test<leaf::catch_<my_exception>>([]{ BOOST_LEAF_THROW_EXCEPTION(my_exception(42)); }));
    BOOST_TEST_EQ(22, test<leaf::catch_<my_exception>>([]{ throw BOOST_LEAF_EXCEPTION(my_exception(42)); }));
    BOOST_TEST_EQ(23, test<leaf::catch_<my_exception>>([]{ throw leaf::exception(my_exception(42)); }));
    BOOST_TEST_EQ(23, test<leaf::catch_<my_exception>>([]{ my_exception exc(42); throw leaf::exception(exc); }));
    BOOST_TEST_EQ(23, test<leaf::catch_<my_exception>>([]{ my_exception const exc(42); throw leaf::exception(exc); }));

    BOOST_TEST_EQ(20, test<my_exception const &>([]{ BOOST_LEAF_THROW_EXCEPTION(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(20, test<my_exception const &>([]{ throw BOOST_LEAF_EXCEPTION(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(21, test<my_exception const &>([]{ throw leaf::exception(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(21, test<my_exception const &>([]{ my_exception exc(42); throw leaf::exception(exc, info{42}); }));
    BOOST_TEST_EQ(21, test<my_exception const &>([]{ my_exception const exc(42); throw leaf::exception(exc, info{42}); }));
    BOOST_TEST_EQ(22, test<my_exception const &>([]{ BOOST_LEAF_THROW_EXCEPTION(my_exception(42)); }));
    BOOST_TEST_EQ(22, test<my_exception const &>([]{ throw BOOST_LEAF_EXCEPTION(my_exception(42)); }));
    BOOST_TEST_EQ(23, test<my_exception const &>([]{ throw leaf::exception(my_exception(42)); }));
    BOOST_TEST_EQ(23, test<my_exception const &>([]{ my_exception exc(42); throw leaf::exception(exc); }));
    BOOST_TEST_EQ(23, test<my_exception const &>([]{ my_exception const exc(42); throw leaf::exception(exc); }));

    BOOST_TEST_EQ(40, test<my_exception &>([]{ BOOST_LEAF_THROW_EXCEPTION(info{42}); }));
    BOOST_TEST_EQ(40, test<my_exception &>([]{ throw BOOST_LEAF_EXCEPTION(info{42}); }));
    BOOST_TEST_EQ(41, test<my_exception &>([]{ throw leaf::exception(info{42}); }));
    BOOST_TEST_EQ(41, test<my_exception &>([]{ info inf{42}; throw leaf::exception(inf); }));
    BOOST_TEST_EQ(41, test<my_exception &>([]{ info const inf{42}; throw leaf::exception(inf); }));
    BOOST_TEST_EQ(42, test<my_exception &>([]{ BOOST_LEAF_THROW_EXCEPTION(); }));
    BOOST_TEST_EQ(42, test<my_exception &>([]{ throw BOOST_LEAF_EXCEPTION(); }));
    BOOST_TEST_EQ(43, test<my_exception &>([]{ throw leaf::exception(); }));
    BOOST_TEST_EQ(23, test<my_exception &>([]{ my_exception exc(42); throw leaf::exception(exc); }));
    BOOST_TEST_EQ(23, test<my_exception &>([]{ my_exception const exc(42); throw leaf::exception(exc); }));

    BOOST_TEST_EQ(20, test<my_exception const>([]{ BOOST_LEAF_THROW_EXCEPTION(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(20, test<my_exception const>([]{ throw BOOST_LEAF_EXCEPTION(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(21, test<my_exception const>([]{ throw leaf::exception(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(21, test<my_exception const>([]{ my_exception exc(42); throw leaf::exception(exc, info{42}); }));
    BOOST_TEST_EQ(21, test<my_exception const>([]{ my_exception const exc(42); throw leaf::exception(exc, info{42}); }));
    BOOST_TEST_EQ(22, test<my_exception const>([]{ BOOST_LEAF_THROW_EXCEPTION(my_exception(42)); }));
    BOOST_TEST_EQ(22, test<my_exception const>([]{ throw BOOST_LEAF_EXCEPTION(my_exception(42)); }));
    BOOST_TEST_EQ(23, test<my_exception const>([]{ throw leaf::exception(my_exception(42)); }));
    BOOST_TEST_EQ(23, test<my_exception const>([]{ my_exception exc(42); throw leaf::exception(exc); }));
    BOOST_TEST_EQ(23, test<my_exception const>([]{ my_exception const exc(42); throw leaf::exception(exc); }));

    BOOST_TEST_EQ(40, test<my_exception>([]{ BOOST_LEAF_THROW_EXCEPTION(info{42}); }));
    BOOST_TEST_EQ(40, test<my_exception>([]{ throw BOOST_LEAF_EXCEPTION(info{42}); }));
    BOOST_TEST_EQ(41, test<my_exception>([]{ throw leaf::exception(info{42}); }));
    BOOST_TEST_EQ(41, test<my_exception>([]{ info inf{42}; throw leaf::exception(inf); }));
    BOOST_TEST_EQ(41, test<my_exception>([]{ info const inf{42}; throw leaf::exception(inf); }));
    BOOST_TEST_EQ(42, test<my_exception>([]{ BOOST_LEAF_THROW_EXCEPTION(); }));
    BOOST_TEST_EQ(42, test<my_exception>([]{ throw BOOST_LEAF_EXCEPTION(); }));
    BOOST_TEST_EQ(43, test<my_exception>([]{ throw leaf::exception(); }));
    BOOST_TEST_EQ(23, test<my_exception>([]{ my_exception exc(42); throw leaf::exception(exc); }));
    BOOST_TEST_EQ(23, test<my_exception>([]{ my_exception const exc(42); throw leaf::exception(exc); }));

    BOOST_TEST_EQ(20, test<leaf::catch_<abstract_base_exception>>([]{ BOOST_LEAF_THROW_EXCEPTION(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(20, test<leaf::catch_<abstract_base_exception>>([]{ throw BOOST_LEAF_EXCEPTION(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(21, test<leaf::catch_<abstract_base_exception>>([]{ throw leaf::exception(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(21, test<leaf::catch_<abstract_base_exception>>([]{ my_exception exc(42); throw leaf::exception(exc, info{42}); }));
    BOOST_TEST_EQ(21, test<leaf::catch_<abstract_base_exception>>([]{ my_exception const exc(42); throw leaf::exception(exc, info{42}); }));
    BOOST_TEST_EQ(22, test<leaf::catch_<abstract_base_exception>>([]{ BOOST_LEAF_THROW_EXCEPTION(my_exception(42)); }));
    BOOST_TEST_EQ(22, test<leaf::catch_<abstract_base_exception>>([]{ throw BOOST_LEAF_EXCEPTION(my_exception(42)); }));
    BOOST_TEST_EQ(23, test<leaf::catch_<abstract_base_exception>>([]{ throw leaf::exception(my_exception(42)); }));
    BOOST_TEST_EQ(23, test<leaf::catch_<abstract_base_exception>>([]{ my_exception exc(42); throw leaf::exception(exc); }));
    BOOST_TEST_EQ(23, test<leaf::catch_<abstract_base_exception>>([]{ my_exception const exc(42); throw leaf::exception(exc); }));

    BOOST_TEST_EQ(20, test<abstract_base_exception const &>([]{ BOOST_LEAF_THROW_EXCEPTION(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(20, test<abstract_base_exception const &>([]{ throw BOOST_LEAF_EXCEPTION(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(21, test<abstract_base_exception const &>([]{ throw leaf::exception(my_exception(42), info{42}); }));
    BOOST_TEST_EQ(21, test<abstract_base_exception const &>([]{ my_exception exc(42); throw leaf::exception(exc, info{42}); }));
    BOOST_TEST_EQ(21, test<abstract_base_exception const &>([]{ my_exception const exc(42); throw leaf::exception(exc, info{42}); }));
    BOOST_TEST_EQ(22, test<abstract_base_exception const &>([]{ BOOST_LEAF_THROW_EXCEPTION(my_exception(42)); }));
    BOOST_TEST_EQ(22, test<abstract_base_exception const &>([]{ throw BOOST_LEAF_EXCEPTION(my_exception(42)); }));
    BOOST_TEST_EQ(23, test<abstract_base_exception const &>([]{ throw leaf::exception(my_exception(42)); }));
    BOOST_TEST_EQ(23, test<abstract_base_exception const &>([]{ my_exception exc(42); throw leaf::exception(exc); }));
    BOOST_TEST_EQ(23, test<abstract_base_exception const &>([]{ my_exception const exc(42); throw leaf::exception(exc); }));

    BOOST_TEST_EQ(40, test<abstract_base_exception &>([]{ BOOST_LEAF_THROW_EXCEPTION(info{42}); }));
    BOOST_TEST_EQ(40, test<abstract_base_exception &>([]{ throw BOOST_LEAF_EXCEPTION(info{42}); }));
    BOOST_TEST_EQ(41, test<abstract_base_exception &>([]{ throw leaf::exception(info{42}); }));
    BOOST_TEST_EQ(41, test<abstract_base_exception &>([]{ info inf{42}; throw leaf::exception(inf); }));
    BOOST_TEST_EQ(41, test<abstract_base_exception &>([]{ info const inf{42}; throw leaf::exception(inf); }));
    BOOST_TEST_EQ(42, test<abstract_base_exception &>([]{ BOOST_LEAF_THROW_EXCEPTION(); }));
    BOOST_TEST_EQ(42, test<abstract_base_exception &>([]{ throw BOOST_LEAF_EXCEPTION(); }));
    BOOST_TEST_EQ(43, test<abstract_base_exception &>([]{ throw leaf::exception(); }));
    BOOST_TEST_EQ(23, test<abstract_base_exception &>([]{ my_exception exc(42); throw leaf::exception(exc); }));
    BOOST_TEST_EQ(23, test<abstract_base_exception &>([]{ my_exception const exc(42); throw leaf::exception(exc); }));

    {
        char const * wh = 0;
        leaf::try_catch(
            []
            {
                throw std::runtime_error("Test");
            },
            [&]( std::exception const & ex )
            {
                wh = ex.what();
            } );
        BOOST_TEST(wh!=0 || !strcmp(wh,"Test"));
    }

    {
        int const id = leaf::leaf_detail::current_id();
        BOOST_TEST_EQ( 21, test<my_exception const &>( []
        {
            auto load = leaf::on_error(info{42});
            throw my_exception(42);
        } ) );
        BOOST_TEST_NE(id, leaf::leaf_detail::current_id());
    }

    {
        int const id = leaf::leaf_detail::current_id();
        BOOST_TEST_EQ( 21, test<my_exception &>( []
        {
            auto load = leaf::on_error(info{42});
            throw my_exception(42);
        } ) );
        BOOST_TEST_NE(id, leaf::leaf_detail::current_id());
    }

    {
        BOOST_TEST_EQ( 23, test<my_exception const &>( []
        {
            int const id = leaf::leaf_detail::current_id();
            try
            {
                leaf::try_catch(
                    []
                    {
                        throw my_exception(42);
                    } );
            }
            catch(...)
            {
                BOOST_TEST_EQ(id, leaf::leaf_detail::current_id());
                throw;
            }
        } ) );
    }

    {
        BOOST_TEST_EQ( 23, test<my_exception &>( []
        {
            int const id = leaf::leaf_detail::current_id();
            try
            {
                leaf::try_catch(
                    []
                    {
                        throw my_exception(42);
                    } );
            }
            catch(...)
            {
                BOOST_TEST_EQ(id, leaf::leaf_detail::current_id());
                throw;
            }
        } ) );
    }

    {
        leaf::try_catch(
            []
            {
                throw leaf::exception( info{42} );
            },
            []( info x )
            {
                BOOST_TEST_EQ(x.value, 42);
            } );
        int r = leaf::try_catch(
            []() -> int
            {
                throw std::exception();
            },
            []( info x )
            {
                return -1;
            },
            []
            {
                return 1;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    return boost::report_errors();
}

#endif
