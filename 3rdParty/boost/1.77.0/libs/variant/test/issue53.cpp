//-----------------------------------------------------------------------------
// boost-libs variant/test/issue53.cpp source file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2019-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Test case from https://github.com/boostorg/variant/issues/53

#include <boost/variant.hpp>
#include <boost/thread/lock_guard.hpp> // this line was causing problems on MSVC

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

struct spanac {};

struct ceapa{
    double a,b;
};

typedef boost::variant<spanac, ceapa> var_t;

struct visitor_t : public boost::static_visitor<bool> {
    bool operator() (const spanac&) const {
        return true;
    }

    bool operator() (const ceapa&) const {
        return false;
    }

private:
    double a, b;
};

var_t get(int k) {
    if (k)
        return spanac();
    else
        return ceapa();
}

int main(int argc, const char** argv) {
    visitor_t v;

    bool result = boost::apply_visitor(v, get(argc - 1));
    (void)result;
}

#else // #ifndef BOOST_NO_CXX11_RVALUE_REFERENCES

int main() {}

#endif // #ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
