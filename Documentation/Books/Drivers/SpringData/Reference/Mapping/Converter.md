<!-- don't edit here, its from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Converter

## Registering a Spring Converter

The `AbstractArangoConfiguration` provides a convenient way to register Spring `Converter` by overriding the method `customConverters()`.

**Examples**

```Java
@Configuration
public class MyConfiguration extends AbstractArangoConfiguration {

  @Override
  protected Collection<Converter<?, ?>> customConverters() {
    Collection<Converter<?, ?>> converters = new ArrayList<>();
    converters.add(new MyConverter());
    return converters;
  }

}
```

## Implementing a Spring Converter

A `Converter` is used for reading if the source type is of type `VPackSlice` or `DBDocumentEntity`.

A `Converter` is used for writing if the target type is of type `VPackSlice`, `DBDocumentEntity`, `BigInteger`, `BigDecimal`, `java.sql.Date`, `java.sql.Timestamp`, `Instant`, `LocalDate`, `LocalDateTime`, `OffsetDateTime`, `ZonedDateTime`, `Boolean`, `Short`, `Integer`, `Byte`, `Float`, `Double`, `Character`, `String`, `Date`, `Class`, `Enum`, `boolean[]`, `long[]`, `short[]`, `int[]`, `byte[]`, `float[]`, `double[]` or `char[]`.

**Examples**

```Java
public class MyConverter implements Converter<MyObject, VPackSlice> {

  @Override
  public VPackSlice convert(final MyObject source) {
    VPackBuilder builder = new VPackBuilder();
    // map fields of MyObject to builder
    return builder.slice();
  }

}
```

For performance reasons `VPackSlice` should always be used within a converter. If your object is too complexe, you can also use `DBDocumentEntity` to simplify the mapping.
