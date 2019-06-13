/*
 * Copyright 1994 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * command.h - the CMD structure and routines to manipulate them
 *
 * Both ACTION and CMD contain a rule, targets, and sources.  An
 * ACTION describes a rule to be applied to the given targets and
 * sources; a CMD is what actually gets executed by the shell.  The
 * differences are due to:
 *
 *  ACTIONS must be combined if 'actions together' is given.
 *  ACTIONS must be split if 'actions piecemeal' is given.
 *  ACTIONS must have current sources omitted for 'actions updated'.
 *
 * The CMD datatype holds a single command that is to be executed
 * against a target, and they can chain together to represent the
 * full collection of commands used to update a target.
 *
 * Structures:
 *
 *  CMD - an action, ready to be formatted into a buffer and executed.
 *
 * External routines:
 *
 *  cmd_new() - return a new CMD or 0 if too many args.
 *  cmd_free() - delete CMD and its parts.
 *  cmd_next() - walk the CMD chain.
 *  cmd_release_targets_and_shell() - CMD forgets about its targets & shell.
 */


/*
 * CMD - an action, ready to be formatted into a buffer and executed.
 */

#ifndef COMMAND_SW20111118_H
#define COMMAND_SW20111118_H

#include "lists.h"
#include "rules.h"
#include "strings.h"


typedef struct _cmd CMD;

/*
 * A list whose elements are either TARGETS or CMDS.
 * CMDLIST is used only by CMD.  A TARGET means that
 * the CMD is the last updating action required to
 * build the target.  A CMD is the next CMD required
 * to build the same target.  (Note that a single action
 * can update more than one target, so the CMDs form
 * a DAG, not a straight linear list.)
 */
typedef struct _cmdlist {
    struct _cmdlist * next;
    union {
        CMD * cmd;
        TARGET * t;
    } impl;
    char iscmd;
} CMDLIST;

CMDLIST * cmdlist_append_cmd( CMDLIST *, CMD * );
CMDLIST * cmdlist_append_target( CMDLIST *, TARGET * );
void cmd_list_free( CMDLIST * );

struct _cmd
{
    CMDLIST * next;
    RULE * rule;      /* rule->actions contains shell script */
    LIST * shell;     /* $(JAMSHELL) value */
    LOL    args;      /* LISTs for $(<), $(>) */
    string buf[ 1 ];  /* actual commands */
    int    noop;      /* no-op commands should be faked instead of executed */
    int    asynccnt;  /* number of outstanding dependencies */
    TARGETS * lock;   /* semaphores that are required by this cmd. */
    TARGETS * unlock; /* semaphores that are released when this cmd finishes. */
    char   status;    /* the command status */
};

CMD * cmd_new
(
    RULE * rule,     /* rule (referenced) */
    LIST * targets,  /* $(<) (ownership transferred) */
    LIST * sources,  /* $(>) (ownership transferred) */
    LIST * shell     /* $(JAMSHELL) (ownership transferred) */
);

void cmd_release_targets_and_shell( CMD * );

void cmd_free( CMD * );

#define cmd_next( c ) ((c)->next)

#endif
