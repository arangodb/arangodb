#!/usr/bin/env python
""" Run a javascript command by spawning an arangosh
    to the configured connection """

import os
import logging
import platform
import signal
import sys
from datetime import datetime, timedelta
from subprocess import PIPE
import psutil
from allure_commons._allure import attach
from abc import ABC, abstractmethod
from tools.asciiprint import print_progress as progress

# import tools.loghelper as lh
# pylint: disable=dangerous-default-value

ON_POSIX = "posix" in sys.builtin_module_names
IS_WINDOWS = platform.win32_ver()[0] != ""


def print_log(string, params):
    """only print if thread debug logging is enabled"""
    if params["trace_io"]:
        logging.debug(string)


def default_line_result(wait, line, params):
    """
    Keep the line, filter it for leading #,
    if verbose print the line. else print progress.
    """
    # pylint: disable=pointless-statement
    if params["verbose"] and wait > 0 and line is None:
        progress("sj" + str(wait))
        return True
    if isinstance(line, tuple):
        if params["verbose"]:
            logging.debug("e: %s", str(line[0], "utf-8").rstrip())
        if not str(line[0]).startswith("#"):
            params["output"].append(line[0])
        else:
            return False
    return True


def make_default_params(verbose):
    """create the structure to work with arrays to output the strings to"""
    return {
        "trace_io": False,
        "error": "",
        "verbose": verbose,
        "output": [],
        "identifier": "",
    }


def tail_line_result(wait, line, params):
    """
    Keep the line, filter it for leading #,
    if verbose print the line. else print progress.
    """
    # pylint: disable=pointless-statement
    if params["skip_done"]:
        if isinstance(line, tuple):
            logging.info("%s%s", params["prefix"], str(line[0], "utf-8").rstrip())
            params["output"].write(line[0])
        return True
    now = datetime.now()
    if now - params["last_read"] > timedelta(seconds=1):
        params["skip_done"] = True
        logging.info("%s initial tail done, starting to output", params["prefix"])
    return True


def make_tail_params(verbose, prefix, logfile):
    """create the structure to work with arrays to output the strings to"""
    return {
        "trace_io": False,
        "error": "",
        "verbose": verbose,
        "output": logfile.open("wb"),
        "lfn": str(logfile),
        "identifier": "",
        "skip_done": False,
        "prefix": prefix,
        "last_read": datetime.now(),
    }


def delete_tail_params(params):
    """teardown the structure to work with logfiles"""
    logging.info("%s closing %s", params["identifier"], params["lfn"])
    params["output"].flush()
    params["output"].close()
    logging.info("%s %s closed", params["identifier"], params["lfn"])


def make_logfile_params(verbose, logfile, trace, temp_dir):
    """create the structure to work with logfiles"""
    return {
        "trace_io": True,
        "trace": trace,
        "error": "",
        "verbose": verbose,
        "output": logfile.open("wb"),
        "identifier": "",
        "lfn": str(logfile),
        "temp_dir": temp_dir,
    }


def logfile_line_result(wait, line, params):
    """Write the line to a logfile, print progress."""
    # pylint: disable=pointless-statement
    if params["trace"] and wait > 0 and line is None:
        progress("sj" + str(wait))
        return True
    if isinstance(line, tuple):
        if params["trace"]:
            logging.debug("e: %s", str(line[0], "utf-8").rstrip())
        sys.stdout.buffer.write(line[0])
        params["output"].write(line[0])
    return True


def delete_logfile_params(params):
    """teardown the structure to work with logfiles"""
    logging.info("%s closing %s", params["identifier"], params["lfn"])
    params["output"].flush()
    params["output"].close()
    logging.info("%s %s closed", params["identifier"], params["lfn"])


def enqueue_output(fd, queue, instance, identifier, params):
    """add stdout/stderr to the specified queue"""
    while True:
        try:
            data = os.read(fd, 1024)
        except OSError as ex:
            print_log(
                f"{identifier} communication line seems to be closed: {str(ex)}", params
            )
            break
        if not data:
            break
        queue.put((data, instance))
    print(f"{identifier} done! {params}")
    print_log(f"{identifier} done!", params)
    queue.put(-1)
    os.close(fd)


def convert_result(result_array):
    """binary -> string"""
    result = ""
    for one_line in result_array:
        if isinstance(one_line, str):
            result += "\n" + one_line.rstrip()
        else:
            result += "\n" + one_line.decode("utf-8").rstrip()
    return result


def add_message_to_report(params, string, print_it=True, add_to_error=False):
    """add a message from python to the report strings/files + print it"""
    oskar = "OSKAR"
    count = int(80 / len(oskar))
    datestr = f"  {datetime.now()} - "
    offset = 80 - (len(string) + len(datestr) + 2 * len(oskar))
    if print_it:
        logging.info(string)
        # we also want these messages to be written to stdout, so they also show up in CircleCI
        print(string)
    if add_to_error:
        params["error"] += "async_client.py: " + string + "\n"
    if isinstance(params["output"], list):
        params[
            "output"
        ] += f"{oskar*count}\n{oskar}{datestr}{string}{' '*offset}{oskar}\n{oskar*count}\n"
    else:
        params["output"].write(
            bytearray(
                f"{oskar*count}\n{oskar}{datestr}{string}{' '*offset}{oskar}\n{oskar*count}\n",
                "utf-8",
            )
        )
        params["output"].flush()
    sys.stdout.flush()
    return string + "\n"


def kill_children(identifier, params, children):
    """slash all processes enlisted in children - if they still exist"""
    err = ""
    killed = []
    for one_child in children:
        if one_child.pid in killed:
            continue
        try:
            pname = one_child.name()
            if pname not in ["svchost.exe", "conhost.exe", "mscorsvw.exe"]:
                killed.append(one_child.pid)
                err += add_message_to_report(
                    params,
                    f"{identifier}: killing {pname} - {str(one_child.pid)}",
                )
                one_child.resume()
        except FileNotFoundError:
            pass
        except AttributeError:
            pass
        except ProcessLookupError:
            pass
        except psutil.NoSuchProcess:
            pass
        except psutil.AccessDenied:
            pass
        try:
            one_child.kill()
        except psutil.NoSuchProcess:  # pragma: no cover
            pass
    print_log(
        f"{identifier}: Waiting for the children to terminate {killed} {len(children)}",
        params,
    )
    psutil.wait_procs(children, timeout=20)
    return err


class CliExecutionException(Exception):
    """transport CLI error texts"""

    def __init__(self, message, execution_result, have_timeout):
        super().__init__()
        self.execution_result = execution_result
        self.message = message
        self.have_timeout = have_timeout


def expect_failure(expect_to_fail, ret, params):
    """convert results, throw error if wanted"""
    attach(str(ret["rc_exit"]), f"Exit code: {str(ret['rc_exit'])} == {expect_to_fail}")
    res = (None, None, None, None)
    if ret["have_deadline"] or ret["progressive_timeout"]:
        res = (False, convert_result(params["output"]), 0, ret["line_filter"])
        raise CliExecutionException(
            "Execution failed.", res, ret["progressive_timeout"] or ret["have_deadline"]
        )
    if ret["rc_exit"] != 0:
        res = (False, convert_result(params["output"]), 0, ret["line_filter"])
        if expect_to_fail:
            return res
        raise CliExecutionException("Execution failed.", res, False)

    if not expect_to_fail:
        if len(params["output"]) == 0:
            res = (True, "", 0, ret["line_filter"])
        else:
            res = (True, convert_result(params["output"]), 0, ret["line_filter"])
        return res

    if len(params["output"]) == 0:
        res = (True, "", 0, ret["line_filter"], params["error"])
    else:
        res = (True, convert_result(params["output"]), 0, ret["line_filter"])
    raise CliExecutionException(
        f"{params.identifier} Execution was expected to fail, but exited successfully.",
        res,
        ret["progressive_timeout"],
    )


ID_COUNTER = 0


class ArangoCLIprogressiveTimeoutExecutor(ABC):
    """
    Abstract base class to run arangodb cli tools
    with username/password/endpoint specification
    timeout will be relative to the last thing printed.
    """

    # pylint: disable=too-few-public-methods too-many-arguments disable=too-many-instance-attributes disable=too-many-statements disable=too-many-branches disable=too-many-locals
    def __init__(self, config, connect_instance, deadline_signal=-1):
        """launcher class for cli tools"""
        self.connect_instance = connect_instance
        self.cfg = config
        self.deadline_signal = deadline_signal
        self.pid = None
        if self.deadline_signal == -1:
            # pylint: disable=no-member
            # yes, one is only there on the wintendo, the other one elsewhere.
            if IS_WINDOWS:
                self.deadline_signal = signal.CTRL_BREAK_EVENT
            else:
                self.deadline_signal = signal.SIGINT

    def dig_for_children(self, params):
        """manual search for children that may be there without the self.pid still being there"""
        children = []
        for process in psutil.process_iter(["pid", "ppid", "name"]):
            if process.ppid() == params["pid"]:
                children.append(process)
            elif process.ppid() == 1 and (
                process.name().lower().find("arango") >= 0
                or process.name().lower().find("tshark") >= 0
            ):
                children.append(process)
        return children

    def get_environment(self, params):
        """hook to implemnet custom environment variable setters"""
        return os.environ.copy()

    def run_arango_tool_monitored(
        self,
        executable,
        more_args,
        use_default_auth=True,
        params={"error": "", "verbose": True, "output": []},
        progressive_timeout=60,
        deadline=0,
        deadline_grace_period=180,
        result_line_handler=default_line_result,
        expect_to_fail=False,
        identifier="",
    ):
        """
        runs a script in background tracing with
        a dynamic timeout that its got output
        (is still alive...)
        """
        # fmt: off
        passvoid = ''
        if self.cfg.passvoid:
            passvoid  = str(self.cfg.passvoid)
        elif self.connect_instance:
            passvoid = str(self.connect_instance.get_passvoid())
        if passvoid is None:
            passvoid = ''

        run_cmd = [
            "--log.foreground-tty", "true",
            "--log.force-direct", "true",
        ]
        if self.connect_instance:
            run_cmd += ["--server.endpoint", self.connect_instance.get_endpoint()]
            if use_default_auth:
                run_cmd += ["--server.username", str(self.cfg.username)]
                run_cmd += ["--server.password", passvoid]

        run_cmd += more_args
        ret = self.run_monitored(executable,
                                 run_cmd,
                                 params,
                                 progressive_timeout,
                                 deadline,
                                 deadline_grace_period,
                                 result_line_handler,
                                 identifier)
        return expect_failure(expect_to_fail, ret, params)

    # fmt: on
    @abstractmethod
    def run_monitored(
        self,
        executable,
        args,
        params={"error": "", "verbose": True, "output": []},
        progressive_timeout=60,
        deadline=0,
        deadline_grace_period=180,
        result_line_handler=default_line_result,
        identifier="",
    ):
        raise NotImplementedError("Subclasses should implement this!")
    
