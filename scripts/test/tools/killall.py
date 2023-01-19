#!/bin/env python3
""" manipulate processes """
import sys
import psutil

def list_all_processes():
    """list all processes for later reference"""
    pseaf = "PID  Process"
    # pylint: disable=catching-non-exception
    for process in psutil.process_iter(["pid", "ppid", "name"]):
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
        print(f"PID: {process.pid} PPID: {process.ppid()} {cmdline}")
    print(pseaf)
    sys.stdout.flush()

def kill_all_arango_processes():
    """list all processes for later reference"""
    pseaf = "PID  Process"
    # pylint: disable=catching-non-exception
    for process in psutil.process_iter(["pid", "name"]):
        if (process.name().lower().find('arango') >= 0 or
            process.name().lower().find('tshark') >= 0):
            try:
                print(f"Main: killing {process.name()} - {str(process.pid)}")
                process.resume()
            except psutil.NoSuchProcess:
                pass
            except psutil.AccessDenied:
                pass
            try:
                process.kill()
            except psutil.NoSuchProcess:  # pragma: no cover
                pass
