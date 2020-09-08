# Programmable Pregel Algorithms (experimental feature)

(Placeholder: What is a programmable Pregel Algorithms )

Programmable Pregel Algorithms do require SmartGraphs. SmartGraphs are only available in the Enterprise Edition

## Definition of a custom algorithm

The format of a custom algorithm right now is based on a JSON object.
 
#### Main algorithm parameters:
 
* resultField (optional): Document attribute as a `string`.
  * The vertex computation results will be in all vertices pointing to the given attribute.
* maxGSS (required): The max amount of global supersteps as a `number`. 
  * After the amount of max defined supersteps is reached, the Pregel execution will stop.  
* globalAccumulators (optional?): 
* vertexAccumulators (optional?):
* customAccumulators (optional?):
* phases (optional?): Array of a single or multiple phase definitions. More info below in the next chapter. 

#### Phases

Phases will run sequentially during your Pregel computation. The definition of multiple phases are allowed. 
Each phase requires instructions based on the operations you want to perform.
  
The computation will follow the order:

Initialization:
1. `initProgram` (database server)  

Computation: 
1. `onPreStep` (coordinator)
2. `updateProgram` (database server)
3. `onPostStep` (coordinator)

#### Phase parameters:

* name (required): Name as a `string`.
  * The given name of the defined phase.
* initProgram: Program as `array of operations` to be executed.
  * The init program will run **initially once** per all vertices that are part of your graph. 
* onPreStep: Program as `array of operations` to be executed.
  * The _onPreStep_ program will run **once before** each pregel execution round.
* updateProgram: Program as `array of operations` to be executed.
 * The _updateProgram_ will be executed **during every pregel execution round** and each **per vertex**. 
* onPostStep Program as `array of operations` to be executed.
* The _onPostStep_ program will run **once after** each pregel execution round. 


Example:

```
{
  resultField: "
}
```

## Execute a custom algorithm

## Primitives

## Vertex Computation

___

#OLD SECTION


# AIR Pregel Algorithm architecture


## EdgeData

## VertexData

## MessageData

## GraphFormat

## VertexComputation

## MasterContext 

## WorkerContext


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
 * quote-splice -- `["foo", ["quote-splice", expr ...]]` evaluates to `["foo", expr ...]`
 * cons -- `["cons", val, [expr ...]]` evaluates to `[val, expr ...]`
 * and -- `["and", expr ...]` evaluates to false if one of `expr` evaluates to `false`. The arguments `expr` are evaluated in order, and evaluation is stopped once an `expr` evaluates to `false`.
 * or -- `["or", expr ...]` evaluates to true if one of `expr` evaluates to `true`. The arguments `expr` are evaluated in order, and evaluation is stopped once an `expr` evaluates to `true`.
 * seq -- `["seq", expr ... lastexpr ]` evaluates `expr` in order, the value of a `seq` expression is `lastexpr`
 * match -- `["match", expr, [c, body] ...]` evaluates `expr` to `val` and evaluates `body` for the first pair `[c, body]`, where `["eq?", val, c]` is `true`.
 * for-each -- `["for-each" [(v list) ...] body ...]` binds `v` to elements of `list` in order and evaluates `body` with this binding.
 * quasi-quote/unquote/unquote-splice -- like `quote` but can be unquoted using `unquote/unquote-splice`. For example 
 `["quasi-quote", ["foo"], ["unquote", ["list", 1, 2]], ["unquote-splice", ["list", 1, 2]]]` evaluates to `[["foo"],[1,2],1,2]`.

### Language primitives

 * var-ref -- `["var-ref", name]` refer to variable `name` in current context
 * var-set!  -- `["var-set!", name, value]` set variable `name` in current context to `value`

 * attrib-ref -- `["attrib-ref", doc, key]`, `["attrib-ref", doc, [p ...]]`, in the first variant, extract attribute `key` from `doc`, in the second variant extract attribute with path `p/...` from doc
 * attrib-set -- `["attrib-set", dict, key, value]` - set dict at key to value 
 * attrib-set -- `["attrib-set", dict, [path...], value]` - set dict at path to value
 
 * array-ref -- `["array-ref", arr, index]` - get value at specified index
 * array-set -- `["array-set", arr, index, value]` - set value at specified index
 
 * dict-merge -- `["merge", dict, dict]` returns the merge of two dicts 
 * dict-keys -- `["dict-keys", dict]` returns an array with all toplevel keys
 * dict-directory -- `["dict-directory", dict]` returns all available paths

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

 * `["this-doc"]` returns the document slice stored in vertexData
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
