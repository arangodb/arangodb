#!/bin/env python3
"""
 Count the sockets in use on the system.
 On mac use the suspect processes as permission workaround
 """
import platform
import psutil

IS_MAC = platform.mac_ver()[0] != ""

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
    psutil.CONN_CLOSING
]

def get_socket_count():
    """ get the number of sockets lingering destruction """
    # pylint: disable=too-many-nested-blocks disable=too-many-branches
    counter = 0
    if IS_MAC:
        # Mac would need root for all sockets, so we just look
        # for arangods and their ports, which works without.
        try:
            for proc in psutil.process_iter(['pid', 'name']):
                if proc.name() in ['arangod', 'arangosh']:
                    try:
                        for socket in psutil.Process(proc.pid).connections():
                            if socket.status in INTERESTING_SOCKETS:
                                counter += 1
                    except psutil.ZombieProcess:
                        pass
                    except psutil.NoSuchProcess:
                        pass
        except ProcessLookupError:
            pass
        except psutil.ZombieProcess:
            pass
        except psutil.NoSuchProcess:
            pass
    else:
        for socket in psutil.net_connections(kind='inet'):
            if socket.status in INTERESTING_SOCKETS:
                counter += 1
    return counter
