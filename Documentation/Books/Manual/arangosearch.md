---
layout: default
description: ArangoSearch is a C++ based full-text search engine including similarity ranking capabilities natively integrated into ArangoDB.
title: ArangoSearch - Integrated Full-text Search Engine
redirect_from:
  - /3.5/views-arango-search.html # 3.4 -> 3.5
---
# ArangoSearch

ArangoSearch provides information retrieval features, natively integrated
into ArangoDB's query language and with support for all data models. It is
primarily a full-text search engine, a much more powerful alternative to the
[full-text index](indexing-fulltext.html) type.

ArangoSearch introduces the concept of **Views** which can be seen as
virtual collections. Each View represents an inverted index to provide fast
full-text searching over one or multiple linked collections and holds the
configuration for the search capabilities, such as the attributes to index.
It can cover multiple or even all attributes of the documents in the linked
collections. Search results can be sorted by their similarity ranking to
return the best matches first using popular scoring algorithms.

Configurable **Analyzers** are available for text processing, such as for
tokenization, language-specific word stemming, case conversion, removal of
diacritical marks (accents) from characters and more. Analyzers can be used
standalone or in combination with Views for sophisticated searching.

The ArangoSearch features are integrated into AQL as
[`SEARCH` operation](aql/operations-search.html) and a set of
[AQL functions](aql/functions-arangosearch.html).

Example use cases:
- Perform federated full-text searches over product descriptions for a
  web shop, with the product documents stored in various collections.
- Find information in a research database using stemmed phrases, case and
  accent insensitive, with irrelevant terms removed from the search index
  (stop word filtering), ranked by relevance based on term frequency (TFIDF).
- Query a movie dataset for titles with words in a particular order
  (optionally with wildcards), and sort the results by best matching (BM25)
  but favor movies with a longer duration.
