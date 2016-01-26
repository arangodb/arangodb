


`new require("@arangodb/cluster").Kickstarter(plan)`

This constructor constructs a kickstarter object. Its first
argument is a cluster plan as for example provided by the planner
(see Cluster Planner Constructor and the general
explanations before this reference). The second argument is
optional and is taken to be "me" if omitted, it is the ID of the
dispatcher this object should consider itself to be. If the plan
contains startup commands for the dispatcher with this ID, these
commands are executed immediately. Otherwise they are handed over
to another responsible dispatcher via a REST call.

The resulting object of this constructors allows to launch,
shutdown, relaunch the cluster described in the plan.

