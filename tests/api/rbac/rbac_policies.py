#!/usr/bin/env python3
"""
Translate the classic grants applied by apitester `setup` into RBAC policies.

For each of the 68 test users it reads their EFFECTIVE access levels from arangod
(GET /_api/user/<u>/database/<...>, which resolves classic precedence incl. the
_system->other-database cascade), then creates one policy / role / binding in the
sidecar granting the equivalent RBAC actions on the concrete resources arangod
actually checks.

Prereq: apitester-rbac.js `setup` has run against the RBAC arangod (so users +
classic grants + fixtures exist) and the sidecar (:8108) is up.

Env overrides:
  RBAC_ARANGO       arangod base URL          (default http://127.0.0.1:8529)
  RBAC_SIDECAR      sidecar mgmt base URL     (default http://127.0.0.1:8108)
  RBAC_JWT_SECRET_FILE  path to the 64-byte JWT secret ("-" file)
"""
import json, os, hmac, hashlib, base64, urllib.request, urllib.error

ARANGO = os.environ.get("RBAC_ARANGO", "http://127.0.0.1:8529")
MGMT   = os.environ.get("RBAC_SIDECAR", "http://127.0.0.1:8108") + "/_management/permissions"
SECRET = open(os.environ.get("RBAC_JWT_SECRET_FILE", "/tmp/arangodb-rbac-test/jwt/-"), "rb").read()

def b64u(b): return base64.urlsafe_b64encode(b).rstrip(b"=").decode()
def superuser_jwt():
    h = b64u(json.dumps({"alg":"HS256","typ":"JWT"},separators=(",",":")).encode())
    p = b64u(json.dumps({"iss":"arangodb","server_id":"foo"},separators=(",",":")).encode())
    sig = b64u(hmac.new(SECRET, f"{h}.{p}".encode(), hashlib.sha256).digest())
    return f"{h}.{p}.{sig}"
SU = superuser_jwt()

def req(url, method="GET", body=None):
    data = json.dumps(body).encode() if body is not None else None
    r = urllib.request.Request(url, data=data, method=method,
        headers={"authorization": f"bearer {SU}", "content-type": "application/json"})
    try:
        with urllib.request.urlopen(r) as resp:
            return resp.status, json.loads(resp.read() or "{}")
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read() or "{}")

def eff_level(user, path):
    # GET /_api/user/<u>/database/<path> -> effective level ("rw"/"ro"/"none"/"undefined")
    st, b = req(f"{ARANGO}/_api/user/{user}/database/{path}")
    return b.get("result", "undefined") if st == 200 else "undefined"

# ---- level -> RBAC actions -------------------------------------------------
DATA_RW = ["db:Read","db:WriteData","db:WriteMeta","db:Create","db:Drop","db:UseApiVersion"]
DATA_RO = ["db:Read","db:UseApiVersion"]
ADMIN_ALL = ["db:AdminMoveShards","db:AdminMonitoring","db:AdminMonitoringInternal",
  "db:AdminCompaction","db:AdminAuthReload","db:AdminCrashHandler","db:AdminApiCalls",
  "db:AdminAqlQueries","db:AdminShutdown","db:AdminReadLogs","db:AdminSetLogLevel",
  "db:AdminOptions","db:AdminSupervisionState","db:AdminRemoveServer","db:AdminClusterInfo",
  "db:AdminMaintenance","db:AdminRebalance","db:AdminLicense","db:AdminBackup","db:AdminJobs",
  "db:AdminReadReplicatedLog","db:AdminWriteReplicatedLog","db:AdminDump","db:AdminRestore",
  "db:AdminWalAccess","db:AdminReadAgency","db:AdminReadOnlyMode","db:AdminReadAqlFunctions",
  "db:AdminWriteAqlFunctions","db:AdminQueryCache"]
ADMIN_RO = [a for a in ADMIN_ALL if "Read" in a or "Monitoring" in a]

def data_actions(level):
    return DATA_RW if level == "rw" else DATA_RO if level == "ro" else None

def stmt(actions, resources):
    return {"effect":"Allow","actions":actions,"resources":resources}

def build_statements(user, is_admin):
    S = []
    # _system access (admin users) AND its cascade to database d is already
    # reflected by arangod's eff_level() below (databaseAuthLevel falls back to
    # the _system grant). We grant the _system resources here and let the
    # d/c/e/wildcard grants below pick up the cascade uniformly.
    syslvl = eff_level(user, "_system")
    if data_actions(syslvl):
        S.append(stmt(data_actions(syslvl), ["db:database:_system","db:collection:_system:*"]))
    if is_admin:
        # server-admin capabilities are keyed off the _system access level
        if syslvl == "rw":
            S.append(stmt(ADMIN_ALL, ["*"]))
        elif syslvl == "ro":
            S.append(stmt(ADMIN_RO, ["*"]))
    # levels on d, its wildcard collections, c and e (cascade-aware)
    d   = eff_level(user, "d")
    wc  = eff_level(user, "d/__wildcard_probe__")   # collection w/o specific grant => max(db, d/*)
    c   = eff_level(user, "d/c")
    e   = eff_level(user, "d/e")
    if data_actions(d):
        S.append(stmt(data_actions(d), ["db:database:d"]))
    # wildcard collections in d, plus graph/view/analyzer resources (which RBAC
    # treats as separate resource types; classic derives them from collections).
    wc_act = set(data_actions(wc) or [])
    if wc_act:
        S.append(stmt(sorted(wc_act), [
            "db:collection:d:*", "db:graph:d:*", "db:view:d:*", "db:analyzer:d:*",
        ]))
    # specific collections c and e: RBAC is any-allow-wins with no specific>wildcard
    # precedence, so where a specific collection is MORE restrictive than the
    # wildcard we must add an explicit Deny to override the wildcard Allow.
    for coll, lvl in [("c", c), ("e", e)]:
        R = f"db:collection:d:{coll}"
        act = set(data_actions(lvl) or [])
        if act:
            S.append(stmt(sorted(act), [R]))
        deny = wc_act - act
        if deny:
            S.append({"effect":"Deny","actions":sorted(deny),"resources":[R]})
    return S

def upsert_user_rbac(user, is_admin):
    stmts = build_statements(user, is_admin)
    pol, role = f"p_{user}", f"r_{user}"
    req(f"{MGMT}/policy/{pol}", "DELETE")
    req(f"{MGMT}/policy/{pol}", "POST", {"item":{"statements":stmts}})
    req(f"{MGMT}/role/{role}", "DELETE")
    req(f"{MGMT}/role/{role}", "POST", {"item":{"policies":[pol]}})
    req(f"{MGMT}/user/{user}/role/{role}", "DELETE")
    req(f"{MGMT}/user/{user}/role/{role}", "POST",
        {"scope":{"statements":[{"effect":"Allow","actions":["*"],"resources":["*"]}]}})

def main():
    LEVELS = ["U","N","R","W"]
    users = [f"{a}{b}{c}" for a in LEVELS for b in LEVELS for c in LEVELS]
    admins = ["AU","AN","AR","AW"]
    n = 0
    for u in users:
        upsert_user_rbac(u, False); n += 1
        if n % 16 == 0: print(f"  ...{n} users")
    for u in admins:
        upsert_user_rbac(u, True)
    req(f"{MGMT}/refresh", "POST", {})
    print(f"created RBAC policies for {len(users)+len(admins)} users; refresh triggered")

if __name__ == "__main__":
    main()
