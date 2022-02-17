// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_TRIE_FWD_HPP
#define BOOST_TEXT_TRIE_FWD_HPP


namespace boost { namespace text {

    /** A statically polymorphic less-than compariason object type.  This is
        only necessary for pre-C++14 portablility. */
    struct less
    {
        template<typename T>
        bool operator()(T const & lhs, T const & rhs) const
            noexcept(noexcept(std::less<T>{}(lhs, rhs)))
        {
            return std::less<T>{}(lhs, rhs);
        }
    };

    template<
        typename Key,
        typename Value,
        typename Compare = less,
        std::size_t KeySize = 0>
    struct trie;

    template<typename Key, typename Value, typename Compare = less>
    struct trie_map;

    template<typename Key, typename Compare = less>
    struct trie_set;

}}

#endif
