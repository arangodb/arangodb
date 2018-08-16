<!-- don't edit here, its from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Query methods

Queries using [ArangoDB Query Language (AQL)](https://docs.arangodb.com/current/AQL/index.html) can be supplied with the `@Query` annotation on methods.

## Passing collection name

Instead of writing the collection name statically into the query string, the placeholder `#collection` can be specified.

```java
public interface MyRepository extends ArangoRepository<Customer, String>{

  // FOR c IN customer RETURN c
  @Query("FOR c IN #collection RETURN c")
  ArangoCursor<Customer> query();

}
```

## Passing bind parameters

There are three ways of passing bind parameters to the query in the query annotation.

### Number matching

Using number matching, arguments will be substituted into the query in the order they are passed to the query method.

```java
public interface MyRepository extends ArangoRepository<Customer, String>{

  @Query("FOR c IN #collection FILTER c.name == @0 AND c.surname == @1 RETURN c")
  ArangoCursor<Customer> query(String name, String surname);

}
```

### @Param

With the `@Param` annotation, the argument will be placed in the query at the place corresponding to the value passed to the `@Param` annotation.

```java
public interface MyRepository extends ArangoRepository<Customer, String>{

  @Query("FOR c IN #collection FILTER c.name == @name AND c.surname == @surname RETURN c")
  ArangoCursor<Customer> query(@Param("name") String name, @Param("surname") String surname);

}
```

### @BindVars

In addition you can use a method parameter of type `Map<String, Object>` annotated with `@BindVars` as your bind parameters. You can then fill the map with any parameter used in the query. (see [here](https://docs.arangodb.com/3.1/AQL/Fundamentals/BindParameters.html#bind-parameters) for more Information about Bind Parameters).

```java
public interface MyRepository extends ArangoRepository<Customer, String>{

  @Query("FOR c IN #collection FILTER c.name == @name AND c.surname = @surname RETURN c")
  ArangoCursor<Customer> query(@BindVars Map<String, Object> bindVars);

}
```

A mixture of any of these methods can be used. Parameters with the same name from an `@Param` annotation will override those in the `bindVars`.

```java
public interface MyRepository extends ArangoRepository<Customer, String>{

  @Query("FOR c IN #collection FILTER c.name == @name AND c.surname = @surname RETURN c")
  ArangoCursor<Customer> query(@BindVars Map<String, Object> bindVars, @Param("name") String name);

}
```

## Query options

`AqlQueryOptions` can also be passed to the driver, as an argument anywhere in the method signature.

```java
public interface MyRepository extends ArangoRepository<Customer, String>{

  @Query("FOR c IN #collection FILTER c.name == @name AND c.surname == @surname RETURN c")
  ArangoCursor<Customer> query(@Param("name") String name, @Param("surname") String surname, AqlQueryOptions options);

}
```
