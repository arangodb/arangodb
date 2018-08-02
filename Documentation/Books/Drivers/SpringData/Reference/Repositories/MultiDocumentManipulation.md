<!-- don't edit here, its from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Manipulating multiple documents

## ArangoRepository.findAll

```
ArangoRepository.findAll() : Iterable<T>
```

Returns all instances of the type.

**Examples**

```Java
@Autowired MyRepository repository;

Iterable<MyDomainClass> entities = repository.findAll();
```

## ArangoRepository.findAllById

```
ArangoRepository.findAllById(Iterable<ID> ids) : Iterable<T>
```

Returns all instances of the type with the given IDs.

**Arguments**

- **ids**: `Iterable<ID>`

  The ids (`_keys`) of the documents

**Examples**

```java
@Autowired MyRepository repository;

Iterable<MyDomainClass> entities = repository.findAllById(Arrays.asList("some-id", "some-other-id"));
```

## ArangoRepository.saveAll

```
ArangoRepository.saveAll(Iterable<S> entities) : Iterable<S>
```

Saves all given entities.

**Arguments**

- **entities**: `Iterable<S>`

  A list of entities to save.

**Examples**

```java
@Autowired MyRepository repository;

MyDomainClass obj1 = ...
MyDomainClass obj2 = ...
MyDomainClass obj3 = ...
repository.saveAll(Arrays.asList(obj1, obj2, obj3))
```

## ArangoRepository.deleteAll (method 1)

```
ArangoRepository.deleteAll() : void
```

Deletes all entities managed by the repository.

**Examples**

```java
@Autowired MyRepository repository;

repository.deleteAll();
```

## ArangoRepository.deleteAll (method 2)

```
ArangoRepository.deleteAll(Iterable<? extends T> entities) : void
```

Deletes the given entities.

**Arguments**

- **entities**: `Iterable<? extends T>`

  The entities to delete.

**Examples**

```java
@Autowired MyRepository repository;

MyDomainClass obj1 = ...
MyDomainClass obj2 = ...
MyDomainClass obj3 = ...
repository.deleteAll(Arrays.asList(obj1, obj2, obj3))
```
