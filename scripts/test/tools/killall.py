#!/bin/env python3
""" manipulate processes """
import time
import json
import sys
import logging
import psutil

def list_all_processes():
    """list all processes for later reference"""
    pseaf = "PID  Process"
    # pylint: disable=catching-non-exception
    for process in psutil.process_iter(["pid", "ppid", "name"]):
        if  process.pid in [1, 2] or process.ppid() == 2:
            continue
        cmdline = process.name
        try:
            cmdline = str(process.cmdline())
            if cmdline == "[]":
                cmdline = "[" + process.name() + "]"
        except psutil.AccessDenied:
            pass
        except psutil.ProcessLookupError:
            pass
        except psutil.NoSuchProcess:
            pass
        logging.info("PID: %s PPID: %s %s", process.pid, process.ppid(), cmdline)
    logging.info(pseaf)
    sys.stdout.flush()

def kill_all_arango_processes():
    """list all processes for later reference"""
    # pylint: disable=catching-non-exception
    for process in psutil.process_iter(["pid", "name"]):
        if (
            process.name().lower().find("arango") >= 0
            or process.name().lower().find("tshark") >= 0
        ):
            try:
                logging.info("Main: killing %s - %s", process.name(), str(process.pid))
                process.resume()
            except psutil.NoSuchProcess:
                pass
            except psutil.AccessDenied:
                pass
            try:
                process.kill()
            except psutil.NoSuchProcess:  # pragma: no cover
                pass

def gather_process_thread_statistics(p):
    """ gather the statistics of one process and all its threads """
    ret = {}
    ret['process'] = [{
        'time': time.ctime(),
        'pid': p.pid,
        'ppid': p.ppid(),
        'cmdline': p.cmdline(),
        'name': p.name(),
        'percent': p.cpu_percent(),
        'iocounters': p.io_counters(),
        'ctxSwitches': p.num_ctx_switches(),
        'numfds': p.num_fds(),
        'cpu_times': p.cpu_times(),
        'meminfo': p.memory_full_info(),
        'netcons': p.connections()
    }]
    for t in p.threads():
        ret[ t.id ] = { 'user': t.user_time, 'sys': t.system_time}
    return ret

def add_delta(p1, p2):
    """ calculate and add a delta in cpu and time to all threads of a process """
    tids = list(p1.keys())
    for tid in tids:
        if tid in p2 and tid != 'process':
            p1[tid]['d_user'] = p2[tid]['user'] - p1[tid]['user']
            p1[tid]['d_sys'] = p2[tid]['sys'] - p1[tid]['sys']
    p1['process'].append(p2['process'][0])

def get_all_processes_stats_json(load):
    """ aggregate a structure of all processes and their threads plus delta """
    # pylint: disable=broad-exception-caught
    process_full_list = {}
    process_full_list['sys'] = {
        'load': load,
        'vmem': psutil.virtual_memory(),
        'mem': psutil.swap_memory(),
        'diskio': psutil.disk_io_counters(perdisk=True, nowrap=True),
        'netio': psutil.net_io_counters(pernic=True, nowrap=True),
    }
    processes = psutil.process_iter()
    for process in processes:
        name = ""
        try:
            name = process.name()
            if  process.pid not in [1, 2] and process.ppid() != 2:
                procstat = gather_process_thread_statistics(process)
                process_full_list[f"p{process.pid}"] = procstat
        except psutil.AccessDenied:
            pass
        except Exception as ex:
            print(f"while inspecting {name}: {ex} ")
    return json.dumps(process_full_list)
