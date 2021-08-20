#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (C) 2020 T. Zachary Laine
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from generate_unicode_normalization_data import cccs
from generate_unicode_normalization_data import expand_decomp_canonical
from generate_unicode_normalization_data import get_decompositions
import lzw


# TODO: These are from the latest FractionalUCA.txt.  When updaing it, these
# need to be updated too:

constants_header_form = '''\
// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Warning! This file is autogenerated.
#ifndef BOOST_TEXT_DETAIL_COLLATION_CONSTANTS_HPP
#define BOOST_TEXT_DETAIL_COLLATION_CONSTANTS_HPP

#include <boost/text/config.hpp>

#include <array>

#include <cstdint>


namespace boost {{ namespace text {{ namespace detail {{

    enum collation_constants : uint32_t {{
        min_variable_collation_weight = 0x03040000,
        max_variable_collation_weight = 0x0b785900,

        OR_CJK_Compatibility_Ideographs = {},
        OR_CJK_Unified_Ideographs_Extension_D = {},

        // Symbolic sentinel values produced by the parser.
        first_tertiary_ignorable = 0xfffffff2,
        last_tertiary_ignorable = 0xfffffff3,
        first_secondary_ignorable = 0xfffffff4,
        last_secondary_ignorable = 0xfffffff5,
        first_primary_ignorable = 0xfffffff6,
        last_primary_ignorable = 0xfffffff7,
        first_variable = 0xfffffff8,
        last_variable = 0xfffffff9,
        first_regular = 0xfffffffa,
        last_regular = 0xfffffffb,
        first_implicit = 0xfffffffc,
        first_trailing = 0xfffffffd,

        invalid_code_point = 0xffffffff,

        initial_first_tertiary_ignorable = 0,
        initial_last_tertiary_ignorable = 0xfffb,
        initial_first_secondary_ignorable = invalid_code_point,
        initial_last_secondary_ignorable = invalid_code_point,
        initial_first_primary_ignorable = 0x0332,
        initial_last_primary_ignorable = 0x00b7,
        initial_first_variable = 0x0009,
        initial_last_variable = 0x10A7F,
        initial_first_regular = 0x0060,
        initial_last_regular = 0x1B2FB,
        initial_first_implicit = 0x3400,
        initial_first_trailing = 0xfffd,

        common_l2_weight_compressed = 0x0500,
        common_l3_weight_compressed = 0x0500,

        first_tertiary_in_secondary_masked = 0x03,
        last_tertiary_in_secondary_masked = 0x38,
        first_secondary_in_primary = 0x0500,
        last_secondary_in_primary = 0x7c00,

        min_secondary_byte = 0x02,
        min_tertiary_byte = 0x03,
        common_secondary_byte = 0x05,
        common_tertiary_byte = 0x05,
        max_secondary_byte = 0xfe,
        max_tertiary_byte = 0xb8,

        implicit_weights_spacing_times_ten = {},
        implicit_weights_first_lead_byte = {},
        implicit_weights_final_lead_byte = {}
    }};

    enum itty_bitty_collation_constants : uint16_t {{
        case_level_bits_mask = 0xc000u,
        disable_case_level_mask = uint16_t(~0xc000u)
    }};

    struct implicit_weights_segment
    {{
        uint32_t first_;
        uint32_t last_;
        uint32_t primary_offset_;
    }};

    BOOST_TEXT_DECL std::array<implicit_weights_segment, 10>
    make_implicit_weights_segments();

}}}}}}

#endif
'''

collation_data_0_file_form = '''\
// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Warning! This file is autogenerated.
#include <boost/text/detail/collation_constants.hpp>
#include <boost/text/detail/collation_data.hpp>

#include <boost/assert.hpp>

#include <unordered_map>


namespace boost {{ namespace text {{ namespace detail {{

std::array<implicit_weights_segment, {1}> make_implicit_weights_segments()
{{
constexpr std::array<implicit_weights_segment, {1}> retval = {{{{
{0}
}}}};
return retval;
}}

std::array<reorder_group, {3}> const & make_reorder_groups()
{{
constexpr static std::array<reorder_group, {3}> retval = {{{{
{2}
}}}};
return retval;
}}

namespace {{
#ifdef _MSC_VER
std::vector<uint16_t>
#else
constexpr std::array<uint16_t, {5}>
#endif
compressed_collation_elements()
{{
#ifdef _MSC_VER
    std::vector<uint16_t> retval({5});
    auto it = retval.begin();
#else
    return {{{{
#endif
{4}
#ifdef _MSC_VER
    return retval;
#else
    }}}};
#endif
}}
}}

void make_collation_elements(std::array<collation_element, {6}> & retval)
{{
container::small_vector<unsigned char, 256> buf;
auto const & compressed = compressed_collation_elements();
lzw_decompress(
    compressed.begin(),
    compressed.end(),
    make_lzw_to_coll_elem_iter(retval.begin(), buf));
BOOST_ASSERT(buf.empty());
}}

}}}}}}
'''

trie_file_form = '''\
// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Warning! This file is autogenerated.
#include <boost/text/detail/collation_data.hpp>

#include <boost/assert.hpp>


namespace boost {{ namespace text {{ namespace detail {{

namespace {{

#ifdef _MSC_VER
std::vector<uint16_t>
#else
constexpr std::array<uint16_t, {1}>
#endif
compressed_trie_keys()
{{
#ifdef _MSC_VER
    std::vector<uint16_t> retval({1});
    auto it = retval.begin();
#else
    return {{{{
#endif
{0}
#ifdef _MSC_VER
    return retval;
#else
    }}}};
#endif
}}

#ifdef _MSC_VER
std::vector<collation_elements>
#else
constexpr std::array<collation_elements, {3}>
#endif
trie_values_()
{{
#ifdef _MSC_VER
    std::vector<collation_elements> retval({3});
    auto it = retval.begin();
#else
    return {{{{
#endif
{2}
#ifdef _MSC_VER
    return retval;
#else
    }}}};
#endif
}}

}}

    void make_trie_keys(std::array<collation_trie_key<3>, {3}> & retval)
    {{
        auto const & compressed = compressed_trie_keys();
        container::small_vector<unsigned char, 256> buf;
        lzw_decompress(
            compressed.begin(),
            compressed.end(),
            make_lzw_to_trie_key_iter(retval.begin(), buf));
        BOOST_ASSERT(buf.empty());
    }}

    void make_trie_values(std::array<collation_elements, {3}> & retval)
    {{
        auto const & values = trie_values_();
        std::copy(values.begin(), values.end(), retval.begin());
    }}

}}}}}}
'''

def ccc(cccs_dict, cp):
    if cp in cccs_dict:
        return cccs_dict[cp]
    return 0

CJK_Compatibility_Ideographs = [
    0xFA0E, 0xFA0F, 0xFA11, 0xFA13, 0xFA14, 0xFA1F, 0xFA21, 0xFA23,
    0xFA24, 0xFA27, 0xFA28, 0xFA29
]
OR_CJK_Compatibility_Ideographs = \
  reduce(lambda x,y: x | y, CJK_Compatibility_Ideographs, 0)

CJK_Unified_Ideographs_Extension_D = [
    0x2B740, 0x2B741, 0x2B742, 0x2B743, 0x2B744, 0x2B745, 0x2B746, 0x2B747,
    0x2B748, 0x2B749, 0x2B74A, 0x2B74B, 0x2B74C, 0x2B74D, 0x2B74E, 0x2B74F,
    0x2B750, 0x2B751, 0x2B752, 0x2B753, 0x2B754, 0x2B755, 0x2B756, 0x2B757,
    0x2B758, 0x2B759, 0x2B75A, 0x2B75B, 0x2B75C, 0x2B75D, 0x2B75E, 0x2B75F,
    0x2B760, 0x2B761, 0x2B762, 0x2B763, 0x2B764, 0x2B765, 0x2B766, 0x2B767,
    0x2B768, 0x2B769, 0x2B76A, 0x2B76B, 0x2B76C, 0x2B76D, 0x2B76E, 0x2B76F,
    0x2B770, 0x2B771, 0x2B772, 0x2B773, 0x2B774, 0x2B775, 0x2B776, 0x2B777,
    0x2B778, 0x2B779, 0x2B77A, 0x2B77B, 0x2B77C, 0x2B77D, 0x2B77E, 0x2B77F,
    0x2B780, 0x2B781, 0x2B782, 0x2B783, 0x2B784, 0x2B785, 0x2B786, 0x2B787,
    0x2B788, 0x2B789, 0x2B78A, 0x2B78B, 0x2B78C, 0x2B78D, 0x2B78E, 0x2B78F,
    0x2B790, 0x2B791, 0x2B792, 0x2B793, 0x2B794, 0x2B795, 0x2B796, 0x2B797,
    0x2B798, 0x2B799, 0x2B79A, 0x2B79B, 0x2B79C, 0x2B79D, 0x2B79E, 0x2B79F,
    0x2B7A0, 0x2B7A1, 0x2B7A2, 0x2B7A3, 0x2B7A4, 0x2B7A5, 0x2B7A6, 0x2B7A7,
    0x2B7A8, 0x2B7A9, 0x2B7AA, 0x2B7AB, 0x2B7AC, 0x2B7AD, 0x2B7AE, 0x2B7AF,
    0x2B7B0, 0x2B7B1, 0x2B7B2, 0x2B7B3, 0x2B7B4, 0x2B7B5, 0x2B7B6, 0x2B7B7,
    0x2B7B8, 0x2B7B9, 0x2B7BA, 0x2B7BB, 0x2B7BC, 0x2B7BD, 0x2B7BE, 0x2B7BF,
    0x2B7C0, 0x2B7C1, 0x2B7C2, 0x2B7C3, 0x2B7C4, 0x2B7C5, 0x2B7C6, 0x2B7C7,
    0x2B7C8, 0x2B7C9, 0x2B7CA, 0x2B7CB, 0x2B7CC, 0x2B7CD, 0x2B7CE, 0x2B7CF,
    0x2B7D0, 0x2B7D1, 0x2B7D2, 0x2B7D3, 0x2B7D4, 0x2B7D5, 0x2B7D6, 0x2B7D7,
    0x2B7D8, 0x2B7D9, 0x2B7DA, 0x2B7DB, 0x2B7DC, 0x2B7DD, 0x2B7DE, 0x2B7DF,
    0x2B7E0, 0x2B7E1, 0x2B7E2, 0x2B7E3, 0x2B7E4, 0x2B7E5, 0x2B7E6, 0x2B7E7,
    0x2B7E8, 0x2B7E9, 0x2B7EA, 0x2B7EB, 0x2B7EC, 0x2B7ED, 0x2B7EE, 0x2B7EF,
    0x2B7F0, 0x2B7F1, 0x2B7F2, 0x2B7F3, 0x2B7F4, 0x2B7F5, 0x2B7F6, 0x2B7F7,
    0x2B7F8, 0x2B7F9, 0x2B7FA, 0x2B7FB, 0x2B7FC, 0x2B7FD, 0x2B7FE, 0x2B7FF,
    0x2B800, 0x2B801, 0x2B802, 0x2B803, 0x2B804, 0x2B805, 0x2B806, 0x2B807,
    0x2B808, 0x2B809, 0x2B80A, 0x2B80B, 0x2B80C, 0x2B80D, 0x2B80E, 0x2B80F,
    0x2B810, 0x2B811, 0x2B812, 0x2B813, 0x2B814, 0x2B815, 0x2B816, 0x2B817,
    0x2B818, 0x2B819, 0x2B81A, 0x2B81B, 0x2B81C, 0x2B81D
]

OR_CJK_Unified_Ideographs_Extension_D = \
  reduce(lambda x,y: x | y, CJK_Unified_Ideographs_Extension_D, 0)


# Note: These are filled in far below.
implicit_weights_first_lead_byte = 0xE0
implicit_weights_spacing = 2.6
implicit_weights_segments = []
implicit_weights_final_lead_byte = 0xE4


# http://www.unicode.org/reports/tr10/#Implicit_Weights
def implicit_ce(cp):
    def to_byte(x):
        retval = hex(x)[2:]
        if len(retval) == 1:
            retval = '0' + retval
        return retval.upper()

    for s in implicit_weights_segments:
        if s[0][0] <= cp and cp < s[0][1]:
            primary_suffix = s[1] + int((cp - s[0][0]) * implicit_weights_spacing)

            return (
                (
                    to_byte(implicit_weights_first_lead_byte),
                    to_byte(((primary_suffix >> 12) & 0xfe) | 0x1), # top 7 bits (|1 -- no zeros)
                    to_byte(((primary_suffix >> 5) & 0xfe) | 0x1), # middle 7 bits (|1 -- no zeros)
                    to_byte((primary_suffix >> 0) & 0x3f) # last 6 bits
                ),
                ('05',),
                ('05',)
            )

    return (
        (
            to_byte((implicit_weights_final_lead_byte << 24) | (cp & 0xffffff))
        ),
        ('05',),
        ('05',)
    )

def ce(cet, cps):
    if cps in cet:
        return cet[cps][0]
    if len(cps) != 1:
        return None
    return implicit_ce(cps[0])

def make_orignal_order(cet):
    original_order = dict(map(lambda x: (x[0], x[1][1]), sorted(cet.items())))
    for k in cet:
        cet[k] = cet[k][0]
    return (cet, original_order)

def make_unique_collation_element_sequence(cet):
    new_cet = {}
    collation_elements = []

    already_processed = {}
    for k,v in cet.items():
        if v not in already_processed:
            first = len(collation_elements)
            collation_elements += v
            last = len(collation_elements)
            already_processed[v] = (first, last)
        new_cet[k] = already_processed[v]

    return (new_cet, collation_elements)

def ce_to_cpp(ce):
    retval = '{0x'
    if ce[0] == ('',):
        retval += '00000000'
    else:
        retval += ''.join(ce[0])
        retval += '0' * (4 - len(ce[0])) * 2
    retval += ', 0x'
    if ce[1] == ('',):
        retval += '0000'
    else:
        retval += ''.join(ce[1])
        retval += '0' * (2 - len(ce[1])) * 2
    retval += ', 0x'
    if ce[2] == ('',):
        retval += '0000'
    else:
        retval += ''.join(ce[2])
        retval += '0' * (2 - len(ce[2])) * 2
    retval += '}'
    return retval

def indices_to_list(indices, all_cps):
    return all_cps[indices[0]:indices[1]]

def group_boundary_line(line):
    return line.startswith('FDD1') and not 'FDD0' in line

def group_boundary_name(line):
    first_idx = line.find(' first')
    return line[:first_idx].split('#')[1].strip()

import re
ce_regex = re.compile(r'\[([^,\]]*)(?:,([^,\]]*)(?:,([^\]]*))?)?\]')
cet_key_regex = re.compile(r'([0-9A-f]+)')

def to_ce(groups, cet):
    def bytes_to_tuple(s):
        return tuple(s.strip().split(' '))
    ce = tuple()
    if groups[0].startswith('U+'):
        if groups[0][2:] in cet:
            ce = cet[groups[0][2:]][0]
        else:
            ce = implicit_ce(int(groups[0][2:], 16))
        if groups[1] != None:
            if groups[2] != None:
                ce = (ce[0], bytes_to_tuple(groups[1]), bytes_to_tuple(groups[2]))
            else:
                ce = (ce[0], ce[1], bytes_to_tuple(groups[1]))
    else:
        ce = tuple(map(lambda x: x == None and ('00',) or bytes_to_tuple(x), groups))

    return ce

class extrema:
    min_secondary = 1000
    min_tertiary = 1000
    max_secondary = 0
    max_tertiary = 0

def get_frac_uca_cet(filename, compressible_lead_bytes = None):
    lines = open(filename, 'r').readlines()

    cet = {}

    # Those a | b keys go here so they can be processed after the "a" part has
    # been seen.
    deferred = {}

    def get_mins(ces):
        for ce in ces:
            for x in ce[1]:
                if x == '':
                    continue
                secondary = int(x, 16)
                extrema.min_secondary = min(secondary, extrema.min_secondary)
                extrema.max_secondary = max(secondary, extrema.max_secondary)
            for x in ce[2]:
                if x == '':
                    continue
                tertiary = int(x, 16)
                extrema.min_tertiary = min(tertiary, extrema.min_tertiary)
                extrema.max_tertiary = max(tertiary, extrema.max_tertiary)

    original_cet_position = 0
    in_top_bytes = False
    after_top_bytes = False
    before_homeless_ces = True
    fffe_placed = False
    for line in lines:
        line = line[:-1]
        if '[top_byte' in line:
            if 'COMPRESS' in line and compressible_lead_bytes != None:
                lead_byte = line.split('\t')[1]
                compressible_lead_bytes.append(lead_byte)
            in_top_bytes = True
        if in_top_bytes and '[top_byte' not in line:
            after_top_bytes = True
        if 'HOMELESS COLLATION ELEMENTS' in line:
            before_homeless_ces = False
        if not line.startswith('#') and not line.startswith('@') and \
           not line.startswith('FDD0 ') and not line.startswith('FDD1 ') \
           and len(line) != 0:
            if after_top_bytes and before_homeless_ces:
                comment_start = line.find('#')
                comment = ''
                if comment_start != -1:
                    comment = line[comment_start + 1:].strip()
                    line = line[:comment_start]
                before_semicolon = line.split(';')[0]
                key = tuple(map(
                    lambda x: int(x.group(1), 16),
                    cet_key_regex.finditer(before_semicolon)
                ))
                ces = []
                for match in ce_regex.finditer(line):
                    ces.append(to_ce(match.groups(), cet))
                ces = tuple(ces)
                if '|' in before_semicolon:
                    deferred[key] = (ces, original_cet_position)
                if not fffe_placed and ces[0][0][0] == '03':
                    ce = (('02',), ('05',), ('05',))
                    cet[(0xfffe,)] = ((ce,), original_cet_position)
                    original_cet_position += 1
                    fffe_placed = True
                if ces[0][0][0] == '02':
                    continue
                get_mins(ces)
                cet[key] = (ces, original_cet_position)
                original_cet_position += 1

    for k,v in deferred.items():
        cet[k] = (cet[(k[0],)][0] + v[0], v[1])

    # TODO: Automate putting these into collation_constants.hpp.
    #print 'min_secondary=',hex(extrema.min_secondary)
    #print 'min_tertiary=',hex(extrema.min_tertiary)
    #print 'max_secondary=',hex(extrema.max_secondary)
    #print 'max_tertiary=',hex(extrema.max_tertiary)

    return cet

def get_group_boundary_to_token_mapping(filename):
    lines = open(filename, 'r').readlines()

    top_byte_tokens = {}
    token_names = []
    group_boundary_names = []
    group_boundary_ces = []
    group_boundary_top_bytes = {}
    implicit_bytes = []

    for line in lines:
        if line.startswith('[top_byte'):
            fields = line.split('\t')
            tokens = fields[2].split(' ')
            for i in range(len(tokens)):
                if ']' in tokens[i] or 'COMPRESS' in tokens[i]:
                    tokens = tokens[:i]
                    break
            top_byte_tokens[fields[1]] = tokens
            if tokens == ['IMPLICIT']:
                implicit_bytes.append(int(fields[1], 16))
            for tok in tokens:
                token_names.append(tok)
        if group_boundary_line(line):
            if 'KATAKANA' in line: # Covered by HIRAGANA.
                continue
            if 'Meroitic_Cursive' in line: # Covered by Meroitic_Hieroglyphs
                continue
            name = group_boundary_name(line)
            group_boundary_names.append(name)
            ce = to_ce(ce_regex.search(line).groups(), {})
            if len(group_boundary_ces) and len(group_boundary_ces[-1]) < 2:
                group_boundary_ces[-1].append(ce)
            group_boundary_ces.append([ce])
            group_boundary_top_bytes[name] = line.split('[')[1][:2]
        elif 'FDD0' in line and 'REORDER_RESERVED_BEFORE_LATIN' in line:
            ce = to_ce(ce_regex.search(line).groups(), {})
            group_boundary_ces[-1].append(ce)
        elif 'FDD0' in line and 'REORDER_RESERVED_AFTER_LATIN' in line:
            ce = to_ce(ce_regex.search(line).groups(), {})
            group_boundary_ces[-1].append(ce)

    # Close out Hani to include the implicit weight leading bytes.
    group_boundary_ces[-1].append(
        ((hex(implicit_weights_final_lead_byte + 1)[2:], '00', '00', '00'), ('05',), ('05',))
    )

    # TODO: Unfortunately, this must be updated by hand as new languages are
    # added.
    name_map = {
        'SPACE': 'space',
        'PUNCTUATION': 'punct',
        'SYMBOL': 'symbol',
        'CURRENCY': 'currency',
        'DIGIT': 'digit',
        'Miao': 'Plrd',
        'HAN': 'Hani',
        'Khudawadi': 'Sind',
        'Phoenician': 'Phnx',
        'Tai Tham': 'Lana',
        'Old Turkic': 'Orkh',
        'SHAVIAN': 'Shaw',
        'OLD_PERSIAN': 'Xpeo',
        'Cuneiform': 'Xsux',
        'Egyptian Hieroglyphs': 'Egyp',
        'Psalter_Pahlavi': 'Phlp',
        'Old South Arabian': 'Sarb',
        'Old_North_Arabian': 'Narb',
        'LINEAR_B': 'Linb',
        'Sora_Sompeng': 'Sora',
        'Imperial Aramaic': 'Armi',
        'Inscriptional Parthian': 'Prti',
        'Inscriptional Pahlavi': 'Phli',
        'Anatolian_Hieroglyphs': 'Hluw',
        'TIFINAGH': 'Tfng',
        'Lydian': 'Lydi',
        'Lycian': 'Lyci',
        'Balinese': 'Bali',
        'MONGOLIAN': 'Mong',
        'Kaithi': 'Kthi',
        'Caucasian_Albanian': 'Aghb',
        'CANADIAN-ABORIGINAL': 'Cans',
        'Tai Tham': 'Lana',
        'Khudawadi': 'Sind',
        'Masaram_Gondi': 'Gonm',
        'KHAROSHTHI': 'Khar',
        'NEW_TAI_LUE': 'Talu',
        'Old_Hungarian': 'Hung',
        'Hanifi_Rohingya': 'Rohg',
        'Medefaidrin': 'Medf',
        'Meroitic_Hieroglyphs': 'Merc'
    }

    for boundary in group_boundary_names:
        p = False
        top_byte = boundary in group_boundary_top_bytes and group_boundary_top_bytes[boundary] or 'NA'
        if p:
            print boundary,top_byte,top_byte_tokens[top_byte]
        all_hits = []
        for i in range(len(token_names)):
            hits = 0
            if token_names[i] in (top_byte in top_byte_tokens and top_byte_tokens[top_byte] or []):
                a = boundary.lower()
                b = token_names[i].lower()
                for c in b:
                    if c in a:
                        hits += 1
                if p:
                    print hits,a,b
            all_hits.append(hits)
        max_ = 0
        max_index = 0
        for i in range(len(all_hits)):
            if max_ < all_hits[i]:
                max_ = all_hits[i]
                max_index = i
        if max_ == 4 and boundary not in name_map:
            name_map[boundary] = token_names[max_index]

    first_lead_byte = 1000
    last_lead_byte = 0
    all_lead_bytes = set()
    groups = {}
    for k,v in name_map.items():
        i = group_boundary_names.index(k)
        groups[v] = group_boundary_ces[i]
        first = int(group_boundary_ces[i][0][0][0], 16)
        last = int(group_boundary_ces[i][1][0][0], 16)
        for j in range(first, last):
            all_lead_bytes.add(j)
        first_lead_byte = min(first_lead_byte, first)
        last_lead_byte = max(last_lead_byte, last)

    free_lead_bytes = []
    for i in range(first_lead_byte, last_lead_byte):
        if i not in all_lead_bytes:
            free_lead_bytes.append(i)
    if free_lead_bytes != [0x27, 0x28, 0x5e, 0x5f]:
        raise Exception('It looks like the free lead bytes list needs updating')

    return (name_map, implicit_bytes, sorted(groups.items(), key=lambda x: x[1]))

if __name__ == "__main__":
    import sys

    (boundary_to_token, implicit_bytes, reorder_groups) = \
      get_group_boundary_to_token_mapping('FractionalUCA.txt')

    if implicit_bytes[-1] - implicit_bytes[0] != 4:
        raise Exception('Oops!  Broken assumption about the FractionalUCA.txt table data.')
    if implicit_bytes[0] & 0xfc != implicit_bytes[0]:
        raise Exception('Oops!  Broken assumption about the FractionalUCA.txt table data.')

    # There are 98020 code points explicitly handled in the implicit weights
    # calculations (counting ranges like CJK_Unified_Ideographs_Extension_D as
    # blocks, not individual elements), which fits in 16.58 bits.  With a
    # spacng of 8, it all fits within 20 bits.
    implicit_weights_spacing = 8

    implicit_weights_first_lead_byte = implicit_bytes[0]
    implicit_weights_final_lead_byte = implicit_bytes[-1]

    implicit_weights_cp_ranges = [
        (0x17000, 0x18AFF + 1), # Tangut and Tangut Components
        (0x1B170, 0x1B2FF + 1), # Nushu
        (0x4E00, 0x9FEA + 1),   # Core Han Unified Ideographs
        (0xFA0E, 0xFA29 + 1),   # CJK_Compatibility_Ideographs
        (0x3400, 0x4DB5 + 1),   # All other Han Unified Ideographs
        (0x20000, 0x2A6D6 + 1),
        (0x2A700, 0x2B734 + 1),
        (0x2B740, 0x2B81D + 1), # CJK_Unified_Ideographs_Extension_D
        (0x2B820, 0x2CEA1 + 1),
        (0x2CEB0, 0x2EBE0 + 1)
    ]

    current_offset = 0
    implicit_weights_segments = []
    for r in implicit_weights_cp_ranges:
        implicit_weights_segments.append((r, current_offset))
        current_offset = int(current_offset + (r[1] - r[0]) * implicit_weights_spacing)

    compressible_lead_bytes = []
    cet = get_frac_uca_cet('FractionalUCA.txt', compressible_lead_bytes)

    cccs_dict = cccs('DerivedCombiningClass.txt')
    (all_cps, decomposition_mapping) = \
      get_decompositions('UnicodeData.txt', cccs_dict, expand_decomp_canonical, True)
    decomposition_mapping = filter(lambda x: x[1][1], decomposition_mapping)
    decomposition_mapping = \
      map(lambda x: (x[0], indices_to_list(x[1], all_cps)), decomposition_mapping)
    decomposition_mapping = dict(decomposition_mapping)

    fcc_cet = cet

    (fcc_cet, original_order) = make_orignal_order(fcc_cet)

    (fcc_cet, collation_elements) = make_unique_collation_element_sequence(fcc_cet)

    hpp_file = open('collation_constants.hpp', 'w')
    hpp_file.write(constants_header_form.format(
        hex(OR_CJK_Compatibility_Ideographs),
        hex(OR_CJK_Unified_Ideographs_Extension_D),
        int(implicit_weights_spacing * 10),
        hex(implicit_weights_first_lead_byte),
        hex(implicit_weights_final_lead_byte),
        len(implicit_weights_segments)
    ))


    weights_strings = map(lambda x: '{{{}, {}, {}}}'.format(hex(x[0][0]), hex(x[0][1]), x[1]), implicit_weights_segments)
    implicit_weights_segments_str = '    ' + ',\n    '.join(weights_strings) + ',\n'
    def to_reorder_group(k, v, simple, compress):
        simple = simple and 'true' or 'false'
        compress = compress and 'true' or 'false'
        return '{{ "{}", {}, {}, {}, {} }}'.format(
            k, ce_to_cpp(v[0]), ce_to_cpp(v[1]), simple, compress
        )
    reorder_group_strings = []
    for i in range(len(reorder_groups)):
        g = reorder_groups[i]
        simple = True
        lead_byte = g[1][0][0][0]
        if i != 0 and reorder_groups[i - 1][1][0][0][0] == lead_byte:
            simple = False
        elif i != len(reorder_groups) - 1 and reorder_groups[i + 1][1][0][0][0] == lead_byte:
            simple = False
        reorder_group_strings.append(
            to_reorder_group(g[0], g[1], simple, lead_byte in compressible_lead_bytes)
        )
    reorder_group_str = '    ' + ',\n    '.join(reorder_group_strings) + ',\n'

    values_per_line = 12

    ce_bytes = []
    for ce in collation_elements:
        x = '0' + ''.join(ce[0])
        if ce[0] != ('',):
            x += '0' * (4 - len(ce[0])) * 2
        lzw.add_int(ce_bytes, int(x, 16))
        x = '0' + ''.join(ce[1])
        if ce[1] != ('',):
            x += '0' * (2 - len(ce[1])) * 2
        lzw.add_short(ce_bytes, int(x, 16))
        x = '0' + ''.join(ce[2])
        if ce[2] != ('',):
            x += '0' * (2 - len(ce[2])) * 2
        lzw.add_short(ce_bytes, int(x, 16))
    compressed_ces = lzw.compress(ce_bytes)

    def values_to_lines(values, value_type, values_per_chunk):
        retval = ''
        chunk_form = '''\
#ifdef _MSC_VER
{{
    std::array<{0}, {1}> values {{{{
#endif
    {2}
#ifdef _MSC_VER
    }}}};
    it = std::copy(values.begin(), values.end(), it);
}}
#endif
'''
        i = 0
        while i < len(values):
            next_i = i + values_per_chunk
            if len(values) < next_i:
                break
            retval += chunk_form.format(
                value_type,
                values_per_chunk,
                ''.join(map(lambda x: x + ', ', values[i:next_i]))
            )
            i = next_i
        if i != len(values):
            retval += chunk_form.format(
                value_type,
                len(values) - i,
                ''.join(map(lambda x: x + ', ', values[i:]))
            )
        return retval

    #print 'rewrote {} * 96 = {} bits as {} * 8 = {} bits'.format(len(collation_elements), len(collation_elements)*96, len(ce_bytes), len(ce_bytes)*8)
    #print 'compressed to {} * 16 = {} bits'.format(len(compressed_ces), len(compressed_ces) * 16)
    ce_lines = values_to_lines(map(lambda x: hex(x), compressed_ces), 'uint16_t', 2500)

    cpp_file = open('collation_data_0.cpp', 'w')
    cpp_file.write(collation_data_0_file_form.format(implicit_weights_segments_str, len(implicit_weights_segments), reorder_group_str, len(reorder_group_strings), ce_lines, len(compressed_ces), len(collation_elements)))

    key_bytes = []
    #value_bytes = []
    value_strings = []
    for k,v in sorted(fcc_cet.items(), key=lambda x: original_order[x[0]]):
        lzw.add_byte(key_bytes, len(k))
        for x in k:
            lzw.add_cp(key_bytes, x)
        value_strings.append('{{{}, {}}}'.format(v[0], v[1]))
        #lzw.add_short(value_bytes, v[0])
        #lzw.add_short(value_bytes, v[1])
    compressed_keys = lzw.compress(key_bytes)

    # The other data sets are optimizaed by LZW compression.  This one is
    # heavily pessimized.
    # compressed_values = lzw.compress(value_bytes)

    #print 'rewrote {} * 128 = {} bits as {} * 8 = {} bits'.format(len(fcc_cet), len(fcc_cet)*128, len(key_bytes), len(key_bytes)*8)
    #print 'compressed to {} * 16 = {} bits'.format(len(compressed_keys), len(compressed_keys) * 16)
    key_lines = values_to_lines(map(lambda x: hex(x), compressed_keys), 'uint16_t', 2500)
    #print 'rewrote {} * 32 = {} bits as {} * 8 = {} bits'.format(len(fcc_cet), len(fcc_cet)*32, len(value_bytes), len(value_bytes)*8)
    #print 'compressed to {} * 16 = {} bits'.format(len(compressed_values), len(compressed_values) * 16)
    value_lines = values_to_lines(value_strings, 'collation_elements', 1000)

    cpp_file = open('collation_data_1.cpp', 'w')
    cpp_file.write(trie_file_form.format(key_lines, len(compressed_keys), value_lines, len(fcc_cet)))

    nonstarters = set()
    for k in cet.keys():
        if 1 < len(k):
            for cp in k[1:]:
                nonstarters.add(cp)
    #print 'min={} max={} len={}'.format(min(nonstarters), max(nonstarters), len(nonstarters))
    #print sorted(nonstarters)

    min_nonstarter = min(nonstarters)
    max_nonstarter = max(nonstarters)
    gamut = max_nonstarter - min_nonstarter + 1
    if gamut % 8:
        gamut += 8 - gamut % 8
    chars = [0] * (gamut / 8)
    for cp in nonstarters:
        #print 'min=',min_nonstarter,'cp=',cp,'gamut=',gamut,'cp-min_nonstarter=',(cp - min_nonstarter)
        table_element = cp - min_nonstarter
        chars[table_element >> 3] |= 1 << (table_element & 0x7)
    chars_str = ''
    for c in chars:
        chars_str += hex(c) + ', '

    cpp_file = open('collation_data_2.cpp', 'w')
    cpp_file.write('''// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Warning! This file is autogenerated.
#include <boost/text/detail/collation_constants.hpp>
#include <boost/text/detail/collation_data.hpp>

#include <boost/assert.hpp>


namespace boost {{ namespace text {{ namespace detail {{

constexpr uint32_t default_table_min_nonstarter_ = {0};
constexpr uint32_t default_table_max_nonstarter_ = {1};

constexpr std::array<unsigned char, {2}> default_table_nonstarters = {{{{
{3}
}}}};

uint32_t default_table_min_nonstarter() noexcept
{{ return default_table_min_nonstarter_; }}

uint32_t default_table_max_nonstarter() noexcept
{{ return default_table_max_nonstarter_; }}

unsigned char const * default_table_nonstarters_ptr() noexcept
{{ return default_table_nonstarters.data(); }}

}}}}}}
'''.format(min_nonstarter, max_nonstarter, gamut / 8, chars_str))
