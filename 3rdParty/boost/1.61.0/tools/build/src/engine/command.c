/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*  This file is ALSO:
 *  Copyright 2001-2004 David Abrahams.
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
 */

/*
 * command.c - maintain lists of commands
 */

#include "jam.h"
#include "command.h"

#include "lists.h"
#include "rules.h"

#include <assert.h>


/*
 * cmdlist_append_cmd
 */
CMDLIST * cmdlist_append_cmd( CMDLIST * l, CMD * cmd )
{
    CMDLIST * result = (CMDLIST *)BJAM_MALLOC( sizeof( CMDLIST ) );
    result->iscmd = 1;
    result->next = l;
    result->impl.cmd = cmd;
    return result;
}

CMDLIST * cmdlist_append_target( CMDLIST * l, TARGET * t )
{
    CMDLIST * result = (CMDLIST *)BJAM_MALLOC( sizeof( CMDLIST ) );
    result->iscmd = 0;
    result->next = l;
    result->impl.t = t;
    return result;
}

void cmdlist_free( CMDLIST * l )
{
    while ( l )
    {
        CMDLIST * tmp = l->next;
        BJAM_FREE( l );
        l = tmp;
    }
}

/*
 * cmd_new() - return a new CMD.
 */

CMD * cmd_new( RULE * rule, LIST * targets, LIST * sources, LIST * shell )
{
    CMD * cmd = (CMD *)BJAM_MALLOC( sizeof( CMD ) );
    FRAME frame[ 1 ];

    assert( cmd );
    cmd->rule = rule;
    cmd->shell = shell;
    cmd->next = 0;
    cmd->noop = 0;
    cmd->asynccnt = 1;
    cmd->status = 0;
    cmd->lock = NULL;
    cmd->unlock = NULL;

    lol_init( &cmd->args );
    lol_add( &cmd->args, targets );
    lol_add( &cmd->args, sources );
    string_new( cmd->buf );

    frame_init( frame );
    frame->module = rule->module;
    lol_init( frame->args );
    lol_add( frame->args, list_copy( targets ) );
    lol_add( frame->args, list_copy( sources ) );
    function_run_actions( rule->actions->command, frame, stack_global(),
        cmd->buf );
    frame_free( frame );

    return cmd;
}


/*
 * cmd_free() - free a CMD
 */

void cmd_free( CMD * cmd )
{
    cmdlist_free( cmd->next );
    lol_free( &cmd->args );
    list_free( cmd->shell );
    string_free( cmd->buf );
    freetargets( cmd->unlock );
    BJAM_FREE( (void *)cmd );
}


/*
 * cmd_release_targets_and_shell()
 *
 *   Makes the CMD release its hold on its targets & shell lists and forget
 * about them. Useful in case caller still has references to those lists and
 * wants to reuse them after freeing the CMD object.
 */

void cmd_release_targets_and_shell( CMD * cmd )
{
    cmd->args.list[ 0 ] = L0;  /* targets */
    cmd->shell = L0;           /* shell   */
}
