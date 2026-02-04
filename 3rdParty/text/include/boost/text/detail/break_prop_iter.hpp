// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_BREAK_PROP_ITER_HPP
#define BOOST_TEXT_DETAIL_BREAK_PROP_ITER_HPP

#include <boost/text/detail/lzw.hpp>

#include <unordered_map>


namespace boost { namespace text { namespace detail {

    template<typename Enum, typename Map = std::unordered_map<uint32_t, Enum>>
    struct lzw_to_break_prop_iter
    {
        using value_type = std::pair<uint32_t, Enum>;
        using difference_type = int;
        using pointer = unsigned char *;
        using reference = unsigned char &;
        using iterator_category = std::output_iterator_tag;
        using buffer_t = container::small_vector<unsigned char, 256>;

        lzw_to_break_prop_iter(
            Map & map, buffer_t & buf) :
            map_(&map),
            buf_(&buf)
        {}

        lzw_to_break_prop_iter & operator=(unsigned char c)
        {
            buf_->push_back(c);
            auto const element_bytes = 4;
            auto it = buf_->begin();
            for (auto end = buf_->end() - buf_->size() % element_bytes;
                 it != end;
                 it += element_bytes) {
                (*map_)[bytes_to_cp(&*it)] = Enum(*(it + 3));
            }
            buf_->erase(buf_->begin(), it);
            return *this;
        }
        lzw_to_break_prop_iter & operator*() { return *this; }
        lzw_to_break_prop_iter & operator++() { return *this; }
        lzw_to_break_prop_iter & operator++(int) { return *this; }

    private:
        Map * map_;
        buffer_t * buf_;
    };

}}}

#endif
