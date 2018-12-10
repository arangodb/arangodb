<!-- don't edit here, it's from https://@github.com/arangodb/arangodb-spark-connector.git / docs/Drivers/ -->
# ArangoDB Spark Connector - Java Reference

## ArangoSpark.save

```
ArangoSpark.save[T](rdd: JavaRDD[T], collection: String, options: WriteOptions)
```

```
ArangoSpark.save[T](dataset: Dataset[T], collection: String, options: WriteOptions)
```

Save data from rdd into ArangoDB

**Arguments**

- **rdd**: `JavaRDD[T]`

  The rdd with the data to save

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

```Java
JavaSparkContext sc = ...
List<MyBean> docs = ...
JavaRDD<MyBean> documents = sc.parallelize(docs);
ArangoSpark.save(documents, "myCollection", new WriteOptions().database("myDB"));
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

```Java
JavaSparkContext sc = ...
List<MyBean> docs = ...
JavaRDD<MyBean> documents = sc.parallelize(docs);
SQLContext sql = SQLContext.getOrCreate(sc);
DataFrame df = sql.createDataFrame(documents, MyBean.class);
ArangoSpark.saveDF(documents, "myCollection", new WriteOptions().database("myDB"));
```

## ArangoSpark.load

```
ArangoSparkload[T](sparkContext: JavaSparkContext, collection: String, options: ReadOptions, clazz: Class[T]): ArangoJavaRDD[T]
```

Load data from ArangoDB into rdd

**Arguments**

- **sparkContext**: `JavaSparkContext`

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

- **clazz**: `Class[T]`

  The type of the document

**Examples**

```Java
JavaSparkContext sc = ...
ArangoJavaRDD<MyBean> rdd = ArangoSpark.load(sc, "myCollection", new ReadOptions().database("myDB"), MyBean.class);
```

## ArangoRDD.filter

```
ArangoJavaRDD.filter(condition: String): ArangoJavaRDD[T]
```

Adds a filter condition. If used multiple times, the conditions will be combined with a logical AND.

**Arguments**

- **condition**: `String`

  The condition for the filter statement. Use `doc` inside to reference the document. e.g. `"doc.name == 'John'"`

**Examples**

```Java
JavaSparkContext sc = ...
ArangoJavaRDD<MyBean> rdd = ArangoSpark.load(sc, "myCollection", new ReadOptions().database("myDB"), MyBean.class);
ArangoJavaRDD<MyBean> rddFiltered = rdd.filter("doc.test <= 50");
```
