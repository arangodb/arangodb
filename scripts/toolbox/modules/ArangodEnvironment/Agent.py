from .ArangodServer import ArangodServer

class Agent(ArangodServer):
    def __init__(self, environment):
        super().__init__(environment)
        environment.register_agent(self)

    def collect_parameters(self, param):
        param["--agency.activate"] = "true"
        param["--agency.my-address"] = self.get_endpoint()
        param["--agency.pool-size"] = "1"
        param["--agency.size"] = "1"
        param["--agency.supervision"] = "true"
        param["--agency.wait-for-sync"] = "false"
        super().collect_parameters(param)
