#!/usr/bin/env node
/**
 * apitester.js - ArangoDB API authorization test runner
 *
 * Usage:
 *   node apitester.js [--endpoint <url>] [--root-password <pw>] [--jwt-secret <file>] setup
 *   node apitester.js [--endpoint <url>] [--root-password <pw>] [--jwt-secret <file>] teardown
 *   node apitester.js [--endpoint <url>] --jwt-secret <file> test <directory>
 *
 * The 'setup' subcommand creates database 'd', collection 'c', 64 test users
 * (covering all permission combinations) and 4 admin test users (AU/AN/AR/AW).
 *
 * The 'teardown' subcommand drops database 'd' and deletes all test users.
 *
 * The 'test' subcommand loads all *.mjs files from <directory> (sorted
 * alphabetically), each of which must export a default array of test entries:
 *
 *   export default [ { name, type, method, path, body?, headers?,
 *                       setup?, teardown? }, ... ];
 *
 *   type:     "collection" | "database" | "admin"
 *   setup:    async (ctx) => { ... }   – runs before the test, superuser auth
 *   teardown: async (ctx) => { ... }   – runs after  the test, superuser auth
 *
 * ctx.request(method, path, body?, extraHeaders?)
 *   Makes an HTTP call with superuser JWT, returns { status, body }.
 *
 * ctx.resolveString(s)
 *   Interpolates a single string as a template literal with ctx in scope.
 *   Strings that do not contain '${' are returned unchanged.
 *
 * ctx.resolveDeep(value)
 *   Recursively walks value and interpolates every string that contains '${'.
 *   Non-string leaves (numbers, booleans, null, …) are returned as-is.
 *
 * A setup function may return an object; the runner binds it to ctx.data so
 * the corresponding teardown can access whatever setup created.
 *
 * String interpolation in path, body, and headers:
 *   Any string value in path, body (recursively), or headers that contains
 *   the substring '${' is evaluated as a JavaScript template literal with
 *   ctx in scope after setup has run.  This lets test entries reference
 *   ctx.data without using backtick template literals in the *.mjs file:
 *
 *     path: "/_db/d/_api/document/${ctx.data.id}"
 *     body: { _key: "${ctx.data.key}", value: 1 }
 *     headers: { "x-my-header": "${ctx.data.token}" }
 *
 *   Strings that do not contain '${' are passed through unchanged.
 *
 * Failure (throw) inside setup or teardown is fatal and stops the run.
 */

import { readFileSync, readdirSync } from 'fs';
import { resolve, join } from 'path';
import { pathToFileURL } from 'url';
import { createHmac } from 'crypto';
import { request as undiciRequest, Agent } from 'undici';

// ─── CLI parsing ──────────────────────────────────────────────────────────────

function parseArgs() {
  const args = process.argv.slice(2);
  const opts = {
    endpoint: 'http://localhost:8529',
    jwtSecretFile: null,
    rootPassword: '',
    subcommand: null,   // 'setup' | 'teardown' | 'test'
    directory: null,    // only used for 'test'
  };

  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if ((a === '--endpoint' || a === '-e') && i + 1 < args.length) {
      opts.endpoint = args[++i];
    } else if ((a === '--jwt-secret' || a === '-j') && i + 1 < args.length) {
      opts.jwtSecretFile = args[++i];
    } else if ((a === '--root-password' || a === '-p') && i + 1 < args.length) {
      opts.rootPassword = args[++i];
    } else if (a === '--help' || a === '-h') {
      printHelp();
      process.exit(0);
    } else if (!a.startsWith('-')) {
      if (opts.subcommand === null) {
        if (a === 'setup' || a === 'teardown' || a === 'test') {
          opts.subcommand = a;
        } else {
          console.error(`Unknown subcommand: ${a}`);
          printHelp();
          process.exit(1);
        }
      } else if (opts.subcommand === 'test' && opts.directory === null) {
        opts.directory = a;
      } else {
        console.error(`Unexpected argument: ${a}`);
        printHelp();
        process.exit(1);
      }
    } else {
      console.error(`Unknown argument: ${a}`);
      printHelp();
      process.exit(1);
    }
  }

  if (!opts.subcommand) {
    console.error('Error: subcommand required (setup, teardown, or test)');
    printHelp();
    process.exit(1);
  }

  if (opts.subcommand === 'test') {
    if (!opts.jwtSecretFile) {
      console.error('Error: --jwt-secret <file> is required for the test subcommand');
      printHelp();
      process.exit(1);
    }
    if (!opts.directory) {
      console.error('Error: <directory> argument is required for the test subcommand');
      printHelp();
      process.exit(1);
    }
  }

  return opts;
}

function printHelp() {
  console.error(`
Usage:
  node apitester.js [options] setup
  node apitester.js [options] teardown
  node apitester.js [options] --jwt-secret <file> test <directory>

Subcommands:
  setup       Create database 'd', collection 'c', 64 permission-matrix users,
              and 4 admin users (AU/AN/AR/AW).
  teardown    Drop database 'd' and delete all test users.
  test        Run API authorization tests from *.mjs files in <directory>.

Options:
  --endpoint,       -e <url>   ArangoDB endpoint URL (default: http://localhost:8529)
  --root-password,  -p <pw>    Password for the root user (default: empty string)
  --jwt-secret,     -j <file>  Path to JWT secret file (required for 'test')
  --help,           -h         Show this help

For the 'test' subcommand each *.mjs file in <directory> must have a default
export that is an array of test objects with the following fields:

  name      string   – human-readable test name
  type      string   – "collection", "database", or "admin"
  method    string   – HTTP method (GET, POST, PUT, DELETE, PATCH, HEAD)
  path      string   – URL path (e.g. "/_db/d/_api/collection/c")
  body      object?  – optional JSON request body
  headers   object?  – optional extra HTTP headers for the test request
  setup     async function(ctx)?    – called before each matrix cell, superuser
  teardown  async function(ctx)?    – called after  each matrix cell, superuser

ctx.request(method, path, body?, extraHeaders?)
  Executes an HTTP call with superuser JWT; returns { status, body }.
ctx.resolveString(s)
  Interpolates a single string as a template literal with ctx in scope.
ctx.resolveDeep(value)
  Recursively walks value and interpolates every string that contains '\${'.

String interpolation:
  Any string value in path, body (recursively), or headers that contains '\${' is
  evaluated as a template literal with ctx in scope after setup has run.
  Example:  path: "/_db/d/_api/document/\${ctx.data.id}"
`);
}

// ─── JWT generation (HS256) ───────────────────────────────────────────────────

function generateSuperuserJwt(secret) {
  const now = Math.floor(Date.now() / 1000);
  const header  = Buffer.from(JSON.stringify({ alg: 'HS256', typ: 'JWT' })).toString('base64url');
  const payload = Buffer.from(JSON.stringify({ iss: 'arangodb', server_id: 'foo', iat: now })).toString('base64url');
  const signingInput = `${header}.${payload}`;
  const sig = createHmac('sha256', secret).update(signingInput).digest('base64url');
  return `${signingInput}.${sig}`;
}

// ─── HTTP helpers ─────────────────────────────────────────────────────────────

/** Single shared undici Agent that ignores TLS cert errors. */
let _agent = null;
function getAgent() {
  if (!_agent) {
    _agent = new Agent({ connect: { rejectUnauthorized: false } });
  }
  return _agent;
}

/**
 * Make an HTTP request and return { status, body }.
 * body is parsed as JSON if possible, otherwise returned as a string.
 */
async function httpRequest(endpoint, method, path, body, headers) {
  const url = `${endpoint}${path}`;
  const reqHeaders = { ...headers };
  let reqBody = null;

  if (body !== null && body !== undefined) {
    reqBody = JSON.stringify(body);
    reqHeaders['content-type'] = 'application/json';
  }

  const resp = await undiciRequest(url, {
    method,
    headers: reqHeaders,
    body: reqBody,
    dispatcher: getAgent(),
  });

  const text = await resp.body.text();
  let parsedBody;
  try {
    parsedBody = JSON.parse(text);
  } catch {
    parsedBody = text;
  }

  return { status: resp.statusCode, body: parsedBody };
}

/** Build a context object for setup/teardown that wraps superuser calls. */
function makeSuperuserCtx(endpoint, superuserToken) {
  const ctx = {
    /**
     * Execute an HTTP call with the superuser JWT.
     * @param {string} method
     * @param {string} path
     * @param {object|null} [body]
     * @param {object} [extraHeaders]
     * @returns {Promise<{status: number, body: any}>}
     */
    async request(method, path, body = null, extraHeaders = {}) {
      return httpRequest(endpoint, method, path, body, {
        ...extraHeaders,
        'Authorization': `bearer ${superuserToken}`,
      });
    },

    /**
     * Interpolate a single string as a template literal with ctx in scope.
     * Strings that do not contain '${' are returned unchanged.
     * @param {string} s
     * @returns {string}
     */
    resolveString(s) {
      return resolveString(s, ctx);
    },

    /**
     * Recursively walk value and interpolate every string that contains '${'.
     * Non-string leaves (numbers, booleans, null, …) are returned as-is.
     * @param {any} value
     * @returns {any}
     */
    resolveDeep(value) {
      return resolveDeep(value, ctx);
    },

    endpoint,
  };
  return ctx;
}

// ─── Access-level constants ───────────────────────────────────────────────────

// U = undefined, N = none, R = ro, W = rw
const LEVELS = ['U', 'N', 'R', 'W'];
const LEVEL_LABEL = { U: 'undef', N: 'none', R: 'ro', W: 'rw' };
// Maps a level letter to the grant string for the ArangoDB user API (null = don't set).
const GRANT_MAP = { U: null, N: 'none', R: 'ro', W: 'rw' };

// ─── Output formatting ────────────────────────────────────────────────────────

/** Center a string within `width` characters. */
function centerStr(s, width) {
  const pad = width - s.length;
  if (pad <= 0) return s.slice(0, width);
  const left  = Math.floor(pad / 2);
  const right = pad - left;
  return ' '.repeat(left) + s + ' '.repeat(right);
}

// Column widths (matching the Rust implementation):
//   regular columns  – 8-char content → "| <8chars> " = 11 chars total
//   admin columns    – 10-char content → "| <10chars> " = 13 chars total

function colCell(content)      { return `| ${content.padEnd(8)} `; }
function codeCell(code)        { return `| ${centerStr(String(code), 8)} `; }
function adminColCell(content) { return `| ${content.padEnd(10)} `; }
function adminCodeCell(code)   { return `| ${centerStr(String(code), 10)} `; }

const COLL_SEP        = '----------------------|----------|----------|----------|----------|';
const LEVEL_SEP       = '|----------|----------|----------|----------|';
const ADMIN_SEP       = '|------------|------------|------------|------------|';
const ADMIN_SEP_SUPER = '|------------|------------|------------|------------|------------|';

// ─── Setup / Teardown ────────────────────────────────────────────────────────

/**
 * Create database 'd', collection 'c' in it, 64 permission-matrix users,
 * and 4 admin test users (AU/AN/AR/AW).
 */
async function doSetup(endpoint, rootPassword) {
  const authHeader = `Basic ${Buffer.from(`root:${rootPassword}`).toString('base64')}`;

  async function api(method, path, body = null) {
    return httpRequest(endpoint, method, path, body, { 'Authorization': authHeader });
  }

  // ── Create database 'd' ──────────────────────────────────────────────────
  process.stdout.write("Creating database 'd'...");
  const dbResp = await api('POST', '/_db/_system/_api/database', { name: 'd' });
  if (dbResp.status >= 200 && dbResp.status < 300) {
    console.log(' OK');
  } else {
    throw new Error(`Failed to create database 'd': ${dbResp.status} - ${JSON.stringify(dbResp.body)}`);
  }

  // ── Create collection 'c' ────────────────────────────────────────────────
  process.stdout.write("Creating collection 'c' in database 'd'...");
  const collResp = await api('POST', '/_db/d/_api/collection', { name: 'c' });
  if (collResp.status >= 200 && collResp.status < 300) {
    console.log(' OK');
  } else {
    throw new Error(`Failed to create collection 'c': ${collResp.status} - ${JSON.stringify(collResp.body)}`);
  }

  // ── Create 64 permission-matrix users ────────────────────────────────────
  console.log('Creating 64 users...');
  for (const db of LEVELS) {
    for (const wc of LEVELS) {
      for (const coll of LEVELS) {
        const username = `${db}${wc}${coll}`;
        process.stdout.write(`  User '${username}': creating`);

        const r = await api('POST', '/_db/_system/_api/user', { user: username, passwd: username });
        if (r.status < 200 || r.status >= 300) {
          throw new Error(`Failed to create user '${username}': ${r.status} - ${JSON.stringify(r.body)}`);
        }

        const dbGrant = GRANT_MAP[db];
        if (dbGrant !== null) {
          process.stdout.write(`, db=${dbGrant}`);
          const r2 = await api('PUT', `/_db/_system/_api/user/${username}/database/d`, { grant: dbGrant });
          if (r2.status < 200 || r2.status >= 300) {
            throw new Error(`Failed to set DB permission for '${username}': ${r2.status} - ${JSON.stringify(r2.body)}`);
          }
        }

        const wcGrant = GRANT_MAP[wc];
        if (wcGrant !== null) {
          process.stdout.write(`, *=${wcGrant}`);
          const r2 = await api('PUT', `/_db/_system/_api/user/${username}/database/d/*`, { grant: wcGrant });
          if (r2.status < 200 || r2.status >= 300) {
            throw new Error(`Failed to set wildcard permission for '${username}': ${r2.status} - ${JSON.stringify(r2.body)}`);
          }
        }

        const collGrant = GRANT_MAP[coll];
        if (collGrant !== null) {
          process.stdout.write(`, c=${collGrant}`);
          const r2 = await api('PUT', `/_db/_system/_api/user/${username}/database/d/c`, { grant: collGrant });
          if (r2.status < 200 || r2.status >= 300) {
            throw new Error(`Failed to set collection permission for '${username}': ${r2.status} - ${JSON.stringify(r2.body)}`);
          }
        }

        console.log();
      }
    }
  }

  // ── Create 4 admin test users ────────────────────────────────────────────
  console.log('Creating admin test users (AU / AN / AR / AW)...');
  const adminUsers = [
    ['AU', null],
    ['AN', 'none'],
    ['AR', 'ro'],
    ['AW', 'rw'],
  ];
  for (const [username, grant] of adminUsers) {
    process.stdout.write(`  User '${username}': creating`);
    const r = await api('POST', '/_db/_system/_api/user', { user: username, passwd: username });
    if (r.status < 200 || r.status >= 300) {
      throw new Error(`Failed to create user '${username}': ${r.status} - ${JSON.stringify(r.body)}`);
    }
    if (grant !== null) {
      process.stdout.write(`, _system=${grant}`);
      const r2 = await api('PUT', `/_db/_system/_api/user/${username}/database/_system`, { grant });
      if (r2.status < 200 || r2.status >= 300) {
        throw new Error(`Failed to set _system permission for '${username}': ${r2.status} - ${JSON.stringify(r2.body)}`);
      }
    }
    console.log();
  }

  console.log('Setup complete. 64 collection/DB-level users + 4 admin users created.');
  console.log("Each user's password equals their username.");
}

/**
 * Drop database 'd' and delete all 64 permission-matrix users plus the
 * 4 admin test users (AU/AN/AR/AW).
 */
async function doTeardown(endpoint, rootPassword) {
  const authHeader = `Basic ${Buffer.from(`root:${rootPassword}`).toString('base64')}`;

  async function api(method, path) {
    return httpRequest(endpoint, method, path, null, { 'Authorization': authHeader });
  }

  // ── Drop database 'd' ────────────────────────────────────────────────────
  process.stdout.write("Dropping database 'd'...");
  const r = await api('DELETE', '/_db/_system/_api/database/d');
  if (r.status >= 200 && r.status < 300) {
    console.log(' OK');
  } else {
    console.error(`  Warning: ${r.status} - ${JSON.stringify(r.body)}`);
  }

  // ── Delete 64 permission-matrix users ────────────────────────────────────
  console.log('Deleting 64 users...');
  for (const db of LEVELS) {
    for (const wc of LEVELS) {
      for (const coll of LEVELS) {
        const username = `${db}${wc}${coll}`;
        process.stdout.write(`  Deleting user '${username}'...`);
        const resp = await api('DELETE', `/_db/_system/_api/user/${username}`);
        if (resp.status >= 200 && resp.status < 300) {
          console.log(' OK');
        } else {
          console.log(` Warning: ${resp.status} - ${JSON.stringify(resp.body)}`);
        }
      }
    }
  }

  // ── Delete 4 admin test users ────────────────────────────────────────────
  console.log('Deleting admin test users...');
  for (const username of ['AU', 'AN', 'AR', 'AW']) {
    process.stdout.write(`  Deleting user '${username}'...`);
    const resp = await api('DELETE', `/_db/_system/_api/user/${username}`);
    if (resp.status >= 200 && resp.status < 300) {
      console.log(' OK');
    } else {
      console.log(` Warning: ${resp.status} - ${JSON.stringify(resp.body)}`);
    }
  }

  console.log('Teardown complete.');
}

// ─── Runtime value resolution ─────────────────────────────────────────────────

/**
 * Interpolate a single string as a template literal with ctx in scope,
 * but only when the string actually contains '${'.
 * Strings without '${' are returned unchanged (zero overhead).
 *
 * new Function('ctx', 'return `...`') compiles the string as the body of a
 * template literal, making ctx (and ctx.data) available at evaluation time.
 * This lets test files use ordinary double-quoted strings such as
 *   "/_db/d/_api/document/${ctx.data.id}"
 * instead of backtick template literals.
 */
function resolveString(s, ctx) {
  if (s.includes('${')) {
    return new Function('ctx', `return \`${s}\``)(ctx);
  }
  return s;
}

/**
 * Recursively walk value and interpolate every string that contains '${'.
 * - Strings  → resolveString(s, ctx)
 * - Arrays   → each element resolved recursively
 * - Objects  → each own-property value resolved recursively
 * - All other types (number, boolean, null, …) → returned as-is
 */
function resolveDeep(value, ctx) {
  if (typeof value === 'string') {
    return resolveString(value, ctx);
  }
  if (Array.isArray(value)) {
    return value.map(v => resolveDeep(v, ctx));
  }
  if (value !== null && typeof value === 'object') {
    const out = {};
    for (const [k, v] of Object.entries(value)) {
      out[k] = resolveDeep(v, ctx);
    }
    return out;
  }
  return value;
}

// ─── Per-type test runners ────────────────────────────────────────────────────

/**
 * Run a collection-level test.
 * Matrix: rows = COLL × wildcard (16 rows), columns = DB level (4 cols).
 * User name = <DB><WC><COLL>  (e.g. "UNR"), password == username.
 */
async function runCollectionTest(endpoint, superuserToken, test) {
  const { name, method, path, body = null, headers = {}, setup, teardown } = test;
  const ctx = makeSuperuserCtx(endpoint, superuserToken);

  console.log(name);
  console.log(`${method} ${path}`);
  if (Object.keys(headers).length > 0) console.log(JSON.stringify(headers));

  const hdr = ' '.repeat(22)
    + colCell('DB undef') + colCell('DB none') + colCell('DB ro') + colCell('DB rw') + '|';
  console.log(hdr);
  console.log(COLL_SEP);

  for (const coll of LEVELS) {
    for (const wc of LEVELS) {
      const label = `COLL ${LEVEL_LABEL[coll]}, * ${LEVEL_LABEL[wc]}`;
      let row = label.padEnd(22);

      for (const db of LEVELS) {
        const username = `${db}${wc}${coll}`;
        const authHeader = `Basic ${Buffer.from(`${username}:${username}`).toString('base64')}`;

        if (setup) {
          ctx.data = await runPhase(setup, ctx, 'setup', name);
        }

        const resolvedPath    = resolveString(path, ctx);
        const resolvedBody    = resolveDeep(body, ctx);
        const resolvedHeaders = resolveDeep(headers, ctx);
        const resp = await httpRequest(endpoint, method, resolvedPath, resolvedBody, {
          ...resolvedHeaders,
          'Authorization': authHeader,
        });

        if (teardown) {
          await runPhase(teardown, ctx, 'teardown', name);
        }
        ctx.data = undefined;

        row += codeCell(resp.status);
      }

      row += '|';
      console.log(row);
    }
  }
}

/**
 * Run a database-level test.
 * Single row with 4 columns (DB level); uses users XUU (wildcard/coll = undef).
 */
async function runDatabaseTest(endpoint, superuserToken, test) {
  const { name, method, path, body = null, headers = {}, setup, teardown } = test;
  const ctx = makeSuperuserCtx(endpoint, superuserToken);

  console.log(name);
  console.log(`${method} ${path}`);
  if (Object.keys(headers).length > 0) console.log(JSON.stringify(headers));

  const hdr = colCell('DB undef') + colCell('DB none') + colCell('DB ro') + colCell('DB rw') + '|';
  console.log(hdr);
  console.log(LEVEL_SEP);

  let row = '';
  for (const db of LEVELS) {
    const username = `${db}UU`;
    const authHeader = `Basic ${Buffer.from(`${username}:${username}`).toString('base64')}`;

    if (setup) {
      ctx.data = await runPhase(setup, ctx, 'setup', name);
    }

    const resolvedPath    = resolveString(path, ctx);
    const resolvedBody    = resolveDeep(body, ctx);
    const resolvedHeaders = resolveDeep(headers, ctx);
    const resp = await httpRequest(endpoint, method, resolvedPath, resolvedBody, {
      ...resolvedHeaders,
      'Authorization': authHeader,
    });

    if (teardown) {
      await runPhase(teardown, ctx, 'teardown', name);
    }
    ctx.data = undefined;

    row += codeCell(resp.status);
  }
  row += '|';
  console.log(row);
}

/**
 * Run an admin test.
 * Columns: AU / AN / AR / AW (users with _system db access undef/none/ro/rw)
 * plus a final superuser column (JWT, no user restrictions).
 */
async function runAdminTest(endpoint, superuserToken, test) {
  const { name, method, path, body = null, headers = {}, setup, teardown } = test;
  const ctx = makeSuperuserCtx(endpoint, superuserToken);

  console.log(name);
  console.log(`${method} ${path}`);
  if (Object.keys(headers).length > 0) console.log(JSON.stringify(headers));

  const hdr = adminColCell('_sys undef')
    + adminColCell('_sys none')
    + adminColCell('_sys ro')
    + adminColCell('_sys rw')
    + adminColCell('superuser')
    + '|';
  console.log(hdr);
  console.log(ADMIN_SEP_SUPER);

  let row = '';

  // Four named admin users
  for (const username of ['AU', 'AN', 'AR', 'AW']) {
    const authHeader = `Basic ${Buffer.from(`${username}:${username}`).toString('base64')}`;

    if (setup) {
      ctx.data = await runPhase(setup, ctx, 'setup', name);
    }

    const resolvedPath    = resolveString(path, ctx);
    const resolvedBody    = resolveDeep(body, ctx);
    const resolvedHeaders = resolveDeep(headers, ctx);
    const resp = await httpRequest(endpoint, method, resolvedPath, resolvedBody, {
      ...resolvedHeaders,
      'Authorization': authHeader,
    });

    if (teardown) {
      await runPhase(teardown, ctx, 'teardown', name);
    }
    ctx.data = undefined;

    row += adminCodeCell(resp.status);
  }

  // Superuser column
  if (setup) {
    ctx.data = await runPhase(setup, ctx, 'setup', name);
  }

  const resolvedPath    = resolveString(path, ctx);
  const resolvedBody    = resolveDeep(body, ctx);
  const resolvedHeaders = resolveDeep(headers, ctx);
  const suResp = await httpRequest(endpoint, method, resolvedPath, resolvedBody, {
    ...resolvedHeaders,
    'Authorization': `bearer ${superuserToken}`,
  });

  if (teardown) {
    await runPhase(teardown, ctx, 'teardown', name);
  }
  ctx.data = undefined;

  row += adminCodeCell(suResp.status) + '|';
  console.log(row);
}

/**
 * Execute a setup or teardown phase function.
 * Any thrown error is wrapped and re-thrown with phase metadata attached.
 */
async function runPhase(fn, ctx, phase, testName) {
  try {
    return await fn(ctx);
  } catch (err) {
    const msg = err instanceof Error ? err.message : String(err);
    const wrapper = new Error(`[${phase}] for test "${testName}": ${msg}`);
    wrapper.phase = phase;
    wrapper.testName = testName;
    wrapper.cause = err;
    throw wrapper;
  }
}

// ─── Main ─────────────────────────────────────────────────────────────────────

async function main() {
  const opts = parseArgs();

  // Dispatch setup / teardown without needing a JWT secret
  if (opts.subcommand === 'setup') {
    await doSetup(opts.endpoint, opts.rootPassword);
    return;
  }

  if (opts.subcommand === 'teardown') {
    await doTeardown(opts.endpoint, opts.rootPassword);
    return;
  }

  // 'test' subcommand ────────────────────────────────────────────────────────

  // Load JWT secret (trim trailing whitespace / newlines)
  let secret;
  try {
    secret = readFileSync(opts.jwtSecretFile, 'utf8').trim();
  } catch (err) {
    console.error(`Error reading JWT secret file "${opts.jwtSecretFile}": ${err.message}`);
    process.exit(1);
  }

  const superuserToken = generateSuperuserJwt(secret);

  // Discover and sort test files
  const dir = resolve(opts.directory);
  let files;
  try {
    files = readdirSync(dir)
      .filter(f => f.endsWith('.mjs'))
      .sort()
      .map(f => join(dir, f));
  } catch (err) {
    console.error(`Error reading directory "${dir}": ${err.message}`);
    process.exit(1);
  }

  if (files.length === 0) {
    console.log(`No *.mjs files found in ${dir}`);
    return;
  }

  // Load all test entries from all files, preserving order within each file
  const collectionTests = [];
  const databaseTests   = [];
  const adminTests      = [];

  for (const file of files) {
    let mod;
    try {
      mod = await import(pathToFileURL(file).href);
    } catch (err) {
      console.error(`Error importing "${file}": ${err.message}`);
      process.exit(1);
    }

    const tests = mod.default;
    if (!Array.isArray(tests)) {
      console.error(`Error: "${file}" does not export a default array.`);
      process.exit(1);
    }

    for (const test of tests) {
      if (!test.name)   { console.error(`Error in "${file}": a test entry is missing "name".`);   process.exit(1); }
      if (!test.type)   { console.error(`Error in "${file}": test "${test.name}" is missing "type".`);   process.exit(1); }
      if (!test.method) { console.error(`Error in "${file}": test "${test.name}" is missing "method".`); process.exit(1); }
      if (!test.path)   { console.error(`Error in "${file}": test "${test.name}" is missing "path".`);   process.exit(1); }

      switch (test.type) {
        case 'collection': collectionTests.push(test); break;
        case 'database':   databaseTests.push(test);   break;
        case 'admin':      adminTests.push(test);      break;
        default:
          console.error(`Error in "${file}": test "${test.name}" has unknown type "${test.type}". Expected "collection", "database", or "admin".`);
          process.exit(1);
      }
    }
  }

  // ── Run tests ──────────────────────────────────────────────────────────────

  let fatalError = false;

  async function execTest(test, runFn) {
    try {
      await runFn(opts.endpoint, superuserToken, test);
      console.log();
    } catch (err) {
      if (err.phase) {
        // Setup or teardown failure – fatal
        console.error(`\nFATAL: ${err.message}`);
        if (err.cause && err.cause.stack) {
          console.error(err.cause.stack);
        }
        fatalError = true;
      } else {
        throw err;
      }
    }
  }

  if (collectionTests.length > 0) {
    console.log('=== Collection-level tests ===\n');
    for (const test of collectionTests) {
      await execTest(test, runCollectionTest);
      if (fatalError) process.exit(1);
    }
  }

  if (databaseTests.length > 0) {
    console.log('=== Database-level tests ===\n');
    for (const test of databaseTests) {
      await execTest(test, runDatabaseTest);
      if (fatalError) process.exit(1);
    }
  }

  if (adminTests.length > 0) {
    console.log('=== Admin tests ===\n');
    for (const test of adminTests) {
      await execTest(test, runAdminTest);
      if (fatalError) process.exit(1);
    }
  }
}

main().catch(err => {
  console.error('Fatal error:', err.stack || err.message || err);
  process.exit(1);
});
