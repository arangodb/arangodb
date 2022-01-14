#!/usr/bin/env python
# Copyright (C) 2006 Douglas Gregor <doug.gregor -at- gmail.com>.

# Use, modification and distribution is subject to the Boost Software
# License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Test broadcast() collective.

from __future__ import print_function
import mpi

def broadcast_test(comm, value, kind, root):
    if comm.rank == 0:
        print ("Broadcasting %s from root %d..." % (kind, root)),
    got_value = None
    got_value = mpi.broadcast(comm, value, root)
    if comm.rank == 0:
        print ("OK.")
    return

broadcast_test(mpi.world, 17, 'integer', 0)
broadcast_test(mpi.world, 'Hello, World!', 'string', 0)
broadcast_test(mpi.world, ['Hello', 'MPI', 'Python', 'World'],
               'list of strings', 0)
if mpi.world.size > 1:
    broadcast_test(mpi.world, 17, 'integer', 1)
    broadcast_test(mpi.world, 'Hello, World!', 'string', 1)
    broadcast_test(mpi.world, ['Hello', 'MPI', 'Python', 'World'],
                   'list of strings', 1)


