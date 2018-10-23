<!-- don't edit here, it's from https://@github.com/arangodb/go-driver.git / docs/Drivers/ -->
# ArangoDB GO Driver - Getting Started

## Supported versions

- ArangoDB versions 3.1 and up.
    - Single server & cluster setups
    - With or without authentication
- Go 1.7 and up.

## Go dependencies 

- None (Additional error libraries are supported).

## Configuration

To use the driver, first fetch the sources into your GOPATH.

```sh
go get github.com/arangodb/go-driver
```

Using the driver, you always need to create a `Client`.
The following example shows how to create a `Client` for a single server 
running on localhost.

```go
import (
	"fmt"

	driver "github.com/arangodb/go-driver"
	"github.com/arangodb/go-driver/http"
)

...

conn, err := http.NewConnection(http.ConnectionConfig{
    Endpoints: []string{"http://localhost:8529"},
})
if err != nil {
    // Handle error
}
c, err := driver.NewClient(driver.ClientConfig{
    Connection: conn,
})
if err != nil {
    // Handle error
}
```

Once you have a `Client` you can access/create databases on the server, 
access/create collections, graphs, documents and so on.

The following example shows how to open an existing collection in an existing database 
and create a new document in that collection.

```go
// Open "examples_books" database
db, err := c.Database(nil, "examples_books")
if err != nil {
    // Handle error
}

// Open "books" collection
col, err := db.Collection(nil, "books")
if err != nil {
    // Handle error
}

// Create document
book := Book{
    Title:   "ArangoDB Cookbook",
    NoPages: 257,
}
meta, err := col.CreateDocument(nil, book)
if err != nil {
    // Handle error
}
fmt.Printf("Created document in collection '%s' in database '%s'\n", col.Name(), db.Name())
```

## API design 

### Concurrency

All functions of the driver are stricly synchronous. They operate and only return a value (or error)
when they're done. 

If you want to run operations concurrently, use a go routine. All objects in the driver are designed 
to be used from multiple concurrent go routines, except `Cursor`.

All database objects (except `Cursor`) are considered static. After their creation they won't change.
E.g. after creating a `Collection` instance you can remove the collection, but the (Go) instance 
will still be there. Calling functions on such a removed collection will of course fail.

### Structured error handling & wrapping

All functions of the driver that can fail return an `error` value. If that value is not `nil`, the 
function call is considered to be failed. In that case all other return values are set to their `zero` 
values.

All errors are structured using error checking functions named `Is<SomeErrorCategory>`.
E.g. `IsNotFound(error)` return true if the given error is of the category "not found". 
There can be multiple internal error codes that all map onto the same category.

All errors returned from any function of the driver (either internal or exposed) wrap errors 
using the `WithStack` function. This can be used to provide detail stack trackes in case of an error.
All error checking functions use the `Cause` function to get the cause of an error instead of the error wrapper.

Note that `WithStack` and `Cause` are actually variables to you can implement it using your own error 
wrapper library. 

If you for example use https://github.com/pkg/errors, you want to initialize to go driver like this:
```go
import (
    driver "github.com/arangodb/go-driver"
    "github.com/pkg/errors"
)

func init() {
    driver.WithStack = errors.WithStack 
    driver.Cause = errors.Cause
}
```

### Context aware 

All functions of the driver that involve some kind of long running operation or 
support additional options not given as function arguments, have a `context.Context` argument. 
This enables you cancel running requests, pass timeouts/deadlines and pass additional options.

In all methods that take a `context.Context` argument you can pass `nil` as value. 
This is equivalent to passing `context.Background()`.

Many functions support 1 or more optional (and infrequently used) additional options.
These can be used with a `With<OptionName>` function.
E.g. to force a create document call to wait until the data is synchronized to disk, 
use a prepared context like this:
```go
ctx := driver.WithWaitForSync(parentContext)
collection.CreateDocument(ctx, yourDocument)
```
