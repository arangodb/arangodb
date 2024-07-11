#!/bin/env python3
"""
 Count the sockets in use on the system.
 On mac use the suspect processes as permission workaround
 """
import platform
import psutil


INTERESTING_SOCKETS = [
    psutil.CONN_FIN_WAIT1,
    psutil.CONN_FIN_WAIT2,
    psutil.CONN_CLOSE_WAIT,
    psutil.CONN_ESTABLISHED,
    psutil.CONN_SYN_SENT,
    psutil.CONN_SYN_RECV,
    psutil.CONN_TIME_WAIT,
    psutil.CONN_CLOSE,
    psutil.CONN_LAST_ACK,
    psutil.CONN_LISTEN,
    psutil.CONN_CLOSING,
]


def get_socket_count():
    """get the number of sockets lingering destruction"""
    # pylint: disable=too-many-nested-blocks disable=too-many-branches
    counter = 0
    for socket in psutil.net_connections(kind="inet"):
        if socket.status in INTERESTING_SOCKETS:
            counter += 1
    return counter
