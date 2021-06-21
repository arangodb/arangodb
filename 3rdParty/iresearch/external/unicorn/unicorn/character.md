# [Unicorn Library](index.html): Character Properties #

_Unicode library for C++ by Ross Smith_

* `#include "unicorn/character.hpp"`

This module contains functions and constants relating to Unicode characters
and their basic properties, as well as version information about the Unicorn
library itself.

## Contents ##

[TOC]

## Version information ##

* `Version` **`unicorn_version`**`() noexcept`
* `Version` **`unicode_version`**`() noexcept`

These return the version of the Unicorn library and the supported version of
the Unicode standard.

## Constants ##

* `constexpr char32_t` **`last_ascii_char`** `=            0x7f      = Highest ASCII code point`
* `constexpr char32_t` **`last_latin1_char`** `=           0xff      = Highest ISO 8859 code point`
* `constexpr char32_t` **`line_separator_char`** `=        0x2028    = Unicode line separator character`
* `constexpr char32_t` **`paragraph_separator_char`** `=   0x2029    = Unicode paragraph separator character`
* `constexpr char32_t` **`first_surrogate_char`** `=       0xd800    = First UTF-16 surrogate code`
* `constexpr char32_t` **`first_high_surrogate_char`** `=  0xd800    = First UTF-16 high surrogate code`
* `constexpr char32_t` **`last_high_surrogate_char`** `=   0xdbff    = Last UTF-16 high surrogate code`
* `constexpr char32_t` **`first_low_surrogate_char`** `=   0xdc00    = First UTF-16 low surrogate code`
* `constexpr char32_t` **`last_low_surrogate_char`** `=    0xdfff    = Last UTF-16 low surrogate code`
* `constexpr char32_t` **`last_surrogate_char`** `=        0xdfff    = Last UTF-16 surrogate code`
* `constexpr char32_t` **`first_private_use_char`** `=     0xe000    = Beginning of BMP private use area`
* `constexpr char32_t` **`last_private_use_char`** `=      0xf8ff    = End of BMP private use area`
* `constexpr char32_t` **`first_noncharacter`** `=         0xfdd0    = Beginning of reserved noncharacter block`
* `constexpr char32_t` **`last_noncharacter`** `=          0xfdef    = End of reserved noncharacter block`
* `constexpr char32_t` **`byte_order_mark`** `=            0xfeff    = Unicode byte order mark`
* `constexpr char32_t` **`replacement_char`** `=           0xfffd    = Unicode replacement character`
* `constexpr char32_t` **`last_bmp_char`** `=              0xffff    = End of basic multilingual plane`
* `constexpr char32_t` **`first_private_use_a_char`** `=   0xf0000   = Beginning of supplementary private use area A`
* `constexpr char32_t` **`last_private_use_a_char`** `=    0xffffd   = End of supplementary private use area A`
* `constexpr char32_t` **`first_private_use_b_char`** `=   0x100000  = Beginning of supplementary private use area B`
* `constexpr char32_t` **`last_private_use_b_char`** `=    0x10fffd  = End of supplementary private use area B`
* `constexpr char32_t` **`last_unicode_char`** `=          0x10ffff  = Highest possible Unicode code point`

Some useful Unicode code points.

* `constexpr uint8_t` **`min_utf8_leading`** `=   0xc2  = Minimum leading byte in a multibyte UTF-8 character`
* `constexpr uint8_t` **`max_utf8_leading`** `=   0xf4  = Maximum leading byte in a multibyte UTF-8 character`
* `constexpr uint8_t` **`min_utf8_trailing`** `=  0x80  = Minimum trailing byte in a multibyte UTF-8 character`
* `constexpr uint8_t` **`max_utf8_trailing`** `=  0xbf  = Maximum trailing byte in a multibyte UTF-8 character`

UTF-8 code unit ranges.

* `constexpr const char*` **`utf8_bom`** `=          "\xef\xbb\xbf"  = Byte order mark (U+FEFF) in UTF-8`
* `constexpr const char*` **`utf8_replacement`** `=  "\xef\xbf\xbd"  = Replacement character (U+FFFD) in UTF-8`

Byte order mark and replacement character in UTF-8.

* `constexpr size_t` **`max_case_decomposition`** `=           3   = Maximum length of a full case mapping`
* `constexpr size_t` **`max_canonical_decomposition`** `=      2   = Maximum length of a canonical decomposition`
* `constexpr size_t` **`max_compatibility_decomposition`** `=  18  = Maximum length of a compatibility decomposition`

The maximum number of characters that a single character can expand into,
under case mapping or decomposition. Note that these represent the maximum
size of a single decomposition step; decomposition is normally applied
recursively, so a single character may end up exceeding these sizes after the
complete decomposition process has been applied.

## Exceptions ##

* `class` **`InitializationError`**`: public std::runtime_error`

An abstract base class for internal errors that may happen while loading the
Unicode tables used by the character functions. Functions that can throw
exceptions derived from this are individually documented.

## Basic character functions ##

* `Ustring` **`char_as_hex`**`(char32_t c)`

Formats a code point in the conventional `U+XXXX` notation.

* `constexpr bool` **`char_is_digit`**`(char32_t c) noexcept`
* `constexpr bool` **`char_is_xdigit`**`(char32_t c) noexcept`

These match only the corresponding ASCII characters.

* `constexpr bool` **`char_is_unicode`**`(char32_t c) noexcept`
* `constexpr bool` **`char_is_ascii`**`(char32_t c) noexcept`
* `constexpr bool` **`char_is_latin1`**`(char32_t c) noexcept`
* `constexpr bool` **`char_is_bmp`**`(char32_t c) noexcept`
* `constexpr bool` **`char_is_astral`**`(char32_t c) noexcept`
* `constexpr bool` **`char_is_surrogate`**`(char32_t c) noexcept`
* `constexpr bool` **`char_is_high_surrogate`**`(char32_t c) noexcept`
* `constexpr bool` **`char_is_low_surrogate`**`(char32_t c) noexcept`
* `constexpr bool` **`char_is_noncharacter`**`(char32_t c) noexcept`
* `constexpr bool` **`char_is_private_use`**`(char32_t c) noexcept`

Basic character classification. These are properties that are related to
simple ranges of code points, without requiring reference to the full Unicode
property tables.

The `char_is_noncharacter()` function only returns true for the Unicode scalar
values that are explicitly designated as noncharacters, not for implicit
noncharacters such as surrogates.

* `template <typename C> constexpr uint32_t` **`char_to_uint`**`(C c) noexcept`

Converts any character type to a 32-bit integer containing the corresponding
Unicode code point. This should be used in preference to simply casting a
character to an integer, because plain `char` is signed on most systems; this
means that 8 bit code points are negative when stored in a `char`, and casting
them directly to an unsigned integer will give the wrong answer.

* `template <typename T> constexpr bool` **`is_character_type`**

True if `T` is one of the character types recognized by the Unicorn library
(`char`, `char16_t`, `char32_t`, or `wchar_t`).

## General category ##

* `enum class` **`GC`**`: uint16_t`
    * _Miscellaneous_
        * **`Cc`** _[control]_
        * **`Cf`** _[format]_
        * **`Cn`** _[unassigned]_
        * **`Co`** _[private use]_
        * **`Cs`** _[surrogate]_
    * _Letters_
        * **`Ll`** _[lowercase letter]_
        * **`Lm`** _[modifier letter]_
        * **`Lo`** _[other letter]_
        * **`Lt`** _[titlecase letter]_
        * **`Lu`** _[uppercase letter]_
    * _Marks_
        * **`Mc`** _[spacing mark]_
        * **`Me`** _[enclosing mark]_
        * **`Mn`** _[nonspacing mark]_
    * _Numbers_
        * **`Nd`** _[decimal number]_
        * **`Nl`** _[letter number]_
        * **`No`** _[other number]_
    * _Punctuation_
        * **`Pc`** _[connector punctuation]_
        * **`Pd`** _[dash punctuation]_
        * **`Pe`** _[close punctuation]_
        * **`Pf`** _[final punctuation]_
        * **`Pi`** _[initial punctuation]_
        * **`Po`** _[other punctuation]_
        * **`Ps`** _[open punctuation]_
    * _Symbols_
        * **`Sc`** _[currency symbol]_
        * **`Sk`** _[modifier symbol]_
        * **`Sm`** _[math symbol]_
        * **`So`** _[other symbol]_
    * _Separators_
        * **`Zl`** _[line separator]_
        * **`Zp`** _[paragraph separator]_
        * **`Zs`** _[space separator]_
* `std::ostream&` **`operator<<`**`(std::ostream& o, GC cat)`

Constants corresponding to the standard GC values. Each constant is
numerically equal to a 16-bit integer composed from the ASCII values of the
two letters in the category's standard abbreviation.

* `GC` **`char_general_category`**`(char32_t c) noexcept`

Returns the general category of a character.

* `char` **`char_primary_category`**`(char32_t c) noexcept`

Returns the first letter of the character's general category.

* `bool` **`char_is_alphanumeric`**`(char32_t c) noexcept  [gc=L,N]`
* `bool` **`char_is_control`**`(char32_t c) noexcept       [gc=Cc]`
* `bool` **`char_is_format`**`(char32_t c) noexcept        [gc=Cf]`
* `bool` **`char_is_letter`**`(char32_t c) noexcept        [gc=L]`
* `bool` **`char_is_mark`**`(char32_t c) noexcept          [gc=M]`
* `bool` **`char_is_number`**`(char32_t c) noexcept        [gc=N]`
* `bool` **`char_is_punctuation`**`(char32_t c) noexcept   [gc=P]`
* `bool` **`char_is_symbol`**`(char32_t c) noexcept        [gc=S]`
* `bool` **`char_is_separator`**`(char32_t c) noexcept     [gc=Z]`

These check for a character's membership in a broad general category. (The
miscellaneous categories not listed here are covered elsewhere in this
module.)

* `bool` **`char_is_alphanumeric_w`**`(char32_t c) noexcept`
* `bool` **`char_is_letter_w`**`(char32_t c) noexcept`
* `bool` **`char_is_punctuation_w`**`(char32_t c) noexcept`

These perform the same checks as the similarly named functions above, except
that the underscore character is counted as a letter instead of a punctuation
mark.

* `function<bool(char32_t)>` **`gc_predicate`**`(GC cat, bool sense = true)`
* `function<bool(char32_t)>` **`gc_predicate`**`(const Ustring& cat, bool sense = true)`
* `function<bool(char32_t)>` **`gc_predicate`**`(const char* cat, bool sense = true)`

These return function objects that can be used to test a character for
membership in one or more categories. If the `sense` flag is false, the
predicate will instead return true for characters not in the selected
categories. The versions that take a string can check for multiple categories;
for example, `gc_predicate("L,Nd,Pcd")` gives you a function that will check
whether a character is a letter, digit, connector punctuation, or dash
punctuation. Following the convention suggested by the Unicode standard, the
special category `"LC"` or `"L&"` tests for a cased letter, i.e. equivalent to
`"Lltu"`.

* `Ustring` **`decode_gc`**`(GC cat)`
* `constexpr GC` **`encode_gc`**`(char c1, char c2) noexcept`
* `constexpr GC` **`encode_gc`**`(const char* cat) noexcept`
* `GC` **`encode_gc`**`(const Ustring& cat) noexcept`

These convert between a GC abbreviation (passed as either a pair of letters or
a string) and its integer code. The result of `encode_gc()` is unspecified if
the arguments are invalid.

* `vector<GC>` **`gc_list`**`()`

Returns a list of all valid general categories, in alphabetical order.

* `const char*` **`gc_name`**`(GC cat) noexcept`

Returns the description of the general category, as shown in the list above.

## Boolean properties ##

* `bool` **`char_is_assigned`**`(char32_t c) noexcept`
* `bool` **`char_is_unassigned`**`(char32_t c) noexcept`
* `bool` **`char_is_white_space`**`(char32_t c) noexcept`
* `bool` **`char_is_line_break`**`(char32_t c) noexcept`
* `bool` **`char_is_inline_space`**`(char32_t c) noexcept`
* `bool` **`char_is_id_start`**`(char32_t c) noexcept`
* `bool` **`char_is_id_nonstart`**`(char32_t c) noexcept`
* `bool` **`char_is_id_continue`**`(char32_t c) noexcept`
* `bool` **`char_is_xid_start`**`(char32_t c) noexcept`
* `bool` **`char_is_xid_nonstart`**`(char32_t c) noexcept`
* `bool` **`char_is_xid_continue`**`(char32_t c) noexcept`
* `bool` **`char_is_pattern_syntax`**`(char32_t c) noexcept`
* `bool` **`char_is_pattern_white_space`**`(char32_t c) noexcept`
* `bool` **`char_is_default_ignorable`**`(char32_t c) noexcept`
* `bool` **`char_is_soft_dotted`**`(char32_t c) noexcept`

Various boolean tests, mostly corresponding to standard Unicode character
properties. The `char_is_line_break()` function is true for characters with
line breaking property values `BK`, `CR`, `LF`, or `NL`; the
`char_is_inline_space()` function is true for whitespace characters that are
not line breaks.

## Bidirectional properties ##

* `Bidi_Class` **`bidi_class`**`(char32_t c) noexcept`
* `bool` **`char_is_bidi_mirrored`**`(char32_t c) noexcept`
* `char32_t` **`bidi_mirroring_glyph`**`(char32_t c) noexcept`
* `char32_t` **`bidi_paired_bracket`**`(char32_t c) noexcept`
* `char` **`bidi_paired_bracket_type`**`(char32_t c) noexcept`

Properties relevant to the Unicode bidirectional algorithm.

## Block properties ##

* `Ustring` **`char_block`**`(char32_t c)`

Returns the name of the block to which a character belongs, or an empty string
if it is not part of any block.

* `struct` **`BlockInfo`**
    * `Ustring BlockInfo::`**`name`**
    * `char32_t BlockInfo::`**`first`**
    * `char32_t BlockInfo::`**`last`**
* `const vector<BlockInfo>&` **`unicode_block_list`**`()`

The `unicode_block_list()` function returns a list of all Unicode character
blocks (in code point order).

## Case folding properties ##

* `enum class` **`Case`**`: uint32_t`
    * `Case::`**`none`**
    * `Case::`**`fold`**
    * `Case::`**`lower`**
    * `Case::`**`title`**
    * `Case::`**`upper`**

Enumeration values for character casing. When used as a query parameter,
`Case::none` is reported for characters that have no associated case;
`Case::fold` is never returned. When used as a transformation argument,
`Case::none` returns the input unchanged.

* `Case` **`char_case`**`(char32_t c) noexcept`
* `bool` **`char_is_case`**`(char32_t c, Case k) noexcept`
* `bool` **`char_is_cased`**`(char32_t c) noexcept`
* `bool` **`char_is_case_ignorable`**`(char32_t c) noexcept`
* `bool` **`char_is_uppercase`**`(char32_t c) noexcept`
* `bool` **`char_is_lowercase`**`(char32_t c) noexcept`
* `bool` **`char_is_titlecase`**`(char32_t c) noexcept`

Boolean Unicode properties related to case conversion.

* `char32_t` **`char_to_simple_uppercase`**`(char32_t c) noexcept`
* `char32_t` **`char_to_simple_lowercase`**`(char32_t c) noexcept`
* `char32_t` **`char_to_simple_titlecase`**`(char32_t c) noexcept`
* `char32_t` **`char_to_simple_casefold`**`(char32_t c) noexcept`
* `char32_t` **`char_to_simple_case`**`(char32_t c, Case k) noexcept`
* `size_t` **`char_to_full_uppercase`**`(char32_t c, char32_t* dst) noexcept`
* `size_t` **`char_to_full_lowercase`**`(char32_t c, char32_t* dst) noexcept`
* `size_t` **`char_to_full_titlecase`**`(char32_t c, char32_t* dst) noexcept`
* `size_t` **`char_to_full_casefold`**`(char32_t c, char32_t* dst) noexcept`
* `size_t` **`char_to_full_case`**`(char32_t c, char32_t* dst, Case k) noexcept`

Single-character case conversion functions. The simple case mapping functions
cover only one-to-one case conversions, while the full case mapping functions
also include case conversions that map one character to multiple characters.
For the full case mapping functions, the output buffer (the `dst` pointer) is
expected to have room for at least `max_case_decomposition` characters; the
function returns the number of characters actually written (which will never
be less than 1 or greater than `max_case_decomposition`).

These functions follow the universal case mapping conventions defined by
Unicode, and make no attempt at localization; locale-dependent cases such as
the Turkish _"I"_ are not handled (these belong in a separate localization
library).

## Character names ##

* `Ustring` **`char_name`**`(char32_t c, uint32_t flags = 0)`

Flag                    | Description
----                    | -----------
`Cname::`**`all`**      | Mask combining all of these flags
`Cname::`**`control`**  | Use the common ASCII or ISO 8859 names for control characters
`Cname::`**`label`**    | Generate the standard code point label for characters that do not have an official name
`Cname::`**`lower`**    | Return the name in lower case (excluding the `U+XXXX` prefix if present)
`Cname::`**`prefix`**   | Prefix the name with the code point in `U+XXXX` format
`Cname::`**`update`**   | Where the official name was in error and a suggested correction has been published, use that instead

Returns the name of a character. By default, only the official Unicode name is
returned; an empty string is returned if the character does not have an
official name. The `flags` argument can contain a bitwise-OR combination of
any of the options. If both `control` and `label` are present, `control` takes
precedence for characters that qualify for both.

## Decomposition properties ##

* `int` **`combining_class`**`(char32_t c) noexcept`

Returns the character's canonical combining class.

* `char32_t` **`canonical_composition`**`(char32_t c1, char32_t c2) noexcept`

Returns the canonical composition of the two characters, or zero if the two
characters do not combine.

* `size_t` **`canonical_decomposition`**`(char32_t c, char32_t* dst) noexcept`
* `size_t` **`compatibility_decomposition`**`(char32_t c, char32_t* dst) noexcept`

These generate the canonical or compatibility decomposition of a character
(`compatibility_decomposition()` will also return canonical decompositions).
The output buffer is expected to have room for at least
`max_canonical_decomposition` or `max_compatibility_decomposition` characters,
respectively; the functions return the number of characters actually written,
or zero if the character does not have a decomposition of the relevant type.

## Enumeration properties ##

* `enum class` **`Bidi_Class`**
    * `Default, AL, AN, B, BN, CS, EN, ES, ET, FSI, L, LRE, LRI, LRO, NSM, ON, PDF, PDI, R, RLE, RLI, RLO, S, WS`
* `enum class` **`East_Asian_Width`**
    * `N, A, F, H, Na, W`
* `enum class` **`Grapheme_Cluster_Break`**
    * `Other, Control, CR, EOT, Extend, L, LF, LV, LVT, Prepend, Regional_Indicator, SOT, SpacingMark, T, V`
* `enum class` **`Hangul_Syllable_Type`**
    * `NA, L, LV, LVT, T, V`
* `enum class` **`Indic_Positional_Category`**
    * `NA, Bottom, Bottom_And_Right, Left, Left_And_Right, Overstruck, Right, Top, Top_And_Bottom, Top_And_Bottom_And_Right, Top_And_Left, Top_And_Left_And_Right, Top_And_Right, Visual_Order_Left`
* `enum class` **`Indic_Syllabic_Category`**
    * `Other, Avagraha, Bindu, Brahmi_Joining_Number, Cantillation_Mark, Consonant, Consonant_Dead, Consonant_Final, Consonant_Head_Letter, Consonant_Killer, Consonant_Medial, Consonant_Placeholder, Consonant_Preceding_Repha, Consonant_Prefixed, Consonant_Subjoined, Consonant_Succeeding_Repha, Consonant_With_Stacker, Gemination_Mark, Invisible_Stacker, Joiner, Modifying_Letter, Non_Joiner, Nukta, Number, Number_Joiner, Pure_Killer, Register_Shifter, Syllable_Modifier, Tone_Letter, Tone_Mark, Virama, Visarga, Vowel, Vowel_Dependent, Vowel_Independent`
* `enum class` **`Joining_Group`**
    * `No_Joining_Group, Ain, Alaph, Alef, Beh, Beth, Burushaski_Yeh_Barree, Dalath_Rish, Dal, E, Farsi_Yeh, Feh, Fe, Final_Semkath, Gaf, Gamal, Hah, Heh_Goal, Heh, Heth, He, Kaf, Kaph, Khaph, Knotted_Heh, Lamadh, Lam, Meem, Mim, Noon, Nun, Nya, Pe, Qaf, Qaph, Reh, Reversed_Pe, Rohingya_Yeh, Sadhe, Sad, Seen, Semkath, Shin, Swash_Kaf, Syriac_Waw, Tah, Taw, Teh_Marbuta_Goal, Teh_Marbuta, Teth, Waw, Yeh_Barree, Yeh_With_Tail, Yeh, Yudh_He, Yudh, Zain, Zhain`
* `enum class` **`Joining_Type`**
    * `Dual_Joining, Join_Causing, Left_Joining, Non_Joining, Right_Joining, Transparent`
* `enum class` **`Line_Break`**
    * `XX, AI, AL, B2, BA, BB, BK, CB, CJ, CL, CM, CP, CR, EX, GL, H2, H3, HL, HY, ID, IN_, IS, JL, JT, JV, LF, NL, NS, NU, OP, PO, PR, QU, RI, SA, SG, SP, SY, WJ, ZW`
* `enum class` **`Numeric_Type`**
    * `None, Decimal, Digit, Numeric`
* `enum class` **`Sentence_Break`**
    * `Other, ATerm, Close, CR, EOT, Extend, Format, LF, Lower, Numeric, OLetter, SContinue, Sep, SOT, Sp, STerm, Upper`
* `enum class` **`Word_Break`**
    * `Other, ALetter, CR, Double_Quote, EOT, Extend, ExtendNumLet, Format, Hebrew_Letter, Katakana, LF, MidLetter, MidNum, MidNumLet, Newline, Numeric, Regional_Indicator, Single_Quote, SOT`
* `std::ostream&` **`operator<<`**`(std::ostream& o, Bidi_Class x)`
* `std::ostream&` **`operator<<`**`(std::ostream& o, East_Asian_Width x)`
* `std::ostream&` **`operator<<`**`(std::ostream& o, Grapheme_Cluster_Break x)`
* `std::ostream&` **`operator<<`**`(std::ostream& o, Hangul_Syllable_Type x)`
* `std::ostream&` **`operator<<`**`(std::ostream& o, Indic_Positional_Category x)`
* `std::ostream&` **`operator<<`**`(std::ostream& o, Indic_Syllabic_Category x)`
* `std::ostream&` **`operator<<`**`(std::ostream& o, Joining_Group x)`
* `std::ostream&` **`operator<<`**`(std::ostream& o, Joining_Type x)`
* `std::ostream&` **`operator<<`**`(std::ostream& o, Line_Break x)`
* `std::ostream&` **`operator<<`**`(std::ostream& o, Numeric_Type x)`
* `std::ostream&` **`operator<<`**`(std::ostream& o, Sentence_Break x)`
* `std::ostream&` **`operator<<`**`(std::ostream& o, Word_Break x)`

Enumeration property values. The spelling of the class and value names follows
their spelling in the Unicode standard, which is not entirely consistent about
naming conventions. (An exception is `Line_Break::IN_`, marked up to avoid
colliding with the Windows macro `IN`.)

* `East_Asian_Width` **`east_asian_width`**`(char32_t c) noexcept`
* `Grapheme_Cluster_Break` **`grapheme_cluster_break`**`(char32_t c) noexcept`
* `Hangul_Syllable_Type` **`hangul_syllable_type`**`(char32_t c) noexcept`
* `Indic_Positional_Category` **`indic_positional_category`**`(char32_t c) noexcept`
* `Indic_Syllabic_Category` **`indic_syllabic_category`**`(char32_t c) noexcept`
* `Joining_Group` **`joining_group`**`(char32_t c) noexcept`
* `Joining_Type` **`joining_type`**`(char32_t c) noexcept`
* `Line_Break` **`line_break`**`(char32_t c) noexcept`
* `Numeric_Type` **`numeric_type`**`(char32_t c) noexcept`
* `Sentence_Break` **`sentence_break`**`(char32_t c) noexcept`
* `Word_Break` **`word_break`**`(char32_t c) noexcept`

Functions returning the properties of a character.

## Numeric properties ##

* `pair<long long, long long>` **`numeric_value`**`(char32_t c)`

Returns the numeric value of a character, as a pair containg the numerator and
denominator of the value. The denominator will always be positive. If the
character is not numeric, the numeric value will be zero (expressed as `0/1`).

## Script properties ##

* `Ustring` **`char_script`**`(char32_t c)`
* `Strings` **`char_script_list`**`(char32_t c)`

These return the principal script associated with a character, or a list of
scripts (in unspecified order) for characters that are commonly used with
multiple scripts. These return the ISO 15924 four letter abbreviations of the
script names; use `script_name()` to convert these to full names.

* `Ustring` **`script_name`**`(const Ustring& abbr)`

Converts an ISO 15924 script code (case insensitive) to the full name of the
script. Unrecognised codes will return an empty string.
