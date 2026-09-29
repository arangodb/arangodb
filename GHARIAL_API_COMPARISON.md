# OpenAPI Spec vs C++ Implementation Comparison for /_api/gharial Endpoints

## Overview
This report compares the OpenAPI specification at `/home/neunhoef/ArangoDB/js/apps/system/_admin/aardvark/APP/api-docs.json` 
with the C++ implementation in `/home/neunhoef/ArangoDB/arangod/RestHandler/RestGraphHandler.cpp` for the `/_api/gharial` endpoints.

## Endpoint Comparison

### 1. `/_api/gharial` (List/Create Graphs)

#### OpenAPI Spec:
- **GET** (listGraphs): List all graphs
  - Response codes: 200
- **POST** (createGraph): Create a graph
  - Query params: `waitForSync` (optional)
  - Request body: `name` (required), `edgeDefinitions`, `isDisjoint`, `isSmart`, `options`, `orphanCollections`
  - Response codes: 201, 202, 400, 403, 409

#### C++ Implementation:
- **GET**: `graphActionReadGraphs()` - ✅ Matches
- **POST**: `graphActionCreateGraph()` - ✅ Matches
  - Parses `waitForSync` from query params
  - Uses body fields correctly

#### Status: ✅ **MATCH**

---

### 2. `/_api/gharial/{graph-name}` (Read/Delete Graph)

#### OpenAPI Spec:
- **GET** (getGraph): Get a graph
  - Response codes: 200, 404
- **DELETE** (deleteGraph): Drop a graph
  - Query params: `dropCollections` (optional)
  - Response codes: 202, 403, 404

#### C++ Implementation:
- **GET**: `graphActionReadGraphConfig()` - ✅ Matches
- **DELETE**: `graphActionRemoveGraph()` - ✅ Matches
  - Parses `waitForSync` and `dropCollections` from query params

#### Status: ✅ **MATCH**
**Note**: C++ also reads `waitForSync` param for DELETE, which is NOT documented in OpenAPI spec

---

### 3. `/_api/gharial/{graph-name}/vertex` (List/Add Vertex Collections)

#### OpenAPI Spec:
- **GET** (listVertexCollections): List node collections
  - Response codes: 200, 404
- **POST** (addVertexCollection): Add a node collection
  - Request body: `collection` (required), `options`
  - Response codes: 201, 202, 400, 403, 404

#### C++ Implementation:
- **GET**: `graphActionReadConfig()` with TRI_COL_TYPE_DOCUMENT - ✅ Matches
- **POST**: `modifyVertexDefinition()` with CREATE action - ✅ Matches
  - Parses `waitForSync`, `createCollection` from query params

#### Status: ✅ **MATCH**
**Note**: C++ reads additional query params (`waitForSync`, `createCollection`) not documented in OpenAPI POST

---

### 4. `/_api/gharial/{graph-name}/vertex/{collection-name}` (Create Vertex/Remove Collection)

#### OpenAPI Spec:
- **POST** (createVertex): Create a node
  - Query params: `waitForSync`, `returnNew` (optional)
  - Headers: `x-arango-trx-id` (optional)
  - Request body: Present (vertex document)
  - Response codes: 201, 202, 403, 404, 410
- **DELETE** (deleteVertexCollection): Remove a node collection
  - Query params: `dropCollection` (optional)
  - Response codes: 200, 202, 400, 403, 404

#### C++ Implementation:
- **POST**: `vertexActionCreate()` - ✅ Matches
  - Reads `waitForSync`, `returnNew` from query
- **DELETE**: `modifyVertexDefinition()` with REMOVE action - ✅ Matches
  - Reads `waitForSync`, `dropCollection` from query

#### Status: ✅ **MATCH**
**Note**: C++ reads `waitForSync` for DELETE, which is NOT documented in OpenAPI

---

### 5. `/_api/gharial/{graph-name}/vertex/{collection-name}/{vertex-key}` (CRUD Vertex)

#### OpenAPI Spec:
- **GET** (getVertex): Get a node
  - Headers: `If-Match`, `If-None-Match`, `x-arango-trx-id` (optional)
  - Response codes: 200, 304, 403, 404, 410, 412
- **PATCH** (updateVertex): Update a node
  - Query params: `waitForSync`, `keepNull`, `returnOld`, `returnNew` (optional)
  - Headers: `If-Match`, `x-arango-trx-id` (optional)
  - Request body: Present (partial vertex document)
  - Response codes: 200, 202, 403, 404, 410, 412
- **PUT** (replaceVertex): Replace a node
  - Query params: `waitForSync`, `keepNull`, `returnOld`, `returnNew` (optional)
  - Headers: `If-Match`, `x-arango-trx-id` (optional)
  - Request body: Present (full vertex document)
  - Response codes: 200, 202, 403, 404, 410, 412
- **DELETE** (deleteVertex): Remove a node
  - Query params: `waitForSync`, `returnOld` (optional)
  - Headers: `If-Match`, `x-arango-trx-id` (optional)
  - Response codes: 200, 202, 403, 404, 410, 412

#### C++ Implementation:
- **GET**: `vertexActionRead()` - ✅ Matches
  - Handles `If-None-Match` header
  - Calls `handleRevision()` for `If-Match` and `rev` query param
- **PATCH**: `vertexActionUpdate()` → `vertexModify()` → `documentModify()` - ✅ Matches
  - Reads `waitForSync`, `returnNew`, `returnOld`, `keepNull` from query
- **PUT**: `vertexActionReplace()` → `vertexModify()` → `documentModify()` - ✅ Matches
  - Same parameters as PATCH
- **DELETE**: `vertexActionRemove()` - ✅ Matches
  - Reads `waitForSync`, `returnOld` from query
  - Handles `If-Match` via `handleRevision()`

#### Status: ✅ **MATCH**

---

### 6. `/_api/gharial/{graph-name}/edge` (List/Add Edge Definitions)

#### OpenAPI Spec:
- **GET** (listEdgeCollections): List edge collections
  - Response codes: 200, 404
- **POST** (createEdgeDefinition): Add an edge definition
  - Request body: `collection`, `from`, `to` (required), `options`
  - Response codes: 201, 202, 400, 403, 404

#### C++ Implementation:
- **GET**: `graphActionReadConfig()` with TRI_COL_TYPE_EDGE - ✅ Matches
- **POST**: `createEdgeDefinition()` → `modifyEdgeDefinition()` with CREATE - ✅ Matches

#### Status: ✅ **MATCH**

---

### 7. `/_api/gharial/{graph-name}/edge/{definition-name}` (Create Edge/Modify Definition)

#### OpenAPI Spec:
- **POST** (createEdge): Create an edge
  - Query params: `waitForSync`, `returnNew` (optional)
  - Headers: `x-arango-trx-id` (optional)
  - Request body: `_from`, `_to` (required)
  - Response codes: 201, 202, 400, 403, 404, 410
- **PUT** (replaceEdgeDefinition): Replace an edge definition
  - Query params: `waitForSync`, `dropCollections` (optional)
  - Request body: `collection`, `from`, `to` (required), `options`
  - Response codes: 201, 202, 400, 403, 404
- **DELETE** (deleteEdgeDefinition): Remove an edge definition
  - Query params: `waitForSync`, `dropCollections` (optional)
  - Response codes: 201, 202, 403, 404

#### C++ Implementation:
- **POST**: `edgeActionCreate()` - ✅ Matches
  - Reads `waitForSync`, `returnNew` from query
- **PUT**: `editEdgeDefinition()` → `modifyEdgeDefinition()` with EDIT - ✅ Matches
  - Reads `waitForSync`, `dropCollections` from query
- **DELETE**: `removeEdgeDefinition()` → `modifyEdgeDefinition()` with REMOVE - ✅ Matches
  - Reads `waitForSync`, `dropCollections` from query

#### Status: ✅ **MATCH**

---

### 8. `/_api/gharial/{graph-name}/edge/{definition-name}/{edge-key}` (CRUD Edge)

#### OpenAPI Spec:
- **GET** (getEdge): Get an edge
  - Headers: `If-Match`, `If-None-Match`, `x-arango-trx-id` (optional)
  - Response codes: 200, 304, 403, 404, 410, 412
- **PATCH** (updateEdge): Update an edge
  - Query params: `waitForSync`, `keepNull`, `returnOld`, `returnNew` (optional)
  - Headers: `If-Match`, `x-arango-trx-id` (optional)
  - Request body: Present (partial edge document)
  - Response codes: 200, 202, 403, 404, 410, 412
- **PUT** (replaceEdge): Replace an edge
  - Query params: `waitForSync`, `keepNull`, `returnOld`, `returnNew` (optional)
  - Headers: `If-Match`, `x-arango-trx-id` (optional)
  - Request body: `_from`, `_to` (required)
  - Response codes: 201, 202, 403, 404, 410, 412
- **DELETE** (deleteEdge): Remove an edge
  - Query params: `waitForSync`, `returnOld` (optional)
  - Headers: `If-Match`, `x-arango-trx-id` (optional)
  - Response codes: 200, 202, 403, 404, 410, 412

#### C++ Implementation:
- **GET**: `edgeActionRead()` - ✅ Matches
  - Handles `If-None-Match` header
  - Calls `handleRevision()` for `If-Match`
- **PATCH**: `edgeActionUpdate()` → `edgeModify()` → `documentModify()` - ✅ Matches
  - Reads `waitForSync`, `returnNew`, `returnOld`, `keepNull` from query
- **PUT**: `edgeActionReplace()` → `edgeModify()` → `documentModify()` - ✅ Matches
  - Same parameters as PATCH
- **DELETE**: `edgeActionRemove()` - ✅ Matches
  - Reads `waitForSync`, `returnOld` from query
  - Handles `If-Match` via `handleRevision()`

#### Status: ✅ **MATCH**

---

## Summary

### ✅ What Matches Correctly

All 8 major endpoint groups match correctly between the OpenAPI spec and C++ implementation:

1. **Graph management** (list, create, read, delete)
2. **Vertex collections** (list, add)
3. **Vertex CRUD operations** (create, read, update, replace, delete)
4. **Edge definitions** (list, create, replace, delete)
5. **Edge CRUD operations** (create, read, update, replace, delete)

All HTTP methods are correctly implemented:
- GET operations for reading graphs, vertices, edges, and collections
- POST operations for creating graphs, vertices, edges, and definitions
- PUT operations for replacing vertices, edges, and edge definitions
- PATCH operations for updating vertices and edges
- DELETE operations for removing graphs, vertices, edges, and definitions

### ⚠️ Discrepancies Found

#### 1. **Additional Query Parameters in C++ Not Documented in OpenAPI**

The C++ implementation reads additional query parameters that are NOT documented in the OpenAPI spec:

| Endpoint | Method | C++ Parameter | OpenAPI Status |
|----------|--------|---------------|----------------|
| `/_api/gharial/{graph}` | DELETE | `waitForSync` | ❌ Not documented |
| `/_api/gharial/{graph}/vertex` | POST | `waitForSync` | ❌ Not documented |
| `/_api/gharial/{graph}/vertex` | POST | `createCollection` | ❌ Not documented |
| `/_api/gharial/{graph}/vertex/{collection}` | DELETE | `waitForSync` | ❌ Not documented |

**Impact**: Medium - Users relying solely on OpenAPI spec may not know these parameters exist.

#### 2. **`rev` Query Parameter**

The C++ implementation's `handleRevision()` method reads a `rev` query parameter in addition to the `If-Match` header for revision matching. This `rev` parameter is NOT documented in the OpenAPI spec for any endpoint.

**Impact**: Low - This appears to be a legacy or alternative parameter mechanism.

#### 3. **`keepNull` Default Value Difference**

The C++ code has a comment indicating:
```cpp
// Note: the default here differs from the one in the RestDocumentHandler
bool keepNull = _request->parsedValue(StaticStrings::KeepNullString, true);
```

The OpenAPI spec shows `keepNull` as optional but doesn't specify the default value explicitly. This should be verified if the default is `true` for gharial and `false` for regular documents.

**Impact**: Low - Likely intentional difference between APIs.

### ❌ What is Missing from the OpenAPI Spec

1. **`waitForSync` query parameter** for:
   - DELETE `/_api/gharial/{graph}`
   - POST `/_api/gharial/{graph}/vertex`
   - DELETE `/_api/gharial/{graph}/vertex/{collection}`

2. **`createCollection` query parameter** for:
   - POST `/_api/gharial/{graph}/vertex`

3. **`rev` query parameter** for revision matching (alternative to `If-Match` header) on:
   - All endpoints that support revision checks

### ✅ What is NOT Missing

No operations documented in OpenAPI are missing from the C++ implementation. All documented endpoints are fully implemented.

### 📝 Additional Notes

1. **Transaction IDs**: The OpenAPI spec documents `x-arango-trx-id` headers for several operations. While not explicitly handled in the shown C++ code, this is likely handled at a higher level in the request processing pipeline.

2. **Response Code Variations**: The OpenAPI spec shows response code 201 for PUT edge definition replacement, while code comments suggest this should be 202 (with a TODO to fix in a major release).

3. **Error Response Code**: There's a commented TODO in the C++ code about fixing the response code for graph removal in a major release (should be 201 when synchronous, currently returns 202).

4. **Database Name Parameter**: The OpenAPI paths include `{database-name}` parameter, but the C++ handler doesn't explicitly parse this - it's handled by the routing layer which sets the vocbase context.

### 🎯 Recommendations

1. **Update OpenAPI spec** to document:
   - `waitForSync` parameter for DELETE graph operation
   - `waitForSync` and `createCollection` for POST vertex collection
   - `waitForSync` for DELETE vertex collection
   - `rev` query parameter as an alternative to `If-Match` header

2. **Consider removing** undocumented parameters from C++ if they're not intentionally supported, or document them properly.

3. **Clarify default values** for optional parameters like `keepNull` in the OpenAPI spec.

4. **Address TODOs** in the C++ code regarding response codes for consistency.
