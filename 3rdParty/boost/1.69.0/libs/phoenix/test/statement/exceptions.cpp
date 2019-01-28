/*=============================================================================
    Copyright (c) 2005-2007 Dan Marsden
    Copyright (c) 2005-2007 Joel de Guzman
    Copyright (c) 2015      John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <stdexcept>
#include <string>

#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/statement.hpp>
#include <boost/phoenix/scope/local_variable.hpp>
#include <boost/phoenix/bind/bind_member_function.hpp>

#include <boost/detail/lightweight_test.hpp>

struct base_exception : std::exception
{
    explicit
    base_exception(char const *msg)
        : _sliced(true), _what(msg)
    {}

    base_exception(base_exception const &other)
        : _sliced(true), _what(other._what)
    {
    }

    char const *what() const BOOST_NOEXCEPT_OR_NOTHROW
    {
        if (_sliced) { return "sliced ..."; }
        return _what;
    }

    bool        _sliced;
    char const *_what;
};

struct extended_exception : base_exception
{
    explicit
    extended_exception(char const *msg)
        : base_exception(msg)
    {
        // mark as not sliced
        _sliced = false;
    }

    extended_exception(extended_exception const &other)
        : base_exception(other)
    {
        // mark as not sliced
        _sliced = false;
    }
};

int main()
{
    using boost::phoenix::throw_;
    using boost::phoenix::try_;
    using boost::phoenix::ref;
    using boost::phoenix::local_names::_e;
    using boost::phoenix::bind;
    using std::exception;
    using std::string;
    using std::runtime_error;
    namespace phx = boost::phoenix;

    {
        try
        {
            throw_(runtime_error("error"))();
            BOOST_ERROR("exception should have been thrown");
        }
        catch(runtime_error& err)
        {
            BOOST_TEST(err.what() == string("error"));
        }
    }

    {
        try
        {
            try
            {
                throw runtime_error("error");
            }
            catch(exception&)
            {
                throw_()();
                BOOST_ERROR("exception should have been rethrown");
            }
        }
        catch(exception& err)
        {
            BOOST_TEST(err.what() == string("error"));
        }
    }

    {
        bool caught_exception = false;

        try_
        [ throw_(runtime_error("error")) ]
        .catch_<exception>(_e) // captured but unused
        [
            ref(caught_exception) = true
        ]();

        BOOST_TEST(caught_exception);
    }

    {
        bool caught_exception = false;
        string what;

        try_
        [ throw_(runtime_error("error")) ]
        .catch_<exception>(_e)
        [
            ref(caught_exception) = true
            // ambiguous with std::ref
          , phx::ref(what) = phx::bind(&exception::what, _e)
        ]();

        BOOST_TEST(caught_exception);
        BOOST_TEST(what == string("error"));
    }

    {
        bool caught_exception = false;
        string what;

        try_
        [ throw_(extended_exception("error")) ]
        .catch_<base_exception>(_e) // A thrown object should not be copied due to slicing.
        [
            ref(caught_exception) = true
            // ambiguous with std::ref
          , phx::ref(what) = phx::bind(&exception::what, _e)
        ]();

        BOOST_TEST(caught_exception);
        BOOST_TEST(what == string("error"));
    }

    {
        bool caught_exception = false;

        try_
        [ throw_(runtime_error("error")) ]
        .catch_all
        [ ref(caught_exception) = true ]();
        BOOST_TEST(caught_exception);
    }

    {
        bool caught_correct_exception = false;
        string what;

        try_
            [ throw_(runtime_error("error")) ]
        .catch_<string>()
            [ ref(caught_correct_exception) = false ]
        .catch_<exception>(_e)
        [
            ref(caught_correct_exception) = true
            // ambiguous with std::ref
          , phx::ref(what) = phx::bind(&exception::what, _e)
        ]();

        BOOST_TEST(caught_correct_exception);
        BOOST_TEST(what == string("error"));
    }

    {
        bool caught_correct_exception = false;

        try_
            [ throw_(runtime_error("error")) ]
        .catch_<string>()
            [ ref(caught_correct_exception) = false ]
        .catch_all
            [ ref(caught_correct_exception) = true]();

        BOOST_TEST(caught_correct_exception);
    }

    return boost::report_errors();
}
