<!-- don't edit here, its from https://@github.com/arangodb/spring-data.git / docs/Drivers/ -->
# Query by example

## ArangoRepository.exists

```
ArangoRepository.exists(Example<S> example) : boolean
```

Checks whether the data store contains elements that match the given `Example`.

**Arguments**

- **example**: `Example<S>`

  The example to use. Must not be `null`.

**Examples**

```java
@Autowired MyRepository repository;

MyDomainClass sample = new MyDomainClass();
sample.setName("John"); // set some data in sample
boolean exists = repository.exists(Example.of(sample));
```

## ArangoRepository.findOne

```
ArangoRepository.findOne(Example<S> example) : Optional<S>
```

Returns a single entity matching the given `Example` or `Optional#empty()` if none was found.

**Arguments**

- **example**: `Example<S>`

  The example to use. Must not be `null`.

**Examples**

```java
@Autowired MyRepository repository;

MyDomainClass sample = new MyDomainClass();
sample.setName("John"); // set some data in sample
MyDomainClass entity = repository.findOne(Example.of(sample));
```

## ArangoRepository.findAll

```
ArangoRepository.findAll(Example<S> example) : Iterable<S>
```

Returns all entities matching the given `Example`. In case no match could be found an empty `Iterable` is returned.

**Arguments**

- **example**: `Example<S>`

  The example to use. Must not be `null`.

**Examples**

```java
@Autowired MyRepository repository;

MyDomainClass sample = new MyDomainClass();
sample.setName("John"); // set some data in sample
Iterable<MyDomainClass> entities = repository.findAll(Example.of(sample));
```

## ArangoRepository.count

```
ArangoRepository.count(Example<S> example) : long
```

Returns the number of instances matching the given `Example`.

**Arguments**

- **example**: `Example<S>`

  The example to use. Must not be `null`.

**Examples**

```java
@Autowired MyRepository repository;

MyDomainClass sample = new MyDomainClass();
sample.setName("John"); // set some data in sample
long count = repository.count(Example.of(sample));
```
