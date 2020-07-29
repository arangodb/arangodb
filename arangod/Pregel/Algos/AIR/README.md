# Vertex Accumulators

The code contained in this directory is part of a project to provide users with
the means to develop their own Pregel algorithms without having to plug C++
code into ArangoDB.

For this purpose we developed a LISPy intermediate represenatation to be able
to transport programs into the existing Pregel implementation in ArangoDB. These
programs are executed using the Greenspun interpreter inside the VertexAccumulators [1]
Pregel algorithm.

At the moment this interpreter is a prototype and hence not optimized and (probably)
slow. It is very flexible in terms of what we can implement, provide and test: we can provide any function as a primitive in the language, and all basic operations are available as it is customary in the LISP tradition [2].

NOTE that the intention is *not* that this language is presented to users as is,
it is merely an intermediate representation which is very flexible for good
prototyping. A surface syntax is subject to development and even flexible in
terms of providing more than one.


## Example Config of a Vertex Accumulator Algorithm

Writing vertex accumulator algorithms is currently done as follows; You need a
properly sharded (smart-)graph (TODO add instructions on how to create it), in
the following example this graph is called `WikiVoteGraph`. The algorithm in the
example computes the shortest path from the single source vertex `sourceId`
using the attribute `cost` on every outgoing edge.

```
arangosh> const pp = require("@arangodb/pregel-programs");
arangosh> var pexec = pp.execute("WikiVoteGraph",
  { "resultField": "SSSP",
    "accumulatorsDeclaration": {
      "distance": { "accumulatorType": "min",
                    "valueType": "ints",
                    "storeSender": true } },
    "initProgram": [ "seq", 
                       ["print", " this = ", ["this"]],
                       ["if",
                          [ ["eq?", ["this"], sourceId],
                            ["seq", ["set", "distance", 0 ],
                            true ] ],
                          [true,
                            ["seq", ["set", "distance", 999999],
                            false ] ] ] ], 
    "updateProgram":  [ "for", "outbound", ["quote", "edge"],
                          ["quote",
                             "seq",
                               ["update",
                                  "distance",
                                    ["attrib", "_to", ["var-ref", "edge"] ],
                                    ["attrib", "cost", ["var-ref", "edge"] ] ] ] ] });

```


[1] We called the Pregel algorithm "VertexAccumulators", because we set out writing
    an implementation of something that looks like TigerGraph's VertexAccumulators.
    This name is subject to change to a more appropriate name in the future.

[2] LISP had it 30 years ago!
