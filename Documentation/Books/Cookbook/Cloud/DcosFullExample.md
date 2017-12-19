# Deploying a highly available application using ArangoDB and Foxx on DC/OS

## Problem

How can I deploy an application using ArangoDB on DC/OS and make everything highly available?

## Solution

To achieve this goal several individual components have to be combined.

### Install DC/OS

Go to https://dcos.io/install/ and follow the instructions to install DC/OS

Also make sure to install the dcos CLI.

### Install ArangoDB

Once your cluster is DC/OS cluster is ready install the package `arangodb3` from the universe tab (default settings are fine)

Detailed instructions may be found in the first chapters of our DC/OS tutorial here: 

https://dcos.io/docs/1.7/usage/tutorials/arangodb/

To understand how ArangoDB ensures that it is highly available make sure to read the cluster documentation here:

https://docs.arangodb.com/3.0/Manual/Scalability/Architecture.html

### Deploy a load balancer for the coordinators
    
Once ArangoDB is installed and healthy you can access the cluster via one of the coordinators.

To do so from the outside DC/OS provides a nice and secure gateway through their admin interface.

However this is intended to be used from the outside only. Applications using ArangoDB as its data store will want to connect to the coordinators from the inside. You could check the task list within DC/OS to find out the endpoints where the coordinators are listening. However these are not to be trusted: They can fail at any time, the number of coordinators might change due to up- and downscaling or someone might even kill a full DC/OS Agent and tasks may simply fail and reappear on a different endpoint.

In short: Endpoints are `temporary`.

To mitigate this problem we have built a special load balancer for these coordinators. 

To install it:

```
$ git clone https://github.com/arangodb/arangodb-mesos-haproxy
$ cd arangodb-mesos-haproxy
$ dcos marathon app add marathon.json
```

Afterwards you can use the following endpoint to access the coordinators from within the cluster:

```
tcp://arangodb-proxy.marathon.mesos:8529
```

To make it highly available you can simply launch a few more instances using the marathon web interface. Details on how to do this and how to deploy an application using the UI can be found here: https://dcos.io/docs/1.7/usage/tutorials/marathon/marathon101/

### Our test application

Now that we have setup ArangoDB on DC/OS it is time to deploy our application. In this example we will use our guesser application which you can find here:

https://github.com/arangodb/guesser

This application has some application code and a Foxx microservice.

### Deploying the Foxx service

Open the ArangoDB interface (via the `Services` tab in the DC/OS interface) and go to `Services`.

Enter `/guesser` as mount directory
Choose `github` on the tab and enter the following repository:

```
ArangoDB/guesser
```

Choose `master` as version.

Press `Install`

### Deploy the application

Finally it is time to deploy the application code. We have packaged everything into a docker container. The only thing that is missing is some connection info for the database. This can be provided via environment variables through marathon.

Open the marathon webinterface in your DC/OS cluster (`Services` and then `marathon`).

Then click `Create application`

On the top right you can change to the JSON view. Paste the following config:

```
{
  "id": "/guesser",
  "cmd": null,
  "cpus": 1,
  "mem": 128,
  "disk": 0,
  "instances": 3,
  "container": {
    "type": "DOCKER",
    "volumes": [],
    "docker": {
      "image": "arangodb/example-guesser",
      "network": "BRIDGE",
      "portMappings": [
        {
          "containerPort": 8000,
          "hostPort": 0,
          "servicePort": 10004,
          "protocol": "tcp"
        }
      ],
      "privileged": false,
      "parameters": [],
      "forcePullImage": true
    }
  },
  "labels":{
    "HAPROXY_GROUP":"external"
  },
  "env": {
    "ARANGODB_SERVER": "http://arangodb-proxy.marathon.mesos:8529",
    "ARANGODB_ENDPOINT": "tcp://arangodb-proxy.marathon.mesos:8529"
  }
}
```

As you can see we are providing the `ARANGODB_ENDPOINT` as an environment variable. The docker container will take that and use it when connecting. This configuration injection via environment variables is considered a docker best practice and this is how you should probably create your applications as well.

Now we have our guesser app started within mesos.

It is highly available right away as we launched 3 instances. To scale it up or down simply use the scale buttons in the marathon UI.

### Make it publically available

For this to work we need another tool, namely `marathon-lb`

Install it:

```
dcos package install marathon-lb
```

After installation it will scan all marathon applications for a special set of labels and make these applications available to the public.

To make the guesser application available to the public you first have to determine a hostname that points to the external loadbalancer in your environment. When installing using the cloudformation template on AWS this is the hostname of the so called `public slave`. You can find it in the output tab of the cloudformation template.

In my case this was:

`mop-publicslaveloa-3phq11mb7oez-1979417947.eu-west-1.elb.amazonaws.com`

In case there are uppercased letters please take *extra care* to lowercase them as the marathon-lb will fail otherwise.

Edit the settings of the guesser app in the marathon UI and add this hostname as the label HAPROXY_0_VHOST either using the UI or using the JSON mode:

```
[...]
  "labels": {
    "HAPROXY_GROUP": "external",
    "HAPROXY_0_VHOST": "mop-publicslaveloa-3phq11mb7oez-1979417947.eu-west-1.elb.amazonaws.com"
  },
[...]
```

To scale it up and thus making it highly available increase the `instances` count within marathon.

For more detailed information and more configuration options including SSL etc be sure to check the documentation:

https://docs.mesosphere.com/1.7/usage/service-discovery/marathon-lb/usage/

### Accessing the guesser game

After marathon-lb has reloaded its configuration (which should happen almost immediately) you should be able to access the guesser game by pointing a web browser to your hostname.

Have fun!

### Conclusion

There are quite a few components involved in this process but once you are finished you will have a highly resilient system which surely was worth the effort. None of the components involved is a single point of failure.

**Author:** [Andreas Streichardt](https://github.com/m0ppers)

**Tags:** #docker #howto #dcos #cluster #ha