/*  =========================================================================
    zsockopt - get/set 0MQ socket options

    GENERATED SOURCE CODE, DO NOT EDIT
    -------------------------------------------------------------------------
    Copyright (c) 1991-2011 iMatix Corporation <www.imatix.com>
    Copyright other contributors as noted in the AUTHORS file.

    This file is part of CZMQ, the high-level C binding for 0MQ:
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

/*
@header
    The zsockopt class provides access to the 0MQ getsockopt/setsockopt API.
@discuss
    This class is generated, using the GSL code generator. See the sockopts
    XML file, which provides the metadata, and the sockopts.gsl template,
    which does the work.
@end
*/

#include "../include/czmq_prelude.h"
#include "../include/zctx.h"
#include "../include/zsocket.h"
#include "../include/zsockopt.h"


#if (ZMQ_VERSION_MAJOR == 2)
//  --------------------------------------------------------------------------
//  Set socket ZMQ_HWM value

void
zsockopt_set_hwm (void *socket, int hwm)
{
    uint64_t value = hwm;
    int rc = zmq_setsockopt (socket, ZMQ_HWM, &value, sizeof (uint64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_HWM value

int
zsockopt_hwm (void *socket)
{
    uint64_t hwm;
    size_t type_size = sizeof (uint64_t);
    zmq_getsockopt (socket, ZMQ_HWM, &hwm, &type_size);
    return (int) hwm;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_SWAP value

void
zsockopt_set_swap (void *socket, int swap)
{
    int64_t value = swap;
    int rc = zmq_setsockopt (socket, ZMQ_SWAP, &value, sizeof (int64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_SWAP value

int
zsockopt_swap (void *socket)
{
    int64_t swap;
    size_t type_size = sizeof (int64_t);
    zmq_getsockopt (socket, ZMQ_SWAP, &swap, &type_size);
    return (int) swap;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_AFFINITY value

void
zsockopt_set_affinity (void *socket, int affinity)
{
    uint64_t value = affinity;
    int rc = zmq_setsockopt (socket, ZMQ_AFFINITY, &value, sizeof (uint64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_AFFINITY value

int
zsockopt_affinity (void *socket)
{
    uint64_t affinity;
    size_t type_size = sizeof (uint64_t);
    zmq_getsockopt (socket, ZMQ_AFFINITY, &affinity, &type_size);
    return (int) affinity;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_IDENTITY value

void
zsockopt_set_identity (void *socket, char * identity)
{
    int rc = zmq_setsockopt (socket, ZMQ_IDENTITY, identity, strlen (identity));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RATE value

void
zsockopt_set_rate (void *socket, int rate)
{
    int64_t value = rate;
    int rc = zmq_setsockopt (socket, ZMQ_RATE, &value, sizeof (int64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RATE value

int
zsockopt_rate (void *socket)
{
    int64_t rate;
    size_t type_size = sizeof (int64_t);
    zmq_getsockopt (socket, ZMQ_RATE, &rate, &type_size);
    return (int) rate;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RECOVERY_IVL value

void
zsockopt_set_recovery_ivl (void *socket, int recovery_ivl)
{
    int64_t value = recovery_ivl;
    int rc = zmq_setsockopt (socket, ZMQ_RECOVERY_IVL, &value, sizeof (int64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RECOVERY_IVL value

int
zsockopt_recovery_ivl (void *socket)
{
    int64_t recovery_ivl;
    size_t type_size = sizeof (int64_t);
    zmq_getsockopt (socket, ZMQ_RECOVERY_IVL, &recovery_ivl, &type_size);
    return (int) recovery_ivl;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RECOVERY_IVL_MSEC value

void
zsockopt_set_recovery_ivl_msec (void *socket, int recovery_ivl_msec)
{
    int64_t value = recovery_ivl_msec;
    int rc = zmq_setsockopt (socket, ZMQ_RECOVERY_IVL_MSEC, &value, sizeof (int64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RECOVERY_IVL_MSEC value

int
zsockopt_recovery_ivl_msec (void *socket)
{
    int64_t recovery_ivl_msec;
    size_t type_size = sizeof (int64_t);
    zmq_getsockopt (socket, ZMQ_RECOVERY_IVL_MSEC, &recovery_ivl_msec, &type_size);
    return (int) recovery_ivl_msec;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_MCAST_LOOP value

void
zsockopt_set_mcast_loop (void *socket, int mcast_loop)
{
    int64_t value = mcast_loop;
    int rc = zmq_setsockopt (socket, ZMQ_MCAST_LOOP, &value, sizeof (int64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_MCAST_LOOP value

int
zsockopt_mcast_loop (void *socket)
{
    int64_t mcast_loop;
    size_t type_size = sizeof (int64_t);
    zmq_getsockopt (socket, ZMQ_MCAST_LOOP, &mcast_loop, &type_size);
    return (int) mcast_loop;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_SNDBUF value

void
zsockopt_set_sndbuf (void *socket, int sndbuf)
{
    uint64_t value = sndbuf;
    int rc = zmq_setsockopt (socket, ZMQ_SNDBUF, &value, sizeof (uint64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_SNDBUF value

int
zsockopt_sndbuf (void *socket)
{
    uint64_t sndbuf;
    size_t type_size = sizeof (uint64_t);
    zmq_getsockopt (socket, ZMQ_SNDBUF, &sndbuf, &type_size);
    return (int) sndbuf;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RCVBUF value

void
zsockopt_set_rcvbuf (void *socket, int rcvbuf)
{
    uint64_t value = rcvbuf;
    int rc = zmq_setsockopt (socket, ZMQ_RCVBUF, &value, sizeof (uint64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RCVBUF value

int
zsockopt_rcvbuf (void *socket)
{
    uint64_t rcvbuf;
    size_t type_size = sizeof (uint64_t);
    zmq_getsockopt (socket, ZMQ_RCVBUF, &rcvbuf, &type_size);
    return (int) rcvbuf;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_LINGER value

void
zsockopt_set_linger (void *socket, int linger)
{
    int rc = zmq_setsockopt (socket, ZMQ_LINGER, &linger, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_LINGER value

int
zsockopt_linger (void *socket)
{
    int linger;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_LINGER, &linger, &type_size);
    return linger;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RECONNECT_IVL value

void
zsockopt_set_reconnect_ivl (void *socket, int reconnect_ivl)
{
    int rc = zmq_setsockopt (socket, ZMQ_RECONNECT_IVL, &reconnect_ivl, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RECONNECT_IVL value

int
zsockopt_reconnect_ivl (void *socket)
{
    int reconnect_ivl;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RECONNECT_IVL, &reconnect_ivl, &type_size);
    return reconnect_ivl;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RECONNECT_IVL_MAX value

void
zsockopt_set_reconnect_ivl_max (void *socket, int reconnect_ivl_max)
{
    int rc = zmq_setsockopt (socket, ZMQ_RECONNECT_IVL_MAX, &reconnect_ivl_max, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RECONNECT_IVL_MAX value

int
zsockopt_reconnect_ivl_max (void *socket)
{
    int reconnect_ivl_max;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RECONNECT_IVL_MAX, &reconnect_ivl_max, &type_size);
    return reconnect_ivl_max;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_BACKLOG value

void
zsockopt_set_backlog (void *socket, int backlog)
{
    int rc = zmq_setsockopt (socket, ZMQ_BACKLOG, &backlog, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_BACKLOG value

int
zsockopt_backlog (void *socket)
{
    int backlog;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_BACKLOG, &backlog, &type_size);
    return backlog;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_SUBSCRIBE value

void
zsockopt_set_subscribe (void *socket, char * subscribe)
{
    int rc = zmq_setsockopt (socket, ZMQ_SUBSCRIBE, subscribe, strlen (subscribe));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_UNSUBSCRIBE value

void
zsockopt_set_unsubscribe (void *socket, char * unsubscribe)
{
    int rc = zmq_setsockopt (socket, ZMQ_UNSUBSCRIBE, unsubscribe, strlen (unsubscribe));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_TYPE value

int
zsockopt_type (void *socket)
{
    int type;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_TYPE, &type, &type_size);
    return type;
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RCVMORE value

int
zsockopt_rcvmore (void *socket)
{
    int64_t rcvmore;
    size_t type_size = sizeof (int64_t);
    zmq_getsockopt (socket, ZMQ_RCVMORE, &rcvmore, &type_size);
    return (int) rcvmore;
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_FD value

int
zsockopt_fd (void *socket)
{
    int fd;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_FD, &fd, &type_size);
    return fd;
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_EVENTS value

int
zsockopt_events (void *socket)
{
    uint32_t events;
    size_t type_size = sizeof (uint32_t);
    zmq_getsockopt (socket, ZMQ_EVENTS, &events, &type_size);
    return (int) events;
}


#endif

#if (ZMQ_VERSION_MAJOR == 3)
//  --------------------------------------------------------------------------
//  Set socket ZMQ_SNDHWM value

void
zsockopt_set_sndhwm (void *socket, int sndhwm)
{
    int rc = zmq_setsockopt (socket, ZMQ_SNDHWM, &sndhwm, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_SNDHWM value

int
zsockopt_sndhwm (void *socket)
{
    int sndhwm;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_SNDHWM, &sndhwm, &type_size);
    return sndhwm;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RCVHWM value

void
zsockopt_set_rcvhwm (void *socket, int rcvhwm)
{
    int rc = zmq_setsockopt (socket, ZMQ_RCVHWM, &rcvhwm, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RCVHWM value

int
zsockopt_rcvhwm (void *socket)
{
    int rcvhwm;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RCVHWM, &rcvhwm, &type_size);
    return rcvhwm;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_AFFINITY value

void
zsockopt_set_affinity (void *socket, int affinity)
{
    uint64_t value = affinity;
    int rc = zmq_setsockopt (socket, ZMQ_AFFINITY, &value, sizeof (uint64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_AFFINITY value

int
zsockopt_affinity (void *socket)
{
    uint64_t affinity;
    size_t type_size = sizeof (uint64_t);
    zmq_getsockopt (socket, ZMQ_AFFINITY, &affinity, &type_size);
    return (int) affinity;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_IDENTITY value

void
zsockopt_set_identity (void *socket, char * identity)
{
    int rc = zmq_setsockopt (socket, ZMQ_IDENTITY, identity, strlen (identity));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RATE value

void
zsockopt_set_rate (void *socket, int rate)
{
    int rc = zmq_setsockopt (socket, ZMQ_RATE, &rate, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RATE value

int
zsockopt_rate (void *socket)
{
    int rate;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RATE, &rate, &type_size);
    return rate;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RECOVERY_IVL value

void
zsockopt_set_recovery_ivl (void *socket, int recovery_ivl)
{
    int rc = zmq_setsockopt (socket, ZMQ_RECOVERY_IVL, &recovery_ivl, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RECOVERY_IVL value

int
zsockopt_recovery_ivl (void *socket)
{
    int recovery_ivl;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RECOVERY_IVL, &recovery_ivl, &type_size);
    return recovery_ivl;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_SNDBUF value

void
zsockopt_set_sndbuf (void *socket, int sndbuf)
{
    int rc = zmq_setsockopt (socket, ZMQ_SNDBUF, &sndbuf, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_SNDBUF value

int
zsockopt_sndbuf (void *socket)
{
    int sndbuf;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_SNDBUF, &sndbuf, &type_size);
    return sndbuf;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RCVBUF value

void
zsockopt_set_rcvbuf (void *socket, int rcvbuf)
{
    int rc = zmq_setsockopt (socket, ZMQ_RCVBUF, &rcvbuf, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RCVBUF value

int
zsockopt_rcvbuf (void *socket)
{
    int rcvbuf;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RCVBUF, &rcvbuf, &type_size);
    return rcvbuf;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_LINGER value

void
zsockopt_set_linger (void *socket, int linger)
{
    int rc = zmq_setsockopt (socket, ZMQ_LINGER, &linger, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_LINGER value

int
zsockopt_linger (void *socket)
{
    int linger;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_LINGER, &linger, &type_size);
    return linger;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RECONNECT_IVL value

void
zsockopt_set_reconnect_ivl (void *socket, int reconnect_ivl)
{
    int rc = zmq_setsockopt (socket, ZMQ_RECONNECT_IVL, &reconnect_ivl, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RECONNECT_IVL value

int
zsockopt_reconnect_ivl (void *socket)
{
    int reconnect_ivl;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RECONNECT_IVL, &reconnect_ivl, &type_size);
    return reconnect_ivl;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RECONNECT_IVL_MAX value

void
zsockopt_set_reconnect_ivl_max (void *socket, int reconnect_ivl_max)
{
    int rc = zmq_setsockopt (socket, ZMQ_RECONNECT_IVL_MAX, &reconnect_ivl_max, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RECONNECT_IVL_MAX value

int
zsockopt_reconnect_ivl_max (void *socket)
{
    int reconnect_ivl_max;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RECONNECT_IVL_MAX, &reconnect_ivl_max, &type_size);
    return reconnect_ivl_max;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_BACKLOG value

void
zsockopt_set_backlog (void *socket, int backlog)
{
    int rc = zmq_setsockopt (socket, ZMQ_BACKLOG, &backlog, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_BACKLOG value

int
zsockopt_backlog (void *socket)
{
    int backlog;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_BACKLOG, &backlog, &type_size);
    return backlog;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_MAXMSGSIZE value

void
zsockopt_set_maxmsgsize (void *socket, int maxmsgsize)
{
    int64_t value = maxmsgsize;
    int rc = zmq_setsockopt (socket, ZMQ_MAXMSGSIZE, &value, sizeof (int64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_MAXMSGSIZE value

int
zsockopt_maxmsgsize (void *socket)
{
    int64_t maxmsgsize;
    size_t type_size = sizeof (int64_t);
    zmq_getsockopt (socket, ZMQ_MAXMSGSIZE, &maxmsgsize, &type_size);
    return (int) maxmsgsize;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_SUBSCRIBE value

void
zsockopt_set_subscribe (void *socket, char * subscribe)
{
    int rc = zmq_setsockopt (socket, ZMQ_SUBSCRIBE, subscribe, strlen (subscribe));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_UNSUBSCRIBE value

void
zsockopt_set_unsubscribe (void *socket, char * unsubscribe)
{
    int rc = zmq_setsockopt (socket, ZMQ_UNSUBSCRIBE, unsubscribe, strlen (unsubscribe));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_TYPE value

int
zsockopt_type (void *socket)
{
    int type;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_TYPE, &type, &type_size);
    return type;
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RCVMORE value

int
zsockopt_rcvmore (void *socket)
{
    int rcvmore;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RCVMORE, &rcvmore, &type_size);
    return rcvmore;
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_FD value

int
zsockopt_fd (void *socket)
{
    int fd;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_FD, &fd, &type_size);
    return fd;
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_EVENTS value

int
zsockopt_events (void *socket)
{
    int events;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_EVENTS, &events, &type_size);
    return events;
}


//  --------------------------------------------------------------------------
//  Set socket high-water mark, emulating 2.x API

void
zsockopt_set_hwm (void *socket, int hwm)
{
    zsockopt_set_sndhwm (socket, hwm);
    zsockopt_set_rcvhwm (socket, hwm);
}

#endif

#if (ZMQ_VERSION_MAJOR == 4)
//  --------------------------------------------------------------------------
//  Set socket ZMQ_SNDHWM value

void
zsockopt_set_sndhwm (void *socket, int sndhwm)
{
    int rc = zmq_setsockopt (socket, ZMQ_SNDHWM, &sndhwm, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_SNDHWM value

int
zsockopt_sndhwm (void *socket)
{
    int sndhwm;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_SNDHWM, &sndhwm, &type_size);
    return sndhwm;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RCVHWM value

void
zsockopt_set_rcvhwm (void *socket, int rcvhwm)
{
    int rc = zmq_setsockopt (socket, ZMQ_RCVHWM, &rcvhwm, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RCVHWM value

int
zsockopt_rcvhwm (void *socket)
{
    int rcvhwm;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RCVHWM, &rcvhwm, &type_size);
    return rcvhwm;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_AFFINITY value

void
zsockopt_set_affinity (void *socket, int affinity)
{
    uint64_t value = affinity;
    int rc = zmq_setsockopt (socket, ZMQ_AFFINITY, &value, sizeof (uint64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_AFFINITY value

int
zsockopt_affinity (void *socket)
{
    uint64_t affinity;
    size_t type_size = sizeof (uint64_t);
    zmq_getsockopt (socket, ZMQ_AFFINITY, &affinity, &type_size);
    return (int) affinity;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RATE value

void
zsockopt_set_rate (void *socket, int rate)
{
    int rc = zmq_setsockopt (socket, ZMQ_RATE, &rate, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RATE value

int
zsockopt_rate (void *socket)
{
    int rate;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RATE, &rate, &type_size);
    return rate;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RECOVERY_IVL value

void
zsockopt_set_recovery_ivl (void *socket, int recovery_ivl)
{
    int rc = zmq_setsockopt (socket, ZMQ_RECOVERY_IVL, &recovery_ivl, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RECOVERY_IVL value

int
zsockopt_recovery_ivl (void *socket)
{
    int recovery_ivl;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RECOVERY_IVL, &recovery_ivl, &type_size);
    return recovery_ivl;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_SNDBUF value

void
zsockopt_set_sndbuf (void *socket, int sndbuf)
{
    int rc = zmq_setsockopt (socket, ZMQ_SNDBUF, &sndbuf, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_SNDBUF value

int
zsockopt_sndbuf (void *socket)
{
    int sndbuf;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_SNDBUF, &sndbuf, &type_size);
    return sndbuf;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RCVBUF value

void
zsockopt_set_rcvbuf (void *socket, int rcvbuf)
{
    int rc = zmq_setsockopt (socket, ZMQ_RCVBUF, &rcvbuf, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RCVBUF value

int
zsockopt_rcvbuf (void *socket)
{
    int rcvbuf;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RCVBUF, &rcvbuf, &type_size);
    return rcvbuf;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_LINGER value

void
zsockopt_set_linger (void *socket, int linger)
{
    int rc = zmq_setsockopt (socket, ZMQ_LINGER, &linger, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_LINGER value

int
zsockopt_linger (void *socket)
{
    int linger;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_LINGER, &linger, &type_size);
    return linger;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RECONNECT_IVL value

void
zsockopt_set_reconnect_ivl (void *socket, int reconnect_ivl)
{
    int rc = zmq_setsockopt (socket, ZMQ_RECONNECT_IVL, &reconnect_ivl, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RECONNECT_IVL value

int
zsockopt_reconnect_ivl (void *socket)
{
    int reconnect_ivl;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RECONNECT_IVL, &reconnect_ivl, &type_size);
    return reconnect_ivl;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_RECONNECT_IVL_MAX value

void
zsockopt_set_reconnect_ivl_max (void *socket, int reconnect_ivl_max)
{
    int rc = zmq_setsockopt (socket, ZMQ_RECONNECT_IVL_MAX, &reconnect_ivl_max, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RECONNECT_IVL_MAX value

int
zsockopt_reconnect_ivl_max (void *socket)
{
    int reconnect_ivl_max;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RECONNECT_IVL_MAX, &reconnect_ivl_max, &type_size);
    return reconnect_ivl_max;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_BACKLOG value

void
zsockopt_set_backlog (void *socket, int backlog)
{
    int rc = zmq_setsockopt (socket, ZMQ_BACKLOG, &backlog, sizeof (int));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_BACKLOG value

int
zsockopt_backlog (void *socket)
{
    int backlog;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_BACKLOG, &backlog, &type_size);
    return backlog;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_MAXMSGSIZE value

void
zsockopt_set_maxmsgsize (void *socket, int maxmsgsize)
{
    int64_t value = maxmsgsize;
    int rc = zmq_setsockopt (socket, ZMQ_MAXMSGSIZE, &value, sizeof (int64_t));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_MAXMSGSIZE value

int
zsockopt_maxmsgsize (void *socket)
{
    int64_t maxmsgsize;
    size_t type_size = sizeof (int64_t);
    zmq_getsockopt (socket, ZMQ_MAXMSGSIZE, &maxmsgsize, &type_size);
    return (int) maxmsgsize;
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_SUBSCRIBE value

void
zsockopt_set_subscribe (void *socket, char * subscribe)
{
    int rc = zmq_setsockopt (socket, ZMQ_SUBSCRIBE, subscribe, strlen (subscribe));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Set socket ZMQ_UNSUBSCRIBE value

void
zsockopt_set_unsubscribe (void *socket, char * unsubscribe)
{
    int rc = zmq_setsockopt (socket, ZMQ_UNSUBSCRIBE, unsubscribe, strlen (unsubscribe));
    assert (rc == 0 || errno == ETERM);
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_TYPE value

int
zsockopt_type (void *socket)
{
    int type;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_TYPE, &type, &type_size);
    return type;
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_RCVMORE value

int
zsockopt_rcvmore (void *socket)
{
    int rcvmore;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_RCVMORE, &rcvmore, &type_size);
    return rcvmore;
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_FD value

int
zsockopt_fd (void *socket)
{
    int fd;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_FD, &fd, &type_size);
    return fd;
}


//  --------------------------------------------------------------------------
//  Return socket ZMQ_EVENTS value

int
zsockopt_events (void *socket)
{
    int events;
    size_t type_size = sizeof (int);
    zmq_getsockopt (socket, ZMQ_EVENTS, &events, &type_size);
    return events;
}


//  --------------------------------------------------------------------------
//  Set socket high-water mark, emulating 2.x API

void
zsockopt_set_hwm (void *socket, int hwm)
{
    zsockopt_set_sndhwm (socket, hwm);
    zsockopt_set_rcvhwm (socket, hwm);
}

#endif

//  --------------------------------------------------------------------------
//  Selftest

int
zsockopt_test (Bool verbose)
{
    printf (" * zsockopt: ");

    //  @selftest
    zctx_t *ctx = zctx_new ();
    void *socket;
#if (ZMQ_VERSION_MAJOR == 2)
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_hwm (socket, 1);
    assert (zsockopt_hwm (socket) == 1);
    zsockopt_hwm (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_swap (socket, 1);
    assert (zsockopt_swap (socket) == 1);
    zsockopt_swap (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_affinity (socket, 1);
    assert (zsockopt_affinity (socket) == 1);
    zsockopt_affinity (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_identity (socket, "test");
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_rate (socket, 1);
    assert (zsockopt_rate (socket) == 1);
    zsockopt_rate (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_recovery_ivl (socket, 1);
    assert (zsockopt_recovery_ivl (socket) == 1);
    zsockopt_recovery_ivl (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_recovery_ivl_msec (socket, 1);
    assert (zsockopt_recovery_ivl_msec (socket) == 1);
    zsockopt_recovery_ivl_msec (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_mcast_loop (socket, 1);
    assert (zsockopt_mcast_loop (socket) == 1);
    zsockopt_mcast_loop (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_sndbuf (socket, 1);
    assert (zsockopt_sndbuf (socket) == 1);
    zsockopt_sndbuf (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_rcvbuf (socket, 1);
    assert (zsockopt_rcvbuf (socket) == 1);
    zsockopt_rcvbuf (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_linger (socket, 1);
    assert (zsockopt_linger (socket) == 1);
    zsockopt_linger (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_reconnect_ivl (socket, 1);
    assert (zsockopt_reconnect_ivl (socket) == 1);
    zsockopt_reconnect_ivl (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_reconnect_ivl_max (socket, 1);
    assert (zsockopt_reconnect_ivl_max (socket) == 1);
    zsockopt_reconnect_ivl_max (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_backlog (socket, 1);
    assert (zsockopt_backlog (socket) == 1);
    zsockopt_backlog (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_subscribe (socket, "test");
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_unsubscribe (socket, "test");
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_type (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_rcvmore (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_fd (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_events (socket);
    zsocket_destroy (ctx, socket);
#endif

#if (ZMQ_VERSION_MAJOR == 3)
    socket = zsocket_new (ctx, ZMQ_PUB);
    zsockopt_set_sndhwm (socket, 1);
    assert (zsockopt_sndhwm (socket) == 1);
    zsockopt_sndhwm (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_rcvhwm (socket, 1);
    assert (zsockopt_rcvhwm (socket) == 1);
    zsockopt_rcvhwm (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_affinity (socket, 1);
    assert (zsockopt_affinity (socket) == 1);
    zsockopt_affinity (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_identity (socket, "test");
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_rate (socket, 1);
    assert (zsockopt_rate (socket) == 1);
    zsockopt_rate (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_recovery_ivl (socket, 1);
    assert (zsockopt_recovery_ivl (socket) == 1);
    zsockopt_recovery_ivl (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_PUB);
    zsockopt_set_sndbuf (socket, 1);
    assert (zsockopt_sndbuf (socket) == 1);
    zsockopt_sndbuf (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_rcvbuf (socket, 1);
    assert (zsockopt_rcvbuf (socket) == 1);
    zsockopt_rcvbuf (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_linger (socket, 1);
    assert (zsockopt_linger (socket) == 1);
    zsockopt_linger (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_reconnect_ivl (socket, 1);
    assert (zsockopt_reconnect_ivl (socket) == 1);
    zsockopt_reconnect_ivl (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_reconnect_ivl_max (socket, 1);
    assert (zsockopt_reconnect_ivl_max (socket) == 1);
    zsockopt_reconnect_ivl_max (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_backlog (socket, 1);
    assert (zsockopt_backlog (socket) == 1);
    zsockopt_backlog (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_maxmsgsize (socket, 1);
    assert (zsockopt_maxmsgsize (socket) == 1);
    zsockopt_maxmsgsize (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_subscribe (socket, "test");
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_unsubscribe (socket, "test");
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_type (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_rcvmore (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_fd (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_events (socket);
    zsocket_destroy (ctx, socket);

    zsockopt_set_hwm (socket, 1);
#endif

#if (ZMQ_VERSION_MAJOR == 4)
    socket = zsocket_new (ctx, ZMQ_PUB);
    zsockopt_set_sndhwm (socket, 1);
    assert (zsockopt_sndhwm (socket) == 1);
    zsockopt_sndhwm (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_rcvhwm (socket, 1);
    assert (zsockopt_rcvhwm (socket) == 1);
    zsockopt_rcvhwm (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_affinity (socket, 1);
    assert (zsockopt_affinity (socket) == 1);
    zsockopt_affinity (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_rate (socket, 1);
    assert (zsockopt_rate (socket) == 1);
    zsockopt_rate (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_recovery_ivl (socket, 1);
    assert (zsockopt_recovery_ivl (socket) == 1);
    zsockopt_recovery_ivl (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_PUB);
    zsockopt_set_sndbuf (socket, 1);
    assert (zsockopt_sndbuf (socket) == 1);
    zsockopt_sndbuf (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_rcvbuf (socket, 1);
    assert (zsockopt_rcvbuf (socket) == 1);
    zsockopt_rcvbuf (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_linger (socket, 1);
    assert (zsockopt_linger (socket) == 1);
    zsockopt_linger (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_reconnect_ivl (socket, 1);
    assert (zsockopt_reconnect_ivl (socket) == 1);
    zsockopt_reconnect_ivl (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_reconnect_ivl_max (socket, 1);
    assert (zsockopt_reconnect_ivl_max (socket) == 1);
    zsockopt_reconnect_ivl_max (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_backlog (socket, 1);
    assert (zsockopt_backlog (socket) == 1);
    zsockopt_backlog (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_maxmsgsize (socket, 1);
    assert (zsockopt_maxmsgsize (socket) == 1);
    zsockopt_maxmsgsize (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_subscribe (socket, "test");
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_set_unsubscribe (socket, "test");
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_type (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_rcvmore (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_fd (socket);
    zsocket_destroy (ctx, socket);
    socket = zsocket_new (ctx, ZMQ_SUB);
    zsockopt_events (socket);
    zsocket_destroy (ctx, socket);

    zsockopt_set_hwm (socket, 1);
#endif

    zctx_destroy (&ctx);
    //  @end

    printf ("OK\n");
    return 0;
}
