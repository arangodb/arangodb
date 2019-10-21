/*=============================================================================
Copyright (c) 2011-2013, 2017 Daniel James

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

namespace quickbook
{
    template <typename Iterator>
    bool read(Iterator& it, Iterator end, char const* text)
    {
        for (Iterator it2 = it;; ++it2, ++text) {
            if (!*text) {
                it = it2;
                return true;
            }

            if (it2 == end || *it2 != *text) return false;
        }
    }

    template <typename Iterator>
    bool read_past(Iterator& it, Iterator end, char const* text)
    {
        while (it != end) {
            if (read(it, end, text)) {
                return true;
            }
            ++it;
        }
        return false;
    }

    inline bool find_char(char const* text, char c)
    {
        for (; *text; ++text)
            if (c == *text) return true;
        return false;
    }

    template <typename Iterator>
    void read_some_of(Iterator& it, Iterator end, char const* text)
    {
        while (it != end && find_char(text, *it))
            ++it;
    }

    template <typename Iterator>
    void read_to_one_of(Iterator& it, Iterator end, char const* text)
    {
        while (it != end && !find_char(text, *it))
            ++it;
    }

    template <typename Iterator>
    void read_to(Iterator& it, Iterator end, char c)
    {
        while (it != end && *it != c)
            ++it;
    }
}
