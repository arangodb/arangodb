# [Unicorn Library](index.html): Text Segmentation #

_Unicode library for C++ by Ross Smith_

* `#include "unicorn/segment.hpp"`

This module contains classes and functions for breaking text up into
characters, words, sentences, lines, and paragraphs. Most of the rules
followed here are defined in [Unicode Standard Annex 29: Unicode Text
Segmentation](http://www.unicode.org/reports/tr29/).

All of the iterators defined here dereference to a substring represented by a
pair of [UTF iterators](unicorn/utf.html), bracketing the text segment of
interest. As usual, the `u_str()` function can be used to copy the actual
substring if this is needed.

All of the flags used to control these functions are placed together in the
`Segment` sub-namespace, but only the flags specifically documented for each
function will affect that function; any other flags will be ignored.

## Contents ##

[TOC]

## Grapheme cluster boundaries ##

* `template <typename C> class` **`GraphemeIterator`**
    * `using GraphemeIterator::`**`utf_iterator`** `= UtfIterator<C>`
    * `using GraphemeIterator::`**`difference_type`** `= ptrdiff_t`
    * `using GraphemeIterator::`**`iterator_category`** `= std::forward_iterator_tag`
    * `using GraphemeIterator::`**`value_type`** `= Irange<utf_iterator>`
    * `using GraphemeIterator::`**`pointer`** `= const value_type*`
    * `using GraphemeIterator::`**`reference`** `= const value_type&`
    * `GraphemeIterator::`**`GraphemeIterator`**`()`
    * _[standard iterator operations]_
* `template <typename C> Irange<GraphemeIterator<C>>` **`grapheme_range`**`(const UtfIterator<C>& i, const UtfIterator<C>& j)`
* `template <typename C> Irange<GraphemeIterator<C>>` **`grapheme_range`**`(const Irange<UtfIterator<C>>& source)`
* `template <typename C> Irange<GraphemeIterator<C>>` **`grapheme_range`**`(const basic_string<C>& source)`

A forward iterator over the grapheme clusters (user-perceived characters) in a
Unicode string.

## Word boundaries ##

* `template <typename C> class` **`WordIterator`**
    * `using WordIterator::`**`utf_iterator`** `= UtfIterator<C>`
    * `using WordIterator::`**`difference_type`** `= ptrdiff_t`
    * `using WordIterator::`**`iterator_category`** `= std::forward_iterator_tag`
    * `using WordIterator::`**`value_type`** `= Irange<utf_iterator>`
    * `using WordIterator::`**`pointer`** `= const value_type*`
    * `using WordIterator::`**`reference`** `= const value_type&`
    * `WordIterator::`**`WordIterator`**`()`
    * _[standard iterator operations]_
* `template <typename C> Irange<WordIterator<C>>` **`word_range`**`(const UtfIterator<C>& i, const UtfIterator<C>& j, uint32_t flags = 0)`
* `template <typename C> Irange<WordIterator<C>>` **`word_range`**`(const Irange<UtfIterator<C>>& source, uint32_t flags = 0)`
* `template <typename C> Irange<WordIterator<C>>` **`word_range`**`(const basic_string<C>& source, uint32_t flags = 0)`

A forward iterator over the words in a Unicode string. By default, all
segments identified as "words" by the UAX29 algorithm are returned; this will
include whitespace between words, punctuation marks, etc. Flags can be used to
select only words containing at least one non-whitespace character, or only
words containing at least one alphanumeric character.

Flag                      | Description
----                      | -----------
`Segment::`**`unicode`**  | Report all UAX29 words (default)
`Segment::`**`graphic`**  | Report only words containing a non-whitespace character
`Segment::`**`alpha`**    | Report only words containing an alphanumeric character

## Sentence boundaries ##

* `template <typename C> class` **`SentenceIterator`**
    * `using SentenceIterator::`**`utf_iterator`** `= UtfIterator<C>`
    * `using SentenceIterator::`**`difference_type`** `= ptrdiff_t`
    * `using SentenceIterator::`**`iterator_category`** `= std::forward_iterator_tag`
    * `using SentenceIterator::`**`value_type`** `= Irange<utf_iterator>`
    * `using SentenceIterator::`**`pointer`** `= const value_type*`
    * `using SentenceIterator::`**`reference`** `= const value_type&`
    * `SentenceIterator::`**`SentenceIterator`**`()`
    * _[standard iterator operations]_
* `template <typename C> Irange<SentenceIterator<C>>` **`sentence_range`**`(const UtfIterator<C>& i, const UtfIterator<C>& j)`
* `template <typename C> Irange<SentenceIterator<C>>` **`sentence_range`**`(const Irange<UtfIterator<C>>& source)`
* `template <typename C> Irange<SentenceIterator<C>>` **`sentence_range`**`(const basic_string<C>& source)`

A forward iterator over the sentences in a Unicode string (as defined by
UAX29).

## Line boundaries ##

* `template <typename C> class` **`LineIterator`**
    * `using LineIterator::`**`utf_iterator`** `= UtfIterator<C>`
    * `using LineIterator::`**`difference_type`** `= ptrdiff_t`
    * `using LineIterator::`**`iterator_category`** `= std::forward_iterator_tag`
    * `using LineIterator::`**`value_type`** `= Irange<utf_iterator>`
    * `using LineIterator::`**`pointer`** `= const value_type*`
    * `using LineIterator::`**`reference`** `= const value_type&`
    * `LineIterator::`**`LineIterator`**`()`
    * _[standard iterator operations]_
* `template <typename C> Irange<LineIterator<C>>` **`line_range`**`(const UtfIterator<C>& i, const UtfIterator<C>& j, uint32_t flags = 0)`
* `template <typename C> Irange<LineIterator<C>>` **`line_range`**`(const Irange<UtfIterator<C>>& source, uint32_t flags = 0)`
* `template <typename C> Irange<LineIterator<C>>` **`line_range`**`(const basic_string<C>& source, uint32_t flags = 0)`

A forward iterator over the lines in a Unicode string. Lines are ended by any
character with the line break property. Multiple consecutive line break
characters are treated as separate lines; except that `CR+LF` is treated as a
single line break. By default, the segment identified by the dereferenced
iterator includes the terminating line break; if the `Segment::strip` flag is
set, the line break is excluded from the segment.

Flag                    | Description
----                    | -----------
`Segment::`**`keep`**   | Include line terminators in reported segments (default)
`Segment::`**`strip`**  | Do not include line terminators

## Paragraph boundaries ##

* `template <typename C> class` **`ParagraphIterator`**
    * `using ParagraphIterator::`**`utf_iterator`** `= UtfIterator<C>`
    * `using ParagraphIterator::`**`difference_type`** `= ptrdiff_t`
    * `using ParagraphIterator::`**`iterator_category`** `= std::forward_iterator_tag`
    * `using ParagraphIterator::`**`value_type`** `= Irange<utf_iterator>`
    * `using ParagraphIterator::`**`pointer`** `= const value_type*`
    * `using ParagraphIterator::`**`reference`** `= const value_type&`
    * `ParagraphIterator::`**`ParagraphIterator`**`()`
    * _[standard iterator operations]_
* `template <typename C> Irange<ParagraphIterator<C>>` **`paragraph_range`**`(const UtfIterator<C>& i, const UtfIterator<C>& j, uint32_t flags = 0)`
* `template <typename C> Irange<ParagraphIterator<C>>` **`paragraph_range`**`(const Irange<UtfIterator<C>>& source, uint32_t flags = 0)`
* `template <typename C> Irange<ParagraphIterator<C>>` **`paragraph_range`**`(const basic_string<C>& source, uint32_t flags = 0)`

A forward iterator over the paragraphs in a Unicode string. The flags passed
to the constructor determine how paragraphs are identified. By default, any
sequence of two or more line breaks ends a paragraph (as usual, `CR+LF` counts
as a single line break); the `Segment::line` flag causes every line break to
be interpreted as a paragraph break, while `Segment::unicode` restricts
paragraph breaks to the Unicode paragraph separator character (`U+2029`). The
`Segment::strip` flag works the same way as in `LineIterator`, skipping the
paragraph delimiters.

Flag                        | Description
----                        | -----------
`Segment::`**`multiline`**  | Divide into paragraphs using multiple line breaks (default)
`Segment::`**`line`**       | Divide into paragraphs using any line break
`Segment::`**`unicode`**    | Divide into paragraphs using only Paragraph Separator
`Segment::`**`keep`**       | Include paragraph terminators in reported segments (default)
`Segment::`**`strip`**      | Do not include paragraph terminators
