<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Driver setup

Setup with default configuration, this automatically loads a properties file
`arangodb.properties` if exists in the classpath:

```Java
// this instance is thread-safe
ArangoDB arangoDB = new ArangoDB.Builder().build();
```

The driver is configured with some default values:

property-key             | description                             | default value
-------------------------|-----------------------------------------|----------------
arangodb.hosts           | ArangoDB hosts                          | 127.0.0.1:8529
arangodb.timeout         | connect & request timeout (millisecond) | 0
arangodb.user            | Basic Authentication User               | 
arangodb.password        | Basic Authentication Password           | 
arangodb.useSsl          | use SSL connection                      | false
arangodb.chunksize       | VelocyStream Chunk content-size (bytes) | 30000
arangodb.connections.max | max number of connections               | 1 VST, 20 HTTP
arangodb.protocol        | used network protocol                   | VST

To customize the configuration the parameters can be changed in the code...

```Java
ArangoDB arangoDB = new ArangoDB.Builder()
  .host("192.168.182.50", 8888)
  .build();
```

... or with a custom properties file (my.properties)

```Java
InputStream in = MyClass.class.getResourceAsStream("my.properties");
ArangoDB arangoDB = new ArangoDB.Builder()
  .loadProperties(in)
  .build();
```

Example for arangodb.properties:

```
arangodb.hosts=127.0.0.1:8529,127.0.0.1:8529
arangodb.user=root
arangodb.password=
```

## Network protocol

The drivers default used network protocol is the binary protocol VelocyStream
which offers the best performance within the driver. To use HTTP, you have to
set the configuration `useProtocol` to `Protocol.HTTP_JSON` for HTTP with JSON
content or `Protocol.HTTP_VPACK` for HTTP with
[VelocyPack](https://github.com/arangodb/velocypack/blob/master/VelocyPack.md) content.

```Java
ArangoDB arangoDB = new ArangoDB.Builder()
  .useProtocol(Protocol.VST)
  .build();
```

In addition to set the configuration for HTTP you have to add the
apache httpclient to your classpath.

```XML
<dependency>
  <groupId>org.apache.httpcomponents</groupId>
  <artifactId>httpclient</artifactId>
  <version>4.5.1</version>
</dependency>
```

**Note**: If you are using ArangoDB 3.0.x you have to set the protocol to
`Protocol.HTTP_JSON` because it is the only one supported.

## SSL

To use SSL, you have to set the configuration `useSsl` to `true` and set a `SSLContext`
(see [example code](https://github.com/arangodb/arangodb-java-driver/blob/master/src/test/java/com/arangodb/example/ssl/SslExample.java)).

```Java
ArangoDB arangoDB = new ArangoDB.Builder()
  .useSsl(true)
  .sslContext(sc)
  .build();
```

## Connection Pooling

The driver supports connection pooling for VelocyStream with a default of 1 and
HTTP with a default of 20 maximum connections per host. To change this value
use the method `maxConnections(Integer)` in `ArangoDB.Builder`.

```Java
ArangoDB arangoDB = new ArangoDB.Builder()
  .maxConnections(8)
  .build();
```

The driver does not explicitly release connections. To avoid exhaustion of
resources when no connection is needed, you can clear the connection pool
(close all connections to the server) or use [connection TTL](#connection-time-to-live).

```Java
arangoDB.shutdown();
```

## Fallback hosts

The driver supports configuring multiple hosts. The first host is used to open a
connection to. When this host is not reachable the next host from the list is used.
To use this feature just call the method `host(String, int)` multiple times.

```Java
ArangoDB arangoDB = new ArangoDB.Builder()
  .host("host1", 8529)
  .host("host2", 8529)
  .build();
```

Since version 4.3 the driver support acquiring a list of known hosts in a
cluster setup or a single server setup with followers. For this the driver has
to be able to successfully open a connection to at least one host to get the
list of hosts. Then it can use this list when fallback is needed. To use this
feature just pass `true` to the method `acquireHostList(boolean)`.

```Java
ArangoDB arangoDB = new ArangoDB.Builder()
  .acquireHostList(true)
  .build();
```

## Load Balancing

Since version 4.3 the driver supports load balancing for cluster setups in
two different ways.

The first one is a round robin load balancing where the driver iterates
through a list of known hosts and performs every request on a different
host than the request before.

```Java
ArangoDB arangoDB = new ArangoDB.Builder()
  .loadBalancingStrategy(LoadBalancingStrategy.ROUND_ROBIN)
  .build();
```

Just like the Fallback hosts feature the round robin load balancing strategy
can use the `acquireHostList` configuration to acquire a list of all known hosts
in the cluster. Do so only requires the manually configuration of only one host.
Because this list is updated frequently it makes load balancing over the whole
cluster very comfortable.

```Java
ArangoDB arangoDB = new ArangoDB.Builder()
  .loadBalancingStrategy(LoadBalancingStrategy.ROUND_ROBIN)
  .acquireHostList(true)
  .build();
```

The second load balancing strategy allows to pick a random host from the
configured or acquired list of hosts and sticks to that host as long as the
connection is open. This strategy is useful for an application - using the driver -
which provides a session management where each session has its own instance of
`ArangoDB` build from a global configured list of hosts. In this case it could
be wanted that every sessions sticks with all its requests to the same host but
not all sessions should use the same host. This load balancing strategy also
works together with `acquireHostList`.

```Java
ArangoDB arangoDB = new ArangoDB.Builder()
  .loadBalancingStrategy(LoadBalancingStrategy.ONE_RANDOM)
  .acquireHostList(true)
  .build();
```

## Connection time to live

Since version 4.4 the driver supports setting a TTL for connections managed
by the internal connection pool.

```Java
ArangoDB arango = new ArangoDB.Builder()
  .connectionTtl(5 * 60 * 1000)
  .build();
```

In this example all connections will be closed/reopened after 5 minutes.

Connection TTL can be disabled setting it to `null`:

```Java
.connectionTtl(null)
```

The default TTL is `null` (no automatic connection closure).
