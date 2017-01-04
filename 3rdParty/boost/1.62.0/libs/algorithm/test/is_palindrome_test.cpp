/*
  Copyright (c) Alexander Zaitsev <zamazan4ik@gmail.com>, 2016

  Distributed under the Boost Software License, Version 1.0. (See
  accompanying file LICENSE_1_0.txt or copy at
  http://www.boost.org/LICENSE_1_0.txt)

  See http://www.boost.org/ for latest version.
*/

#include <boost/config.hpp>
#include <boost/algorithm/is_palindrome.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <iostream>
#include <list>
#include <vector>


namespace ba = boost::algorithm;


template <typename T>
bool funcComparator(const T& v1, const T& v2)
{
    return v1 == v2;
}

struct functorComparator
{
    template <typename T>
    bool operator()(const T& v1, const T& v2) const
    {
        return v1 == v2;
    }
};

#define	Begin(arr)	(arr)
#define	End(arr)	(arr+(sizeof(arr)/(sizeof(arr[0]))))

void test_is_palindrome()
{
    const std::list<int> empty;
    const std::vector<char> singleElement(1, 'z');
    int oddNonPalindrome[] = {3,2,2};
    const int oddPalindrome[] = {1,2,3,2,1};
    const int evenPalindrome[] = {1,2,2,1};
    int evenNonPalindrome[] = {1,4,8,8};
    const char* stringNullPtr = NULL;

    // Test a default operator==
    BOOST_CHECK ( ba::is_palindrome(empty));
    BOOST_CHECK ( ba::is_palindrome(singleElement));
    BOOST_CHECK (!ba::is_palindrome(Begin(oddNonPalindrome),  End(oddNonPalindrome)));
    BOOST_CHECK ( ba::is_palindrome(Begin(oddPalindrome),     End(oddPalindrome)));
    BOOST_CHECK ( ba::is_palindrome(Begin(evenPalindrome),    End(evenPalindrome)));
    BOOST_CHECK (!ba::is_palindrome(Begin(evenNonPalindrome), End(evenNonPalindrome)));

    //Test the custom comparators
    BOOST_CHECK ( ba::is_palindrome(empty.begin(), empty.end(), functorComparator()));
    BOOST_CHECK (!ba::is_palindrome(Begin(oddNonPalindrome), End(oddNonPalindrome), funcComparator<int>));
    BOOST_CHECK ( ba::is_palindrome(evenPalindrome, std::equal_to<int>()));
    
    //Test C-strings like cases
    BOOST_CHECK ( ba::is_palindrome(stringNullPtr));
    BOOST_CHECK ( ba::is_palindrome(""));
    BOOST_CHECK ( ba::is_palindrome("a"));
    BOOST_CHECK ( ba::is_palindrome("abacaba", std::equal_to<char>()));
    BOOST_CHECK ( ba::is_palindrome("abba"));
    BOOST_CHECK (!ba::is_palindrome("acab"));
}

BOOST_AUTO_TEST_CASE( test_main )
{
  test_is_palindrome ();
}
