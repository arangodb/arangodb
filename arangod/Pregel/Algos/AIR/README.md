# Programmable Pregel Algorithms (experimental feature)

(Placeholder: What is a programmable Pregel Algorithms )

Programmable Pregel Algorithms do require SmartGraphs. SmartGraphs are only available in the Enterprise Edition

## Definition of a custom algorithm

The format of a custom algorithm right now is based on a JSON object.

## Algorithm skeleton

```json
{
  "resultField": "<string>",
  "maxGSS": "<number>",
  "dataAccess": {
    "writeVertex": "<program>",
    "readVertex": "<array>",
    "readEdge": "<array>"
  },
  "vertexAccumulators": "<object>",
  "globalAccumulators": "<object>",
  "customAccumulators": "<object>",
  "phases": "<array>"
}
```
 
#### Algorithm parameters:
 
* resultField (optional): Document attribute as a `string`.
  * The vertex computation results will be in all vertices pointing to the given attribute.
* maxGSS (required): The max amount of global supersteps as a `number`. 
  * After the amount of max defined supersteps is reached, the Pregel execution will stop.
* dataAccess (optional): Allows to define `writeVertex`, `readVertex` and `readEdge`.
  * writeVertex: A `<program>` that is used to write the results into vertices. If `writeVertex` is used, the `resultField` will be ignored.
  * readVertex: An `array` that consists of `strings` and/or additional `arrays` (that represents a path).
    * `string`: Represents a single path at the toplevel which is **not** nested. 
    * `array of strings`: Represents a nested path   
  * readEdge: An `array` that consists of `strings` and/or additional `arrays` (that represents a path).
      * `string`: Represents a single path at the toplevel which is **not** nested. 
      * `array of strings`: Represents a nested path
* vertexAccumulators (optional): An `object` defining all used vertex accumulators. 
  * Vertex Accumulators 
* globalAccumulators (optional): An `object` defining all used global accumulators.
  * Global Accumulators are able to access variables at shared global level.
* customAccumulators (optional): An `object` defining all used custom accumulators.
* phases (optional?): Array of a single or multiple phase definitions. More info below in the next chapter.

## Phases

Phases will run sequentially during your Pregel computation. The definition of multiple phases is allowed. 
Each phase requires instructions based on the operations you want to perform.
  
The pregel program execution will follow the order:

Initialization:
1. `initProgram` (database server)  

Computation: 
1. `onPreStep` (coordinator)
2. `updateProgram` (database server)
3. `onPostStep` (coordinator)

##### Phase parameters:

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

## Program

## Vertex Accumulator

Each vertex accumulator requires a name as `string`:

```json
  {
    "<name>": {
      "accumulatorType": "<accumulator-name>",
      "valueType": "<valueType>",
      "customType": "<accumulator-type>" 
    }
  }
```

#### Vertex Accumulator Parameters:

* accumulatorType (required): The name of the used accumulator type as a `string`.
* valueType (required): The name of the value type as a `string`.
  * Valid value types are:
    * `slice` (VelocyPack Slice)
    * `ints` (Integer type)
    * `doubles`: (Double type)
    * `bools`: (Boolean type)
    * `strings`: (String type)
* customType (required): The name of the used accumulator type as a `string`.

## Global Accumulator

## Custom Accumulator

## Language primitives

Language primitives are methods which can be used inside of a program definition.

#### Calculation operators
* \+ (addition - works on a list)
* \- (substraction - works on a list)
* \* (multiplication - works on a list)
* \/ (division - works on a list)

#### Logical operators
* not  -- unary logical negation
* false? -- `["false?", val]` evaluates to `true` if `val` is considered `false`
* true? -- `["true?", val]` evaluates to `true` if `val` is considered `true`

#### Comparison operators
* eq? -- `["eq?", left, right]` evaluates to `true` if `left` is equal to `right`
* gt? -- `["gt?", left, right]` evaluates to `true` if `left` is greater than `right`
* ge? -- `["ge?", left, right]` evaluates to `true` if `left` is greater than or equal to`right`
* le? -- `["le?", left, right]` evaluates to `true` if `left` is less than or equal to `right`
* lt? -- `["lt?", left, right]` evaluates to `true` if `left` is less than `right`
* ne? -- `["ne?", left, right]` evaluates to `true` if `left` is not equal to `right`

#### Misc
* min -- todo
* max -- todo
* avg -- todo

#### Debug operators
* print -- `["print", expr ...]` print `expr` for each `expr`.
* error -- todo
* assert -- todo

#### Constructors
* dict -- todo
* dict-merge -- `["merge", dict, dict]` returns the merge of two dicts 
* dict-keys -- `["dict-keys", dict]` returns an array with all toplevel keys
* dict-directory -- `["dict-directory", dict]` returns all available paths

#### Lambdas
* lambda -- todo

#### Utilities
* list-cat -- `["list-cat", list ...]` concatenates the lists `list ...` forming one list
* string-cat -- `["string-cat", string ...]` concatenates the strings `string ...` forming one string
* int-to-str -- `["int-to-str", val]` returns a string representation of the integer `val`
 
#### Functional
* id -- todo
* apply -- todo
* map -- todo
* filter -- todo
* foldl -- todo
* foldl1 -- todo

#### Access operators
* attrib-ref -- `["attrib-ref", doc, key]`, `["attrib-ref", doc, [p ...]]`, in the first variant, extract attribute `key` from `doc`, in the second variant extract attribute with path `p/...` from doc
* attrib-get -- todo
* attrib-set -- `["attrib-set", dict, key, value]` - set dict at key to value 
* attrib-set -- `["attrib-set", dict, [path...], value]` - set dict at path to value
* array-ref -- `["array-ref", arr, index]` - get value at specified index
* array-set -- `["array-set", arr, index, value]` - set value at specified index
* var-ref -- `["var-ref", name]` refer to variable `name` in current context
* bind-ref -- todo

## Execute a custom algorithm

## Vertex Computation

## Examples

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

The following list of functions and special forms is available in all contexts. 
_AIR_ is based on lisp, but represented in JSON and supports its datatypes. 

Strings, numeric constants, booleans and objects (dicts) are self-evaluating, i.e. the result of the evaluation of those
values is the value itself. Array are not self-evaluating. In general you should read an array like a function
call:
```
["foo", 1, 2, "bar"] // read as foo(1, 2, "bar")
```
The first element of a list specifies the function. This can either be a string containing the function name, or
a lambda object. See `lambda` function below.

To prevent unwanted evaluation or to actually write down a list there are multiple options: 
`quote`, `list`, `quasi-quote`. 
```
["list", 1, 2, ["foo", "bar"]] // evaluates to [1, 2, foo("bar")] -- evaluated parameters
["quote", 1, 2, ["foo", "bar"]] // evaluates to [1, 2, ["foo", "bar"]] -- unevaluated parameters
```
For more see below.

### Truthiness of values

A value is considered _false_ if it is boolean `false` or `none`. All other values are considered true.

### Special forms

A _special form_ is special in the sense that it does not necessarily evaluate its parameters.

#### Let
_binding values to variables_
```
["let", [[var, value]...], expr...]
```
Expects as first parameter a list of name-value-pairs. Both members of each pair are evaluated. `first` has to evaluate
to a string. The following expression are then evaluated in a context where the named variables are assigned to their
given values. When evaluating the expression `let` behaves like `seq`.

```
> ["let", [["x", 12], ["y", 5]], ["+", ["var-ref", "x"], ["var-ref", "y"]]]
 = 17
```

#### Seq
_sequence of commands_
```
["seq", expr ...]
```
`seq` evaluates `expr` in order. The result values it the result value of the last expression. An empty `seq` evaluates
to `none`.

```
> ["seq", ["print, "Hello World!"], 2, 3]
Hello World!
 = 3
```

#### If
_classical if-elseif-else-statement_
```
["if", [cond, body], ...]
```
Takes pairs `[cond, body]` of conditions `cond` and expression `body` and evaluates 
the first `body` for which `cond` evaluates to a value that is considered true. It does not evaluate the other `cond`s.
If no condition matches, it evaluates to `none`. To simulate a `else` statement, set the last condition to `true`.

```
> ["if", [
        ["lt?", ["var-ref", "x"], 0],
        ["-", ["var-ref", "x"]]
    ], [
        true, // else
        ["var-ref", "x"]
    ]]
 = 5
```

#### Match
_not-so-classical switch-statement_
```
["match", proto, [c, body]...]
```
First evaluates `proto`, then evaluates each `c` until `["eq?", val, c]` is considered true. Then the corresponding `body` 
is evaluated and its return value is returned. If no branch matches, `none` is returned. This is a c-like `switch` statement
except that its `case`-values are not treated as constants.



 * for-each -- `["for-each" [(v list) ...] body ...]` binds `v` to elements of `list` in order and evaluates `body` with this binding.


#### Quote and Quote-Splice
_escape sequences for lisp_
```
["quote", expr...]
["quote-splice", expr...]
```

`quote`/`quote-splice` copies/splices its parameter verbatim into its output. `quote-splice` fails if it is called
in a context where it can not splice into something, for example at top-level.
```
> ["quote", "foo", ["bar"]]
 = ["foo", ["bar"]]
> ["foo", ["quote-splice", ["bar"], "baz"]
 = ["foo", ["bar"], "baz"]
```

#### Quasi-Quote, Unquote and Unquote-Splice
_like `quote` but can be unquoted_

 * quasi-quote/unquote/unquote-splice -- like `quote` but can be unquoted using `unquote/unquote-splice`.

```
> ["quasi-quote", ["foo"], ["unquote", ["list", 1, 2]], ["unquote-splice", ["list", 1, 2]]]
 = [["foo"],[1,2],1,2]
```

#### Cons
_constructor for lists_
```
["cons", value, list]
```
Classical lisp instruction that prepends `value` to the list `list`.
```
> ["cons", 1, [2, 3]]
 = [1, 2, 3]
```

#### And and Or
_basic logical operations_
```
["and", expr...]
["or", expr...]
```
Computes the logical `and`/`or` expression of the given expression. As they are special forms, those expression shortcut,
i.e. `and`/`or` terminates the evaluation on the first value considered `false`/`true`. The empty list evaluates as `true`/`false`.
The rules for truthiness are applied.


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
