!CHAPTER Recommended firewall setup

This section is intended for people who run a cluster in production
systems.

The whole idea of the cluster setup is that the coordinators serve HTTP
requests to the outside world and that all other processes (DBservers
and agency) are only available from within the cluster itself.
Therefore, in a production environment, one has to put the whole cluster
behind a firewall and only open the ports to the coordinators to the
client processes. 

Note however that for the asynchronous cluster-internal communication, 
the DBservers perform HTTP requests to the coordinators, which means
that the coordinators must also be reachable from within the cluster.

Furthermore, it is of the utmost importance to hide the agent processes of
the agency behind the firewall, since, at least at this stage, requests
to them are completely unauthorized. Leaving their ports exposed to
the outside world, endangers all data in the cluster, because everybody
on the internet could make the cluster believe that, for example, you wanted 
your databases dropped! This weakness will be alleviated in future versions,
because we will replace *etcd* by our own specialized agency
implementation, which will allow for authentication.

A further comment applies to the dispatchers. Usually you will open the
HTTP endpoints of your dispatchers to the outside world and switch on
authentication for them. This is necessary to contact them from the
outside, in the cluster launch phase. However, actually you only
need to contact one of them, who will then in turn contact the others
using cluster-internal communication. You can even get away with closing
access to all dispatchers to the outside world, provided the machine
running your browser is within the cluster network and does not have to
go through the firewall to contact the dispatchers. It is important to
be aware that anybody who can reach a dispatcher and can authorize
himself to it can launch arbitrary processes on the machine on which
the dispatcher runs!

Therefore we recommend to use SSL endpoints with user/password 
authentication on the dispatchers *and* to block access to them in
the firewall. You then have to launch the cluster using an *arangosh*
or browser running within the cluster.
