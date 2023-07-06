class ArangodEnvironment:
    """An environment for starting arangod processes
    """
    def __init__(self, binary, topdir, workdir):
        self.binary = binary
        self.topdir = topdir
        self._workdir = workdir
        self._next_port = 8500
        self._address = "127.0.0.1"
        self._transport = "tcp"
        self._agents = []
        self._coordinators = []
        self._dbservers = []

    def __del__(self):
        pass

    def get_next_port(self):
        result = self._next_port
        self._next_port = self._next_port + 1
        return result

    def register_agent(self, agent):
        self._agents.append(agent)

    def register_coordinator(self, coordinator):
        self._coordinators.append(coordinator)

    def register_dbserver(self, dbserver):
        self._dbservers.append(dbserver)

    def insert_cluster_agency_endpoints(self, param):
        for agent in self._agents:
            param["--cluster.agency-endpoint"] = agent.get_endpoint()
