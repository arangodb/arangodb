# JSON

Libbson contains routines for converting a BSON document to JSON.

## Generating JSON from BSON

Generating JSON can be done using the `bson_as_json()` routine.
The generated JSON is in [MongoDB Extended JSON](http://docs.mongodb.org/manual/reference/mongodb-extended-json/) format.

```c
char *str;
size_t len;

str = bson_as_json(doc, &len);
printf("%s\n", str);
bson_free(str);
```

## Parsing JSON into BSON

This is not currently supported.
Until then, I suggest using an external JSON library.
