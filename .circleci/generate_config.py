#!/bin/env python3
""" read test definition, and generate the output for the specified target """
import argparse
import os
import sys
import traceback
import yaml

# check python 3
if sys.version_info[0] != 3:
    print("found unsupported python version ", sys.version_info)
    sys.exit()

# pylint: disable=line-too-long disable=broad-except disable=chained-comparison
IS_COVERAGE = 'COVERAGE' in os.environ and os.environ['COVERAGE'] == 'On'

known_flags = {
    "cluster": "this test requires a cluster",
    "single": "this test requires a single server",
    "full": "this test is only executed in full tests",
    "!full": "this test is only executed in non-full tests",
    "gtest": "only the gtest are to be executed",
    "sniff": "whether tcpdump / ngrep should be used",
    "ldap": "ldap",
    "enterprise": "this tests is only executed with the enterprise version",
    "!windows": "test is excluded from ps1 output",
    "!mac": "test is excluded when launched on MacOS",
    "!arm": "test is excluded when launched on Arm Linux/MacOS hosts",
    "!coverage": "test is excluded when coverage scenario are ran",
    "no_report": "disable reporting",
}

known_parameter = {
    "name": "name of the test suite",
    "buckets": "number of buckets to use for this test",
    "suffix": "suffix that is appended to the tests folder name",
    "priority": "priority that controls execution order. Testsuites with lower priority are executed later",
    "parallelity": "parallelity how many resources will the job use in the SUT? Default: 1 in Single server, 4 in Clusters",
    "size": "docker container size to be used in CircleCI",
}


def print_help_flags():
    """print help for flags"""
    print("Flags are specified as a single token.")
    for flag, exp in known_flags.items():
        print(f"{flag}: {exp}")

    print("Parameter have a value and specified as param=value.")
    for flag, exp in known_parameter.items():
        print(f"{flag}: {exp}")


def parse_arguments():
    """argv"""
    if "--help-flags" in sys.argv:
        print_help_flags()
        sys.exit()

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "base_config", help="file containing the circleci base config", type=str
    )
    parser.add_argument(
        "definitions", help="file containing the test definitions", type=str
    )
    parser.add_argument("-o", "--output", type=str, help="filename of the output")
    parser.add_argument(
        "--validate-only",
        help="validates the test definition file",
        action="store_true",
    )
    parser.add_argument(
        "--help-flags",
        help="prints information about available flags and exits",
        action="store_true",
    )
    parser.add_argument(
        "--single", help="output single server tests", action="store_true"
    )
    parser.add_argument("--cluster", help="output cluster tests", action="store_true")
    parser.add_argument("--gtest", help="only run gtest", action="store_true")
    parser.add_argument("--full", help="output full test set", action="store_true")
    parser.add_argument(
        "--all", help="output all test, ignore other filters", action="store_true"
    )
    return parser.parse_args()


def validate_params(params):
    """check for argument validity"""

    def parse_number(value):
        """check value"""
        try:
            return int(value)
        except Exception as exc:
            raise Exception(f"invalid numeric value: {value}") from exc

    def parse_number_or_default(key, default_value=None):
        """check number"""
        if key in params:
            if params[key][0] == "*":  # factor the default
                params[key] = default_value * parse_number(params[key][1:])
            else:
                params[key] = parse_number(params[key])
        elif default_value is not None:
            params[key] = default_value

    parse_number_or_default("buckets")

    return params


def validate_flags(flags):
    """check whether target flags are valid"""
    if "cluster" in flags and "single" in flags:
        raise Exception("`cluster` and `single` specified for the same test")
    if "full" in flags and "!full" in flags:
        raise Exception("`full` and `!full` specified for the same test")


def read_definition_line(line):
    """parse one test definition line"""
    bits = line.split()
    if len(bits) < 1:
        raise Exception("expected at least one argument: <testname>")
    suites, *remainder = bits

    flags = []
    params = {}
    args = []

    for idx, bit in enumerate(remainder):
        if bit == "--":
            args = remainder[idx + 1 :]
            break

        if "=" in bit:
            key, value = bit.split("=", maxsplit=1)
            params[key] = value
        else:
            flags.append(bit)

    # check all flags
    for flag in flags:
        if flag not in known_flags:
            raise Exception(f"Unknown flag `{flag}` in `{line}`")

    # check all params
    for param in params:
        if param not in known_parameter:
            raise Exception(f"Unknown parameter `{param}` in `{line}`")

    validate_flags(flags)
    is_cluster = "cluster" in flags
    params = validate_params(params)

    return {
        "name": params.get("name", suites),
        "suites": suites,
        "size": params.get("size", "medium" if is_cluster else "small"),
        "flags": flags,
        "args": args,
        "params": params,
    }


def read_definitions(filename):
    """read test definitions txt"""
    tests = []
    has_error = False
    with open(filename, "r", encoding="utf-8") as filep:
        for line_no, line in enumerate(filep):
            line = line.strip()
            if line.startswith("#") or len(line) == 0:
                continue  # ignore comments
            try:
                test = read_definition_line(line)
                test["lineNumber"] = line_no
                tests.append(test)
            except Exception as exc:
                print(f"{filename}:{line_no + 1}: \n`{line}`\n {exc}", file=sys.stderr)
                has_error = True
    if has_error:
        raise Exception("abort due to errors")
    return tests


def filter_tests(args, tests, enterprise):
    """filter testcase by operations target Single/Cluster/full"""
    if args.all:
        return tests

    filters = []
    # if args.cluster:
    #     filters.append(lambda test: "single" not in test["flags"])
    # else:
    #     filters.append(lambda test: "cluster" not in test["flags"])

    if args.full:
        filters.append(lambda test: "!full" not in test["flags"])
    else:
        filters.append(lambda test: "full" not in test["flags"])

    # if args.gtest:
    #     filters.append(lambda test: "gtest" == test["name"])
    #else:
    #   filters.append(lambda test: "gtest" != test["name"])

    if not enterprise:
        filters.append(lambda test: "enterprise" not in test["flags"])

    # if IS_ARM:
    #     filters.append(lambda test: "!arm" not in test["flags"])

    if IS_COVERAGE:
        filters.append(lambda test: "!coverage" not in test["flags"])

    for one_filter in filters:
        tests = filter(one_filter, tests)
    return list(tests)


def create_test_job(test, cluster, edition):
    """creates the test job definition to be put into the config yaml"""
    params = test["params"]
    suite_name = test["name"]
    suffix = params.get("suffix", "")
    if suffix:
        suite_name += f"-{suffix}"

    if not test["size"] in ["small", "medium", "medium+", "large", "xlarge"]:
        raise Exception("Invalid resource class size " + test["size"])

    result = {
        "name": f"test-{edition}-{'cluster' if cluster else 'single'}-{suite_name}",
        "suiteName": suite_name,
        "suites": test["suites"],
        "size": test["size"],
        "cluster": cluster,
        "requires": [f"build-{edition}"],
    }

    extra_args = test["args"]
    if extra_args != []:
        result["extraArgs"] = " ".join(extra_args)

    buckets = params.get("buckets", 1)
    if buckets != 1:
        result["buckets"] = buckets

    return result


def generate_output(config, tests, enterprise):
    """generate output"""
    workflow = "enterprise-pr" if enterprise else "community-pr"
    edition = "ee" if enterprise else "ce"
    jobs = config["workflows"][workflow]["jobs"]
    for test in tests:
        print(f"test: {test}")
        if "cluster" in test["flags"]:
            jobs.append({"run-tests": create_test_job(test, True, edition)})
        elif "single" in test["flags"]:
            jobs.append({"run-tests": create_test_job(test, False, edition)})
        else:
            jobs.append({"run-tests": create_test_job(test, True, edition)})
            jobs.append({"run-tests": create_test_job(test, False, edition)})


def generate_jobs(config, args, tests, enterprise):
    """generate job definitions"""
    tests = filter_tests(args, tests, enterprise)
    generate_output(config, tests, enterprise)


def main():
    """entrypoint"""
    try:
        args = parse_arguments()
        tests = read_definitions(args.definitions)
        # if args.validate_only:
        #    return  # nothing left to do
        print("args", args)
        with open(args.base_config, "r", encoding="utf-8") as instream:
            with open(args.output, "w", encoding="utf-8") as outstream:
                config = yaml.safe_load(instream)
                generate_jobs(config, args, tests, False) # community
                generate_jobs(config, args, tests, True) # enterprise
                yaml.dump(config, outstream)
    except Exception as exc:
        traceback.print_exc(exc, file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
