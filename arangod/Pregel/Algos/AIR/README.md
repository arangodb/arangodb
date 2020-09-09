# Programmable Pregel Algorithms (experimental feature)

Pregel is a system for large scale graph processing. It is already implemented in ArangoDB and can be used
with **predefined** algorithms, e.g. _PageRank_, _Single-Source Shortest Path_ and _Connected components_. 
Programmable Pregel algorithms are based on the already existing ArangoDB Pregel engine. The big change here is
the possibility to write and execute your own defined algorithms.

The best part: You can write, develop and execute your custom algorithms **without** having to plug C++ code into
ArangoDB (and re-compile). Algorithms can be defined and executed in a running ArangoDB instance without the need
of restarting your instance.

To keeps things more readable, "Programmable Pregel Algorithms" will be called PPAs in the next chapters.

**Important**: The naming might change in the future. As this feature is experimental in development, execution
times are not representative by now.

# Requirements

PPAs can be run on a single-server instance but as it is designed to run in parallel
in a distributed environment, you'll only be able to add computing power in a clustered environment. Also   
PPAs do require proper graph sharding to be efficient. Using SmartGraphs is the recommend way to run Pregel
algorithms. 

As this is an extension of the native Pregel framework, more detailed information on prerequisites and
requirements can be found here: "arangod/Pregel/Algos/AIR/README.md" (TODO: add Link)

# Basics

A Pregel computation consists of a sequence of iterations, each one of them is called a superstep.
During a superstep, the custom algorithm will be executed for each vertex. This is happening in parallel,
as the vertices are communicating via messages and not with shared memory.

The basic methods are:
- Read messages which are sent to the vertex V in the previous superstep (S-1)
- Send messages to other vertices that will be received in the next superstep (S+1)
- Modify the state of the vertex V
- Vote to Halt (mark a vertex V as "done". V will be inactive in S+1, but it is possible to re-activate)

More details on this in the next chapters.

# Definition of a custom algorithm

The format of a custom algorithm right now is based on a JSON object.

# Algorithm skeleton

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
 
### Algorithm parameters:
 
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

# Phases

Phases will run sequentially during your Pregel computation. The definition of multiple phases is allowed. 
Each phase requires instructions based on the operations you want to perform.
  
The pregel program execution will follow the order:

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

# Program

As the name already indicates, the _Program_ is the part where the actual algorithmic action takes place.
Currently a program is represented with the Arango Intermediate Representation - currently called "AIR".

### AIR (Arango Intermediate Representation - note: naming might change in the future)

We developed a LISPy intermediate represenatation to be able to transport programs into
the existing Pregel implementation in ArangoDB. These programs are executed using the
Greenspun interpreter inside the AIR Pregel algorithm.

At the moment this interpreter is a prototype and hence not optimized and (probably)
slow. It is very flexible in terms of what we can implement, provide and test:
We can provide any function as a primitive in the language, and all basic
operations are available as it is customary in the LISP tradition.

**IMPORTANT NOTE**: The intention is *not* that this language is presented to users as is.
This is only the representation we're using in our early stage of that experimental feature
state. It is merely an intermediate representation which is very flexible for good
prototyping. A surface syntax is subject to development and even flexible in
terms of providing more than one. In particular this way we get a better feeling for which
functionality is needed by clients and users of graph analytics.

A surface language / syntax will be available later. 

## Current AIR specification

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

#### Lambdas
```
["lambda", captures, parameters, body]
```
`lambda` create a new function object. `captures` is a list of variables that are _copied into_ the lambda at creating 
time. `parameters` is a list of names that the parameters are bound to. Both can be accessed using their name via 
`var-ref`. `body` is evaluated when the lambda is called.

```
> [["lambda", ["quote"], ["quote", "x"], ["quote", "+", ["var-ref", "x"], 4]], 6]
 = 10
```

#### Utilities
_random functions that fit no other category_
```
["string-cat", strings...]
["int-to-str", int]
["min", numbers...]
["max", numbers...]
["avg", numbers...]
```
`string-cat` concatenates the given strings. `int-to-string` converts a integer to its decimal representation.
`min`/`max`/`avg` computes the minimum/maximum/average of its values.

```
> ["string-cat", "hello", " ", "world"]
 = "hello world"
> ["min", 1, 2, 3]
 = 1
> ["max", 1, 2, 3]
 = 3
> ["avg", 1, 2, 3]
 = 2
```
 
#### Functional
```
["id", value]
["apply", func, list]
["map", func, list]
["map", func, dict]
```
`id` returns its argument. `apply` invokes `func` using the values from `list` as arguments. `map` invokes `func` for
every value/key-value-pair in the `list`/`dict`. `func` should accept one parameter `(value)`/two parameters `(key, value)`.

```
> ["id", 12]
 = 12
> ["apply", "min", ["quote", 1, 2, 3]]
 = 1
> ["map", "int-to-string", ["quote", 1, 2, 3, 4]]
 = ["1", "2", "3", "4"]
```

#### Variables
```
["var-ref", name]
["bind-ref", name]
```
`var-ref` evalutes to the current value of the variable with name `name`. It is an error to reference a variable that
is not defined in the current context. `bind-ref` is an alias of `var-ref`.

#### Debug operators
```
["print", values...]
["error", msg...]
["assert", cond, msg...]
```
`print` print in an context dependent way the string representation of its arguments joined by spaces. Strings represent
themselves, numbers are converted to decimal representation, booleans are represented as `true` or `false`. Dicts and lists
are converted to JSON.
`error` creates an error an aborts execution immediately. Errors are reported in a context dependent way. The error
message is constructed from the remaining parameter like `print`, except that it is not printed but 
associated with the error. This like a panic or an uncaught exception.

`assert` checks if cond is considered true if it an error with the remaining parameters as message is raised.
It is equivalent to `["if", [cond, ["error", msg...]]]`.

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

Executing a PPA using the `pregel.start` method will deliver unique ID to the status of the algorithm execution.

```js
  let pregelID = pregel.start("air", graphName, "<custom-algorithm>");
  var status = pregel.status(pregelID);
```

The result will tell you the current status of the algorithm execution. It will tell you the current state of the
execution, the current global superstep, the runtime, the global aggregator values as well as the number of send and
received messages.

More detailed information about the status can be found [here](https://www.arangodb.com/docs/stable/graphs-pregel.html#status-of-an-algorithm-execution).
Additionally, the status objects for custom algorithms is extended and contains more info as the general pregel one.
More details in the next chapter. 

#### Error reporting

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

Also we've added a few debugging primitives to help you increase your developing speed. For example, there is
the possibility to add "prints" to your program.
  
For more, please take a look at the _Debug operators_ contained in the chapter: "Language primitives".

## Developing a Programmable Pregel Algorithm

There are two ways of developing your PPA. You can either run and develop in the ArangoShell or you can use the
Foxx Service "Pregelator" (_Development name: This might change in the future as well_). The Pregelator can be installed
seperately and provides a nice UI to write a PPA, execute it and get direct feedback in both "success" and "error"
cases.

#### Pregelator

The Pregelator Service is available on GitHub:
-  https://github.com/arangodb-foxx/pregelator

The bundled ZIP files are kept in the directory: `zippedBuilds` and can be installed via `foxx-cli`, the standard
`web-ui` or via `arangosh`. 

## Examples

As there are almost no limits regarding the definition of a PPA, here we will provide a basic example of the 
"vertex-degree" algorithm and demonstrate how the implementation would look like.  

Note: We've implemented also more complex algorithms in PPA to demonstrate advanced usage. As those are complex
algorithms, they are not included as examples in the documentation. But for the curious ones, they can be found
here:

- [Propagation Demo](https://github.com/arangodb/arangodb/blob/feature/pregel-vertex-accumulation-algorithm-2/js/client/modules/%40arangodb/air/propagation-demo.js)
- [PageRank](https://github.com/arangodb/arangodb/blob/feature/pregel-vertex-accumulation-algorithm-2/js/client/modules/%40arangodb/air/pagerank.js)
- [Single Source Shortest Path](https://github.com/arangodb/arangodb/blob/feature/pregel-vertex-accumulation-algorithm-2/js/client/modules/%40arangodb/air/single-source-shortest-paths.js)
- [Strongly Connected Components](https://github.com/arangodb/arangodb/blob/feature/pregel-vertex-accumulation-algorithm-2/js/client/modules/%40arangodb/air/strongly-connected-components.js)

#### Vertex Degree



## Vertex Computation
___

#OLD SECTION

# AIR Pregel Algorithm architecture (TODO: move chapters to a dedicated document - out of documentation scope)

## EdgeData

## VertexData

## MessageData

## GraphFormat

## VertexComputation

## MasterContext 

## WorkerContext




## Writing Vertex Accumulator Algorithms using AIR



## Possible future developments/ROADMAP

 * A much nicer surface syntax will be provided in the future.

 * It is possible (and maybe adviseable) to replace/plug in a reasonably small and controllable 
   common lisp (for instance embeddable common lisp) or a scheme (such as chicken, gambit or chez)
   instead of rolling our own. This has the advantage of having all the language features available
   it has the disatvantage of having a much less controlloable amount of code.

 * The pregel foundation in ArangoDB needs could be rewritten / optimized to be much simpler and smaller.
   This would have the advantage of fewer bugs (beacuse of less code), and more controllable behaviour.
