#include "unicorn/string.hpp"
#include <algorithm>

namespace RS::Unicorn {

    namespace {

        template <typename C, typename F>
        const Ustring casemap_helper(C&& src, F f) {
            Ustring dst;
            char32_t buf[max_case_decomposition];
            auto out = utf_writer(dst);
            for (auto c: utf_range(std::forward<C>(src))) {
                auto n = f(c, buf);
                std::copy_n(buf, n, out);
            }
            return dst;
        }

        template <typename FwdIter>
        bool next_cased(FwdIter i, FwdIter e) {
            if (i == e)
                return false;
            for (++i; i != e; ++i)
                if (! char_is_case_ignorable(*i))
                    return char_is_cased(*i);
            return false;
        }

        struct LowerChar {
            static constexpr char32_t sigma = 0x3c3;
            static constexpr char32_t final_sigma = 0x3c2;
            bool last_cased = false;
            char32_t buf[max_case_decomposition];
            template <typename FwdIter, typename OutIter> void convert(FwdIter i, FwdIter e, OutIter to) {
                auto n = char_to_full_lowercase(*i, buf);
                if (buf[0] == sigma && last_cased && ! next_cased(i, e))
                    buf[0] = final_sigma;
                std::copy_n(buf, n, to);
                if (! char_is_case_ignorable(*i))
                    last_cased = char_is_cased(*i);
            }
        };

    }

    Ustring str_uppercase(const Ustring& str) {
        return casemap_helper(str, char_to_full_uppercase);
    }

    Ustring str_uppercase(Uview str) {
        return casemap_helper(str, char_to_full_uppercase);
    }
        
    Ustring str_lowercase(const Utf8Range& range) {
        Ustring dst;
        LowerChar lc;
        auto out = utf_writer(dst);
        for (auto i = range.begin(), e = range.end(); i != e; ++i)
            lc.convert(i, e, out);
        return dst;
    }

    Ustring str_titlecase(const Ustring& str) {
        Ustring dst;
        LowerChar lc;
        auto e = utf_end(str);
        auto out = utf_writer(dst);
        for (auto& w: word_range(str)) {
            bool initial = true;
            for (auto i = w.begin(); i != w.end(); ++i) {
                if (initial && char_is_cased(*i)) {
                    auto n = char_to_full_titlecase(*i, lc.buf);
                    std::copy_n(lc.buf, n, out);
                    lc.last_cased = true;
                    initial = false;
                } else {
                    lc.convert(i, e, out);
                }
            }
        }
        return dst;
    }

    Ustring str_casefold(const Ustring& str) {
        return casemap_helper(str, char_to_full_casefold);
    }

    Ustring str_case(const Ustring& str, Case c) {
        switch (c) {
            case Case::fold:   return str_casefold(str);
            case Case::lower:  return str_lowercase(str);
            case Case::title:  return str_titlecase(str);
            case Case::upper:  return str_uppercase(str);
            default:           return str;
        }
    }

    Ustring str_initial_titlecase(const Ustring& str) {
        if (str.empty())
            return {};
        auto i = utf_begin(str);
        char32_t buf[max_case_decomposition];
        size_t n = char_to_full_titlecase(*i, buf);
        Ustring dst;
        recode(buf, n, dst);
        dst.append(str, i.count(), npos);
        return dst;
    }

    void str_uppercase_in(Ustring& str) {
        auto result = str_uppercase(str);
        str = std::move(result);
    }

    void str_lowercase_in(Ustring& str) {
        auto result = str_lowercase(str);
        str = std::move(result);
    }

    void str_titlecase_in(Ustring& str) {
        auto result = str_titlecase(str);
        str = std::move(result);
    }

    void str_casefold_in(Ustring& str) {
        auto result = str_casefold(str);
        str = std::move(result);
    }

    void str_case_in(Ustring& str, Case c) {
        auto result = str_case(str, c);
        str = std::move(result);
    }

    void str_initial_titlecase_in(Ustring& str) {
        auto result = str_initial_titlecase(str);
        str = std::move(result);
    }

}
