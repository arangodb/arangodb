// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning(disable: 4702) // unreachable code
# pragma warning(disable: 4577) // noexcept used without /EHsc
#endif

// Make sure that simple inclusion does not
// require boost::throw_exception to be defined

#include <boost/throw_exception.hpp>

int main()
{
}
