<!-- don't edit here, it's from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Spring Data ArangoDB - Getting Started

## Supported versions

| Spring Data ArangoDB | Spring Data | ArangoDB    |
| -------------------- | ----------- | ----------- |
| 1.3.x                | 1.13.x      | 3.0\*, 3.1+ |
| 2.3.x                | 2.0.x       | 3.0\*, 3.1+ |
| 3.0.x                | 2.0.x       | 3.0\*, 3.1+ |

Spring Data ArangoDB requires ArangoDB 3.0 or higher - which you can download [here](https://www.arangodb.com/download/) - and Java 8 or higher.

**Note**: ArangoDB 3.0 does not support the default transport protocol
[VelocyStream](https://github.com/arangodb/velocystream). A manual switch to
HTTP is required. See chapter [configuration](#configuration). Also ArangoDB 3.0
does not support geospatial queries.

## Maven

To use Spring Data ArangoDB in your project, your build automation tool needs to be configured to include and use the Spring Data ArangoDB dependency. Example with Maven:

```xml
<dependency>
  <groupId>com.arangodb</groupId>
  <artifactId>arangodb-spring-data</artifactId>
  <version>3.1.0</version>
</dependency>
```

There is a [demonstration app](https://github.com/arangodb/spring-data-demo), which contains common use cases and examples of how to use Spring Data ArangoDB's functionality.

## Configuration

You can use Java to configure your Spring Data environment as show below. Setting up the underlying driver (`ArangoDB.Builder`) with default configuration automatically loads a properties file `arangodb.properties`, if it exists in the classpath.

```java
@Configuration
@EnableArangoRepositories(basePackages = { "com.company.mypackage" })
public class MyConfiguration extends AbstractArangoConfiguration {

  @Override
  public ArangoDB.Builder arango() {
    return new ArangoDB.Builder();
  }

  @Override
  public String database() {
    // Name of the database to be used
    return "example-database";
  }

}
```

The driver is configured with some default values:

| property-key      | description                         | default value |
| ----------------- | ----------------------------------- | ------------- |
| arangodb.host     | ArangoDB host                       | 127.0.0.1     |
| arangodb.port     | ArangoDB port                       | 8529          |
| arangodb.timeout  | socket connect timeout(millisecond) | 0             |
| arangodb.user     | Basic Authentication User           |
| arangodb.password | Basic Authentication Password       |
| arangodb.useSsl   | use SSL connection                  | false         |

To customize the configuration, the parameters can be changed in the Java code.

```java
@Override
public ArangoDB.Builder arango() {
  ArangoDB.Builder arango = new ArangoDB.Builder()
    .host("127.0.0.1")
    .port(8529)
    .user("root");
  return arango;
}
```

In addition you can use the _arangodb.properties_ or a custom properties file to supply credentials to the driver.

_Properties file_

```
arangodb.hosts=127.0.0.1:8529
arangodb.user=root
arangodb.password=
```

_Custom properties file_

```java
@Override
public ArangoDB.Builder arango() {
  InputStream in = MyClass.class.getResourceAsStream("my.properties");
  ArangoDB.Builder arango = new ArangoDB.Builder()
    .loadProperties(in);
  return arango;
}
```

**Note**: When using ArangoDB 3.0 it is required to set the transport protocol to HTTP and fetch the dependency `org.apache.httpcomponents:httpclient`.

```java
@Override
public ArangoDB.Builder arango() {
  ArangoDB.Builder arango = new ArangoDB.Builder()
    .useProtocol(Protocol.HTTP_JSON);
  return arango;
}
```

```xml
<dependency>
  <groupId>org.apache.httpcomponents</groupId>
  <artifactId>httpclient</artifactId>
  <version>4.5.1</version>
</dependency>
```
