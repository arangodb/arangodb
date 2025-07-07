#!/bin/env python3
""" launch a testing.js instance with given testsuite and arguments """
import logging
import os
from async_client import (
    ArangoCLIprogressiveTimeoutExecutor,

    make_logfile_params,
    logfile_line_result,
    delete_logfile_params,
)

class ArangoshExecutor(ArangoCLIprogressiveTimeoutExecutor):
    """configuration"""

    def __init__(self, site_config, slot_lock):
        self.slot_lock = slot_lock
        self.read_only = False
        super().__init__(site_config, None)

    def get_environment(self, params):
        """hook to implemnet custom environment variable setters"""
        my_env = os.environ.copy()
        my_env["TMPDIR"] = str(params["temp_dir"])
        my_env["TEMP"] = str(params["temp_dir"])
        my_env["TMP"] = str(params["temp_dir"])
        return my_env

    def get_memory_limit_arg(self):
        limit = 0
        cgroup_path = "/"
        try:
            with open('/proc/self/cgroup', 'r') as f:
                cgroup_path = f.read().strip().split(':')[-1]
        except Exception:
            pass

        # Construct the path to the memory controller
        memory_path = f'/sys/fs/cgroup{cgroup_path}'

        for path in [
                "/sys/fs/cgroup/memory/memory.limit_in_bytes",  # cgroups v1 hard limit
                "/sys/fs/cgroup/memory/memory.soft_limit_in_bytes",  # cgroups v1 soft limit
                os.path.join(memory_path, "memory.max"),  # cgroups v2 hard limit
                os.path.join(memory_path, "memory.high"),  # cgroups v2 soft limit
        ]:
            try:
                with open(path) as f:
                    cgroups_limit = int(f.read())
                if cgroups_limit > 0:
                    limit = cgroups_limit
                    break
            except Exception:
                pass
        if limit > 0:
            san_mode = os.environ.get("SAN_MODE")
            if san_mode is not None:
                # sanitizer builds need more resources, but the sanitizer
                # allocations are not considered in the memory accounting,
                # so we reduce the memory assigned to all the instances
                if san_mode == "tsan":
                    # tsan is even more memory hungry
                    limit //= 3
                else:
                    limit //= 2
            return ["--memory", str(limit)]
        return []

    def run_testing(
        self,
        testcase,
        arangosh_args,
        testing_args,
        timeout,
        directory,
        logfile,
        identifier,
        temp_dir,
        verbose,
    ):
        # pylint: disable=R0913 disable=R0902
        """testing.js wrapper"""
        logging.info("------")
        testscript = (
            self.cfg.base_path
            / "js"
            / "client"
            / "modules"
            / "@arangodb"
            / "testutils"
            / "unittest.js"
        )
        args = [
            "-c",
            str(self.cfg.cfgdir / "arangosh.conf"),
            "--log.foreground-tty",
            "true",
            "--log.force-direct",
            "true",
            "--log.level",
            "warning",
            "--log.level",
            "v8=debug",
            "--server.endpoint",
            "none",
            "--javascript.allow-external-process-control",
            "true",
            "--javascript.execute",
            testscript,
        ]
        run_cmd = (
            arangosh_args + args + [
                "--",
                testcase,
                "--testOutput",
                directory,
                # ATM CircleCI does not support to attach debuggers, so we generate core dumps instead
                "--coreAbort",
                "true",
                "--build",
                self.cfg.build_dir,
            ]
            + self.get_memory_limit_arg()
            + testing_args
        )
        logging.info("Starting arangosh with the following arguments: %s", str(run_cmd))
        params = make_logfile_params(verbose, logfile, self.cfg.trace, temp_dir)
        ret = self.run_monitored(
            self.cfg.bin_dir / "arangosh",
            run_cmd,
            params=params,
            progressive_timeout=timeout,
            deadline=self.cfg.deadline,
            result_line_handler=logfile_line_result,
            identifier=identifier,
        )
        delete_logfile_params(params)
        ret["error"] = params["error"]
        ret['pid'] = params['pid']
        return ret
