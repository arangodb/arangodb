/*=============================================================================
    Copyright (c) 2013 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "glob.hpp"
#include <cassert>

namespace quickbook
{
    typedef string_iterator glob_iterator;

    void check_glob_range(glob_iterator&, glob_iterator);
    void check_glob_escape(glob_iterator&, glob_iterator);

    bool match_section(
        glob_iterator& pattern_begin,
        glob_iterator pattern_end,
        glob_iterator& filename_begin,
        glob_iterator& filename_end);
    bool match_range(
        glob_iterator& pattern_begin, glob_iterator pattern_end, char x);

    // Is pattern a glob or a plain file name?
    // Throws glob_error if pattern is an invalid glob.
    bool check_glob(quickbook::string_view pattern)
    {
        bool is_glob = false;
        bool is_ascii = true;

        glob_iterator begin = pattern.begin();
        glob_iterator end = pattern.end();

        while (begin != end) {
            if (*begin < 32 || (*begin & 0x80)) is_ascii = false;

            switch (*begin) {
            case '\\':
                check_glob_escape(begin, end);
                break;

            case '[':
                check_glob_range(begin, end);
                is_glob = true;
                break;

            case ']':
                throw glob_error("uneven square brackets");

            case '?':
                is_glob = true;
                ++begin;
                break;

            case '*':
                is_glob = true;
                ++begin;

                if (begin != end && *begin == '*') {
                    throw glob_error("'**' not supported");
                }
                break;

            default:
                ++begin;
            }
        }

        if (is_glob && !is_ascii)
            throw glob_error("invalid character, globs are ascii only");

        return is_glob;
    }

    void check_glob_range(glob_iterator& begin, glob_iterator end)
    {
        assert(begin != end && *begin == '[');
        ++begin;

        if (*begin == ']') throw glob_error("empty range");

        while (begin != end) {
            switch (*begin) {
            case '\\':
                ++begin;

                if (begin == end) {
                    throw glob_error("trailing escape");
                }
                else if (*begin == '\\' || *begin == '/') {
                    throw glob_error("contains escaped slash");
                }

                ++begin;
                break;
            case '[':
                throw glob_error("nested square brackets");
            case ']':
                ++begin;
                return;
            case '/':
                throw glob_error("slash in square brackets");
            default:
                ++begin;
            }
        }

        throw glob_error("uneven square brackets");
    }

    void check_glob_escape(glob_iterator& begin, glob_iterator end)
    {
        assert(begin != end && *begin == '\\');

        ++begin;

        if (begin == end) {
            throw glob_error("trailing escape");
        }
        else if (*begin == '\\' || *begin == '/') {
            throw glob_error("contains escaped slash");
        }

        ++begin;
    }

    // Does filename match pattern?
    // Might throw glob_error if pattern is an invalid glob,
    // but should call check_glob first to validate the glob.
    bool glob(
        quickbook::string_view const& pattern,
        quickbook::string_view const& filename)
    {
        // If there wasn't this special case then '*' would match an
        // empty string.
        if (filename.empty()) return pattern.empty();

        glob_iterator pattern_it = pattern.begin();
        glob_iterator pattern_end = pattern.end();

        glob_iterator filename_it = filename.begin();
        glob_iterator filename_end = filename.end();

        if (!match_section(pattern_it, pattern_end, filename_it, filename_end))
            return false;

        while (pattern_it != pattern_end) {
            assert(*pattern_it == '*');
            ++pattern_it;

            if (pattern_it == pattern_end) return true;

            if (*pattern_it == '*') {
                throw glob_error("'**' not supported");
            }

            for (;;) {
                if (filename_it == filename_end) return false;
                if (match_section(
                        pattern_it, pattern_end, filename_it, filename_end))
                    break;
                ++filename_it;
            }
        }

        return filename_it == filename_end;
    }

    bool match_section(
        glob_iterator& pattern_begin,
        glob_iterator pattern_end,
        glob_iterator& filename_begin,
        glob_iterator& filename_end)
    {
        glob_iterator pattern_it = pattern_begin;
        glob_iterator filename_it = filename_begin;

        while (pattern_it != pattern_end && *pattern_it != '*') {
            if (filename_it == filename_end) return false;

            switch (*pattern_it) {
            case '*':
                assert(false);
                throw new glob_error("Internal error");
            case '[':
                if (!match_range(pattern_it, pattern_end, *filename_it))
                    return false;
                ++filename_it;
                break;
            case ']':
                throw glob_error("uneven square brackets");
            case '?':
                ++pattern_it;
                ++filename_it;
                break;
            case '\\':
                ++pattern_it;
                if (pattern_it == pattern_end) {
                    throw glob_error("trailing escape");
                }
                else if (*pattern_it == '\\' || *pattern_it == '/') {
                    throw glob_error("contains escaped slash");
                }
                BOOST_FALLTHROUGH;
            default:
                if (*pattern_it != *filename_it) return false;
                ++pattern_it;
                ++filename_it;
            }
        }

        if (pattern_it == pattern_end && filename_it != filename_end)
            return false;

        pattern_begin = pattern_it;
        filename_begin = filename_it;
        return true;
    }

    bool match_range(
        glob_iterator& pattern_begin, glob_iterator pattern_end, char x)
    {
        assert(pattern_begin != pattern_end && *pattern_begin == '[');
        ++pattern_begin;
        if (pattern_begin == pattern_end) {
            throw glob_error("uneven square brackets");
        }

        bool invert_match = false;
        bool matched = false;

        if (*pattern_begin == '^') {
            invert_match = true;
            ++pattern_begin;
            if (pattern_begin == pattern_end) {
                throw glob_error("uneven square brackets");
            }
        }
        else if (*pattern_begin == ']') {
            throw glob_error("empty range");
        }

        // Search for a match
        for (;;) {
            unsigned char first = *pattern_begin;
            ++pattern_begin;
            if (first == ']') break;
            if (first == '[') {
                throw glob_error("nested square brackets");
            }
            if (pattern_begin == pattern_end) {
                throw glob_error("uneven square brackets");
            }

            if (first == '\\') {
                first = *pattern_begin;
                if (first == '\\' || first == '/') {
                    throw glob_error("contains escaped slash");
                }
                ++pattern_begin;
                if (pattern_begin == pattern_end) {
                    throw glob_error("uneven square brackets");
                }
            }
            else if (first == '/') {
                throw glob_error("slash in square brackets");
            }

            if (*pattern_begin != '-') {
                matched = matched || (first == x);
            }
            else {
                ++pattern_begin;
                if (pattern_begin == pattern_end) {
                    throw glob_error("uneven square brackets");
                }

                unsigned char second = *pattern_begin;
                ++pattern_begin;
                if (second == ']') {
                    matched = matched || (first == x) || (x == '-');
                    break;
                }
                if (pattern_begin == pattern_end) {
                    throw glob_error("uneven square brackets");
                }

                if (second == '\\') {
                    second = *pattern_begin;
                    if (second == '\\' || second == '/') {
                        throw glob_error("contains escaped slash");
                    }
                    ++pattern_begin;
                    if (pattern_begin == pattern_end) {
                        throw glob_error("uneven square brackets");
                    }
                }
                else if (second == '/') {
                    throw glob_error("slash in square brackets");
                }

                matched = matched || (first <= x && x <= second);
            }
        }

        return invert_match != matched;
    }

    std::size_t find_glob_char(quickbook::string_view pattern, std::size_t pos)
    {
        // Weird style is because quickbook::string_view's find_first_of
        // doesn't take a position argument.
        std::size_t removed = 0;

        for (;;) {
            pos = pattern.find_first_of("[]?*\\");
            if (pos == quickbook::string_view::npos) return pos;
            if (pattern[pos] != '\\') return pos + removed;
            pattern.remove_prefix(pos + 2);
            removed += pos + 2;
        }
    }

    std::string glob_unescape(quickbook::string_view pattern)
    {
        std::string result;

        for (;;) {
            std::size_t pos = pattern.find("\\");
            if (pos == quickbook::string_view::npos) {
                result.append(pattern.data(), pattern.size());
                break;
            }

            result.append(pattern.data(), pos);
            ++pos;
            if (pos < pattern.size()) {
                result += pattern[pos];
                ++pos;
            }
            pattern.remove_prefix(pos);
        }

        return result;
    }
}
