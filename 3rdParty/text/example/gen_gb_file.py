#!/usr/bin/env python

f = open('book.txt', 'w')

f.write('Chapter One:\n============\n\n')

s = 'All work and no play makes jack a dull boy.\n'
s_len = len(s)

size = 0
while size < (1 << 26):
    f.write(s)
    size += s_len
