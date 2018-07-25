Import and Export
=================

Import and export can be done via the tools [_arangoimport_](../Programs/Arangoimport/README.md) and [_arangoexport_](../Programs/Arangoexport/README.md)

<!-- Importing from files -->

<!-- Bulk import via HTTP API -->

<!-- Export to files -->

<!-- Bulk export via HTTP API -->

<!-- Syncing with 3rd party systems? -->

Converting JSON pretty printed files into JSONlines format
==========================================================

[_arangoimport_](../Programs/Arangoimport/README.md) may perform better with [_JSONlines_](http://jsonlines.org/) formatted input files.

Depending on the actual formatting of a JSON input file processing large data with lots of objects (that requires reading all them first into memory) it may be optimized using JSONlines. An input with pretty printed JSON objects within an array can be converted into JSON object per line using [_jq_](http://stedolan.github.io/jq/) command line tool. Converted data can be validated and processed faster in this case.

You can easily use the `jq` command line tool for this conversion:
    
    jq -c ".[]" inputFile.json > outputFile.jsonl

An example `inputFile.json` can look like this:

```json
[
  {
    "_id": "5b575e382e8cfbe474cb870d",
    "index": 0,
    "guid": "0a991404-ac99-4cb7-94f1-2e629a02ae02",
    "isActive": true,
    "name": "Evans Wheeler",
    "gender": "male",
    "latitude": -0.119406,
    "longitude": 146.271888,
    "tags": [
      "amet",
      "qui",
      "proident",
      "velit",
      "id",
      "enim",
      "officia"
    ],
    "greeting": "Hello, Evans Wheeler! You have 9 unread messages.",
    "favoriteFruit": "strawberry"
  },
  {
    "_id": "5b575e3897cb664a12066f63",
    "index": 1,
    "guid": "3b94a6f2-6381-46f6-9516-e8df4f945fd9",
    "isActive": true,
    "name": "Coffey Barron",
    "gender": "male",
    "latitude": -37.78772,
    "longitude": 131.218935,
    "tags": [
      "laborum",
      "quis",
      "dolore",
      "anim",
      "exercitation",
      "irure",
      "velit"
    ],
    "greeting": "Hello, Coffey Barron! You have 10 unread messages.",
    "favoriteFruit": "apple"
  }
]
```

After conversion the following `outputFile.jsonl` will be created:

```json
{"_id":"5b575e382e8cfbe474cb870d","index":0,"guid":"0a991404-ac99-4cb7-94f1-2e629a02ae02","isActive":true,"name":"Evans Wheeler","gender":"male","latitude":-0.119406,"longitude":146.271888,"tags":["amet","qui","proident","velit","id","enim","officia"],"greeting":"Hello, Evans Wheeler! You have 9 unread messages.","favoriteFruit":"strawberry"}
{"_id":"5b575e3897cb664a12066f63","index":1,"guid":"3b94a6f2-6381-46f6-9516-e8df4f945fd9","isActive":true,"name":"Coffey Barron","gender":"male","latitude":-37.78772,"longitude":131.218935,"tags":["laborum","quis","dolore","anim","exercitation","irure","velit"],"greeting":"Hello, Coffey Barron! You have 10 unread messages.","favoriteFruit":"apple"}
```

