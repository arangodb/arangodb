#!/bin/env python3
""" launch tests from the config, write reports, etc. Control logic """
from pathlib import Path
import sys
import time
from threading  import Thread
from traceback import print_exc

from dmesg import DmesgWatcher, dmesg_runner
from site_config import SiteConfig, IS_LINUX
from testing_runner import TestingRunner

# pylint: disable=broad-except
def launch(args, tests):
    runner = None
    try:
        runner = TestingRunner(SiteConfig(Path(args.definitions).resolve()))
        for test in tests:
            runner.register_test_func(args.cluster, test)
        runner.sort_by_priority()
    except Exception as exc:
        print(exc)
        raise exc
    create_report = True
    if args.no_report:
        print("won't generate report as you demanded!")
        create_report = False
    launch_runner(runner, create_report)


def launch_runner(runner, create_report):
    """ Manage test execution on our own """
    dmesg = DmesgWatcher(runner.cfg)
    if IS_LINUX:
        dmesg_thread = Thread(target=dmesg_runner, args=[dmesg])
        dmesg_thread.start()
        time.sleep(3)
    print(runner.scenarios)
    try:
        print("about to start test")
        runner.testing_runner()
        runner.overload_report_fh.close()
        runner.generate_report_txt("")
        if create_report:
            runner.generate_test_report()
            if not runner.cfg.is_asan:
                runner.generate_crash_report()
    except Exception as exc:
        print("Caught exception in launch runner")
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
        if IS_LINUX:
            dmesg.end_run()
            print('joining dmesg threads')
            dmesg_thread.join()
        runner.print_and_exit_closing_stance()
