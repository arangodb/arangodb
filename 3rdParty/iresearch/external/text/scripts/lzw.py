# Copyright (C) 2020 T. Zachary Laine
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

def add_int_impl(values, n):
    values.append(n)
def add_int(values, n):
    _0 = (n >> 24) & 0xff
    _1 = (n >> 16) & 0xff
    _2 = (n >> 8)  & 0xff
    _3 = (n >> 0)  & 0xff
    add_int_impl(values, _0)
    add_int_impl(values, _1)
    add_int_impl(values, _2)
    add_int_impl(values, _3)
def add_cp(values, n):
    _0 = (n >> 16) & 0xff
    _1 = (n >> 8)  & 0xff
    _2 = (n >> 0)  & 0xff
    add_int_impl(values, _0)
    add_int_impl(values, _1)
    add_int_impl(values, _2)
def add_short(values, n):
    _0 = (n >> 8)  & 0xff
    _1 = (n >> 0)  & 0xff
    add_int_impl(values, _0)
    add_int_impl(values, _1)
def add_byte(values, n):
    _0 = (n >> 0)  & 0xff
    add_int_impl(values, _0)

def compressed_bytes_to_lines(compressed_bytes, values_per_line):
    compressed_byte_lines = ''
    i = 0
    while i + values_per_line < len(compressed_bytes):
        compressed_byte_lines += ', '.join(map(lambda x: hex(x), compressed_bytes[i:i+values_per_line])) + ',\n'
        i += values_per_line
    compressed_byte_lines += ', '.join(map(lambda x: hex(x), compressed_bytes[i:])) + '\n'
    return compressed_byte_lines, len(compressed_bytes)

def compress(uncompressed, bits = 16):
    if bits < 9:
        raise Exception('lzw.compress() requires bits >= 9.')
    string_table = {}
    for i in range(256):
        #print 'initial value',(i,),i
        string_table[(i,)] = i
    next_table_value = 256
    end_table_value = 1 << bits
    compressed = []
    i = 1
    s = [uncompressed[0]]
    while i < len(uncompressed):
        c = uncompressed[i]
        if c < 0 or 256 <= c:
            raise Exception('lzw.compress() requires all input values to be in [0, 256).')
        i += 1
        s_plus_c = tuple(s + [c])
        if s_plus_c in string_table:
            s.append(c)
        else:
            code_for_s = string_table[tuple(s)]
            compressed.append(code_for_s)
            if next_table_value < end_table_value:
                #print 'table value',s_plus_c,next_table_value
                string_table[s_plus_c] = next_table_value
                next_table_value += 1
            s = [c]
    code_for_s = string_table[tuple(s)]
    compressed.append(code_for_s)
    round_tripped = decompress(compressed, bits)
    if round_tripped != uncompressed:
        print len(uncompressed),len(round_tripped)
        raise Exception('lzw.compress() round-trip check failed.')
    return compressed

def decompress(compressed, bits = 16):
    if bits < 9:
        raise Exception('lzw.decompress() requires bits >= 9.')
    reverse_table = {}
    for i in range(256):
        #print 'initial value',(i,),i
        reverse_table[i] = [i]
    next_table_value = 256
    end_table_value = 1 << bits
    uncompressed = []
    i = 1
    prev_code = compressed[0]
    uncompressed.append(prev_code)
    c = prev_code
    while i < len(compressed):
        code = compressed[i]
        i += 1
        #print 'looking up code',code,'in table'
        if code in reverse_table:
            s = reverse_table[code]
        else:
            s = reverse_table[prev_code] + [c]
        uncompressed += s
        c = s[0]
        prev_code_string_plus_c = reverse_table[prev_code] + [c]
        if next_table_value < end_table_value:
            #print 'table value',tuple(prev_code_string_plus_c),next_table_value
            reverse_table[next_table_value] = prev_code_string_plus_c
            next_table_value += 1
        prev_code = code
    return uncompressed
