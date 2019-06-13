
print '''
# Generated file `python boost_contract_no.jam-gen.py > boost_contract_no.jam`.

# Copyright (C) 2008-2018 Lorenzo Caminiti
# Distributed under the Boost Software License, Version 1.0 (see accompanying
# file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
# See: https://lcaminiti.github.io/boost-contract
'''

import collections
import itertools

defs = collections.OrderedDict([
    # Short keys (1-2 chars) as MSVC gives linker errors on long file names.
    ('y', 'BOOST_CONTRACT_NO_ENTRY_INVARIANTS'),
    ('r', 'BOOST_CONTRACT_NO_PRECONDITIONS'),
    ('x', 'BOOST_CONTRACT_NO_EXIT_INVARIANTS'),
    ('s', 'BOOST_CONTRACT_NO_POSTCONDITIONS'),
    ('e', 'BOOST_CONTRACT_NO_EXCEPTS'),
    ('k', 'BOOST_CONTRACT_NO_CHECKS')
    # Add more macros here.
])
separator = '' # Might want to set to '_' if keys longer than 1 char.

print 'module boost_contract_no {\n'
s = ''
exit
for r in range(len(defs.keys())):
    for comb in itertools.combinations(defs.keys(), r + 1):
        c = ''
        d = ''
        sep = ''
        for cond in comb:
            c += sep + cond
            sep = separator
            d += " <define>" + defs[cond]
        s += ' ' + c
        print 'rule defs_{0} {{ return {1} ; }}\n'.format(c, d[1:])

print '''rule combinations {{ return {0} ; }}

}} # module

# All combinations: {1}
'''.format(s[1:], s.replace(" ", ",")[1:])

