# Monitoring ArangoDB Cluster network usage

## Problem

We run a cluster and want to know whether the traffic is unbalanced or something like that. We want a cheap estimate which host has how much traffic.

## Solution

As we already run [Collectd](http://collectd.org) as our metric-hub, we want to utilize it to also give us these figures. A very cheap way to generate these values are the counters in the IPTables firewall of our system.

###Ingredients

For this recipe you need to install the following tools:

  * [collectd](https://collectd.org/): the aggregation Daemon
  * [kcollectd](https://www.forwiss.uni-passau.de/~berberic/Linux/kcollectd.html) for inspecting the data
  * [iptables](http://en.wikipedia.org/wiki/Iptables) - should come with your Linux distribution
  * [ferm](http://ferm.foo-projects.org/download/2.2/ferm.html#basic_iptables_match_keywords) for compact firewall code
  * we base on [Monitoring with Collecd recipe](Collectd.md) for understanding the basics about collectd

### Getting the state and the Ports of your cluster
Now we need to find out the current configuration of our cluster. For the time being we assume you simply issued

    ./scripts/startLocalCluster.sh

to get you set up. So you know you've got two DB-Servers - one Coordinator, one agent:

    ps -eaf |grep arango
    arangod    21406     1  1 16:59 pts/14   00:00:00 bin/etcd-arango --data-dir /var/tmp/tmp-21550-1347489353/shell_server/agentarango4001 --name agentarango4001 --bind-addr 127.0.0.1:4001 --addr 127.0.0.1:4001 --peer-bind-addr 127.0.0.1:7001 --peer-addr 127.0.0.1:7001 --initial-cluster-state new --initial-cluster agentarango4001=http://127.0.0.1:7001
    arangod    21408     1  4 16:56 pts/14   00:00:01 bin/arangod --database.directory cluster/data8629 --cluster.agency-endpoint tcp://localhost:4001 --cluster.my-address tcp://localhost:8629 --server.endpoint tcp://localhost:8629 --cluster.my-local-info dbserver:localhost:8629 --log.file cluster/8629.log --cluster.my-id Pavel
    arangod    21410     1  5 16:56 pts/14   00:00:02 bin/arangod --database.directory cluster/data8630 --cluster.agency-endpoint tcp://localhost:4001 --cluster.my-address tcp://localhost:8630 --server.endpoint tcp://localhost:8630 --cluster.my-local-info dbserver:localhost:8630 --log.file cluster/8630.log --cluster.my-id Perry
    arangod    21416     1  5 16:56 pts/14   00:00:02 bin/arangod --database.directory cluster/data8530 --cluster.agency-endpoint tcp://localhost:4001 --cluster.my-address tcp://localhost:8530 --server.endpoint tcp://localhost:8530 --cluster.my-local-info coordinator:localhost:8530 --log.file cluster/8530.log --cluster.my-id Claus

We can now check which ports they occupied:

    netstat -aplnt |grep arango
    tcp        0      0 127.0.0.1:7001          0.0.0.0:*               LISTEN      21406/etcd-arango
    tcp        0      0 127.0.0.1:4001          0.0.0.0:*               LISTEN      21406/etcd-arango
    tcp        0      0 127.0.0.1:8530          0.0.0.0:*               LISTEN      21416/arangod
    tcp        0      0 127.0.0.1:8629          0.0.0.0:*               LISTEN      21408/arangod
    tcp        0      0 127.0.0.1:8630          0.0.0.0:*               LISTEN      21410/arangod

* The agent has 7001 and 4001. Since it's running in single server mode its cluster port (7001) should not show any traffic, port 4001 is the interesting one.
* Claus - This is the coordinator. Your Application will talk to it on port 8530
* Pavel - This is the first DB-Server; Claus will talk to it on port 8629
* Perry - This is the second DB-Server; Claus will talk to it on port 8630

###Configuring IPTables / ferm
Since the usual solution using shell scripts calling iptables brings the [DRY principle](http://en.wikipedia.org/wiki/Don%27t_repeat_yourself) to a grinding hold, we need something better. Here [ferm](http://ferm.foo-projects.org/download/2.2/ferm.html#basic_iptables_match_keywords) comes to the rescue - It enables you to produce very compact and well readable firewall configurations.

According to the ports we found in the last section, we will configure our firewall in `/etc/ferm/ferm.conf`, and put the identities into the comments so we have a persistent naming scheme:

    # blindly forward these to the accounting chain:
    @def $ARANGO_RANGE=4000:9000;

    @def &TCP_ACCOUNTING($PORT, $COMMENT, $SRCCHAIN) = {
        @def $FULLCOMMENT=@cat($COMMENT, "_", $SRCCHAIN);
        dport $PORT mod comment comment $FULLCOMMENT NOP;
    }

    @def &ARANGO_ACCOUNTING($CHAINNAME) = {
    # The coordinators:
        &TCP_ACCOUNTING(8530, "Claus", $CHAINNAME);
    # The db-servers:
        &TCP_ACCOUNTING(8629, "Pavel", $CHAINNAME);
        &TCP_ACCOUNTING(8630, "Perry", $CHAINNAME);
    # The agency:
        &TCP_ACCOUNTING(4001, "etcd_client", $CHAINNAME);
    # it shouldn't talk to itself if it is only running with a single instance:
        &TCP_ACCOUNTING(7007, "etcd_cluster", $CHAINNAME);
    }

    table filter {
        chain INPUT {
            proto tcp dport $ARANGO_RANGE @subchain "Accounting" {
                &ARANGO_ACCOUNTING("input");
            }
            policy DROP;

            # connection tracking
            mod state state INVALID DROP;
            mod state state (ESTABLISHED RELATED) ACCEPT;

            # allow local packet
            interface lo ACCEPT;

            # respond to ping
            proto icmp ACCEPT; 

            # allow IPsec
            proto udp dport 500 ACCEPT;
            proto (esp ah) ACCEPT;

            # allow SSH connections
            proto tcp dport ssh ACCEPT;
        }
        chain OUTPUT {
            policy ACCEPT;

            proto tcp dport $ARANGO_RANGE @subchain "Accounting" {
                &ARANGO_ACCOUNTING("output");
            }

            # connection tracking
            #mod state state INVALID DROP;
            mod state state (ESTABLISHED RELATED) ACCEPT;
        }
        chain FORWARD {
            policy DROP;

            # connection tracking
            mod state state INVALID DROP;
            mod state state (ESTABLISHED RELATED) ACCEPT;
        }
    }

**Note**: This is a very basic configuration, mainly with the purpose to demonstrate the accounting feature - so don't run this in production)

After activating it interactively with

    ferm -i /etc/ferm/ferm.conf

We now use the iptables command line utility directly to review the status our current setting:

    iptables -L -nvx
    Chain INPUT (policy DROP 85 packets, 6046 bytes)
        pkts      bytes target     prot opt in     out     source               destination
        7636  1821798 Accounting  tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpts:4000:9000
           0        0 DROP       all  --  *      *       0.0.0.0/0            0.0.0.0/0            state INVALID
       14700 14857709 ACCEPT     all  --  *      *       0.0.0.0/0            0.0.0.0/0            state RELATED,ESTABLISHED
         130     7800 ACCEPT     all  --  lo     *       0.0.0.0/0            0.0.0.0/0
           0        0 ACCEPT     icmp --  *      *       0.0.0.0/0            0.0.0.0/0
           0        0 ACCEPT     udp  --  *      *       0.0.0.0/0            0.0.0.0/0            udp dpt:500
           0        0 ACCEPT     esp  --  *      *       0.0.0.0/0            0.0.0.0/0
           0        0 ACCEPT     ah   --  *      *       0.0.0.0/0            0.0.0.0/0
           0        0 ACCEPT     tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpt:22

    Chain FORWARD (policy DROP 0 packets, 0 bytes)
        pkts      bytes target     prot opt in     out     source               destination
           0        0 DROP       all  --  *      *       0.0.0.0/0            0.0.0.0/0            state INVALID
           0        0 ACCEPT     all  --  *      *       0.0.0.0/0            0.0.0.0/0            state RELATED,ESTABLISHED

    Chain OUTPUT (policy ACCEPT 296 packets, 19404 bytes)
        pkts      bytes target     prot opt in     out     source               destination
        7720  1882404 Accounting  tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpts:4000:9000
       14575 14884356 ACCEPT     all  --  *      *       0.0.0.0/0            0.0.0.0/0            state RELATED,ESTABLISHED

    Chain Accounting (2 references)
        pkts      bytes target     prot opt in     out     source               destination
         204    57750            tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpt:8530 /* Claus_input */
          20    17890            tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpt:8629 /* Pavel_input */
         262    97352            tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpt:8630 /* Perry_input */
        2604   336184            tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpt:4001 /* etcd_client_input */
           0        0            tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpt:7007 /* etcd_cluster_input */
         204    57750            tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpt:8530 /* Claus_output */
          20    17890            tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpt:8629 /* Pavel_output */
         262    97352            tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpt:8630 /* Perry_output */
        2604   336184            tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpt:4001 /* etcd_client_output */
           0        0            tcp  --  *      *       0.0.0.0/0            0.0.0.0/0            tcp dpt:7007 /* etcd_cluster_output */


You can see nicely the Accounting sub-chain with our comments. These should be pretty straight forward to match.
We also see the **pkts** and **bytes** columns. They contain the current value of these counters of your system.

Read more about [linux firewalling](http://lartc.org) and [ferm configuration](http://ferm.foo-projects.org/download/2.2/ferm.html) to be sure you do the right thing.

### Configuring Collectd to pick up these values
Since your system now generates these numbers, we want to configure collectd with its [iptables plugin](https://collectd.org/wiki/index.php/Plugin:IPTables) to aggregate them.

We do so in the `/etc/collectd/collectd.conf.d/iptables.conf`:

    LoadPlugin iptables
    <Plugin iptables>
      Chain filter "Accounting" "Claus_input"
      Chain filter "Accounting" "Pavel_input"
      Chain filter "Accounting" "Perry_input"
      Chain filter "Accounting" "etcd_client_input"
      Chain filter "Accounting" "etcd_cluster_input"
      Chain filter "Accounting" "Claus_output"
      Chain filter "Accounting" "Pavel_output"
      Chain filter "Accounting" "Perry_output"
      Chain filter "Accounting" "etcd_client_output"
      Chain filter "Accounting" "etcd_cluster_output"
    </Plugin>

Now we restart collectd with `/etc/init.d/collectd restart`, watch the syslog for errors. If everything is OK, our values should show up in:

    /var/lib/collectd/rrd/localhost/iptables-filter-Accounting/ipt_packets-Claus_output.rrd

We can inspect our values with kcollectd:

![Kcollectd screenshot](/assets/MonitoringWithCollectd/KCollectdIPtablesAccounting.png)

**Author:** [Wilfried Goesgens](https://github.com/dothebart)

**Tags:**  #monitoring
