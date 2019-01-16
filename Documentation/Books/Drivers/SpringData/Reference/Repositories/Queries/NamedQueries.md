<!-- don't edit here, it's from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Named queries

An alternative to using the `@Query` annotation on methods is specifying them in a separate `.properties` file. The default path for the file is `META-INF/arango-named-queries.properties` and can be changed with the `EnableArangoRepositories#namedQueriesLocation()` setting. The entries in the properties file must adhere to the following convention: `{simple entity name}.{method name} = {query}`. Let's assume we have the following repository interface:

```java
package com.arangodb.repository;

public interface CustomerRepository extends ArangoRepository<Customer, String> {

    Customer findByUsername(@Param("username") String username);

}
```

The corresponding `arango-named-queries.properties` file looks like this:

```properties
Customer.findByUsername = FOR c IN customers FILTER c.username == @username RETURN c
```

The queries specified in the properties file are no different than the queries that can be defined with the `@Query` annotation. The only difference is that the queries are in one place. If there is a `@Query` annotation present and a named query defined, the query in the `@Query` annotation takes precedence.
