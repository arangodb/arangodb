# Detailed overview of ArangoSearch views

ArangoSearch is a powerful fulltext search component with additional functionality, 
supported via the 'text' analyzer and 'tfidf'/'bm25' [scorers](Scorers.md), 
without impact on performance when specifying documents from different collections or 
filtering on multiple document attributes.

## View datasource

The IResearch functionality is exposed to ArangoDB via the the ArangoSearch view
API because the ArangoSearch view is merely an identity transformation applied
onto documents stored in linked collections of the same ArangoDB database.
In plain terms an ArangoSearch view only allows filtering and sorting of documents
located in collections of the same database. The matching documents themselves
are returned as-is from their corresponding collections.

## Links to ArangoDB collections

A concept of an ArangoDB collection 'link' is introduced to allow specifying
which ArangoDB collections a given ArangoSearch View should query for documents
and how these documents should be queried.

An ArangoSearch Link is a uni-directional connection from an ArangoDB collection
to an ArangoSearch view describing how data coming from the said collection should
be made available in the given view. Each ArangoSearch Link in an ArangoSearch
view is uniquely identified by the name of the ArangoDB collection it links to.
An ArangoSearch view may have zero or more links, each to a distinct ArangoDB
collection. Similarly an ArangoDB collection may be referenced via links by zero
or more distinct ArangoSearch views. In plain terms any given ArangoSearch view
may be linked to any given ArangoDB collection of the same database with zero or
at most one link. However, any ArangoSearch view may be linked to multiple
distinct ArangoDB collections and similarly any ArangoDB collection may be
referenced by multiple ArangoSearch views.

To configure an ArangoSearch view for consideration of documents from a given
ArangoDB collection a link definition must be added to the properties of the
said ArangoSearch view defining the link parameters as per the section
[View definition/modification](#view-definitionmodification).

## Analyzers

To simplify query syntax ArangoSearch provides a concept of 
[named analyzers](Analyzers.md) which are merely aliases for
type+configuration of IResearch analyzers. Management of named analyzers
is exposed via REST, GUI and JavaScript APIs.

## View definition/modification

An ArangoSearch view is configured via an object containing a set of
view-specific configuration directives and a map of link-specific configuration
directives.

During view creation the following directives apply:

* id _(optional)_: the desired view identifier
* name _(required)_: the view name
* type _(required)_: the value "arangosearch"
  any of the directives from the section [View properties](#view-properties-updatable)

During view modification the following directives apply:

* links _(optional)_:
  a mapping of collection-name/collection-identifier to one of:
  * link creation - link definition as per the section [Link properties](#link-properties)
  * link removal - JSON keyword *null* (i.e. nullify a link if present)
    any of the directives from the section [modifiable view properties](#view-properties-updatable)

## View properties (non-updatable)

* **locale** (_optional_, default: `C`)<br/>
  the default locale used for ordering processed attribute names

## View properties (updatable)

* **commit** (_optional_, default: use defaults for all values)<br/>
  configure ArangoSearch View commit policy for single-item inserts/removals,
  e.g. when adding removing documents from a linked ArangoDB collection

  * **cleanupIntervalStep** (_optional_, default: `10`; to disable use: `0`)<br/>
    wait at least this many commits between removing unused files in the
    ArangoSearch data directory
    for the case where the consolidation policies merge segments often (i.e. a
    lot of commit+consolidate), a lower value will cause a lot of disk space to
    be wasted
    for the case where the consolidation policies rarely merge segments (i.e.
    few inserts/deletes), a higher value will impact performance without any
    added benefits

  * **commitIntervalMsec** (_optional_, default: `60000`; to disable use: `0`)<br/>
    wait at least *count* milliseconds between committing view data store
    changes and making documents visible to queries
    for the case where there are a lot of inserts/updates, a lower value will
    cause the view not to account for them, (unlit commit), and memory usage
    would continue to grow
    for the case where there are a few inserts/updates, a higher value will
    impact performance and waste disk space for each commit call without any
    added benefits

  * **consolidate** (_optional_, default: `none`)<br/>
    a per-policy mapping of thresholds in the range `[0.0, 1.0]` to determine data
    store segment merge candidates, if specified then only the listed policies
    are used, keys are any of:

    * **bytes** (_optional_, for default values use an empty object: `{}`)

      * **segmentThreshold** (_optional_, default: `300`; to disable use: `0`)<br/>
        apply consolidation policy IFF {segmentThreshold} >= #segments

      * **threshold** (_optional_, default: `0.85`)<br/>
        consolidate `IFF {threshold} > segment_bytes / (all_segment_bytes / #segments)`

    * **bytes_accum** (_optional_, for default values use: `{}`)<br/>

      * **segmentThreshold** (_optional_, default: `300`; to disable use: `0`)<br/>
        apply consolidation policy IFF {segmentThreshold} >= #segments

      * **threshold** (_optional_, default: `0.85`)<br/>
        consolidate `IFF {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes`

    * **count** (_optional_, for default values use: `{}`)

      * **segmentThreshold** (_optional_, default: `300`; to disable use: `0`)<br/>
        apply consolidation policy IFF {segmentThreshold} >= #segments

      * **threshold** (_optional_, default: `0.85`)<br/>
        consolidate `IFF {threshold} > segment_docs{valid} / (all_segment_docs{valid} / #segments)`

    * fill: (optional)
      if specified, use empty object for default values, i.e. `{}`

      * **segmentThreshold** (_optional_, default: `300`; to disable use: `0`)<br/>
        apply consolidation policy IFF {segmentThreshold} >= #segments

      * **threshold** (_optional_, default: `0.85`)<br/>
        consolidate `IFF {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})`

## Link properties

* **analyzers** (_optional_, default: `[ 'identity' ]`)<br/>
  a list of analyzers, by name as defined via the [Analyzers](Analyzers.md), that
  should be applied to values of processed document attributes

* **fields** (_optional_, default: `{}`)<br/>
  an object `{attribute-name: [Link properties]}` of fields that should be
  processed at each level of the document
  each key specifies the document attribute to be processed, the value of
  *includeAllFields* is also consulted when selecting fields to be processed
  each value specifies the [Link properties](#link-properties) directives to be used when
  processing the specified field, a Link properties value of `{}` denotes
  inheritance of all (except *fields*) directives from the current level

* **includeAllFields** (_optional_, default: `false`)<br/>
  if true then process all document attributes (if not explicitly specified
  then process the fields with default Link properties directives, i.e. `{}`),
  otherwise only consider attributes mentioned in *fields*

* **trackListPositions** (_optional_, default: `false`)<br/>
  if true then for array values track the value position in the array, e.g. when
  querying for the input: `{ attr: [ 'valueX', 'valueY', 'valueZ' ] }`
  the user must specify: `doc.attr[1] == 'valueY'`
  otherwise all values in an array are treated as equal alternatives, e.g. when
  querying for the input: `{ attr: [ 'valueX', 'valueY', 'valueZ' ] }`
  the user must specify: `doc.attr == 'valueY'`

* **storeValues** (_optional_, default: `"none"`)<br/>
  how should the view track the attribute values, this setting allows for
  additional value retrieval optimizations, one of:
  * none: Do not store values by the view
  * id: Store only information about value presence, to allow use of the EXISTS() function
