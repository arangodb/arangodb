#!/bin/env python3
""" check which resources we have on the test host """
from datetime import datetime, timedelta
import logging
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import sys

import psutil
from socket_counter import get_socket_count

IS_ARM = platform.processor() == "arm" or platform.processor() == "aarch64"


def sigint_boomerang_handler(signum, frame):
    """do the right thing to behave like linux does"""
    # pylint: disable=unused-argument
    if signum != signal.SIGINT:
        sys.exit(1)
    # pylint: disable=unnecessary-pass
    pass


def get_workspace():
    """evaluates the directory to put reports to"""
    if "INNERWORKDIR" in os.environ:
        workdir = Path(os.environ["INNERWORKDIR"])
        if workdir.exists():
            return workdir
    if "WORKDIR" in os.environ:
        workdir = Path(os.environ["WORKDIR"])
        if workdir.exists():
            return workdir
    # if 'WORKSPACE' in os.environ:
    #    workdir = Path(os.environ['WORKSPACE'])
    #    if workdir.exists():
    #        return workdir
    return Path.cwd() / "work"


ansi_escape = re.compile(r"\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])")
for env in os.environ:
    logging.info("%s=%s", env, ansi_escape.sub("", os.environ[env]))
TEMP = Path("/tmp/")
if "TMP" in os.environ:
    TEMP = Path(os.environ["TMP"])
if "TEMP" in os.environ:
    TEMP = Path(os.environ["TEMP"])
if "TMP" in os.environ:
    TEMP = Path(os.environ["TMP"])
if "INNERWORKDIR" in os.environ:
    TEMP = Path(os.environ["INNERWORKDIR"])
    wd = TEMP / "ArangoDB"
    wd.cwd()
    TEMP = TEMP / "tmp"
else:
    TEMP = TEMP / "ArangoDB"
if TEMP.exists():
    # pylint: disable=broad-except
    try:
        shutil.rmtree(TEMP)
        TEMP.mkdir(parents=True)
    except Exception as ex:
        msg = f"failed to clean temporary directory: {ex} - won't launch tests!"
        (get_workspace() / "testfailures.txt").write_text(msg + "\n")
        logging.info(msg)
        sys.exit(2)
else:
    TEMP.mkdir(parents=True)
os.environ["TMPDIR"] = str(TEMP)
os.environ["TEMP"] = str(TEMP)
os.environ["TMP"] = str(TEMP)


class SiteConfig:
    """this environment - adapted to oskar defaults"""

    # pylint: disable=too-few-public-methods disable=too-many-instance-attributes
    def __init__(self, base_source_dir, build_dir):
        # pylint: disable=too-many-statements disable=too-many-branches
        self.datetime_format = "%Y-%m-%dT%H%M%SZ"
        self.trace = False
        self.portbase = 7000
        if "PORTBASE" in os.environ:
            self.portbase = int(os.environ["PORTBASE"])
        self.port_offset = 100
        self.timeout = 1800
        if "timeLimit".upper() in os.environ:
            self.timeout = int(os.environ["timeLimit".upper()])
        elif "timeLimit" in os.environ:
            self.timeout = int(os.environ["timeLimit"])

        san_mode = os.environ.get("SAN_MODE") or "<na>"
        if san_mode == "alubsan":
            self.timeout *= 3
        elif san_mode == "tsan":
            self.timeout *= 6

        self.small_machine = False
        self.extra_args = []
        if psutil.cpu_count(logical=False) <= 12:
            logging.info(
                "Small machine detected, quadrupling deadline, disabling buckets!"
            )
            self.small_machine = True
            self.port_offset = 400
            self.timeout *= 4
        self.no_threads = psutil.cpu_count()
        self.available_slots = round(self.no_threads * 2)  # logical=False)
        self.max_load = self.no_threads * 0.9
        self.max_load1 = self.no_threads * 1.1
        # roughly increase 1 per ten cores
        self.core_dozend = round(self.no_threads / 10)
        if self.core_dozend == 0:
            self.core_dozend = 1
        self.loop_sleep = round(5 / self.core_dozend)
        self.overload = self.max_load * 1.4
        self.slots_to_parallelity_factor = self.max_load / self.available_slots
        self.rapid_fire = round(self.available_slots / 10)
        self.is_asan = "SAN" in os.environ and os.environ["SAN"] == "On"
        self.is_aulsan = self.is_asan and os.environ["SAN_MODE"] == "AULSan"
        self.is_gcov = "COVERAGE" in os.environ and os.environ["COVERAGE"] == "On"
        logging.info("Timeout is set to %d - san_mode is %s", self.timeout, san_mode)
        san_gcov_msg = ""
        if self.is_asan or self.is_gcov:
            san_gcov_msg = " - SAN "
            slot_divisor = 4
            if self.is_aulsan:
                san_gcov_msg = " - AUL-SAN "
            elif self.is_gcov:
                san_gcov_msg = " - GCOV"
                slot_divisor = 3
            san_gcov_msg += " enabled, reducing possible system capacity\n"
            self.rapid_fire = 1
            self.available_slots /= slot_divisor
            # self.timeout *= 1.5
            self.loop_sleep *= 2
            self.max_load /= 2
        self.deadline = datetime.now() + timedelta(seconds=self.timeout)
        self.hard_deadline = datetime.now() + timedelta(seconds=self.timeout + 660)
        bin_dir = (build_dir / "bin").resolve()
        socket_count = "was not allowed to see socket counts!"
        try:
            socket_count = str(get_socket_count())
        except psutil.AccessDenied:
            pass

        logging.info(
            f"""Machine Info [{psutil.Process().pid}]:
 - {psutil.cpu_count(logical=False)} Cores / {psutil.cpu_count(logical=True)} Threads
 - {platform.processor()} processor architecture
 - {psutil.virtual_memory()} virtual Memory
 - {self.slots_to_parallelity_factor} parallelity to load estimate factor
 - {self.overload} load1 threshhold for overload logging
 - {self.max_load} / {self.max_load1} configured maximum load 0 / 1
 - {self.slots_to_parallelity_factor} parallelity to load estimate factor
 - {self.available_slots} test slots {self.rapid_fire} rapid fire slots
 - {str(TEMP)} - temporary directory
 - current Disk I/O: {str(psutil.disk_io_counters())}
 - current Swap: {str(psutil.swap_memory())}
 - Starting {str(datetime.now())} soft deadline will be: {str(self.deadline)} hard deadline will be: {str(self.hard_deadline)}
 - {self.core_dozend} / {self.loop_sleep} machine size / loop frequency
 - {socket_count} number of currently active tcp sockets
{san_gcov_msg}"""
        )
        self.cfgdir = base_source_dir / "etc" / "testing"
        self.build_dir = build_dir
        self.bin_dir = bin_dir
        self.base_path = base_source_dir
        self.test_data_dir = base_source_dir
        self.passvoid = ""
        self.run_root = base_source_dir / "testrun"
        if self.run_root.exists():
            shutil.rmtree(self.run_root)
        self.xml_report_dir = base_source_dir / "testrunXml"
        if self.xml_report_dir.exists():
            shutil.rmtree(self.xml_report_dir)
        self.xml_report_dir.mkdir(parents=True)
        self.test_data_dir_x = self.run_root / "run"
        self.test_data_dir_x.mkdir(parents=True)
        self.test_report_dir = self.run_root / "report"
        self.test_report_dir.mkdir(parents=True)
        self.portbase = 7000
        if "PORTBASE" in os.environ:
            self.portbase = int(os.environ["PORTBASE"])

    def get_overload(self):
        """estimate whether the system is overloaded"""
        load = psutil.getloadavg()
        if load[0] > self.overload:
            return f"HIGH SYSTEM LOAD! {load[0]:9.2f} > {self.overload:9.2f} "
        return None
