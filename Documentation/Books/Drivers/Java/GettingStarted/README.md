<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# ArangoDB Java Driver - Getting Started

## Supported versions

arangodb-java-driver | ArangoDB     | network protocol   | Java version
---------------------|--------------|--------------------|-------------
5.x.x+               | 3.0.0+       | VelocyStream, HTTP | 1.6+
4.2.x+               | 3.0.0+       | VelocyStream, HTTP | 1.6+
4.1.x                | 3.1.0+       | VelocyStream       | 1.6+
3.1.x                | 3.1.0+       |               HTTP | 1.6+
3.0.x                | 3.0.x        |               HTTP | 1.6+
2.7.4                | 2.7.x, 2.8.x |               HTTP | 1.6+

**Note**: VelocyStream is only supported in ArangoDB 3.1 and above.

## Maven

To add the driver to your project with maven, add the following code to your pom.xml
(please use a driver with a version number compatible to your ArangoDB server's version):

ArangoDB 3.x.x

```XML
<dependencies>
  <dependency>
    <groupId>com.arangodb</groupId>
    <artifactId>arangodb-java-driver</artifactId>
    <version>5.0.0</version>
  </dependency>
</dependencies>
```

If you want to test with a snapshot version (e.g. 4.6.0-SNAPSHOT),
add the staging repository of oss.sonatype.org to your pom.xml:

```XML
<repositories>
  <repository>
    <id>arangodb-snapshots</id>
    <url>https://oss.sonatype.org/content/groups/staging</url>
  </repository>
</repositories>
```

## Compile the Java Driver

```
mvn clean install -DskipTests=true -Dgpg.skip=true -Dmaven.javadoc.skip=true -B
```
