<!-- don't edit here, its from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# ArangoDB Java Driver - Getting Started

## Supported versions

<table>
<tr><th>arangodb-java-driver</th><th>ArangoDB</th><th>network protocol</th><th>Java version</th></tr>
<tr><td>4.2.x+</td><td>3.0.0+</td><td>VelocyStream, HTTP</td><td>1.6+</td></tr>
<tr><td>4.1.x</td><td>3.1.0+</td><td>VelocyStream</td><td>1.6+</td></tr>
<tr><td>3.1.x</td><td>3.1.0+</td><td>HTTP</td><td>1.6+</td></tr>
<tr><td>3.0.x</td><td>3.0.x</td><td>HTTP</td><td>1.6+</td></tr>
<tr><td>2.7.4</td><td>2.7.x, 2.8.x</td><td>HTTP</td><td>1.6+</td></tr>
</table>

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
    <version>4.6.0</version>
  </dependency>
</dependencies>
```

If you want to test with a snapshot version (e.g. 4.6.0-SNAPSHOT), add the staging repository of oss.sonatype.org to your pom.xml:

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
