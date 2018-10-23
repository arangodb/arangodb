<!-- don't edit here, it's from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Manipulating documents

## ArangoRepository.existsById

```
ArangoRepository.existsById(ID id) : boolean
```

Returns whether an entity with the given id exists.

**Arguments**

- **id**: `ID`

  The id (`_key`) of the document. Must not be `null`.

**Examples**

```Java
@Autowired MyRepository repository;

boolean exists = repository.existsById("some-id");
```

## ArangoRepository.findById

```
ArangoRepository.findById(ID id) : Optional<T>
```

Retrieves an entity by its id.

**Arguments**

- **id**: `ID`

  The id (`_key`) of the document. Must not be `null`.

**Examples**

```java
@Autowired MyRepository repository;

Optional<MyDomainClass> entity = repository.findById("some-id");
```

## ArangoRepository.save

```
ArangoRepository.save(S entity) : S
```

Saves a given entity. Use the returned instance for further operations as the save operation might have changed the entity instance completely.

**Arguments**

- **entity**: `S`

  The entity to save in the database. Must not be `null`.

```java
@Autowired MyRepository repository;

MyDomainClass entity = new MyDomainClass();
entity = repository.save(entity);
```

## ArangoRepository.deleteById

```
ArangoRepository.deleteById(ID id) : void
```

Deletes the entity with the given id.

**Arguments**

- **id**: `ID`

  The id (`_key`) of the document. Must not be `null`.

**Examples**

```java
@Autowired MyRepository repository;

repository.deleteById("some-id");
```

## ArangoRepository.delete

```
ArangoRepository.delete(T entity) : void
```

Deletes a given entity.

**Arguments**

- **entity**: `T`

  The entity to delete. Must not be `null`.

**Examples**

```java
@Autowired MyRepository repository;

MyDomainClass entity = ...
repository.delete(entity);
```
