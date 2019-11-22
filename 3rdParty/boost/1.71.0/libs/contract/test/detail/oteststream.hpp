
#ifndef BOOST_CONTRACT_TEST_DETAIL_OTESTSTREAM_HPP_
#define BOOST_CONTRACT_TEST_DETAIL_OTESTSTREAM_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/iostreams/tee.hpp>
#include <boost/iostreams/stream.hpp>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace boost { namespace contract { namespace test { namespace detail {

namespace oteststream_ {
    struct oss_base { // Wrap oss data member for proper initialization order.
    protected:
        std::ostringstream oss_;
    };
}

// Print to clog plus build internal string (using ostringstream) for checking.
struct oteststream :
    private oteststream_::oss_base,
    public boost::iostreams::stream<boost::iostreams::tee_device<std::ostream,
            std::ostringstream> >
{
    oteststream() :
        oteststream_::oss_base(),
        boost::iostreams::stream<boost::iostreams::tee_device<
                std::ostream, std::ostringstream> >(
            boost::iostreams::tee_device<std::ostream, std::ostringstream>(
                    std::clog, oss_)
        )
    {}

    std::string str() const { return oss_.str(); }
    void str(std::string const& s) { oss_.str(s); }

    bool eq(std::string const& s) { return eq(str(), s); }
    
    // Also display mismatching characters.
    static bool eq(std::string const& r, std::string const& s) {
        std::string::size_type i = 0;
        for(; i < r.size() && i < s.size(); ++i) if(r[i] != s[i]) break;
        if(i < r.size() || i < s.size()) {
            std::cout << std::endl;
            std::cout <<
                "Error: Following strings differ at position " << i <<
                ", because '" << r[i] << "' != '" << s[i] << "':" << std::endl
            ;
            std::cout << std::endl;
            std::cout
                << r.substr(0, i)
                << "(((" << r[i] << ")))"
                // Extra () to avoid clashes with MSVC min macro.
                << r.substr((std::min)(i + 1, r.size()), r.size())
                << std::endl
            ;
            std::cout << std::endl;
            std::cout
                << s.substr(0, i)
                << "(((" << s[i] << ")))"
                // Extra () to avoid clashes with MSVC min macro.
                << s.substr((std::min)(i + 1, s.size()), s.size())
                << std::endl
            ;
            std::cout << std::endl;
            return false;
        }
        return true;
    }
};

} } } } // namespace

#endif // #include guard

