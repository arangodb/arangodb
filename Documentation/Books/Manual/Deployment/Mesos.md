Distributed deployment using Apache Mesos
-----------------------------------------

ArangoDB has a sophisticated and yet easy way to use cluster mode. To leverage the full cluster feature set (monitoring, scaling, automatic failover and automatic replacement of failed nodes) you have to run ArangoDB on some kind of cluster management system. Currently ArangoDB relies on Apache Mesos in that matter. Mesos is a cluster operating system which powers some of the worlds biggest datacenters running several thousands of nodes.

### DC/OS

DC/OS is the recommended way to install a cluster as it eases much of the process to install a Mesos cluster. You can deploy it very quickly on a variety of cloud hosters or setup your own DC/OS locally. DC/OS is a set of tools built on top of Apache Mesos. Apache Mesos is a so called "Distributed Cluster Operation System" and the core of DC/OS. Apache Mesos has the concept of so called [persistent volumes](http://mesos.apache.org/documentation/latest/persistent-volume/) which make it perfectly suitable for a database.

#### Installing

First prepare a DC/OS cluster by going to https://dcos.io and following 
the instructions there.

DC/OS comes with its own package management. Packages can be installed from the so called "Universe". As an official DC/OS partner ArangoDB can be installed from there straight away.

1. Installing via DC/OS UI

   1. Open your browser and go to the DC/OS admin interface
   2. Open the "Universe" tab
   3. Locate arangodb and hit "Install Package"
   4. Press "Install Package"

2. Installing via the DC/OS command line

   1. Install the [dcos cli](https://docs.mesosphere.com/usage/cli/)
   2. Open a terminal and issue `dcos install arangodb`
        
Both options are essentially doing the same in the background. Both are starting ArangoDB with its default options set.

To review the default options using the web interface simply click "Advanced Installation" in the web interface. There you will find a list of options including some explanation.

To review the default options using the CLI first type `dcos package describe --config arangodb`. This will give you a flat list of default settings.

To get an explanation of the various command line options please check the latest options here (choose the most recent number and have a look at `config.json`):

https://github.com/mesosphere/universe/tree/version-3.x/repo/packages/A/arangodb

After installation DC/OS will start deploying the ArangoDB cluster on the DC/OS cluster. You can watch ArangoDB starting on the "Services" tab in the web interface. Once it is listed as healthy click the link next to it and you should see the ArangoDB web interface.

#### ArangoDB Mesos framework

As soon as ArangoDB was deployed Mesos will keep your cluster running. The web interface has many monitoring facilities so be sure to make yourself familiar with the DC/OS web interface. As a fault tolerant system Mesos will take care of most failure scenarios automatically. Mesos does that by running ArangoDB as a so called "framework". This framework has been specifically built to keep ArangoDB running in a healthy condition on the Mesos cluster. From time to time a task might fail. The ArangoDB framework will then take care of rescheduling the failed task. As it knows about the very specifics of each cluster task and its role it will automatically take care of most failure scenarios.

To inspect what the framework is doing go to `http://web-interface-url/mesos` in your browser. Locate the task "arangodb" and inspect stderr in the "Sandbox". This can be of interest for example when a slave got lost and the framework is rescheduling the task.

#### Using ArangoDB

To use ArangoDB as a datastore in your DC/OS cluster you can facilitate the service discovery of DC/OS. Assuming you deployed a standard ArangoDB cluster the [mesos dns](https://github.com/mesosphere/mesos-dns) will know about `arangodb.mesos`. By doing a SRV DNS request (check the documentation of mesos dns) you can find out the port where the internal HAProxy of ArangoDB is running. This will offer a round robin load balancer to access all ArangoDB coordinators.

#### Scaling ArangoDB

To change the settings of your ArangoDB Cluster access the ArangoDB UI and hit "Nodes". On the scale tab you will have the ability to scale your cluster up and down.

After changing the settings the ArangoDB framework will take care of the rest. Scaling your cluster up is generally a straightforward operation as Mesos will simply launch another task and be done with it. Scaling down is a bit more complicated as the data first has to be moved to some other place so that will naturally take somewhat longer.

Please note that scaling operations might not always work. For example if the underlying Mesos cluster is completely saturated with its running tasks scaling up will not be possible. Scaling down might also fail due to the cluster not being able to move all shards of a DBServer to a new destination because of size limitations. Be sure to check the output of the ArangoDB framework.

#### Deinstallation

Deinstalling ArangoDB is a bit more difficult as there is much state being kept in the Mesos cluster which is not automatically cleaned up. To deinstall from the command line use the following one liner:

`dcos arangodb uninstall ; dcos package uninstall arangodb`

This will first cleanup the state in the cluster and then uninstall arangodb.

#### arangodb-cleanup-framework

Should you forget to cleanup the state you can do so later by using the [arangodb-cleanup-framework](https://github.com/arangodb/arangodb-cleanup-framework/) container. Otherwise you might not be able to deploy a new arangodb installation.

The cleanup framework will announce itself as a normal ArangoDB. Mesos will recognize this and offer all persistent volumes it still has for ArangoDB to this framework. The cleanup framework will then properly free the persistent volumes. Finally it will clean up any state left in zookeeper (the central configuration manager in a Mesos cluster).

To deploy the cleanup framework, follow the instructions in the github repository. After deployment watch the output in the sandbox of the Mesos web interface. After a while there shouldn't be any persistent resource offers anymore as everything was cleaned up. After that you can delete the cleanup framework again via Marathon.

### Apache Mesos and Marathon

You can also install ArangoDB on a bare Apache Mesos cluster provided that Marathon is running on it.

Doing so has the following downsides:

1. Manual Mesos cluster setup
1. You need to implement your own service discovery
1. You are missing the dcos cli
1. Installation and deinstallation are tedious
1. You need to setup some kind of proxy tunnel to access ArangoDB from the outside
1. Sparse monitoring capabilities

However these are things which do not influence ArangoDB itself and operating your cluster like this is fully supported.

#### Installing via Marathon

To install ArangoDB via marathon you need a proper config file:

```
{
  "id": "arangodb",
  "cpus": 0.25,
  "mem": 256.0,
  "ports": [0, 0, 0],
  "instances": 1,
  "args": [
    "framework",
    "--framework_name=arangodb",
    "--master=zk://172.17.0.2:2181/mesos",
    "--zk=zk://172.17.0.2:2181/arangodb",
    "--user=",
    "--principal=pri",
    "--role=arangodb",
    "--mode=cluster",
    "--async_replication=true",
    "--minimal_resources_agent=mem(*):512;cpus(*):0.25;disk(*):512",
    "--minimal_resources_dbserver=mem(*):512;cpus(*):0.25;disk(*):1024",
    "--minimal_resources_secondary=mem(*):512;cpus(*):0.25;disk(*):1024",
    "--minimal_resources_coordinator=mem(*):512;cpus(*):0.25;disk(*):1024",
    "--nr_agents=1",
    "--nr_dbservers=2",
    "--nr_coordinators=2",
    "--failover_timeout=86400",
    "--arangodb_image=arangodb/arangodb-mesos:3.1",
    "--secondaries_with_dbservers=false",
    "--coordinators_with_dbservers=false"
  ],
  "container": {
    "type": "DOCKER",
    "docker": {
      "image": "arangodb/arangodb-mesos-framework:3.1",
      "network": "HOST"
    }
  },
  "healthChecks": [
    {
      "protocol": "HTTP",
      "path": "/framework/v1/health.json",
      "gracePeriodSeconds": 3,
      "intervalSeconds": 10,
      "portIndex": 0,
      "timeoutSeconds": 10,
      "maxConsecutiveFailures": 0
    }
  ]
}
```

Carefully review the settings (especially the IPs and the resources). Then you can deploy to Marathon:

    curl -X POST -H "Content-Type: application/json" http://url-of-marathon/v2/apps -d @arangodb3.json

Alternatively use the web interface of Marathon to deploy ArangoDB. It has a JSON mode and you can use the above configuration file.

#### Deinstallation via Marathon

As with DC/OS you first need to properly cleanup any state leftovers.

The easiest is to simply delete ArangoDB and then deploy the cleanup-framework (see section arangodb-cleanup-framework).

### Configuration options

The Arangodb Mesos framework has a ton of different options which are listed and described here: https://github.com/arangodb/arangodb-mesos-framework/tree/3.1
