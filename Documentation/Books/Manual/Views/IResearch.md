# Bringing the power of IResearch to ArangoDB

## What is IResearch

### To the end-user

A natively integrated AQL extension that allows one to:
* evaluate together documents located in different collections
* filter documents based on AQL boolean expressions and functions
* sort the resultset based on how closely each document matched the filter

A concept of value 'analysis' that is meant to break up a given value into
a set of sub-values internally tied together by metadata which influences both
the filter and sort stages to provide the most appropriate match for the
specified conditions, similar to queries to web search engines.

In plain terms this means a user can for example:
* request documents where the 'body' attribute best matches 'a quick brown fox'
* request documents where the 'dna' attribute best matches a DNA sub sequence
* request documents where the 'name' attribute best matches gender
* etc... (via custom analyzers described in the next section)

### To the developer

A cross-platform open source indexing and searching engine written in C++,
optimized for speed and memory footprint, with source available from:
https://github.com/iresearch-toolkit/iresearch

A framework for indexing, filtering and sorting of data. The indexing stage can
treat each data item as an atom or use custom 'analyzers' to break the data item
into sub-atomic pieces tied together with internally tracked metadata.

The IResearch framework in general can be further extended at runtime with
custom implementations of analyzers (used during the indexing and filtering
stages) and scorers (used during the sorting stage) allowing full control over
the behaviour of the engine.

### Analyzers:

To simplify query syntax ArangoDB provides a concept of named analyzers which
are merely aliases for type+configuration of IResearch analyzers. Management of
named analyzers is exposed via both REST, GUI and JavaScript APIs, e.g.
<br>db._globalSettings("iresearch.analyzers")

A user then merely uses these analyzer names in IResearch view configurations
and AQL queries, e.g.

IResearch provides a 'text' analyzer to analyze human readable text. A required
configuration parameter for this type of analyzer is 'locale' used to specify
the language used for analysis.

The ArangoDB administrator may then set up a named analyzer 'text_des':
<br>{ "name": "text_des", "type": "text", "properties": { "locale": "de-ch" } }

The user is then immediately able to run queries with the said analyzer, e.g.
<br>FILTER doc.description IN TOKENS('Ein brauner Fuchs springt', 'text_des')

Similarly an administrator may choose to deploy a custom DNA analyzer 'DnaSeq':
<br>{ "name": "dna", "type": "DnaSeq", "properties": "use-human-config" }

The user is then immediately able to run queries with the said analyzer, e.g.
<br>FILTER doc.dna IN TOKENS('ACGTCGTATGCACTGA', 'DnaSeq')

To a limited degree the concept of 'analysis' is even available in non-IResearch
AQL, e.g. the TOKENS(...) function will utilize the power of IResearch to break
up a value into an AQL array that can be used anywhere in the AQL query.

In plain terms this means a user can match a document attribute when its
value matches at least one value form a set, (yes this is independent of doc),
e.g. to match docs with 'word == quick' OR 'word == brown' OR 'word == fox'

    FOR doc IN someCollection
      FILTER doc.word IN TOKENS('a quick brown fox', 'text_en')
      RETRUN doc

Runtime-plugging functionality for analyzers is not avaiable in ArangoDB at this
point in time, so ArangoDB comes with a few default-initialized analyzers:

* identity
  treat the value as an atom

* text_de
  tokenize the value into case-insensitive word stems as per the German locale,
  do not discard any any stopwords

* text_en
  tokenize the value into case-insensitive word stems as per the English locale,
  do not discard any any stopwords

* text_es
  tokenize the value into case-insensitive word stems as per the Spanish locale,
  do not discard any any stopwords

* text_fi
  tokenize the value into case-insensitive word stems as per the Finnish locale,
  do not discard any any stopwords

* text_fr
  tokenize the value into case-insensitive word stems as per the French locale,
  do not discard any any stopwords

* text_it
  tokenize the value into case-insensitive word stems as per the Italian locale,
  do not discard any any stopwords

* text_nl
  tokenize the value into case-insensitive word stems as per the Dutch locale,
  do not discard any any stopwords

* text_no
  tokenize the value into case-insensitive word stems as per the Norwegian
  locale, do not discard any any stopwords

* text_pt
  tokenize the value into case-insensitive word stems as per the Portuguese
  locale, do not discard any any stopwords

* text_ru
  tokenize the value into case-insensitive word stems as per the Russian locale,
  do not discard any any stopwords

* text_sv
  tokenize the value into case-insensitive word stems as per the Swedish locale,
  do not discard any any stopwords

### Scorers:

ArangoDB accesses IResearch scorers directly by their internal names. The name
(in upper-case) of the scorer is the function name to be used in the 'SORT'
section. Function arguments, (excluding the first argument), are serialized as a
string representation of a JSON array and passed directly to the corresponding
scorer. The first argument to any scorer function (an AQL requirement) is the
current document parameter from the 'FOR', i.e. it would be 'doc' for the
statement:
<br>FOR doc IN VIEW someView

IResearch provides a 'bm25' scorer implementing the
[BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25). This scorer
optionally takes 'k' and 'b' positional parameters.

The user is able to run queries with the said scorer, e.g.
<br>SORT BM25(doc, 1.2, 0.75)

The function arguments will then be serialized into a JSON representation:
<br>"[ 1.2, 0.75 ]"
<br>
and passed to the scorer implementation.

Similarly an administrator may choose to deploy a custom DNA analyzer 'DnaRank'.

The user is then immediately able to run queries with the said scorer, e.g.
<br>SORT DNARANK(doc, 123, 456, "abc", { "def", "ghi" })

The function arguments will then be serialized into a JSON representation:
<br>"[ 123, 456, \"abc\", { \"def\", \"ghi\" } ]"
<br>
and passed to the scorer implementation.

Runtime-plugging functionality for scores is not avaiable in ArangoDB at this
point in time, so ArangoDB comes with a few default-initialized scores:

* \<attribute-name\>
  order results based on the value of **attribute-name**

* BM25
  order results based on the
  [BM25 algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)

* TFIDF
  order results based on the
  [TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF)

### IResearch is much more than a fulltext search

But fulltext searching is a subset of its available functionality, supported via
the 'text' analyzer and 'tfidf'/'bm25' scorers, without impact to performance
when specifying documents from different collections or filtering on multiple
document attributes.

### View datasource

IResearch functionality is exposed through an ArangoDB view API because the
IResearch view is merely an identity transformation applied onto documents
stored in linked collections of the same ArangoDB database. In plain terms an
IResearch view only allows filtering and sorting of documents located in
collections of the same database. The matching documents themselves are returned
as-is from their corresponding collections.

### Links to ArangoDB collections

A concept of an ArangoDB collection 'link' is introduced to allow specifying
which ArangoDB collections a given IResearch View should query for documents and
how these documents should be queried.

An IResearch Link is a uni-directional connection from an ArangoDB collection
to an IResearch view describing how data coming from the said collection should
be made available in the given view. Each IResearch Link in an IResearch view is
uniquely identified by the name of the ArangoDB collection it links to. An
IResearch view may have zero or more links, each to a distinct ArangoDB
collection. Similarly an ArangoDB collection may be referenced via links by zero
or more distinct IResearch views. In plain terms any given IResearch view may be
linked to any given ArangoDB collection of the same database with zero or at
most one link. However, any IResearch view may be linked to multiple distinct
ArangoDB collections and similarly any ArangoDB collection may be referenced by
multiple IResearch views.

To configure an IResearch view for consideration of documents from a given
ArangoDB collection a link definition must be added to the properties of the
said IResearch view defining the link parameters as per the section
[View definition/modification].


### View definition/modification

An IResearch view is configured via an object containing a set of
view-specific configuration directives and a map of link-specific configuration
directives.

During view creation the following directives apply:
* id: \<optional\> the desired view identifier
* name: \<required\> the view name
* type: \<required\> the value "iresearch"
<br>any of the directives from the section [View properties (modifiable)]

During view modification the following directives apply:
* links: \<optional\>
  a mapping of collection-name/collection-identifier to one of:
  * link creation - link definition as per the section [Link properties]
  * link removal - JSON keyword *null* (i.e. nullify a link if present)
<br>any of the directives from the section [View properties (modifiable)]


### View properties (modifiable)

* commitBulk: \<optional\> (default: use defaults for all values)
  configure IResearch View commit policy for bulk inserts/removals,
  e.g. when adding a new link to an ArangoDB collection

  * cleanupIntervalStep: \<optional\> (default: 10) (to disable use: 0)
    wait at least this many commits between removing unused files in the
    IResearch data directory
    for the case where the consolidation policies merge segments often (i.e. a
    lot of commit+consolidate), a lower value will cause a lot of disk space to
    be wasted
    for the case where the consolidation policies rarely merge segments (i.e.
    few inserts/deletes), a higher value will impact performance without any
    added benefits

  * commitIntervalBatchSize: \<optional\> (default: 10000)
    when bulk-indexing (e.g. adding a new link) issue commit after *count*
    records have been bulk inserted/removed
    for the case where collection records contain a small number of indexable
    fields, a lower value would cause unnecessary computation overhead and
    performance degradation
    for the case where collection records contain a large number of indexable
    fields, a higher value would cause higher memory consumption between commits

  * consolidate: \<optional\> (default: \<none\>)
    a per-policy mapping of thresholds in the range [0.0, 1.0] to determine data
    store segment merge candidates, if specified then only the listed policies
    are used, keys are any of:

    * bytes: \<optional\> (for default values use: {})

      * intervalStep: \<optional\> (default: 10) (to disable use: 0)
        apply consolidation policy with every Nth commit

      * threshold: \<optional\> (default: 0.85)
        consolidate IFF {threshold} > segment_bytes / (all_segment_bytes / #segments)

    * bytes_accum: \<optional\> (for default values use: {})

      * intervalStep: \<optional\> (default: 10) (to disable use: 0)
        apply consolidation policy with every Nth commit

      * threshold: \<optional\> (default: 0.85)
        consolidate IFF {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes

    * count: \<optional\> (for default values use: {})

      * intervalStep: \<optional\> (default: 10) (to disable use: 0)
        apply consolidation policy with every Nth commit

      * threshold: <optional> (default: 0.85)
        consolidate IFF {threshold} > segment_docs{valid} / (all_segment_docs{valid} / #segments)

    * fill: \<optional\>
      if specified, use empty object for default values, i.e. {}

      * intervalStep: \<optional\> (default: 10) (to disable use: 0)
        apply consolidation policy with every Nth commit

      * threshold: <optional> (default: 0.85)
        consolidate IFF {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})

* commitItem: \<optional\>(default: use defaults for all values)
  configure IResearch View commit policy for single-item inserts/removals,
  e.g. when adding removing documents from a linked ArangoDB collection

  * cleanupIntervalStep: \<optional\> (default: 10) (to disable use: 0)
    wait at least this many commits between removing unused files in the
    IResearch data directory
    for the case where the consolidation policies merge segments often (i.e. a
    lot of commit+consolidate), a lower value will cause a lot of disk space to
    be wasted
    for the case where the consolidation policies rarely merge segments (i.e.
    few inserts/deletes), a higher value will impact performance without any
    added benefits

  * commitIntervalMsec: \<optional\> (default: 60000) (to disable use: 0)
    wait at least *count* milliseconds between committing view data store
    changes and making documents visible to queries
    for the case where there are a lot of inserts/updates, a lower value will
    cause the view not to account for them, (unlit commit), and memory usage
    would continue to grow
    for the case where there are a few inserts/updates, a higher value will
    impact performance and waste disk space for each commit call without any
    added benefits

  * commitTimeoutMsec: \<optional\> (default: 5000) (to disable use: 0)
    try to commit as much as possible before *count* milliseconds
    for the case where there are a lot of inserts/updates, a lower value will
    cause a delay in the view accounting for them, due skipping of some commits
    for the case where there are a lot of inserts/updates, a higher value will
    cause higher memory consumption between commits due to accumulation of
    document modifications while a commit is in progress

  * consolidate: \<optional\> (default: \<none\>)
    a per-policy mapping of thresholds in the range [0.0, 1.0] to determine data
    store segment merge candidates, if specified then only the listed policies
    are used, keys are any of:

    * bytes: \<optional\> (for default values use: {})

      * intervalStep: \<optional\> (default: 10) (to disable use: 0)
        apply consolidation policy with every Nth commit

      * threshold: \<optional\> (default: 0.85)
        consolidate IFF {threshold} > segment_bytes / (all_segment_bytes / #segments)

    * bytes_accum: \<optional\> (for default values use: {})

      * intervalStep: \<optional\> (default: 10) (to disable use: 0)
        apply consolidation policy with every Nth commit

      * threshold: \<optional\> (default: 0.85)
        consolidate IFF {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes

    * count: \<optional\> (for default values use: {})

      * intervalStep: \<optional\> (default: 10) (to disable use: 0)
        apply consolidation policy with every Nth commit

      * threshold: <optional> (default: 0.85)
        consolidate IFF {threshold} > segment_docs{valid} / (all_segment_docs{valid} / #segments)

    * fill: \<optional\>
      if specified, use empty object for default values, i.e. {}

      * intervalStep: \<optional\> (default: 10) (to disable use: 0)
        apply consolidation policy with every Nth commit

      * threshold: <optional> (default: 0.85)
        consolidate IFF {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})

* dataPath: <optional> (default: \<ArangoDB database path\>/iresearch-\<view-id\>)
  the filesystem path where to store persisted view metadata

* locale: <optional> (default: 'C')
  the default locale used for ordering processed attribute names

* threadsMaxIdle: <optional> (default: 5)
  maximum idle number of threads for single-run tasks
  for the case where there are a lot of short-lived asynchronous tasks, a lower
  value will cause a lot of thread creation/deletion calls
  for the case where there are no short-lived asynchronous tasks, a higher
  value will only waste memory

* threadsMaxTotal: <optional> (default: 5)
  maximum total number of threads (>0) for single-run tasks
  for the case where there are a lot of parallelizable tasks and an abundance
  of resources, a lower value would limit performance
  for the case where there are limited resources CPU/memory, a higher value
  will negatively impact performance

### View properties (unmodifiable)

* collections:
  an internally tracked array of collection identifiers which are known to have
  links to the current collection
  among other things used for adding collections during a view query transaction

### Link properties

* analyzers: \<optional\> (default: [ 'identity' ])
  a list of analyzers, by name as defined via the section [Analyzers], that
  should be applied to values of processed document attributes

* boost: \<optional\> (default: 1.0)
  sort preference factor that should be applied to values of processed 
  document attributes, documents otherwise scored equally but having a higher
  preference will appear sooner/higher in the sorted resultset

* fields: \<optional\> (default: {})
  a map{\<attribute-name\>, [Link properties]} of fields that should be
  processed at each level of the document
  each key specifies the document attribute to be processed, the value of
  *includeAllFields* is also consulted when selecting fields to be processed
  each value specifies the [Link properties] directives to be used when
  processing the specified field, a [Link properties] value of '{}' denotes
  inheritance of all (except *fields*) directives from the current level

* includeAllFields: \<optional\> (default: false)
  if true then process all document attributes (if not explicitly specified
  then process the fields with default [Link properties] directives, i.e. {}),
  otherwise only consider attributes mentioned in *fields*

* nestListValues: \<optional\> (default: false)
  if true then for array values track the value position in the array, e.g. when
  querying for the input: { attr: [ 'valueX', 'valueY', 'valueZ' ] }
  the user must specify: doc.attr[1] == 'valueY'
  otherwise all values in an array are treated as equal alternatives, e.g. when
  querying for the input: { attr: [ 'valueX', 'valueY', 'valueZ' ] }
  the user must specify: doc.attr == 'valueY'
