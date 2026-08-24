#!/usr/bin/env python3
"""Validate the scenario catalog against arangod's own RBAC vocabulary.

*AI generated docs*

An RBAC policy is just strings. A typo like `db:WriteMata` or `db:collections:*:*`
is accepted by the sidecar, matches nothing, and turns a scenario that was meant
to prove "this permission set is sufficient" into one that silently proves
nothing. Catching that needs a source of truth, and the only real one is arangod:
`arangod/Auth/Rbac/ServiceImpl.cpp` is where every action and resource URN the
server can ask about is spelled out.

So this parses that file and checks the catalog against it. Two extra rules come
from the sidecar's own validation (kube-arangodb
pkg/sidecar/services/authorization/types/policy.go):

  ValidateAction   - `*`, or exactly `<namespace>:<name>` with both non-empty
  ValidateResource - non-empty

Run standalone, or as `run_scenarios.py --self-check`. Needs no server.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE_ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
SERVICE_IMPL = os.path.join(SOURCE_ROOT, "arangod", "Auth", "Rbac", "ServiceImpl.cpp")

# `return "db:Read";` in actionToWireString()
ACTION_RE = re.compile(r'return\s+"(db:[A-Za-z]+)"\s*;')
# `std::format("db:collection:{}:{}", ...)` in resourceToWireString()
RESOURCE_TYPE_RE = re.compile(r'std::format\("db:([a-z]+):')


def arangod_vocabulary(path=SERVICE_IMPL):
    """The actions and resource types arangod can actually emit."""
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    with open(path, encoding="utf-8") as handle:
        source = handle.read()
    actions = set(ACTION_RE.findall(source))
    resource_types = set(RESOURCE_TYPE_RE.findall(source))
    if not actions or not resource_types:
        raise RuntimeError(
            f"could not extract the RBAC vocabulary from {path} - the file's shape "
            f"has changed and this checker needs updating"
        )
    return actions, resource_types


ARANGOD_BINARY = os.path.join(SOURCE_ROOT, "build", "bin", "arangod")

# Format strings compiled into arangod; their presence in the binary is the
# ground truth for what the running server can ask about.
BINARY_MARKERS = {
    "api_version_gate": b"db:apiversion:v",
    "admin_read_users": b"db:AdminReadUsers",
}


def _binary_contains(path, needles):
    """Which of `needles` occur in the file. Chunked, so a 300MB binary is fine."""
    found = {name: False for name in needles}
    overlap = max(len(needle) for needle in needles.values())
    tail = b""
    with open(path, "rb") as handle:
        while True:
            chunk = handle.read(8 << 20)
            if not chunk:
                break
            window = tail + chunk
            for name, needle in needles.items():
                if not found[name] and needle in window:
                    found[name] = True
            if all(found.values()):
                break
            tail = window[-overlap:]
    return found


def capabilities(path=SERVICE_IMPL, binary=ARANGOD_BINARY):
    """Which optional parts of the RBAC model the arangod under test has.

    The RBAC implementation is still moving and the catalog has to run against
    more than one point on that line, so nothing here pins a branch.

    `api_version_gate`: devel checks db:UseApiVersion against a
    `db:apiversion:v<n>` resource on every request
    (RestHandler::checkApiVersionAccess). feature/rbac-api-tester has the action
    string but no resource type and no call site, so nothing is gated. Detected
    by the presence of the resource type, since that is what a scope has to
    match.

    `admin_read_users`: db:AdminReadUsers exists on devel only. Unused by the
    catalog; reported so the flavour is visible in the log.

    The **binary** is preferred over the source when one is present, because the
    binary is what actually serves the requests. A checkout whose build is stale
    is easy to end up with - build/bin/arangod here was built from a third
    branch entirely - and silently trusting the source would then produce a
    catalog tuned to code that is not running. A disagreement is reported under
    the `stale_build` key rather than being resolved quietly.
    """
    actions, resource_types = arangod_vocabulary(path)
    from_source = {
        "api_version_gate": "apiversion" in resource_types,
        "admin_read_users": "db:AdminReadUsers" in actions,
    }
    result = dict(from_source)
    result["source_of_truth"] = "source"
    result["stale_build"] = False

    if binary and os.path.exists(binary):
        try:
            from_binary = _binary_contains(binary, BINARY_MARKERS)
        except OSError:
            return result
        result.update(from_binary)
        result["source_of_truth"] = "binary"
        result["stale_build"] = any(
            from_binary[name] != from_source[name] for name in BINARY_MARKERS
        )
    return result


def describe_capabilities(caps):
    flags = ", ".join(
        f"{name}={'yes' if caps[name] else 'no'}" for name in sorted(BINARY_MARKERS)
    )
    lines = [f"arangod RBAC capabilities ({flags}) as read from the "
             f"{caps['source_of_truth']}"]
    if caps["stale_build"]:
        lines.append(
            "  WARNING build/bin/arangod and the checked-out source disagree about "
            "the RBAC model - the build is stale. The binary wins, since it is what "
            "runs, but rebuild before trusting a result.")
    if not caps["api_version_gate"]:
        lines.append(
            "  no API-version gate in this build: dropping the db:apiversion:* scope "
            "entry and the admin-without-api-version scenario")
    return "\n".join(lines)


def check_action(action, known_actions):
    """Return a problem string, or None."""
    if action == "*":
        return None
    parts = action.split(":")
    if len(parts) != 2 or not parts[0] or not parts[1]:
        return f"action {action!r} is not '<namespace>:<name>' (sidecar ValidateAction)"
    if "*" in action:
        # A pattern such as `db:*`; matching is segment-wise so we can only
        # check the namespace.
        if parts[0] != "*" and parts[0] != "db":
            return f"action pattern {action!r} uses an unknown namespace {parts[0]!r}"
        return None
    if action not in known_actions:
        return (f"action {action!r} is not emitted by arangod "
                f"(not in ServiceImpl.cpp::actionToWireString)")
    return None


def check_resource(resource, known_types):
    if resource == "*":
        return None
    if not resource:
        return "empty resource (sidecar ValidateResource rejects it)"
    parts = resource.split(":")
    if parts[0] != "db":
        return f"resource {resource!r} does not start with the 'db' namespace"
    if len(parts) < 2:
        return f"resource {resource!r} has no resource type segment"
    kind = parts[1]
    if kind == "*":
        return None
    if kind not in known_types:
        return (f"resource {resource!r} uses type {kind!r}, which arangod never emits "
                f"(known: {', '.join(sorted(known_types))})")
    # Arity check: arangod emits db:database:<n> and db:user:<n> with one
    # trailing segment, the rest with two. A pattern with too many segments can
    # never match, because manyMatch requires len(resource) >= len(pattern).
    expected = 3 if kind in ("database", "user", "apiversion") else 4
    if len(parts) > expected:
        return (f"resource {resource!r} has {len(parts)} segments but arangod emits "
                f"{expected} for {kind!r}, so it can never match")
    return None


VALID_GRANT_LEVELS = ("rw", "ro", "none")

# Groups where an `error` expectation is meaningful: the workload breaks without
# reaching a permission decision, and that is the documented outcome rather than
# a broken test. Kept to an explicit list so it cannot spread by accident.
ERROR_GROUPS = {"known-issue", "role-modelling"}


def check_grants(scenarios):
    """Classic scenarios carry `_users` grants, which have no RBAC vocabulary to
    validate - but the access level has to be one arangod accepts, and a grant
    path has to be `<db>` or `<db>/<collection>`."""
    problems = []
    for scenario in scenarios:
        grants = getattr(scenario, "grants", None)
        if grants is None:
            continue
        if scenario.policy is not None:
            problems.append((scenario.name, "grants",
                             "carries both classic grants and an RBAC policy; the two "
                             "authorization models are not combined"))
        for path, level in grants:
            if level not in VALID_GRANT_LEVELS:
                problems.append((scenario.name, f"grant {path}",
                                 f"level {level!r} is not one of "
                                 f"{', '.join(VALID_GRANT_LEVELS)}"))
            if path.count("/") > 1:
                problems.append((scenario.name, f"grant {path}",
                                 "path must be '<db>' or '<db>/<collection>'"))
    return problems


def check_catalog(scenarios):
    """Yield (scenario_name, where, problem)."""
    known_actions, known_types = arangod_vocabulary()
    problems = []
    for scenario in scenarios:
        blocks = []
        if scenario.policy is not None:
            blocks.append(("policy", scenario.policy))
        if scenario.scope is not None:
            blocks.append(("scope", scenario.scope))
        for where, statements in blocks:
            for index, statement in enumerate(statements):
                label = f"{where}[{index}]"
                effect = statement.get("effect")
                if effect not in ("Allow", "Deny"):
                    problems.append((scenario.name, label,
                                     f"effect {effect!r} is neither Allow nor Deny"))
                if not statement.get("actions"):
                    problems.append((scenario.name, label, "statement has no actions"))
                if not statement.get("resources"):
                    problems.append((scenario.name, label, "statement has no resources"))
                for action in statement.get("actions", []):
                    problem = check_action(action, known_actions)
                    if problem:
                        problems.append((scenario.name, label, problem))
                for resource in statement.get("resources", []):
                    problem = check_resource(resource, known_types)
                    if problem:
                        problems.append((scenario.name, label, problem))
    return problems, known_actions, known_types


def check_structure(scenarios):
    """Catalog-level invariants that are easy to break while editing."""
    problems = []
    names = [scenario.name for scenario in scenarios]
    for name in set(names):
        if names.count(name) > 1:
            problems.append((name, "catalog", "duplicate scenario name"))
    users = [scenario.user for scenario in scenarios]
    for user in set(users):
        if users.count(user) > 1:
            problems.append((user, "catalog",
                             "two scenarios map to the same user, so their bindings "
                             "would collide"))
    for scenario in scenarios:
        expects_rejection = getattr(scenario, "expect_setup_error", None) is not None
        if not scenario.steps and not expects_rejection:
            problems.append((scenario.name, "steps", "no steps"))
        if scenario.steps and expects_rejection:
            problems.append((
                scenario.name, "steps",
                "expects its setup to be rejected, so no workload can run under it - "
                "the steps would never execute"))
        # A scenario that grants nothing cannot have a passing scenario-identity
        # step; that would be a contradiction in the catalog.
        grants = (scenario.policy is not None and scenario.scope is not None
                  and scenario.bind) or bool(getattr(scenario, "grants", None))
        if not grants:
            for step in scenario.steps:
                if step.identity == "scenario" and step.expect == "pass":
                    problems.append((
                        scenario.name, f"step {step.phase}",
                        "expects pass as the scenario user, but the scenario grants "
                        "nothing (no policy, no scope, or no binding)"))

    for scenario in scenarios:
        for step in scenario.steps:
            if step.expect == "error" and scenario.group not in ERROR_GROUPS:
                problems.append((
                    scenario.name, f"step {step.phase}",
                    f"expects `error`, i.e. the workload breaking without reaching a "
                    f"permission decision. That is only a legitimate expectation in "
                    f"{', '.join(sorted(ERROR_GROUPS))}; anywhere else it hides a "
                    f"broken test"))
    return problems


def main():
    sys.path.insert(0, HERE)
    import scenarios as catalog

    caps = capabilities()
    built = catalog.build(
        "system_rta_rbac", "system_rta_rbac_elsewhere", "cgeo_0",
        api_version_gate=caps["api_version_gate"],
    )
    classic = catalog.build_classic(
        "system_rta_rbac", "system_rta_rbac_elsewhere", "cgeo_0")
    problems, known_actions, known_types = check_catalog(built)
    problems += check_structure(built)
    problems += check_structure(classic)
    problems += check_grants(classic)

    print(f"arangod vocabulary from {os.path.relpath(SERVICE_IMPL, SOURCE_ROOT)}:")
    print(f"  {len(known_actions)} actions, "
          f"resource types: {', '.join(sorted(known_types))}")
    print(describe_capabilities(caps))
    print(f"checked {len(built)} RBAC and {len(classic)} classic scenarios")

    if not problems:
        print("OK - every action and resource in the catalog is one arangod can emit")
        return 0
    print(f"\n{len(problems)} problem(s):")
    for name, where, problem in problems:
        print(f"  {name} {where}: {problem}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
