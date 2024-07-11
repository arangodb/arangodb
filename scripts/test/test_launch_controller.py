#!/bin/env python3
""" read test definition, and generate the output for the specified target """
import logging
import argparse
import sys
from pathlib import Path
from traceback import print_exc

logging.basicConfig(
    filename="work/test_launcher.log",
    filemode="a",
    format="%(asctime)s [%(levelname)s] %(message)s",
    level=logging.DEBUG,
)

from site_config import SiteConfig
from testing_runner import TestingRunner
from test_config import TestConfig
from launch_handler import launch_runner

# check python 3
if sys.version_info[0] != 3:
    print("found unsupported python version ", sys.version_info)
    sys.exit()

# pylint: disable=line-too-long disable=broad-except disable=chained-comparison


def str2bool(value):
    if isinstance(value, bool):
        return value
    if value.lower() in ("yes", "true", "t", "y", "1"):
        return True
    if value.lower() in ("no", "false", "f", "n", "0"):
        return False
    raise argparse.ArgumentTypeError("Boolean value expected.")


def parse_arguments():
    """argv"""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "suites", help="comma separated list of suites to run", type=str
    )
    parser.add_argument("--testBuckets", help="", type=str)
    parser.add_argument(
        "--cluster", type=str2bool, nargs="?", const=True, default=False, help=""
    )
    parser.add_argument("--extraArgs", help="", type=str)
    parser.add_argument("--suffix", help="", type=str)
    parser.add_argument("--build", help="build folder", type=str)
    return parser.parse_args()


def main():
    """entrypoint"""
    try:
        args = parse_arguments()
        suite = args.suites
        extra_args = args.extraArgs.split()
        suffix = args.suffix
        name = suite
        if suffix:
            name += f"_{suffix}"
        if args.cluster:
            extra_args += ["--cluster", "true", "--dumpAgencyOnError", "true"]
        extra_args += ["--testBuckets", args.testBuckets]
        base_source_dir = Path(".").resolve()
        build_dir = (
            Path(args.build).resolve()
            if args.build is not None
            else base_source_dir / "build"
        )
        runner = TestingRunner(SiteConfig(base_source_dir, build_dir))
        runner.scenarios.append(
            TestConfig(runner.cfg, name, suite, [*extra_args], 1, 1, [])
        )
        launch_runner(runner, True)
    except Exception as exc:
        print(exc, file=sys.stderr)
        print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
