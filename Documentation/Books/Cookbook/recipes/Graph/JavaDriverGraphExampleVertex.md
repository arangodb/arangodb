# How to use an example vertex with the java driver

## Problem

**Note**: Arango 2.2 and the corresponding javaDriver is needed.

You want to retrieve information out of a graph using an object<T> as example vertex, and the object contains primitive datatypes such as 'int' or 'char'. E. g. you have a graph "myGraph" with vertices that are objects of the following class...

``` java
public class MyObject {

    private String name;
    private int age;

    public MyObject(String name, int age) {
        this.name = name;
        this.age = age;
    }

    /*
    *  + getter and setter
    */


}
```

... and edges of:

``` java
public class MyEdge {

    private String desc;

    public MyEdge(String desc) {
        this.desc = desc;
    }

    /*
    *  + getter and setter
    */


}
```

To retrieve all edges from vertices with a given name (e. g. "Bob") and arbitrary age the method   
``` java
arangoDriver.graphGetEdgeCursorByExample("myGraph", MyEdge.class, myVertexExample)
```
can not be used, because primitive datatypes (like 'int') can not be set to null (all attributes that are not null will be used as filter criteria). 

## Solution
There is a solution, but it's not that satisfying, because you need to know the attribute names of the stored documents representing the objects. If you know the attribute names, which are used to filter vertices it's possible to create a map as vertex example:  


```java
Map<String, Object> exampleVertex = new HashMap<String, Object>();
exampleVertex.put("name", "Bob");
CursorEntity<MyEdge> cursor = arangoDriver.graphGetEdgeCursorByExample("myGraph", MyEdge.class, exampleVertex);
```

Vice versa it's no problem to retrieve all edges of vertices that have been set to a certain age (e. g. 42):

``` java
MyObject myVertexExample = new MyObject(null, 42);
CursorEntity<MyEdge> cursor = arangoDriver.graphGetEdgeCursorByExample("myGraph", MyEdge.class, myVertexExample)
```

## Other resources
More documentation about the ArangoDB java driver is available:
 - [Arango DB Java in ten minutes](https://www.arangodb.com/tutorials/tutorial-java/)
 - [java driver Readme at Github](https://github.com/arangodb/arangodb-java-driver), [especially the graph example](https://github.com/arangodb/arangodb-java-driver/blob/master/src/test/java/com/arangodb/example/GraphQueryExample.java)
 - [Example source in java](https://github.com/arangodb/arangodb-java-driver/tree/master/src/test/java/com/arangodb/example)
 - [Unittests](https://github.com/arangodb/arangodb-java-driver/tree/master/src/test/java/com/arangodb)

**Author**: [gschwab](https://github.com/gschwab)


**Tags**: #java #graph #driver
