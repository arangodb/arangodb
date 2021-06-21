// Internal to the library, do not include this directly
// NOT INSTALLED

#pragma once

#include "unicorn/property-values.hpp"
#include "unicorn/utility.hpp"
#include <algorithm>
#include <array>
#include <iterator>

namespace RS::Unicorn::UnicornDetail {

    // Constants

    constexpr char32_t not_found = 0xffffffff;

    // Types

    template <typename K, typename V>
    struct KeyValue {
        using key_type = K;
        using mapped_type = V;
        K key;
        V value;
        constexpr bool operator==(const KeyValue& rhs) const noexcept { return key == rhs.key; }
        constexpr bool operator<(const KeyValue& rhs) const noexcept { return key < rhs.key; }
    };

    using CharacterFunction = char32_t (*)(char32_t);
    template <typename K, typename V> using TableView = Irange<const KeyValue<K, V>*>;

    template <typename T>
    struct PackedPair {
        using value_type = T;
        T first;
        T second;
    };

    // Lookup functions

    template <typename T, typename K, typename V>
    V table_lookup(Irange<const T*> table, K key, V def) noexcept {
        T t;
        t.key = key;
        auto it = std::lower_bound(table.begin(), table.end(), t);
        if (it != table.end() && it->key == key)
            return static_cast<V>(it->value);
        else
            return def;
    }

    template <size_t N>
    size_t extended_table_lookup(char32_t key, char32_t* dst,
            Irange<const KeyValue<char32_t, std::array<char32_t, N>>*> table,
            CharacterFunction fallback = nullptr) noexcept {
        std::array<char32_t, N> value;
        value[0] = not_found;
        value = table_lookup(table, key, value);
        if (value[0] != not_found) {
            size_t i = 0;
            for (; i < N && value[i]; ++i)
                *dst++ = value[i];
            return i;
        } else if (fallback) {
            *dst = fallback(key);
            return 1;
        } else {
            return 0;
        }
    }

    template <typename K>
    bool sparse_set_lookup(Irange<const KeyValue<K, K>*> table, K key) noexcept {
        if (table.begin() == table.end())
            return false;
        KeyValue<K, K> t;
        t.key = key;
        auto it = std::upper_bound(table.begin(), table.end(), t);
        if (it == table.begin())
            return false;
        --it;
        return key >= it->key && key <= it->value;
    }

    template <typename T, typename K>
    typename T::mapped_type sparse_table_lookup(Irange<const T*> table, K key) noexcept {
        using V = typename T::mapped_type;
        if (table.begin() == table.end())
            return V();
        T t {key, V()};
        auto it = std::upper_bound(table.begin(), table.end(), t);
        if (it != table.begin())
            --it;
        return it->value;
    }

    // General character property tables

    extern const TableView<char32_t, uint16_t> general_category_table;
    extern const TableView<char32_t, char32_t> default_ignorable_table;
    extern const TableView<char32_t, char32_t> soft_dotted_table;
    extern const TableView<char32_t, char32_t> white_space_table;
    extern const TableView<char32_t, char32_t> id_start_table;
    extern const TableView<char32_t, char32_t> id_nonstart_table;
    extern const TableView<char32_t, char32_t> xid_start_table;
    extern const TableView<char32_t, char32_t> xid_nonstart_table;
    extern const TableView<char32_t, char32_t> pattern_syntax_table;
    extern const TableView<char32_t, char32_t> pattern_white_space_table;

    // Arabic shaping property tables

    extern const TableView<char32_t, Joining_Type> joining_type_table;
    extern const TableView<char32_t, Joining_Group> joining_group_table;

    // Bidirectional property tables

    extern const TableView<char32_t, Bidi_Class> bidi_class_table;
    extern const Irange<char32_t const*> bidi_mirrored_table;
    extern const TableView<char32_t, char32_t> bidi_mirroring_glyph_table;
    extern const TableView<char32_t, char32_t> bidi_paired_bracket_table;
    extern const TableView<char32_t, char32_t> bidi_paired_bracket_type_table;

    // Block tables

    extern const TableView<char32_t, char const*> blocks_table;

    // Case folding tables

    extern const TableView<char32_t, char32_t> other_uppercase_table;
    extern const TableView<char32_t, char32_t> other_lowercase_table;
    extern const TableView<char32_t, char32_t> simple_uppercase_table;
    extern const TableView<char32_t, char32_t> simple_lowercase_table;
    extern const TableView<char32_t, char32_t> simple_titlecase_table;
    extern const TableView<char32_t, char32_t> simple_casefold_table;
    extern const TableView<char32_t, std::array<char32_t, 3>> full_uppercase_table;
    extern const TableView<char32_t, std::array<char32_t, 3>> full_lowercase_table;
    extern const TableView<char32_t, std::array<char32_t, 3>> full_titlecase_table;
    extern const TableView<char32_t, std::array<char32_t, 3>> full_casefold_table;

    // Character name tables

    extern const char main_names_data[];
    extern const size_t main_names_compressed;
    extern const size_t main_names_expanded;
    extern const TableView<char32_t, char const*> corrected_names_table;

    // Decomposition tables

    extern const TableView<char32_t, int> combining_class_table;
    extern const TableView<char32_t, std::array<char32_t, 2>> canonical_table;
    extern const TableView<char32_t, std::array<char32_t, 3>> short_compatibility_table;
    extern const TableView<char32_t, std::array<char32_t, 18>> long_compatibility_table;
    extern const TableView<std::array<char32_t, 2>, char32_t> composition_table;

    // Indic property tables

    extern const TableView<char32_t, Indic_Positional_Category> indic_positional_category_table;
    extern const TableView<char32_t, Indic_Syllabic_Category> indic_syllabic_category_table;

    // Numeric property tables

    extern const TableView<char32_t, Numeric_Type> numeric_type_table;
    extern const TableView<char32_t, PackedPair<long long>> numeric_value_table;

    // Script tables

    extern const TableView<char32_t, uint32_t> scripts_table;
    extern const TableView<char32_t, char const*> script_extensions_table;

    // Text segmentation property tables

    extern const TableView<char32_t, Grapheme_Cluster_Break> grapheme_cluster_break_table;
    extern const TableView<char32_t, Line_Break> line_break_table;
    extern const TableView<char32_t, Sentence_Break> sentence_break_table;
    extern const TableView<char32_t, Word_Break> word_break_table;

    // Other enumerated property tables

    extern const TableView<char32_t, East_Asian_Width> east_asian_width_table;
    extern const TableView<char32_t, Hangul_Syllable_Type> hangul_syllable_type_table;

    // Normalization test tables

    extern const Irange<const std::array<char const*, 5>*> normalization_test_table;
    extern const TableView<char32_t, char32_t> normalization_identity_table;

    // Segmentation test tables

    extern const Irange<char const* const*> grapheme_break_test_table;
    extern const Irange<char const* const*> word_break_test_table;
    extern const Irange<char const* const*> sentence_break_test_table;

}
