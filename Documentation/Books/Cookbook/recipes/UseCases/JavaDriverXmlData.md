# How to add XML data to ArangoDB?

## Problem
You want to store XML data files into a database to have the ability to make queries onto them.

**Note**: ArangoDB > 2.6 and the javaDriver => 2.7.2 is needed.

## Solution
Since version 2.7.2 the aragodb-java-driver supports writing `createDocumentRaw(...)`, reading `getDocumentRaw(...)` and querying `executeAqlQueryRaw(...)` of raw strings containing the JSON documents.

With [JsonML](http://www.jsonml.org/) you can convert a XML string into a JSON string and back to XML again.

Converting XML into JSON with JsonML example:
``` java
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
``` json
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
``` java
DocumentEntity<String> entity = arangoDriver.createDocumentRaw(
                "testCollection",
                jsonObject.toString(), true,false);
String documentHandle = entity.getDocumentHandle();
```
Reading the stored JSON as a string and convert it back to XML example:
``` java
String rawJsonString = arangoDriver.getDocumentRaw(documentHandle, null, null);
String xml = JSONML.toString(rawJsonString);
System.out.println(xml);
```
Example output:
``` xml
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
**Note:** The [fields mandatory to ArangoDB documents](https://docs.arangodb.com/2.8/Documents/index.html) are added; If they break your XML schema you have to remove them.

Query raw data example:
``` java
String queryString = "FOR t IN TestCollection FILTER t.cook_time == '3 hours' RETURN t";
CursorRawResult cursor = arangoDriver.executeAqlQueryRaw(queryString, null, null);
Iterator<String> iter = cursor.iterator();
while (iter.hasNext()) {
	JSONObject jsonObject = new JSONObject(iter.next());
	String xml = JSONML.toString(jsonObject);
	System.out.println("XML value: " + xml);
}
```

## Other resources
More documentation about the ArangoDB java driver is available:
 - [Arango DB Java in ten minutes](https://www.arangodb.com/tutorials/tutorial-java/)
 - [java driver at Github](https://github.com/arangodb/arangodb-java-driver)
 - [Raw JSON string example](https://github.com/arangodb/arangodb-java-driver/blob/master/src/test/java/com/arangodb/example/document/RawDocumentExample.java)

**Author**: [Achim Brandt](https://github.com/a-brandt)

**Tags**: #java #driver
