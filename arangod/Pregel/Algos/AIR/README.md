# AIR (Arango Intermediate Representation)

The code contained in this directory is part of a project to provide users with
the means to develop their own Pregel algorithms without having to plug C++
code into ArangoDB.

For this purpose we developed a LISPy intermediate represenatation to be able
to transport programs into the existing Pregel implementation in ArangoDB. These
programs are executed using the Greenspun interpreter inside the AIR [1]
Pregel algorithm.

At the moment this interpreter is a prototype and hence not optimized and (probably)
slow. It is very flexible in terms of what we can implement, provide and test:
we can provide any function as a primitive in the language, and all basic
operations are available as it is customary in the LISP tradition [2].

NOTE that the intention is *not* that this language is presented to users as is,
it is merely an intermediate representation which is very flexible for good
prototyping. A surface syntax is subject to development and even flexible in
terms of providing more than one.

In particular this way we get a better feeling for which functionality is needed
by (potential) clients and users of graph analytics.

## Writing Vertex Accumulator Algorithms using AIR

Writing vertex accumulator algorithms is currently done as follows; You need a
properly sharded (smart-)graph (TODO add instructions on how to create it), in
the following example this graph is called `WikiVoteGraph`. The algorithm in the
example computes the shortest path from the single source vertex `sourceId`
using the attribute `cost` on every outgoing edge.

```
arangosh> const pp = require("@arangodb/pregel-programs");
arangosh> var pexec = pp.execute("WikiVoteGraph",
 // TODO: test and copy program from pregel-programs.js
```

## Current AIR spec

### Special forms

 * if -- takes pairs `[cond, body]` of conditions `cond` and expression `body` and evaluates 
         the first `body` for which `cond` avaluates to `true`
 * quote --  `["quote", expr ...]` evaluates to `[expr ...]`
 * cons -- `["cons", val, [expr ...]]` evaluates to `[val, expr ...]`
 * and -- `["and", expr ...]` evaluates to false if one of `expr` evaluates to `false`. The arguments `expr` are evaluated in order, and evaluation is stopped once an `expr` evaluates to `false`.
 * or -- `["or", expr ...]` evaluates to true if one of `expr` evaluates to `true`. The arguments `expr` are evaluated in order, and evaluation is stopped once an `expr` evaluates to `true`.
 * seq -- `["seq", expr ... lastexpr ]` evaluates `expr` in order, the value of a `seq` expression is `lastexpr`
 * match -- `["match", expr, [c, body] ...]` evaluates `expr` to `val` and evaluates `body` for the first pair `[c, body]`, where `["eq?", val, c]` is `true`.
 * for-each -- `["for-each" [(v list) ...] body ...]` binds `v` to elements of `list` in order and evaluates `body` with this binding.

### Language primitives

 * var-ref -- `["var-ref", name]` refer to variable `name` in current context
 * var-set!  -- `["var-set!", name, value]` set variable `name` in current context to `value`

 * attrib-ref -- `["attrib-ref", doc, key]`, `["attrib-ref", doc, [p ...]]`, in the first variant, extract attribute `key` from `doc`, in the second variant extract attribute with path `p/...` from doc
 * attrib-set -- `["attrib-set", dict, key, value]` - set dict at key to value 
 * attrib-set -- `["attrib-set", dict, [path...], value]` - set dict at path to value
 
 * array-ref -- `["array-ref", arr, index]` - get value at specified index
 * array-set -- `["array-set", arr, index, value]` - set value at specified index
 
 * dict-merge -- `["merge", dict, dict]` returns the merge of two dicts 

 * print -- `["print", expr ...]` print `expr` for each `expr`. 

 * +, -, \*, / -- arithmetic operators, all work on lists
 * not  -- unary logical negation
 * false? -- `["false?", val]` evaluates to `true` if `val` is considered `false`
 * true? -- `["true?", val]` evaluates to `true` if `val` is considered `true`
 * eq? -- `["eq?", left, right]` evaluates to `true` if `left` is equal to `right`
 * gt? -- `["gt?", left, right]` evaluates to `true` if `left` is greater than `right`
 * ge? -- `["ge?", left, right]` evaluates to `true` if `left` is greater than or equal to`right`
 * le? -- `["le?", left, right]` evaluates to `true` if `left` is less than or equal to `right`
 * lt? -- `["lt?", left, right]` evaluates to `true` if `left` is less than `right`
 * ne? -- `["ne?", left, right]` evaluates to `true` if `left` is not equal to `right`

 * list-cat -- `["list-cat", list ...]` concatenates the lists `list ...` forming one list
 * string-cat -- `["string-cat", string ...]` concatenates the strings `string ...` forming one string
 * int-to-str -- `["int-to-str", val]` returns a string representation of the integer `val`

 * min, max, avg -- 

### Foreign calls in Vertex Computation "context" [3]

The following functions are only available when running as a vertex computation (i.e. as a `initProgram`, `updateProgram`).
`this` usually refers to the vertex we are attached to. They are "foreign calls" into the `VertexComputation` object.

 * `["accum-ref", name]` evaluates to the current value of the accumulator `name`
 * `["accum-set!", name, value]` sets the current value of the accumulator `name` to `value`
 * `["accum-clear!, name]` resets the current value of the accumulator `name` to a well-known one (currently numeric limits for `max` and `min` accumulators, 0 for sums, false for or, true for and, and empyt for lists and velocypack)

 * `["send-to-accum", name, to-vertex, value]` send the value `value` to the accumulator `name` at vertex `to-vertex`
 * `["send-to-all-neighbors", name,  value]` send the value `value` to the accumulator `name` in all neighbors (reachable by an edge; note that if there are multiple edges from us to the neighbour, the value is sent multiple times.

 * `["vertex-count"]` the number of vertices in the graph under consideration.
 * `["global-superstep"]` the current superstep the algorithm is in.

 * `["this-vertex-id"]`
 * `["this-unique-id"]`
 * `["this-pregel-id"]`
 * `["this-outbound-edges-count"]` (TODO: `["this-outdegree"]`?) the number of outgoing edges
 * `["this-outbound-edges"]` a list of outbound edges of the form `{ "doc": document, "pregel-id": { "shard": id, "key": key } }`. This is currently necessary to send messages to neighbouring vertices conveniently; at a later stage the edge type should become opaque.

### Foreign calls in Master "context" [3] (TODO: this looks a bit like we could call it "coordinatorcontext" or somesuch

The following functions are only available when running in the MasterContext on the Coordinator to coordinate
phases and phase changes.
They are "foreign calls" into the `VertexComputation` object.
 
 * `["goto-phase", phase]` -- sets the current phase to `phase`
 * `["finish"]` -- finishes the pregel computation
 * `accum-ref`, `accum-set!`, `accum-clear!` as described above. For global accumulators only.

### Foreign calls in _Custom Accumulator_ context

 * `["parameters"]` -- returns the object passed as parameter to the accumulator definition
 * `["current-value"]` -- returns the current value of the accumulator
 * `["get-current-value"]` -- returns the current value but uses the getter
 * `["input-value"]` -- returns the value
 * `["input-sender"]` -- returns the _id_ of the sending vertex
 * `["this-set!", value]` -- set the new value of the accumulator
 * `["this-*-id]` -- id functions as described above
 * `???` -- to be continued

```json
{
  "minAccumulator": {
    "clearProgram": {"isSet": false, "value": 0},
    "updateProgram": ["dict",["isSet",true],["value",
                      ["if",
                        [
                          ["attrib-get",["current-value"],"isSet"],
                            ["min",["attrib-get",["current-value"],"value"],["update-value"]]
                        ],[
                          true,
                          ["update-value"]]
                      ]
                     ]]
  }
}
```

## Possible future developments/ROADMAP

 * This project needs a much nicer surface syntax

 * It is possible (and maybe adviseable) to replace/plug in a reasonably small and controllable 
   common lisp (for instance embeddable common lisp) or a scheme (such as chicken, gambit or chez)
   instead of rolling our own. This has the advantage of having all the language features available
   it has the disatvantage of having a much less controlloable amount of code.

 * The pregel foundation in ArangoDB needs could be rewritten to be much simpler and smaller.
   This would have the advantage of fewer bugs (beacuse of less code), and more controllable
   behaviour.

[1] We called the Pregel algorithm "VertexAccumulators", because we set out writing
    an implementation of something that looks like TigerGraph's VertexAccumulators.
    This name is subject to change to a more appropriate name in the future.

[2] LISP had it 30 years ago!

[3] I don't like the word context at the moment, but for the lack of a better one, just use it atm
