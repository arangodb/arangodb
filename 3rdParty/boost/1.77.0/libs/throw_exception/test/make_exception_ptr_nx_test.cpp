// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning(disable: 4702) // unreachable code
# pragma warning(disable: 4577) // noexcept used without /EHsc
#endif

//#include <boost/exception_ptr.hpp>

#include <boost/throw_exception.hpp>
#include <boost/exception/exception.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

typedef boost::shared_ptr<boost::exception_detail::clone_base const> exception_ptr;

template<class E> exception_ptr make_exception_ptr( E const& e )
{
    return boost::make_shared< boost::wrapexcept<E> >( e );
}

class my_exception: public std::exception {};

int main()
{
    ::make_exception_ptr( my_exception() );
}

namespace boost
{

// shared_ptr needs this
void throw_exception( std::exception const & )
{
    std::exit( 1 );
}

} // namespace boost
