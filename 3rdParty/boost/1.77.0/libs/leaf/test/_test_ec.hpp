#ifndef BOOST_LEAF_TEST_EC_HPP_INCLUDED
#define BOOST_LEAF_TEST_EC_HPP_INCLUDED

// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <system_error>

enum class errc_a { a0 = 10, a1, a2, a3 };

namespace std { template <> struct is_error_code_enum<errc_a>: true_type { }; }

inline std::error_category const & cat_errc_a()
{
    class cat : public std::error_category
    {
        char const * name() const noexcept
        {
            return "errc_a";
        }
        std::string message(int code) const
        {
            switch( errc_a(code) )
            {
                case errc_a::a0: return "a0";
                case errc_a::a1: return "a1";
                case errc_a::a2: return "a2";
                case errc_a::a3: return "a3";
                default: return "error";
            }
        }
    };
    static cat c;
    return c;
}

inline std::error_code make_error_code( errc_a code )
{
    return std::error_code(int(code), cat_errc_a());
}

template <int> struct e_errc_a { std::error_code value; };

///////////////////////////////////

enum class errc_b { b0 = 20, b1, b2, b3 };

namespace std { template <> struct is_error_code_enum<errc_b>: true_type { }; };

inline std::error_category const & cat_errc_b()
{
    class cat : public std::error_category
    {
        char const * name() const noexcept
        {
            return "errc_b";
        }
        std::string message(int code) const
        {
            switch( errc_b(code) )
            {
                case errc_b::b0: return "b0";
                case errc_b::b1: return "b1";
                case errc_b::b2: return "b2";
                case errc_b::b3: return "b3";
                default: return "error";
            }
        }
    };
    static cat c;
    return c;
}

inline std::error_code make_error_code( errc_b code )
{
    return std::error_code(int(code), cat_errc_b());
}

template <int> struct e_errc_b { std::error_code value; };

///////////////////////////////////

enum class cond_x { x00 = 30, x11, x22, x33 };

namespace std { template <> struct is_error_condition_enum<cond_x>: true_type { }; };

inline std::error_category const & cat_cond_x()
{
    class cat : public std::error_category
    {
        char const * name() const noexcept
        {
            return "cond_x";
        }
        std::string message(int cond) const
        {
            switch( cond_x(cond) )
            {
                case cond_x::x00:
                    return "x00";
                case cond_x::x11:
                    return "x11";
                case cond_x::x22:
                    return "x22";
                case cond_x::x33:
                    return "x33";
                default:
                    return "error";
            }
        }
        bool equivalent(std::error_code const & code, int cond) const noexcept
        {
            switch( cond_x(cond) )
            {
                case cond_x::x00:
                    return code==make_error_code(errc_a::a0) || code==make_error_code(errc_b::b0);
                case cond_x::x11:
                    return code==make_error_code(errc_a::a1) || code==make_error_code(errc_b::b1);
                case cond_x::x22:
                    return code==make_error_code(errc_a::a2) || code==make_error_code(errc_b::b2);
                case cond_x::x33:
                    return code==make_error_code(errc_a::a3) || code==make_error_code(errc_b::b3);
                default:
                    return false;
            }
        }
    };
    static cat c;
    return c;
}

inline std::error_condition make_error_condition( cond_x cond )
{
    return std::error_condition(int(cond), cat_cond_x());
}

template <int> struct e_cond_x { cond_x value; };

///////////////////////////////////

enum class cond_y { y03 = 40, y12, y21, y30 };

namespace std { template <> struct is_error_condition_enum<cond_y>: true_type { }; };

inline std::error_category const & cat_cond_y()
{
    class cat : public std::error_category
    {
        char const * name() const noexcept
        {
            return "cond_y";
        }
        std::string message( int cond ) const
        {
            switch( cond_y(cond) )
            {
                case cond_y::y03:
                    return "y03";
                case cond_y::y12:
                    return "y12";
                case cond_y::y21:
                    return "y21";
                case cond_y::y30:
                    return "y30";
                default:
                    return "error";
            }
        }
        bool equivalent(std::error_code const & code, int cond) const noexcept
        {
            switch( cond_y(cond) )
            {
                case cond_y::y03:
                    return code==make_error_code(errc_a::a0) || code==make_error_code(errc_b::b0);
                case cond_y::y12:
                    return code==make_error_code(errc_a::a1) || code==make_error_code(errc_b::b1);
                case cond_y::y21:
                    return code==make_error_code(errc_a::a2) || code==make_error_code(errc_b::b2);
                case cond_y::y30:
                    return code==make_error_code(errc_a::a3) || code==make_error_code(errc_b::b3);
                default:
                    return false;
            }
        }
    };
    static cat c;
    return c;
}

inline std::error_condition make_error_condition( cond_y cond )
{
    return std::error_condition(int(cond), cat_cond_y());
}

template <int> struct e_cond_y { cond_y value; };

#endif
