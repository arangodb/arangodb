How to add XML data to ArangoDB?
================================

Problem
-------

You want to store XML data files into a database to have the ability to make queries onto them.

**Note**: ArangoDB 3.1 and the corresponding Java driver is needed.

Solution
--------

Since version 3.1.0 the arangodb-java-driver supports writing, reading and querying of raw strings containing the JSON documents.

With [JsonML](http://www.jsonml.org/) you can convert a XML string into a JSON string and back to XML again.

Converting XML into JSON with JsonML example:

```java
String string = "<recipe name=\"bread\" prep_time=\"5 mins\" cook_time=\"3 hours\"> "
        + "<title>Basic bread</title> "
        + "<ingredient amount=\"8\" unit=\"dL\">Flour</ingredient> "
        + "<ingredient amount=\"10\" unit=\"grams\">Yeast</ingredient> "
        + "<ingredient amount=\"4\" unit=\"dL\" state=\"warm\">Water</ingredient> "
        + "<ingredient amount=\"1\" unit=\"teaspoon\">Salt</ingredient> "
        + "<instructions> "
        + "<step>Mix all ingredients together.</step> "
        + "<step>Knead thoroughly.</step> "
        + "<step>Cover with a cloth, and leave for one hour in warm room.</step> "
        + "<step>Knead again.</step> "
        + "<step>Place in a bread baking tin.</step> "
        + "<step>Cover with a cloth, and leave for one hour in warm room.</step> "
        + "<step>Bake in the oven at 180(degrees)C for 30 minutes.</step> "
        + "</instructions> "
        + "</recipe> ";

JSONObject jsonObject = JSONML.toJSONObject(string);
System.out.println(jsonObject.toString());
```

The converted JSON string:

```json
{
   "prep_time" : "5 mins",
   "name" : "bread",
   "cook_time" : "3 hours",
   "tagName" : "recipe",
   "childNodes" : [
      {
         "childNodes" : [
            "Basic bread"
         ],
         "tagName" : "title"
      },
      {
         "childNodes" : [
            "Flour"
         ],
         "tagName" : "ingredient",
         "amount" : 8,
         "unit" : "dL"
      },
      {
         "unit" : "grams",
         "amount" : 10,
         "tagName" : "ingredient",
         "childNodes" : [
            "Yeast"
         ]
      },
      {
         "childNodes" : [
            "Water"
         ],
         "tagName" : "ingredient",
         "amount" : 4,
         "unit" : "dL",
         "state" : "warm"
      },
      {
         "childNodes" : [
            "Salt"
         ],
         "tagName" : "ingredient",
         "unit" : "teaspoon",
         "amount" : 1
      },
      {
         "childNodes" : [
            {
               "tagName" : "step",
               "childNodes" : [
                  "Mix all ingredients together."
               ]
            },
            {
               "tagName" : "step",
               "childNodes" : [
                  "Knead thoroughly."
               ]
            },
            {
               "childNodes" : [
                  "Cover with a cloth, and leave for one hour in warm room."
               ],
               "tagName" : "step"
            },
            {
               "tagName" : "step",
               "childNodes" : [
                  "Knead again."
               ]
            },
            {
               "childNodes" : [
                  "Place in a bread baking tin."
               ],
               "tagName" : "step"
            },
            {
               "tagName" : "step",
               "childNodes" : [
                  "Cover with a cloth, and leave for one hour in warm room."
               ]
            },
            {
               "tagName" : "step",
               "childNodes" : [
                  "Bake in the oven at 180(degrees)C for 30 minutes."
               ]
            }
         ],
         "tagName" : "instructions"
      }
   ]
}
```

Saving the converted JSON to ArangoDB example:

```java
ArangoDB.Builder arango = new ArangoDB.Builder().build();
ArangoCollection collection = arango.db().collection("testCollection")
DocumentCreateEntity<String> entity = collection.insertDocument(
                jsonObject.toString());
String key = entity.getKey();
```

Reading the stored JSON as a string and convert it back to XML example:

```java
String rawJsonString = collection.getDocument(key, String.class);
String xml = JSONML.toString(rawJsonString);
System.out.println(xml);
```

Example output:

```xml
<recipe _id="RawDocument/6834407522" _key="6834407522" _rev="6834407522"
         cook_time="3 hours" name="bread" prep_time="5 mins">
  <title>Basic bread</title>
  <ingredient amount="8" unit="dL">Flour</ingredient>
  <ingredient amount="10" unit="grams">Yeast</ingredient>
  <ingredient amount="4" state="warm" unit="dL">Water</ingredient>
  <ingredient amount="1" unit="teaspoon">Salt</ingredient>
  <instructions>
    <step>Mix all ingredients together.</step>
    <step>Knead thoroughly.</step>
    <step>Cover with a cloth, and leave for one hour in warm room.</step>
    <step>Knead again.</step>
    <step>Place in a bread baking tin.</step>
    <step>Cover with a cloth, and leave for one hour in warm room.</step>
    <step>Bake in the oven at 180(degrees)C for 30 minutes.</step>
  </instructions>
</recipe>
```

**Note:** The [fields mandatory to ArangoDB documents](../../Manual/DataModeling/Documents/DocumentAddress.html) are added; If they break your XML schema you have to remove them.

Query raw data example:

```java
String queryString = "FOR t IN testCollection FILTER t.cook_time == '3 hours' RETURN t";
ArangoCursor<String> cursor = arango.db().query(queryString, null, null, String.class);
while (cursor.hasNext()) {
    JSONObject jsonObject = new JSONObject(cursor.next());
    String xml = JSONML.toString(jsonObject);
    System.out.println("XML value: " + xml);
}
```

Other resources
---------------

More documentation about the ArangoDB Java driver is available:

- [Tutorial: Java in ten minutes](https://www.arangodb.com/tutorials/tutorial-sync-java-driver/)
- [Java driver at Github](https://github.com/arangodb/arangodb-java-driver)
- [Example source code](https://github.com/arangodb/arangodb-java-driver/tree/master/src/test/java/com/arangodb/example)
- [JavaDoc](http://arangodb.github.io/arangodb-java-driver/javadoc-4_1/index.html)

**Author**: [Achim Brandt](https://github.com/a-brandt),
  [Mark Vollmary](https://github.com/mpv1989)

**Tags**: #java #driver
