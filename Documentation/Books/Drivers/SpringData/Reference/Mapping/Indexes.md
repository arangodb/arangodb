<!-- don't edit here, it's from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Indexes

## Annotation @\<IndexType\>Indexed

With the `@<IndexType>Indexed` annotations user defined indexes can be created at a collection level by annotating single fields of a class.

Possible `@<IndexType>Indexed` annotations are:

- `@HashIndexed`
- `@SkiplistIndexed`
- `@PersistentIndexed`
- `@GeoIndexed`
- `@FulltextIndexed`

The following example creates a hash index on the field `name` and a separate hash index on the field `age`:

```java
public class Person {
  @HashIndexed
  private String name;

  @HashIndexed
  private int age;
}
```

With the `@<IndexType>Indexed` annotations different indexes can be created on the same field.

The following example creates a hash index and also a skiplist index on the field `name`:

```java
public class Person {
  @HashIndexed
  @SkiplistIndexed
  private String name;
}
```

## Annotation @\<IndexType\>Index

If the index should include multiple fields the `@<IndexType>Index` annotations can be used on the type instead.

Possible `@<IndexType>Index` annotations are:

- `@HashIndex`
- `@SkiplistIndex`
- `@PersistentIndex`
- `@GeoIndex`
- `@FulltextIndex`

The following example creates a single hash index on the fields `name` and `age`, note that if a field is renamed in the database with @Field, the new field name must be used in the index declaration:

```java
@HashIndex(fields = {"fullname", "age"})
public class Person {
  @Field("fullname")
  private String name;

  private int age;
}
```

The `@<IndexType>Index` annotations can also be used to create an index on a nested field.

The following example creates a single hash index on the fields `name` and `address.country`:

```java
@HashIndex(fields = {"name", "address.country"})
public class Person {
  private String name;

  private Address address;
}
```

The `@<IndexType>Index` annotations and the `@<IndexType>Indexed` annotations can be used at the same time in one class.

The following example creates a hash index on the fields `name` and `age` and a separate hash index on the field `age`:

```java
@HashIndex(fields = {"name", "age"})
public class Person {
  private String name;

  @HashIndexed
  private int age;
}
```

The `@<IndexType>Index` annotations can be used multiple times to create more than one index in this way.

The following example creates a hash index on the fields `name` and `age` and a separate hash index on the fields `name` and `gender`:

```java
@HashIndex(fields = {"name", "age"})
@HashIndex(fields = {"name", "gender"})
public class Person {
  private String name;

  private int age;

  private Gender gender
}
```
