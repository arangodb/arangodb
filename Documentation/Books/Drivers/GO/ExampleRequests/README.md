<!-- don't edit here, its from https://@github.com/arangodb/go-driver.git / docs/Drivers/ -->
# ArangoDB GO Driver - Example requests 

## Connecting to ArangoDB

```go
conn, err := http.NewConnection(http.ConnectionConfig{
    Endpoints: []string{"http://localhost:8529"},
    TLSConfig: &tls.Config{ /*...*/ },
})
if err != nil {
    // Handle error
}
c, err := driver.NewClient(driver.ClientConfig{
    Connection: conn,
    Authentication: driver.BasicAuthentication("user", "password"),
})
if err != nil {
    // Handle error
}
```

## Opening a database 

```go
ctx := context.Background()
db, err := client.Database(ctx, "myDB")
if err != nil {
    // handle error 
}
```

## Opening a collection

```go
ctx := context.Background()
col, err := db.Collection(ctx, "myCollection")
if err != nil {
    // handle error 
}
```

## Checking if a collection exists

```go
ctx := context.Background()
found, err := db.CollectionExists(ctx, "myCollection")
if err != nil {
    // handle error 
}
```

## Creating a collection

```go
ctx := context.Background()
options := &driver.CreateCollectionOptions{ /* ... */ }
col, err := db.CreateCollection(ctx, "myCollection", options)
if err != nil {
    // handle error 
}
```

## Reading a document from a collection 

```go
var doc MyDocument 
ctx := context.Background()
meta, err := col.ReadDocument(ctx, myDocumentKey, &doc)
if err != nil {
    // handle error 
}
```

## Reading a document from a collection with an explicit revision

```go
var doc MyDocument 
revCtx := driver.WithRevision(ctx, "mySpecificRevision")
meta, err := col.ReadDocument(revCtx, myDocumentKey, &doc)
if err != nil {
    // handle error 
}
```

## Creating a document 

```go
doc := MyDocument{
    Name: "jan",
    Counter: 23,
}
ctx := context.Background()
meta, err := col.CreateDocument(ctx, doc)
if err != nil {
    // handle error 
}
fmt.Printf("Created document with key '%s', revision '%s'\n", meta.Key, meta.Rev)
```

## Removing a document 

```go
ctx := context.Background()
err := col.RemoveDocument(revCtx, myDocumentKey)
if err != nil {
    // handle error 
}
```

## Removing a document with an explicit revision

```go
revCtx := driver.WithRevision(ctx, "mySpecificRevision")
err := col.RemoveDocument(revCtx, myDocumentKey)
if err != nil {
    // handle error 
}
```

## Updating a document 

```go
ctx := context.Background()
patch := map[string]interface{}{
    "Name": "Frank",
}
meta, err := col.UpdateDocument(ctx, myDocumentKey, patch)
if err != nil {
    // handle error 
}
```

## Querying documents, one document at a time 

```go
ctx := context.Background()
query := "FOR d IN myCollection LIMIT 10 RETURN d"
cursor, err := db.Query(ctx, query, nil)
if err != nil {
    // handle error 
}
defer cursor.Close()
for {
    var doc MyDocument 
    meta, err := cursor.ReadDocument(ctx, &doc)
    if driver.IsNoMoreDocuments(err) {
        break
    } else if err != nil {
        // handle other errors
    }
    fmt.Printf("Got doc with key '%s' from query\n", meta.Key)
}
```

## Querying documents, fetching total count

```go
ctx := driver.WithQueryCount(context.Background())
query := "FOR d IN myCollection RETURN d"
cursor, err := db.Query(ctx, query, nil)
if err != nil {
    // handle error 
}
defer cursor.Close()
fmt.Printf("Query yields %d documents\n", cursor.Count())
```

## Querying documents, with bind variables

```go
ctx := context.Background()
query := "FOR d IN myCollection FILTER d.Name == @name RETURN d"
bindVars := map[string]interface{}{
    "name": "Some name",
}
cursor, err := db.Query(ctx, query, bindVars)
if err != nil {
    // handle error 
}
defer cursor.Close()
...
```
