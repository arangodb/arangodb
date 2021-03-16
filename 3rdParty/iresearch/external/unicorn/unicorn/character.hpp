#pragma once

#include "unicorn/property-values.hpp"
#include "unicorn/utility.hpp"
#include <cstring>
#include <functional>
#include <map>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

RS_LDLIB(unicorn pcre2-8 z);

namespace RS::Unicorn {

    // Version information

    namespace UnicornDetail {

        struct UnicodeVersionTable {
            std::map<Version, char32_t> table;
            UnicodeVersionTable();
        };

        const UnicodeVersionTable& unicode_version_table();

    }

    Version unicorn_version() noexcept;
    Version unicode_version() noexcept;

    // Constants

    constexpr char32_t last_ascii_char                = 0x7f;            // Highest ASCII code point
    constexpr char32_t last_latin1_char               = 0xff;            // Highest ISO 8859 code point
    constexpr char32_t line_separator_char            = 0x2028;          // Line separator
    constexpr char32_t paragraph_separator_char       = 0x2029;          // Paragraph separator
    constexpr char32_t first_surrogate_char           = 0xd800;          // First UTF-16 surrogate code
    constexpr char32_t first_high_surrogate_char      = 0xd800;          // First UTF-16 high surrogate code
    constexpr char32_t last_high_surrogate_char       = 0xdbff;          // Last UTF-16 high surrogate code
    constexpr char32_t first_low_surrogate_char       = 0xdc00;          // First UTF-16 low surrogate code
    constexpr char32_t last_low_surrogate_char        = 0xdfff;          // Last UTF-16 low surrogate code
    constexpr char32_t last_surrogate_char            = 0xdfff;          // Last UTF-16 surrogate code
    constexpr char32_t first_private_use_char         = 0xe000;          // Beginning of BMP private use area
    constexpr char32_t last_private_use_char          = 0xf8ff;          // End of BMP private use area
    constexpr char32_t first_noncharacter             = 0xfdd0;          // Beginning of reserved noncharacter block
    constexpr char32_t last_noncharacter              = 0xfdef;          // End of reserved noncharacter block
    constexpr char32_t byte_order_mark                = 0xfeff;          // Byte order mark
    constexpr char32_t replacement_char               = 0xfffd;          // Unicode replacement character
    constexpr char32_t last_bmp_char                  = 0xffff;          // End of basic multilingual plane
    constexpr char32_t first_private_use_a_char       = 0xf0000;         // Beginning of supplementary private use area A
    constexpr char32_t last_private_use_a_char        = 0xffffd;         // End of supplementary private use area A
    constexpr char32_t first_private_use_b_char       = 0x100000;        // Beginning of supplementary private use area B
    constexpr char32_t last_private_use_b_char        = 0x10fffd;        // End of supplementary private use area B
    constexpr char32_t last_unicode_char              = 0x10ffff;        // Highest possible Unicode code point
    constexpr uint8_t min_utf8_leading                = 0xc2;            // Minimum leading byte in a multibyte UTF-8 character
    constexpr uint8_t max_utf8_leading                = 0xf4;            // Maximum leading byte in a multibyte UTF-8 character
    constexpr uint8_t min_utf8_trailing               = 0x80;            // Minimum trailing byte in a multibyte UTF-8 character
    constexpr uint8_t max_utf8_trailing               = 0xbf;            // Maximum trailing byte in a multibyte UTF-8 character
    constexpr const char* utf8_bom                    = "\xef\xbb\xbf";  // Byte order mark (U+FEFF) in UTF-8
    constexpr const char* utf8_replacement            = "\xef\xbf\xbd";  // Unicode replacement character (U+FFFD) in UTF-8
    constexpr size_t max_case_decomposition           = 3;               // Maximum length of a full case mapping
    constexpr size_t max_canonical_decomposition      = 2;               // Maximum length of a canonical decomposition
    constexpr size_t max_compatibility_decomposition  = 18;              // Maximum length of a compatibility decomposition

    // Exceptions

    class InitializationError:
    public std::runtime_error {
    public:
        explicit InitializationError(const Ustring& message): std::runtime_error(message) {}
    };

    // Basic character functions

    inline Ustring char_as_hex(char32_t c) { return "U+" + ascii_uppercase(hex(c, 4)); }
    constexpr bool char_is_digit(char32_t c) noexcept { return c >= U'0' && c <= U'9'; }
    constexpr bool char_is_xdigit(char32_t c) noexcept { return (c >= U'0' && c <= U'9') || (c >= U'A' && c <= U'F') || (c >= U'a' && c <= U'f'); }
    constexpr bool char_is_ascii(char32_t c) noexcept { return c <= last_ascii_char; }
    constexpr bool char_is_latin1(char32_t c) noexcept { return c <= last_latin1_char; }
    constexpr bool char_is_surrogate(char32_t c) noexcept { return c >= first_surrogate_char && c <= last_surrogate_char; }
    constexpr bool char_is_bmp(char32_t c) noexcept { return c <= last_bmp_char && ! char_is_surrogate(c); }
    constexpr bool char_is_astral(char32_t c) noexcept { return c > last_bmp_char && c <= last_unicode_char; }
    constexpr bool char_is_unicode(char32_t c) noexcept { return c <= last_unicode_char && ! char_is_surrogate(c); }
    constexpr bool char_is_high_surrogate(char32_t c) noexcept { return c >= first_high_surrogate_char && c <= last_high_surrogate_char; }
    constexpr bool char_is_low_surrogate(char32_t c) noexcept { return c >= first_low_surrogate_char && c <= last_low_surrogate_char; }
    constexpr bool char_is_noncharacter(char32_t c) noexcept { return (c >= first_noncharacter && c <= last_noncharacter) || (c & 0xfffe) == 0xfffe; }
    constexpr bool char_is_private_use(char32_t c) noexcept {
        return (c >= first_private_use_char && c <= last_private_use_char)
            || (c >= first_private_use_a_char && c <= last_private_use_a_char)
            || (c >= first_private_use_b_char && c <= last_private_use_b_char);
    }
    template <typename C> constexpr uint32_t char_to_uint(C c) noexcept { return std::make_unsigned_t<C>(c); }

    template <typename T> constexpr bool is_character_type =
        std::is_same<T, char>::value || std::is_same<T, char16_t>::value
        || std::is_same<T, char32_t>::value || std::is_same<T, wchar_t>::value;

    // General category

    namespace unicornDetail {

        constexpr uint16_t encode_gc(char c1, char c2) noexcept { return uint16_t((uint16_t(uint8_t(c1)) << 8) + uint8_t(c2)); }

    }

    enum class GC: uint16_t {
        Cc = unicornDetail::encode_gc('C','c'),  // Other: Control
        Cf = unicornDetail::encode_gc('C','f'),  // Other: Format
        Cn = unicornDetail::encode_gc('C','n'),  // Other: Unassigned
        Co = unicornDetail::encode_gc('C','o'),  // Other: Private use
        Cs = unicornDetail::encode_gc('C','s'),  // Other: Surrogate
        Ll = unicornDetail::encode_gc('L','l'),  // Letter: Lowercase letter
        Lm = unicornDetail::encode_gc('L','m'),  // Letter: Modifier letter
        Lo = unicornDetail::encode_gc('L','o'),  // Letter: Other letter
        Lt = unicornDetail::encode_gc('L','t'),  // Letter: Titlecase letter
        Lu = unicornDetail::encode_gc('L','u'),  // Letter: Uppercase letter
        Mc = unicornDetail::encode_gc('M','c'),  // Mark: Spacing mark
        Me = unicornDetail::encode_gc('M','e'),  // Mark: Enclosing mark
        Mn = unicornDetail::encode_gc('M','n'),  // Mark: Nonspacing mark
        Nd = unicornDetail::encode_gc('N','d'),  // Number: Decimal number
        Nl = unicornDetail::encode_gc('N','l'),  // Number: Letter number
        No = unicornDetail::encode_gc('N','o'),  // Number: Other number
        Pc = unicornDetail::encode_gc('P','c'),  // Punctuation: Connector punctuation
        Pd = unicornDetail::encode_gc('P','d'),  // Punctuation: Dash punctuation
        Pe = unicornDetail::encode_gc('P','e'),  // Punctuation: Close punctuation
        Pf = unicornDetail::encode_gc('P','f'),  // Punctuation: Final punctuation
        Pi = unicornDetail::encode_gc('P','i'),  // Punctuation: Initial punctuation
        Po = unicornDetail::encode_gc('P','o'),  // Punctuation: Other punctuation
        Ps = unicornDetail::encode_gc('P','s'),  // Punctuation: Open punctuation
        Sc = unicornDetail::encode_gc('S','c'),  // Symbol: Currency symbol
        Sk = unicornDetail::encode_gc('S','k'),  // Symbol: Modifier symbol
        Sm = unicornDetail::encode_gc('S','m'),  // Symbol: Math symbol
        So = unicornDetail::encode_gc('S','o'),  // Symbol: Other symbol
        Zl = unicornDetail::encode_gc('Z','l'),  // Separator: Line separator
        Zp = unicornDetail::encode_gc('Z','p'),  // Separator: Paragraph separator
        Zs = unicornDetail::encode_gc('Z','s'),  // Separator: Space separator
    };

    GC char_general_category(char32_t c) noexcept;
    inline char char_primary_category(char32_t c) noexcept { return char(uint16_t(char_general_category(c)) >> 8); }
    inline bool char_is_alphanumeric(char32_t c) noexcept { auto g = char_primary_category(c); return g == 'L' || g == 'N'; }
    inline bool char_is_alphanumeric_w(char32_t c) noexcept { return char_is_alphanumeric(c) || c == U'_'; }
    inline bool char_is_control(char32_t c) noexcept { return char_general_category(c) == GC::Cc; }
    inline bool char_is_format(char32_t c) noexcept { return char_general_category(c) == GC::Cf; }
    inline bool char_is_letter(char32_t c) noexcept { return char_primary_category(c) == 'L'; }
    inline bool char_is_letter_w(char32_t c) noexcept { return char_is_letter(c) || c == U'_'; }
    inline bool char_is_mark(char32_t c) noexcept { return char_primary_category(c) == 'M'; }
    inline bool char_is_number(char32_t c) noexcept { return char_primary_category(c) == 'N'; }
    inline bool char_is_punctuation(char32_t c) noexcept { return char_primary_category(c) == 'P'; }
    inline bool char_is_punctuation_w(char32_t c) noexcept { return char_is_punctuation(c) && c != U'_'; }
    inline bool char_is_symbol(char32_t c) noexcept { return char_primary_category(c) == 'S'; }
    inline bool char_is_separator(char32_t c) noexcept { return char_primary_category(c) == 'Z'; }

    std::function<bool(char32_t)> gc_predicate(GC cat, bool sense = true);
    std::function<bool(char32_t)> gc_predicate(const Ustring& cat, bool sense = true);
    std::function<bool(char32_t)> gc_predicate(const char* cat, bool sense = true);

    inline Ustring decode_gc(GC cat) { return {char((uint16_t(cat) >> 8) & 0xff), char(uint16_t(cat) & 0xff)}; }
    constexpr GC encode_gc(char c1, char c2) noexcept { return GC(unicornDetail::encode_gc(c1, c2)); }
    constexpr GC encode_gc(const char* cat) noexcept { return cat && *cat ? encode_gc(cat[0], cat[1]) : GC(); }
    inline GC encode_gc(const Ustring& cat) noexcept { return encode_gc(cat.data()); }
    std::vector<GC> gc_list();
    const char* gc_name(GC cat) noexcept;

    inline std::ostream& operator<<(std::ostream& o, GC cat) { return o << decode_gc(cat); }

    // Boolean properties

    inline bool char_is_assigned(char32_t c) noexcept { return char_general_category(c) != GC::Cn; }
    inline bool char_is_unassigned(char32_t c) noexcept { return char_general_category(c) == GC::Cn; }
    bool char_is_white_space(char32_t c) noexcept;
    inline bool char_is_line_break(char32_t c) noexcept {
        return c == U'\n' || c == U'\v' || c == U'\f' || c == U'\r'
            || c == 0x85 || c == line_separator_char || c == paragraph_separator_char;
    }
    inline bool char_is_inline_space(char32_t c) noexcept { return char_is_white_space(c) && ! char_is_line_break(c); }
    bool char_is_id_start(char32_t c) noexcept;
    bool char_is_id_nonstart(char32_t c) noexcept;
    inline bool char_is_id_continue(char32_t c) noexcept { return char_is_id_start(c) || char_is_id_nonstart(c); }
    bool char_is_xid_start(char32_t c) noexcept;
    bool char_is_xid_nonstart(char32_t c) noexcept;
    inline bool char_is_xid_continue(char32_t c) noexcept { return char_is_xid_start(c) || char_is_xid_nonstart(c); }
    bool char_is_pattern_syntax(char32_t c) noexcept;
    bool char_is_pattern_white_space(char32_t c) noexcept;
    bool char_is_default_ignorable(char32_t c) noexcept;
    bool char_is_soft_dotted(char32_t c) noexcept;

    // Bidirectional properties

    Bidi_Class bidi_class(char32_t c) noexcept;
    bool char_is_bidi_mirrored(char32_t c) noexcept;
    char32_t bidi_mirroring_glyph(char32_t c) noexcept;
    char32_t bidi_paired_bracket(char32_t c) noexcept;
    char bidi_paired_bracket_type(char32_t c) noexcept;

    // Block properties

    struct BlockInfo {
        Ustring name;
        char32_t first;
        char32_t last;
    };

    Ustring char_block(char32_t c);
    const std::vector<BlockInfo>& unicode_block_list();

    // Case folding properties

    RS_ENUM_CLASS(Case, uint32_t, 0, none, fold, lower, title, upper);

    Case char_case(char32_t c) noexcept;
    bool char_is_case(char32_t c, Case k) noexcept;
    bool char_is_cased(char32_t c) noexcept;
    bool char_is_case_ignorable(char32_t c) noexcept;
    bool char_is_uppercase(char32_t c) noexcept;
    bool char_is_lowercase(char32_t c) noexcept;
    inline bool char_is_titlecase(char32_t c) noexcept { return char_general_category(c) == GC::Lt; }
    char32_t char_to_simple_uppercase(char32_t c) noexcept;
    char32_t char_to_simple_lowercase(char32_t c) noexcept;
    char32_t char_to_simple_titlecase(char32_t c) noexcept;
    char32_t char_to_simple_casefold(char32_t c) noexcept;
    char32_t char_to_simple_case(char32_t c, Case k) noexcept;
    size_t char_to_full_uppercase(char32_t c, char32_t* dst) noexcept;
    size_t char_to_full_lowercase(char32_t c, char32_t* dst) noexcept;
    size_t char_to_full_titlecase(char32_t c, char32_t* dst) noexcept;
    size_t char_to_full_casefold(char32_t c, char32_t* dst) noexcept;
    size_t char_to_full_case(char32_t c, char32_t* dst, Case k) noexcept;

    // Character names

    struct Cname {

        static constexpr uint32_t control  = setbit<0>;
        static constexpr uint32_t label    = setbit<1>;
        static constexpr uint32_t lower    = setbit<2>;
        static constexpr uint32_t prefix   = setbit<3>;
        static constexpr uint32_t update   = setbit<4>;
        static constexpr uint32_t all      = control | label | lower | prefix | update;

    };

    Ustring char_name(char32_t c, uint32_t flags = 0);

    // Decomposition properties

    int combining_class(char32_t c) noexcept;
    char32_t canonical_composition(char32_t u1, char32_t u2) noexcept;
    size_t canonical_decomposition(char32_t c, char32_t* dst) noexcept;
    size_t compatibility_decomposition(char32_t c, char32_t* dst) noexcept;

    // Enumeration properties

    East_Asian_Width east_asian_width(char32_t c) noexcept;
    Grapheme_Cluster_Break grapheme_cluster_break(char32_t c) noexcept;
    Hangul_Syllable_Type hangul_syllable_type(char32_t c) noexcept;
    Indic_Positional_Category indic_positional_category(char32_t c) noexcept;
    Indic_Syllabic_Category indic_syllabic_category(char32_t c) noexcept;
    Joining_Group joining_group(char32_t c) noexcept;
    Joining_Type joining_type(char32_t c) noexcept;
    Line_Break line_break(char32_t c) noexcept;
    Numeric_Type numeric_type(char32_t c) noexcept;
    Sentence_Break sentence_break(char32_t c) noexcept;
    Word_Break word_break(char32_t c) noexcept;

    // Numeric properties

    std::pair<long long, long long> numeric_value(char32_t c);

    // Script properties

    Ustring char_script(char32_t c);
    Strings char_script_list(char32_t c);
    Ustring script_name(const Ustring& abbr);

}
