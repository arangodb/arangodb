
Apache Mesos integration
------------------------

For the distributed setup, we use the Apache Mesos infrastructure by default.

ArangoDB is a fully certified package for DC/OS and can thus
be deployed essentially with a few mouse clicks or a single command, once
you have an existing DC/OS cluster. But even on a plain Apache Mesos cluster
one can deploy ArangoDB via Marathon with a single API call and some JSON
configuration.

The advantage of this approach is that we can not only implement the
initial deployment, but also the later management of automatic
replacement of failed instances and the scaling of the ArangoDB cluster
(triggered manually or even automatically). Since all manipulations are
either via the graphical web UI or via JSON/REST calls, one can even
implement auto-scaling very easily.

A DC/OS cluster is a very natural environment to deploy microservice
architectures, since it is so convenient to deploy various services,
including potentially multiple ArangoDB cluster instances within the
same DC/OS cluster. The built-in service discovery makes it extremely
simple to connect the various microservices and Mesos automatically
takes care of the distribution and deployment of the various tasks.

See the [Deployment](../../../Deployment/README.md) chapter and its subsections
for instructions.

It is possible to deploy an ArangoDB cluster by simply launching a bunch of
Docker containers with the right command line options to link them up,
or even on a single machine starting multiple ArangoDB processes. In that
case, synchronous replication will work within the deployed ArangoDB cluster,
and automatic fail-over in the sense that the duties of a failed server will
automatically be assigned to another, surviving one. However, since the
ArangoDB cluster cannot within itself launch additional instances, replacement
of failed nodes is not automatic and scaling up and down has to be managed
manually. This is why we do not recommend this setup for production
deployment.
