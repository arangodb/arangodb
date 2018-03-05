AQL tutorial
============

This is an introduction to ArangoDB's query language AQL, built around a small
dataset of characters from the novel and fantasy drama television series
Game of Thrones (as of season 1). It includes character traits in two languages,
some family relations, and last but not least a small set of filming locations,
which makes for an interesting mix of data to work with.

There is no need to import the data before you start. It is provided as part
of the AQL queries in this tutorial. You can interact with ArangoDB using its
[web interface](../../Manual/GettingStarted/WebInterface.html) to manage
collections and execute the queries.

Chapters
--------

- [Basic CRUD](CRUD.md)
- [Matching documents](Filter.md)
- [Sorting and limiting](SortLimit.md)
- [Joining together](Join.md)
- [Graph traversal](Traversal.md)
- [Geospatial queries](Geospatial.md)

<!-- Add later:
Advanced data manipulation: attributes, projections, calculations...
Aggregation: Grouping techniques
-->

Dataset
-------

### Characters

The dataset features 43 characters with their name, surname, age, alive status
and trait references. The surname and age properties are not always present.
The column *traits (resolved)* is not part of the actual data used in this
tutorial, but included for your convenience.

![Characters table](Characters_Table.png)

### Traits

There are 18 unique traits. Each trait has a random letter as document key.
The trait labels come in English and German.

![Traits table](Traits_Table.png)

### Locations

This small collection of 8 filming locations comes with two attributes, a
*name* and a *coordinate*. The coordinates are modeled as number arrays,
comprised of a latitude and a longitude value each.

![Locations table](Locations_Table.png)
