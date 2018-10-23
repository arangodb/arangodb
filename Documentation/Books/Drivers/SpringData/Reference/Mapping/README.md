<!-- don't edit here, it's from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Mapping

In this section we will describe the features and conventions for mapping Java objects to documents and how to override those conventions with annotation based mapping metadata.

## Conventions

- The Java class name is mapped to the collection name
- The non-static fields of a Java object are used as fields in the stored document
- The Java field name is mapped to the stored document field name
- All nested Java object are stored as nested objects in the stored document
- The Java class needs a constructor which meets the following criteria:
  - in case of a single constructor:
    - a non-parameterized constructor or
    - a parameterized constructor
  - in case of multiple constructors:
    - a non-parameterized constructor or
    - a parameterized constructor annotated with `@PersistenceConstructor`

## Type conventions

ArangoDB uses [VelocyPack](https://github.com/arangodb/velocypack) as it's internal storage format which supports a large number of data types. In addition Spring Data ArangoDB offers - with the underlying Java driver - built-in converters to add additional types to the mapping.

| Java type                | VelocyPack type               |
| ------------------------ | ----------------------------- |
| java.lang.String         | string                        |
| java.lang.Boolean        | bool                          |
| java.lang.Integer        | signed int 4 bytes, smallint  |
| java.lang.Long           | signed int 8 bytes, smallint  |
| java.lang.Short          | signed int 2 bytes, smallint  |
| java.lang.Double         | double                        |
| java.lang.Float          | double                        |
| java.math.BigInteger     | string                        |
| java.math.BigDecimal     | string                        |
| java.lang.Number         | double                        |
| java.lang.Character      | string                        |
| java.util.UUID           | string                        |
| java.lang.byte[]         | string (Base64)               |
| java.util.Date           | string (date-format ISO 8601) |
| java.sql.Date            | string (date-format ISO 8601) |
| java.sql.Timestamp       | string (date-format ISO 8601) |
| java.time.Instant        | string (date-format ISO 8601) |
| java.time.LocalDate      | string (date-format ISO 8601) |
| java.time.LocalDateTime  | string (date-format ISO 8601) |
| java.time.OffsetDateTime | string (date-format ISO 8601) |
| java.time.ZonedDateTime  | string (date-format ISO 8601) |

## Type mapping

As collections in ArangoDB can contain documents of various types, a mechanism to retrieve the correct Java class is required. The type information of properties declared in a class may not be enough to restore the original class (due to inheritance). If the declared complex type and the actual type do not match, information about the actual type is stored together with the document. This is necessary to restore the correct type when reading from the DB. Consider the following example:

```java
public class Person {
    private String name;
    private Address homeAddress;
    // ...

    // getters and setters omitted
}

public class Employee extends Person {
    private Address workAddress;
    // ...

    // getters and setters omitted
}

public class Address {
    private final String street;
    private final String number;
    // ...

    public Address(String street, String number) {
        this.street = street;
        this.number = number;
    }

    // getters omitted
}

@Document
public class Company {
    @Key
    private String key;
    private Person manager;

    // getters and setters omitted
}

Employee manager = new Employee();
manager.setName("Jane Roberts");
manager.setHomeAddress(new Address("Park Avenue", "432/64"));
manager.setWorkAddress(new Address("Main Street",  "223"));
Company comp = new Company();
comp.setManager(manager);
```

The serialized document for the DB looks like this:

```json
{
  "manager": {
    "name": "Jane Roberts",
    "homeAddress": {
      "street": "Park Avenue",
      "number": "432/64"
    },
    "workAddress": {
      "street": "Main Street",
      "number": "223"
    },
    "_class": "com.arangodb.Employee"
  },
  "_class": "com.arangodb.Company"
}
```

Type hints are written for top-level documents (as a collection can contain different document types) as well as for every value if it's a complex type and a sub-type of the property type declared. `Map`s and `Collection`s are excluded from type mapping. Without the additional information about the concrete classes used, the document couldn't be restored in Java. The type information of the `manager` property is not enough to determine the `Employee` type. The `homeAddress` and `workAddress` properties have the same actual and defined type, thus no type hint is needed.

### Customizing type mapping

By default, the fully qualified class name is stored in the documents as a type hint. A custom type hint can be set with the `@TypeAlias("my-alias")` annotation on an entity. Make sure that it is an unique identifier across all entities. If we would add a `TypeAlias("employee")` annotation to the `Employee` class above, it would be persisted as `"_class": "employee"`.

The default type key is `_class` and can be changed by overriding the `typeKey()` method of the `AbstractArangoConfiguration` class.

If you need to further customize the type mapping process, the `arangoTypeMapper()` method of the configuration class can be overridden. The included `DefaultArangoTypeMapper` can be customized by providing a list of [`TypeInformationMapper`](https://docs.spring.io/spring-data/commons/docs/current/api/org/springframework/data/convert/TypeInformationMapper.html)s that create aliases from types and vice versa.

In order to fully customize the type mapping process you can provide a custom type mapper implementation by extending the `DefaultArangoTypeMapper` class.

### Deactivating type mapping

To deactivate the type mapping process, you can return `null` from the `typeKey()` method of the `AbstractArangoConfiguration` class. No type hints are stored in the documents with this setting. If you make sure that each defined type corresponds to the actual type, you can disable the type mapping, otherwise it can lead to exceptions when reading the entities from the DB.

## Annotations

### Annotation overview

| annotation              | level                     | description                                                                                                                                         |
| ----------------------- | ------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| @Document               | class                     | marks this class as a candidate for mapping                                                                                                         |
| @Edge                   | class                     | marks this class as a candidate for mapping                                                                                                         |
| @Id                     | field                     | stores the field as the system field \_key                                                                                                          |
| @Rev                    | field                     | stores the field as the system field \_rev                                                                                                          |
| @Field("alt-name")      | field                     | stores the field with an alternative name                                                                                                           |
| @Ref                    | field                     | stores the \_id of the referenced document and not the nested document                                                                              |
| @From                   | field                     | stores the \_id of the referenced document as the system field \_from                                                                               |
| @To                     | field                     | stores the \_id of the referenced document as the system field \_to                                                                                 |
| @Relations              | field                     | vertices which are connected over edges                                                                                                             |
| @Transient              | field, method, annotation | marks a field to be transient for the mapping framework, thus the property will not be persisted and not further inspected by the mapping framework |
| @PersistenceConstructor | constructor               | marks a given constructor - even a package protected one - to use when instantiating the object from the database                                   |
| @TypeAlias("alias")     | class                     | set a type alias for the class when persisted to the DB                                                                                             |
| @HashIndex              | class                     | describes a hash index                                                                                                                              |
| @HashIndexed            | field                     | describes how to index the field                                                                                                                    |
| @SkiplistIndex          | class                     | describes a skiplist index                                                                                                                          |
| @SkiplistIndexed        | field                     | describes how to index the field                                                                                                                    |
| @PersistentIndex        | class                     | describes a persistent index                                                                                                                        |
| @PersistentIndexed      | field                     | describes how to index the field                                                                                                                    |
| @GeoIndex               | class                     | describes a geo index                                                                                                                               |
| @GeoIndexed             | field                     | describes how to index the field                                                                                                                    |
| @FulltextIndex          | class                     | describes a fulltext index                                                                                                                          |
| @FulltextIndexed        | field                     | describes how to index the field                                                                                                                    |
| @CreatedBy              | field                     | Declares a field as the one representing the principal that created the entity containing the field.                                                |
| @CreatedDate            | field                     | Declares a field as the one representing the date the entity containing the field was created.                                                      |
| @LastModifiedBy         | field                     | Declares a field as the one representing the principal that recently modified the entity containing the field.                                      |
| @LastModifiedDate       | field                     | Declares a field as the one representing the date the entity containing the field was recently modified.                                            |
