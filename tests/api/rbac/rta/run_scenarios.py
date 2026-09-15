#!/usr/bin/env python3
"""Run rta-makedata under a series of RBAC configurations and diff the outcomes.

    tests/api/rbac/rta/run_scenarios.py --setup

For every scenario in scenarios.py this
  1. seeds the scenario's policy / role / user-role-binding into the
     authorization sidecar through the management API,
  2. waits until the change is visible to the policy decision point,
  3. runs the *unmodified* rta-makedata phases via arangosh, authenticating as
     the scenario's user with a per-user JWT,
  4. classifies each run as pass / deny / error and compares it against the
     scenario's expectation.

Stack setup is delegated to the sibling scripts in ../scripts (start_arangod.sh,
start_sidecar.sh); this script does not reimplement it.

AUTHENTICATION NOTE
===================
arangosh must authenticate with a JWT, never with username/password. Under RBAC
arangod forwards `req.jwtToken()` to the policy decision point, and that is
empty for Basic auth, so the request is rejected before any policy is consulted
and every scenario would "fail" for the wrong reason. Hence --server.jwt-token
with a token minted by ../mkjwt.py.

Exit code is 0 only if every step matched its expectation.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
RBAC_DIR = os.path.dirname(HERE)  # tests/api/rbac
SCRIPTS = os.path.join(RBAC_DIR, "scripts")
MKJWT = os.path.join(RBAC_DIR, "mkjwt.py")
# tests/api/rbac/rta -> tests/api/rbac -> tests/api -> tests -> <source root>
SOURCE_ROOT = os.path.abspath(os.path.join(RBAC_DIR, "..", "..", ".."))

sys.path.insert(0, HERE)
import scenarios as catalog  # noqa: E402

# ---------------------------------------------------------------------------
# outcome classification
# ---------------------------------------------------------------------------

# A run that exits non-zero counts as a *permission* denial only if the output
# actually shows one. Anything else that merely crashed is reported as `error`,
# so a scenario expecting `deny` cannot be satisfied by an unrelated failure.
DENIAL_SIGNATURES = (
    # What arangod actually surfaces for an RBAC denial: the batch message from
    # the policy decision point, propagated verbatim through
    # Auth/Rbac/ServiceImpl.cpp. This is the common case by far.
    "has been denied",
    "insufficient permissions",
    "permission denied",  # the sidecar's own single-item wording
    "explicit deny",
    "forbidden",
    "not authorized",
    "unauthorized",
    "error_forbidden",
    # RestHandler::handleAuthorizationChecks emits this for the database gate on
    # API version 0 - misleading wording, but it is an authorization denial.
    "user not authenticated",
    "http 401",
    "http 403",
    '"code":401',
    '"code":403',
    "errornum: 11",  # TRI_ERROR_FORBIDDEN as JSON
    '"errornum":11',
    # ...and as arangosh renders it. The trailing colon keeps this from matching
    # other error numbers that merely start with 11 (1101, 1128, ...).
    "arangoerror 11:",
    # RestHandler::checkDatabaseAccess emits this for the classic database gate
    # on API version 0 - the counterpart of RBAC's "User not authenticated" on
    # the very same code path. Only ever an authorization failure.
    "no read access to database",
    # Classic authorization words its refusals completely differently:
    #   "Failed to use database 'x'. Request requires database authentication
    #    level 'rw' but it has only level 'ro'."
    # built by accessLevelMismatchReason() in arangod/Auth/AuthMode.cpp. That
    # format string is the only occurrence of "authentication level" in the whole
    # source tree, and it only ever appears in an authorization failure, so it is
    # a safe signature.
    "authentication level",
    "but it has only level",
)

# When a user is denied their very first request, arangosh never completes its
# connect handshake and reports `ArangoError 2001: not connected` - with no
# mention of permissions at all. That is indistinguishable from a dead server by
# text alone, so it is resolved by probing: see needs_liveness_check().
CONNECT_FAILURE_SIGNATURES = (
    "not connected",
    "could not connect",
    "connection refused",
    "error 2001",
)

# Some authorization failures surface as 404 rather than 403, because hiding a
# resource's existence is preferable to admitting it exists but is off limits.
# The wording is then identical to a genuinely missing object, so 404 must never
# be a denial signal on its own: a missing fixture or a real product bug would
# score as a successful `deny`. It is resolved the same way as the connect
# failure above - by asking an identity that RBAC never denies. See
# resolve_not_found().
NOT_FOUND_SIGNATURES = (
    "collection or view not found",
    "data source not found",
    "graph not found",
    "view not found",
    '"errornum":1203',
    "errornum: 1203",
    '"code":404',
    "http 404",
)
# The messages usually name the object: `collection or view not found: foo`.
NOT_FOUND_NAME_RE = re.compile(
    r"(?:collection or view|data source|view|graph) not found:?\s*([A-Za-z0-9_\-]+)")

PASS = catalog.PASS
DENY = catalog.DENY
ERROR = "error"


def classify(returncode, output):
    if returncode == 0:
        return PASS, ""
    haystack = output.lower()
    for signature in DENIAL_SIGNATURES:
        if signature in haystack:
            return DENY, signature
    return ERROR, ""


def needs_liveness_check(returncode, output):
    """Did this run fail in a way only a liveness probe can classify?

    A user whose first request is denied never gets through arangosh's connect
    handshake, so the failure reads as `not connected` and says nothing about
    permissions. Rather than adding that phrase to DENIAL_SIGNATURES - which
    would let a dead server satisfy every `deny` expectation - the caller probes
    arangod as superuser and only then decides.
    """
    if returncode == 0:
        return False
    haystack = output.lower()
    if any(signature in haystack for signature in DENIAL_SIGNATURES):
        return False
    return any(signature in haystack for signature in CONNECT_FAILURE_SIGNATURES)


def looks_like_not_found(returncode, output):
    """Did this run fail only with `not found`, with no denial evidence?"""
    if returncode == 0:
        return False
    haystack = output.lower()
    if any(signature in haystack for signature in DENIAL_SIGNATURES):
        return False
    return any(signature in haystack for signature in NOT_FOUND_SIGNATURES)


def resolve_not_found(config, tokens, output):
    """Decide whether a `not found` failure was really a denial.

    Authorization failures can be reported as 404 to avoid revealing that an
    object exists. The wording is then the same as for an object that genuinely
    is not there, so text alone cannot tell them apart. What can is asking an
    identity RBAC never denies: if the superuser can see the object the workload
    was told does not exist, the 404 was hiding it.

    Returns (is_denial, explanation).
    """
    names = []
    for match in NOT_FOUND_NAME_RE.finditer(output):
        name = match.group(1)
        if name and name not in names:
            names.append(name)
    if not names:
        return False, ("reported `not found` without naming an object, so it cannot "
                       "be told apart from a genuine absence - treated as an error")
    token = tokens.superuser()
    database = urllib.parse.quote(config.database)
    for name in names:
        for path in (f"/_api/collection/{name}/properties",
                     f"/_api/view/{name}/properties"):
            status, _ = http(f"{config.arangod_url}/_db/{database}{path}",
                             token=token, timeout=10)
            if 200 <= status < 300:
                return True, (f"404 for '{name}', which the superuser can see - "
                              f"the response was hiding it, not reporting absence")
    return False, (f"404 for {', '.join(names)}, which the superuser cannot see either - "
                   f"genuinely absent, so this is an error and not a denial")


# ---------------------------------------------------------------------------
# configuration
# ---------------------------------------------------------------------------


class Config:
    def __init__(self, args):
        work = os.environ.get("RBAC_WORK", "/tmp/arangodb-rbac-test")
        self.work = args.work or work
        self.jwt_dir = os.path.join(self.work, "jwt")
        # The operator's ActiveJWTKey convention: the active signing key is the
        # file literally named "-" inside the secret folder.
        self.secret_file = args.jwt_secret_file or os.path.join(self.jwt_dir, "-")
        self.log_dir = os.path.join(self.work, "log")

        self.arangod_endpoint = args.endpoint
        self.arangod_url = (
            args.endpoint.replace("tcp://", "http://").replace("ssl://", "https://")
        )
        self.mgmt_url = args.management.rstrip("/")
        self.integration_url = args.integration.rstrip("/")

        self.arangosh = args.arangosh or os.path.join(SOURCE_ROOT, "build", "bin", "arangosh")
        # Used both for capability detection and by the setup scripts.
        self.arangod = args.arangod or os.path.join(SOURCE_ROOT, "build", "bin", "arangod")
        self.rta = args.rta or os.path.join(SOURCE_ROOT, "3rdParty", "rta-makedata")

        self.database = args.database
        self.other_database = f"{args.database}_elsewhere"
        self.suite_filter = args.test
        self.timeout = args.phase_timeout
        self.propagation_timeout = args.propagation_timeout
        self.dry_run = args.dry_run
        self.verbose = args.verbose
        self.requested_mode = args.auth_mode
        self.external_sidecar = args.external_sidecar

    def script_env(self):
        """Environment for the vendored ../scripts, which honour these names."""
        env = dict(os.environ)
        env.setdefault("RBAC_WORK", self.work)
        env.setdefault("ARANGODB_SRC", SOURCE_ROOT)
        env.setdefault("ARANGOSH", self.arangosh)
        env.setdefault("ARANGOD", self.arangod)
        return env


# ---------------------------------------------------------------------------
# tokens
# ---------------------------------------------------------------------------


class Tokens:
    """Per-identity JWTs, minted by the vendored mkjwt.py so there is exactly
    one implementation of the token format in this tree."""

    def __init__(self, config):
        self.config = config
        self._cache = {}

    def _mint(self, *argv):
        # A dry run must not depend on a real signing secret existing.
        if self.config.dry_run:
            return f"<jwt:{'/'.join(argv[2:]) or 'superuser'}>"
        if not os.path.exists(self.config.secret_file):
            raise RuntimeError(
                f"JWT secret {self.config.secret_file} does not exist. It is created "
                f"by scripts/env.sh (ensure_secret); run with --setup or point "
                f"--jwt-secret-file at the deployment's active key."
            )
        result = subprocess.run(
            [sys.executable, MKJWT, *argv],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            raise RuntimeError(f"mkjwt.py failed: {result.stderr.strip()}")
        return result.stdout.strip()

    def superuser(self):
        if "\0super" not in self._cache:
            self._cache["\0super"] = self._mint("superuser", self.config.secret_file)
        return self._cache["\0super"]

    def user(self, name):
        if name not in self._cache:
            self._cache[name] = self._mint("user", self.config.secret_file, name)
        return self._cache[name]

    def forget(self, name):
        self._cache.pop(name, None)

    def for_identity(self, identity, scenario):
        if identity == catalog.SUPERUSER:
            return self.superuser()
        return self.user(scenario.user)


# ---------------------------------------------------------------------------
# HTTP
# ---------------------------------------------------------------------------


def http(url, method="GET", body=None, token=None, timeout=30):
    data = json.dumps(body).encode() if body is not None else None
    headers = {"content-type": "application/json", "accept": "application/json"}
    if token:
        headers["authorization"] = f"bearer {token}"
    request = urllib.request.Request(url, data=data, method=method, headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            raw = response.read()
            return response.status, (json.loads(raw) if raw else {})
    except urllib.error.HTTPError as error:
        raw = error.read()
        try:
            return error.code, (json.loads(raw) if raw else {})
        except json.JSONDecodeError:
            return error.code, {"raw": raw.decode(errors="replace")}
    except (urllib.error.URLError, TimeoutError, OSError) as error:
        return 0, {"error": str(error)}


class Sidecar:
    """The authorization sidecar's management API plus the PDP's evaluate API."""

    def __init__(self, config, tokens):
        self.config = config
        self.tokens = tokens

    @property
    def mgmt(self):
        return f"{self.config.mgmt_url}/_management/permissions"

    def call(self, method, path, body=None):
        if self.config.dry_run:
            print(f"    [dry-run] {method} {self.mgmt}{path}")
            if body is not None and self.config.verbose:
                print(f"             {json.dumps(body)}")
            return 200, {}
        return http(f"{self.mgmt}{path}", method, body, self.tokens.superuser())

    # -- policies / roles / bindings -----------------------------------
    def put_policy(self, name, statements):
        self.call("DELETE", f"/policy/{name}")
        status, body = self.call("POST", f"/policy/{name}", {"item": {"statements": statements}})
        _expect_ok(status, body, f"create policy {name}")

    def put_role(self, name, policies):
        self.call("DELETE", f"/role/{name}")
        status, body = self.call("POST", f"/role/{name}", {"item": {"policies": policies}})
        _expect_ok(status, body, f"create role {name}")

    def put_binding(self, user, role, scope):
        self.call("DELETE", f"/user/{user}/role/{role}")
        # `scope` may be None on purpose: a binding whose scope is absent is
        # skipped by the evaluator and therefore grants nothing.
        payload = {} if scope is None else {"scope": {"statements": scope}}
        status, body = self.call("POST", f"/user/{user}/role/{role}", payload)
        _expect_ok(status, body, f"bind {role} to {user}")

    def unbind(self, user, role):
        return self.call("DELETE",
                         f"/user/{urllib.parse.quote(user)}/role/{urllib.parse.quote(role)}")

    def drop(self, scenario):
        if scenario.policy is None:
            return
        self.call("DELETE", f"/user/{scenario.user}/role/{scenario.role_name}")
        self.call("DELETE", f"/role/{scenario.role_name}")
        self.call("DELETE", f"/policy/{scenario.policy_name}")

    def refresh(self):
        self.call("POST", "/refresh", {})

    # -- evaluation ----------------------------------------------------
    def evaluate(self, user, action, resource):
        """Ask the PDP directly; used to detect that a change has propagated."""
        status, body = http(
            f"{self.config.integration_url}/_integration/authorization/v1/evaluate",
            "POST",
            {"user": user, "action": action, "resource": resource},
        )
        if status != 200:
            return None
        # Effect_Deny is 0, and protojson omits default values, so an absent
        # `effect` means Deny.
        return body.get("effect", "Deny")


def _expect_ok(status, body, what):
    if not 200 <= status <= 299:
        raise RuntimeError(f"{what} failed: HTTP {status} {json.dumps(body)[:300]}")


# ---------------------------------------------------------------------------
# stack control
# ---------------------------------------------------------------------------


def run_script(config, name, *args):
    path = os.path.join(SCRIPTS, name)
    if not os.path.exists(path):
        raise RuntimeError(
            f"{path} is missing. The stack-setup scripts live on the RBAC feature "
            f"branch; see rta/README.md."
        )
    print(f"  -> {name} {' '.join(args)}")
    if config.dry_run:
        return
    result = subprocess.run(["bash", path, *args], env=config.script_env(), check=False)
    if result.returncode != 0:
        raise RuntimeError(f"{name} exited {result.returncode}")


def wait_for_arangod(config, attempts=120):
    """arangod is ready once it answers - 401 included, since we probe
    unauthenticated on purpose."""
    if config.dry_run:
        return
    for _ in range(attempts):
        status, _ = http(f"{config.arangod_url}/_api/version", timeout=2)
        if status != 0:
            return
        time.sleep(1)
    raise RuntimeError(f"arangod at {config.arangod_url} did not become reachable")


def ensure_user(config, tokens, user):
    """Create the arangod user. Not strictly required for authorization - the
    JWT carries the identity and the sidecar resolves bindings by name - but a
    real deployment has the user, so we match that."""
    if config.dry_run:
        print(f"    [dry-run] create arangod user {user}")
        return
    status, body = http(
        f"{config.arangod_url}/_api/user",
        "POST",
        {"user": user, "passwd": ""},
        tokens.superuser(),
    )
    # 409 = already there, which is fine on a re-run.
    if status not in (200, 201, 409):
        print(f"    warning: creating user {user} returned HTTP {status} "
              f"{json.dumps(body)[:200]}")


def remove_user(config, tokens, user):
    """Delete the arangod user again.

    The unittest harness runs a SUT cleanliness check after every suite and
    fails the run when a test leaves accounts behind, so the users created by
    ensure_user() have to go away with the rest of the scenario."""
    if config.dry_run:
        print(f"    [dry-run] delete arangod user {user}")
        return
    status, body = http(
        f"{config.arangod_url}/_api/user/{urllib.parse.quote(user)}",
        "DELETE",
        None,
        tokens.superuser(),
    )
    # 404 = already gone, which is fine.
    if status not in (200, 202, 404):
        print(f"    warning: deleting user {user} returned HTTP {status} "
              f"{json.dumps(body)[:200]}")


def create_database(config, tokens, database):
    """Create the target database as superuser. Idempotent."""
    if config.dry_run:
        print(f"    [dry-run] create database {database}")
        return
    status, body = http(f"{config.arangod_url}/_api/database", "POST",
                        {"name": database}, tokens.superuser())
    # 409 = already there, which is fine.
    if status not in (200, 201, 409):
        raise RuntimeError(f"pre-creating database {database} failed: HTTP {status} "
                           f"{json.dumps(body)[:200]}")


def create_collection(config, tokens, database, name):
    """Create a collection as superuser. Idempotent."""
    if config.dry_run:
        print(f"    [dry-run] create collection {database}/{name}")
        return
    status, body = http(f"{config.arangod_url}/_db/{urllib.parse.quote(database)}"
                        f"/_api/collection", "POST", {"name": name},
                        tokens.superuser())
    if status not in (200, 201, 409):
        raise RuntimeError(f"pre-creating collection {database}/{name} failed: "
                           f"HTTP {status} {json.dumps(body)[:200]}")


def drop_database(config, tokens, database):
    """Reset between scenarios so each starts from the same state."""
    if config.dry_run:
        print(f"    [dry-run] drop database {database}")
        return
    http(
        f"{config.arangod_url}/_api/database/{database}",
        "DELETE",
        None,
        tokens.superuser(),
    )


# ---------------------------------------------------------------------------
# propagation
# ---------------------------------------------------------------------------


PREFLIGHT_NAME = "rta_rbac_preflight"
ALLOW_EVERYTHING = [{"effect": "Allow", "actions": ["*"], "resources": ["*"]}]


BOOTSTRAP_NAME = "rta_rbac_bootstrap"


def bootstrap_user(config, sidecar, tokens, user, remove=False):
    """Give `user` an allow-all binding, or take it away again.

    Deny-by-default means the identity the harness drives arangod with cannot do
    anything until it is bound - not even connect. The scenarios each manage
    their own users; this is only about the account the surrounding workload
    uses, and it is the same thing the operator does for `root` in a real
    deployment.
    """
    name = f"{BOOTSTRAP_NAME}_{user}"
    if remove:
        sidecar.unbind(user, name)
        sidecar.call("DELETE", f"/role/{urllib.parse.quote(name)}")
        sidecar.call("DELETE", f"/policy/{urllib.parse.quote(name)}")
        sidecar.refresh()
        print(f"removed the allow-all binding for '{user}'")
        return 0

    sidecar.put_policy(name, ALLOW_EVERYTHING)
    sidecar.put_role(name, [name])
    sidecar.put_binding(user, name, ALLOW_EVERYTHING)
    sidecar.refresh()

    deadline = time.time() + config.propagation_timeout
    effect = None
    while time.time() < deadline:
        effect = sidecar.evaluate(user, "db:Read", "db:database:_system")
        if effect == "Allow":
            print(f"'{user}' now has an allow-all binding")
            return 0
        time.sleep(2)
    print(f"the allow-all binding for '{user}' never became visible "
          f"(last effect: {effect})", file=sys.stderr)
    return 3


def detect_auth_mode(config, sidecar, tokens):
    """Is arangod authorizing through RBAC, or through classic `_users` grants?

    Both are supported configurations: without --server.external-rbac-service
    arangod falls back to the classic grant model. Which one is in force decides
    which scenario catalog is meaningful, and getting it wrong produces a
    misleading half-green run - the negative scenarios still report `deny`,
    because a user with no classic grants has no access either, while the
    positive ones fail as though the product were broken.

    The probe is a user with **no classic grants** and an allow-all RBAC binding.
    Only an arangod consulting the RBAC service can let that user in; under
    classic permissions they are denied. The policy decision point is asked
    first, so a `classic` verdict is never just propagation lag.

    Returns "rbac" or "classic".
    """
    if config.dry_run:
        return config.requested_mode if config.requested_mode != "auto" else "rbac"

    # No sidecar reachable at all is the clearest possible answer: there is no
    # RBAC service for arangod to consult, so authorization must be classic. A
    # pure classic deployment has no sidecar to probe, and demanding one would
    # make `auto` unusable exactly where it is most convenient.
    status, _ = http(f"{config.mgmt_url}/_management/permissions/policy", timeout=5)
    if status == 0:
        print(f"  no RBAC management API at {config.mgmt_url}")
        return "classic"

    ensure_user(config, tokens, PREFLIGHT_NAME)
    try:
        sidecar.put_policy(PREFLIGHT_NAME, ALLOW_EVERYTHING)
        sidecar.put_role(PREFLIGHT_NAME, [PREFLIGHT_NAME])
        sidecar.put_binding(PREFLIGHT_NAME, PREFLIGHT_NAME, ALLOW_EVERYTHING)
        sidecar.refresh()

        deadline = time.time() + config.propagation_timeout
        effect = None
        while time.time() < deadline:
            effect = sidecar.evaluate(PREFLIGHT_NAME, "db:Read", "db:database:_system")
            if effect == "Allow":
                break
            time.sleep(2)
        if effect != "Allow":
            raise RuntimeError(
                f"the policy decision point never granted the probe user "
                f"(last effect: {effect}). The sidecar at {config.mgmt_url} is not "
                f"serving usable decisions, so the mode cannot be established."
            )

        status, _ = http(f"{config.arangod_url}/_api/collection",
                         token=tokens.user(PREFLIGHT_NAME), timeout=15)
        if 200 <= status < 300:
            return "rbac"
        # Granted everything by the RBAC service and still refused: arangod is
        # not asking it, i.e. classic authorization.
        return "classic"
    finally:
        try:
            sidecar.unbind(PREFLIGHT_NAME, PREFLIGHT_NAME)
            sidecar.call("DELETE", f"/role/{PREFLIGHT_NAME}")
            sidecar.call("DELETE", f"/policy/{PREFLIGHT_NAME}")
            http(f"{config.arangod_url}/_api/user/{PREFLIGHT_NAME}", "DELETE",
                 token=tokens.superuser())
            sidecar.refresh()
            tokens.forget(PREFLIGHT_NAME)
        except Exception as error:  # cleanup must never mask the real verdict
            print(f"  warning: mode-probe cleanup incomplete: {error}")


class ClassicGrants:
    """Seeds `_users` access levels, the classic counterpart of Sidecar.

    Levels are set with PUT /_api/user/<user>/database/<db>[/<collection>] and
    are visible immediately - there is no policy store, no pool and no streaming
    cache, so none of the propagation machinery the RBAC path needs applies here.
    """

    def __init__(self, config, tokens):
        self.config = config
        self.tokens = tokens

    def _url(self, user, path):
        return (f"{self.config.arangod_url}/_api/user/{urllib.parse.quote(user)}"
                f"/database/{path}")

    def apply(self, user, grants):
        for path, level in grants:
            url = self._url(user, path)
            if self.config.dry_run:
                print(f"    [dry-run] PUT {url} grant={level}")
                continue
            status, body = http(url, "PUT", {"grant": level}, self.tokens.superuser())
            if not 200 <= status <= 299:
                raise RuntimeError(f"granting {level} on {path} to {user} failed: "
                                   f"HTTP {status} {json.dumps(body)[:200]}")

    def revoke(self, user, grants):
        for path, _ in grants:
            if self.config.dry_run:
                print(f"    [dry-run] DELETE {self._url(user, path)}")
                continue
            http(self._url(user, path), "DELETE", None, self.tokens.superuser())

    def effective(self, user, path):
        status, body = http(self._url(user, path), token=self.tokens.superuser())
        if status != 200:
            return None
        return body.get("result")


def wait_until_visible(config, sidecar, scenario):
    """Block until the scenario's binding is visible to the PDP.

    Probes db:Read on db:database:_system, which every scenario that binds a
    policy is expected to allow (all three modelled role policies include
    db:Read, and every scope covers _system). Scenarios that intentionally grant
    nothing have no positive marker to wait for; for those a propagation delay
    can only produce the Deny we already expect, so waiting is unnecessary.
    """
    if scenario.policy is None or scenario.scope is None or not scenario.bind:
        return
    if config.dry_run:
        print("    [dry-run] wait for propagation")
        return
    deadline = time.time() + config.propagation_timeout
    while True:
        effect = sidecar.evaluate(scenario.user, "db:Read", "db:database:_system")
        if effect == "Allow":
            return
        if time.time() >= deadline:
            raise RuntimeError(
                f"the binding for {scenario.user} was not visible to the policy "
                f"decision point within {config.propagation_timeout}s "
                f"(last effect: {effect}). Documented worst case is ~30s."
            )
        time.sleep(2)


# ---------------------------------------------------------------------------
# running a phase
# ---------------------------------------------------------------------------


def phase_argv(config, phase, token, suite_filter=None):
    script = os.path.join(config.rta, "test_data", f"{phase}.js")
    suite_filter = config.suite_filter if suite_filter is None else suite_filter
    argv = [
        config.arangosh,
        "--server.endpoint", config.arangod_endpoint,
        # Must be a JWT: see the module docstring.
        "--server.jwt-token", token,
        "--log.level", "warning",
        "--javascript.execute", script,
        # Everything after `--` is ARGUMENTS inside the script; the first
        # positional is the database name.
        "--", config.database,
        "--progress", "true",
    ]
    if suite_filter:
        # makedata.js parses its own argv with internal.parseArgv, which turns a
        # purely numeric value into a Number, and then calls
        # options.test.split(',') on it: `--test 050` crashes with
        # "options.test.split is not a function". Repeating the value keeps it a
        # string without changing which suites match, since each entry is used as
        # an independent substring filter. rta-makedata stays unmodified, so the
        # workaround lives here.
        if "," not in suite_filter:
            suite_filter = f"{suite_filter},{suite_filter}"
        argv += ["--test", suite_filter]
    return argv


def redact(argv):
    """The JWT is a credential; keep it out of logs."""
    out = []
    skip = False
    for item in argv:
        if skip:
            out.append("<jwt>")
            skip = False
            continue
        out.append(item)
        if item == "--server.jwt-token":
            skip = True
    return out


def superuser_can_reach(config, tokens):
    """Is arangod healthy right now, judged by an identity RBAC never denies?"""
    status, _ = http(f"{config.arangod_url}/_api/version", token=tokens.superuser(),
                     timeout=10)
    return 200 <= status < 300


def run_phase(config, tokens, phase, token, assume=None, suite_filter=None):
    argv = phase_argv(config, phase, token, suite_filter)
    if config.verbose or config.dry_run:
        print(f"    $ {' '.join(redact(argv))}")
    if config.dry_run:
        # Nothing ran, so report what was expected rather than inventing a
        # result; a dry run is about the plumbing, not the verdict.
        return assume or PASS, "", ""
    try:
        result = subprocess.run(
            argv, capture_output=True, text=True, timeout=config.timeout, check=False
        )
    except subprocess.TimeoutExpired as expired:
        return ERROR, "", f"timed out after {config.timeout}s\n{expired.output or ''}"
    output = (result.stdout or "") + (result.stderr or "")
    outcome, signature = classify(result.returncode, output)
    if outcome == ERROR and needs_liveness_check(result.returncode, output):
        # `not connected` with a healthy server means the very first request was
        # refused, i.e. a denial. With an unhealthy server it means what it says.
        if superuser_can_reach(config, tokens):
            outcome, signature = DENY, "not connected, but the server answers the superuser"
        else:
            signature = "server unreachable for the superuser too"
    elif outcome == ERROR and looks_like_not_found(result.returncode, output):
        # A 404 can be an authorization failure hiding the object's existence.
        is_denial, why = resolve_not_found(config, tokens, output)
        signature = why
        if is_denial:
            outcome = DENY
    return outcome, signature, output


# ---------------------------------------------------------------------------
# the loop
# ---------------------------------------------------------------------------


class Result:
    def __init__(self, scenario, step, actual, signature, output):
        self.scenario = scenario
        self.step = step
        self.actual = actual
        self.signature = signature
        self.output = output

    @property
    def ok(self):
        return self.actual == self.step.expect

    @property
    def phase(self):
        return self.step.phase

    @property
    def identity(self):
        return self.step.identity

    @property
    def expected(self):
        return self.step.expect


class SetupResult:
    """A scenario whose assertion is about the management API refusing a
    configuration, so there is no workload run to report."""

    def __init__(self, scenario, ok, detail):
        self.scenario = scenario
        self.ok = ok
        self.detail = detail
        self.actual = "rejected" if ok else "accepted"

    phase = "setup"
    identity = "-"
    expected = "rejected"


def run_scenario(config, sidecar, tokens, scenario, grants=None, mode="rbac"):
    # scenario.mode is the sidecar's authorization mode; it means nothing in a
    # classic run, so it is only shown where it applies.
    label = f" [{scenario.mode}]" if mode == "rbac" else ""
    print(f"\n=== {scenario.name}{label} ===")
    print(f"    {scenario.summary}")

    # Fresh state, so a scenario cannot pass on data left by its predecessor.
    drop_database(config, tokens, config.database)

    if scenario.grants is not None:
        # Classic authorization: grants replace policy/role/binding entirely.
        ensure_user(config, tokens, scenario.user)
        # Classic grants only exist for objects that already exist: arangod
        # answers 404 "database not found" / "collection or view not found"
        # otherwise. 050_database and createCollectionSafe both cope with finding
        # things already there, so pre-creating does not weaken the scenario - it
        # moves the failure onto the grant being tested.
        if scenario.precreate_database or scenario.precreate_collections:
            create_database(config, tokens, config.database)
        for name in scenario.precreate_collections:
            create_collection(config, tokens, config.database, name)
        # Wipe any level left from a previous run before applying this one, or a
        # revoked grant would linger and quietly widen the scenario.
        grants.revoke(scenario.user, [(config.database, ""),
                                      (config.other_database, "")])
        grants.apply(scenario.user, scenario.grants)
        if not config.dry_run and scenario.grants:
            for path, level in scenario.grants:
                actual = grants.effective(scenario.user, path)
                print(f"    grant {path} = {level} (server reports {actual})")

    if scenario.policy is not None:
        ensure_user(config, tokens, scenario.user)
        sidecar.put_policy(scenario.policy_name, scenario.policy)
        sidecar.put_role(scenario.role_name, [scenario.policy_name])
        if scenario.bind:
            try:
                sidecar.put_binding(scenario.user, scenario.role_name, scenario.scope)
            except RuntimeError as error:
                if scenario.expect_setup_error is None:
                    raise
                # The refusal *is* the result being asserted.
                ok = scenario.expect_setup_error.lower() in str(error).lower()
                print(f"    setup rejected as expected: {error}"
                      if ok else
                      f"    setup rejected, but not with the expected message\n"
                      f"      wanted substring: {scenario.expect_setup_error}\n"
                      f"      got: {error}")
                sidecar.drop(scenario)
                remove_user(config, tokens, scenario.user)
                return [SetupResult(scenario, ok, str(error))]
            if scenario.expect_setup_error is not None:
                if config.dry_run:
                    # Nothing was really sent, so there is nothing to reject.
                    sidecar.drop(scenario)
                    remove_user(config, tokens, scenario.user)
                    return [SetupResult(scenario, True, "dry run")]
                message = (f"the API accepted a binding it was expected to reject "
                           f"({scenario.expect_setup_error})")
                print(f"    MISMATCH {message}")
                sidecar.drop(scenario)
                remove_user(config, tokens, scenario.user)
                return [SetupResult(scenario, False, message)]
        sidecar.refresh()
        wait_until_visible(config, sidecar, scenario)

    results = []
    for step in scenario.steps:
        token = tokens.for_identity(step.identity, scenario)
        actual, signature, output = run_phase(config, tokens, step.phase, token,
                                              assume=step.expect,
                                              suite_filter=scenario.test_filter)
        result = Result(scenario, step, actual, signature, output)
        results.append(result)
        verdict = "ok" if result.ok else "MISMATCH"
        detail = f" ({signature})" if signature else ""
        note = f"  # {step.note}" if step.note else ""
        print(
            f"    {step.phase:<10} as {step.identity:<9} "
            f"expected {step.expect:<4} got {actual:<5}{detail} -> {verdict}{note}"
        )
        if not result.ok:
            # Stop this scenario: later steps assume the earlier ones behaved.
            print(f"    ---- last 40 lines of {step.phase} output ----")
            for line in (output.strip().splitlines() or ["<no output>"])[-40:]:
                print(f"    | {line}")
            break

    sidecar.drop(scenario)
    if scenario.grants and grants is not None:
        grants.revoke(scenario.user, scenario.grants)
    remove_user(config, tokens, scenario.user)
    drop_database(config, tokens, config.database)
    return results


def denied_collection_for(suite_filter):
    """A collection the selected suites really create.

    The deny-precedence scenario is only meaningful if the workload actually
    touches the denied resource; otherwise it would pass vacuously.
    """
    if not suite_filter or "100" in suite_filter:
        return "cgeo_0"  # 100_collections.js: cgeo_${dbCount}
    if "400" in suite_filter:
        return "old_cview1_0"  # 400_views.js: old_cview1_${dbCount}
    return None


# Suites verified to attempt a write even when the objects they touch already
# exist, so a second makedata pass really does ask for a write permission.
# Established by measurement, not by reading: with `--test 050` the reader and
# developer scenarios both reported `pass`, because 050 creates databases, finds
# them present on the re-run and does nothing. Adding 400 made both deny.
WRITE_ON_RERUN_SUITES = ("100", "400")


def writes_on_rerun(suite_filter):
    """Whether the selected suites write on a re-run over existing data."""
    if not suite_filter:
        return True  # no filter means every suite, which includes writers
    return any(suite in suite_filter for suite in WRITE_ON_RERUN_SUITES)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--setup", action="store_true",
                        help="start arangod and the sidecar first "
                             "(via ../scripts). Without it, attach to a running stack.")
    parser.add_argument("--endpoint", default="tcp://127.0.0.1:8529",
                        help="arangod endpoint (default: %(default)s)")
    parser.add_argument("--management", default="http://127.0.0.1:8108",
                        help="sidecar management API base URL (default: %(default)s)")
    parser.add_argument("--integration", default="http://127.0.0.1:8108",
                        help="base URL serving authentication.v1 / authorization.v1. In "
                             "the current topology the sidecar serves these itself, so "
                             "this is the same address as --management and is also what "
                             "arangod's --server.external-rbac-service points at. "
                             "(default: %(default)s)")
    parser.add_argument("--database", default="system_rta_rbac",
                        help="database for the workload. Keep the system_ prefix: "
                             "makedata.js warns that other names may be dropped by "
                             "replication fuzzing. (default: %(default)s)")
    parser.add_argument("--test", default="050,100,400,500,580,607,612",
                        help="rta-makedata suite filter: databases; collections, indexes "
                             "and documents; views; graphs; analyzers. Every suite here "
                             "has been verified to run under both authorization models. "
                             "070/071 (Foxx) cannot run under RBAC at all and 700 (users) "
                             "needs a tenant-admin scenario first; see README.md. "
                             "(default: %(default)s)")
    parser.add_argument("--scenario", action="append", default=[],
                        help="run only these scenarios (repeatable)")
    parser.add_argument("--group", action="append", default=[],
                        help="run only these scenario groups (repeatable)")
    parser.add_argument("--list", action="store_true", help="list the catalog and exit")
    parser.add_argument("--self-check", action="store_true",
                        help="validate the catalog against arangod's RBAC vocabulary "
                             "and exit. Needs no server.")
    parser.add_argument("--dry-run", action="store_true",
                        help="print the RBAC calls and arangosh commands without "
                             "touching a server")
    parser.add_argument("--arangosh", default=None)
    parser.add_argument("--arangod", default=None,
                        help="arangod binary, used for RBAC capability detection and "
                             "by the setup scripts")
    parser.add_argument("--rta", default=None, help="path to the rta-makedata checkout")
    parser.add_argument("--work", default=None, help="runtime dir (default: $RBAC_WORK)")
    parser.add_argument("--jwt-secret-file", default=None)
    parser.add_argument("--phase-timeout", type=int, default=1800,
                        help="per-arangosh-run timeout in seconds. Generous on purpose: "
                             "rta-makedata's createSafe() retries a failing create 50 "
                             "times with a growing sleep before giving up, so a step "
                             "that is *meant* to be denied still takes ~2 minutes. "
                             "(default: %(default)s)")
    parser.add_argument("--propagation-timeout", type=int, default=90,
                        help="seconds to wait for a permission change to reach the PDP. "
                             "The documented worst case is ~30s. (default: %(default)s)")
    parser.add_argument("--bootstrap-user", default=None,
                        help="seed an allow-all policy/role/binding for this user and "
                             "exit. A real sidecar denies by default, so the account the "
                             "test harness itself drives arangod with has no access at "
                             "all until it is bound. This mirrors production, where the "
                             "operator binds `root` to managed:predefined:super-admin "
                             "when the deployment bootstraps.")
    parser.add_argument("--remove-bootstrap-user", default=None,
                        help="remove what --bootstrap-user created, and exit.")
    parser.add_argument("--external-sidecar", action="store_true",
                        help="the authorization sidecar is managed by someone else, so "
                             "do not start, stop or restart it. Required whenever the "
                             "sidecar was not brought up by --setup - restarting it "
                             "would replace a correctly configured service with one "
                             "built from this script's own defaults. Scenarios that need "
                             "a different sidecar authorization mode are skipped, since "
                             "changing it requires a restart.")
    parser.add_argument("--auth-mode", choices=["auto", "rbac", "classic"], default="auto",
                        help="which authorization model the target arangod uses. "
                             "`classic` is what you get without "
                             "--server.external-rbac-service, and is a supported "
                             "configuration, not a broken one - it selects the "
                             "grant-based catalog instead of the policy-based one. "
                             "`auto` probes the server. (default: %(default)s)")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    # A full matrix takes tens of minutes and is normally redirected to a log,
    # where block buffering would hide all progress until the very end.
    try:
        sys.stdout.reconfigure(line_buffering=True)
    except AttributeError:  # pragma: no cover - very old Python
        pass

    if args.self_check:
        import selfcheck
        return selfcheck.main()

    config = Config(args)

    # The RBAC model differs between arangod revisions, so read the shape out of
    # the source under test rather than assuming a branch. See
    # selfcheck.capabilities().
    import selfcheck
    caps = selfcheck.capabilities(binary=config.arangod)
    print(selfcheck.describe_capabilities(caps))

    denied = denied_collection_for(config.suite_filter)

    tokens = Tokens(config)
    if args.bootstrap_user or args.remove_bootstrap_user:
        return bootstrap_user(config, Sidecar(config, Tokens(config)),
                              Tokens(config),
                              args.bootstrap_user or args.remove_bootstrap_user,
                              remove=bool(args.remove_bootstrap_user))

    sidecar = Sidecar(config, tokens)
    grants = ClassicGrants(config, tokens)

    # The stack has to be up before the mode can be probed - the probe talks to
    # both arangod and the sidecar - so --setup happens here rather than further
    # down, next to the scenario loop.
    started_mode = None
    if args.setup and not args.list:
        print("=== bringing up the RBAC stack ===")
        # arangod's policy decision point is the sidecar's own HTTP gateway: the
        # sidecar serves authentication.v1 and authorization.v1 alongside the
        # management API, so there is no separate integration service here.
        run_script(config, "start_arangod.sh", config.integration_url)
        wait_for_arangod(config)
        if not config.external_sidecar:
            run_script(config, "start_sidecar.sh", "central")
            started_mode = "central"

    # Which authorization model is in force decides which catalog is meaningful.
    # `--list` must not need a server, so defer the probe until it is needed.
    if args.list:
        mode = config.requested_mode if config.requested_mode != "auto" else "rbac"
    elif config.requested_mode != "auto":
        mode = config.requested_mode
    else:
        mode = detect_auth_mode(config, sidecar, tokens)
    print(f"authorization model in force: {mode}"
          + ("" if config.requested_mode != "auto" else " (probed)"))

    if mode == "classic":
        all_scenarios = catalog.build_classic(
            config.database, config.other_database, denied or "cgeo_0")
        if denied is None:
            all_scenarios = [s for s in all_scenarios
                             if s.name != "classic-collection-denied"]
            print("note: skipping classic-collection-denied - the suite filter "
                  f"'{config.suite_filter}' creates no collection this script knows "
                  "how to target.")
    else:
        all_scenarios = catalog.build(
            config.database, config.other_database, denied or "cgeo_0",
            api_version_gate=caps["api_version_gate"],
        )
        if denied is None:
            all_scenarios = [s for s in all_scenarios
                             if s.name != "admin-with-explicit-deny"]
            print("note: skipping admin-with-explicit-deny - the suite filter "
                  f"'{config.suite_filter}' creates no collection this script knows "
                  "how to target.")

    if not writes_on_rerun(config.suite_filter):
        vacuous = [s for s in all_scenarios if s.needs_write_on_rerun]
        all_scenarios = [s for s in all_scenarios if not s.needs_write_on_rerun]
        for scenario in vacuous:
            print(f"note: skipping '{scenario.name}' - its denial is only "
                  f"meaningful if the workload attempts a write on the second "
                  f"makedata pass, and the filter '{config.suite_filter}' "
                  f"contains none of {', '.join(WRITE_ON_RERUN_SUITES)}. It "
                  f"would report 'pass' and mean nothing by it.")

    selected = all_scenarios
    if args.scenario:
        selected = [s for s in selected if s.name in args.scenario]
    if args.group:
        selected = [s for s in selected if s.group in args.group]
    if not args.scenario and not args.group:
        # Opt-in groups. `known-issue` asserts a defect; `role-modelling`
        # demonstrates a consequence of the role catalog as written rather than a
        # pass/deny decision. Both expect the workload to break, so leaving them
        # in the default set would make a healthy run look like a failure.
        opt_in = ("known-issue", "role-modelling")
        excluded = [s for s in selected if s.group in opt_in]
        selected = [s for s in selected if s.group not in opt_in]
        for scenario in excluded:
            print(f"note: skipping '{scenario.name}' "
                  f"(opt in with --group {scenario.group})")
    if config.external_sidecar:
        # Their expectation depends on the sidecar running in a particular
        # authorization mode, and that is fixed at its startup.
        needs_restart = [s for s in selected if s.mode != "central"]
        selected = [s for s in selected if s.mode == "central"]
        for scenario in needs_restart:
            print(f"note: skipping '{scenario.name}' - it needs the sidecar in "
                  f"{scenario.mode} mode, which cannot be arranged for an "
                  f"externally managed sidecar")

    if not selected:
        print("no scenarios selected", file=sys.stderr)
        return 2

    if args.list:
        for scenario in all_scenarios:
            marker = "*" if scenario in selected else " "
            print(f"{marker} {scenario.name:<32} [{scenario.group}/{scenario.mode}] "
                  f"{scenario.summary}")
            for step in scenario.steps:
                print(f"      {step.phase:<10} as {step.identity:<9} -> {step.expect}")
            if scenario.expect_note:
                print(f"      note: {scenario.expect_note}")
        return 0

    # Group by required mode so the sidecar is restarted as rarely
    # as possible - its mode is fixed at startup and cannot change at runtime.
    selected = sorted(selected, key=lambda s: (s.mode != "central", s.mode))

    # `started_mode` is whatever --setup already brought up, so the loop below
    # does not restart the sidecar just to put it into the mode it is in.
    current_mode = started_mode
    results = []
    try:
        for scenario in selected:
            if (mode == "rbac" and not config.external_sidecar
                    and scenario.mode != current_mode):
                # The mode is fixed at sidecar startup (--sidecar.auth.mode) and
                # cannot be changed at runtime, so switching means a restart.
                print(f"\n=== sidecar -> {scenario.mode} ===")
                run_script(config, "start_sidecar.sh", scenario.mode)
                # The streaming cache has to reconnect and resync before the
                # first decision is trustworthy.
                if not config.dry_run:
                    time.sleep(8)
                current_mode = scenario.mode
            results.extend(run_scenario(config, sidecar, tokens, scenario, grants, mode))
    except RuntimeError as error:
        print(f"\nABORTED: {error}", file=sys.stderr)
        return 3

    print("\n" + "=" * 78)
    print(f"{'scenario':<32} {'phase':<10} {'as':<10} {'want':<8} {'got':<8} verdict")
    print("-" * 78)
    failures = 0
    for result in results:
        if not result.ok:
            failures += 1
        print(
            f"{result.scenario.name:<32} {result.phase:<10} "
            f"{result.identity:<10} {result.expected:<8} "
            f"{result.actual:<8} {'ok' if result.ok else 'MISMATCH'}"
        )
    print("-" * 78)
    print(f"{len(results)} step(s), {failures} mismatch(es)")
    if config.dry_run:
        print("\n(dry run: nothing was executed. Each step echoes its expectation, so "
              "'0 mismatches' says the plumbing is wired up, not that RBAC behaves.)")
        return 0
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
