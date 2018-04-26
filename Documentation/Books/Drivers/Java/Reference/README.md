<!-- don't edit here, its from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# ArangoDB Java Driver - Reference

# Driver setup

Setup with default configuration, this automatically loads a properties file arangodb.properties if exists in the classpath:

```Java
  // this instance is thread-safe
  ArangoDB arangoDB = new ArangoDB.Builder().build();
```

The driver is configured with some default values:

<table>
<tr><th>property-key</th><th>description</th><th>default value</th></tr>
<tr><td>arangodb.hosts</td><td>ArangoDB hosts</td><td>127.0.0.1:8529</td></tr>
<tr><td>arangodb.timeout</td><td>socket connect timeout(millisecond)</td><td>0</td></tr>
<tr><td>arangodb.user</td><td>Basic Authentication User</td><td></td></tr>
<tr><td>arangodb.password</td><td>Basic Authentication Password</td><td></td></tr>
<tr><td>arangodb.useSsl</td><td>use SSL connection</td><td>false</td></tr>
<tr><td>arangodb.chunksize</td><td>VelocyStream Chunk content-size(bytes)</td><td>30000</td></tr>
<tr><td>arangodb.connections.max</td><td>max number of connections</td><td>1 VST, 20 HTTP</td></tr>
<tr><td>arangodb.protocol</td><td>used network protocol</td><td>VST</td></tr>
</table>

To customize the configuration the parameters can be changed in the code...

```Java
  ArangoDB arangoDB = new ArangoDB.Builder().host("192.168.182.50", 8888).build();
```

... or with a custom properties file (my.properties)

```Java
  InputStream in = MyClass.class.getResourceAsStream("my.properties");
  ArangoDB arangoDB = new ArangoDB.Builder().loadProperties(in).build();
```

Example for arangodb.properties:

```Java
  arangodb.hosts=127.0.0.1:8529,127.0.0.1:8529
  arangodb.user=root
  arangodb.password=
```

## Network protocol

The drivers default used network protocol is the binary protocol VelocyStream which offers the best performance within the driver. To use HTTP, you have to set the configuration `useProtocol` to `Protocol.HTTP_JSON` for HTTP with Json content or `Protocol.HTTP_VPACK` for HTTP with [VelocyPack](https://github.com/arangodb/velocypack/blob/master/VelocyPack.md) content.

```Java
  ArangoDB arangoDB = new ArangoDB.Builder().useProtocol(Protocol.VST).build();
```

In addition to set the configuration for HTTP you have to add the apache httpclient to your classpath.

```XML
<dependency>
  <groupId>org.apache.httpcomponents</groupId>
  <artifactId>httpclient</artifactId>
  <version>4.5.1</version>
</dependency>
```

**Note**: If you are using ArangoDB 3.0.x you have to set the protocol to `Protocol.HTTP_JSON` because it is the only one supported.

## SSL

To use SSL, you have to set the configuration `useSsl` to `true` and set a `SSLContext`. (see [example code](../src/test/java/com/arangodb/example/ssl/SslExample.java))

```Java
  ArangoDB arangoDB = new ArangoDB.Builder().useSsl(true).sslContext(sc).build();
```

## Connection Pooling

The driver supports connection pooling for VelocyStream with a default of 1 and HTTP with a default of 20 maximum connections. To change this value use the method `maxConnections(Integer)` in `ArangoDB.Builder`.

```Java
  ArangoDB arangoDB = new ArangoDB.Builder().maxConnections(8).build();
```

The driver does not explicitly release connections. To avoid exhaustion of resources when no connection is needed, you can clear the connection pool (close all connections to the server) or use [connection TTL](#connection-time-to-live).

```Java
arangoDB.shutdown();
```

## Fallback hosts

The driver supports configuring multiple hosts. The first host is used to open a connection to. When this host is not reachable the next host from the list is used. To use this feature just call the method `host(String, int)` multiple times.

```Java
  ArangoDB arangoDB = new ArangoDB.Builder().host("host1", 8529).host("host2", 8529).build();
```

Since version 4.3 the driver support acquiring a list of known hosts in a cluster setup or a single server setup with followers. For this the driver has to be able to successfully open a connection to at least one host to get the list of hosts. Then it can use this list when fallback is needed. To use this feature just pass `true` to the method `acquireHostList(boolean)`.

```Java
  ArangoDB arangoDB = new ArangoDB.Builder().acquireHostList(true).build();
```

## Load Balancing

Since version 4.3 the driver supports load balancing for cluster setups in two different ways.

The first one is a round robin load balancing where the driver iterates through a list of known hosts and performs every request on a different host than the request before. This load balancing strategy only work when the maximun of connections is greater 1.

```Java
  ArangoDB arangoDB = new ArangoDB.Builder().loadBalancingStrategy(LoadBalancingStrategy.ROUND_ROBIN).maxConnections(8).build();
```

Just like the Fallback hosts feature the round robin load balancing strategy can use the `acquireHostList` configuration to acquire a list of all known hosts in the cluster. Do so only requires the manually configuration of only one host. Because this list is updated frequently it makes load balancing over the whole cluster very comfortable.

```Java
  ArangoDB arangoDB = new ArangoDB.Builder().loadBalancingStrategy(LoadBalancingStrategy.ROUND_ROBIN).maxConnections(8).acquireHostList(true).build();
```

The second load balancing strategy allows to pick a random host from the configured or acquired list of hosts and sticks to that host as long as the connection is open. This strategy is useful for an application - using the driver - which provides a session management where each session has its own instance of `ArangoDB` build from a global configured list of hosts. In this case it could be wanted that every sessions sticks with all its requests to the same host but not all sessions should use the same host. This load balancing strategy also works together with `acquireHostList`.

```Java
  ArangoDB arangoDB = new ArangoDB.Builder().loadBalancingStrategy(LoadBalancingStrategy.ONE_RANDOM).acquireHostList(true).build();
```

## Connection time to live

Since version 4.4 the driver supports setting a TTL for connections managed by the internal connection pool. Setting a TTL helps when using load balancing strategy `ROUND_ROBIN`, because as soon as a coordinator goes down, every open connection to that host will be closed and opened again with another target coordinator. As long as the driver does not have to open new connections (all connections in the pool are used) it will use only the coordinators which never went down. To use the downed coordinator again, when it is running again, the connections in the connection pool have to be closed and opened again with the target host mentioned by the load balancing startegy. To achieve this you can manually call `ArangoDB.shutdown` in your client code or use the TTL for connection so that a downed coordinator (which is then brought up again) will be used again after a certain time.

```Java
ArangoDB arango = new ArangoDB.Builder().connectionTtl(5 * 60 * 1000).build();
```

In this example all connections will be closed/reopened after 5 minutes.

## configure VelocyPack serialization

Since version `4.1.11` you can extend the VelocyPack serialization by registering additional `VPackModule`s on `ArangoDB.Builder`.

### Java 8 types

Added support for:

* java.time.Instant
* java.time.LocalDate
* java.time.LocalDateTime
* java.util.Optional;
* java.util.OptionalDouble;
* java.util.OptionalInt;
* java.util.OptionalLong;

```XML
<dependencies>
  <dependency>
    <groupId>com.arangodb</groupId>
    <artifactId>velocypack-module-jdk8</artifactId>
    <version>1.0.2</version>
  </dependency>
</dependencies>
```

```Java
ArangoDB arangoDB = new ArangoDB.Builder().registerModule(new VPackJdk8Module()).build();
```

### Scala types

Added support for:

* scala.Option
* scala.collection.immutable.List
* scala.collection.immutable.Map

```XML
<dependencies>
  <dependency>
    <groupId>com.arangodb</groupId>
    <artifactId>velocypack-module-scala</artifactId>
    <version>1.0.1</version>
  </dependency>
</dependencies>
```

```Scala
val arangoDB: ArangoDB = new ArangoDB.Builder().registerModule(new VPackScalaModule).build
```

### Joda-Time

Added support for:

* org.joda.time.DateTime;
* org.joda.time.Instant;
* org.joda.time.LocalDate;
* org.joda.time.LocalDateTime;

```XML
<dependencies>
  <dependency>
    <groupId>com.arangodb</groupId>
    <artifactId>velocypack-module-joda</artifactId>
    <version>1.0.0</version>
  </dependency>
</dependencies>
```

```Java
ArangoDB arangoDB = new ArangoDB.Builder().registerModule(new VPackJodaModule()).build();
```

## custom serializer

```Java
  ArangoDB arangoDB = new ArangoDB.Builder().registerModule(new VPackModule() {
    @Override
    public <C extends VPackSetupContext<C>> void setup(final C context) {
      context.registerDeserializer(MyObject.class, new VPackDeserializer<MyObject>() {
        @Override
        public MyObject deserialize(VPackSlice parent,VPackSlice vpack,
            VPackDeserializationContext context) throws VPackException {
          MyObject obj = new MyObject();
          obj.setName(vpack.get("name").getAsString());
          return obj;
        }
      });
      context.registerSerializer(MyObject.class, new VPackSerializer<MyObject>() {
        @Override
        public void serialize(VPackBuilder builder,String attribute,MyObject value,
            VPackSerializationContext context) throws VPackException {
          builder.add(attribute, ValueType.OBJECT);
          builder.add("name", value.getName());
          builder.close();
        }
      });
    }
  }).build();
```

# Manipulating databases

## create database

```Java
  // create database
  arangoDB.createDatabase("myDatabase");
```

## drop database

```Java
  // drop database
  arangoDB.db("myDatabase").drop();
```

# Manipulating collections

## create collection

```Java
  // create collection
  arangoDB.db("myDatabase").createCollection("myCollection", null);
```

## drop collection

```Java
  // delete collection
  arangoDB.db("myDatabase").collection("myCollection").drop();
```

## truncate collection

```Java
  arangoDB.db("myDatabase").collection("myCollection").truncate();
```

# Basic Document operations

Every document operations works with POJOs (e.g. MyObject), VelocyPack (VPackSlice) and Json (String).

For the next examples we use a small object:

```Java
  public class MyObject {

    private String key;
    private String name;
    private int age;

    public MyObject(String name, int age) {
      this();
      this.name = name;
      this.age = age;
    }

    public MyObject() {
      super();
    }

    /*
     *  + getter and setter
     */

  }
```

## insert document

```Java
  MyObject myObject = new MyObject("Homer", 38);
  arangoDB.db("myDatabase").collection("myCollection").insertDocument(myObject);
```

When creating a document, the attributes of the object will be stored as key-value pair
E.g. in the previous example the object was stored as follows:

```properties
  "name" : "Homer"
  "age" : "38"
```

## delete document

```Java
  arangoDB.db("myDatabase").collection("myCollection").deleteDocument(myObject.getKey());
```

## update document

```Java
  arangoDB.db("myDatabase").collection("myCollection").updateDocument(myObject.getKey(), myUpdatedObject);
```

## replace document

```Java
  arangoDB.db("myDatabase").collection("myCollection").replaceDocument(myObject.getKey(), myObject2);
```

## read document as JavaBean

```Java
  MyObject document = arangoDB.db("myDatabase").collection("myCollection").getDocument(myObject.getKey(), MyObject.class);
  document.getName();
  document.getAge();
```

## read document as VelocyPack

```Java
  VPackSlice document = arangoDB.db("myDatabase").collection("myCollection").getDocument(myObject.getKey(), VPackSlice.class);
  document.get("name").getAsString();
  document.get("age").getAsInt();
```

## read document as Json

```Java
  String json = arangoDB.db("myDatabase").collection("myCollection").getDocument(myObject.getKey(), String.class);
```

## read document by key

```Java
  arangoDB.db("myDatabase").collection("myCollection").getDocument("myKey", MyObject.class);
```

## read document by id

```Java
  arangoDB.db("myDatabase").getDocument("myCollection/myKey", MyObject.class);
```

# Multi Document operations

## insert documents

```Java
  Collection<MyObject> documents = new ArrayList<>;
  documents.add(myObject1);
  documents.add(myObject2);
  documents.add(myObject3);
  arangoDB.db("myDatabase").collection("myCollection").insertDocuments(documents);
```

## delete documents

```Java
  Collection<String> keys = new ArrayList<>;
  keys.add(myObject1.getKey());
  keys.add(myObject2.getKey());
  keys.add(myObject3.getKey());
  arangoDB.db("myDatabase").collection("myCollection").deleteDocuments(keys);
```

## update documents

```Java
  Collection<MyObject> documents = new ArrayList<>;
  documents.add(myObject1);
  documents.add(myObject2);
  documents.add(myObject3);
  arangoDB.db("myDatabase").collection("myCollection").updateDocuments(documents);
```

## replace documents

```Java
  Collection<MyObject> documents = new ArrayList<>;
  documents.add(myObject1);
  documents.add(myObject2);
  documents.add(myObject3);
  arangoDB.db("myDatabase").collection("myCollection").replaceDocuments(documents);
```

# AQL

## Executing an AQL statement

Every AQL operations works with POJOs (e.g. MyObject), VelocyPack (VPackSlice) and Json (String).

E.g. get all Simpsons aged 3 or older in ascending order:

```Java
  arangoDB.createDatabase("myDatabase");
  ArangoDatabase db = arangoDB.db("myDatabase");

  db.createCollection("myCollection");
  ArangoCollection collection = db.collection("myCollection");

  collection.insertDocument(new MyObject("Homer", 38));
  collection.insertDocument(new MyObject("Marge", 36));
  collection.insertDocument(new MyObject("Bart", 10));
  collection.insertDocument(new MyObject("Lisa", 8));
  collection.insertDocument(new MyObject("Maggie", 2));

  Map<String, Object> bindVars = new HashMap<>();
  bindVars.put("age", 3);

  ArangoCursor<MyObject> cursor = db.query(query, bindVars, null, MyObject.class);

  for(; cursor.hasNext;) {
    MyObject obj = cursor.next();
    System.out.println(obj.getName());
  }
```

or return the AQL result as VelocyPack:

```Java
  ArangoCursor<VPackSlice> cursor = db.query(query, bindVars, null, VPackSlice.class);

  for(; cursor.hasNext;) {
    VPackSlice obj = cursor.next();
    System.out.println(obj.get("name").getAsString());
  }
```

**Note**: The parameter `type` in `query()` has to match the result of the query, otherwise you get an VPackParserException. E.g. you set `type` to `BaseDocument` or a POJO and the query result is an array or simple type, you get an VPackParserException caused by VPackValueTypeException: Expecting type OBJECT.

# Graphs

The driver supports the [graph api](https://docs.arangodb.com/HTTP/Gharial/index.html).

Some of the basic graph operations are described in the following:

## add graph

A graph consists of vertices and edges (stored in collections). Which collections are used within a graph is defined via edge definitions. A graph can contain more than one edge definition, at least one is needed.

```Java
  Collection<EdgeDefinition> edgeDefinitions = new ArrayList<>();
  EdgeDefinition edgeDefinition = new EdgeDefinition();
  // define the edgeCollection to store the edges
  edgeDefinition.collection("myEdgeCollection");
  // define a set of collections where an edge is going out...
  edgeDefinition.from("myCollection1", "myCollection2");

  // repeat this for the collections where an edge is going into
  edgeDefinition.to("myCollection1", "myCollection3");

  edgeDefinitions.add(edgeDefinition);

  // A graph can contain additional vertex collections, defined in the set of orphan collections
  GraphCreateOptions options = new GraphCreateOptions();
  options.orphanCollections("myCollection4", "myCollection5");

  // now it's possible to create a graph
  arangoDB.db("myDatabase").createGraph("myGraph", edgeDefinitions, options);
```

## delete graph

A graph can be deleted by its name

```Java
  arangoDB.db("myDatabase").graph("myGraph").drop();
```

## add vertex

Vertices are stored in the vertex collections defined above.

```Java
  MyObject myObject1 = new MyObject("Homer", 38);
  MyObject myObject2 = new MyObject("Marge", 36);
  arangoDB.db("myDatabase").graph("myGraph").vertexCollection("collection1").insertVertex(myObject1, null);
  arangoDB.db("myDatabase").graph("myGraph").vertexCollection("collection3").insertVertex(myObject2, null);
```

## add edge

Now an edge can be created to set a relation between vertices

```Java
  arangoDB.db("myDatabase").graph("myGraph").edgeCollection("myEdgeCollection").insertEdge(myEdgeObject, null);
```

# Foxx

## call a service

```Java
  Request request = new Request("mydb", RequestType.GET, "/my/foxx/service")
  Response response = arangoDB.execute(request);
```

# User management

If you are using [authentication](https://docs.arangodb.com/Manual/GettingStarted/Authentication.html) you can manage users with the driver.

## add user

```Java
  //username, password
  arangoDB.createUser("myUser", "myPassword");
```

## delete user

```Java
  arangoDB.deleteUser("myUser");
```

## list users

```Java
  Collection<UserResult> users = arangoDB.getUsers();
  for(UserResult user : users) {
    System.out.println(user.getUser())
  }
```

## grant user access

```Java
  arangoDB.db("myDatabase").grantAccess("myUser");
```

## revoke user access

```Java
  arangoDB.db("myDatabase").revokeAccess("myUser");
```

# Serialization

## JavaBeans

The driver can serialize/deserialize JavaBeans. They need at least a constructor without parameter.

```Java
  public class MyObject {

    private String name;
    private Gender gender;
    private int age;

    public MyObject() {
      super();
    }

  }
```

## internal fields

To use Arango-internal fields (like \_id, \_key, \_rev, \_from, \_to) in your JavaBeans, use the annotation `DocumentField`.

```Java
  public class MyObject {

    @DocumentField(Type.KEY)
    private String key;

    private String name;
    private Gender gender;
    private int age;

    public MyObject() {
      super();
    }

  }
```

## serialized fieldnames

To use a different serialized name for a field, use the annotation `SerializedName`.

```Java
  public class MyObject {

    @SerializedName("title")
    private String name;

    private Gender gender;
    private int age;

    public MyObject() {
      super();
    }

  }
```

## ignore fields

To ignore fields at serialization/deserialization, use the annotation `Expose`

```Java
  public class MyObject {

    @Expose
    private String name;
    @Expose(serialize = true, deserialize = false)
    private Gender gender;
    private int age;

    public MyObject() {
      super();
    }

  }
```

## custom serializer

```Java
  ArangoDB arangoDB = new ArangoDB.Builder().registerModule(new VPackModule() {
    @Override
    public <C extends VPackSetupContext<C>> void setup(final C context) {
      context.registerDeserializer(MyObject.class, new VPackDeserializer<MyObject>() {
        @Override
        public MyObject deserialize(VPackSlice parent,VPackSlice vpack,
            VPackDeserializationContext context) throws VPackException {
          MyObject obj = new MyObject();
          obj.setName(vpack.get("name").getAsString());
          return obj;
        }
      });
      context.registerSerializer(MyObject.class, new VPackSerializer<MyObject>() {
        @Override
        public void serialize(VPackBuilder builder,String attribute,MyObject value,
            VPackSerializationContext context) throws VPackException {
          builder.add(attribute, ValueType.OBJECT);
          builder.add("name", value.getName());
          builder.close();
        }
      });
    }
  }).build();
```

## manually serialization

To de-/serialize from and to VelocyPack before or after a database call, use the `ArangoUtil` from the method `util()` in `ArangoDB`, `ArangoDatabase`, `ArangoCollection`, `ArangoGraph`, `ArangoEdgeCollection`or `ArangoVertexCollection`.

```Java
  ArangoDB arangoDB = new ArangoDB.Builder();
  VPackSlice vpack = arangoDB.util().serialize(myObj);
```

```Java
  ArangoDB arangoDB = new ArangoDB.Builder();
  MyObject myObj = arangoDB.util().deserialize(vpack, MyObject.class);
```
