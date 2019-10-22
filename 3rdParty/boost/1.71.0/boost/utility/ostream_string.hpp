/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef BOOST_UTILITY_OSTREAM_STRING_HPP
#define BOOST_UTILITY_OSTREAM_STRING_HPP

#include <boost/config.hpp>
#include <iosfwd>
#include <cstddef>

namespace boost {
namespace detail {

template<class charT, class traits>
inline std::size_t
oss_put(std::basic_ostream<charT, traits>& os, const charT* data,
    std::size_t size)
{
    return static_cast<std::size_t>(os.rdbuf()->sputn(data, size));
}

template<class charT, class traits>
inline bool
oss_fill(std::basic_ostream<charT, traits>& os, std::size_t size)
{
    charT c = os.fill();
    charT fill[] = { c, c, c, c, c, c, c, c };
    enum {
        chunk = sizeof fill / sizeof(charT)
    };
    for (; size > chunk; size -= chunk) {
        if (boost::detail::oss_put(os, fill, chunk) != chunk) {
            return false;
        }
    }
    return boost::detail::oss_put(os, fill, size) == size;
}

template<class charT, class traits>
class oss_guard {
public:
    explicit oss_guard(std::basic_ostream<charT, traits>& os) BOOST_NOEXCEPT
        : os_(&os) { }
    ~oss_guard() BOOST_NOEXCEPT_IF(false) {
        if (os_) {
            os_->setstate(std::basic_ostream<charT, traits>::badbit);
        }
    }
    void release() BOOST_NOEXCEPT {
        os_ = 0;
    }
private:
    oss_guard(const oss_guard&);
    oss_guard& operator=(const oss_guard&);
    std::basic_ostream<charT, traits>* os_;
};

} /* detail */

template<class charT, class traits>
inline std::basic_ostream<charT, traits>&
ostream_string(std::basic_ostream<charT, traits>& os, const charT* data,
    std::size_t size)
{
    typedef std::basic_ostream<charT, traits> stream;
    detail::oss_guard<charT, traits> guard(os);
    typename stream::sentry entry(os);
    if (entry) {
        std::size_t width = static_cast<std::size_t>(os.width());
        if (width <= size) {
            if (detail::oss_put(os, data, size) != size) {
                return os;
            }
        } else if ((os.flags() & stream::adjustfield) == stream::left) {
            if (detail::oss_put(os, data, size) != size ||
                !detail::oss_fill(os, width - size)) {
                return os;
            }
        } else if (!detail::oss_fill(os, width - size) ||
            detail::oss_put(os, data, size) != size) {
            return os;
        }
        os.width(0);
    }
    guard.release();
    return os;
}

} /* boost */

#endif
