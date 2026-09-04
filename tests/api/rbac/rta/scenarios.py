#!/usr/bin/env python3
"""RBAC scenarios to run rta-makedata against.

*AI generated docs*

Each scenario is an RBAC *configuration* (a policy, a role, and a user role
binding carrying a scope) plus the outcome we expect when the unmodified
rta-makedata workload is run under it. rta-makedata is the payload; RBAC is the
environment being varied. Nothing in 3rdParty/rta-makedata is modified.

WHY THIS SHAPE
==============
makedata/checkdata/cleardata already touch nearly every CoreDB resource type
and every access level: they create and drop databases, collections, indexes,
views, analyzers and graphs, write and read documents, and run AQL. That makes
them a far broader probe of arangod's RBAC enforcement than any hand-written
list of endpoint checks - and, unlike such a list, it stays in step with the
product because the workload is maintained elsewhere.

So instead of testing the RBAC APIs directly, we ask: given permission set X,
does the workload behave as the role definition says it should?

ROLE MODELLING
==============
The action sets below are taken verbatim from the CoreDB half of
documents/DesignDocuments/03_IN_PROGRESS/RbacPredefinedRolesInformationFlow.md,
which specifies `coredb-reader` as

    Allow db:UseApiVersion on "*"
    Allow db:Read on db:database:*, db:collection:*:*, db:view:*:*,
                    db:analyzer:*:*, db:graph:*:*

We deliberately do NOT bind the real `managed:predefined:coredb-*` roles.
kube-arangodb design/rbac/predefined_roles.md is explicit that in the MVP only
`super-admin` ships a bundled policy and the rest are empty containers, so
binding them would grant nothing and every scenario would fail identically.
Emulating the documented policies instead means these scenarios test the role
*definitions* - if the intended action set is wrong or incomplete, that shows
up here as a workload failure, which is the point.

Per the same document, the two halves divide as: the policy restricts ACTIONS
(its resources are wildcards), the binding scope restricts RESOURCES (its
actions are `*`). Access needs both to agree.
"""

# ---------------------------------------------------------------------------
# action sets
# ---------------------------------------------------------------------------

# Gate action checked on EVERY authenticated request before the handler is
# reached (RestHandler::handleAuthorizationChecks -> checkApiVersionAccess).
# A policy or scope that omits it denies literally everything, which is the
# single easiest RBAC misconfiguration to make - hence a scenario for it.
API_VERSION = ["db:UseApiVersion"]

READ = ["db:Read"]
WRITE = ["db:WriteData", "db:WriteMeta"]
LIFECYCLE = ["db:Create", "db:Drop"]

# Required to read the server version, and therefore by any arangosh-based
# tooling: on a hardened server `/_api/version` puts its `version` field behind
# canUseHardenedAction(AdminMonitoringInternal) - see RestVersionHandler.cpp
# getVersion(), where result.add("version", ...) sits inside `if (allowInfo)`.
# RBAC *requires* --server.harden (ExecContext.h:160 asserts it), so under RBAC
# this is always needed; classic only gates it when started hardened, and then
# wants rw on _system instead. This is intended behaviour, not a defect.
#
# Worth noting for the role catalog: it is NOT part of any documented coredb-*
# action set, so a user holding only those roles cannot run tooling that reads
# the version - rta-makedata's 100_collections.js gates itself on
# semver.coerce(db._version()) and dies before any permission decision.
MONITORING = ["db:AdminMonitoringInternal"]

# The five CoreDB resource types a scope can be expressed over, per the
# information-flow document. Wildcarded here because the policy constrains
# actions only.
ALL_RESOURCE_TYPES = [
    "db:database:*",
    "db:collection:*:*",
    "db:view:*:*",
    "db:analyzer:*:*",
    "db:graph:*:*",
]


def allow(actions, resources):
    return {"effect": "Allow", "actions": list(actions), "resources": list(resources)}


def deny(actions, resources):
    return {"effect": "Deny", "actions": list(actions), "resources": list(resources)}


def coredb_policy(*action_groups, api_version=True, monitoring=True):
    """A predefined-role-shaped policy: wildcard resources, specific actions.

    `monitoring` adds db:AdminMonitoringInternal, which the documented coredb-*
    sets do not include but any version-reading client needs. Admin actions carry
    no resource, so it goes on `*` - see MONITORING above.
    """
    statements = []
    if api_version:
        statements.append(allow(API_VERSION, ["*"]))
    if monitoring:
        statements.append(allow(MONITORING, ["*"]))
    actions = [action for group in action_groups for action in group]
    if actions:
        statements.append(allow(sorted(actions), ALL_RESOURCE_TYPES))
    return statements


READER_POLICY = coredb_policy(READ)
DEVELOPER_POLICY = coredb_policy(READ, WRITE)
ADMIN_POLICY = coredb_policy(READ, WRITE, LIFECYCLE)

# Same as ADMIN_POLICY but missing the API-version gate.
ADMIN_POLICY_NO_API_VERSION = coredb_policy(READ, WRITE, LIFECYCLE, api_version=False)

# The documented coredb-admin set exactly as written, with no monitoring action.
# Used to show what a user holding only the documented role can actually do.
ADMIN_POLICY_AS_DOCUMENTED = coredb_policy(READ, WRITE, LIFECYCLE, monitoring=False)


# The `db:apiversion:v<n>` resource only exists in arangod builds that have the
# API-version gate (resources::ApiVersion in Auth/Rbac/Actions.h). Older builds
# - including feature/rbac-api-tester - carry the `UseApiVersion` action but
# never check it, and have no such resource, so a scope entry for it would just
# never match. The runner detects which it is and passes the flag down.
API_VERSION_SCOPE = "db:apiversion:*"

# An admin action carries NO resource - resourceToWireString() maps NoResource to
# the empty string - and only the bare `*` pattern matches that. So a scope has
# to permit admin actions in a statement of their own, on `*`, or they can never
# be granted however generous the policy is.
#
# The scope example in RbacPredefinedRolesInformationFlow.md
# (actions ["*"], resources ["db:database:xyz", "db:collection:xyz:*"]) therefore
# cannot grant any admin action at all. Keeping the admin allowance in a separate
# statement preserves resource scoping for every data action, which is the point
# of scoping - putting `*` in the main statement would silently unscope
# everything.
def admin_scope_statement():
    return allow(MONITORING, ["*"])


def scope_for(database, api_version_gate=True):
    """An in-scope binding: wildcard actions, resources pinned to `database`.

    `db:database:_system` is required even when the workload targets another
    database: arangosh's connect handshake and makedata's own startup probes
    (`/_api/version`, `/_admin/server/role`) carry no `/_db/` prefix, so
    RestHandler::checkDatabaseAccess evaluates db:Read on `db:database:_system`
    for them. 050_database.js additionally does `_useDatabase('_system')` and
    `_databases()` before creating the target database. Without this entry every
    scenario fails during startup instead of at the interesting point.
    """
    resources = [
        "db:database:_system",
        "db:collection:_system:*",
        f"db:database:{database}",
        f"db:collection:{database}:*",
        f"db:view:{database}:*",
        f"db:analyzer:{database}:*",
        f"db:graph:{database}:*",
    ]
    if api_version_gate:
        resources.insert(0, API_VERSION_SCOPE)
    return [allow(["*"], resources), admin_scope_statement()]


def scope_elsewhere(other_database, api_version_gate=True):
    """A well-formed scope that simply does not cover the target database."""
    resources = [
        "db:database:_system",
        "db:collection:_system:*",
        f"db:database:{other_database}",
        f"db:collection:{other_database}:*",
    ]
    if api_version_gate:
        resources.insert(0, API_VERSION_SCOPE)
    # The admin allowance is deliberately present here too: the out-of-scope
    # scenario must differ from the in-scope one *only* in its data resources, or
    # a denial could be blamed on the missing monitoring action instead.
    return [allow(["*"], resources), admin_scope_statement()]


# ---------------------------------------------------------------------------
# model
# ---------------------------------------------------------------------------

# Identities a step can run as.
SUPERUSER = "superuser"  # no user claim -> sidecar treats it as operator-internal
SCENARIO = "scenario"  # the scenario's own user, subject to its RBAC config

# Expected outcomes.
PASS = "pass"  # arangosh exits 0
DENY = "deny"  # arangosh exits non-zero AND the output shows a permission denial
# The run failed without reaching a permission decision at all. Only ever an
# *expected* outcome for a scenario that documents a known defect; anywhere else
# it means the test itself is broken.
ERROR = "error"

PHASES = ("makedata", "checkdata", "cleardata")


class Step:
    """One rta-makedata invocation and its expected outcome."""

    def __init__(self, phase, identity, expect, note=""):
        assert phase in PHASES, phase
        assert identity in (SUPERUSER, SCENARIO), identity
        assert expect in (PASS, DENY, ERROR), expect
        self.phase = phase
        self.identity = identity
        self.expect = expect
        self.note = note

    def __repr__(self):
        return f"Step({self.phase}, {self.identity}, {self.expect})"


class Scenario:
    """An RBAC configuration plus the workload outcomes it should produce."""

    def __init__(
        self,
        name,
        summary,
        steps,
        policy=None,
        scope=None,
        bind=True,
        mode="central",
        group="core",
        expect_note="",
        expect_setup_error=None,
        test_filter=None,
        grants=None,
        precreate_database=False,
        precreate_collections=(),
        needs_write_on_rerun=False,
    ):
        self.name = name
        self.summary = summary
        self.steps = steps
        # `policy is None` means "create no policy/role at all".
        self.policy = policy
        # `scope is None` with bind=True creates a binding whose scope is absent,
        # which the evaluator skips entirely (see kube-arangodb
        # client/scope.go: a nil scope contributes nothing).
        self.scope = scope
        self.bind = bind
        # Required integration-service mode. Scenarios are grouped by mode so the
        # runner restarts that service as few times as possible.
        self.mode = mode
        self.group = group
        self.expect_note = expect_note
        # When set, seeding this scenario's RBAC objects is *expected* to fail,
        # and the substring the error has to contain. The scenario passes when
        # the API refuses, and fails when it accepts.
        self.expect_setup_error = expect_setup_error
        # Overrides the run-wide --test filter. Used by scenarios that need a
        # particular suite to be present to demonstrate anything.
        self.test_filter = test_filter
        # A scenario whose denial only shows up on a *second* makedata pass over
        # data a superuser already created needs the selected suites to attempt
        # a write even when their objects exist. Suite 050 does not: it creates
        # databases, finds them present on the re-run and does nothing, so such a
        # scenario would report `pass` and mean nothing by it. The runner skips
        # these, with a note, when the filter cannot make them meaningful.
        self.needs_write_on_rerun = needs_write_on_rerun
        # Classic-authentication scenarios carry `_users` grants instead of an
        # RBAC policy: a list of (path, level) where path is "<db>" or
        # "<db>/<collection>" and level is rw / ro / none. Mutually exclusive
        # with `policy`.
        self.grants = grants
        # Classic grants are evaluated against objects that must already exist,
        # and the runner drops the target database for a fresh start. Without
        # pre-creation a deny scenario fails while *creating* the database - which
        # in classic needs `_system` rw - so every one of them would be refused
        # for the same uninteresting reason instead of for its own grant.
        self.precreate_database = precreate_database
        self.precreate_collections = tuple(precreate_collections)

    @property
    def user(self):
        return f"rta_{self.name.replace('-', '_')}"

    @property
    def policy_name(self):
        return f"rta_pol_{self.name}"

    @property
    def role_name(self):
        return f"rta_role_{self.name}"

    def __repr__(self):
        return f"Scenario({self.name})"


# ---------------------------------------------------------------------------
# catalog
# ---------------------------------------------------------------------------


def build(database, other_database, denied_collection, api_version_gate=True):
    """Build the scenario catalog for a given target database.

    `denied_collection` must be a collection that the selected makedata suites
    really create, otherwise the deny-precedence scenario would pass for the
    wrong reason (nothing ever touches the denied resource). The runner derives
    it from the suite filter.

    `api_version_gate` says whether the arangod under test enforces
    `db:UseApiVersion` against a `db:apiversion:v<n>` resource. It does on
    `devel`; it does not on `feature/rbac-api-tester`, which has the action but
    no resource type and no call site. When absent, the scope entry is dropped
    (it could never match) and the scenario that exists purely to guard the gate
    is left out of the catalog rather than being asserted against a build that
    has no gate to guard. The runner detects this from the arangod source.
    """
    in_scope = scope_for(database, api_version_gate)

    scenarios = [
        # -- control -------------------------------------------------------
        # Not an RBAC test: proves the stack and the workload are healthy, so a
        # failure below can be attributed to the permission set rather than to
        # a broken deployment. The superuser identity bypasses evaluation.
        Scenario(
            name="superuser-control",
            summary="superuser (RBAC bypassed) runs the full workload",
            policy=None,
            scope=None,
            bind=False,
            steps=[
                Step("makedata", SUPERUSER, PASS),
                Step("checkdata", SUPERUSER, PASS),
                Step("cleardata", SUPERUSER, PASS),
            ],
            group="control",
        ),
        # -- the positive case ---------------------------------------------
        Scenario(
            name="coredb-admin-in-scope",
            summary="coredb-admin action set, scope covering the target database",
            policy=ADMIN_POLICY,
            scope=in_scope,
            steps=[
                Step("makedata", SCENARIO, PASS),
                Step("checkdata", SCENARIO, PASS),
                Step("cleardata", SCENARIO, PASS),
            ],
            expect_note=(
                "The documented coredb-admin action set must be sufficient for the "
                "whole workload. A failure here means the role definition is "
                "missing an action the product actually checks."
            ),
        ),
        # -- read-only -----------------------------------------------------
        Scenario(
            name="coredb-reader-in-scope",
            summary="coredb-reader action set: may verify data, may not create it",
            policy=READER_POLICY,
            scope=in_scope,
            steps=[
                # Populate as superuser so there is something to read.
                Step("makedata", SUPERUSER, PASS, note="fixture"),
                Step("checkdata", SCENARIO, PASS, note="read-only workload succeeds"),
                Step("makedata", SCENARIO, DENY, note="creating is refused"),
                Step("cleardata", SUPERUSER, PASS, note="teardown"),
            ],
            expect_note=(
                "The discriminating scenario for the reader role: the same user "
                "passes the read-only phase and is refused the writing phase."
            ),
            needs_write_on_rerun=True,
        ),
        # -- write but no lifecycle ---------------------------------------
        Scenario(
            name="coredb-developer-in-scope",
            summary="coredb-developer action set: no db:Create, so makedata cannot build its fixtures",
            policy=DEVELOPER_POLICY,
            scope=in_scope,
            steps=[
                Step("makedata", SUPERUSER, PASS, note="fixture"),
                Step("checkdata", SCENARIO, PASS),
                Step("makedata", SCENARIO, DENY, note="db:Create is absent"),
                Step("cleardata", SUPERUSER, PASS, note="teardown"),
            ],
            expect_note=(
                "LIMITATION: makedata has no write-without-create phase, so this "
                "cannot separate developer from reader positively - both pass "
                "checkdata and fail makedata. It does establish that the "
                "developer action set alone is insufficient for the workload, "
                "i.e. that db:Create is genuinely required and enforced."
            ),
            needs_write_on_rerun=True,
        ),
        # -- scope is the boundary ----------------------------------------
        # Identical policy to coredb-admin-in-scope, which passes everything.
        # Only the scope differs, so any denial here is attributable to scoping.
        Scenario(
            name="coredb-admin-out-of-scope",
            summary="coredb-admin action set, scope pinned to a different database",
            policy=ADMIN_POLICY,
            scope=scope_elsewhere(other_database, api_version_gate),
            steps=[Step("makedata", SCENARIO, DENY)],
            expect_note=(
                "Same policy as coredb-admin-in-scope. The scope, and only the "
                "scope, must make the difference - this is the MVP's central "
                "claim that customers control access by scoping a fixed role."
            ),
        ),
        # -- default deny --------------------------------------------------
        Scenario(
            name="no-binding",
            summary="policy and role exist but are never bound to the user",
            policy=ADMIN_POLICY,
            scope=None,
            bind=False,
            steps=[Step("makedata", SCENARIO, DENY)],
            expect_note="A role grants nothing until it is bound (default deny).",
        ),
        Scenario(
            name="binding-without-scope",
            summary="the management API refuses to create a binding with no scope",
            policy=ADMIN_POLICY,
            scope=None,
            bind=True,
            steps=[],
            expect_setup_error="Scope cannot be empty",
            expect_note=(
                "A scope is what turns a resource-agnostic role into a concrete "
                "grant, and kube-arangodb client/scope.go skips a group whose "
                "scope is nil - so a scopeless binding would grant nothing. This "
                "scenario originally asserted that silent behaviour; the API "
                "turns out to reject the binding outright at creation time, "
                "which is the better guarantee, so that is what is asserted. "
                "Runs no workload: there is nothing to run under a binding that "
                "cannot exist."
            ),
        ),
        # -- the API-version footgun --------------------------------------
        Scenario(
            name="admin-without-api-version",
            summary="full CoreDB action set but db:UseApiVersion omitted",
            policy=ADMIN_POLICY_NO_API_VERSION,
            scope=in_scope,
            steps=[Step("makedata", SCENARIO, DENY)],
            expect_note=(
                "Every authenticated request is gated on db:UseApiVersion before "
                "the handler runs, so omitting it denies everything however "
                "generous the rest of the policy is. Guards the gate against "
                "being silently dropped."
            ),
        ),
        # -- deny precedence -----------------------------------------------
        Scenario(
            name="admin-with-explicit-deny",
            summary="coredb-admin action set plus an explicit Deny on one collection",
            policy=ADMIN_POLICY
            + [deny(WRITE + LIFECYCLE, [f"db:collection:{database}:{denied_collection}"])],
            scope=in_scope,
            steps=[Step("makedata", SCENARIO, DENY)],
            expect_note=(
                f"Deny must beat Allow within the same policy. The workload "
                f"creates {denied_collection}, so it has to fail there and "
                f"nowhere earlier."
            ),
        ),
        # -- what the documented role set alone can do ----------------------
        # Not a defect: on a hardened server the `version` field of
        # /_api/version sits behind AdminMonitoringInternal, by design. RBAC
        # forces hardening, so a user holding *only* the documented coredb-admin
        # action set cannot read the version - and any arangosh-based tooling that
        # does breaks before reaching a permission decision. Kept as a scenario
        # because it is a real consequence of the role catalog as written, and it
        # is the reason MONITORING is added to the other scenarios.
        Scenario(
            name="documented-admin-set-only",
            summary="the documented coredb-admin set alone cannot run version-reading tooling",
            policy=ADMIN_POLICY_AS_DOCUMENTED,
            scope=in_scope,
            group="role-modelling",
            # 100_collections.js gates itself on semver.coerce(db._version()).
            test_filter="050,100,400",
            steps=[Step("makedata", SCENARIO, ERROR,
                        note="semver TypeError - the client breaks, no denial is reached")],
            expect_note=(
                "Intended behaviour, confirmed against RestVersionHandler.cpp "
                "getVersion() and ExecContext.h:160: `version` is inside "
                "`if (allowInfo)`, and canUseHardenedAction returns early unless "
                "the server is hardened. Under RBAC hardening is mandatory, so "
                "db:AdminMonitoringInternal is always required; under classic the "
                "equivalent is rw on _system, and only when started hardened. "
                "The point of the scenario is that the documented coredb-* sets "
                "do not include that action, so the other scenarios add it "
                "explicitly (see MONITORING) - otherwise none of them could run "
                "a version-dependent suite. Outcome is `error` rather than `deny` "
                "because the client dies before any permission decision."
            ),
        ),
        # -- permissive mode ----------------------------------------------
        # Same insufficient config as coredb-reader-in-scope, but the policy
        # decision point is asked to log denials instead of enforcing them.
        Scenario(
            name="reader-permissive-mode",
            summary="reader action set under central-permissive: denials are logged, not enforced",
            policy=READER_POLICY,
            scope=in_scope,
            mode="central-permissive",
            steps=[
                Step("makedata", SCENARIO, PASS),
                Step("cleardata", SCENARIO, PASS),
            ],
            group="mode",
            expect_note=(
                "The recommended rollout path: verify policies without blocking "
                "traffic. A permission set that is refused under `central` must "
                "succeed here, otherwise permissive mode is not safe to roll out "
                "behind. Check the sidecar log for the overridden "
                "denials."
            ),
        ),
    ]

    if not api_version_gate:
        # There is no gate on this build, so a policy that omits db:UseApiVersion
        # is simply equivalent to one that includes it. Asserting a denial would
        # be asserting a behaviour the build does not have.
        scenarios = [s for s in scenarios if s.name != "admin-without-api-version"]

    return scenarios


# ---------------------------------------------------------------------------
# classic authentication catalog
# ---------------------------------------------------------------------------

# Running arangod *without* --server.external-rbac-service is a supported
# configuration, not a misconfiguration: authorization falls back to the classic
# `_users` grant model. That path has to keep working, both because it is the
# default and because it is the regression baseline the RBAC work must not
# break, so the same workload is driven through it with grants in place of
# policies.
#
# Two differences from the RBAC catalog are worth knowing:
#   - Classic has no notion of a scope. The database grant *is* the boundary, so
#     "out of scope" becomes "granted on a different database".
#   - Reading the server version needs an admin permission on a *hardened*
#     server: rw on _system in classic, db:AdminMonitoringInternal under RBAC.
#     start_arangod_classic.sh does not harden, so classic keeps the `version`
#     field for every user; RBAC always hardens, so the RBAC scenarios grant the
#     monitoring action explicitly (see MONITORING). Both catalogs therefore run
#     the same suites. A hardened classic server is not covered by either.

GRANT_RW = "rw"
GRANT_RO = "ro"
GRANT_NONE = "none"

# The classic counterpart of the `db:database:_system` entry in scope_for():
# arangosh's connect handshake and makedata's startup probes carry no `/_db/`
# prefix, so classic evaluates them against `_system`. Without at least read
# access there the user cannot connect at all, and every scenario is denied
# during startup - which makes the deny scenarios pass for the wrong reason and
# the positive ones fail for one that has nothing to do with what they test.
SYSTEM_RO = ("_system", GRANT_RO)
# Creating and dropping a database is gated on `_system` **rw** in the classic
# model - unlike RBAC, where it maps to db:Create/db:Drop on the database
# resource itself. A classic user who is to run the full make/clear cycle
# therefore needs this, which is a genuine difference between the two models
# rather than a quirk of the harness.
SYSTEM_RW = ("_system", GRANT_RW)


def build_classic(database, other_database, denied_collection):
    """The classic-authentication counterpart of build()."""
    return [
        Scenario(
            name="classic-superuser-control",
            summary="superuser runs the full workload with classic authentication",
            grants=None,
            policy=None,
            bind=False,
            group="control",
            steps=[
                Step("makedata", SUPERUSER, PASS),
                Step("checkdata", SUPERUSER, PASS),
                Step("cleardata", SUPERUSER, PASS),
            ],
        ),
        Scenario(
            name="classic-rw",
            summary="rw on the database: the whole workload should run",
            grants=[SYSTEM_RW, (database, GRANT_RW)],
            steps=[
                Step("makedata", SCENARIO, PASS),
                Step("checkdata", SCENARIO, PASS),
                Step("cleardata", SCENARIO, PASS),
            ],
            expect_note=(
                "The classic equivalent of coredb-admin-in-scope, and the "
                "baseline the RBAC positive case is measured against."
            ),
        ),
        Scenario(
            name="classic-ro",
            summary="ro on the database: may verify data, may not create it",
            grants=[SYSTEM_RO, (database, GRANT_RO)],
            steps=[
                Step("makedata", SUPERUSER, PASS, note="fixture"),
                Step("checkdata", SCENARIO, PASS, note="read-only workload succeeds"),
                Step("makedata", SCENARIO, DENY, note="creating is refused"),
                Step("cleardata", SUPERUSER, PASS, note="teardown"),
            ],
            expect_note="The classic equivalent of coredb-reader-in-scope.",
            needs_write_on_rerun=True,
        ),
        Scenario(
            name="classic-none",
            summary="an explicit `none` grant on the database",
            grants=[SYSTEM_RO, (database, GRANT_NONE)],
            steps=[Step("makedata", SCENARIO, DENY)],
            precreate_database=True,
        ),
        Scenario(
            name="classic-no-grant",
            summary="no grant on the target database - classic default deny",
            grants=[SYSTEM_RO],
            steps=[Step("makedata", SCENARIO, DENY)],
            expect_note=(
                "Distinct from classic-none: nothing was ever said about this "
                "user and the target database, rather than access being said to "
                "be none. Read access to _system is still granted, so the failure "
                "happens at the target database and not at the handshake."
            ),
            precreate_database=True,
        ),
        Scenario(
            name="classic-other-database",
            summary="rw, but on a different database",
            grants=[SYSTEM_RO, (other_database, GRANT_RW)],
            steps=[Step("makedata", SCENARIO, DENY)],
            expect_note=(
                "The classic counterpart of coredb-admin-out-of-scope. Classic "
                "has no scope, so the database grant is the whole boundary."
            ),
            precreate_database=True,
        ),
        Scenario(
            name="classic-collection-denied",
            summary="rw on the database but `none` on one collection",
            grants=[SYSTEM_RW, (database, GRANT_RW),
                    (f"{database}/{denied_collection}", GRANT_NONE)],
            steps=[Step("makedata", SCENARIO, DENY)],
            expect_note=(
                f"The classic counterpart of admin-with-explicit-deny: a "
                f"collection-level grant overriding the database grant. The "
                f"workload creates {denied_collection}, so it must fail there "
                f"and nowhere earlier. A classic collection grant only exists "
                f"for a collection that already exists - arangod answers 404 "
                f"otherwise - so the runner pre-creates both the database and "
                f"the collection. createCollectionSafe then finds it already "
                f"there and the denial lands on using it, which is what a "
                f"classic collection grant governs anyway."
            ),
            precreate_database=True,
            precreate_collections=(denied_collection,),
        ),
    ]


# Scenario names that need the sidecar in a non-default authorization mode.
def modes(scenarios):
    seen = []
    for scenario in scenarios:
        if scenario.mode not in seen:
            seen.append(scenario.mode)
    return seen
