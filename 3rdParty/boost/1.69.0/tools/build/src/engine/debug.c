/*
 * Copyright 2005, 2016. Rene Rivera
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include "jam.h"
#include "debug.h"
#include "output.h"
#include "hash.h"
#include <time.h>


static profile_frame * profile_stack = 0;
static struct hash   * profile_hash  = 0;
static profile_info    profile_other = { 0 };
static profile_info    profile_total = { 0 };


profile_frame * profile_init( OBJECT * rulename, profile_frame * frame )
{
    if ( DEBUG_PROFILE ) profile_enter( rulename, frame );
    return frame;
}


void profile_enter( OBJECT * rulename, profile_frame * frame )
{
    if ( DEBUG_PROFILE )
    {
        double start = profile_clock();
        profile_info * p;

        if ( !profile_hash && rulename )
            profile_hash = hashinit( sizeof( profile_info ), "profile" );

        if ( rulename )
        {
            int found;
            p = (profile_info *)hash_insert( profile_hash, rulename, &found );
            if ( !found )
            {
                p->name = rulename;
                p->cumulative = 0;
                p->net = 0;
                p->num_entries = 0;
                p->stack_count = 0;
                p->memory = 0;
            }
        }
        else
        {
             p = &profile_other;
        }

        p->num_entries += 1;
        p->stack_count += 1;

        frame->info = p;

        frame->caller = profile_stack;
        profile_stack = frame;

        frame->entry_time = profile_clock();
        frame->overhead = 0;
        frame->subrules = 0;

        /* caller pays for the time it takes to play with the hash table */
        if ( frame->caller )
            frame->caller->overhead += frame->entry_time - start;
    }
}


void profile_memory( long mem )
{
    if ( DEBUG_PROFILE )
        if ( profile_stack && profile_stack->info )
            profile_stack->info->memory += ((double)mem) / 1024;
}


void profile_exit( profile_frame * frame )
{
    if ( DEBUG_PROFILE )
    {
        /* Cumulative time for this call. */
        double t = profile_clock() - frame->entry_time - frame->overhead;
        /* If this rule is already present on the stack, do not add the time for
         * this instance.
         */
        if ( frame->info->stack_count == 1 )
            frame->info->cumulative += t;
        /* Net time does not depend on presence of the same rule in call stack.
         */
        frame->info->net += t - frame->subrules;

        if ( frame->caller )
        {
            /* Caller's cumulative time must account for this overhead. */
            frame->caller->overhead += frame->overhead;
            frame->caller->subrules += t;
        }
        /* Pop this stack frame. */
        --frame->info->stack_count;
        profile_stack = frame->caller;
    }
}


static void dump_profile_entry( void * p_, void * ignored )
{
    profile_info * p = (profile_info *)p_;
    double mem_each = ( p->memory / ( p->num_entries ? p->num_entries : 1
        ) );
    double q = p->net;
    if (p->num_entries) q /= p->num_entries;
    if ( !ignored )
    {
        profile_total.cumulative += p->net;
        profile_total.memory += p->memory;
    }
    out_printf( "%10ld %12.6f %12.6f %12.8f %10.2f %10.2f %s\n", p->num_entries,
    	p->cumulative, p->net, q, p->memory, mem_each, object_str( p->name ) );
}


void profile_dump()
{
    if ( profile_hash )
    {
        out_printf( "%10s %12s %12s %12s %10s %10s %s\n", "--count--", "--gross--",
            "--net--", "--each--", "--mem--", "--each--", "--name--" );
        hashenumerate( profile_hash, dump_profile_entry, 0 );
        profile_other.name = constant_other;
        dump_profile_entry( &profile_other, 0 );
        profile_total.name = constant_total;
        dump_profile_entry( &profile_total, (void *)1 );
    }
}

double profile_clock()
{
    return ((double) clock()) / CLOCKS_PER_SEC;
}

OBJECT * profile_make_local( char const * scope )
{
    if ( DEBUG_PROFILE )
    {
        return object_new( scope );
    }
    else
    {
       return 0;
    }
}
