/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman
    Copyright (c) 2001-2011 Hartmut Kaiser
    http://spirit.sourceforge.net/

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_SIGN_MAR_11_2009_0734PM)
#define BOOST_SPIRIT_X3_SIGN_MAR_11_2009_0734PM

#include <boost/math/special_functions/sign.hpp>

namespace boost { namespace spirit { namespace x3
{
    template<typename T>
    inline bool (signbit)(T x)
    {
        return (boost::math::signbit)(x) ? true : false;
    }

    template<typename T>
    inline T (changesign)(T x)
    {
        return (boost::math::changesign)(x);
    }

}}}

#endif
