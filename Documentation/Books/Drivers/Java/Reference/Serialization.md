<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Serialization

## VelocyPack serialization

Since version `4.1.11` you can extend the VelocyPack serialization by
registering additional `VPackModule`s on `ArangoDB.Builder`.

### Java 8 types

GitHub: https://github.com/arangodb/java-velocypack-module-jdk8

Added support for:

- `java.time.Instant`
- `java.time.LocalDate`
- `java.time.LocalDateTime`
- `java.time.ZonedDateTime`
- `java.time.OffsetDateTime`
- `java.time.ZoneId`
- `java.util.Optional`
- `java.util.OptionalDouble`
- `java.util.OptionalInt`
- `java.util.OptionalLong`

```XML
<dependencies>
  <dependency>
    <groupId>com.arangodb</groupId>
    <artifactId>velocypack-module-jdk8</artifactId>
    <version>1.1.0</version>
  </dependency>
</dependencies>
```

```Java
ArangoDB arangoDB = new ArangoDB.Builder().registerModule(new VPackJdk8Module()).build();
```

### Scala types

GitHub: https://github.com/arangodb/java-velocypack-module-scala

Added support for:

- `scala.Option`
- `scala.collection.immutable.List`
- `scala.collection.immutable.Map`
- `scala.math.BigInt`
- `scala.math.BigDecimal`

```XML
<dependencies>
  <dependency>
    <groupId>com.arangodb</groupId>
    <artifactId>velocypack-module-scala</artifactId>
    <version>1.0.2</version>
  </dependency>
</dependencies>
```

```Scala
val arangoDB: ArangoDB = new ArangoDB.Builder().registerModule(new VPackScalaModule).build
```

### Joda-Time

GitHub: https://github.com/arangodb/java-velocypack-module-joda

Added support for:

- `org.joda.time.DateTime`
- `org.joda.time.Instant`
- `org.joda.time.LocalDate`
- `org.joda.time.LocalDateTime`

```XML
<dependencies>
  <dependency>
    <groupId>com.arangodb</groupId>
    <artifactId>velocypack-module-joda</artifactId>
    <version>1.1.1</version>
  </dependency>
</dependencies>
```

```Java
ArangoDB arangoDB = new ArangoDB.Builder().registerModule(new VPackJodaModule()).build();
```

## Use of jackson as an alternative serializer

Since version 4.5.2, the driver supports alternative serializer to de-/serialize
documents, edges and query results. One implementation is
[VelocyJack](https://github.com/arangodb/jackson-dataformat-velocypack#within-arangodb-java-driver)
which is based on [Jackson](https://github.com/FasterXML/jackson) working with
[jackson-dataformat-velocypack](https://github.com/arangodb/jackson-dataformat-velocypack).

**Note**: Any registered custom [serializer/deserializer or module](#custom-serialization)
will be ignored.

## custom serialization

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

## JavaBeans

The driver can serialize/deserialize JavaBeans. They need at least a
constructor without parameter.

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

## Internal fields

To use Arango-internal fields (like \_id, \_key, \_rev, \_from, \_to) in your
JavaBeans, use the annotation `DocumentField`.

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

## Serialized fieldnames

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

## Ignore fields

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

## Custom serializer

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

## Manual serialization

To de-/serialize from and to VelocyPack before or after a database call, use the
`ArangoUtil` from the method `util()` in `ArangoDB`, `ArangoDatabase`,
`ArangoCollection`, `ArangoGraph`, `ArangoEdgeCollection`or `ArangoVertexCollection`.

```Java
ArangoDB arangoDB = new ArangoDB.Builder();
VPackSlice vpack = arangoDB.util().serialize(myObj);
```

```Java
ArangoDB arangoDB = new ArangoDB.Builder();
MyObject myObj = arangoDB.util().deserialize(vpack, MyObject.class);
```
