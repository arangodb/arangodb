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
kkawade@dev1:~/projects/arangodb/internal-tools/asindexviewer/build$ ./asindexviewer /tmp/tdb/databases/database-1/arangosearch-216684_438367/                                                     07:25:25 [202/1866]
Index segmentsCount=1 docsCount=7 liveDocsCount=7
Segment id=0 docsCount=7 liveDocsCount=7
  IResearch assigns doc ids 1..liveDocs independently in EACH segment (not global, not Arango _key).
  Use S<segment>/d<id> below; same d1 in S0 vs S1 is two different Arango rows if PKs differ.
  @_PK column rows loaded=7
  Documents in this segment (local ids; repeat 1..N each segment):
    S0/d1 -> LocalDocumentId 19 d9 89 1a 91 b0 00 00 u64_le=194137261988121
    S0/d2 -> LocalDocumentId 19 db 07 9b f0 10 00 00 u64_le=18625579178777
    S0/d3 -> LocalDocumentId 19 db 07 9f a0 40 00 00 u64_le=71058607037209
    S0/d4 -> LocalDocumentId 19 dc ea 5f 52 e0 00 00 u64_le=246644401167385
    S0/d5 -> LocalDocumentId 19 dd 36 8f 34 30 00 00 u64_le=53002299170073
    S0/d6 -> LocalDocumentId 19 dd 5a 70 31 d0 00 00 u64_le=228910756977945
    S0/d7 -> LocalDocumentId 19 dd 5a 83 96 a0 00 00 u64_le=176568309308697
----------------------------------------------
Fields data:
----------------------------------------------
  arangoDetectedKind=unknown logicalField="@_PK" storageNameBytes=40 5f 50 4b
Field indexFeatures=0 minTerm=(ى) maxTerm=(Z) termsCount=7 docsCount=7
Values
    Term
      display: (rawUtf8="ى" hex=19 d9 89 1a 91 b0 00 00)
      raw_bytes: 19 d9 89 1a 91 b0 00 00
      term_meta: docs_with_term=1  position_total=0  (freq = sum of within-doc occurrences across all postings)
      positions: (not stored — field has no IndexFeatures::POS)
    Term
      display: (rawUtf8="" hex=19 db 07 9b f0 10 00 00)
      raw_bytes: 19 db 07 9b f0 10 00 00
      term_meta: docs_with_term=1  position_total=0  (freq = sum of within-doc occurrences across all postings)
      positions: (not stored — field has no IndexFeatures::POS)
    Term
      display: (rawUtf8="@" hex=19 db 07 9f a0 40 00 00)
      raw_bytes: 19 db 07 9f a0 40 00 00
      term_meta: docs_with_term=1  position_total=0  (freq = sum of within-doc occurrences across all postings)
      positions: (not stored — field has no IndexFeatures::POS)
    Term
      display: (rawUtf8="_R" hex=19 dc ea 5f 52 e0 00 00)
      raw_bytes: 19 dc ea 5f 52 e0 00 00
      term_meta: docs_with_term=1  position_total=0  (freq = sum of within-doc occurrences across all postings)
      positions: (not stored — field has no IndexFeatures::POS)
    Term
      display: (rawUtf8="640" hex=19 dd 36 8f 34 30 00 00)
      raw_bytes: 19 dd 36 8f 34 30 00 00
      term_meta: docs_with_term=1  position_total=0  (freq = sum of within-doc occurrences across all postings)
      positions: (not stored — field has no IndexFeatures::POS)
..
.
..
  arangoDetectedKind=numeric logicalField="age" storageNameBytes=61 67 65 00 5f 64
Field indexFeatures=0 minTerm=() maxTerm=(8) termsCount=12 docsCount=4
Values
    Term
      display: ((): (double: 5))
      raw_bytes: a0 c0 14 00 00 00 00 00 00
      term_meta: docs_with_term=1  position_total=0  (freq = sum of within-doc occurrences across all postings)
      positions: (not stored — field has no IndexFeatures::POS)
    Term
      display: ((): (double: 15))
      raw_bytes: a0 c0 2e 00 00 00 00 00 00
      term_meta: docs_with_term=1  position_total=0  (freq = sum of within-doc occurrences across all postings)
      positions: (not stored — field has no IndexFeatures::POS)
    Term
      display: ((): (double: 24))
      raw_bytes: a0 c0 38 00 00 00 00 00 00
      term_meta: docs_with_term=2  position_total=0  (freq = sum of within-doc occurrences across all postings)
      positions: (not stored — field has no IndexFeatures::POS)
    Term
      display: ((): (double: 5))
      raw_bytes: b0 c0 14 00 00 00 00
      term_meta: docs_with_term=1  position_total=0  (freq = sum of within-doc occurrences across all postings)
      positions: (not stored — field has no IndexFeatures::POS)
    Term
      display: ((): (double: 15))
      raw_bytes: b0 c0 2e 00 00 00 00
      term_meta: docs_with_term=1  position_total=0  (freq = sum of within-doc occurrences across all postings)
      positions: (not stored — field has no IndexFeatures::POS)
    Term
      display: ((): (double: 24))
      raw_bytes: b0 c0 38 00 00 00 00
      term_meta: docs_with_term=2  position_total=0  (freq = sum of within-doc occurrences across all postings)
      positions: (not stored — field has no IndexFeatures::POS)
    Term
      display: ((): (double: 5))
      raw_bytes: c0 c0 14 00 00
      term_meta: docs_with_term=1  position_total=0  (freq = sum of within-doc occurrences across all postings)
      positions: (not stored — field has no IndexFeatures::POS)
..
.
.
  arangoDetectedKind=analyzed_string logicalField="text" analyzer="text_en" storageNameBytes=74 65 78 74 01 74 65 78 74 5f 65 6e
Field indexFeatures=3 minTerm=(a) maxTerm=(this) termsCount=14 docsCount=3
Values
    Term
      display: (text:a)
      raw_bytes: 61
      term_meta: docs_with_term=1  position_total=1  (freq = sum of within-doc occurrences across all postings)
      postings (per doc: in-doc frequency, 1-based term positions in field)
        doc S0/d6{LocalDocumentId 19 dd 5a 70 31 d0 00 00 u64_le=228910756977945}  in_doc_frequency=1
          positions: 5
    Term
      display: (text:begin)
      raw_bytes: 62 65 67 69 6e
      term_meta: docs_with_term=1  position_total=1  (freq = sum of within-doc occurrences across all postings)
      postings (per doc: in-doc frequency, 1-based term positions in field)
        doc S0/d7{LocalDocumentId 19 dd 5a 83 96 a0 00 00 u64_le=176568309308697}  in_doc_frequency=1
          positions: 4
    Term
      display: (text:brown)
      raw_bytes: 62 72 6f 77 6e
      term_meta: docs_with_term=3  position_total=3  (freq = sum of within-doc occurrences across all postings)
      postings (per doc: in-doc frequency, 1-based term positions in field)
        doc S0/d4{LocalDocumentId 19 dc ea 5f 52 e0 00 00 u64_le=246644401167385}  in_doc_frequency=1
          positions: 3
        doc S0/d6{LocalDocumentId 19 dd 5a 70 31 d0 00 00 u64_le=228910756977945}  in_doc_frequency=1
          positions: 6
        doc S0/d7{LocalDocumentId 19 dd 5a 83 96 a0 00 00 u64_le=176568309308697}  in_doc_frequency=1
          positions: 7
    Term
      display: (text:fenc)
      raw_bytes: 66 65 6e 63
      term_meta: docs_with_term=1  position_total=1  (freq = sum of within-doc occurrences across all postings)
      postings (per doc: in-doc frequency, 1-based term positions in field)
        doc S0/d4{LocalDocumentId 19 dc ea 5f 52 e0 00 00 u64_le=246644401167385}  in_doc_frequency=1
          positions: 8
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