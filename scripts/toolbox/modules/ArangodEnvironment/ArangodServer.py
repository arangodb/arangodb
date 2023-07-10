import json
import os.path as path
import subprocess

def flatten_dict(d):
    result = []
    for k, v in d.items():
        result.append(k)
        result.append(v)
    return result

class ArangodServer:
    """A supervisor object for an arangod process
    """
    def __init__(self, environment):
        self._environment = environment
        self._proc = None
        self._port = environment.get_next_port()

    def __del__(self):
        if self._proc is not None:
            self._proc.terminate()
            self._proc.wait()

    def collect_parameters(self, param):
        param["-c"] = "none"
        param["--server.authentication"] = "false"
        param["--database.directory"] = path.join(self._environment._workdir, f"data{self._port}")
        param["--log.role"] = "true"
        param["--log.file"] = path.join(self._environment._workdir, f"{self._port}.log")
        param["--server.endpoint"] = self.get_endpoint()

    def get_endpoint(self):
        return f"tcp://127.0.0.1:{self._port}"

    def get_http_endpoint(self):
        return f"http://127.0.0.1:{self._port}"

    def start(self, silent=True):
        param = {}
        self.collect_parameters(param)
        if silent:
            self._proc = subprocess.Popen([self._environment.binary] + flatten_dict(param),
                                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        else:
            print(json.dumps(param, indent=4))
            self._proc = subprocess.Popen([self._environment.binary] + flatten_dict(param))

    def shutdown(self):
        self._proc.terminate()
        self._proc.wait()
        self._proc = None
