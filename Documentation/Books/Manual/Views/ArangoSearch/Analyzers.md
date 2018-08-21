ArangoSearch Analyzers
======================

To simplify query syntax ArangoSearch provides a concept of named analyzers which
are merely aliases for type+configuration of IResearch analyzers. Management of
named analyzers will be exposed in upcoming ArangoDB versions via both REST, GUI
and JavaScript APIs, e.g.

`db._globalSettings("iresearch.analyzers")`

A user then merely uses these analyzer names in ArangoSearch view configurations
and AQL queries.

ArangoSearch provides a 'text' analyzer to analyze human readable text. A required
configuration parameter for this type of analyzer is 'locale' used to specify
the language used for analysis.

The ArangoDB administrator may then set up a named analyzer 'text_des':

```json
{
  "name": "text_des",
  "type": "text",
  "properties": {
    "locale": "de-ch"
  }
}
```

The user is then immediately able to run queries with the said analyzer, e.g.

`SEARCH doc.description IN TOKENS('Ein brauner Fuchs springt', 'text_des')`

Similarly an administrator may choose to deploy a custom DNA analyzer 'DnaSeq':

```json
{
  "name": "dna",
  "type": "DnaSeq",
  "properties": "use-human-config"
}
```

The user is then immediately able to run queries with the said analyzer, e.g.

`SEARCH doc.dna IN TOKENS('ACGTCGTATGCACTGA', 'DnaSeq')`

To a limited degree the concept of 'analysis' is even available in non-IResearch
AQL, e.g. the `TOKENS(...)` function will utilize the power of IResearch to break
up a value into an AQL array that can be used anywhere in the AQL query.

In plain terms this means a user can match a document attribute when its
value matches at least one value form a set, (yes this is independent of doc),
e.g. to match docs with 'word == quick' OR 'word == brown' OR 'word == fox'

    FOR doc IN someCollection
      FILTER doc.word IN TOKENS('a quick brown fox', 'text_en')
      RETURN doc

Runtime-plugging functionality for analyzers is not available in ArangoDB at
this point in time, so ArangoDB comes with a few default-initialized analyzers:

* `identity`
  treat the value as an atom

* `text_de`
  tokenize the value into case-insensitive word stems as per the German locale,
  do not discard any any stopwords

* `text_en`
  tokenize the value into case-insensitive word stems as per the English locale,
  do not discard any any stopwords

* `text_es`
  tokenize the value into case-insensitive word stems as per the Spanish locale,
  do not discard any any stopwords

* `text_fi`
  tokenize the value into case-insensitive word stems as per the Finnish locale,
  do not discard any any stopwords

* `text_fr`
  tokenize the value into case-insensitive word stems as per the French locale,
  do not discard any any stopwords

* `text_it`
  tokenize the value into case-insensitive word stems as per the Italian locale,
  do not discard any any stopwords

* `text_nl`
  tokenize the value into case-insensitive word stems as per the Dutch locale,
  do not discard any any stopwords

* `text_no`
  tokenize the value into case-insensitive word stems as per the Norwegian
  locale, do not discard any any stopwords

* `text_pt`
  tokenize the value into case-insensitive word stems as per the Portuguese
  locale, do not discard any any stopwords

* `text_ru`
  tokenize the value into case-insensitive word stems as per the Russian locale,
  do not discard any any stopwords

* `text_sv`
  tokenize the value into case-insensitive word stems as per the Swedish locale,
  do not discard any any stopwords

* `text_zh`
  tokenize the value into word stems as per the Chinese locale
