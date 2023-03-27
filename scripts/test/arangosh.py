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
        """ hook to implemnet custom environment variable setters """
        my_env = os.environ.copy()
        my_env['TMPDIR'] = str(params['temp_dir'])
        my_env['TEMP'] = str(params['temp_dir'])
        my_env['TMP'] = str(params['temp_dir'])
        return my_env

    def run_testing(self,
                    testcase,
                    testing_args,
                    timeout,
                    directory,
                    logfile,
                    identifier,
                    temp_dir,
                    verbose
                    ):
       # pylint: disable=R0913 disable=R0902
        """ testing.js wrapper """
        logging.info('------')
        logging.info(testing_args)
        testscript = self.cfg.base_path / 'js' / 'client' / 'modules' / '@arangodb' / 'testutils' / 'unittest.js'
        args = [
            '-c', str(self.cfg.cfgdir / 'arangosh.conf'),
            "--log.foreground-tty", "true",
            "--log.force-direct", "true",
            '--log.level', 'warning',
            "--log.level", "v8=debug",
            '--server.endpoint', 'none',
            '--javascript.allow-external-process-control', 'true',
            '--javascript.execute', testscript,
            ]
        run_cmd = args +[
            '--',
            testcase,
            '--testOutput', directory ] + testing_args
        params = make_logfile_params(verbose, logfile, self.cfg.trace, temp_dir)
        ret = self.run_monitored(
            self.cfg.bin_dir / "arangosh",
            run_cmd,
            params=params,
            progressive_timeout=timeout,
            deadline=self.cfg.deadline,
            result_line_handler=logfile_line_result,
            identifier=identifier
        )
        delete_logfile_params(params)
        ret['error'] = params['error']
        return ret
