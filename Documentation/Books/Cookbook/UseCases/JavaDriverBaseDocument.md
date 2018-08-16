How to retrieve documents from ArangoDB without knowing the structure? 
======================================================================

Problem
-------

If you use a NoSQL database it's common to retrieve documents with an unknown attribute structure. Furthermore, the amount and types of attributes may differ in documents resulting from a single query. Another problem is that you want to add one ore more attributes to a document.
 
In Java you are used to work with objects. Regarding the upper requirements it is possible to directly retrieve objects with the same attribute structure as the document out of the database. Adding attributes to an object at runtime could be very messy.

**Note**: ArangoDB 3.1 and the corresponding [Java driver](https://github.com/arangodb/arangodb-java-driver#supported-versions) is needed.


Solution
--------

With the latest version of the Java driver of ArangoDB an object called `BaseDocument` is provided.

The structure is very simple: It only has four attributes:

```java
public class BaseDocument {

    String id;
    String key;
    String revision;
    Map<String, Object> properties;

}
```

The first three attributes are the system attributes `_id`, `_key` and `_rev`. The fourth attribute is a `HashMap`. The key always is a String, the value an object. These properties contain all non system attributes of the document. 

The map can contain values of the following types:

* Map<String, Object>
* List<Object>
* Boolean
* Number
* String
* null

**Note**: `Map` and `List` contain objects, which are of the same types as listed above.

To retrieve a document is similar to the known procedure, except that you use `BaseDocument` as type.
 
```java
ArangoDB.Builder arango = new ArangoDB.Builder().builder();
DocumentEntity<BaseDocument> myObject = arango.db().collection("myCollection").getDocument("myDocumentKey", BaseDocument.class);
```


Other resources
---------------

More documentation about the ArangoDB Java driver is available:

- [Tutorial: Java in ten minutes](https://www.arangodb.com/tutorials/tutorial-sync-java-driver/)
- [Java driver at Github](https://github.com/arangodb/arangodb-java-driver)
- [Example source code](https://github.com/arangodb/arangodb-java-driver/tree/master/src/test/java/com/arangodb/example)
- [JavaDoc](http://arangodb.github.io/arangodb-java-driver/javadoc-4_1/index.html)

**Author**: [gschwab](https://github.com/gschwab),
   [Mark Vollmary](https://github.com/mpv1989)

**Tags**: #java #driver
