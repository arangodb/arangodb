/*
   Copyright (c) Alexander Zaitsev <zamazan4ik@gmail.by>, 2016

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <vector>
#include <list>
#include <iterator>
#include <functional>
#include <iostream>

#include <boost/algorithm/is_palindrome.hpp>


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


int main ( int /*argc*/, char * /*argv*/ [] )
{
    //You can this algorithm with iterators(minimum Bidirectional)
    std::vector<int> vec{1,2,1};
    if(ba::is_palindrome(vec.begin(), vec.end()))
        std::cout << "This container is palindrome" << std::endl;
    else
        std::cout << "This container is not palindrome" << std::endl;


    //Of course, you can use const iterators
    if(ba::is_palindrome(vec.cbegin(), vec.cend()))
        std::cout << "This container is palindrome" << std::endl;
    else
        std::cout << "This container is not palindrome" << std::endl;


    //Example with bidirectional iterators
    std::list<int> list{1,2,1};
    if(ba::is_palindrome(list.begin(), list.end()))
        std::cout << "This container is palindrome" << std::endl;
    else
        std::cout << "This container is not palindrome" << std::endl;


    //You can use custom comparators like functions, functors, lambdas
    auto lambdaComparator = [](int v1, int v2){ return v1 == v2; };
    auto objFunc = std::function<bool(int, int)>(lambdaComparator);

    if(ba::is_palindrome(vec.begin(), vec.end(), lambdaComparator))
        std::cout << "This container is palindrome" << std::endl;
    else
        std::cout << "This container is not palindrome" << std::endl;

    if(ba::is_palindrome(vec.begin(), vec.end(), funcComparator<int>))
        std::cout << "This container is palindrome" << std::endl;
    else
        std::cout << "This container is not palindrome" << std::endl;

    if(ba::is_palindrome(vec.begin(), vec.end(), functorComparator()))
        std::cout << "This container is palindrome" << std::endl;
    else
        std::cout << "This container is not palindrome" << std::endl;

    if(ba::is_palindrome(vec.begin(), vec.end(), objFunc))
        std::cout << "This container is palindrome" << std::endl;
    else
        std::cout << "This container is not palindrome" << std::endl;


    //You can use ranges
    if(ba::is_palindrome(vec))
        std::cout << "This container is palindrome" << std::endl;
    else
        std::cout << "This container is not palindrome" << std::endl;
    
    //You can use C-strings
    if(ba::is_palindrome("aba"))
	std::cout << "This C-string is palindrome" << std::endl;
    else
        std::cout << "This C-string is not palindrome" << std::endl;
    return 0;
}
