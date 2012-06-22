/*  =========================================================================
    czmq_tests.c - run selftests

    Runs all selftests.

    -------------------------------------------------------------------------
    Copyright (c) 1991-2011 iMatix Corporation <www.imatix.com>
    Copyright other contributors as noted in the AUTHORS file.

    This file is part of czmq, the high-level C binding for 0MQ:
    http://czmq.zeromq.org.

    This is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or (at
    your option) any later version.

    This software is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this program. If not, see
    <http://www.gnu.org/licenses/>.
    =========================================================================
*/

#include "../include/czmq_prelude.h"
#include "../include/zclock.h"
#include "../include/zctx.h"
#include "../include/zfile.h"
#include "../include/zframe.h"
#include "../include/zhash.h"
#include "../include/zlist.h"
#include "../include/zloop.h"
#include "../include/zmsg.h"
#include "../include/zsocket.h"
#include "../include/zsockopt.h"
#include "../include/zstr.h"
#include "../include/zthread.h"

int main (int argc, char *argv [])
{
    Bool verbose;
    if (argc == 2 && streq (argv [1], "-v"))
        verbose = TRUE;
    else
        verbose = FALSE;

    printf ("Running czmq self tests...\n");

    zclock_test (verbose);
    zctx_test (verbose);
    zfile_test (verbose);
    zframe_test (verbose);
    zhash_test (verbose);
    zlist_test (verbose);
    zloop_test (verbose);
    zmsg_test (verbose);
    zsocket_test (verbose);
    zsockopt_test (verbose);
    zstr_test (verbose);
    zthread_test (verbose);
    printf ("Tests passed OK\n");
    return 0;
}
