#!/bin/env python3
""" read test definition, and generate the output for the specified target """
import argparse
import sys
from pathlib import Path
from traceback import print_exc

from site_config import SiteConfig
from testing_runner import TestingRunner
from test_config import TestConfig 

# check python 3
if sys.version_info[0] != 3:
    print("found unsupported python version ", sys.version_info)
    sys.exit()

#pylint: disable=line-too-long disable=broad-except disable=chained-comparison

def str2bool(v):
    if isinstance(v, bool):
        return v
    if v.lower() in ('yes', 'true', 't', 'y', '1'):
        return True
    elif v.lower() in ('no', 'false', 'f', 'n', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')

def parse_arguments():
    """ argv """
    parser = argparse.ArgumentParser()
    parser.add_argument("suites", help="comma separated list of suites to run", type=str)
    parser.add_argument("--testBuckets", help="", type=str)
    parser.add_argument("--cluster", type=str2bool, nargs='?', const=True, default=False, help="")
    parser.add_argument("--extraArgs", help="", type=str)
    parser.add_argument("--suffix", help="", type=str)
    parser.add_argument("--definitions", help="path to the test definitions file", type=str)
    return parser.parse_args()


def main():
    """ entrypoint """
    try:
        args = parse_arguments()
        print(args)
        suite = args.suites
        extraArgs = args.extraArgs.split()
        print(extraArgs)
        suffix = args.suffix
        name = suite
        if suffix:
            name += f"_{suffix}"
        if args.cluster:
            extraArgs += ['--cluster', 'true', '--dumpAgencyOnError', 'true']
        extraArgs += ['--testBuckets', args.testBuckets]
        print(extraArgs)
        runner = TestingRunner(SiteConfig(Path(args.definitions).resolve()))
        runner.scenarios.append(
                TestConfig(runner.cfg,
                        name,
                        suite,
                        [*extraArgs],
                        1,
                        1,
                        []))
        try:
            runner.testing_runner()
            runner.overload_report_fh.close()
            runner.generate_report_txt("")
            runner.generate_test_report()
            if not runner.cfg.is_asan:
                runner.generate_crash_report()
        except Exception as exc:
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
            #if IS_LINUX:
                #dmesg.end_run()
                #print('joining dmesg threads')
                #dmesg_thread.join()
            runner.print_and_exit_closing_stance()
    except Exception as exc:
        print(exc, file=sys.stderr)
        print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
