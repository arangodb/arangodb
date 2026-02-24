I now have all the context I need. Let me generate the section content.

# Section 3: REST Client

## Overview

This section refactors the existing monolithic `Toast.Client` module into a layered REST client architecture. The current client has all API operations as functions directly on the `Toast.Client` struct. The target design separates concerns into:

1. **Core HTTP module** (`Toast.Client`) -- struct with scoping functions, raw HTTP methods
2. **Domain modules** (`Toast.Client.Collection`, `Toast.Client.Document`, etc.) -- infrastructure helpers that use the client's configured API version
3. **`with_api_version/2`** -- runtime override for tests targeting a specific API version
4. **Server-specific client creation** via `Toast.Deployment.client/2`

The client lives entirely within `lib/toast/client/` (the infrastructure library). It has no ExUnit dependency and is usable from IEx, scripts, and test suites alike.

## Dependencies

- **Section 02 (Library Extraction)**: The `lib/toast/` directory structure and `Toast.Config` module must exist. The `Toast.Deployment` struct with its `controller` PID must be in place.
- **`joken` dependency**: Must be added to `mix.exs` for JWT token generation.
- **`Req` library**: Already a dependency (used by the existing client).

## File Structure

After this section, the following files exist:

```
lib/toast/
  client.ex                          # Core client struct + HTTP methods + scoping
  client/
    collection.ex                    # Collection operations
    document.ex                      # Document operations
    aql.ex                           # AQL operations
    admin.ex                         # Admin operations
    index.ex                         # Index operations
test/toast/
  client_test.exs                    # Core client tests
  client/
    collection_test.exs              # Collection domain module tests
    document_test.exs                # Document domain module tests
    aql_test.exs                     # AQL domain module tests
    admin_test.exs                   # Admin domain module tests
    index_test.exs                   # Index domain module tests
```

Additionally, `Toast.Deployment.client/2` is added to the existing `lib/toast/deployment.ex`.

---

## Tests

Write all tests before implementing. Tests mock HTTP calls via Req's test adapter (or via Mox against a behaviour) so they run without a live ArangoDB.

### Core Client Tests

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/client_test.exs`

This file replaces the existing client test file. The existing tests for `new/1` and `new/2` are preserved and extended.

```elixir
defmodule Toast.ClientTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  # --- Construction ---

  # Test: Client.new/2 creates client with base_url
  # Test: Client.new/2 preserves existing Req options behavior (retry: false, etc.)

  # --- Scoping functions ---

  # Test: Client.with_database/2 returns new client with database set
  #   - Original client is unchanged (immutability)
  #   - Database field on returned client matches argument

  # Test: Client.with_auth/2 returns new client with auth (basic)
  #   - auth field is {:basic, user, password}

  # Test: Client.with_auth/2 returns new client with auth (JWT via joken)
  #   - auth field is {:jwt, token_string}

  # Test: Client.with_api_version/2 with integer 1 sets api_version to 1
  # Test: Client.with_api_version/2 with string "experimental" sets api_version to "experimental"
  # Test: Client.with_api_version/2 with nil clears api_version (uses global default or no prefix)

  # --- URL Construction ---

  # Test: URL construction with no api_version, no database
  #   → path is just the API path (e.g., "/_api/collection")

  # Test: URL construction with integer api_version 1, no database
  #   → "/_arango/v1/_api/collection"

  # Test: URL construction with string api_version "experimental", no database
  #   → "/_arango/experimental/_api/collection"

  # Test: URL construction with api_version and database
  #   → "/_arango/v1/_db/mydb/_api/collection"
  #   (version prefix → database prefix → API path)

  # Test: URL construction with database but no api_version
  #   → "/_db/mydb/_api/collection"

  # Test: global default API version read from Toast.Config when client api_version is nil

  # --- HTTP Methods ---

  # Test: get/post/put/delete delegate to Req with correct URL and headers
  #   (use Req's plug-based test adapter or Mox)

  # Test: auth header set correctly for basic auth
  #   → Authorization: Basic base64(user:password)

  # Test: auth header set correctly for JWT
  #   → Authorization: Bearer <token>

  # --- Warnings ---

  # Test: incomplete API version warning when setting a version that may be incomplete
  #   (this is a log-level advisory, not an error)
end
```

### Unversioned Domain Module Tests

Each domain module gets its own test file. The tests verify that the module constructs the correct HTTP request (method, URL, body) and delegates to the core client's HTTP methods.

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/client/collection_test.exs`

```elixir
defmodule Toast.Client.CollectionTest do
  use ExUnit.Case, async: true

  # Test: Collection.create/3 sends POST /_api/collection with name and type in body
  # Test: Collection.create/3 with edge: true sets type 3
  # Test: Collection.drop/2 sends DELETE /_api/collection/{name}
  # Test: Collection.list/1 sends GET /_api/collection
  # Test: unversioned modules use client's configured api_version
  #   → if client has api_version: 1, URL is /_arango/v1/_api/collection
end
```

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/client/document_test.exs`

```elixir
defmodule Toast.Client.DocumentTest do
  use ExUnit.Case, async: true

  # Test: Document.insert/3 sends POST /_api/document/{collection}
  # Test: Document.get/3 sends GET /_api/document/{collection}/{key}
  # Test: Document.remove/3 sends DELETE /_api/document/{collection}/{key}
  # Test: unversioned modules use client's configured api_version
end
```

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/client/aql_test.exs`

```elixir
defmodule Toast.Client.AQLTest do
  use ExUnit.Case, async: true

  # Test: AQL.execute/2 sends POST /_api/cursor with query and bindVars
  # Test: AQL.execute/3 with bind_vars passes them correctly
  # Test: cursor pagination (hasMore: true) follows up with PUT /_api/cursor/{id}
end
```

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/client/admin_test.exs`

```elixir
defmodule Toast.Client.AdminTest do
  use ExUnit.Case, async: true

  # Test: Admin.version/1 sends GET /_api/version
  # Test: Admin.status/1 sends GET /_admin/status (if needed)
end
```

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/client/index_test.exs`

```elixir
defmodule Toast.Client.IndexTest do
  use ExUnit.Case, async: true

  # Test: Index.create/3 sends POST /_api/index with collection query param and body
  # Test: Index.list/2 sends GET /_api/index?collection={name}
  # Test: Index.drop/2 sends DELETE /_api/index/{handle}
end
```

### Deployment.client/2 Tests

These tests belong in the deployment test file, but are listed here for completeness.

```elixir
# In the appropriate deployment test file:

# Test: Deployment.client/2 returns client for specific server by toast ID
#   → base_url matches that server's endpoint
# Test: Deployment.client/2 returns client for server by role/index
#   → e.g., client(deployment, role: :dbserver, index: 0)
# Test: Deployment.client/2 returns client for server by cluster_id
#   → e.g., client(deployment, cluster_id: "PRMR-xxx")
# Test: Deployment.client/2 returns error for unknown server
```

---

## Implementation Details

### 1. New `joken` Dependency

Add `joken` to `mix.exs` dependencies for JWT token generation:

```elixir
{:joken, "~> 2.6"}
```

This is the only new dependency this section introduces.

### 2. Core Client Module

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/client.ex`

Replace the existing monolithic client with the new layered design.

#### Struct

```elixir
defmodule Toast.Client do
  @moduledoc "Thin REST client for ArangoDB, designed for test use."

  @type t :: %__MODULE__{
    base_url: String.t(),
    database: String.t() | nil,
    api_version: non_neg_integer() | String.t() | nil,
    auth: auth_t() | nil,
    req: Req.Request.t()
  }

  @type auth_t :: {:basic, String.t(), String.t()} | {:jwt, String.t()}

  defstruct [:base_url, :database, :api_version, :auth, :req]

  # ...
end
```

Key changes from the existing struct:
- The existing struct has only `req`. The new struct adds `base_url`, `database`, `api_version`, and `auth` as explicit fields. The `req` field is still kept as the underlying Req.Request, but `base_url` is stored separately so scoping functions can rebuild URLs without parsing.
- The `database` field replaces the current approach of baking `/_db/{name}` into `base_url` during construction. This enables `with_database/2` to produce a new client cleanly.

#### Constructor

```elixir
def new(base_url, opts \\ [])
```

Creates a client. The `base_url` is stored as-is (no database prefix). Options:
- `:database` -- set the database (stored in struct field, not baked into URL)
- `:api_version` -- set the API version
- `:auth` -- set authentication
- All other opts forwarded to `Req.new/1`

The `Req.Request` is created with `base_url` set to the raw endpoint. URL construction (version prefix, database prefix) happens at request time, not at construction time. This is the key design difference from the current implementation.

#### Scoping Functions

Each returns a **new** client struct with one field changed. The original is never mutated.

```elixir
def with_database(client, database)
def with_auth(client, auth)
def with_api_version(client, version)
```

`with_auth/2` accepts either `{:basic, user, password}` or `{:jwt, token}`. For JWT, the caller generates the token externally (using `joken`) and passes it in. The client does not handle token generation itself -- that would complect HTTP concerns with auth token lifecycle.

`with_api_version/2` accepts:
- An integer (e.g., `1`) -- produces `/_arango/v1` prefix
- A string (e.g., `"experimental"`) -- produces `/_arango/experimental` prefix
- `nil` -- clears the per-client override, falling back to the global default from `Toast.Config`

#### URL Construction

A private function builds the full URL path from the client's state:

```
[version_prefix] [database_prefix] [api_path]
```

Concrete examples:
- No version, no database: `/_api/document/coll/key`
- Version 1, no database: `/_arango/v1/_api/document/coll/key`
- Version 1, database "mydb": `/_arango/v1/_db/mydb/_api/document/coll/key`
- No version, database "mydb": `/_db/mydb/_api/document/coll/key`

The version prefix is determined by:
1. The client's `api_version` field (if set via `with_api_version/2`)
2. The global default from `Toast.Config` (if client field is nil)
3. No prefix (if both are nil)

This is a single code path used by all HTTP methods and all domain modules.

#### HTTP Methods

```elixir
def get(client, path, opts \\ [])
def post(client, path, body, opts \\ [])
def put(client, path, body, opts \\ [])
def delete(client, path, opts \\ [])
```

Each method:
1. Builds the full URL via the URL construction function
2. Sets auth headers if `client.auth` is non-nil
3. Delegates to `Req.get/2`, `Req.post/2`, etc.
4. Returns `{:ok, response}` or `{:error, reason}` -- the raw Req response, not a parsed domain object

The domain modules are responsible for interpreting responses. The core client just does HTTP.

**Auth header construction**:
- `{:basic, user, password}` → `Authorization: Basic #{Base.encode64("#{user}:#{password}")}`
- `{:jwt, token}` → `Authorization: Bearer #{token}`

#### Global Default API Version

The global default is read from `Toast.Config`. This requires `Toast.Config` to gain an `api_version` field (added in section 02 or here if not yet present):

```elixir
# In Toast.Config
api_version: nil  # default: no version prefix
```

Configurable via:
- `TOAST_API_VERSION` environment variable
- `.toast.local.exs` config file
- Keyword opts to `Toast.Config.load/1`

When `TOAST_API_VERSION` is set to e.g. `"1"`, it is parsed as an integer. When set to `"experimental"`, it stays as a string. The parsing logic: try `Integer.parse/1`; if it succeeds, use the integer; otherwise, use the string as-is.

### 3. Domain Modules

These modules provide ergonomic wrappers around common ArangoDB REST operations. They use the client's configured API version (which may be the global default). Tests that need to target a specific API version use `with_api_version/2` to override before calling domain module functions.

Each module takes `%Toast.Client{}` as the first argument and returns tagged tuples.

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/client/collection.ex`

```elixir
defmodule Toast.Client.Collection do
  @moduledoc "Collection management operations."

  alias Toast.Client

  @doc "Create a collection. Options: :edge (boolean), :type (integer)."
  @spec create(Client.t(), String.t(), keyword()) :: {:ok, map()} | {:error, term()}
  def create(client, name, opts \\ [])

  @doc "Drop a collection by name. Idempotent (404 returns :ok)."
  @spec drop(Client.t(), String.t()) :: :ok | {:error, term()}
  def drop(client, name)

  @doc "List collections. Options: :exclude_system (boolean)."
  @spec list(Client.t(), keyword()) :: {:ok, [map()]} | {:error, term()}
  def list(client, opts \\ [])
end
```

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/client/document.ex`

```elixir
defmodule Toast.Client.Document do
  @moduledoc "Document CRUD operations."

  alias Toast.Client

  @spec insert(Client.t(), String.t(), map()) :: {:ok, map()} | {:error, term()}
  def insert(client, collection, doc)

  @spec get(Client.t(), String.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def get(client, collection, key)

  @spec remove(Client.t(), String.t(), String.t()) :: :ok | {:error, term()}
  def remove(client, collection, key)
end
```

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/client/aql.ex`

```elixir
defmodule Toast.Client.AQL do
  @moduledoc "AQL query execution."

  alias Toast.Client

  @doc "Execute an AQL query. Handles cursor pagination automatically."
  @spec execute(Client.t(), String.t(), map()) :: {:ok, [term()]} | {:error, term()}
  def execute(client, query, bind_vars \\ %{})
end
```

The cursor pagination logic (currently in `Toast.Client.collect_cursor_results/2` and `collect_cursor_pages/3`) moves here. This is the only domain module with non-trivial internal logic.

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/client/admin.ex`

```elixir
defmodule Toast.Client.Admin do
  @moduledoc "Administrative and status endpoints."

  alias Toast.Client

  @spec version(Client.t()) :: {:ok, map()} | {:error, term()}
  def version(client)
end
```

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/client/index.ex`

```elixir
defmodule Toast.Client.Index do
  @moduledoc "Index management operations."

  alias Toast.Client

  @spec create(Client.t(), String.t(), map()) :: {:ok, map()} | {:error, term()}
  def create(client, collection, definition)

  @spec list(Client.t(), String.t()) :: {:ok, [map()]} | {:error, term()}
  def list(client, collection)

  @spec drop(Client.t(), String.t()) :: :ok | {:error, term()}
  def drop(client, handle)
end
```

All domain modules delegate to `Toast.Client.get/3`, `Toast.Client.post/4`, etc. They do NOT call `Req` directly. This ensures URL construction (version prefix, database prefix) always goes through the single code path in the core client.

**Version pinning for tests**: When a test needs to target a specific API version, it uses `with_api_version/2` before calling domain module functions:

```elixir
client
|> Client.with_api_version(1)
|> Client.Collection.create("test_coll")
```

This keeps all URL construction in a single code path while making the tested version explicit at the call site. Versioned domain modules (e.g., `Toast.Client.V1.Collection`) are deferred until a real second API version exists with incompatible function signatures — today there is exactly one implementation per domain.

### 4. Deployment.client/2

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment.ex` (addition to existing file)

Add a `client/2` function that creates a `Toast.Client` pointed at a specific server within the deployment.

```elixir
@doc """
Create a client for a specific server in the deployment.

Accepts:
  - A toast server ID string: `"dbserver-1"`
  - Role-based targeting: `role: :dbserver, index: 0`
  - Cluster-internal ID: `cluster_id: "PRMR-abc123"`
"""
@spec client(t(), String.t() | keyword()) :: {:ok, Client.t()} | {:error, term()}
def client(deployment, target)
```

The function:
1. Resolves the target to a server (queries the controller for the server's endpoint)
2. Creates a `Toast.Client.new/2` with that server's endpoint as `base_url`
3. Returns `{:ok, client}` or `{:error, :unknown_server}`

This function lives on `Toast.Deployment` (not `Toast.Client`) because the dependency direction is `Deployment → Client`, not the reverse. The client has no knowledge of deployments, servers, or controllers.

### 5. Toast.Config Changes

Add the `api_version` field to the `Toast.Config` struct:

```elixir
# In the defstruct:
api_version: nil

# In the type spec:
api_version: non_neg_integer() | String.t() | nil
```

Add reading from `TOAST_API_VERSION` in `Toast.Config.load/1`:

```elixir
api_version: opt_or(opts, :api_version, read_api_version())
```

Where `read_api_version/0` reads `TOAST_API_VERSION` and parses it (integer if numeric, string otherwise, nil if unset).

### 6. Migration of Existing Client Callers

The existing `Toast.Client` functions (`version/1`, `aql/2`, `create_collection/3`, etc.) are replaced by the domain modules. Existing callers need to be updated:

| Old call | New call |
|----------|----------|
| `Toast.Client.version(client)` | `Toast.Client.Admin.version(client)` |
| `Toast.Client.aql(client, query)` | `Toast.Client.AQL.execute(client, query)` |
| `Toast.Client.aql!(client, query)` | `Toast.Client.AQL.execute!(client, query)` |
| `Toast.Client.create_collection(client, name)` | `Toast.Client.Collection.create(client, name)` |
| `Toast.Client.drop_collection(client, name)` | `Toast.Client.Collection.drop(client, name)` |
| `Toast.Client.list_collections(client)` | `Toast.Client.Collection.list(client)` |
| `Toast.Client.insert_document(client, coll, doc)` | `Toast.Client.Document.insert(client, coll, doc)` |
| `Toast.Client.get_document(client, coll, key)` | `Toast.Client.Document.get(client, coll, key)` |
| `Toast.Client.remove_document(client, coll, key)` | `Toast.Client.Document.remove(client, coll, key)` |
| `Toast.Client.new(url, database: "foo")` | `Toast.Client.new(url) \|> Toast.Client.with_database("foo")` |

The `client.req` field should not be accessed directly by callers anymore. All HTTP goes through the core client's `get/post/put/delete` functions.

During migration, consider adding deprecated wrapper functions on `Toast.Client` that delegate to the domain modules and emit compiler warnings. This eases the transition but is not strictly necessary if all callers are updated at once (they are all within the Toast codebase).

---

## Design Decisions

**Why scoping functions return new structs**: Immutability prevents accidental mutation. A test that calls `with_database(client, "test_db")` gets a new client; the original is unchanged. This makes it safe to share a base client across tests and derive specialized clients from it.

**Why URL construction happens at request time, not construction time**: The current implementation bakes the database into `base_url` during `new/2`. This makes `with_database/2` impossible without reconstructing the Req.Request. By deferring URL construction to request time, all scoping functions are simple struct updates.

**Why domain modules delegate to core client HTTP methods**: A single code path for URL construction prevents bugs where a domain module forgets the version prefix or database prefix. The core client owns URL construction; domain modules only know about API paths.

**Why versioned domain modules are deferred**: Today there is exactly one client module per domain. Pre-building `V1.Collection`, `V1.Document`, etc. is YAGNI — tests use `with_api_version/2` to pin the version at the call site. Versioned modules will be added when a second API version exists with incompatible function signatures.

**Why `Deployment.client/2` lives on Deployment, not Client**: The client is a pure HTTP concern. It should not know about deployments, controllers, or server topologies. `Deployment.client/2` is a convenience that bridges the deployment and client domains.

---

## Verification

After implementation:

1. `mix test test/toast/client_test.exs` -- all core client tests pass
2. `mix test test/toast/client/` -- all domain module tests pass
3. `mix compile --warnings-as-errors` -- no warnings from old-style client calls
4. If a deployment is available: manually verify `Toast.Client.new("http://localhost:8529") |> Toast.Client.Admin.version()` returns version info
5. Verify `Toast.Client.with_api_version(client, 1) |> Toast.Client.Collection.list()` produces requests with `/_arango/v1/` prefix