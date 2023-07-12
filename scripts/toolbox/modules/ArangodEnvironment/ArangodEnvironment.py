from .Agent import Agent
from .Coordinator import Coordinator
from .DBServer import DBServer

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

    def start_agent(self):
        agent = Agent(self)
        agent.start()

    def register_agent(self, agent):
        self._agents.append(agent)

    def start_coordinator(self):
        coordinator = Coordinator(self)
        coordinator.start()

    def register_coordinator(self, coordinator):
        self._coordinators.append(coordinator)

    def start_dbserver(self):
        dbserver = DBServer(self)
        dbserver.start()

    def register_dbserver(self, dbserver):
        self._dbservers.append(dbserver)

    def insert_cluster_agency_endpoints(self, param):
        for agent in self._agents:
            param["--cluster.agency-endpoint"] = agent.get_endpoint()

    def get_coordinator_http_endpoint(self):
        return self._coordinators[0].get_http_endpoint()
