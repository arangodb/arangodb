#!/usr/bin/env python
""" Run a javascript command by spawning an arangosh
    to the configured connection """

import os
import logging
import signal
import sys
from queue import Queue, Empty
from threading import Thread
import pty
from datetime import datetime, timedelta
import psutil
from tools.asciiprint import print_progress as progress
from allure_commons._allure import attach

# import tools.loghelper as lh
# pylint: disable=dangerous-default-value

ON_POSIX = "posix" in sys.builtin_module_names


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


class ArangoCLIprogressiveTimeoutExecutor:
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
        """
        run a script in background tracing with a dynamic timeout that its got output
        Deadline will represent an absolute timeout at which it will be signalled to
        exit, and yet another minute later a hard kill including sub processes will
        follow.
        (is still alive...)
        """
        rc_exit = None
        line_filter = False
        run_cmd = [executable] + args
        children = []
        if identifier == "":
            # pylint: disable=global-statement
            identifier = f"IO_{str(params.my_id)}"
        logging.info(params)
        params["identifier"] = identifier
        if not isinstance(deadline, datetime):
            if deadline == 0:
                deadline = datetime.now() + timedelta(seconds=progressive_timeout * 10)
            else:
                deadline = datetime.now() + timedelta(seconds=deadline)
        final_deadline = deadline + timedelta(seconds=deadline_grace_period)
        logging.info("%s: launching %s", {identifier}, str(run_cmd))
        master, slave = pty.openpty()
        with psutil.Popen(
            run_cmd,
            stdout=slave,
            stderr=slave,
            close_fds=ON_POSIX,
            cwd=self.cfg.test_data_dir.resolve(),
            env=self.get_environment(params),
        ) as process:
            os.close(slave)
            # pylint: disable=consider-using-f-string
            params["pid"] = process.pid
            queue = Queue()
            io_thread = Thread(
                name=f"readIO {identifier}",
                target=enqueue_output,
                args=(master, queue, self.connect_instance, identifier, params),
            )
            io_thread.start()

            try:
                logging.info(
                    "%s me PID:%s launched PID:%s with LWPID:%s",
                    identifier,
                    str(os.getpid()),
                    str(process.pid),
                    str(io_thread.native_id),
                )
            except AttributeError:
                logging.info(
                    "%s me PID:%s launched PID:%s with LWPID:N/A",
                    identifier,
                    str(os.getpid()),
                    str(process.pid),
                )

            # read line without blocking
            have_progressive_timeout = False
            tcount = 0
            have_deadline = 0
            deadline_grace_count = 0
            while not have_progressive_timeout:
                # if you want to tail the output, enable this:
                # out.flush()
                result_line_handler(tcount, None, params)
                line = ""
                try:
                    overload = self.cfg.get_overload()
                    if overload:
                        add_message_to_report(params, overload, False)
                    line = queue.get(timeout=1)
                    ret = result_line_handler(0, line, params)
                    line_filter = line_filter or ret
                    tcount = 0
                    if not isinstance(line, tuple):
                        print_log(f"{identifier} 1 IO Thread done!", params)
                        break
                except Empty:
                    tcount += 1
                    have_progressive_timeout = tcount >= progressive_timeout
                    if have_progressive_timeout:
                        try:
                            children = process.children(recursive=True)
                        except psutil.NoSuchProcess:
                            pass
                        process.kill()
                        kill_children(identifier, params, children)
                        rc_exit = process.wait()
                    elif tcount % 30 == 0:
                        try:
                            children = children + process.children(recursive=True)
                            rc_exit = process.wait(timeout=1)
                            children = children + self.dig_for_children(params)
                            add_message_to_report(
                                params,
                                f"{identifier} exited unexpectedly: {str(rc_exit)}",
                                True,
                                True,
                            )
                            kill_children(identifier, params, children)
                            break
                        except psutil.NoSuchProcess:
                            children = children + self.dig_for_children(params)
                            add_message_to_report(
                                params,
                                f"{identifier} exited unexpectedly: {str(rc_exit)}",
                                True,
                                True,
                            )
                            kill_children(identifier, params, children)
                            break
                        except psutil.TimeoutExpired:
                            pass  # Wait() has thrown, all is well!
                except OSError as error:
                    logging.error("Got an OS-Error, will abort all! %s", error.strerror)
                    try:
                        # get ALL subprocesses!
                        children = psutil.Process().children(recursive=True)
                    except psutil.NoSuchProcess:
                        pass
                    process.kill()
                    kill_children(identifier, params, children)
                    io_thread.join()
                    return {
                        "progressive_timeout": True,
                        "have_deadline": True,
                        "rc_exit": -99,
                        "line_filter": -99,
                    }

                if datetime.now() > deadline:
                    have_deadline += 1
                if have_deadline == 1:
                    # pylint: disable=line-too-long
                    add_message_to_report(
                        params,
                        f"{identifier} Execution Deadline reached - will trigger signal {self.deadline_signal}!",
                    )
                    # Send the process our break / sigint
                    try:
                        children = process.children(recursive=True)
                    except psutil.NoSuchProcess:
                        pass
                    try:
                        process.send_signal(self.deadline_signal)
                    except psutil.NoSuchProcess:
                        children = children + self.dig_for_children(params)
                        print_log(f"{identifier} process already dead!", params)
                elif have_deadline > 1 and datetime.now() > final_deadline:
                    try:
                        # give it some time to exit:
                        print_log(f"{identifier} try wait exit:", params)
                        try:
                            children = children + process.children(recursive=True)
                        except psutil.NoSuchProcess:
                            pass
                        rc_exit = process.wait(1)
                        add_message_to_report(
                            params, f"{identifier}  exited: {str(rc_exit)}"
                        )
                        kill_children(identifier, params, children)
                        print_log(f"{identifier}  closing", params)
                        process.stderr.close()
                        process.stdout.close()
                        break
                    except psutil.TimeoutExpired:
                        # pylint: disable=line-too-long
                        deadline_grace_count += 1
                        print_log(
                            f"{identifier} timeout waiting for exit {str(deadline_grace_count)}",
                            params,
                        )
                        # if its not willing, use force:
                        if deadline_grace_count > deadline_grace_period:
                            print_log(f"{identifier} getting children", params)
                            try:
                                children = process.children(recursive=True)
                            except psutil.NoSuchProcess:
                                pass
                            kill_children(identifier, params, children)
                            add_message_to_report(params, f"{identifier} killing")
                            process.kill()
                            print_log(f"{identifier} waiting", params)
                            rc_exit = process.wait()
                            print_log(f"{identifier} closing", params)
                            process.stderr.close()
                            process.stdout.close()
                            break
            print_log(f"{identifier} IO-Loop done", params)
            timeout_str = ""
            if have_progressive_timeout:
                timeout_str = "TIMEOUT OCCURED!"
                logging.info(timeout_str)
                timeout_str += "\n"
                params['error'] += timeout_str
            elif rc_exit is None:
                print_log(f"{identifier} waiting for regular exit", params)
                rc_exit = process.wait()
                print_log(f"{identifier} done", params)
            kill_children(identifier, params, children)
            print_log(f"{identifier} joining io Threads", params)
            io_thread.join()
            print_log(f"{identifier} OK", params)

        return {
            "progressive_timeout": have_progressive_timeout,
            "have_deadline": have_deadline,
            "rc_exit": rc_exit,
            "line_filter": line_filter,
        }
