# ArangoSearch Index Viewer tool

## Introduction
This is an internal tool to peek into the contents of the ArangoSearch index. Currently it can process the indexed textual and numeric data, prints out the postings list, field and term metadata, and term position information.

## Usage
```
./asindexviewer <ArangoSearch directory>
```

Example,
```
./asindexviewer <arangod-database-directory>/databases/database-1/arangosearch-216684_438367/
```

Sample output,
```
kkawade@dev1:~/projects/arangodb/internal-tools/asindexviewer/build$ ./asindexviewer /tmp/tdb/databases/database-1/{
  "index": {
    "docsCount": 3,
    "liveDocsCount": 3,
    "segments": [
      {
        "docsCount": 2,
        "fields": [
          {
            "fieldIndexFeatures": {
              "docsCount": 2,
              "maxTerm": "\u0019��Π\u0000\u0000",
              "minTerm": "\u0019���`\u0000\u0000",
              "termsCount": 2
            },
            "name": "@_PK",
            "terms": [
              {
                "display": "rawUtf8=\"\u0019���`\u0000\u0000\" hex=19 ef 9f d3 ce 60 00 00",
                "payload": "(empty)",
                "postings": [
                  {
                    "docId": "S0/d1 {LocalDocumentId 19 ef 9f d3 ce 60 00 00 u64_le=106441430003481}",
                    "positions": []
                  }
                ],
                "raw_bytes": "19 ef 9f d3 ce 60 00 00",
                "term_meta": {
                  "docs_with_term": 1,
                  "freq": 0
                }
              },
              {
                "display": "rawUtf8=\"\u0019��Π\u0000\u0000\" hex=19 ef 9f d3 ce a0 00 00",
                "payload": "(empty)",
                "postings": [
                  {
                    "docId": "S0/d2 {LocalDocumentId 19 ef 9f d3 ce a0 00 00 u64_le=176810174181145}",
                    "positions": []
                  }
                ],
                "raw_bytes": "19 ef 9f d3 ce a0 00 00",
                "term_meta": {
                  "docs_with_term": 1,
                  "freq": 0
                }
              }
              ..
            ]
..
..
.
..
```
# Examples
This is an example of a simple dataset that can be used to test the tool. Run the tool after creating the following collection and view, then inspect the arangosearch directory with the tool.
Inspect how the collections are stored in the ArangoSearch index.
```
db._create('c1')
db.c1.insert({ name: 'Hutch', address: { country: 'USA', city: 'Boston' }})
db.c1.insert({ name: 'John', address: { country: 'Germany', city: 'Hamburg' }})
db._createView('v1', 'arangosearch')
db.v1.properties(
 {
  links: {
    c1: {
      fields: {
        name: {},
        address: {
          fields: {
            city: {}
          }
        }
      },
      storeValues: 'id',
      includeAllFields: true
    }
  }})
```