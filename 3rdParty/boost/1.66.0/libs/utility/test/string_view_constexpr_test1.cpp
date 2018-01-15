/*
   Copyright (c) Marshall Clow 2017-2017.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <new>        // for placement new
#include <iostream>
#include <cstddef>    // for NULL, std::size_t, std::ptrdiff_t
#include <cstring>    // for std::strchr and std::strcmp
#include <cstdlib>    // for std::malloc and std::free

#include <boost/config.hpp>
#include <boost/utility/string_view.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>


#if __cplusplus < 201402L
BOOST_AUTO_TEST_CASE( test_main ) {}
#else

struct constexpr_char_traits
{
    typedef char		    char_type;
    typedef int   			int_type;
    typedef std::streamoff	off_type;
    typedef std::streampos	pos_type;
    typedef std::mbstate_t	state_type;

    static void assign(char_type& c1, const char_type& c2) noexcept { c1 = c2; }
    static constexpr bool eq(char_type c1, char_type c2) noexcept   { return c1 == c2; }
    static constexpr bool lt(char_type c1, char_type c2) noexcept   { return c1 < c2; }

    static constexpr int              compare(const char_type* s1, const char_type* s2, size_t n) noexcept;
    static constexpr size_t           length(const char_type* s) noexcept;
    static constexpr const char_type* find(const char_type* s, size_t n, const char_type& a) noexcept;
    static constexpr char_type*       move(char_type* s1, const char_type* s2, size_t n) noexcept;
    static constexpr char_type*       copy(char_type* s1, const char_type* s2, size_t n) noexcept;
    static constexpr char_type*       assign(char_type* s, size_t n, char_type a) noexcept;

    static constexpr int_type  not_eof(int_type c) noexcept { return eq_int_type(c, eof()) ? ~eof() : c; }
    static constexpr char_type to_char_type(int_type c) noexcept              { return char_type(c); }
    static constexpr int_type  to_int_type(char_type c) noexcept              { return int_type(c); }
    static constexpr bool      eq_int_type(int_type c1, int_type c2) noexcept { return c1 == c2; }
    static constexpr int_type  eof() noexcept                                 { return EOF; }
};

//	yields:
//		0 if for each i in [0,n), X::eq(s1[i],s2[i]) is true; 
//		else, a negative value if, for some j in [0,n), X::lt(s1[j],s2[j]) is true and
//			for each i in [0,j) X::eq(s2[i],s2[i]) is true;
//		else a positive value.
constexpr int constexpr_char_traits::compare(const char_type* s1, const char_type* s2, size_t n) noexcept
{
    for (; n != 0; --n, ++s1, ++s2)
    {
        if (lt(*s1, *s2))
            return -1;
        if (lt(*s2, *s1))
            return 1;
    }
    return 0;
}

//	yields: the smallest i such that X::eq(s[i],charT()) is true.
constexpr size_t constexpr_char_traits::length(const char_type* s) noexcept
{
    size_t len = 0;
    for (; !eq(*s, char_type(0)); ++s)
        ++len;
    return len;
}

typedef boost::basic_string_view<char, constexpr_char_traits> string_view;

BOOST_AUTO_TEST_CASE( test_main )
{
    constexpr string_view sv1;
    constexpr string_view sv2{"abc", 3}; // ptr, len
    constexpr string_view sv3{"def"}; 	 // ptr

	constexpr const char *s1 = "";
	constexpr const char *s2 = "abc";
	
	static_assert( (sv1 == sv1), "" );
	
	static_assert(!(sv1 == sv2), "" );    
	static_assert( (sv1 != sv2), "" );    
	static_assert( (sv1 <  sv2), "" );    
	static_assert( (sv1 <= sv2), "" );    
	static_assert(!(sv1 >  sv2), "" );    
	static_assert(!(sv1 >= sv2), "" );    

	static_assert(!(s1 == sv2), "" );    
	static_assert( (s1 != sv2), "" );    
	static_assert( (s1 <  sv2), "" );    
	static_assert( (s1 <= sv2), "" );    
	static_assert(!(s1 >  sv2), "" );    
	static_assert(!(s1 >= sv2), "" );    

	static_assert(!(sv1 == s2), "" );    
	static_assert( (sv1 != s2), "" );    
	static_assert( (sv1 <  s2), "" );    
	static_assert( (sv1 <= s2), "" );    
	static_assert(!(sv1 >  s2), "" );    
	static_assert(!(sv1 >= s2), "" );    

	static_assert( sv1.compare(sv2)  < 0, "" );    
	static_assert( sv1.compare(sv1) == 0, "" );    
	static_assert( sv3.compare(sv1)  > 0, "" );    

	static_assert( sv1.compare(s2)  < 0, "" );    
	static_assert( sv1.compare(s1) == 0, "" );    
	static_assert( sv3.compare(s1)  > 0, "" );    
}
#endif
