#!/bin/env python3
""" read test definition, and generate the output for the specified target """
from collections import namedtuple
from datetime import date
import argparse
import os
import re
import sys
import traceback
import yaml

BuildConfig = namedtuple(
    "BuildConfig", ["arch", "enterprise", "sanitizer", "isNightly"]
)

# check python 3
if sys.version_info[0] != 3:
    print("found unsupported python version ", sys.version_info)
    sys.exit()

# pylint: disable=line-too-long disable=broad-except disable=chained-comparison
IS_COVERAGE = "COVERAGE" in os.environ and os.environ["COVERAGE"] == "On"

known_flags = {
    "cluster": "this test requires a cluster",
    "single": "this test requires a single server",
    "full": "this test is only executed in full tests",
    "!full": "this test is only executed in non-full tests",
    "gtest": "only the gtest are to be executed",
    "sniff": "whether tcpdump / ngrep should be used",
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
    parser.add_argument("-s", "--sanitizer", type=str, help="sanitizer to use")
    parser.add_argument(
        "--ui", type=str, help="whether to run UI test [off|on|only|community]"
    )
    parser.add_argument(
        "--ui-testsuites", type=str, help="which test of UI job to run [off|on|only]"
    )
    parser.add_argument(
        "--ui-deployments", type=str, help="which deployments [CL, SG, ...] to run"
    )
    parser.add_argument(
        "--rta-branch", type=str, help="which branch for the ui tests to check out"
    )
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
    parser.add_argument(
        "-rt",
        "--replication_two",
        default=False,
        action="store_true",
        help="flag if we should enable replication two tests",
    )
    parser.add_argument(
        "--nightly",
        default=False,
        action="store_true",
        help="flag whether this is a nightly build",
    )
    parser.add_argument(
        "--create-docker-images",
        default=False,
        action="store_true",
        help="flag whether we want to create docker images based on the build results",
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


def filter_tests(args, tests, enterprise, nightly):
    """filter testcase by operations target Single/Cluster/full"""
    if args.all:
        return tests

    full = args.full or nightly
    filters = []
    # if args.cluster:
    #     filters.append(lambda test: "single" not in test["flags"])
    # else:
    #     filters.append(lambda test: "cluster" not in test["flags"])

    if full:
        filters.append(lambda test: "!full" not in test["flags"])
    else:
        filters.append(lambda test: "full" not in test["flags"])

    # if args.gtest:
    #     filters.append(lambda test: "gtest" == test["name"])
    # else:
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


def get_size(size, arch):
    aarch64_sizes = {
        "small": "arm.medium",
        "medium": "arm.medium",
        "medium+": "arm.large",
        "large": "arm.large",
        "xlarge": "arm.xlarge",
        "2xlarge": "arm.2xlarge",
    }
    x86_sizes = {
        "small": "small",
        "medium": "medium",
        "medium+": "medium+",
        "large": "large",
        "xlarge": "xlarge",
        "2xlarge": "2xlarge",
    }
    return aarch64_sizes[size] if arch == "aarch64" else x86_sizes[size]


def get_test_size(size, build_config, cluster):
    if build_config.sanitizer != "":
        # sanitizer builds need more resources
        if size == "small":
            size = "xlarge" if build_config.sanitizer == "tsan" and cluster else "large"
        elif size in ["medium", "medium+", "large"]:
            size = "xlarge"
    return get_size(size, build_config.arch)


def create_test_job(test, cluster, build_config, build_jobs, replication_version=1):
    """creates the test job definition to be put into the config yaml"""
    edition = "ee" if build_config.enterprise else "ce"
    params = test["params"]
    suite_name = test["name"]
    suffix = params.get("suffix", "")
    if suffix:
        suite_name += f"-{suffix}"

    size = test["size"]
    if not size in ["small", "medium", "medium+", "large", "xlarge", "2xlarge"]:
        raise Exception(f"Invalid resource class size {size}")

    deployment_variant = (
        f"cluster{'-repl2' if replication_version==2 else ''}" if cluster else "single"
    )

    job = {
        "name": f"test-{edition}-{deployment_variant}-{suite_name}-{build_config.arch}",
        "suiteName": suite_name,
        "suites": test["suites"],
        "size": get_test_size(size, build_config, cluster),
        "cluster": cluster,
        "requires": build_jobs,
    }
    if suite_name == "chaos" and build_config.isNightly:
        # nightly chaos tests runs 32 different combinations, each running for 3 min plus some time to check for consistency
        job["timeLimit"] = 32 * 5 * 60

    if suite_name == "shell_client_aql" and build_config.isNightly and not cluster:
        # nightly single shell_client_aql suite runs some chaos tests that require more memory, so beef up the size
        job["size"] = get_test_size("medium+", build_config, cluster)

    extra_args = test["args"].copy()
    if cluster:
        extra_args.append(f"--replicationVersion {replication_version}")
    if build_config.isNightly:
        extra_args.append(f"--skipNightly false")
    if extra_args != []:
        job["extraArgs"] = " ".join(extra_args)

    buckets = params.get("buckets", 1)
    if suite_name == "replication_sync":
        # Note: for the replication_sync suite the test-definition.txt specifies only 2 buckets,
        # because increasing this causes issues in Jenkins. However, in CircleCI we definitely
        # want to use more buckets, especially when running with sanitizers.
        buckets = 5

    if buckets != 1:
        job["buckets"] = buckets

    return {"run-linux-tests": job}


def create_rta_test_job(build_config, build_jobs, deployment_mode, filter_statement, rta_branch):
    edition = "ee" if build_config.enterprise else "ce"
    job = {
        "name": f"test-{filter_statement}-{edition}-{deployment_mode}-UI",
        "suiteName": filter_statement,
        "deployment": deployment_mode,
        "browser": "Remote_CHROME",
        "enterprise": "EP" if build_config.enterprise else "C",
        "filterStatement": f"--ui-include-test-suite {filter_statement}",
        "requires": build_jobs,
        "rta-branch": rta_branch,
    }
    return {"run-rta-tests": job}


def add_test_definition_jobs_to_workflow(
    workflow, tests, build_config, build_jobs, repl2
):
    jobs = workflow["jobs"]
    for test in tests:
        if "cluster" in test["flags"]:
            jobs.append(create_test_job(test, True, build_config, build_jobs))
            if repl2:
                jobs.append(create_test_job(test, True, build_config, build_jobs, 2))
        elif "single" in test["flags"]:
            jobs.append(create_test_job(test, False, build_config, build_jobs))
        else:
            jobs.append(create_test_job(test, True, build_config, build_jobs))
            if repl2:
                jobs.append(create_test_job(test, True, build_config, build_jobs, 2))
            jobs.append(create_test_job(test, False, build_config, build_jobs))


def add_rta_ui_test_jobs_to_workflow(args, workflow, build_config, build_jobs):
    jobs = workflow["jobs"]
    ui_testsuites = [
        "UserPageTestSuite",
        "CollectionsTestSuite",
        "ViewsTestSuite",
        "GraphTestSuite",
        "QueryTestSuite",
        "AnalyzersTestSuite",
        "DatabaseTestSuite",
        "LogInTestSuite",
        "DashboardTestSuite",
        "SupportTestSuite",
        "ServiceTestSuite",
    ]
    if args.ui_testsuites != "":
        ui_testsuites = args.ui_testsuites.split(",")
    deployments = [
        "SG",
        "CL",
    ]
    if args.ui_deployments:
        deployments = args.ui_deployments.split(",")

    for deployment in deployments:
        for test_suite in ui_testsuites:
            jobs.append(
                create_rta_test_job(build_config, build_jobs, deployment, test_suite, args.rta_branch)
            )


def add_test_jobs_to_workflow(args, workflow, tests, build_config, build_jobs, repl2):
    if build_config.arch == "x64" and args.ui != "" and args.ui != "off":
        add_rta_ui_test_jobs_to_workflow(args, workflow, build_config, build_jobs)
    if args.ui == "only":
        return
    if build_config.enterprise:
        workflow["jobs"].append(
            {
                "run-hotbackup-tests": {
                    "name": f"run-hotbackup-tests-{build_config.arch}",
                    "size": get_test_size("medium", build_config, True),
                    "requires": build_jobs,
                }
            }
        )
    add_test_definition_jobs_to_workflow(
        workflow, tests, build_config, build_jobs, repl2
    )


def add_cppcheck_job(workflow, build_job):
    workflow["jobs"].append(
        {
            "run-cppcheck": {
                "name": "cppcheck",
                "requires": [build_job],
            }
        }
    )


def add_create_docker_image_job(workflow, build_config, build_jobs, args):
    if not args.create_docker_images:
        return
    edition = "ee" if build_config.enterprise else "ce"
    arch = "amd64" if build_config.arch == "x64" else "arm64"
    image = (
        "public.ecr.aws/b0b8h2r4/enterprise-preview"
        if build_config.enterprise
        else "public.ecr.aws/b0b8h2r4/arangodb-preview"
    )
    branch = os.environ.get("CIRCLE_BRANCH", "unknown-brach")
    match = re.fullmatch("(.+\/)?(.+)", branch)
    if match:
        branch = match.group(2)

    sha1 = os.environ.get("CIRCLE_SHA1")
    if sha1 is None:
        sha1 = "unknown-sha1"
    else:
        sha1 = sha1[:7]
    tag = f"{date.today()}-{branch}-{sha1}-{arch}"
    workflow["jobs"].append(
        {
            "create-docker-image": {
                "name": f"create-{edition}-{build_config.arch}-docker-image",
                "resource-class": get_size("large", build_config.arch),
                "arch": arch,
                "tag": f"{image}:{tag}",
                "requires": build_jobs,
            }
        }
    )


def add_build_job(workflow, build_config, overrides=None):
    edition = "ee" if build_config.enterprise else "ce"
    preset = "enterprise-pr" if build_config.enterprise else "community-pr"
    if build_config.sanitizer != "":
        preset += f"-{build_config.sanitizer}"
    suffix = "" if build_config.sanitizer == "" else f"-{build_config.sanitizer}"
    name = f"build-{edition}-{build_config.arch}{suffix}"
    params = {
        "context": [
            "sccache-aws-bucket"
        ],  # add the environment variables to setup sccache for the S3 bucket
        "resource-class": get_size("2xlarge", build_config.arch),
        "name": name,
        "preset": preset,
        "enterprise": build_config.enterprise,
        "arch": build_config.arch,
    }
    if build_config.arch == "aarch64":
        params["s3-prefix"] = "aarch64"

    workflow["jobs"].append({"compile-linux": params | (overrides or {})})
    return name


def add_frontend_build_job(workflow, build_config, overrides=None):
    edition = "ee" if build_config.enterprise else "ce"
    preset = "enterprise-pr" if build_config.enterprise else "community-pr"
    if build_config.sanitizer != "":
        preset += f"-{build_config.sanitizer}"
    suffix = "" if build_config.sanitizer == "" else f"-{build_config.sanitizer}"
    name = f"build-{edition}-{build_config.arch}{suffix}-frontend"
    workflow["jobs"].append({"build-frontend": {"name": name}})
    return name


def add_workflow(workflows, tests, build_config, args):
    repl2 = args.replication_two
    suffix = "nightly" if build_config.isNightly else "pr"
    if build_config.arch == "x64" and args.ui != "" and args.ui != "off":
        ui = True
        if args.ui == "only":
            suffix = "only_ui_tests-" + suffix
        else:
            suffix = "with_ui_tests-" + suffix
    else:
        ui = False
        suffix = "no_ui_tests-" + suffix
    if build_config.sanitizer != "":
        suffix += "-" + build_config.sanitizer
    if args.replication_two:
        suffix += "-repl2"
    edition = "enterprise" if build_config.enterprise else "community"
    name = f"{build_config.arch}-{edition}-{suffix}"
    workflows[name] = {"jobs": []}
    workflow = workflows[name]
    build_job = add_build_job(workflow, build_config)
    frontend_job = add_frontend_build_job(workflow, build_config)
    build_jobs = [build_job, frontend_job]
    if build_config.arch == "x64" and not ui:
        add_cppcheck_job(workflow, build_job)
    add_create_docker_image_job(workflow, build_config, build_jobs, args)

    tests = filter_tests(args, tests, build_config.enterprise, build_config.isNightly)
    add_test_jobs_to_workflow(args, workflow, tests, build_config, build_jobs, repl2)
    return workflow


def add_x64_community_workflow(workflows, tests, args):
    if args.sanitizer != "" and args.nightly:
        # for nightly sanitizer runs we skip community and only test enterprise
        return
    add_workflow(
        workflows,
        tests,
        BuildConfig("x64", False, args.sanitizer, args.nightly),
        args,
    )


def add_x64_enterprise_workflow(workflows, tests, args):
    build_config = BuildConfig("x64", True, args.sanitizer, args.nightly)
    workflow = add_workflow(workflows, tests, build_config, args)
    if args.sanitizer == "" and (args.ui == "off" or args.ui == ""):
        add_build_job(
            workflow,
            build_config,
            {
                "name": "build-ee-non-maintainer-x64",
                "preset": "enterprise-pr-non-maintainer",
                "publish-artifacts": False,
                "build-tests": False,
            },
        )


def add_aarch64_community_workflow(workflows, tests, args):
    if args.ui != "only":
        add_workflow(
            workflows,
            tests,
            BuildConfig("aarch64", False, args.sanitizer, args.nightly),
            args,
        )


def add_aarch64_enterprise_workflow(workflows, tests, args):
    if args.ui != "only":
        add_workflow(
            workflows,
            tests,
            BuildConfig("aarch64", True, args.sanitizer, args.nightly),
            args,
        )


def generate_jobs(config, args, tests):
    """generate job definitions"""
    workflows = config["workflows"]
    add_x64_community_workflow(workflows, tests, args)
    add_x64_enterprise_workflow(workflows, tests, args)
    if args.sanitizer == "":
        # ATM we run ARM only without sanitizer
        add_aarch64_community_workflow(workflows, tests, args)
        add_aarch64_enterprise_workflow(workflows, tests, args)


def main():
    """entrypoint"""
    try:
        args = parse_arguments()
        if not args.sanitizer in ["", "tsan", "alubsan"]:
            raise Exception(
                f"Invalid sanitizer {args.sanitizer} - must be either empty, 'tsan' or 'alubsan'"
            )
        tests = read_definitions(args.definitions)
        # if args.validate_only:
        #    return  # nothing left to do
        with open(args.base_config, "r", encoding="utf-8") as instream:
            with open(args.output, "w", encoding="utf-8") as outstream:
                config = yaml.safe_load(instream)
                generate_jobs(config, args, tests)
                yaml.dump(config, outstream)
    except Exception as exc:
        traceback.print_exc(exc, file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
