<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-spark-connector.git / docs/Drivers/ -->
# ArangoDB Spark Connector - Getting Started

## Maven

```XML
<dependencies>
  <dependency>
    <groupId>com.arangodb</groupId>
    <artifactId>arangodb-spark-connector</artifactId>
    <version>1.0.2</version>
  </dependency>
	....
</dependencies>
```

## SBT

```Json
libraryDependencies += "com.arangodb" % "arangodb-spark-connector" % "1.0.2"
```

## Configuration

| property-key                   | description                            | default value  |
| ------------------------------ | -------------------------------------- | -------------- |
| arangodb.hosts                 | comma separated list of ArangoDB hosts | 127.0.0.1:8529 |
| arangodb.user                  | basic authentication user              | root           |
| arangodb.password              | basic authentication password          |                |
| arangodb.protocol              | network protocol                       | VST            |
| arangodb.useSsl                | use SSL connection                     | false          |
| arangodb.ssl.keyStoreFile      | SSL certificate keystore file          |                |
| arangodb.ssl.passPhrase        | SSL pass phrase                        |                |
| arangodb.ssl.protocol          | SSL protocol                           | TLS            |
| arangodb.maxConnections        | max number of connections per host     | 1              |
| arangodb.acquireHostList       | auto acquire list of available hosts   | false          |
| arangodb.loadBalancingStrategy | load balancing strategy to be used     | NONE           |

## Setup SparkContext

**Scala**

```Scala
val conf = new SparkConf()
    .set("arangodb.hosts", "127.0.0.1:8529")
    .set("arangodb.user", "myUser")
    .set("arangodb.password", "myPassword")
    ...

val sc = new SparkContext(conf)
```

**Java**

```Java
SparkConf conf = new SparkConf()
    .set("arangodb.hosts", "127.0.0.1:8529")
    .set("arangodb.user", "myUser")
    .set("arangodb.password", "myPassword");
    ...

JavaSparkContext sc = new JavaSparkContext(conf);
```
