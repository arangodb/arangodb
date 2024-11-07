#!/bin/env python3
""" check for resource shortage of the test host """
# pylint: disable=global-statement disable=global-variable-not-assigned
from threading import Thread, Lock
import time
from datetime import datetime
import psutil
# from tools.socket_counter import get_socket_count
from tools.killall import get_all_processes_stats_json

END_THREAD_LOCK = Lock()
END_THREAD = False
OVERLOAD_THREAD = None


def overload_thread(sitecfg, _):
    """watcher thread to track system load"""
    continue_running = True
    # print("starting load monitoring thread")
    with open((sitecfg.base_dir / "overloads.jsonl"), "w+", encoding="utf-8")  as jsonl_file:
        while continue_running:
            #try:
            #    sock_count = get_socket_count()
            #    if sock_count > 8000:
            #        print(f"Socket count high: {sock_count}")
            #except psutil.AccessDenied:
            #    pass
            load = psutil.getloadavg()
            if (load[0] > sitecfg.max_load) or (load[1] > sitecfg.max_load1) or (load[0] > sitecfg.overload):
                #print(f"{str(load)} <= {sitecfg.overload} Load to high - Disk I/O: " + str(psutil.swap_memory()))
                jsonl_file.write(f'["{datetime.now ()}", {get_all_processes_stats_json()}]\n')
            time.sleep(1)
            with END_THREAD_LOCK:
                continue_running = not END_THREAD
    #print("exiting load monitoring thread")


def spawn_overload_watcher_thread(siteconfig):
    """launch the overload watcher thread"""
    global OVERLOAD_THREAD
    OVERLOAD_THREAD = Thread(target=overload_thread, args=(siteconfig, True))
    OVERLOAD_THREAD.start()


def shutdown_overload_watcher_thread():
    """terminate the overload watcher thread"""
    global END_THREAD
    with END_THREAD_LOCK:
        END_THREAD = True
    if OVERLOAD_THREAD is not None:
        OVERLOAD_THREAD.join()
