// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ editor_buffer
#ifndef EDITOR_BUFFER_HPP
#define EDITOR_BUFFER_HPP

#include "event.hpp"

#include <boost/text/line_break.hpp>
#include <boost/text/rope.hpp>
#include <boost/text/text.hpp>
#include <boost/text/segmented_vector.hpp>
#include <boost/text/word_break.hpp>

#include <boost/filesystem/fstream.hpp>

#include <vector>


// This is our underlying storage type.  This editor takes advantage of the
// efficient copies this type provides.  If you change this to
// boost::text::text, the editor becomes unusably slow.
using content_t = boost::text::rope;

// This is the data needed for laying out and editing within a single line as
// it appears in one of the editor's buffers.  Each column in the editor
// represents a single grapheme.
struct line_t
{
    int code_units_ = 0;
    int graphemes_ = 0;
    bool hard_break_ = false;
};

// Represents an applcation state that we may want to return to later, say
// becauee of an undo operation.
struct snapshot_t
{
    content_t content_;
    boost::text::segmented_vector<line_t> lines_;
    int first_row_ = 0;
    int desired_col_ = 0;
    screen_pos_t cursor_pos_;
    std::ptrdiff_t first_char_index_ = 0;
};

// A single editable buffer in the editor (the editor currently only supports
// one).  It contains a current state, snapshot_, plus a copy of the content
// so we know what the latest saved state, and a history for performing undo
// operations.
//
// Note that the only element here that is not cheap to copy is history_.  Due
// to history_'s unbounded size and potentially costly copy, you should move
// buffer_t's when possible.
struct buffer_t
{
    snapshot_t snapshot_;
    content_t latest_save_;
    boost::filesystem::path path_;
    std::vector<snapshot_t> history_;
};

// This is our dirty-buffer predicate.  We use this to show a '**' in the
// status line when there are unsaved changes.  Try implementing this without
// the equal_root() trick here -- it's very difficult to get right.
inline bool dirty(buffer_t const & buffer)
{
    return !buffer.snapshot_.content_.equal_root(buffer.latest_save_);
}

inline int cursor_line(snapshot_t const & snapshot)
{
    return snapshot.first_row_ + snapshot.cursor_pos_.row_;
}

inline bool cursor_at_last_line(snapshot_t const & snapshot)
{
    return cursor_line(snapshot) == (std::ptrdiff_t)snapshot.lines_.size();
}

struct cursor_iterators_t
{
    content_t::iterator first_;
    content_t::iterator cursor_;
    content_t::iterator last_;
};

inline content_t::iterator
iterator_at_start_of_line(snapshot_t const & snapshot, std::ptrdiff_t line_index)
{
    assert(snapshot.first_row_ <= line_index);

    if (line_index == (std::ptrdiff_t)snapshot.lines_.size())
        return snapshot.content_.end();

    std::ptrdiff_t offset = snapshot.first_char_index_;
    for (std::ptrdiff_t i = snapshot.first_row_; i < line_index; ++i) {
        offset += snapshot.lines_[i].code_units_;
    }

    // Here we peel back the curtain a bit and show how a graphem_iterator is
    // constructed from code point iterators constructed from char iterators.
    auto const first = snapshot.content_.begin().base().base();
    auto const it = first + offset;
    auto const last = snapshot.content_.end().base().base();
    return {content_t::iterator::iterator_type{first, first, last},
            content_t::iterator::iterator_type{first, it, last},
            content_t::iterator::iterator_type{first, last, last}};
}

inline cursor_iterators_t cursor_iterators(snapshot_t const & snapshot)
{
    auto const line_index = cursor_line(snapshot);
    if (line_index == (std::ptrdiff_t)snapshot.lines_.size()) {
        return {snapshot.content_.end(),
                snapshot.content_.end(),
                snapshot.content_.end()};
    }

    auto const line_grapheme_first =
        iterator_at_start_of_line(snapshot, line_index);
    return {line_grapheme_first,
            std::next(line_grapheme_first, snapshot.cursor_pos_.col_),
            std::next(
                line_grapheme_first,
                snapshot.lines_[snapshot.cursor_pos_.row_].graphemes_)};
}

struct cursor_word_t
{
    boost::text::grapheme_view<content_t::iterator::iterator_type> word_;
    content_t::iterator cursor_;
};

inline cursor_word_t cursor_word(snapshot_t const & snapshot)
{
    if (cursor_at_last_line(snapshot)) {
        return {
            {snapshot.content_.end().base(), snapshot.content_.end().base()},
            snapshot.content_.end()};
    }
    auto const iterators = cursor_iterators(snapshot);
    return {boost::text::word(snapshot.content_, iterators.cursor_),
            iterators.cursor_};
}

// This will fill container with pairs of code unit and grapheme extents for
// each line.  A line may end in a hard line break like '\n', or it mey be
// broken because there's just no room left on that line within the screen
// width.  This works for getting the line breaks for all lines in the buffer
// contents, and we can reuse it to re-break lines as we edit the buffer too.
template<typename GraphemeRange, typename LinesContainer>
inline void get_lines(
    GraphemeRange const & range, int screen_width, LinesContainer & container)
{
    for (auto line : boost::text::lines(
             range,
             screen_width - 1,
             [](content_t::const_iterator::iterator_type first,
                content_t::const_iterator::iterator_type last) noexcept {
                 boost::text::grapheme_view<
                     content_t::const_iterator::iterator_type>
                     range(first, last);
                 return std::distance(range.begin(), range.end());
             })) {
        // Note that we don't count the terminating hard line break grapheme
        // here.  This requires special casing in some places, but makes much
        // of the logic simpler.
        line_t const line_{
            int(line.end().base().base() - line.begin().base().base()),
            (int)std::distance(line.begin(), line.end()) -
                (line.hard_break() ? 1 : 0),
            line.hard_break()};
        container.push_back(line_);
    }
}

inline buffer_t load_buffer(boost::filesystem::path path, int screen_width)
{
    boost::filesystem::ifstream ifs(path);

    buffer_t retval;
    retval.path_ = std::move(path);

    int const chunk_size = 1 << 16;
    while (ifs.good()) {
        std::string chunk;
        chunk.resize(chunk_size, ' ');

        ifs.read(const_cast<char *>(chunk.data()), chunk_size);
        if (!ifs.good())
            chunk.resize(ifs.gcount(), ' ');

        retval.snapshot_.content_ += std::move(chunk);
    }

    get_lines(retval.snapshot_.content_, screen_width, retval.snapshot_.lines_);

    retval.latest_save_ = retval.snapshot_.content_;
    retval.history_.push_back(retval.snapshot_);

    return retval;
}

inline void save_buffer(boost::filesystem::path path, buffer_t const & buffer)
{
    boost::filesystem::ofstream ofs(path);
    ofs << buffer.snapshot_.content_;
}

#endif
//]
