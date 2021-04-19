#pragma once

#include "unicorn/character.hpp"
#include "unicorn/utf.hpp"
#include "unicorn/utility.hpp"
#include <algorithm>
#include <deque>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace RS::Unicorn {

    // Constants

    struct Segment {

        static constexpr uint32_t unicode    = setbit<0>;  // [word] Report all UAX29 words (default); [para] Divide into paragraphs using only PS
        static constexpr uint32_t graphic    = setbit<1>;  // [word] Report only words with graphic characters
        static constexpr uint32_t alpha      = setbit<2>;  // [word] Report only words with alphanumeric characters
        static constexpr uint32_t keep       = setbit<3>;  // [line/para] Include line/para terminators in results (default)
        static constexpr uint32_t strip      = setbit<4>;  // [line/para] Do not include line/para terminators
        static constexpr uint32_t multiline  = setbit<5>;  // [para] Divide into paragraphs using multiple breaks (default)
        static constexpr uint32_t line       = setbit<6>;  // [para] Divide into paragraphs using any line break

    };

    // Common base template for grapheme, word, and sentence iterators

    namespace UnicornDetail {

        template <typename Property> using PropertyQuery = Property (*)(char32_t);
        template <typename Property> using SegmentFunction = size_t (*)(const std::deque<Property>&, bool);

        size_t find_grapheme_break(const std::deque<Grapheme_Cluster_Break>& buf, bool eof);
        size_t find_word_break(const std::deque<Word_Break>& buf, bool eof);
        size_t find_sentence_break(const std::deque<Sentence_Break>& buf, bool eof);

    }

    template <typename C, typename Property, UnicornDetail::PropertyQuery<Property> PQ, UnicornDetail::SegmentFunction<Property> SF>
    class BasicSegmentIterator:
    public ForwardIterator<BasicSegmentIterator<C, Property, PQ, SF>, const Irange<UtfIterator<C>>> {
    public:
        using utf_iterator = UtfIterator<C>;
        BasicSegmentIterator() noexcept {}
        BasicSegmentIterator(const utf_iterator& i, const utf_iterator& j, uint32_t flags):
            seg{i, i}, ends(j), next(i), bufsize(initsize), mode(flags) { ++*this; }
        const Irange<utf_iterator>& operator*() const noexcept { return seg; }
        BasicSegmentIterator& operator++() noexcept;
        bool operator==(const BasicSegmentIterator& rhs) const noexcept { return seg.begin() == rhs.seg.begin(); }
    private:
        static constexpr size_t initsize = 16;
        Irange<utf_iterator> seg;  // Iterator pair marking current segment
        size_t len = 0;            // Length of segment
        utf_iterator ends;         // End of source string
        utf_iterator next;         // End of buffer contents
        std::deque<Property> buf;  // Property lookahead buffer
        size_t bufsize = 0;        // Current lookahead limit
        uint32_t mode = 0;         // Mode flags
        bool select_segment() const noexcept;
    };

    template <typename C, typename Property, UnicornDetail::PropertyQuery<Property> PQ, UnicornDetail::SegmentFunction<Property> SF>
    BasicSegmentIterator<C, Property, PQ, SF>& BasicSegmentIterator<C, Property, PQ, SF>::operator++() noexcept {
        do {
            seg.first = seg.second;
            if (seg.first == ends)
                break;
            buf.erase(buf.begin(), buf.begin() + len);
            for (;;) {
                while (next != ends && buf.size() < bufsize)
                    buf.push_back(PQ(*next++));
                len = SF(buf, next == ends);
                if (len || next == ends)
                    break;
                bufsize += initsize;
            }
            if (len == 0)
                seg.second = ends;
            else
                std::advance(seg.second, len);
        } while (! select_segment());
        return *this;
    }

    template <typename C, typename Property, UnicornDetail::PropertyQuery<Property> PQ, UnicornDetail::SegmentFunction<Property> SF>
    bool BasicSegmentIterator<C, Property, PQ, SF>::select_segment() const noexcept {
        if (mode & Segment::graphic)
            return std::find_if_not(seg.begin(), seg.end(), char_is_white_space) != seg.end();
        else if (mode & Segment::alpha)
            return std::find_if(seg.begin(), seg.end(), char_is_alphanumeric) != seg.end();
        else
            return true;
    }

    // Grapheme cluster boundaries

    template <typename C> using GraphemeIterator = BasicSegmentIterator<C, Grapheme_Cluster_Break, grapheme_cluster_break, UnicornDetail::find_grapheme_break>;

    template <typename C> Irange<GraphemeIterator<C>>
    grapheme_range(const UtfIterator<C>& i, const UtfIterator<C>& j) {
        return {{i, j, {}}, {j, j, {}}};
    }

    template <typename C> Irange<GraphemeIterator<C>>
    grapheme_range(const Irange<UtfIterator<C>>& source) {
        return grapheme_range(source.begin(), source.end());
    }

    template <typename C> Irange<GraphemeIterator<C>>
    grapheme_range(const std::basic_string<C>& source) {
        return grapheme_range(utf_range(source));
    }

    // Word boundaries

    template <typename C> using WordIterator = BasicSegmentIterator<C, Word_Break, word_break, UnicornDetail::find_word_break>;

    template <typename C> Irange<WordIterator<C>>
    word_range(const UtfIterator<C>& i, const UtfIterator<C>& j, uint32_t flags = 0) {
        if (popcount(flags & (Segment::unicode | Segment::graphic | Segment::alpha)) > 1)
            throw std::invalid_argument("Inconsistent word breaking flags");
        return {{i, j, flags}, {j, j, flags}};
    }

    template <typename C> Irange<WordIterator<C>>
    word_range(const Irange<UtfIterator<C>>& source, uint32_t flags = 0) {
        return word_range(source.begin(), source.end(), flags);
    }

    template <typename C> Irange<WordIterator<C>>
    word_range(const std::basic_string<C>& source, uint32_t flags = 0) {
        return word_range(utf_range(source), flags);
    }

    template <typename C> Irange<WordIterator<C>>
    word_range(const std::basic_string_view<C> source, uint32_t flags = 0) {
        return word_range(utf_range(source), flags);
    }

    // Sentence boundaries

    template <typename C> using SentenceIterator = BasicSegmentIterator<C, Sentence_Break, sentence_break, UnicornDetail::find_sentence_break>;

    template <typename C> Irange<SentenceIterator<C>>
    sentence_range(const UtfIterator<C>& i, const UtfIterator<C>& j) {
        return {{i, j, {}}, {j, j, {}}};
    }

    template <typename C> Irange<SentenceIterator<C>>
    sentence_range(const Irange<UtfIterator<C>>& source) {
        return sentence_range(source.begin(), source.end());
    }

    template <typename C> Irange<SentenceIterator<C>>
    sentence_range(const std::basic_string<C>& source) {
        return sentence_range(utf_range(source));
    }

    // Common base template for line and paragraph iterators

    namespace UnicornDetail {

        // This takes a pair of UTF iterators marking the current position and
        // the end of the subject string, and returns a pair delimiting the
        // end-of-block marker.

        template <typename C> using FindBlockFunction = Irange<UtfIterator<C>> (*)(const UtfIterator<C>&, const UtfIterator<C>&);

        inline bool is_restricted_line_break(char32_t c) { return c == U'\n' || c == U'\v' || c == U'\r' || c == 0x85; }
        inline bool is_basic_para_break(char32_t c) { return is_restricted_line_break(c) || c == paragraph_separator_char; }

        template <typename C>
        Irange<UtfIterator<C>> find_end_of_line(const UtfIterator<C>& current, const UtfIterator<C>& endstr) {
            auto i = std::find_if(current, endstr, char_is_line_break);
            auto j = i;
            if (j != endstr)
                ++j;
            if (j != endstr && *i == U'\r' && *j == U'\n')
                ++j;
            return {i, j};
        }

        template <typename C>
        Irange<UtfIterator<C>> find_basic_para(const UtfIterator<C>& current, const UtfIterator<C>& endstr) {
            auto i = std::find_if(current, endstr, is_basic_para_break);
            auto j = i;
            if (j != endstr)
                ++j;
            if (j != endstr && *i == U'\r' && *j == U'\n')
                ++j;
            return {i, j};
        }

        template <typename C>
        Irange<UtfIterator<C>> find_multiline_para(const UtfIterator<C>& current, const UtfIterator<C>& endstr) {
            auto from = current;
            UtfIterator<C> i, j;
            for (;;) {
                i = j = std::find_if(from, endstr, char_is_line_break);
                if (i == endstr)
                    break;
                if (*i == paragraph_separator_char) {
                    ++j;
                    break;
                }
                if (*i == line_separator_char || *i == U'\f') {
                    from = std::next(i);
                    continue;
                }
                int linebreaks = 0;
                do {
                    auto pre = j;
                    ++j;
                    if (j != endstr && *pre == U'\r' && *j == U'\n')
                        ++j;
                    ++linebreaks;
                } while (j != endstr && is_restricted_line_break(*j));
                if (linebreaks >= 2)
                    break;
                from = j;
            }
            return {i, j};
        }

        template <typename C>
        Irange<UtfIterator<C>> find_unicode_para(const UtfIterator<C>& current, const UtfIterator<C>& endstr) {
            auto i = std::find(current, endstr, paragraph_separator_char);
            auto j = i;
            if (j != endstr)
                ++j;
            return {i, j};
        }

    }

    template <typename C>
    class BlockSegmentIterator:
    public ForwardIterator<BlockSegmentIterator<C>, const Irange<UtfIterator<C>>> {
    private:
        using find_block = UnicornDetail::FindBlockFunction<C>;
    public:
        using utf_iterator = UtfIterator<C>;
        BlockSegmentIterator() = default;
        BlockSegmentIterator(const utf_iterator& i, const utf_iterator& j, uint32_t flags, find_block f) noexcept:
            next(i), ends(j), mode(flags), find(f) { ++*this; }
        const Irange<utf_iterator>& operator*() const noexcept { return seg; }
        BlockSegmentIterator& operator++() noexcept;
        bool operator==(const BlockSegmentIterator& rhs) const noexcept { return seg.begin() == rhs.seg.begin(); }
    private:
        Irange<utf_iterator> seg;   // Iterator pair marking current block
        utf_iterator next;          // Start of next block
        utf_iterator ends;          // End of source string
        uint32_t mode = 0;          // Mode flags
        find_block find = nullptr;  // Find end of block
    };

    template <typename C>
    BlockSegmentIterator<C>& BlockSegmentIterator<C>::operator++() noexcept {
        seg.first = seg.second = next;
        if (next == ends)
            return *this;
        auto endblock = find(next, ends);
        seg.first = next;
        seg.second = mode & Segment::strip ? endblock.first : endblock.second;
        next = endblock.second;
        return *this;
    }

    // Line boundaries

    template <typename C> using LineIterator = BlockSegmentIterator<C>;

    template <typename C>
    Irange<BlockSegmentIterator<C>> line_range(const UtfIterator<C>& i, const UtfIterator<C>& j, uint32_t flags = 0) {
        using namespace UnicornDetail;
        if (popcount(flags & (Segment::keep | Segment::strip)) > 1)
            throw std::invalid_argument("Inconsistent line breaking flags");
        return {{i, j, flags, find_end_of_line}, {j, j, flags, find_end_of_line}};
    }

    template <typename C>
    Irange<BlockSegmentIterator<C>> line_range(const Irange<UtfIterator<C>>& source, uint32_t flags = 0) {
        return line_range(source.begin(), source.end(), flags);
    }

    template <typename C>
    Irange<BlockSegmentIterator<C>> line_range(const std::basic_string<C>& source, uint32_t flags = 0) {
        return line_range(utf_range(source), flags);
    }

    // Paragraph boundaries

    template <typename C> using ParagraphIterator = BlockSegmentIterator<C>;

    template <typename C>
    Irange<BlockSegmentIterator<C>> paragraph_range(const UtfIterator<C>& i, const UtfIterator<C>& j, uint32_t flags = 0) {
        using namespace UnicornDetail;
        if (popcount(flags & (Segment::keep | Segment::strip)) > 1
                || popcount(flags & (Segment::multiline | Segment::line | Segment::unicode)) > 1)
            throw std::invalid_argument("Inconsistent paragraph breaking flags");
        FindBlockFunction<C> f;
        if (flags & Segment::unicode)
            f = find_unicode_para;
        else if (flags & Segment::line)
            f = find_basic_para;
        else
            f = find_multiline_para;
        return {{i, j, flags, f}, {j, j, flags, f}};
    }

    template <typename C>
    Irange<BlockSegmentIterator<C>> paragraph_range(const Irange<UtfIterator<C>>& source, uint32_t flags = 0) {
        return paragraph_range(source.begin(), source.end(), flags);
    }

    template <typename C>
    Irange<BlockSegmentIterator<C>> paragraph_range(const std::basic_string<C>& source, uint32_t flags = 0) {
        return paragraph_range(utf_range(source), flags);
    }

}
