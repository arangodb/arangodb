
@startDocuBlock general_graph_example_description

For many of the following functions *examples* can be passed in as a parameter.
*Examples* are used to filter the result set for objects that match the conditions.
These *examples* can have the following values:

* *null*, there is no matching executed all found results are valid.
* A *string*, only results are returned, which *_id* equal the value of the string
* An example *object*, defining a set of attributes.
    Only results having these attributes are matched.
* A *list* containing example *objects* and/or *strings*.
    All results matching at least one of the elements in the list are returned.

@endDocuBlock

