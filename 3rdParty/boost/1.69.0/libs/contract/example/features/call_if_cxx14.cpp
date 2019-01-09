
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract/call_if.hpp>
#include <type_traits>
#include <iterator>
#include <functional> // std::bind for generic lambdas.
#include <vector>
#include <list>
#include <sstream>

template<typename Iter>
struct is_random_access_iterator : std::is_same<
    typename std::iterator_traits<Iter>::iterator_category,
    std::random_access_iterator_tag
> {};

template<typename Iter>
struct is_bidirectional_iterator : std::is_same<
    typename std::iterator_traits<Iter>::iterator_category,
    std::bidirectional_iterator_tag
> {};

template<typename Iter>
struct is_input_iterator : std::is_same<
    typename std::iterator_traits<Iter>::iterator_category,
    std::input_iterator_tag
> {};

//[call_if_cxx14
template<typename Iter, typename Dist>
void myadvance(Iter& i, Dist n) {
    Iter* p = &i; // So captures change actual pointed iterator value.
    boost::contract::call_if<is_random_access_iterator<Iter> >(
        std::bind([] (auto p, auto n) { // C++14 generic lambda.
            *p += n;
        }, p, n)
    ).template else_if<is_bidirectional_iterator<Iter> >(
        std::bind([] (auto p, auto n) {
            if(n >= 0) while(n--) ++*p;
            else while(n++) --*p;
        }, p, n)
    ).template else_if<is_input_iterator<Iter> >(
        std::bind([] (auto p, auto n) {
            while(n--) ++*p;
        }, p, n)
    ).else_(
        std::bind([] (auto false_) {
            static_assert(false_, "requires at least input iterator");
        }, std::false_type()) // Use constexpr value.
    );
}
//]

struct x {}; // Test not an iterator (static_assert failure in else_ above).

namespace std {
    template<>
    struct iterator_traits<x> {
        typedef void iterator_category;
    };
}

int main() {
    std::vector<char> v;
    v.push_back('a');
    v.push_back('b');
    v.push_back('c');
    v.push_back('d');
    std::vector<char>::iterator r = v.begin(); // Random iterator.
    myadvance(r, 1);
    assert(*r == 'b');

    std::list<char> l(v.begin(), v.end());
    std::list<char>::iterator b = l.begin(); // Bidirectional iterator.
    myadvance(b, 2);
    assert(*b == 'c');

    std::istringstream s("a b c d");
    std::istream_iterator<char> i(s);
    myadvance(i, 3);
    assert(*i == 'd');

    // x j;
    // myadvance(j, 0); // Error (correctly because x not even input iter).

    return 0;
}

