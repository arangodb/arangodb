#!/bin/env python3
""" read test definition, and generate the output for the specified target """
from collections import namedtuple
from datetime import date
import argparse
import copy
from enum import Enum
import json
import os
import re
import sys
import traceback
import yaml
# pylint: disable=broad-exception-raised
class DeploymentVariant(Enum):
    """ which sort of arangodb deployment? """
    SINGLE = 1,
    MIXED = 2,
    CLUSTER = 3

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
    "mixed": "some buckets will run cluster, some not.",
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
    "type": "single or cluster flag",
    "full": "whether to spare from a single or full run",
    "sniff": "whether to enable sniffing",
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
        "definitions", help="file containing the test definitions", type=str, nargs='*'
    )
    parser.add_argument("-o", "--output", type=str, required=True, help="filename of the output")
    parser.add_argument("-s", "--sanitizer", type=str, help="sanitizer to use")
    parser.add_argument("-d", "--default-container", type=str, required=True, help="default container to be used")
    parser.add_argument("-b", "--test-branches", type=str, help="colon separated list of test-definition-prefix=branch")
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
        "--arangosh-args", type=str, help="additional arguments to append to arangosh"
    )
    parser.add_argument(
        "--extra-args", type=str, help="additional arguments to append to all testing.js"
    )
    parser.add_argument(
        "--arangod-without-v8", type=str, help="whether we run a setup without .js"
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
            if isinstance(params[key], int):
                return
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

def read_yaml_suite(name, suite, definition, testfile_definitions):
    """ convert yaml representation into the internal one """
    # pylint: disable=too-many-branches
    if not 'options' in definition:
        definition['options'] = {}
    flags = []
    params = {}
    arangosh_args = []
    args = []
    if 'args' in definition:
        if not isinstance(definition['args'], dict):
            raise Exception(f"expected args to be a key value list! have: {definition['args']}")
        for key, val in definition['args'].items():
            if key == 'moreArgv':
                args.append(val)
            else:
                args.append(f"--{key}")
                if isinstance(val, bool):
                    args.append("true" if val else "false")
                else:
                    args.append(val)
    if 'arangosh_args' in definition:
        if not isinstance(definition['arangosh_args'], dict):
            raise Exception(f"expected arangosh_args to be a key value list! have: {definition['arangosh_args']}")
        for key, val in definition['arangosh_args'].items():
            arangosh_args.append(f"--{key}")
            if isinstance(val, bool):
                arangosh_args.append("true" if val else "false")
            else:
                arangosh_args.append(val)
    params = validate_params(definition['options'])

    medium_size = False
    if 'type' in params:
        if params['type'] == "cluster":
            medium_size = True
            flags.append('cluster')
        elif params['type'] == "mixed":
            medium_size = True
            flags.append('mixed')
        else:
            flags.append('single')
    size = "medium" if medium_size else "small"
    size = size if not "size" in params else params['size']

    if 'full' in params:
        flags.append("full" if params["full"] else "!full")
    if 'coverage' in params:
        flags.append("coverage" if params["coverage"] else "!coverage")
    if 'sniff' in params:
        flags.append("sniff" if params["sniff"] else "!sniff")
    run_job = definition.get('job', 'run-linux-tests')
    return {
        "name": name if not "name" in params else params['name'],
        "suites": suite,
        "size": size,
        "flags": flags,
        "args": args.copy(),
        "arangosh_args": arangosh_args.copy(),
        "params": params.copy(),
        "testfile_definitions": testfile_definitions,
        "run_job": run_job,
    }

def get_args(args):
    """ serialize args into json similar to fromArgv in testing.js """
    sub_args = {}
    for key in args.keys():
        value = args[key]
        if ":" in key:
            keyparts = key.split(":")
            if not keyparts[0] in sub_args:
                sub_args[keyparts[0]] = {}
            sub_args[keyparts[0]][keyparts[1]] = value
        elif key in sub_args:
            if isinstance(sub_args[key], list):
                sub_args[key].append(value)
            else:
                sub_args[key] = [value]
        else:
            sub_args[key] = value
    return sub_args

def read_yaml_multi_bucket_suite(name, definition, testfile_definitions, cli_args):
    """ convert yaml representation into the internal one """
    args = {}
    if 'args' in definition:
        args = definition['args']
    suite_names = []
    sub_suites = []
    options_json = []
    for suite in definition['suites']:
        if isinstance(suite, str):
            options_json.append({})
            suite_names.append(suite)
        else:
            suite_name = list(suite.keys())[0]
            if 'options' in suite[suite_name]:
                if filter_one_test(cli_args, suite[suite_name]['options']):
                    print(f"skipping {suite}")
                    continue
            suite_names.append(suite_name)
            sub_suites.append(suite[suite_name])
            if 'args' in suite[suite_name]:
                options_json.append(get_args(suite[suite_name]['args']))
            else:
                options_json.append({})
    args['optionsJson'] = json.dumps(options_json, separators=(',', ':'))
    joint_suite_name = ','.join(suite_names)
    if ('buckets' in definition['options'] and
        definition['options']['buckets'] == 'auto'):
        definition['options']['buckets'] = len(suite_names)
    definition['options']['args'] = args

    job_def = {
        'options': definition['options'],
        'name': name,
        'args': args,
        'suites': definition['suites'],
    }
    if 'job' in definition:
        job_def['job'] = definition['job']
    return read_yaml_suite(name,
                           joint_suite_name,
                           job_def,
                           testfile_definitions)

def read_definitions(filename, override_branch, cli_args):
    """read test definitions txt"""
    tests = []
    if filename.endswith(".yml"):
        with open(filename, "r", encoding="utf-8") as filep:
            testfile_definitions = {}
            config = yaml.safe_load(filep)
            if isinstance(config, dict):
                if "jobProperties" in config:
                    testfile_definitions = copy.deepcopy(config["jobProperties"])
                    if override_branch is not None:
                        testfile_definitions['branch'] = override_branch
                config = config['tests']
            for testcase in config:
                suite_name = list(testcase.keys())[0]
                try:
                    suite = testcase[suite_name]
                    if "suites" in suite:
                        tests.append(read_yaml_multi_bucket_suite(suite_name,
                                                                  suite,
                                                                  testfile_definitions,
                                                                  cli_args))
                    else:
                        tests.append(read_yaml_suite(suite_name,
                                                     suite_name,
                                                     suite,
                                                     testfile_definitions))
                except Exception as ex:
                    print(f"while parsing {suite_name} {testcase}")
                    raise ex
    else:
        raise Exception(f"Cannot handle {filename} - only .yml file format supported")
    return tests


def filter_tests(args, tests, nightly):
    """filter testcase by operations target Single/Cluster/full"""
    if args.all:
        return tests

    full = args.full or nightly
    filters = []
    if full:
        filters.append(lambda test: "!full" not in test["flags"])
    else:
        filters.append(lambda test: "full" not in test["flags"])

    # if IS_ARM:
    #     filters.append(lambda test: "!arm" not in test["flags"])

    if IS_COVERAGE:
        filters.append(lambda test: "!coverage" not in test["flags"])

    for one_filter in filters:
        tests = filter(one_filter, tests)
    return list(tests)

def filter_one_test(args, test):
    """filter testcase by operations target Single/Cluster/full"""
    if args.all:
        return False
    if IS_COVERAGE:
        if 'coverage' in test:
            return True
    full = args.full or args.nightly

    if 'full' in test:
        if full and not test['full']:
            return True
        if not full and test['full']:
            return True
    return False

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


def create_test_job(test, depl_variant, build_config, build_jobs, args, replication_version=1):
    """creates the test job definition to be put into the config yaml"""
    params = test["params"]
    suite_name = test["name"]
    suffix = params.get("suffix", "")
    if suffix:
        suite_name += f"-{suffix}"

    size = test["size"]
    if not size in ["small", "medium", "medium+", "large", "xlarge", "2xlarge"]:
        raise Exception(f"Invalid resource class size {size}")

    deployment_v_str = ""
    cluster = False
    if depl_variant == DeploymentVariant.CLUSTER:
        deployment_v_str = f"cluster{'-repl2' if replication_version==2 else ''}"
        cluster = True
    elif depl_variant == DeploymentVariant.SINGLE:
        deployment_v_str = "single"
    else:
        deployment_v_str = "mixed"
    sub_arangosh_args = args.arangosh_args
    if 'arangosh_args' in test:
        # Yaml workaround: prepend an A to stop bad things from happening.
        if test["arangosh_args"] != "":
            sub_arangosh_args = test["arangosh_args"] + args.arangosh_args
        del test["arangosh_args"]
    job = {
        # "name": f"test-{edition}-{deployment_v_str}-{suite_name}-{build_config.arch}",
        "name": f"test-{deployment_v_str}-{suite_name}-{build_config.arch}",
        "suiteName": suite_name,
        "suites": test["suites"],
        "size": get_test_size(size, build_config, depl_variant == DeploymentVariant.CLUSTER),
        "cluster": depl_variant == DeploymentVariant.CLUSTER,
        "requires": build_jobs,
        "arangosh_args": "A " + json.dumps(sub_arangosh_args),

    }
    if 'timeLimit' in params:
        job['timeLimit'] = params['timeLimit']

    if suite_name == "shell_client_aql" and build_config.isNightly and not cluster:
        # nightly single shell_client_aql suite runs some chaos tests that require more memory, so beef up the size
        job["size"] = get_test_size("medium+", build_config, cluster)
    sub_extra_args = test["args"].copy()
    if depl_variant == DeploymentVariant.CLUSTER:
        sub_extra_args += ["--replicationVersion", f"{replication_version}"]
    if build_config.isNightly:
        sub_extra_args += ["--skipNightly", "false"]
    if sub_extra_args != [] or args.extra_args != []:
        job["extraArgs"] = " ".join(sub_extra_args + args.extra_args)

    buckets = params.get("buckets", 1)
    if suite_name == "replication_sync":
        # Note: for the replication_sync suite the test-definition.txt specifies only 2 buckets,
        # because increasing this causes issues in Jenkins. However, in CircleCI we definitely
        # want to use more buckets, especially when running with sanitizers.
        buckets = 5

    if buckets != 1:
        job["buckets"] = buckets

    if test['testfile_definitions'] != {}:
        job['docker_image'] = args.default_container.replace(
            ':', test['testfile_definitions']['container_suffix'])
        job['driver-git-repo'] = test['testfile_definitions']['second_repo']
        job['driver-git-branch'] = test['testfile_definitions']['branch']
        if 'init_command' in test['testfile_definitions']:
            job['init_command'] = test['testfile_definitions']['init_command']
    else:
        job['docker_image'] = args.default_container
    return {test['run_job']: job}

def create_rta_test_job(build_config, build_jobs, deployment_mode, filter_statement, buckets, rta_branch):
    """ this job will use RTA to launch arangod """
    edition = "ee" if build_config.enterprise else "ce"
    job = {
        "name": f"test-{deployment_mode}-UI",
        "suiteName": filter_statement,
        "arangosh_args": "",
        "deployment": deployment_mode,
        "browser": "Remote_CHROME",
        "enterprise": "EP" if build_config.enterprise else "C",
        "filterStatement": filter_statement,
        "requires": build_jobs,
        "rta-branch": rta_branch,
        "buckets": buckets,
    }
    return {"run-rta-tests": job}


def add_test_definition_jobs_to_workflow(
        workflow, tests, build_config, build_jobs, args
):
    """ add tests for one architecture """
    jobs = workflow["jobs"]
    for test in tests:
        if "cluster" in test["flags"]:
            jobs.append(create_test_job(test, DeploymentVariant.CLUSTER, build_config, build_jobs, args))
            if args.replication_two:
                jobs.append(create_test_job(test, DeploymentVariant.CLUSTER, build_config, build_jobs, args, 2))
        elif "single" in test["flags"]:
            jobs.append(create_test_job(test, DeploymentVariant.SINGLE, build_config, build_jobs, args))
        elif "mixed" in test["flags"]:
            jobs.append(create_test_job(test, DeploymentVariant.MIXED, build_config, build_jobs, args))
        else:
            jobs.append(create_test_job(test, DeploymentVariant.CLUSTER, build_config, build_jobs, args))
            if args.replication_two:
                jobs.append(create_test_job(test, DeploymentVariant.CLUSTER, build_config, build_jobs, args, 2))
            jobs.append(create_test_job(test, DeploymentVariant.SINGLE, build_config, build_jobs, args))


def add_rta_ui_test_jobs_to_workflow(args, workflow, build_config, build_jobs):
    jobs = workflow["jobs"]
    ui_testsuites = [
        "UserPageTestSuite",
        "CollectionsTestSuite",
        "ViewsTestSuite",
        "GraphTestSuite",
        "QueryTestSuite",
        "AnalyzersTestSuite",
        "AnalyzersTestSuite2",
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

    ui_filter = ""
    for one_filter in ui_testsuites:
        ui_filter += f"--ui-include-test-suite {one_filter} "
    for deployment in deployments:
        jobs.append(
            create_rta_test_job(build_config, build_jobs, deployment, ui_filter, len(ui_testsuites), args.rta_branch)
        )


def add_test_jobs_to_workflow(args, workflow, tests, build_config, build_jobs):
    """ add jobs for all architectures """
    if build_config.arch == "x64" and args.ui != "" and args.ui != "off":
        add_rta_ui_test_jobs_to_workflow(args, workflow, build_config, build_jobs)
    if args.ui == "only":
        return
    # TODO: QA-698
    if build_config.enterprise and args.arangod_without_v8 != 'true':
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
        workflow, tests, build_config, build_jobs, args
    )


def add_cppcheck_job(workflow, build_job):
    """ add the cppcheck job """
    workflow["jobs"].append(
        {
            "run-cppcheck": {
                "name": "cppcheck",
                "requires": [build_job],
            }
        }
    )


def add_create_docker_image_job(workflow, build_config, build_jobs, args):
    """ add the job to build a docker image """
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
    match = re.fullmatch(r"(.+\/)?(.+)", branch)
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
    """ add the jobs to compile arangod """
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


def add_frontend_build_job(workflow, build_config):
    """ add the job to build the aardvark """
    edition = "ee" if build_config.enterprise else "ce"
    preset = "enterprise-pr" if build_config.enterprise else "community-pr"
    if build_config.sanitizer != "":
        preset += f"-{build_config.sanitizer}"
    suffix = "" if build_config.sanitizer == "" else f"-{build_config.sanitizer}"
    name = f"build-{edition}-{build_config.arch}{suffix}-frontend"
    workflow["jobs"].append({"build-frontend": {"name": name}})
    return name


def add_workflow(workflows, tests, build_config, args):
    """ add the complete overal workflow """
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

    tests = filter_tests(args, tests, build_config.isNightly)
    add_test_jobs_to_workflow(args, workflow, tests, build_config, build_jobs)
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
    """ add the enterprise run """
    build_config = BuildConfig("x64", True, args.sanitizer, args.nightly)
    workflow = add_workflow(workflows, tests, build_config, args)
    if args.sanitizer == "" and args.ui != "only":
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
    # add_x64_community_workflow(workflows, tests, args)
    add_x64_enterprise_workflow(workflows, tests, args)
    add_aarch64_enterprise_workflow(workflows, tests, args)


def main():
    """entrypoint"""
    try:
        args = parse_arguments()
        if args.sanitizer is None:
            args.sanitizer = ""
        if not args.sanitizer in ["", "tsan", "alubsan"]:
            raise Exception(
                f"Invalid sanitizer {args.sanitizer} - must be either empty, 'tsan' or 'alubsan'"
            )
        arangosh_args = args.arangosh_args
        if not arangosh_args:
            args.arangosh_args = ""
            arangosh_args = ""
        if arangosh_args in ["A", ""]:
            args.arangosh_args = []
        else:
            args.arangosh_args = arangosh_args[1:].split(' ')
        if not args.extra_args:
            args.extra_args = []
        elif args.extra_args in ["A", ""]:
            args.extra_args = []
        else:
            args.extra_args = args.extra_args[1:].split(' ')
        if args.arangod_without_v8 == "true":
            args.extra_args += [ '--skipServerJS', 'true']
        if args.ui_testsuites is None:
            args.ui_testsuites = ""
        tests = []
        for one_definition in args.definitions:
            override_branch = None
            if args.test_branches is not None and args.test_branches != "":
                try:
                    for branch_name_pair in args.test_branches.split(":"):
                        (name, branch) = branch_name_pair.split("=")
                        if one_definition.find(name) >= 0:
                            override_branch = branch
                except Exception as ex:
                    raise Exception(f"Syntax error in --test-branches: {branch_name_pair} must be 'name=branch:name2=branch2'") from ex
            new_tests = read_definitions(one_definition, override_branch, args)
            tests += new_tests
        # if args.validate_only:
        #    return  # nothing left to do
        with open(args.base_config, "r", encoding="utf-8") as instream:
            with open(args.output, "w", encoding="utf-8") as outstream:
                config = yaml.safe_load(instream)
                generate_jobs(config, args, tests)
                yaml.dump(config, outstream)
    except Exception as exc:
        traceback.print_exc(exc, file=sys.stderr)
        #sys.exit(1)
        raise exc


if __name__ == "__main__":
    main()
