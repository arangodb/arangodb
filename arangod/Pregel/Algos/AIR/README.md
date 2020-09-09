# Programmable Pregel Algorithms (experimental feature)

Pregel is a system for large scale graph processing. It is already implemented in ArangoDB and can be used
with **predefined** algorithms, e.g. _PageRank_, _Single-Source Shortest Path_ and _Connected components_. 
 
Programmable Pregel algorithms are based on the already existing ArangoDB Pregel engine. The big change here is
the possibility to write and execute your own defined algorithms.

To keeps things more readable, "Programmable Pregel Algorithms" will be called PPAs in the next chapters.

**Important**: The naming might change in the future. 

## Requirements

PPAs can be run on a single-server instance but as it is designed to run in parallel
in a distributed environment, you'll only be able to add computing power in a clustered environment. Also   
PPAs do require proper graph sharding to be efficient. Using SmartGraphs is the recommend way to run Pregel
algorithms. 

As this is an extension of the native Pregel framework, more detailed information on prerequisites and
requirements can be found here: "arangod/Pregel/Algos/AIR/README.md" (TODO: add Link)

## Basics

A Pregel computation consists of a sequence of iterations, each one of them is called a superstep.
During a superstep, the custom algorithm will be executed for each vertex. This is happening in parallel,
as the vertices are communicating via messages and not with shared memory.

The basic methods are:
- Read messages which are sent to the vertex V in the previous superstep (S-1)
- Send messages to other vertices that will be received in the next superstep (S+1)
- Modify the state of the vertex V
- Vote to Halt (mark a vertex V as "done". V will be inactive in S+1, but it is possible to re-activate)

More details on this in the next chapters.

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

### Language primitives

Language primitives are methods which can be used inside of a program definition. They execute on local state
and do not require network communication.

## Execute a Programmable Pregel Algorithm

Except the precondition to have your custom defined algorithm, the execution of a PPA follows the basic Pregel
implementation. To start a PPA, you need to require the Pregel module in _arangosh_.  

```js
const pregel = require("@arangodb/pregel");
  return pregel.start(
    "air",
    graphName,
    "<custom-algorithm>"
  );
```

## Status of a Programmable Pregel Algorithm

## Developing a Programmable Pregel Algorithm

Before the execution of a PPAs starts, it will be validated and checked for potential errors.
This helps a lot during development. If a PPA fails, the status will be "fatal error". In that case
there will be an additional field called `reports`. All debugging messages and errors will be listed
there. Also you'll get detailed information when, where and why the error occured.

Example:
```js
{
  "reports": [{
    "msg": "in phase `init` init-program failed: pregel program returned \"vote-halts\", expect one of `none`, `true`, `false`, `\"vote-halt\", or `\"vote-active\"`\n",
    "level": "error",
    "annotations": {
      "vertex": "LineGraph10_V/0:4020479",
      "pregel-id": {
        "key": "0:4020479",
        "shard": 1
      },
      "phase-step": 0,
      "phase": "init",
      "global-superstep": 0
    }
  }]
}
```


Also we've added a few debugging primitives to help you
increase your developing speed. For example, there is the possibility to add "prints" to your program.
  
For more, please take a look at the _Debug operators_ contained in the chapter: "Language primitives".

## Vertex Computation

## Examples

___

#OLD SECTION


# AIR Pregel Algorithm architecture (TODO: move chapter)


## EdgeData

## VertexData

## MessageData

## GraphFormat

## VertexComputation

## MasterContext 

## WorkerContext


# AIR (Arango Intermediate Representation) (TODO: move chapter)

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

#### For-Each
```
["for-each", [[var, list]...] expr...]
```
Behaves similar to `let` but expects a list as `value` for each variable. It then produces the cartesian product of all
lists and evaluates its expression for each n-tuple. The return value is always `none`. The order is guaranteed to be
reversed lexicographic order.

```
> ["for-each", [["x", [1, 2]], ["y", [3, 4]]], ["print", ["var-ref", "x"], ["var-ref", "y"]]]
1 3
1 4
2 3
2 4
(no value)
```

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

_There is also a `not`, but its not a special form. See below._

### Language Primitives

Language primitives are methods which can be used in any context. As those are functions, all parameters are always
evaluated before passed to the function.

#### Basic Algebraic Operators
_left-fold with algebraic operators and the first value as initial value_
```
["+", ...]
["-", ...]
["*", ...]
["/", ...]
```
All operators accept multiple parameters. The commutative operators `+`/`*` calculate the sum/product of all values
passed. The empty list evaluates to `0`/`1`. The operator `-` subtracts the remaining operands from the first, while `/`
divides the first operand by the remaining. Again empty lists evaluate to `0`/`1`.
```
> ["+", 1, 2, 3]
 = 6
> ["-", 5, 3, 2]
 = 0
```

#### Logical operators
_convert values to booleans according to their truthiness_
```
["true?", expr]
["false?", expr]
["not", expr]
```
`true?` returns true if `expr` is considered true, returns false otherwise. 
`false?` returns true if `expr` is considered false, returns true otherwise.
`not` is an alias for `false?`.

```
> ["true?", 5]
 = true
> ["true?", 0]
 = true
> ["true?", false]
 = false
> ["true?", "Hello world!"]
 = true
> ["false?", 5]
 = false
```

#### Comparison operators
_compares on value to other values_
```
["eq?", proto, expr...]
["gt?", proto, expr...]
["ge?", proto, expr...]
["le?", proto, expr...]
["lt?", proto, expr...]
["ne?", proto, expr...]
```
Compares `proto` to all other expressions according to the selected operator. Returns true if all comparisons are true.
Returns true for the empty list. Relational operators are only available for numeric values. If proto is a boolean value
the other values are first converted to booleans using `true?`, i.e. you compare their truthiness.

The operator names translate as follows:
* `eq?` -- `["eq?", left, right]` evaluates to `true` if `left` is equal to `right`
* `gt?` -- `["gt?", left, right]` evaluates to `true` if `left` is greater than `right`
* `ge?` -- `["ge?", left, right]` evaluates to `true` if `left` is greater than or equal to`right`
* `le?` -- `["le?", left, right]` evaluates to `true` if `left` is less than or equal to `right`
* `lt?` -- `["lt?", left, right]` evaluates to `true` if `left` is less than `right`
* `ne?` -- `["ne?", left, right]` evaluates to `true` if `left` is not equal to `right`

Given more than two parameters
```
[<op>, proto, expr_1, expr_2, ...]
```
is equivalent to
```
["and", [<op>, proto, expr_1], [<op>, proto, expr_2], ...]
```
except that `proto` is only evaluated once.

```
> ["eq?", "foo", "foo"]
 = true
> ["lt?", 1, 2, 3]
 = true
> ["lt?", 1, 3, 0]
 = false
> ["ne?", "foo", "bar"]
 = false
```

#### Lists
_sequential container of inhomogeneous values_
```
["list", expr...]
["list-cat", lists...]
["list-append", list, expr...]
["list-ref", list, index]
["list-set", arr, index, value]
["list-empty?", value]
["list-length", list]
```
`list` constructs a new list using the evaluated `expr`s. `list-cat` concatenates given lists. `list-append` returns a 
new list, consisting of the old list and the evaluated `expr`s. `list-ref` returns the value at `index`. Accessing out
of bound is an error. Offsets are zero based. `list-set` returns a copy of the old list, where the entry and index `index`
is replaced by `value`. Writing a index that is out of bounds is an error. `list-empty?` returns true if and only if the
given value is an empty list. `list-length` returns the length of the list.

#### Dicts
```
["dict", [key, value]...]
["dict-merge", dict...]
["dict-keys", dict]
["dict-directory", dict]

["attrib-ref", dict, key]
["attrib-ref", dict, path]
["attrib-set", dict, key, value]
["attrib-set", dict, path, value]
```
`dict` creates a new dict using the specified key-value pairs. It is undefined behavior to specified a key more than once.
`dict-merge` merges two or more dicts, keeping the latest occurrence of a key. `dict-keys` returns a list of all
toplevel keys. `dict-directory` returns a list of all available paths in preorder.

`attrib-ref` returns the value of `key` in `dict`. If `key` is not present `none` is returned. `attrib-set` returns a copy
of `dict` but with `key` set to `value`. Both functions have a variant that accepts a path. A path is a list of strings.
The function will recurse into the dict using that path. `attrib-set` returns the whole dict but with updated subdict.

#### Constructors

#### Lambdas
* lambda -- todo

#### Utilities
* string-cat -- `["string-cat", string ...]` concatenates the strings `string ...` forming one string
* int-to-str -- `["int-to-str", val]` returns a string representation of the integer `val`
 
#### Functional
* id -- todo
* apply -- todo
* map -- todo
* filter -- todo
* foldl -- todo
* foldl1 -- todo

#### Variables
* var-ref -- `["var-ref", name]` refer to variable `name` in current context
* bind-ref -- alias for `var-ref`

#### Misc
* min -- todo
* max -- todo
* avg -- todo

#### Debug operators
* print -- `["print", expr ...]` print `expr` for each `expr`.
* error -- todo
* assert -- todo


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
