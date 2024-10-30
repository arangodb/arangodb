#!/bin/env python3
""" keep the config for one testsuite to execute """
import copy
import json
import logging
import os

from site_config import TEMP

TEST_LOG_FILES = []


class TestConfig:
    """setup of one test"""

    # pylint: disable=too-many-instance-attributes disable=too-many-arguments
    # pylint: disable=too-many-branches disable=too-many-statements
    # pylint: disable=too-few-public-methods disable=line-too-long
    def __init__(self, cfg, name, suite, args, arangosh_args, priority, parallelity, flags):
        """defaults for test config"""
        self.parallelity = parallelity
        self.launch_delay = 1.3
        self.progressive_timeout = 100
        self.priority = priority
        self.suite = suite
        self.name = name
        self.name_enum = name
        self.crashed = False
        self.success = True
        self.structured_results = ""
        self.summary = ""
        self.start = None
        self.finish = None
        self.delta_seconds = 0
        self.delta = None

        self.base_logdir = cfg.test_report_dir / self.name
        if not self.base_logdir.exists():
            self.base_logdir.mkdir()
        self.log_file = cfg.run_root / f"{self.name}.log"

        self.xml_report_dir = cfg.xml_report_dir / self.name
        if not self.xml_report_dir.exists():
            self.xml_report_dir.mkdir(parents=True)

        self.temp_dir = TEMP / self.name
        # pylint: disable=global-variable-not-assigned
        global TEST_LOG_FILES
        try:
            logging.info(TEST_LOG_FILES.index(str(self.log_file)))
            raise Exception(f"duplicate testfile {str(self.log_file)}")
        except ValueError:
            TEST_LOG_FILES.append(str(self.log_file))
        self.summary_file = self.base_logdir / "testfailures.txt"
        self.crashed_file = self.base_logdir / "UNITTEST_RESULT_CRASHED.json"
        self.success_file = self.base_logdir / "UNITTEST_RESULT_EXECUTIVE_SUMMARY.json"
        self.report_file = self.base_logdir / "UNITTEST_RESULT.json"
        self.base_testdir = cfg.test_data_dir_x / self.name

        self.arangosh_args = [];
        # the yaml work around is to have an A prepended. detect and strip out:
        if arangosh_args is not None and len(arangosh_args) > 0 and arangosh_args != 'A ""':
            print(arangosh_args)
            self.arangosh_args = json.loads(arangosh_args[1:])
        self.args = copy.deepcopy(cfg.extra_args)
        for param in args:
            if param.startswith("$"):
                paramname = param[1:]
                if paramname in os.environ:
                    self.args += os.environ[paramname].split(" ")
                else:
                    logging.error(
                        "Error: failed to expand environment variable: '%s' for '%s'",
                        param,
                        self.name,
                    )
            else:
                self.args.append(param)
        self.args += [
            "--coreCheck",
            "true",
            "--disableMonitor",
            "true",
            "--writeXmlReport",
            "true",
            "--testXmlOutputDirectory",
            str(self.xml_report_dir),
        ]

        if "filter" in os.environ:
            self.args += ["--test", os.environ["filter"]]
        if "sniff" in flags:
            self.args += ["--sniff", "true"]

        if "SKIPNONDETERMINISTIC" in os.environ:
            self.args += ["--skipNondeterministic", os.environ["SKIPNONDETERMINISTIC"]]
        if "SKIPTIMECRITICAL" in os.environ:
            self.args += ["--skipTimeCritical", os.environ["SKIPTIMECRITICAL"]]

        if "BUILDMODE" in os.environ:
            self.args += ["--buildType", os.environ["BUILDMODE"]]

        if "DUMPAGENCYONERROR" in os.environ:
            self.args += ["--dumpAgencyOnError", os.environ["DUMPAGENCYONERROR"]]

        myport = cfg.portbase
        cfg.portbase += cfg.port_offset
        self.args += [
            "--minPort",
            str(myport),
            "--maxPort",
            str(myport + cfg.port_offset - 1),
        ]
        if "SKIPGREY" in os.environ:
            self.args += ["--skipGrey", os.environ["SKIPGREY"]]
        if "ONLYGREY" in os.environ:
            self.args += ["--onlyGrey", os.environ["ONLYGREY"]]

        if "ssl" in flags:
            self.args += ["--protocol", "ssl"]
        if "http2" in flags:
            self.args += ["--http2", "true"]

    def __repr__(self):
        return f"""
{self.name} => {self.parallelity}, {self.priority}, {self.success} -- {' '.join(self.args)}"""

    def print_test_log_line(self):
        """get visible representation"""
        # pylint: disable=consider-using-f-string
        resultstr = "Good result in"
        if not self.success:
            resultstr = "Bad result in"
        if self.crashed:
            resultstr = "Crash occured in"
        return """
{1} {0.name} => {0.delta_seconds}, {0.parallelity}, {0.priority}, {0.success} -- {2}""".format(
            self, resultstr, " ".join(self.args)
        )

    def print_testruns_line(self):
        """get visible representation"""
        # pylint: disable=consider-using-f-string
        resultstr = "GOOD"
        if not self.success:
            resultstr = "BAD"
        if self.crashed:
            resultstr = "CRASH"
        return """
<tr><td>{0.name}</td><td align="right">{0.delta}</td><td align="right">{1}</td></tr>""".format(
            self, resultstr
        )


def get_priority(test_config):
    """sorter function to return the priority"""
    return test_config.priority
