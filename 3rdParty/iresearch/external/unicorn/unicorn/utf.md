# [Unicorn Library](index.html): UTF Encodings #

_Unicode library for C++ by Ross Smith_

* `#include "unicorn/utf.hpp"`

This module defines classes and functions for encoding, decoding, and
converting between the standard Unicode transformation formats: UTF-8, UTF-16,
and UTF-32. Encoded strings are stored in any of the standard C++ string
classes, with the encoding defined by the size of the code units: `string` (or
`Ustring`) holds UTF-8, `u16string` holds UTF-16, and `u32string` holds
UTF-32; `wstring` may hold either UTF-16 or UTF-32, depending on the compiler.

## Contents ##

[TOC]

## Constants ##

Flag                  | Description
----                  | -----------
`Utf::`**`ignore`**   | Assume valid UTF input
`Utf::`**`replace`**  | Replace invalid UTF with `U+FFFD`
`Utf::`**`throws`**   | Throw `EncodingError` on invalid UTF

These bitmask flags are used in most encoding conversion functions, and some
related functions, to indicate how to handle encoding errors in the input
data.

The `ignore` option is the default for the UTF conversion functions. This
tells the function to assume that the input is already known to be a valid UTF
encoding. If this is not true, behaviour is unspecified (but not undefined);
basically, the output will be garbage. The UTF conversion code is optimized
for this case.

The `replace` option causes invalid input encoding to be replaced with the
standard Unicode replacement character (`U+FFFD`). Error handling for invalid
UTF-8 subsequences follows the Unicode recommended behaviour (Unicode Standard
7.0, section 3.9, page 128).

The `throws` option causes any input encoding error to throw an
`EncodingError` exception.

Behaviour is unspecified if more than one of these flags is combined.

If ignoring errors sounds like an unsafe choice for the default action,
remember that the Unicorn library is designed with the intention that text
manipulation within a program will be done entirely in Unicode; text is
normally converted back and forth to other encodings, and checked for
validity, only at the point of input and output. Unlike the UTF conversion
functions in this module, the functions in [`unicorn/mbcs`](mbcs.html) that
convert between Unicode and other encodings default to `Utf::replace`, and do
not accept the `Utf::ignore` option.

## Exceptions ##

* `class` **`EncodingError`**`: public std::runtime_error`
    * `EncodingError::`**`EncodingError`**`()`
    * `explicit EncodingError::`**`EncodingError`**`(const Ustring& encoding, size_t offset = 0, char32_t c = 0)`
    * `template <typename C> EncodingError::`**`EncodingError`**`(const Ustring& encoding, size_t offset, const C* ptr, size_t n = 1)`
    * `const char* EncodingError::`**`encoding`**`() noexcept const`
    * `size_t EncodingError::`**`offset`**`() const noexcept`

An exception thrown to indicate a text encoding error encountered when
converting a string from one encoding to another, or when checking an encoded
string for validity. The offset of the offending data within the source string
(when available) can be retrieved through the `offset()` method. If possible,
a hexadecimal representation of the offending data will be included in the
error message.

## Single character functions ##

* `size_t` **`char_from_utf8`**`(const char* src, size_t n, char32_t& dst) noexcept`
* `size_t` **`char_from_utf16`**`(const char16_t* src, size_t n, char32_t& dst) noexcept`
* `size_t` **`char_to_utf8`**`(char32_t src, char* dst) noexcept`
* `size_t` **`char_to_utf16`**`(char32_t src, char16_t* dst) noexcept`

Read one character from `src` (reading up to `n` code units for the decoding
functions), and write the decoded or encoded form into `dst`, returning the
number of code units read or written. If `src` does not contain a valid
character, zero is returned and `dst` is left unchanged. Behaviour is
undefined if a pointer argument is null.

* `template <typename C> size_t` **`code_units`**`(char32_t c)`

Returns the number of code units in the encoding of the character `c`, in the
UTF encoding implied by the character type `C`, or zero if `c` is not a valid
Unicode character.

* `template <typename C> bool` **`is_single_unit`**`(C c)` _-- This code unit represents a character by itself_
* `template <typename C> bool` **`is_start_unit`**`(C c)` _-- This is the first code unit of a multi-unit character_
* `template <typename C> bool` **`is_nonstart_unit`**`(C c)` _-- This is the second or subsequent code unit of a multi-unit character_
* `template <typename C> bool` **`is_invalid_unit`**`(C c)` _-- This value is not a legal code unit_
* `template <typename C> bool` **`is_initial_unit`**`(C c)` _-- Either a single unit or a start unit_

These give the properties of individual code units. Exactly one of the first
four functions will be true for any value of the argument.

## UTF decoding iterator ##

* `template <typename C> class` **`UtfIterator`**
    * `using UtfIterator::`**`code_unit`** `= C`
    * `using UtfIterator::`**`string_type`** `= basic_string<C>`
    * `using UtfIterator::`**`difference_type`** `= ptrdiff_t`
    * `using UtfIterator::`**`iterator_category`** `= std::bidirectional_iterator_tag`
    * `using UtfIterator::`**`pointer`** `= const char32_t*`
    * `using UtfIterator::`**`reference`** `= const char32_t&`
    * `using UtfIterator::`**`value_type`** `= char32_t`
    * `UtfIterator::`**`UtfIterator`**`() noexcept`
    * `explicit UtfIterator::`**`UtfIterator`**`(const string_type& src)`
    * `UtfIterator::`**`UtfIterator`**`(const string_type& src, size_t offset, uint32_t flags = 0)`
    * `size_t UtfIterator::`**`count`**`() const noexcept`
    * `size_t UtfIterator::`**`offset`**`() const noexcept`
    * `UtfIterator UtfIterator::`**`offset_by`**`(ptrdiff_t n) const noexcept`
    * `const C* UtfIterator::`**`ptr`**`() const noexcept`
    * `const string_type& UtfIterator::`**`source`**`() const noexcept`
    * `string_type` **`str`**`() const`
    * `bool UtfIterator::`**`valid`**`() const noexcept`
    * `std::basic_string_view<C> UtfIterator::`**`view`**`() const noexcept`
    * _[standard iterator operations]_

This is a bidirectional iterator over any UTF-encoded text. The template
argument type (`C`) is the code unit type of the underlying encoded string,
the encoding form is determined by the size of the code unit. The iterator
dereferences to a Unicode character; incrementing or decrementing the iterator
moves it to the next or previous encoded character. The iterator holds a
reference to the underlying string; UTF iterators are invalidated by any of
the same operations on the underlying string that would invalidate an ordinary
string iterator.

The constructor can optionally take an offset into the subject string; if the
offset points to the beginning of an encoded character, the iterator will
start at that character. If the offset does not point to a character boundary,
it will be treated as an invalid character; such an iterator can be
incremented to the next character boundary in the normal way, but decrementing
past that point has unspecified behaviour.

The `flags` argument determines the behaviour when invalid encoded data is
found, as described above. If an `EncodingError` exception is caught and
handled, the iterator is still in a valid state, and can be dereferenced
(yielding `U+FFFD`), incremented, or decremented in the normal way.

When invalid UTF-8 data is replaced, the substitution rules recommended in the
Unicode Standard (section 3.9, table 3-8) are followed. Replacements in UTF-16
or 32 are always one-for-one.

Besides the normal operations that can be applied to an iterator,
`UtfIterator` has some extra member functions that can be used to query its
state. The `source()` function returns a reference to the underlying encoded
string. The `offset()` and `count()` functions return the position and length
(in code units) of the current encoded character (or the group of code units
currently being interpreted as an invalid character). The `view()` function
returns the same sequence of code units as a string view.

The `offset_by()` function returns an iterator moved by the specified number
of code units. The resulting offset will be clamped to the bounds of the
source string; behaviour is the same as for the constructor if it is not on a
character boundary.

The `ptr()` function returns a pointer to the start of the encoded character,
a pointer to the end of the underlying string if the iterator is past the end,
or a null pointer if the iterator was default constructed.

The `str()` function returns a copy of the code units making up the current
character. This will be empty if the iterator is default constructed or past
the end, but behaviour is undefined if this is called on any other kind of
invalid iterator.

The `valid()` function indicates whether the current character is valid; it
will always be true if `Utf::ignore` was set, and its value is unspecified on
a past-the-end iterator.

If the underlying string is UTF-32, this is just a simple pass-through
iterator, but if one of the non-default error handling options is selected, it
will check for valid Unicode characters and treat invalid code points as
errors.

* `using` **`Utf8Iterator`** `= UtfIterator<char>`
* `using` **`Utf16Iterator`** `= UtfIterator<char16_t>`
* `using` **`Utf32Iterator`** `= UtfIterator<char32_t>`
* `using` **`WcharIterator`** `= UtfIterator<wchar_t>`
* `using` **`Utf8Range`** `= Irange<Utf8Iterator>`
* `using` **`Utf16Range`** `= Irange<Utf16Iterator>`
* `using` **`Utf32Range`** `= Irange<Utf32Iterator>`
* `using` **`WcharRange`** `= Irange<WcharIterator>`

Convenience aliases for specific iterators and ranges.

* `template <typename C> UtfIterator<C>` **`utf_begin`**`(const basic_string<C>& src, uint32_t flags = 0)`
* `template <typename C> UtfIterator<C>` **`utf_end`**`(const basic_string<C>& src, uint32_t flags = 0)`
* `template <typename C> Irange<UtfIterator<C>>` **`utf_range`**`(const basic_string<C>& src, uint32_t flags = 0)`

These return iterators over an encoded string.

* `template <typename C> UtfIterator<C>` **`utf_iterator`**`(const basic_string<C>& src, size_t offset, uint32_t flags = 0)`

Returns an iterator pointing to a specific offset in a string.

* `template <typename C> basic_string<C>` **`u_str`**`(const UtfIterator<C>& i, const UtfIterator<C>& j)`
* `template <typename C> basic_string<C>` **`u_str`**`(const Irange<UtfIterator<C>>& range)`

These return a copy of the substring between two iterators.

## UTF encoding iterator ##

* `template <typename C> class` **`UtfWriter`**
    * `using UtfWriter::`**`code_unit`** `= C`
    * `using UtfWriter::`**`string_type`** `= basic_string<C>`
    * `using UtfWriter::`**`difference_type`** `= void`
    * `using UtfWriter::`**`iterator_category`** `= std::output_iterator_tag`
    * `using UtfWriter::`**`pointer`** `= void`
    * `using UtfWriter::`**`reference`** `= void`
    * `using UtfWriter::`**`value_type`** `= char32_t`
    * `UtfWriter::`**`UtfWriter`**`() noexcept`
    * `explicit UtfWriter::`**`UtfWriter`**`(string_type& dst) noexcept`
    * `UtfWriter::`**`UtfWriter`**`(string_type& dst, uint32_t fags) flagst`
    * `bool UtfWriter::`**`valid`**`() const noexcept`
    * _[standard iterator operations]_

This is an output iterator that writes encoded characters onto the end of a
string. As with `UtfIterator`, the encoding form is determined by the size of
the code unit type (`C`), and behaviour is undefined if the destination string
is destroyed while the iterator exists. Changing the destination string
through other means is allowed, however; the `UtfWriter` will continue to
write to the end of the modified string.

If an exception is thrown, nothing will be written to the output string.
Otherwise, the `flags` argument and the `valid()` function work in much the
same way as for `UtfIterator`.

* `using` **`Utf8Writer`** `= UtfWriter<char>`
* `using` **`Utf16Writer`** `= UtfWriter<char16_t>`
* `using` **`Utf32Writer`** `= UtfWriter<char32_t>`
* `using` **`WcharWriter`** `= UtfWriter<wchar_t>`

Convenience aliases for specific iterators.

* `template <typename C> UtfWriter<C>` **`utf_writer`**`(basic_string<C>& dst, uint32_t flags = 0) noexcept`

Returns an encoding iterator writing to the given destination string.

## UTF conversion functions ##

* `template <typename C1, typename C2> void` **`recode`**`(const basic_string<C1>& src, basic_string<C2>& dst, uint32_t flags = 0)`
* `template <typename C1, typename C2> void` **`recode`**`(const basic_string<C1>& src, size_t offset, basic_string<C2>& dst, uint32_t flags = 0)`
* `template <typename C1, typename C2> void` **`recode`**`(const C1* src, size_t count, basic_string<C2>& dst, uint32_t flags = 0)`
* `template <typename C2, typename C1> basic_string<C2>` **`recode`**`(const basic_string<C1>& src, uint32_t flags = 0)`
* `template <typename C2, typename C1> basic_string<C2>` **`recode`**`(const basic_string<C1>& src, size_t offset, uint32_t flags)`

Encoding conversion functions. These convert from one UTF encoding to another;
as usual, the encoding forms are determined by the size of the input (`C1`)
and output (`C2`) code units. The input string can be supplied as a string
object (with an optional starting offset), or a code unit pointer and length
(a null pointer is treated as an empty string).

The last two versions return the converted string instead of writing it to a
destination string passed by reference; in this case the output code unit type
must be supplied explicitly as a template argument.

The `flags` argument has its usual meaning. If the `Utf::ignore` flag is used,
and the source and destination code units are the same size, the string will
simply be copied unchanged. If the `Utf::throws` flag is used, and the
destination string was supplied by reference, after an exception is thrown the
destination string will contain the successfully converted part of the string
before the error.

* `template <typename C> Ustring` **`to_utf8`**`(const basic_string<C>& src, uint32_t flags = 0)`
* `template <typename C> u16string` **`to_utf16`**`(const basic_string<C>& src, uint32_t flags = 0)`
* `template <typename C> u32string` **`to_utf32`**`(const basic_string<C>& src, uint32_t flags = 0)`
* `template <typename C> wstring` **`to_wstring`**`(const basic_string<C>& src, uint32_t flags = 0)`
* `template <typename C> NativeString` **`to_native`**`(const basic_string<C>& src, uint32_t flags = 0)`

These are just shorthand for the corresponding invocation of `recode()`.

## UTF validation functions ##

* `template <typename C> void` **`check_string`**`(const basic_string<C>& str)`
* `template <typename C> bool` **`valid_string`**`(const basic_string<C>& str) noexcept`

These check for valid encoding. If the string contains invalid UTF,
`valid_string()` returns `false`, while `check_string()` throws
`EncodingError`.

* `template <typename C> basic_string<C>` **`sanitize`**`(const basic_string<C>& str)`
* `template <typename C> void` **`sanitize_in`**`(basic_string<C>& str)`

Ensure that the string is a valid UTF encoding, by replacing any invalid data
with the `U+FFFD` replacement character.

* `template <typename C> size_t` **`valid_count`**`(const basic_string<C>& str) noexcept`

Finds the position of the first invalid UTF encoding in a string. The return
value is the offset (in code units) to the first invalid code unit, or `npos`
if no invalid encoding is found.
