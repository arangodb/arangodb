# [Unicorn Library](index.html): String Operations #

_Unicode library for C++ by Ross Smith_

* `#include "unicorn/string.hpp"`

This module contains assorted functions related to strings that don't belong
in other modules with a more specific focus.

## Contents ##

[TOC]

## Introduction ##

Not all of these functions are directly related to Unicode; some, such as the
`str_starts_with()` and `str_ends_with()` tests or the `str_replace()`
function, simply operate on the string as a sequence of code units, without
needing to know anything about how it is interpreted. Functions that do need
to be aware of how Unicode encoding works usually operate on, or return, UTF
iterators (see [`unicorn/utf`](utf.html)); many of these can accept either an
actual string or a UTF iterator range as their subject string.

All of the functions here are intended to be used only with known valid
Unicode strings. They all use `ignore_errors` mode internally, and do not
provide any other error handling options.

Many of the string manipulation functions in this module come in two versions,
one that takes the subject string by `const` reference and returns the
modified string, and one that takes it by non-`const` reference and modifies
it in place. Usually the in-place version has a name ending with `_in` to
distinguish them, since in most cases they have identical argument lists apart
from the `const` vs non-`const` argument, and therefore would not be reliably
distinguished by overload resolution if they had the same name.

In some cases the in-place version of the function takes a non-`const`
reference to the subject string accompanied by one or more UTF iterators (see
[`unicorn/utf`](utf.html)) to indicate positions in the string, whereas the
return-value version of the function does not need to be passed the string
explicitly since it can obtain a `const` reference to it from the iterators.
In the in-place function, behaviour is undefined if the iterators do not point
to the same string passed to the reference argument. (In all functions that
take iterators, behaviour is undefined if a pair of iterators that are
expected to mark the beginning and end of a substring do not point to the same
string or are in the wrong order.)

Any in-place function that might modify its subject string invalidates any
iterators (string or UTF) and references that were pointing into that string.
Note that the iterators and references should still be considered to be
invalidated even if the string turns out not to actually be modified by a
specific call.

Modified strings, or new strings returned by a function, are not guaranteed to
preserve any particular normalization form that the original string may have
been in.

Any function that implicitly compares strings uses a simple literal
comparison, making no attempt to handle Unicode's concepts of canonical or
compatibility equivalence; if your code needs to be aware of such things, you
will need to normalise your strings first (see
[`unicorn/normal`](normal.html)).

When a function can take a string argument as either a string object or a
character pointer, a null pointer is always treated as equivalent to an empty
string. When it takes a pointer and size, the size is ignored if the pointer
is null.

## String size functions ##

Most functions that need to calculate the size of a string, in this module and
others, accept one of the following flags to indicate which definition of
"size" the caller wants:

Flag                        | Description
----                        | -----------
`Length::`**`characters`**  | Count the number of Unicode characters in the string
`Length::`**`graphemes`**   | Count the number of grapheme clusters (user-perceived characters) in the string
`Length::`**`narrow`**      | Calculate the East Asian width (ambiguous characters default to narrow)
`Length::`**`wide`**        | Calculate the East Asian width (ambiguous characters default to wide)

Functions that deal with string length usually return a count of grapheme
clusters (user-perceived characters) by default; see also
[`unicorn/segment`](segment.html).

The two options for East Asian width determine how ambiguous width characters
are handled, defaulting to narrow (one unit) or wide (two units). The
`Length::graphemes` flag can be combined with either of the East Asian width
options, giving a size based on the width of the base character of each
grapheme cluster.

The various methods of measurement are implemented in the `str_length()`
function and the `Length` class, described below; anything else that needs a
string size will normally obtain it by calling `str_length()` (or a related
function such as `str_find_index()`).

Any function that accepts length flags will throw `std::invalid_argument` if
an inconsistent combination of flags is passed (e.g.
`Length::characters|Length::graphemes`).

* `template <typename C> size_t` **`str_length`**`(const basic_string<C>& str, uint32_t flags = 0)`
* `template <typename C> size_t` **`str_length`**`(const Irange<UtfIterator<C>>& range, uint32_t flags = 0)`
* `template <typename C> size_t` **`str_length`**`(const UtfIterator<C>& begin, const UtfIterator<C>& end, uint32_t flags = 0)`

Return the length of the string, measured according to the flags supplied.

* `struct` **`Length`**
    * `static constexpr uint32_t Length::`**`characters`**
    * `static constexpr uint32_t Length::`**`graphemes`**
    * `static constexpr uint32_t Length::`**`narrow`**
    * `static constexpr uint32_t Length::`**`wide`**
    * `uint32_t Length::`**`flags`** `= 0`
    * `Length::`**`Length`**`()`
    * `explicit Length::`**`Length`**`(uint32_t length_flags)`
    * `template <typename C> size_t Length::`**`operator()`**`(const std::basic_string<C>& str) const`
    * `template <typename C> size_t Length::`**`operator()`**`(const Irange<UtfIterator<C>>& range) const`
    * `template <typename C> size_t Length::`**`operator()`**`(const UtfIterator<C>& b, const UtfIterator<C>& e) const`

A function object that performs the same length measurements as the
`str_length()` functions.

* `template <typename C> UtfIterator<C>` **`str_find_index`**`(const basic_string<C>& str, size_t pos, uint32_t flags = 0)`
* `template <typename C> UtfIterator<C>` **`str_find_index`**`(const Irange<UtfIterator<C>>& range, size_t pos, uint32_t flags = 0)`
* `template <typename C> UtfIterator<C>` **`str_find_index`**`(const UtfIterator<C>& begin, const UtfIterator<C>& end, size_t pos, uint32_t flags = 0)`
* `template <typename C> size_t` **`str_find_offset`**`(basic_string<C>& str, size_t pos, uint32_t flags = 0)`

These return an iterator, or an offset in code units, pointing to the
character at a given position, measured according to the flags supplied. If
the requested position would be past the end of the string, an end iterator
will be returned (or `npos` for `str_find_offset()`). If the position can't be
adjusted to exactly the specified value (because one of the East Asian width
options was selected and wide characters are present), the first valid
position after the requested point will be returned.

## Other string properties ##

* `template <typename C> char32_t` **`str_char_at`**`(const basic_string<C>& str, size_t index) noexcept`

Returns the character at a specific index, or zero if the index is out of
range.

* `template <typename C> char32_t` **`str_first_char`**`(const basic_string<C>& str) noexcept`
* `template <typename C> char32_t` **`str_last_char`**`(const basic_string<C>& str) noexcept`

Return the first or last character in a string, or zero if the string is
empty.

* `bool` **`str_is_east_asian`**`(const Ustring& str)`

True if the string contains any East Asian characters.

* `bool` **`str_starts_with`**`(const Ustring& str, const Ustring& prefix) noexcept`
* `bool` **`str_ends_with`**`(const Ustring& str, const Ustring& suffix) noexcept`

These return true if the string starts or ends with the specified substring.

## String comparison ##

* `struct` **`Strcmp`**
    * `static constexpr uint32_t Strcmp::`**`equal`**
    * `static constexpr uint32_t Strcmp::`**`less`**
    * `static constexpr uint32_t Strcmp::`**`triple`**
    * `static constexpr uint32_t Strcmp::`**`fallback`**
    * `static constexpr uint32_t Strcmp::`**`icase`**
    * `static constexpr uint32_t Strcmp::`**`natural`**
* `template <uint32_t Flags> struct` **`StringCompare`**
    * `using StringCompare::`**`result_type`** `= [see below]`
    * `result_type StringCompare::`**`operator()`**`(const Ustring& lhs, const Ustring& rhs) const`

This combines several options for comparing strings into a single template.
Exactly one of the first three flags must be supplied, to indicate whether
equality comparison, less-than comparison, or three-way comparison is
required. The result type is `bool` for equality or less-than comparison,
`int` for three-way comparison, which always returns -1, 0, or 1.

If the `icase` flag is used, comparison is case insensitive. This is
equivalent to calling `str_casefold()` on the strings before comparing them;
using `StringCompare<icase>` is usually more efficient for a small number of
comparisons, while calling `str_casefold()` and saving the case folded form of
the string will be more efficient if the same strings are going to be compared
frequently.

If the `natural` flag is used, this attempts to perform a "natural" (human
friendly) comparison between two strings. It treats numbers (currently only
ASCII digits are recognised) as discrete string elements to be sorted
numerically (e.g. `"abc99"` will sort before `"abc100"`; leading zeros are not
significant), and ignores punctuation (significant characters are defined as
general categories `L` [letters], `M` [marks], `N` [numbers], and `S`
[symbols]). Natural comparison is always case insensitive; the presence or
absence of the `icase` flag has no effect.

If the `fallback` flag is combined with `icase` or `natural`, a full case and
punctuation sensitive comparison will be done if no differences are found in
the case insensitive comparison. The `fallback` flag has no effect if used
without `icase` or `natural`.

## Other string algorithms ##

* `size_t` **`str_common`**`(const Ustring& s1, const Ustring& s2, size_t start = 0) noexcept`
* `size_t` **`str_common_utf`**`(const Ustring& s1, const Ustring& s2, size_t start = 0) noexcept`

These return the count of code units in the longest common prefix of two
strings, optionally starting at a given offset (or, equivalently, the offset,
relative to `start`, of the first difference between the strings). The
`str_common()` function simply finds the longest common prefix of code units
without regard to encoding, while `str_common_utf()` finds the longest common
prefix of whole encoded characters (the returned count is still in code
units); this means it will return a smaller value than `str_common()` if the
offset found by `str_common()` is partway through an encoded character. Both
functions will return zero if `start` is past the end of either string.

* `bool` **`str_expect`**`(Utf8Iterator& i, const Ustring& prefix)`
* `bool` **`str_expect`**`(Utf8Iterator& i, const Utf8Iterator& end, const Ustring& prefix)`

If the string starting from `i` starts with `prefix`, `str_expect()` updates
`i` to point to the end of the prefix and returns `true`; otherwise, it leaves
`i` unchanged and returns `false`. Optionally an endpoint other than the end
of the string can be supplied. These will always return `false` if `prefix` is
empty.

* `Utf8Iterator` **`str_find_char`**`(const Ustring& str, char32_t c)`
* `Utf8Iterator` **`str_find_char`**`(const Utf8Iterator& begin, const Utf8Iterator& end, char32_t c)`
* `Utf8Iterator` **`str_find_char`**`(const Irange<Utf8Iterator>& range, char32_t c)`
* `Utf8Iterator` **`str_find_last_char`**`(const Ustring& str, char32_t c)`
* `Utf8Iterator` **`str_find_last_char`**`(const Utf8Iterator& begin, const Utf8Iterator& end, char32_t c)`
* `Utf8Iterator` **`str_find_last_char`**`(const Irange<Utf8Iterator>& range, char32_t c)`

These return an iterator pointing to the first or last occurrence of the
specified character, or an end iterator if it is not found.

* `Utf8Iterator` **`str_find_first_of`**`(const Ustring& str, const Ustring& target)`
* `Utf8Iterator` **`str_find_first_of`**`(const Utf8Iterator& begin, const Utf8Iterator& end, const Ustring& target)`
* `Utf8Iterator` **`str_find_first_of`**`(const Irange<Utf8Iterator>& range, const Ustring& target)`
* `Utf8Iterator` **`str_find_first_not_of`**`(const Ustring& str, const Ustring& target)`
* `Utf8Iterator` **`str_find_first_not_of`**`(const Utf8Iterator& begin, const Utf8Iterator& end, const Ustring& target)`
* `Utf8Iterator` **`str_find_first_not_of`**`(const Irange<Utf8Iterator>& range, const Ustring& target)`
* `Utf8Iterator` **`str_find_last_of`**`(const Ustring& str, const Ustring& target)`
* `Utf8Iterator` **`str_find_last_of`**`(const Utf8Iterator& begin, const Utf8Iterator& end, const Ustring& target)`
* `Utf8Iterator` **`str_find_last_of`**`(const Irange<Utf8Iterator>& range, const Ustring& target)`
* `Utf8Iterator` **`str_find_last_not_of`**`(const Ustring& str, const Ustring& target)`
* `Utf8Iterator` **`str_find_last_not_of`**`(const Utf8Iterator& begin, const Utf8Iterator& end, const Ustring& target)`
* `Utf8Iterator` **`str_find_last_not_of`**`(const Irange<Utf8Iterator>& range, const Ustring& target)`

These find the first or last character in their subject range that is in, or
not in, the target list of characters. They return an end iterator if no
matching character is found. (They are essentially the same as the similarly
named member functions in `std::string`, except that they work on characters
instead of code units.)

* `std::pair<size_t, size_t>` **`str_line_column`**`(const Ustring& str, size_t offset, uint32_t flags = 0)`

Converts a code unit offset (0-based) into a string to a line and column
number (1-based). The `flags` argument takes the same values as for
`str_length()`, and determines how the column number is measured. Any of the
standard Unicode line break symbols, including the `CR+LF` combination, will
be recognised as a line break. If the offset points to a line break character,
it is counted as the last character of the previous line. If the offset does
not match an encoded character boundary, the position of the character that
contains the offset will be reported. If `offset>=str.size()`, this will
report what the line and column number would be for the next character
appended to the string.

* `Irange<Utf8Iterator>` **`str_search`**`(const Ustring& str, const Ustring& target)`
* `Irange<Utf8Iterator>` **`str_search`**`(const Utf8Iterator& begin, const Utf8Iterator& end, const Ustring& target)`
* `Irange<Utf8Iterator>` **`str_search`**`(const Irange<Utf8Iterator>& range, const Ustring& target)`

Find the first occurrence of the target substring in the subject range,
returning an iterator range marking the located substring, or a pair of end
iterators if it was not found.

* `size_t` **`str_skipws`**`(Utf8Iterator& i)`
* `size_t` **`str_skipws`**`(Utf8Iterator& i, const Utf8Iterator& end)`

Advances `i` to point to the next non-whitespace character, or the end of the
string if no such character was found. Optionally an endpoint other than the
end of the string can be supplied. The return value is the number of
characters skipped.

## String manipulation functions ##

* `template <typename C> void` **`str_append`**`(Ustring& str, const basic_string<C>& suffix)`
* `template <typename C> void` **`str_append`**`(Ustring& str, const Irange<UtfIterator<C>>& suffix)`
* `template <typename C> void` **`str_append`**`(Ustring& str, const UtfIterator<C>& suffix_begin, const UtfIterator<C>& suffix_end)`
* `template <typename C> void` **`str_append`**`(Ustring& str, const C* suffix)`
* `template <typename C> void` **`str_append`**`(Ustring& dst, const C* ptr, size_t n)`
* `template <typename C, typename... Chars> void` **`str_append_char`**`(Ustring& dst, C c, Chars... chars)`
* `void` **`str_append_chars`**`(Ustring& dst, size_t n, char32_t c)`

These append one or more characters to a Unicode string, performing any
necessary encoding conversions.

* `Ustring` **`str_char`**`(char32_t c)`
* `Ustring` **`str_chars`**`(size_t n, char32_t c)`

Return a string containing `n` copies of the character.

* `template <typename C, typename... Strings> Ustring` **`str_concat`**`(const basic_string<C>& s, const Strings&... ss)`
* `template <typename C, typename... Strings> Ustring` **`str_concat`**`(const C* s, const Strings&... ss)`
* `template <typename C, typename... Strings> Ustring` **`str_concat_with`**`(const basic_string<C>& delim, const Strings&... ss)`
* `template <typename C, typename... Strings> Ustring` **`str_concat_with`**`(const C* delim, const Strings&... ss)`

These concatenate one or more strings, which can be an arbitrary mixture of
different Unicode encodings. The `str_concat_with()` versions insert a
delimiter between each pair of strings.

* `Ustring` **`str_drop_prefix`**`(const Ustring& str, const Ustring& prefix)`
* `void` **`str_drop_prefix_in`**`(Ustring& str, const Ustring& prefix) noexcept`
* `Ustring` **`str_drop_suffix`**`(const Ustring& str, const Ustring& suffix)`
* `void` **`str_drop_suffix_in`**`(Ustring& str, const Ustring& suffix) noexcept`

If the first argument string starts or ends with the given prefix or suffix,
remove it; otherwise, just return the original string unchanged.

* `Ustring` **`str_erase_left`**`(const Ustring& str, size_t length)`
* `void` **`str_erase_left_in`**`(Ustring& str, size_t length) noexcept`
* `Ustring` **`str_erase_right`**`(const Ustring& str, size_t length)`
* `void` **`str_erase_right_in`**`(Ustring& str, size_t length) noexcept`

Erase the specified number of Unicode characters from the beginning or end of
the string. These will return an empty string if `length` is greater than the
number of characters in `str`.

* `Ustring` **`str_expand_tabs`**`(const Ustring& str)`
* `template <typename IntList> Ustring` **`str_expand_tabs`**`(const Ustring& str, const IntList& tabs, uint32_t flags = 0)`
* `template <typename IntType> Ustring` **`str_expand_tabs`**`(const Ustring& str, initializer_list<IntType> tabs, uint32_t flags = 0)`
* `void` **`str_expand_tabs_in`**`(Ustring& str)`
* `template <typename IntList> void` **`str_expand_tabs_in`**`(Ustring& str, const IntList& tabs, uint32_t flags = 0)`
* `template <typename IntType> void` **`str_expand_tabs_in`**`(Ustring& str, initializer_list<IntType> tabs, uint32_t flags = 0)`

Expand tab characters to spaces. If the input string contains multiple lines
(delimited by any of the standard Unicode line break characters), each line
will be expanded separately. The `tabs` argument is a list of tab positions,
passed either as a range of integers, or as an explicit braced initializer
list of integers. The `flags` argument indicates which units will be used to
measure horizontal position.

By default, a tab stop every 8 columns is assumed. Tab stop positions that are
less than or equal to the previous tab stop are ignored. If more tab stops
beyond the last one listed are needed, the difference between the last two tab
stops is used to increment the last one (e.g. `{5,10,20}` will be expanded to
`{5,10,20,30,40,...}`). An implicit tab stop at position zero is always
assumed.

* `Ustring` **`str_fix_left`**`(const Ustring& str, size_t length, char32_t c = U' ', uint32_t flags = 0)`
* `void` **`str_fix_left_in`**`(Ustring& str, size_t length, char32_t c = U' ', uint32_t flags = 0)`
* `Ustring` **`str_fix_right`**`(const Ustring& str, size_t length, char32_t c = U' ', uint32_t flags = 0)`
* `void` **`str_fix_right_in`**`(Ustring& str, size_t length, char32_t c = U' ', uint32_t flags = 0)`

Pad or truncate a string to a specific length; the character argument `c` is
used for padding (converted to UTF-8). The `str_fix_left()` function anchors
the string on the left and pads or truncates on the right; this is similar to
`std::string::resize()`, except that the flags determine how the length of the
string is measured. The `str_fix_right()` function anchors the string on the
right and pads or truncates on the left. If the string can't be adjusted to
exactly the specified size (because one of the East Asian width options was
selected and wide characters are present), the result will be one unit longer
than the requested length.

* `Ustring` **`str_insert`**`(const Utf8Iterator& dst, const Ustring& src)`
* `Ustring` **`str_insert`**`(const Utf8Iterator& dst, const Irange<Utf8Iterator>& src)`
* `Ustring` **`str_insert`**`(const Utf8Iterator& dst, const Utf8Iterator& src_begin, const Utf8Iterator& src_end)`
* `Ustring` **`str_insert`**`(const Irange<Utf8Iterator>& dst, const Ustring& src)`
* `Ustring` **`str_insert`**`(const Irange<Utf8Iterator>& dst, const Irange<Utf8Iterator>& src)`
* `Ustring` **`str_insert`**`(const Utf8Iterator& dst_begin, const Utf8Iterator& dst_end, const Ustring& src)`
* `Ustring` **`str_insert`**`(const Utf8Iterator& dst_begin, const Utf8Iterator& dst_end, const Utf8Iterator& src_begin, const Utf8Iterator& src_end)`
* `Irange<Utf8Iterator>` **`str_insert_in`**`(Ustring& dst, const Utf8Iterator& where, const Ustring& src)`
* `Irange<Utf8Iterator>` **`str_insert_in`**`(Ustring& dst, const Utf8Iterator& where, const Irange<Utf8Iterator>& src)`
* `Irange<Utf8Iterator>` **`str_insert_in`**`(Ustring& dst, const Utf8Iterator& where, const Utf8Iterator& src_begin, const Utf8Iterator& src_end)`
* `Irange<Utf8Iterator>` **`str_insert_in`**`(Ustring& dst, const Irange<Utf8Iterator>& range, const Ustring& src)`
* `Irange<Utf8Iterator>` **`str_insert_in`**`(Ustring& dst, const Irange<Utf8Iterator>& range, const Irange<Utf8Iterator>& src)`
* `Irange<Utf8Iterator>` **`str_insert_in`**`(Ustring& dst, const Utf8Iterator& range_begin, const Utf8Iterator& range_end, const Ustring& src)`
* `Irange<Utf8Iterator>` **`str_insert_in`**`(Ustring& dst, const Utf8Iterator& range_begin, const Utf8Iterator& range_end, const Utf8Iterator& src_begin, const Utf8Iterator& src_end)`

These insert a copy of the source string into the destination string, either
at a specified location or replacing a specified substring. (For the versions
that return the result, the source string is not needed as an explicit
argument since it can be obtained from the iterator.) The effect is similar to
the `std::string::insert()` and `replace()` methods, except that positions
within the string are specified by UTF iterators instead of ordinary string
iterators or offsets. The `str_insert_in()` functions return a pair of
iterators delimiting the newly inserted replacement string within the updated
`dst`.

* `template <typename FwdRange> Ustring` **`str_join`**`(const FwdRange& r)`
* `template <typename FwdRange> Ustring` **`str_join`**`(const FwdRange& r, const Ustring& delim, bool term = false)`

These concatenate a list of strings, optionally inserting a delimiter between
each pair of strings. The value type of the range must be `Ustring` or
convertible to it. If the `term` argument is set, an extra delimiter will be
added after the last element (useful when joining lines to form a text that
would be expected to end with a line break).

* `Ustring` **`str_pad_left`**`(const Ustring& str, size_t length, char32_t c = U' ', uint32_t flags = 0)`
* `void` **`str_pad_left_in`**`(Ustring& str, size_t length, char32_t c = U' ', uint32_t flags = 0)`
* `Ustring` **`str_pad_right`**`(const Ustring& str, size_t length, char32_t c = U' ', uint32_t flags = 0)`
* `void` **`str_pad_right_in`**`(Ustring& str, size_t length, char32_t c = U' ', uint32_t flags = 0)`

Pad a string on the left or right to a specified length; the character
argument `c` is used for padding (converted to UTF-8). The string will be
returned unchanged if it is already equal to or longer than the required
length. If the string can't be adjusted to exactly the specified size (because
one of the East Asian width options was selected and wide characters are
present), the result will be one unit longer than the requested length.

* `bool` **`str_partition`**`(const Ustring& str, Ustring& prefix, Ustring& suffix)`
* `bool` **`str_partition_at`**`(const Ustring& str, Ustring& prefix, Ustring& suffix, const Ustring& delim)`
* `bool` **`str_partition_by`**`(const Ustring& str, Ustring& prefix, Ustring& suffix, const Ustring& delim)`

These split a string into two parts at the first occurrence of a given
delimiter. If the delimiter is found, the two parts are written into `prefix`
and `suffix` (the delimiter itself is discarded), and the function returns
`true`; otherwise, the original string is copied into `prefix`, `suffix` is
made empty, and the function returns `false`. The `str_partition()` function
splits the string on the first contiguous sequence of whitespace characters;
`str_partition_at()` splits it at the first occurrence of the `delim` string;
and `str_partition_by()` splits it at the first contiguous sequence of
characters that are in the `delim` list. An empty delimiter string will be
treated as never being found.

* `Ustring` **`str_remove`**`(const Ustring& str, char32_t c)`
* `Ustring` **`str_remove`**`(const Ustring& str, const Ustring& chars)`
* `template <typename Pred> Ustring` **`str_remove_if`**`(const Ustring& str, Pred p)`
* `template <typename Pred> Ustring` **`str_remove_if_not`**`(const Ustring& str, Pred p)`
* `void` **`str_remove_in`**`(Ustring& str, char32_t c)`
* `void` **`str_remove_in`**`(Ustring& str, const Ustring& chars)`
* `template <typename Pred> void` **`str_remove_in_if`**`(Ustring& str, Pred p)`
* `template <typename Pred> void` **`str_remove_in_if_not`**`(Ustring& str, Pred p)`

These remove all occurrences of a specific character, all characters in a set,
or characters matching (or not matching) a condition from the string.

* `Ustring` **`str_repeat`**`(const Ustring& str, size_t n)`
* `void` **`str_repeat_in`**`(Ustring& str, size_t n)`

Return a string formed by concatenating `n` copies of the original string.

* `Ustring` **`str_replace`**`(const Ustring& str, const Ustring& target, const Ustring& sub, size_t n = npos)`
* `void` **`str_replace_in`**`(Ustring& str, const Ustring& target, const Ustring& sub, size_t n = npos)`

These return a copy of the first argument string, with the first `n`
substrings that match `target` replaced with `sub`. By default, all matches
are replaced. The string will be returned unchanged if `target` is empty or
`n=0`.

* `template <typename OutIter> void` **`str_split`**`(const Ustring& src, OutIter dst)`
* `template <typename OutIter> void` **`str_split_at`**`(const Ustring& src, OutIter dst, const Ustring& delim)`
* `template <typename OutIter> void` **`str_split_by`**`(const Ustring& src, OutIter dst, const Ustring& delim)`
* `Strings` **`str_splitv`**`(const Ustring& src)`
* `Strings` **`str_splitv_at`**`(const Ustring& src, const Ustring& delim)`
* `Strings` **`str_splitv_by`**`(const Ustring& src, const Ustring& delim)`

These split a string into substrings, using the specified delimiter to mark
the substring boundaries, and copying the resulting substrings into the
destination defined by the output iterator. The `splitv` versions return a
vector instead of writing to an existing container. The `str_split[v]()`
function splits the string on each contiguous sequence of whitespace
characters; `str_split[v]_at()` splits it at each occurrence of the `delim`
string; and `str_split[v]_by()` splits it at each contiguous sequence of
characters that are in the `delim` list. Nothing will be written if the
original source string is empty; if the delimiter string is empty (but the
source string is not), a single string will be written.

* `template <typename OutIter> void` **`str_split_lines`**`(const Ustring& src, OutIter dst)`
* `Strings` **`str_splitv_lines`**`(const Ustring& src)`

Split a string at each line break, copying lines into the output iterator. Any
character that passes `char_is_line_break()` is counted as a line break. The
line break characters are not included in the output strings. The `CR+LF`
combination is counted as a single line break; any other sequence of multiple
line breaks will generate empty lines in the output. An empty line will not be
generated at the end if the last character in the input was a line break.

* `Ustring` **`str_squeeze`**`(const Ustring& str)`
* `Ustring` **`str_squeeze`**`(const Ustring& str, const Ustring& chars)`
* `Ustring` **`str_squeeze_trim`**`(const Ustring& str)`
* `Ustring` **`str_squeeze_trim`**`(const Ustring& str, const Ustring& chars)`
* `void` **`str_squeeze_in`**`(Ustring& str)`
* `void` **`str_squeeze_in`**`(Ustring& str, const Ustring& chars)`
* `void` **`str_squeeze_trim_in`**`(Ustring& str)`
* `void` **`str_squeeze_trim_in`**`(Ustring& str, const Ustring& chars)`

These replace every sequence of one or more characters from `chars` with the
first character in `chars`. By default, if `chars` is not supplied, every
sequence of whitespace characters will be replaced with a single space. The
`str_squeeze_trim()` functions do the same thing, except that leading and
trailing characters from `chars` are removed completely instead of reduced to
one character. In all cases, the original string will be left unchanged if
`chars` is empty.

* `Ustring` **`str_substring`**`(const Ustring& str, size_t offset, size_t count = npos)`
* `Ustring` **`utf_substring`**`(const Ustring& str, size_t index, size_t length = npos, uint32_t flags = 0)`

These return a substring of the original string. The `str_substring()`
function returns the same string as `basic_string::substr()`, except that an
offset out of bounds will yield an empty string instead of undefined
behaviour; `utf_substring()` does the same thing, except that the position and
length of the substring are measured according according to the `flags`
argument instead of by code units (the flags are the same as for
`str_length()`, defaulting to characters).

* `Ustring` **`str_translate`**`(const Ustring& str, const Ustring& target, const Ustring& sub)`
* `void` **`str_translate_in`**`(Ustring& str, const Ustring& target, const Ustring& sub)`

These return a copy of the first argument string, with any characters that
occur in `target` replaced with the corresponding character in `sub`. The
string will be returned unchanged if either `target` or `sub` is empty. If
`target` is longer than `sub`, `sub` will be extended to match the length of
`target` by repeating its last character; if `target` is shorter than `sub`,
the extra characters in `sub` are ignored. If the same character occurs more
than once in `target`, only the first is used. (This function is similar to
the Unix `tr` utility.)

* `Ustring` **`str_trim`**`(const Ustring& str)`
* `Ustring` **`str_trim`**`(const Ustring& str, const Ustring& chars)`
* `void` **`str_trim_in`**`(Ustring& str)`
* `void` **`str_trim_in`**`(Ustring& str, const Ustring& chars)`
* `Ustring` **`str_trim_left`**`(const Ustring& str)`
* `Ustring` **`str_trim_left`**`(const Ustring& str, const Ustring& chars)`
* `void` **`str_trim_left_in`**`(Ustring& str)`
* `void` **`str_trim_left_in`**`(Ustring& str, const Ustring& chars)`
* `Ustring` **`str_trim_right`**`(const Ustring& str)`
* `Ustring` **`str_trim_right`**`(const Ustring& str, const Ustring& chars)`
* `void` **`str_trim_right_in`**`(Ustring& str)`
* `void` **`str_trim_right_in`**`(Ustring& str, const Ustring& chars)`
* `template <typename Pred> Ustring` **`str_trim_if`**`(const Ustring& str, Pred p)`
* `template <typename Pred> Ustring` **`str_trim_if_not`**`(const Ustring& str, Pred p)`
* `template <typename Pred> void` **`str_trim_in_if`**`(const Ustring& str, Pred p)`
* `template <typename Pred> void` **`str_trim_in_if_not`**`(const Ustring& str, Pred p)`
* `template <typename Pred> Ustring` **`str_trim_left_if`**`(const Ustring& str, Pred p)`
* `template <typename Pred> Ustring` **`str_trim_left_if_not`**`(const Ustring& str, Pred p)`
* `template <typename Pred> void` **`str_trim_left_in_if`**`(const Ustring& str, Pred p)`
* `template <typename Pred> void` **`str_trim_left_in_if_not`**`(const Ustring& str, Pred p)`
* `template <typename Pred> Ustring` **`str_trim_right_if`**`(const Ustring& str, Pred p)`
* `template <typename Pred> Ustring` **`str_trim_right_if_not`**`(const Ustring& str, Pred p)`
* `template <typename Pred> void` **`str_trim_right_in_if`**`(const Ustring& str, Pred p)`
* `template <typename Pred> void` **`str_trim_right_in_if_not`**`(const Ustring& str, Pred p)`

These trim unwanted characters from one or both ends of the string. By
default, any whitespace characters (according to the Unicode property) are
stripped; alternatively, you can supply a string containing the list of
unwanted characters, or a predicate function that takes a character and
returns `true` if the character should be trimmed. The predicate takes a
Unicode character, i.e. a `char32_t`, not a code unit.

* `Ustring` **`str_unify_lines`**`(const Ustring& str, char32_t newline = U'\n')`
* `Ustring` **`str_unify_lines`**`(const Ustring& str, const Ustring& newline)`
* `void` **`str_unify_lines_in`**`(Ustring& str, char32_t newline = U'\n')`
* `void` **`str_unify_lines_in`**`(Ustring& str, const Ustring& newline)`

These convert all line breaks to the same form, a single `LF` by default. Any
Unicode line or paragraph breaking character is recognised and replaced; the
`CR+LF` sequence is also treated as a single line break. Unless the original
string was completely empty, a line break will be added at the end if it was
not already there.

* `class` **`Wrap`**
    * _Keyword arguments (see below)_
    * `Wrap::`**`Wrap`**`()`
    * `template <typename... Args> Wrap::`**`Wrap`**`(Args... args)`
    * `Ustring Wrap::`**`wrap`**`(const Ustring& src) const`
    * `void Wrap::`**`wrap_in`**`(Ustring& src) const`
    * `Ustring Wrap::`**`operator()`**`(const Ustring& src) const`
* `template <typename... Args> Ustring` **`str_wrap`**`(const Ustring& str, Args... args)`
* `template <typename... Args> void` **`str_wrap_in`**`(Ustring& str, Args... args)`

The `Wrap` class, and the two `str_wrap*()` functions, wrap the text in a
string to a given width. The class stores a set of wrapping parameters,
supplied as keyword arguments to the constructor; the two free functions may
be more convenient when only a single call is needed and the parameters do not
need to be saved. The function call operator is equivalent to the `wrap()`
method.

The following keyword arguments are recognised:

Keyword                 | Type        | Description                                | Default
-------                 | ----        | -----------                                | -------
`Wrap::`**`enforce`**   | `bool`      | Enforce right margin strictly              | `false`
`Wrap::`**`lines`**     | `bool`      | Treat every line as a paragraph            | `false`
`Wrap::`**`preserve`**  | `bool`      | Preserve layout on already indented lines  | `false`
`Wrap::`**`flags`**     | `uint32_t`  | Flags for string length                    | `Length::graphemes`
`Wrap::`**`margin`**    | `size_t`    | Margin for first line                      | 0
`Wrap::`**`margin2`**   | `size_t`    | Margin for subsequent lines                | same as `margin`
`Wrap::`**`width`**     | `size_t`    | Wrap width                                 | see below
`Wrap::`**`newline`**   | `Ustring`   | Line break on output                       | `"\n"`
`Wrap::`**`newpara`**   | `Ustring`   | Paragraph break on output                  | two `newline`

Wrapping is done separately for each paragraph. Words are simply delimited by
whitespace, which may not be appropriate for all languages; no attempt is made
at anything more sophisticated such as hyphenation or locale-specific word
breaking rules.

Paragraphs are normally delimited by two or more line breaks; if the `lines`
flag is set, every line break is interpreted as a paragraph break. As usual,
`CR+LF` is counted as a single line break.

Width is measured using the usual rules for string length, controlled by the
`flags` argument. By default, the width is set to two characters less than the
current terminal width, obtained from the `COLUMNS` environment variable; the
terminal width is assumed to be 80 characters if `COLUMNS` is undefined or
invalid. The `margin` and `margin2` arguments determine the number of spaces
used to indent the first and subsequent lines, respectively, of a paragraph
(the width includes the indentation); if `margin2` is not supplied, it
defaults to the same value as `margin`. The function will throw
`std::length_error` if either margin is greater than or equal to the width.

The `newline` value is used for all line breaks in the output; any line
breaking already present in the input text is discarded. The `newpara` value
is used for paragraph breaks in the output; the default is two line breaks. A
single line break is inserted at the end of the output. If the `preserve` flag
is used, any paragraphs that start with an indented line are left in their
original layout.

If a single word is too long to fit on one line, the default behaviour is to
allow it to violate the right margin. If the `enforce` flag is used, this will
cause the function to throw `std::length_error` instead.

## Case mapping functions ##

* `Ustring` **`str_uppercase`**`(const Ustring& str)`
* `void` **`str_uppercase_in`**`(Ustring& str)`
* `Ustring` **`str_lowercase`**`(const Ustring& str)`
* `void` **`str_lowercase_in`**`(Ustring& str)`
* `Ustring` **`str_titlecase`**`(const Ustring& str)`
* `void` **`str_titlecase_in`**`(Ustring& str)`
* `Ustring` **`str_casefold`**`(const Ustring& str)`
* `void` **`str_casefold_in`**`(Ustring& str)`
* `Ustring` **`str_case`**`(const Ustring& str, Case c)`
* `void` **`str_case_in`**`(Ustring& str, Case c)`

These convert a string to upper case, lower case, title case, or the case
folded form (the form recommended by Unicode for case insensitive string
comparison; this is similar, but not always identical, to the lower case
form). These use the full Unicode case mappings; the returned string will not
necessarily be the same length as the original string (measured either in code
units or characters). These functions only perform the default case mappings
recommended by the Unicode standard; they do not make any attempt at
localisation.

* `Ustring` **`str_initial_titlecase`**`(const Ustring& str)`
* `void` **`str_initial_titlecase_in`**`(Ustring& str)`

These change just the first character of a string to title case, leaving the
rest of the string unchanged.

## Escaping and quoting functions ##

Flag                    | Description
----                    | -----------
`Escape::`**`ascii`**   | Escape all non-ASCII characters
`Escape::`**`nostdc`**  | Do not use standard C symbols such as `\n`
`Escape::`**`pcre`**    | Use `\x{...}` instead of `\u` and `\U` (implies `ascii`)
`Escape::`**`punct`**   | Escape all ASCII punctuation

Flags recognised by `str_escape()` and related functions.

* `Ustring` **`str_encode_uri`**`(const Ustring& str)`
* `Ustring` **`str_encode_uri_component`**`(const Ustring& str)`
* `void` **`str_encode_uri_in`**`(Ustring& str)`
* `void` **`str_encode_uri_component_in`**`(Ustring& str)`

These replace some characters in the string with percent encoding. These are
similar to the correspondingly named JavaScript functions, except that they
follow the slightly more stringent rules from [RFC
3986](https://www.ietf.org/rfc/rfc3986.txt). Characters outside the graphic
ASCII range (`0x21-0x7e`) will always be encoded; ASCII alphanumeric
characters will never be encoded. ASCII punctuation is selectively encoded:

| Characters            | Behaviour
| ----------            | ---------
| `"%<>\^`\``{|}`       | Encoded by both `str_encode_uri()` and `str_encode_uri_component()`
| `!#$&'()*+,/:;=?@[]`  | Encoded by `str_encode_uri_component()` but not by `str_encode_uri()`
| `-._~`                | Left unencoded by both functions

These functions only apply percent encoding; they do not make any attempt to
support IDNA domain names.

* `Ustring` **`str_unencode_uri`**`(const Ustring& str)`
* `void` **`str_unencode_uri_in`**`(Ustring& str)`

These perform the reverse transformation to `str_encode_uri()` and
`str_encode_uri_component()`, replacing percent escape codes with the original
characters.

* `Ustring` **`str_escape`**`(const Ustring& str, uint32_t flags = 0)`
* `void` **`str_escape_in`**`(Ustring& str, uint32_t flags = 0)`

Replace some of the characters in the string with escape codes using a leading
backslash. By default, only C0 and C1 control characters, plus the backslash
itself, will be escaped, and conventional C codes such as `"\n"` will be used
instead of `"\x..."` for the relevant control characters. These behaviour
settings can be changed by using the flags listed above.

* `Ustring` **`str_unescape`**`(const Ustring& str)`
* `void` **`str_unescape_in`**`(Ustring& str)`

These perform the reverse transformation to `str_escape()`, replacing escape
codes with the original characters. If a backslash is followed by a character
not recognised as an escape code, the backslash will simply be discarded and
the second character left unchanged. Any of `"\xHH"`, `"\x{HHH...}"`,
`"\uHHHH"`, and `"\UHHHHHHHH"` can be used to encode hexadecimal code points.
These will throw `EncodingError` if a hex code does not represent a valid
Unicode scalar value.

* `Ustring` **`str_quote`**`(const Ustring& str, uint32_t flags = 0, char32_t quote = '\"')`
* `void` **`str_quote_in`**`(Ustring& str, uint32_t flags = 0, char32_t quote = '\"')`

These perform the same operation as `str_escape()`, except that quote marks
are added around the string, and internal quotes are escaped.

* `Ustring` **`str_unquote`**`(const Ustring& str, char32_t quote = '\"')`
* `void` **`str_unquote_in`**`(Ustring& str, char32_t quote = '\"')`

These perform the reverse transformation to `str_quote()`, removing quote
marks from the string, or from any quoted substrings within it, and then
unescaping the resulting strings.

## Type conversion functions ##

* `template <typename T> size_t` **`str_to_int`**`(T& t, const Ustring& str, size_t offset = 0, uint32_t flags = 0)`
* `template <typename T> Utf8Iterator` **`str_to_int`**`(T& t, const Utf8Iterator& start, uint32_t flags = 0)`
* `template <typename T> T` **`str_to_int`**`(const Ustring& str, uint32_t flags = 0)`
* `template <typename T> T` **`str_to_int`**`(const Utf8Iterator& start, uint32_t flags = 0)`
* `template <typename T> size_t` **`hex_to_int`**`(T& t, const Ustring& str, size_t offset = 0, uint32_t flags = 0)`
* `template <typename T> Utf8Iterator` **`hex_to_int`**`(T& t, const Utf8Iterator& start, uint32_t flags = 0)`
* `template <typename T> T` **`hex_to_int`**`(const Ustring& str, uint32_t flags = 0)`
* `template <typename T> T` **`hex_to_int`**`(const Utf8Iterator& start, uint32_t flags = 0)`
* `template <typename T> size_t` **`str_to_float`**`(T& t, const Ustring& str, size_t offset = 0, uint32_t flags = 0)`
* `template <typename T> Utf8Iterator` **`str_to_float`**`(T& t, const Utf8Iterator& start, uint32_t flags = 0)`
* `template <typename T> T` **`str_to_float`**`(const Ustring& str, uint32_t flags = 0)`
* `template <typename T> T` **`str_to_float`**`(const Utf8Iterator& start, uint32_t flags = 0)`

Conversions from a string to an integer (in decimal or hexadecimal) or a
floating point number. In each set of four overloaded functions, the first two
versions write the result into a variable passed by reference, and return the
number of bytes read from the string, or an iterator marking the end of the
number; the last two versions return the result, require the return type to be
explicitly specified at the call site, and do not return any indication of
where the number ended.

Any characters after a valid number are ignored. Note that, unlike the
otherwise similar `strtol()` and related functions, these do not skip leading
whitespace. For the versions that take a string and offset, results are
unspecified if the offset does not point to an encoded character boundary.

The only flag recognised is `Utf::throws`. By default, a value out of range
for the return type will be clamped to the nearest end of its valid range, and
the result will be zero if the string does not contain a valid number. If
`Utf::throws` is set, an invalid number will throw `std::invalid_argument`,
and a result out of range will throw `std::range_error`. The versions that
return the result by value will also throw `std::invalid_argument` if
`Utf::throws` is set and there are any other characters following the number.
In the versions that take the result as a reference argument, this will be
left unchanged if an exception is thrown.
