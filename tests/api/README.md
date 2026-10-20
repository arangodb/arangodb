# ArangoDB API Authorization Test Runner

## Overview

This directory contains `apitester.js`, a Node.js-based test runner for
systematically probing the authorization behaviour of ArangoDB HTTP APIs.
The primary goal is **not** to check functional correctness but to document and
compare what HTTP status codes different ArangoDB versions return for every
combination of user permissions.  By capturing the output of a test run as a
plain-text file you can diff two runs against each other and instantly spot any
change in authorization semantics across versions.

---

## Directory Layout

```
tests/api/
├── apitester.js      # the test runner
├── apitests/         # *.mjs test files (one or more, sorted alphabetically)
├── package.json
└── package-lock.json
```

Place your test files (`.mjs`) inside `apitests/`.  The runner imports them in
alphabetical order.

---

## Prerequisites

- **Node.js ≥ 18** (ESM `import()` and `fetch`-era APIs are required)
- Dependencies are managed with npm:

  ```bash
  npm install          # inside tests/api/
  ```

- A running ArangoDB instance with a known **JWT secret** (needed for the
  `test` subcommand so the runner can authenticate as a superuser when
  executing setup/teardown hooks).

---

## The User Matrix

### Collection-level users (64 users)

To cover every meaningful combination of collection access permissions the
runner creates **64 users** named with a three-letter code `<DB><WC><COLL>`.
Each position is one of four access levels:

| Letter | Level     | Meaning                                      |
|--------|-----------|----------------------------------------------|
| `U`    | undefined | No explicit grant (inherits default)         |
| `N`    | none      | Explicitly denied (`none`)                   |
| `R`    | ro        | Read-only (`ro`)                             |
| `W`    | rw        | Read-write (`rw`)                            |

The three positions in the username control three different permission layers on
database `d`:

| Position | Controls                                                |
|----------|---------------------------------------------------------|
| `DB`     | Access to database `d` itself                          |
| `WC`     | Wildcard collection access (`d/*`)                      |
| `COLL`   | Specific access to collection `c` inside database `d`  |

So for example user **`WRN`** has:
- `rw` access to database `d`
- `ro` wildcard access to all collections in `d`
- `none` explicit access to collection `c`

All 4 × 4 × 4 = **64** combinations are created.  Each user's password equals
their username.

### Admin users (4 users)

Four additional users (`AU`, `AN`, `AR`, `AW`) are created to cover
administrative access to the `_system` database:

| Username | `_system` DB permission |
|----------|-------------------------|
| `AU`     | undefined (no grant)    |
| `AN`     | `none`                  |
| `AR`     | `ro`                    |
| `AW`     | `rw`                    |

These users have no permissions on database `d` and are used specifically for
tests that probe admin-only or `_system`-level endpoints.

---

## Subcommands

### `setup`

Creates database `d`, collection `c` inside it, all 64 permission-matrix users,
and the 4 admin users.

```bash
node apitester.js \
  --endpoint http://localhost:8529 \
  --root-password <password> \
  setup
```

`--root-password` defaults to an empty string if omitted.

### `teardown`

Drops database `d` and deletes all 68 test users.

```bash
node apitester.js \
  --endpoint http://localhost:8529 \
  --root-password <password> \
  teardown
```

### `test`

Runs one or more `*.mjs` test files, then prints a formatted result table to
**stdout**.  Each positional argument after `test` may be:

- a **directory** — all `*.mjs` files inside are collected and sorted
  alphabetically before running, or
- an **individual `*.mjs` file** — used exactly as given.

Multiple paths are accepted and processed in the order supplied.

```bash
# Run every file in a directory (original behaviour)
node apitester.js \
  --endpoint http://localhost:8529 \
  --jwt-secret /path/to/jwt.secret \
  test apitests/

# Run a single file
node apitester.js \
  --endpoint http://localhost:8529 \
  --jwt-secret /path/to/jwt.secret \
  test apitests/open.mjs

# Run several specific files
node apitester.js \
  --endpoint http://localhost:8529 \
  --jwt-secret /path/to/jwt.secret \
  test apitests/open.mjs apitests/documents.mjs

# Mix a specific file with a whole directory
node apitester.js \
  --endpoint http://localhost:8529 \
  --jwt-secret /path/to/jwt.secret \
  test apitests/open.mjs apitests/
```

`--jwt-secret` is **required** for the `test` subcommand.  The file must
contain the plain-text JWT secret used to start the ArangoDB instance (the
content of `--server.jwt-secret` or the cluster JWT file).  The runner uses it
to mint a short-lived HS256 superuser token, which it then uses for
setup/teardown hooks.

#### Full option reference

```
Options:
  --endpoint,       -e <url>   ArangoDB endpoint (default: http://localhost:8529)
  --root-password,  -p <pw>    root user password (default: empty)
  --jwt-secret,     -j <file>  path to JWT secret file (required for 'test')
  --help,           -h         show help
```

---

## Typical Workflow

```bash
# 1. Install dependencies (once)
npm install

# 2. Create test users / database / collection
node apitester.js -e http://localhost:8529 -p secret setup

# 3. Run all tests, capture output
node apitester.js -e http://localhost:8529 -j /var/lib/arangodb3/jwt.secret \
  test apitests/ > results-3.12.txt

# 3a. (Optional) Run only specific files during development
node apitester.js -e http://localhost:8529 -j /var/lib/arangodb3/jwt.secret \
  test apitests/open.mjs apitests/documents.mjs

# 4. Clean up
node apitester.js -e http://localhost:8529 -p secret teardown

# 5. Compare two versions
diff results-3.11.txt results-3.12.txt
```

---

## Writing Test Files

Each file in `apitests/` must be a valid ES-module (`.mjs`) that **default-
exports an array** of test-entry objects.  Files are imported (not `require`d)
by the runner and processed in alphabetical filename order.

### Minimal example

```js
// apitests/010-collection-read.mjs

export default [
  {
    name:   'Read document count of collection c',
    type:   'collection',
    method: 'GET',
    path:   '/_db/d/_api/collection/c/count',
  },
];
```

### Test entry fields

| Field      | Type               | Required | Description |
|------------|--------------------|----------|-------------|
| `name`     | `string`           | yes      | Human-readable label printed in the output |
| `type`     | `string`           | yes      | `"collection"`, `"database"`, or `"admin"` |
| `method`   | `string`           | yes      | HTTP verb: `GET`, `POST`, `PUT`, `PATCH`, `DELETE`, `HEAD` |
| `path`     | `string`           | yes      | Full URL path, e.g. `/_db/d/_api/collection/c` |
| `body`     | `object \| null`   | no       | JSON body sent with the request. String values anywhere inside the object are subject to `${…}` interpolation (see below). |
| `headers`  | `object`           | no       | Extra HTTP headers. String values are subject to `${…}` interpolation (see below). If the object contains an `Authorization` key it **overrides** the runner's default authorization header for that request (useful for endpoints that require a specific auth scheme such as JWT). |}
| `setup`    | `async (ctx) => …` | no       | Hook executed **before** each matrix cell; runs as superuser. May return an object that is bound to `ctx.data` for the subsequent `teardown` call. |
| `teardown` | `async (ctx) => …` | no       | Hook executed **after** each matrix cell; runs as superuser. Receives whatever `setup` returned via `ctx.data`. |

### Test types

#### `"collection"` — full 64-user matrix

The runner iterates all 64 users and prints a 16 × 4 table:

- **Rows** — all 16 combinations of collection-level (`COLL`) and wildcard (`WC`) grants
- **Columns** — the 4 database-level (`DB`) grants

Each cell contains the HTTP status code returned for that permission
combination.

#### `"database"` — database-level row

A single row with 4 columns (one per `DB` level).  The runner uses users
`UUU`, `NUU`, `RUU`, `WUU` (wildcard and collection permissions left
undefined) to isolate the effect of the database-level grant alone.

#### `"admin"` — admin / `_system` columns

Five columns: `AU`, `AN`, `AR`, `AW` (the four admin users), plus a final
**superuser** column (JWT authentication, no user-level restrictions).

### The `ctx` object (setup / teardown)

Both `setup` and `teardown` receive a single `ctx` argument with the following
interface:

```ts
interface Ctx {
  /** Make an HTTP request authenticated as the superuser. */
  request(
    method:       string,
    path:         string,
    body?:        object | null,
    extraHeaders?: object,
  ): Promise<{ status: number; body: any }>;

  /** The configured endpoint URL, e.g. "http://localhost:8529". */
  endpoint: string;

  /**
   * Set by the runner to the return value of the most recent setup call.
   * Undefined before setup runs or after teardown completes.
   * Use this in teardown to access resources created during setup,
   * and in interpolated strings inside path, body, and headers.
   */
  data: any;
}
```

`ctx.request` always uses the superuser JWT token, so it can perform any
administrative operation regardless of the user currently under test.

`ctx.resolveString(s)` and `ctx.resolveDeep(value)` expose the same
interpolation engine that the runner uses for `path`, `body`, and `headers`.
They are useful inside `teardown` (or `setup`) when you need to expand a
`${…}` expression yourself — for example to build a URL from `ctx.data`
without relying on automatic interpolation:

```js
teardown: async (ctx) => {
  const url = ctx.resolveString('/_db/d/_api/document/${ctx.data.id}');
  await ctx.request('DELETE', url);
},
```

If `setup` returns a value, the runner assigns it to `ctx.data` before
resolving `path`/`body`/`headers` and before invoking `teardown`.  After
`teardown` completes, `ctx.data` is reset to `undefined` so it does not
bleed into the next matrix cell.

If the object returned by `setup` has a truthy `skipTest` property, the
test request **and** teardown are both skipped for that matrix cell.  The
output table prints `SKIP` in that cell instead of a status code.  This is
useful when a precondition cannot be met for a specific user (e.g. a
required resource does not exist in that permission context):

```js
setup: async (ctx) => {
  const r = await ctx.request('GET', '/_api/version');
  if (r.status !== 200) {
    return { skipTest: true };   // skip this cell entirely
  }
  return { token: r.body.version };
},
```

A failure (thrown error) inside `setup` or `teardown` is treated as **fatal**:
the runner prints the error and exits immediately.

### Runtime string interpolation in `path`, `body`, and `headers`

Any string value in `path`, anywhere inside the `body` object (recursively),
or in the `headers` map that contains the substring `${` is evaluated as a
JavaScript template literal at runtime — **after** `setup` has run and
`ctx.data` has been set.  Strings that do not contain `${` are passed through
unchanged with zero overhead.

The interpolation is performed by compiling the string with `new Function`:

```
new Function('ctx', `return \`${yourString}\``)(ctx)
```

This makes the full `ctx` object (including `ctx.data` and its sub-attributes)
available inside the expression.

**Rules:**
- Write the string with ordinary double or single quotes in the `.mjs` file —
  no backticks are needed or wanted.
- Use `${ctx.data.<attr>}` to reference data produced by `setup`.
- Non-string values in `body` (numbers, booleans, nested objects, arrays) are
  walked recursively; only string leaves are interpolated.
- Object **keys** are never interpolated — only values.

**Example — path interpolation:**

```js
setup: async (ctx) => {
  const r = await ctx.request('POST', '/_db/d/_api/document/c', { value: 1 });
  return { id: r.body._id };   // e.g. "c/12345"
},

// path is resolved after setup; ctx.data.id == "c/12345"
path: "/_db/d/_api/document/${ctx.data.id}",
```

**Example — body interpolation:**

```js
setup: async (ctx) => {
  return { key: 'generatedKey42' };
},

method: 'POST',
path:   '/_db/d/_api/document/c',
body:   { _key: "${ctx.data.key}", value: 0 },
```

**Example — header interpolation:**

```js
setup: async (ctx) => {
  return { token: 'abc123' };
},

headers: { 'x-custom-token': "${ctx.data.token}" },
```

### Complete example with setup and teardown

When the document key is known in advance a fixed `_key` is the simplest
approach:

```js
// apitests/020-document-insert.mjs

export default [
  {
    name:   'Insert document into collection c',
    type:   'collection',
    method: 'POST',
    path:   '/_db/d/_api/document/c',
    body:   { _key: 'testdoc', value: 42 },

    // Ensure the document does not exist before each attempt.
    setup: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
      // Ignore 404 – the document may not exist yet.
    },

    // Remove the document after each attempt so the next user starts clean.
    teardown: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
    },
  },
];
```

### Using `ctx.data` to pass setup results to teardown and into the request

When the resource identifier is not known ahead of time — for example when
ArangoDB generates the `_id` — `setup` returns an object.  The runner binds
the return value to `ctx.data`, which is then available both in `teardown`
and in any `${…}` expressions inside `path`, `body`, and `headers`:

```js
// apitests/025-document-server-key.mjs

export default [
  {
    name:   'Read document with server-generated key',
    type:   'collection',
    method: 'GET',

    // ctx.data.id is set by setup; the path is interpolated at runtime.
    path:   "/_db/d/_api/document/${ctx.data.id}",

    // Insert a document before each attempt and remember its _id.
    setup: async (ctx) => {
      const r = await ctx.request('POST', '/_db/d/_api/document/c', { value: 99 });
      return { id: r.body._id };   // e.g. "c/98765"
    },

    // ctx.data.id is still available here.
    teardown: async (ctx) => {
      await ctx.request('DELETE', `/_db/d/_api/document/${ctx.data.id}`);
    },
  },
];
```

### Multiple tests in one file

A single file may export multiple test entries.  They are appended to the
appropriate type bucket (collection / database / admin) in the order they
appear, and are executed after all entries of the same type from earlier files.

```js
// apitests/030-collection-ops.mjs

export default [
  {
    name:   'Get collection properties',
    type:   'collection',
    method: 'GET',
    path:   '/_db/d/_api/collection/c/properties',
  },
  {
    name:   'Truncate collection c (admin)',
    type:   'admin',
    method: 'PUT',
    path:   '/_db/d/_api/collection/c/truncate',
  },
];
```

---

## Output Format

All output goes to **stdout** so it can be captured with shell redirection.
The output is plain text; no colours or terminal escapes are used, making it
safe to store as a file and diff later.

### Collection-level test block

```
GET /_db/d/_api/collection/c/count
                      | DB undef | DB none  | DB ro    | DB rw    |
----------------------|----------|----------|----------|----------|
COLL undef, * undef   |   401    |   401    |   200    |   200    |
COLL undef, * none    |   401    |   401    |   403    |   403    |
COLL undef, * ro      |   401    |   401    |   200    |   200    |
COLL undef, * rw      |   401    |   401    |   200    |   200    |
COLL none,  * undef   |   401    |   401    |   403    |   403    |
...
```

Rows are ordered: for each collection level (`U`, `N`, `R`, `W`) the four
wildcard levels (`U`, `N`, `R`, `W`) are enumerated — giving 16 rows.
Columns are the four database levels (`U`, `N`, `R`, `W`).

### Database-level test block

```
GET /_db/d/_api/database/current
| DB undef | DB none  | DB ro    | DB rw    |
|----------|----------|----------|----------|
|   401    |   403    |   200    |   200    |
```

### Admin test block

```
GET /_api/cluster/health
| _sys undef | _sys none  | _sys ro    | _sys rw    | superuser  |
|------------|------------|------------|------------|------------|
|    403     |    403     |    403     |    200     |    200     |
```

### Section headers

The three test types are separated by section headers:

```
=== Collection-level tests ===

=== Database-level tests ===

=== Admin tests ===
```

### Comparing versions

```bash
diff results-3.11.txt results-3.12.txt
```

Any line that changes indicates a permission-behaviour difference between the
two ArangoDB versions.  Because the output is deterministic (tests run in the
same alphabetical file order, same user order, same row/column order) the diff
is minimal and easy to read.

---

## Naming Conventions for Test Files

Use a numeric prefix so files are processed in a predictable order:

```
apitests/
├── 010-documents.mjs
├── 020-collections.mjs
├── 030-databases.mjs
├── 040-graphs.mjs
└── 050-admin.mjs
```

Group related endpoints in the same file and choose names that make the diff
output self-explanatory.

---

## Tips

- **Idempotent setup/teardown**: always write `setup`/`teardown` hooks so that
  they succeed even when the resource already exists (or does not exist).  Use
  `ctx.request` return values to check the status rather than assuming
  a particular state.
- **Cleanup in teardown**: an API call which does something should be undone
  in the `teardown` routine.
- **Minimal body**: provide only the fields required for the request to be
  syntactically valid.  Avoid side effects that could influence unrelated test
  rows.
- **Stable `_key` values**: when inserting documents in `setup`, use a fixed
  `_key` so `teardown` can reliably delete them.
- **TLS**: the runner ignores TLS certificate errors (`rejectUnauthorized:
  false`), so self-signed certificates are fine.
- **Large matrices are slow**: a single `"collection"` test makes 64 HTTP
  requests (plus up to 128 setup/teardown calls).  Keep setup/teardown hooks
  lightweight.
