Launching an ArangoDB cluster on multiple machines
--------------------------------------------------

Essentially, one can use the method from [the previous
section](Local.md) to start an ArangoDB cluster on multiple machines as
well. The only changes are that one has to replace all local addresses `127.0.0.1` by the actual IP address of the corresponding server.

If we assume that you want to start you ArangoDB cluster on three different machines with IP addresses

```
192.168.1.1
192.168.1.2
192.168.1.3
```

then the commands you have to use are (you can use host names if they can be resolved to IP addresses on all machines):

On 192.168.1.1:

```
sudo arangod --server.endpoint tcp://0.0.0.0:5001 --agency.my-address tcp://192.168.1.1:5001 --server.authentication false --agency.activate true --agency.size 3 --agency.supervision true --database.directory agency
```

On 192.168.1.2:

```
sudo arangod --server.endpoint tcp://0.0.0.0:5001 --agency.my-address tcp://192.168.1.2:5001 --server.authentication false --agency.activate true --agency.size 3 --agency.supervision true --database.directory agency
```

On 192.168.1.3:

```
sudo arangod --server.endpoint tcp://0.0.0.0:5001 --agency.my-address tcp://192.168.1.3:5001 --server.authentication false --agency.activate true --agency.size 3 --agency.endpoint tcp://192.168.1.1:5001 --agency.endpoint tcp://192.168.1.2:5001 --agency.endpoint tcp://192.168.1.3:5001 --agency.supervision true --database.directory agency
```

On 192.168.1.1:
```
sudo arangod --server.authentication=false --server.endpoint tcp://0.0.0.0:8529 --cluster.my-address tcp://192.168.1.1:8529 --cluster.my-local-info db1 --cluster.my-role PRIMARY --cluster.agency-endpoint tcp://192.168.1.1:5001 --cluster.agency-endpoint tcp://192.168.1.2:5001 --cluster.agency-endpoint tcp://192.168.1.3:5001 --database.directory primary1 &
```

On 192.168.1.2:
```
sudo arangod --server.authentication=false --server.endpoint tcp://0.0.0.0:8530 --cluster.my-address tcp://192.168.1.2:8530 --cluster.my-local-info db2 --cluster.my-role PRIMARY --cluster.agency-endpoint tcp://192.168.1.1:5001 --cluster.agency-endpoint tcp://192.168.1.2:5001 --cluster.agency-endpoint tcp://192.168.1.3:5001 --database.directory primary2 &
```

On 192.168.1.3:
```
arangod --server.authentication=false --server.endpoint tcp://0.0.0.0:8531 --cluster.my-address tcp://192.168.1.3:8531 --cluster.my-local-info coord1 --cluster.my-role COORDINATOR --cluster.agency-endpoint tcp://192.168.1.1:5001 --cluster.agency-endpoint tcp://192.168.1.2:5001 --cluster.agency-endpoint tcp://192.168.1.3:5001 --database.directory coordinator &
```

Obviously, it would no longer be necessary to use different port numbers on different servers. We have chosen to keep all port numbers in comparison to the local setup to minimize the necessary changes.

After having swallowed these longish commands, we hope that you appreciate the simplicity of the setup with Apache Mesos and DC/OS.
