#!/bin/env python3
""" launch tests from the config, write reports, etc. Control logic """
from pathlib import Path
import logging
import sys
import time
from threading import Thread
from traceback import print_exc
from dmesg import DmesgWatcher, dmesg_runner


def launch_runner(runner, create_report):
    """Manage test execution on our own"""
    dmesg = DmesgWatcher(runner.cfg)
    dmesg_thread = Thread(target=dmesg_runner, args=[dmesg])
    dmesg_thread.start()
    time.sleep(3)
    logging.info(runner.scenarios)
    try:
        logging.info("about to start test")
        runner.testing_runner()
        runner.overload_report_fh.close()
        runner.generate_report_txt("")
        if create_report:
            runner.generate_test_report()
            if not runner.cfg.is_asan:
                runner.generate_crash_report()
    except Exception as exc:
        logging.exception("Caught exception in launch runner")
        runner.success = False
        sys.stderr.flush()
        sys.stdout.flush()
        print(exc, file=sys.stderr)
        print_exc()
    finally:
        sys.stderr.flush()
        sys.stdout.flush()
        runner.create_log_file()
        runner.create_testruns_file()
        dmesg.end_run()
        logging.info("joining dmesg threads")
        dmesg_thread.join()
        runner.print_and_exit_closing_stance()
