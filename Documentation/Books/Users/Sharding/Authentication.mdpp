!CHAPTER Authentication in a cluster

In this section we describe, how authentication in a cluster is done
properly. For experiments it is possible to run the cluster completely
unauthorized by using the option *--server.disable-authentication true*
on the command line or the corresponding entry in the configuration
file. However, for production use, this is not desirable.

You can turn on authentication in the cluster by switching it on in the
configuration of your dispatchers. When you now use the planner and
kickstarter to create and launch a cluster, the *arangod* processes in
your cluster will automatically run with authentication, exactly as the
dispatchers themselves. However, the cluster will have a sharded
collection *_users* with one shard containing only the user *root* with
an empty password. We emphasize that this sharded cluster-wide
collection is different from the *_users* collections in each
dispatcher!

The coordinators in your cluster will use this cluster-wide sharded collection
to authenticate HTTP requests. If you add users using the usual methods
via a coordinator, you will in fact change the cluster-wide
collection *_users* and thus all coordinators will eventually see the
new users and authenticate against them. "Eventually" means that they
might need a few seconds to notice the change in user setup and update
their user cache.

The DBservers will have their authentication switched on as well.
However, they do not use the cluster-wide *_users* collection for
authentication, because the idea is, that the outside clients do not talk
to the DBservers directly, but always go via the coordinators. For the
cluster-internal communication between coordinators and DBservers (in
both directions), we use a simpler setup: There are two new
configuration options *cluster.username* and *cluster.password*, which
default to *root* and the empty password *""*. If you want to deviate
from this default you have to change these two configuration options
in all configuration files on all machines in the cluster. This just
means that you have to set these two options to the same values in all
configuration files *arangod.conf* in all dispatchers, since the
coordinators and DBservers will simply inherit this configuration file
from the dispatcher that has launched them.

Let us summarize what you have to do, to enable authentication in a cluster:

  1. Set *server.disable-authentication* to *false* in all configuration
     files of all dispatchers (this is already the default).
  2. Put the same values for *cluster.username* and *cluster.password*
     in the very same configuration files of all dispatchers.
  3. Create users via the usual interface on the coordinators
     (initially after the cluster launch there will be a single user *root*
     with empty password).

Please note, that in Version 2.0 of ArangoDB you can already configure the 
endpoints of the coordinators to use SSL. However, this is not yet conveniently
supported in the planner, kickstarter and in the graphical cluster 
management tools. We will fix this in the next version.

Please also consider the comments in the following section about
firewall setup.
