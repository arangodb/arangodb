<!-- don't edit here, its from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Template

With `ArangoTemplate` Spring Data ArangoDB offers a central support for interactions with the database over a rich feature set. It mostly offers the features from the ArangoDB Java driver with additional exception translation from the drivers exceptions to the Spring Data access exceptions inheriting the `DataAccessException` class.
The `ArangoTemplate` class is the default implementation of the operations interface `ArangoOperations` which developers of Spring Data are encouraged to code against.
