//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/string.hpp>

template class ::boost::container::basic_string<char>;
volatile ::boost::container::basic_string<char> dummy;

#include <boost/container/vector.hpp>
#include "dummy_test_allocator.hpp"

namespace boost {
namespace container {

typedef test::simple_allocator<char>           SimpleCharAllocator;
typedef basic_string<char, std::char_traits<char>, SimpleCharAllocator> SimpleString;
typedef test::simple_allocator<SimpleString>    SimpleStringAllocator;
typedef test::simple_allocator<wchar_t>              SimpleWCharAllocator;
typedef basic_string<wchar_t, std::char_traits<wchar_t>, SimpleWCharAllocator> SimpleWString;
typedef test::simple_allocator<SimpleWString>         SimpleWStringAllocator;

//Explicit instantiations of container::basic_string
template class basic_string<char,    std::char_traits<char>, SimpleCharAllocator>;
template class basic_string<wchar_t, std::char_traits<wchar_t>, SimpleWCharAllocator>;
template class basic_string<char,    std::char_traits<char>, std::allocator<char> >;
template class basic_string<wchar_t, std::char_traits<wchar_t>, std::allocator<wchar_t> >;

//Explicit instantiation of container::vectors of container::strings
template class vector<SimpleString, SimpleStringAllocator>;
template class vector<SimpleWString, SimpleWStringAllocator>;

}}

int main()
{
   return 0;
}
