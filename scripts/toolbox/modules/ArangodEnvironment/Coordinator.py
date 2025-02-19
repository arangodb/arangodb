from .ArangodServer import ArangodServer

import os.path as path

class Coordinator(ArangodServer):
    def __init__(self, environment):
        super().__init__(environment)
        environment.register_coordinator(self)

    def collect_parameters(self, param):
        self._environment.insert_cluster_agency_endpoints(param)
        param["--cluster.my-address"] = self.get_endpoint()
        param["--cluster.my-role"] = "COORDINATOR"
        param["--javascript.startup-directory"] = path.join(self._environment.topdir, "js")
        param["--javascript.module-directory"] = path.join(self._environment.topdir, "enterprise/js")
        param["--javascript.app-path"] = path.join(self._environment._workdir, f"apps{self._port}")
        param["--javascript.allow-admin-execute"] = "true"
        param["--http.trusted-origin"] = "all"
        param["--database.check-version"] = "false"
        param["--database.upgrade-check"] = "false"
        super().collect_parameters(param)
