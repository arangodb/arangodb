Import and Export
=================

Imports and exports can be done with the tools
[_arangoimport_](../Programs/Arangoimport/README.md) and
[_arangoexport_](../Programs/Arangoexport/README.md).

<!-- Importing from files -->

<!-- Bulk import via HTTP API -->

<!-- Export to files -->

<!-- Bulk export via HTTP API -->

<!-- Syncing with 3rd party systems? -->

JSONlines
---------

Multiple documents can be stored in JSON format in a top-level array with
objects as members:

```json
[
  {"attr": "a document"},
  {"attr": "another document"}
]
```

It requires parsers to read the entire input in order to verify that the
array is properly closed at the very end. This can be problematic for large
files. [_arangoimport_](../Programs/Arangoimport/README.md) can perform better
with [_JSONlines_](http://jsonlines.org/) (also known as _JSONL_) formatted
input files, which allows processing each line individually:

```json
{"attr": "a document"}
{"attr": "another document"}
```

An input with JSON objects in an array, optionally pretty printed, can be
easily converted into JSONL with one JSON object per line using the
[**jq** command line tool](http://stedolan.github.io/jq/):

```
jq -c ".[]" inputFile.json > outputFile.jsonl
```

The `-c` option enables compact JSON (as opposed to pretty printed JSON).
`".[]"` is a filter that unpacks the top-level array and effectively puts each
object in that array on a separate line in combination with the compact option.

An example `inputFile.json` can look like this:

```json
[
  {
    "isActive": true,
    "name": "Evans Wheeler",
    "latitude": -0.119406,
    "longitude": 146.271888,
    "tags": [
      "amet",
      "qui",
      "velit"
    ]
  },
  {
    "isActive": true,
    "name": "Coffey Barron",
    "latitude": -37.78772,
    "longitude": 131.218935,
    "tags": [
      "dolore",
      "exercitation",
      "irure",
      "velit"
    ]
  }
]
```

The conversion produces the following `outputFile.jsonl`:

```json
{"isActive":true,"name":"Evans Wheeler","latitude":-0.119406,"longitude":146.271888,"tags":["amet","qui","velit"]}
{"isActive":true,"name":"Coffey Barron","latitude":-37.78772,"longitude":131.218935,"tags":["dolore","exercitation","irure","velit"]}
```
