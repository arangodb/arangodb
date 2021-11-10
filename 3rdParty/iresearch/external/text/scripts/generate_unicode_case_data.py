#!/usr/bin/env python

# Copyright (C) 2020 T. Zachary Laine
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import lzw

constants_header_form = '''\
// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Warning! This file is autogenerated.
#ifndef BOOST_TEXT_DETAIL_CASE_CONSTANTS_HPP
#define BOOST_TEXT_DETAIL_CASE_CONSTANTS_HPP

#include <array>

#include <cstdint>


namespace boost {{ namespace text {{ namespace detail {{

    enum class case_condition : uint16_t {{
{0}
    }};

}}}}}}

#endif
'''

case_impl_file_form = '''\
// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Warning! This file is autogenerated.
#include <boost/text/trie_map.hpp>

#include <boost/text/detail/case_mapping_data.hpp>


namespace boost {{ namespace text {{ namespace detail {{

    std::array<uint32_t, {1}> make_case_cps()
    {{
return {{{{
        {0}
    }}}};
    }}

    std::array<case_mapping_to, {3}> make_case_mapping_to()
    {{
return {{{{
        {2}
    }}}};
    }}

    namespace {{

    constexpr std::array<uint32_t, {5}> cased_cps()
    {{
return {{{{
        {4}
    }}}};
    }}

    constexpr std::array<uint32_t, {7}> case_ignorable_cps()
    {{
return {{{{
        {6}
    }}}};
    }}

    constexpr std::array<uint32_t, {9}> soft_dotted_cps()
    {{
return {{{{
        {8}
    }}}};
    }}

    constexpr std::array<case_mapping, {11}> to_lower_mappings()
    {{
return {{{{
        {10}
    }}}};
    }}

    constexpr std::array<case_mapping, {13}> to_title_mappings()
    {{
return {{{{
        {12}
    }}}};
    }}

    constexpr std::array<case_mapping, {15}> to_upper_mappings()
    {{
return {{{{
        {14}
    }}}};
    }}

    constexpr std::array<uint32_t, {17}> changes_when_uppered()
    {{
return {{{{
        {16}
    }}}};
    }}

    constexpr std::array<uint32_t, {19}> changes_when_lowered()
    {{
return {{{{
        {18}
    }}}};
    }}

    constexpr std::array<uint32_t, {21}> changes_when_titled()
    {{
return {{{{
        {20}
    }}}};
    }}

    }}

    case_map_t make_to_lower_map()
    {{
        case_map_t retval;
        for (auto datum : to_lower_mappings()) {{
            retval[datum.from_] =
                case_elements{{datum.first_, datum.last_}};
        }}
        return retval;
    }}

    case_map_t make_to_title_map()
    {{
        case_map_t retval;
        for (auto datum : to_title_mappings()) {{
            retval[datum.from_] =
                case_elements{{datum.first_, datum.last_}};
        }}
        return retval;
    }}

    case_map_t make_to_upper_map()
    {{
        case_map_t retval;
        for (auto datum : to_upper_mappings()) {{
            retval[datum.from_] =
                case_elements{{datum.first_, datum.last_}};
        }}
        return retval;
    }}

    std::vector<uint32_t> make_soft_dotted_cps()
    {{
        auto const cps = soft_dotted_cps();
        return std::vector<uint32_t>(cps.begin(), cps.end());
    }}

    std::unordered_set<uint32_t> make_cased_cps()
    {{
        auto const cps = cased_cps();
        return std::unordered_set<uint32_t>(cps.begin(), cps.end());
    }}

    std::unordered_set<uint32_t> make_case_ignorable_cps()
    {{
        auto const cps = case_ignorable_cps();
        return std::unordered_set<uint32_t>(cps.begin(), cps.end());
    }}

    std::unordered_set<uint32_t> make_changes_when_uppered_cps()
    {{
        auto const cps = changes_when_uppered();
        return std::unordered_set<uint32_t>(cps.begin(), cps.end());
    }}

    std::unordered_set<uint32_t> make_changes_when_lowered_cps()
    {{
        auto const cps = changes_when_lowered();
        return std::unordered_set<uint32_t>(cps.begin(), cps.end());
    }}

    std::unordered_set<uint32_t> make_changes_when_titled_cps()
    {{
        auto const cps = changes_when_titled();
        return std::unordered_set<uint32_t>(cps.begin(), cps.end());
    }}

}}}}}}
'''

def get_case_mappings(unicode_data, special_casing, prop_list, derived_core_props):
    to_lower = {}
    to_title = {}
    to_upper = {}

    all_tuples = set()
    conditions = set()

    changes_when_u = []
    changes_when_l = []
    changes_when_t = []

    def init_dict_elem(k, m):
        if k not in m:
            m[k] = []

    lines = open(unicode_data, 'r').readlines()
    for line in lines:
        line = line[:-1]
        if not line.startswith('#') and len(line) != 0:
            comment_start = line.find('#')
            comment = ''
            if comment_start != -1:
                comment = line[comment_start + 1:].strip()
                line = line[:comment_start]
            fields = map(lambda x: x.strip(), line.split(';'))
            cp = fields[0]
            upper = fields[12]
            lower = fields[13]
            title = fields[14]
            if lower != '':
                init_dict_elem(cp, to_lower)
                to_lower[cp].append(([lower], [], 'from_unicode_data'))
                all_tuples.add((lower, None, None))
            if title != '':
                init_dict_elem(cp, to_title)
                to_title[cp].append(([title], [], 'from_unicode_data'))
                all_tuples.add((title, None, None))
            if upper != '':
                init_dict_elem(cp, to_upper)
                to_upper[cp].append(([upper], [], 'from_unicode_data'))
                all_tuples.add((upper, None, None))

    def to_tuple_2(l):
        if len(l) == 1:
            return (l[0], None)
        if len(l) == 2:
            return (l[0], l[1])
        return None

    def to_tuple_3(l):
        if len(l) == 1:
            return (l[0], None, None)
        if len(l) == 2:
            return (l[0], l[1], None)
        if len(l) == 3:
            return (l[0], l[1], l[2])
        return None

    def from_tuple(t):
        retval = []
        retval.append(t[0])
        if t[1] != None:
            retval.append(t[1])
        if 2 < len(t) and t[2] != None:
            retval.append(t[2])
        return retval

    lines = open(special_casing, 'r').readlines()
    for line in lines:
        line = line[:-1]
        if not line.startswith('#') and len(line) != 0:
            fields = map(lambda x: x.strip(), line.split(';'))
            cp = fields[0].strip()
            lower = fields[1].strip().split(' ')
            if lower == ['']:
                lower = []
            title = fields[2].strip().split(' ')
            if title == ['']:
                title = []
            upper = fields[3].strip().split(' ')
            if upper == ['']:
                upper = []
            conditions_ = []
            if 3 < len(fields) and '#' not in fields[4]:
                conditions_ = fields[4].strip().split(' ')
                for c in conditions_:
                    conditions.add(c)
            if len(lower):
                init_dict_elem(cp, to_lower)
                to_lower[cp].append((lower, conditions_, None))
                all_tuples.add(to_tuple_3(lower))
            if len(title):
                init_dict_elem(cp, to_title)
                to_title[cp].append((title, conditions_, None))
                all_tuples.add(to_tuple_3(title))
            if len(upper):
                init_dict_elem(cp, to_upper)
                to_upper[cp].append((upper, conditions_, None))
                all_tuples.add(to_tuple_3(upper))

    all_tuples = sorted(map(from_tuple, all_tuples))
    conditions = sorted(conditions)

    def subsequence(seq, subseq):
        i = 0
        while i < len(seq):
            if seq[i] == subseq[0]:
                break
            i += 1
        if i == len(seq):
            return (i, i)
        lo = i
        sub_i = 0
        while i < len(seq) and sub_i < len(subseq) and seq[i] == subseq[sub_i]:
            i += 1
            sub_i += 1
        if sub_i == len(subseq):
            return (lo, i)
        return (lo, lo)

    cps = []
    tuple_offsets = []
    tuple_offset = 0
    for i in range(len(all_tuples)):
        subseq = subsequence(cps, all_tuples[i])
        if subseq[0] != subseq[1]:
            tuple_offsets.append(subseq)
            continue
        cps += all_tuples[i]
        lo = tuple_offset
        tuple_offset += len(all_tuples[i])
        hi = tuple_offset
        tuple_offsets.append((lo, hi))
    def cp_indices(t):
        return tuple_offsets[all_tuples.index(from_tuple(t))]

    def to_cond_bitset(conds):
        retval = ' | '.join(map(lambda x: '(uint16_t)case_condition::' + x, conds))
        if retval == '':
            retval = '0'
        return retval

    all_mapped_tos = []

    def filter_dupes(l):
        retval = []
        for x in l:
            if x not in retval:
                retval.append(x)
        return retval

    def unconditioned_last(l):
        unconditioned = None
        retval = []
        for x in l:
            if x[1] == '0':
                unconditioned = x
            else:
                retval.append(x)
        if unconditioned != None:
            retval.append(unconditioned)
        return retval

    tmp = to_lower
    to_lower = []
    for k,v in tmp.items():
        lo = len(all_mapped_tos)
        mapped_tos = map(lambda x: (cp_indices(to_tuple_3(x[0])), to_cond_bitset(x[1])), v)
        mapped_tos = unconditioned_last(filter_dupes(mapped_tos))
        subseq = subsequence(all_mapped_tos, mapped_tos)
        if subseq[0] != subseq[1]:
            to_lower.append((k, subseq))
            continue
        all_mapped_tos += mapped_tos
        hi = len(all_mapped_tos)
        to_lower.append((k, (lo, hi)))

    tmp = to_title
    to_title = []
    for k,v in tmp.items():
        lo = len(all_mapped_tos)
        mapped_tos = map(lambda x: (cp_indices(to_tuple_3(x[0])), to_cond_bitset(x[1])), v)
        mapped_tos = unconditioned_last(filter_dupes(mapped_tos))
        subseq = subsequence(all_mapped_tos, mapped_tos)
        if subseq[0] != subseq[1]:
            to_title.append((k, subseq))
            continue
        all_mapped_tos += mapped_tos
        hi = len(all_mapped_tos)
        to_title.append((k, (lo, hi)))

    tmp = to_upper
    to_upper = []
    for k,v in tmp.items():
        lo = len(all_mapped_tos)
        mapped_tos = map(lambda x: (cp_indices(to_tuple_3(x[0])), to_cond_bitset(x[1])), v)
        mapped_tos = unconditioned_last(filter_dupes(mapped_tos))
        subseq = subsequence(all_mapped_tos, mapped_tos)
        if subseq[0] != subseq[1]:
            to_upper.append((k, subseq))
            continue
        all_mapped_tos += mapped_tos
        hi = len(all_mapped_tos)
        to_upper.append((k, (lo, hi)))

    soft_dotteds = []
    lines = open(prop_list, 'r').readlines()
    for line in lines:
        line = line[:-1]
        if not line.startswith('#') and len(line) != 0:
            fields = map(lambda x: x.strip(), line.split(';'))
            if fields[1].startswith('Soft_Dotted'):
                cps_ = fields[0].split('.')
                soft_dotteds.append(cps_[0])
                if 1 < len(cps_):
                    for i in range(int(cps_[0], 16) + 1, int(cps_[2], 16) + 1):
                         soft_dotteds.append(hex(i).upper()[2:])

    cased_cps = []
    cased_ignorable_cps = []
    lines = open(derived_core_props, 'r').readlines()
    for line in lines:
        line = line[:-1]
        if not line.startswith('#') and len(line) != 0:
            fields = map(lambda x: x.strip(), line.split(';'))
            if fields[1].startswith('Cased') or \
               fields[1].startswith('Case_Ignorable') or \
               fields[1].startswith('Changes_When_Lowercased') or \
               fields[1].startswith('Changes_When_Uppercased') or \
               fields[1].startswith('Changes_When_Titlecased'):
                cps_ = fields[0].split('.')
                if 1 < len(cps_):
                    r = range(int(cps_[0], 16) + 1, int(cps_[2], 16) + 1)
                    cps_ = cps_[:1]
                    for i in r:
                         cps_.append(hex(i).upper()[2:])
                else:
                    cps_ = cps_[:1]
                if fields[1].startswith('Cased'):
                    cased_cps += cps_
                elif fields[1].startswith('Case_Ignorable'):
                    cased_ignorable_cps += cps_
                elif fields[1].startswith('Changes_When_Uppercased'):
                    changes_when_u += cps_
                elif fields[1].startswith('Changes_When_Lowercased'):
                    changes_when_l += cps_
                elif fields[1].startswith('Changes_When_Titlecased'):
                    changes_when_t += cps_

    return to_lower, to_title, to_upper, cps, conditions, soft_dotteds, \
        cased_cps, cased_ignorable_cps, all_mapped_tos, changes_when_u, \
        changes_when_l, changes_when_t

to_lower, to_title, to_upper, cps, conditions, soft_dotteds, \
    cased_cps, cased_ignorable_cps, all_mapped_tos, \
    changes_when_u, changes_when_l, changes_when_t = \
    get_case_mappings('UnicodeData.txt', 'SpecialCasing.txt', \
                      'PropList.txt', 'DerivedCoreProperties.txt')

#changes_when_l = sorted(map(lambda x: int(x, 16), changes_when_l))
#changes_when_l_ranges = []
#prev_cp = 0xffffffff
#curr_range = [0xffffffff, 0xffffffff]
#ranged_n = 0
#for cp in changes_when_l:
#    #cp = int(cp_, 16)
#    if cp != prev_cp + 1:
#        if curr_range[0] == 0xffffffff:
#            curr_range[0] = cp
#        else:
#            curr_range[1] = prev_cp + 1
#            if curr_range[1] != curr_range[0] + 1:
#                changes_when_l_ranges.append((hex(curr_range[0]), hex(curr_range[1])))
#                ranged_n += curr_range[1] - curr_range[0]
#            curr_range = [0xffffffff, 0xffffffff]
#    prev_cp = cp
#print changes_when_l_ranges
#print len(changes_when_l_ranges), ranged_n, len(changes_when_l)

hpp_file = open('case_constants.hpp', 'w')
condition_enums = []
for i in range(len(conditions)):
    c = conditions[i]
    condition_enums.append('        {} = {},'.format(c, 1 << i))
hpp_file.write(constants_header_form.format('\n'.join(condition_enums)))

def make_case_mapping_to(t):
    return '{{ {}, {}, {} }}'.format(t[0][0], t[0][1], t[1])

def make_case_mapping(t):
    return '{{ 0x{}, {}, {} }}'.format(t[0], t[1][0], t[1][1])

def compressed_cp_lines(cps):
    values_per_line = 12
    bytes_ = []
    for cp in cps:
        lzw.add_cp(bytes_, int(cp, 16))
    compressed_bytes = lzw.compress(bytes_)
    print 'rewrote {} * 32 = {} bits as {} * 8 = {} bits'.format(len(cps), len(cps)*32, len(bytes_), len(bytes_)*8)
    print 'compressed to {} * 16 = {} bits'.format(len(compressed_bytes), len(compressed_bytes) * 16)
    return lzw.compressed_bytes_to_lines(compressed_bytes, values_per_line)

# No real gain (<1%).  Use uncompressed byte stream instead.
#compressed_cp_lines(cased_cps)
# No real gain (identical size!).  Use uncompressed byte stream instead.
#compressed_cp_lines(cased_ignorable_cps)

case_conditions = {
    '(uint16_t)case_condition::After_I': 1,
    '(uint16_t)case_condition::After_Soft_Dotted': 2,
    '(uint16_t)case_condition::Final_Sigma': 4,
    '(uint16_t)case_condition::More_Above': 8,
    '(uint16_t)case_condition::Not_Before_Dot': 16,
    '(uint16_t)case_condition::az': 32,
    '(uint16_t)case_condition::lt': 64,
    '(uint16_t)case_condition::tr': 128
}
def compressed_case_mapping_to_lines(mappings):
    values_per_line = 12
    bytes_ = []
    for t in mappings:
        lzw.add_short(bytes_, t[0][0])
        lzw.add_short(bytes_, t[0][1])
        try:
            x = case_conditions[t[1]] # TODO: Totally wrong!  Just here for size eval.
        except:
            x = 0
        lzw.add_short(bytes_, x)
    compressed_bytes = lzw.compress(bytes_)
    print 'rewrote {} * 48 = {} bits as {} * 8 = {} bits'.format(len(mappings), len(mappings)*48, len(bytes_), len(bytes_)*8)
    print 'compressed to {} * 16 = {} bits'.format(len(compressed_bytes), len(compressed_bytes) * 16)
    return lzw.compressed_bytes_to_lines(compressed_bytes, values_per_line)

# Heavy pessimization.
#compressed_case_mapping_to_lines(all_mapped_tos)

def compressed_case_mapping_lines(mappings):
    values_per_line = 12
    bytes_ = []
    for t in mappings:
        lzw.add_cp(bytes_, int(t[0], 16))
        lzw.add_short(bytes_, t[1][0])
        lzw.add_short(bytes_, t[1][1])
    compressed_bytes = lzw.compress(bytes_)
    print 'rewrote {} * 64 = {} bits as {} * 8 = {} bits'.format(len(mappings), len(mappings)*64, len(bytes_), len(bytes_)*8)
    print 'compressed to {} * 16 = {} bits'.format(len(compressed_bytes), len(compressed_bytes) * 16)
    return lzw.compressed_bytes_to_lines(compressed_bytes, values_per_line)

# compressed_case_mapping_lines(to_lower)
# compressed_case_mapping_lines(to_title)
# compressed_case_mapping_lines(to_upper)

def cps_string(cps):
    return ''.join(map(lambda x: r'\U' + '0' * (8 - len(x)) + x, cps))

def cus_lines(cus):
    as_ints = map(lambda x: ord(x), cus)
    values_per_line = 12
    return lzw.compressed_bytes_to_lines(as_ints, values_per_line)[0]

def utf8_cps(cps):
    s = cps_string(cps)
    exec("s = u'" + s + "'")
    s = s.encode('UTF-8', 'strict')
    retval = cus_lines(s)
    print 'rewrote {} * 32 = {} bits as {} * 8 = {} bits ({} bytes saved)'.format(len(cps), len(cps) * 32, len(s), len(s) * 8, (len(cps) * 32 - len(s) * 8) / 8.0)
    return retval

#utf8_cps(cps)
#utf8_cps(cased_cps)
#utf8_cps(cased_ignorable_cps)
#utf8_cps(soft_dotteds)
#utf8_cps(changes_when_u)
#utf8_cps(changes_when_l)
#utf8_cps(changes_when_t)

cpp_file = open('case_mapping.cpp', 'w')
cpp_file.write(case_impl_file_form.format(
    ',\n        '.join(map(lambda x: '0x' + x, cps)),
    len(cps),
    ',\n        '.join(map(make_case_mapping_to, all_mapped_tos)),
    len(all_mapped_tos),
    ',\n        '.join(map(lambda x: '0x' + x, cased_cps)),
    len(cased_cps),
    ',\n        '.join(map(lambda x: '0x' + x, cased_ignorable_cps)),
    len(cased_ignorable_cps),
    ',\n        '.join(map(lambda x: '0x' + x, soft_dotteds)),
    len(soft_dotteds),
    ',\n        '.join(map(make_case_mapping, to_lower)),
    len(to_lower),
    ',\n        '.join(map(make_case_mapping, to_title)),
    len(to_title),
    ',\n        '.join(map(make_case_mapping, to_upper)),
    len(to_upper),
    ',\n        '.join(map(lambda x: '0x' + x, changes_when_u)),
    len(changes_when_u),
    ',\n        '.join(map(lambda x: '0x' + x, changes_when_l)),
    len(changes_when_l),
    ',\n        '.join(map(lambda x: '0x' + x, changes_when_t)),
    len(changes_when_t)
))
