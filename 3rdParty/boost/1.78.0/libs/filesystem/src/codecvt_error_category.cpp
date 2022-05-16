//  codecvt_error_category implementation file  ----------------------------------------//

//  Copyright Beman Dawes 2009

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt)

//  Library home page at http://www.boost.org/libs/filesystem

//--------------------------------------------------------------------------------------//

#include "platform_config.hpp"

#include <boost/config/warning_disable.hpp>

#include <boost/filesystem/config.hpp>
#include <boost/filesystem/path_traits.hpp>
#include <boost/system/error_code.hpp>
#include <locale>
#include <string>
#include <vector>
#include <cstdlib>
#include <cassert>

//--------------------------------------------------------------------------------------//

namespace boost {
namespace filesystem {

namespace {

class codecvt_error_cat BOOST_FINAL :
    public boost::system::error_category
{
public:
#if !defined(BOOST_CLANG) || __clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ > 8)
    BOOST_DEFAULTED_FUNCTION(codecvt_error_cat(), {})
#else
    // clang up to version 3.8 requires a user-defined default constructor in order to be able to declare a static constant of the error category.
    codecvt_error_cat() {}
#endif
    const char* name() const BOOST_SYSTEM_NOEXCEPT BOOST_OVERRIDE;
    std::string message(int ev) const BOOST_OVERRIDE;
};

const char* codecvt_error_cat::name() const BOOST_SYSTEM_NOEXCEPT
{
    return "codecvt";
}

std::string codecvt_error_cat::message(int ev) const
{
    std::string str;
    switch (ev)
    {
    case std::codecvt_base::ok:
        str = "ok";
        break;
    case std::codecvt_base::partial:
        str = "partial";
        break;
    case std::codecvt_base::error:
        str = "error";
        break;
    case std::codecvt_base::noconv:
        str = "noconv";
        break;
    default:
        str = "unknown error";
        break;
    }
    return str;
}

} // unnamed namespace

BOOST_FILESYSTEM_DECL boost::system::error_category const& codecvt_error_category()
{
    static const codecvt_error_cat codecvt_error_cat_const;
    return codecvt_error_cat_const;
}

} // namespace filesystem
} // namespace boost
