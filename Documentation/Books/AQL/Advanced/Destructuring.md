Destructuring
=============

<small>Introduced in: v3.5.0</small>

{% hint 'info' %}
Destructuring is only supported in the `LET` statement.
{% endhint %}

Array Destructuring
-------------------

Array destructuring is the assignment of array values
into one or multiple variables with a single `LET` assignment. This can be
achieved by putting the target assignment variables of the `LET` assignment
into angular brackets.

For example, the statement:

```js
LET [y, z] = [1, 2]
```

… will assign the value `1` to variable `y` and the value `2` to variable `z`.
The mapping of input array members to the target variables is positional.

It is also possible to skip over unneeded array values. The following assignment
will assign the value `2` to variable `y` and the value `3` to variable `z`. The
array member with value `1` will not be assigned to any variable:

```js
LET [, y, z] = [1, 2, 3]
```

When there are more variables assigned than there are array members, the target
variables that are mapped to non-existing array members are populated with a value
of `null`. The assigned target variables will also receive a value of `null` when
the destructuring is used on a non-array.

Destructuring can also be applied on nested arrays, e.g.

```js
LET [[sub1], [sub2, sub3]] = [["foo", "bar"], [1, 2, 3]]
```

In the above example, the value of variable `sub1` will be `foo`, the value of variable
`sub2` will be `1` and the value of variable `sub3` will be `2`.

Object Destructuring
--------------------

Object destructuring is the assignment
of multiple target variables from a source object value. This is achieved by using the
curly brackets after the `LET` assignment token:

```js
LET { name, age } = { valid: true, age: 39, name: "John Doe" }
```

The mapping of input object members to the target variables is by name.
In the above example, the variable `name` will get a value of `John Doe`, the variable
`age` will get a value of `39`. The attribute `valid` from the source object will be
ignored.

Object destructuring also works with nested objects, e.g.

```js
LET { name: {first, last} } = { name: { first: "John", middle: "J", last: "Doe" } }
```

The above statement will assign the value `John` to the variable `first` and the value
`Doe` to the variable `last`. The attribute `middle` from the source object is ignored.
Note that here only variables `first` and `last` will be populated, but variable `name`
is not.

It is also possible for the target variable to get a different name than in the source
object, e.g.

```js
LET { name: {first: firstName, last: lastName} } = { name: { first: "John", last: "Doe" } }
```

The above statement assigns the value `John` to the target variable `firstName` and the
value `Doe` to the target variable `lastName`. Note that neither of these attributes exist
in the source object.
