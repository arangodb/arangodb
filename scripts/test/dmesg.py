#!/bin/env python3
"""
on supported systems launch a thread that will follow system messages
to detect serious incidents like oom
"""

import psutil

from async_client import (
    ArangoCLIprogressiveTimeoutExecutor,

    make_tail_params,
    tail_line_result,
    delete_tail_params
)

def dmesg_runner(dmesg):
    """ thread to run dmesg in """
    dmesg.launch()

class DmesgWatcher(ArangoCLIprogressiveTimeoutExecutor):
    """configuration"""

    def __init__(self, site_config):
        self.params = None
        super().__init__(site_config, None)

    def launch(self):
       # pylint: disable=R0913 disable=R0902
        """ dmesg wrapper """
        print('------')
        args = ['-wT']
        verbose = False
        self.params = make_tail_params(verbose,
                                       'dmesg ',
                                       self.cfg.test_report_dir / 'dmesg_log.txt')
        ret = self.run_monitored(
            "dmesg",
            args,
            params=self.params,
            progressive_timeout=9999999,
            deadline=self.cfg.deadline,
            result_line_handler=tail_line_result,
            identifier='0_dmesg'
        )
        #delete_logfile_params(params)
        ret = {}
        ret['error'] = self.params['error']
        delete_tail_params(self.params)
        return ret

    def end_run(self):
        """ terminate dmesg again """
        print(f"killing dmesg {self.params['pid']}")
        try:
            psutil.Process(self.params['pid']).kill()
        except psutil.NoSuchProcess:
            print('dmesg already gone?')
