<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-spark-connector.git / docs/Drivers/ -->
# ArangoDB Spark Connector - Scala Reference

## ArangoSpark.save

```
ArangoSpark.save[T](rdd: RDD[T], collection: String, options: WriteOptions)
```

```
ArangoSpark.save[T](dataset: Dataset[T], collection: String, options: WriteOptions)
```

Save data from rdd or dataset into ArangoDB

**Arguments**

- **rdd**/**dataset**: `RDD[T]` or `Dataset[T]`

  The rdd or dataset with the data to save

- **collection**: `String`

  The collection to save in

- **options**: `WriteOptions`

  - **database**: `String`

    Database to write into

  - **hosts**: `String`

    Alternative hosts to context property `arangodb.hosts`

  - **user**: `String`

    Alternative user to context property `arangodb.user`

  - **password**: `String`

    Alternative password to context property `arangodb.password`

  - **useSsl**: `Boolean`

    Alternative useSsl to context property `arangodb.useSsl`

  - **sslKeyStoreFile**: `String`

    Alternative sslKeyStoreFile to context property `arangodb.ssl.keyStoreFile`

  - **sslPassPhrase**: `String`

    Alternative sslPassPhrase to context property `arangodb.ssl.passPhrase`

  - **sslProtocol**: `String`

    Alternative sslProtocol to context property `arangodb.ssl.protocol`

**Examples**

```Scala
val sc: SparkContext = ...
val documents = sc.parallelize((1 to 100).map { i => MyBean(i) })
ArangoSpark.save(documents, "myCollection", WriteOptions("myDB"))
```

## ArangoSpark.saveDF

```
ArangoSpark.saveDF(dataframe: DataFrame, collection: String, options: WriteOptions)
```

Save data from dataframe into ArangoDB

**Arguments**

- **dataframe**: DataFrame`

  The dataFrame with the data to save

- **collection**: `String`

  The collection to save in

- **options**: `WriteOptions`

  - **database**: `String`

    Database to write into

  - **hosts**: `String`

    Alternative hosts to context property `arangodb.hosts`

  - **user**: `String`

    Alternative user to context property `arangodb.user`

  - **password**: `String`

    Alternative password to context property `arangodb.password`

  - **useSsl**: `Boolean`

    Alternative useSsl to context property `arangodb.useSsl`

  - **sslKeyStoreFile**: `String`

    Alternative sslKeyStoreFile to context property `arangodb.ssl.keyStoreFile`

  - **sslPassPhrase**: `String`

    Alternative sslPassPhrase to context property `arangodb.ssl.passPhrase`

  - **sslProtocol**: `String`

    Alternative sslProtocol to context property `arangodb.ssl.protocol`

**Examples**

```Scala
val sc: SparkContext = ...
val documents = sc.parallelize((1 to 100).map { i => MyBean(i) })
val sql: SQLContext = SQLContext.getOrCreate(sc);
val df = sql.createDataFrame(documents, classOf[MyBean])
ArangoSpark.saveDF(df, "myCollection", WriteOptions("myDB"))
```

## ArangoSpark.load

```
ArangoSpark.load[T: ClassTag](sparkContext: SparkContext, collection: String, options: ReadOptions): ArangoRDD[T]
```

Load data from ArangoDB into rdd

**Arguments**

- **sparkContext**: `SparkContext`

  The sparkContext containing the ArangoDB configuration

- **collection**: `String`

  The collection to load data from

- **options**: `ReadOptions`

  - **database**: `String`

    Database to write into

  - **hosts**: `String`

    Alternative hosts to context property `arangodb.hosts`

  - **user**: `String`

    Alternative user to context property `arangodb.user`

  - **password**: `String`

    Alternative password to context property `arangodb.password`

  - **useSsl**: `Boolean`

    Alternative useSsl to context property `arangodb.useSsl`

  - **sslKeyStoreFile**: `String`

    Alternative sslKeyStoreFile to context property `arangodb.ssl.keyStoreFile`

  - **sslPassPhrase**: `String`

    Alternative sslPassPhrase to context property `arangodb.ssl.passPhrase`

  - **sslProtocol**: `String`

    Alternative sslProtocol to context property `arangodb.ssl.protocol`

**Examples**

```Scala
val sc: SparkContext = ...
val rdd = ArangoSpark.load[MyBean](sc, "myCollection", ReadOptions("myDB"))
```

## ArangoRDD.filter

```
ArangoRDD.filter(condition: String): ArangoRDD[T]
```

Adds a filter condition. If used multiple times, the conditions will be combined with a logical AND.

**Arguments**

- **condition**: `String`

  The condition for the filter statement. Use `doc` inside to reference the document. e.g. `"doc.name == 'John'"`

**Examples**

```Scala
val sc: SparkContext = ...
val rdd = ArangoSpark.load[MyBean](sc, "myCollection").filter("doc.name == 'John'")
```
