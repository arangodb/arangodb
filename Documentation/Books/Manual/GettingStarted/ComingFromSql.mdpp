!CHAPTER Coming from SQL

If you worked with a relational database management system (RDBMS) such as MySQL,
MariaDB or PostgreSQL, you will be familiar with its query language, a dialect
of SQL (Structured Query Language).

ArangoDB's query language is called AQL. There are some similarities between both
languages despite the different data models of the database systems. The most
notable difference is probably the concept of loops in AQL, which makes it feel
more like a programming language. It suites the schema-less model more natural
and makes the query language very powerful while remaining easy to read and write.

To get started with AQL, have a look at our detailed
[comparison of SQL and AQL](https://arangodb.com/why-arangodb/sql-aql-comparison/).
It will also help you to translate SQL queries to AQL when migrating to ArangoDB.

!SUBSECTION How do browse vectors translate into document queries?

In traditional SQL you may either fetch all columns of a table row by row, using
`SELECT * FROM table`, or select a subset of the columns. The list of table
columns to fetch is commonly called *column list* or *browse vector*:

```sql
SELECT columnA, columnB, columnZ FROM table
```

Since documents aren't two-dimensional, and neither do you want to be limited to
returning two-dimensional lists, the requirements for a query language are higher.
AQL is thus a little bit more complex than plain SQL at first, but offers much
more flexibility in the long run. It lets you handle arbitrarily structured
documents in convenient ways, mostly leaned on the syntax used in JavaScript.

!SUBSUBSECTION Composing the documents to be returned

The AQL `RETURN` statement returns one item per document it is handed. You can
return the whole document, or just parts of it. Given that *oneDocument* is
a document (retrieved like `LET oneDocument = DOCUMENT("myusers/3456789")`
for instance), it can be returned as-is like this:

```js
RETURN oneDocument
```

```json
[
    {
        "_id": "myusers/3456789",
        "_key": "3456789"
        "_rev": "14253647",
        "firstName": "John",
        "lastName": "Doe",
        "address": {
            "city": "Gotham",
            "street": "Road To Nowhere 1"
        },
        "hobbies": [
            { name: "swimming", howFavorite: 10 },
            { name: "biking", howFavorite: 6 },
            { name: "programming", howFavorite: 4 }
        ]
    }
]
```

Return the hobbies sub-structure only:

```js
RETURN oneDocument.hobbies
```

```json
[
    [
        { name: "swimming", howFavorite: 10 },
        { name: "biking", howFavorite: 6 },
        { name: "programming", howFavorite: 4 }
    ]
]
```

Return the hobbies and the address:

```js
RETURN {
    hobbies: oneDocument.hobbies,
    address: oneDocument.address
}
```

```json
[
    {
        hobbies: [
            { name: "swimming", howFavorite: 10 },
            { name: "biking", howFavorite: 6 },
            { name: "programming", howFavorite: 4 }
        ],
        address: {
            "city": "Gotham",
            "street": "Road To Nowhere 1"
        }
    }
]
```

Return the first hobby only:

```js
RETURN oneDocument.hobbies[0].name
```

```json
[
    "swimming"
]
```

Return a list of all hobby strings:

```js
RETURN { hobbies: oneDocument.hobbies[*].name }
```

```json
[
    { hobbies: ["swimming", "biking", "porgramming"] }
]
```

More complex [array](../../AQL/Functions/Array.html) and
[object manipulations](../../AQL/Functions/Document.html) can be done using
AQL functions and [operators](../../AQL/Operators.html).
