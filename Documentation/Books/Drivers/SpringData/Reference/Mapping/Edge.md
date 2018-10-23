<!-- don't edit here, it's from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Edge

## Annotation @Edge

The annotations `@Edge` applied to a class marks this class as a candidate for mapping to the database. The most relevant parameter is `value` to specify the collection name in the database. The annotation `@Edge` specifies the collection type to `EDGE`.

```java
@Edge("relations")
public class Relation {
  ...
}
```

## Spring Expression support

Spring Data ArangoDB supports the use of SpEL expressions within `@Edge#value`. This feature lets you define a dynamic collection name which can be used to implement multi tenancy applications.

```Java
@Component
public class TenantProvider {

	public String getId() {
		// threadlocal lookup
	}

}
```

```java
@Edge("#{tenantProvider.getId()}_relations")
public class Relation {
  ...
}
```

## Annotation @From and @To

With the annotations `@From` and `@To` applied on a field in a class annotated with `@Edge` the nested object is fetched from the database. The nested object has to be stored as a separate document in the collection described in the `@Document` annotation of the nested object class. The _\_id_ field of this nested object is stored in the fields `_from` or `_to` within the edge document.

```java
@Edge("relations")
public class Relation {
  @From
  private Person c1;
  @To
  private Person c2;
}

@Document(value="persons")
public class Person {
  @Id
  private String id;
}
```

The database representation of `Relation` in collection _relations_ looks as follow:

```
{
  "_key" : "123",
  "_id" : "relations/123",
  "_from" : "persons/456",
  "_to" : "persons/789"
}
```

and the representation of `Person` in collection _persons_:

```
{
  "_key" : "456",
  "_id" : "persons/456",
}
{
  "_key" : "789",
  "_id" : "persons/789",
}
```

**Note:** If you want to save an instance of `Relation`, both `Person` objects (from & to) already have to be persisted and the class `Person` needs a field with the annotation `@Id` so it can hold the persisted `_id` from the database.
