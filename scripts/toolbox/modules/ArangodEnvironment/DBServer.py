from .ArangodServer import ArangodServer

class DBServer(ArangodServer):
    def __init__(self, environment):
        super().__init__(environment)
        environment.register_dbserver(self)

    def collect_parameters(self, param):
        self._environment.insert_cluster_agency_endpoints(param)
        param["--cluster.my-address"] = self.get_endpoint()
        param["--cluster.my-role"] = "DBSERVER"
        param["--javascript.startup-directory"] = "js" # yolo
        param["--http.trusted-origin"] = "all"
        param["--database.check-version"] = "false"
        param["--database.upgrade-check"] = "false"
        super().collect_parameters(param)
