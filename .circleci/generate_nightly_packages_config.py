#!/usr/bin/env python3
"""Generate the nightly-packages continuation config.

Reads base_nightly_packages.yml and removes the workflow jobs disabled by
pipeline parameters, including their entries in other jobs' requires lists,
so a disabled job never occupies an executor (the former in-job
"circleci-agent step halt" skipping kept scheduling a node per skipped job).

Invoked by the generate-nightly-packages-config job in nightly_packages.yml (the
setup config); the boolean pipeline parameters are passed through as
true/false CLI options.

With --pr-run true the workflow is emitted as "nightly-packages-pr" instead
of "nightly-packages": same job graph, but the run is recognizable as a PR
test everywhere the workflow name shows up, and the publish job (which reads
the pr-run pipeline parameter itself) degrades to a dry run.
"""

import argparse
import sys
from typing import Any, Dict, List, Set, Tuple, Union

import yaml

WORKFLOW_NAME = "nightly-packages"
PR_WORKFLOW_NAME = "nightly-packages-pr"
ARCHES = ("amd64", "arm64")
PACKAGE_FORMATS = ("deb", "rpm", "tar")
DOCKER_DISTROS = ("alpine", "deb")

JobEntry = Union[str, Dict[str, Any]]


def parse_bool(value: str) -> bool:
    if value not in ("true", "false"):
        raise argparse.ArgumentTypeError(f"expected 'true' or 'false', got {value!r}")
    return value == "true"


def disabled_jobs(
    deb: bool,
    rpm: bool,
    tar: bool,
    alpine_image: bool,
    deb_image: bool,
    sign: bool,
    scan: bool,
    security: bool,
) -> Set[str]:
    """Workflow job names (workflow-level "name", not job type) to drop."""
    formats = {"deb": deb, "rpm": rpm, "tar": tar}
    drop: Set[str] = set()

    for fmt, enabled in formats.items():
        if not enabled:
            drop.update(f"{fmt}-enterprise-{arch}" for arch in ARCHES)
        if not enabled or not security:
            drop.update(f"security-check-{fmt}-{arch}" for arch in ARCHES)

    # Each distro image has its own build-*-image flag, and every built
    # image gets its own Trivy job.
    distros = {"alpine": alpine_image, "deb": deb_image}
    for distro, enabled in distros.items():
        if not enabled:
            drop.update(f"docker-enterprise-{distro}-{arch}" for arch in ARCHES)
        if not enabled or not security:
            drop.update(
                f"security-check-docker-{distro}-{arch}" for arch in ARCHES
            )

    # scan/sign cover packages only; without any package format they would
    # find nothing to work on.
    any_packages = any(formats.values())
    if not scan or not any_packages:
        drop.add("scan-packages")
    if not sign or not any_packages:
        drop.add("sign-packages")

    # The verdict job has nothing to collect once the scans are gone.
    if not security:
        drop.add("security-gate")

    return drop


def entry_name(entry: JobEntry) -> str:
    if isinstance(entry, str):
        return entry
    [(job_type, job_config)] = entry.items()
    if job_config and "name" in job_config:
        return job_config["name"]
    return job_type


def dep_name(dep: JobEntry) -> str:
    """Job name of one requires entry: either "job" or, for CircleCI's
    status-qualified requires, {"job": <status or [statuses]>}."""
    if isinstance(dep, str):
        return dep
    [(name, _)] = dep.items()
    return name


def dep_statuses(dep: JobEntry) -> List[str]:
    """Upstream statuses this requires entry accepts. A plain string entry
    means the CircleCI default, success."""
    if isinstance(dep, str):
        return ["success"]
    [(name, statuses)] = dep.items()
    if isinstance(statuses, str):
        statuses = [statuses]
    if not isinstance(statuses, list) or not all(
        isinstance(s, str) for s in statuses
    ):
        raise ValueError(f"requires entry {name!r} has a malformed status list")
    return statuses


def blocks_on(dep: JobEntry) -> bool:
    """Whether this requires entry actually gates the depending job, i.e.
    only a successful upstream lets it run."""
    return dep_statuses(dep) == ["success"]


def workflow_name(pr_run: bool) -> str:
    """PR test runs get their own workflow name so they never look like a
    real nightly-packages run (in the CircleCI UI and in GitHub PR checks)
    and so cancel-redundant-pipelines can tell the two apart."""
    return PR_WORKFLOW_NAME if pr_run else WORKFLOW_NAME


def prune_workflow(
    config: Dict[str, Any], drop: Set[str], name: str = WORKFLOW_NAME
) -> None:
    workflow = config["workflows"][name]
    kept: List[JobEntry] = []
    for entry in workflow["jobs"]:
        if entry_name(entry) in drop:
            continue
        if isinstance(entry, dict):
            [(_, job_config)] = entry.items()
            if job_config and "requires" in job_config:
                job_config["requires"] = [
                    dep for dep in job_config["requires"] if dep_name(dep) not in drop
                ]
                if not job_config["requires"]:
                    del job_config["requires"]
        kept.append(entry)
    workflow["jobs"] = kept


def check_workflow(config: Dict[str, Any], name: str = WORKFLOW_NAME) -> None:
    """The pruned graph must be self-consistent, or the continuation would
    be rejected by CircleCI after the heavy pipeline has already started."""
    workflow = config["workflows"][name]
    names = [entry_name(entry) for entry in workflow["jobs"]]
    duplicates = {name for name in names if names.count(name) > 1}
    if duplicates:
        raise ValueError(f"duplicate workflow job names: {sorted(duplicates)}")
    known = set(names)
    for entry in workflow["jobs"]:
        if not isinstance(entry, dict):
            continue
        [(_, job_config)] = entry.items()
        for dep in (job_config or {}).get("requires", []):
            if dep_name(dep) not in known:
                raise ValueError(
                    f"{entry_name(entry)} requires pruned/unknown job {dep_name(dep)!r}"
                )
    if "publish-nightly" not in known:
        raise ValueError("publish-nightly is missing from the workflow")

    # publish-nightly must gate on every artifact-producing job that is
    # still in the workflow — DIRECTLY, not only transitively through the
    # scan/sign/security jobs, which can all be disabled. Without this,
    # a gates-all-off run would let publish race the packaging jobs.
    artifact_prefixes = (
        "deb-enterprise",
        "rpm-enterprise",
        "tar-enterprise",
        "docker-enterprise",
    )
    publish_requires: Dict[str, JobEntry] = {}
    for entry in workflow["jobs"]:
        if entry_name(entry) == "publish-nightly" and isinstance(entry, dict):
            [(_, job_config)] = entry.items()
            publish_requires = {
                dep_name(dep): dep for dep in (job_config or {}).get("requires", [])
            }
    missing = sorted(
        name
        for name in known
        if name.startswith(artifact_prefixes) and name not in publish_requires
    )
    if missing:
        raise ValueError(
            "publish-nightly must directly require every artifact-producing "
            f"job in the workflow; missing: {missing}"
        )

    # security-gate failing is the one failure the publish tolerates: a
    # finding must alert without stopping a pre-release nightly. Anything
    # else tolerated anywhere in the graph would let a job run on missing
    # artifacts or missing reports, so both the edge and its status set
    # are pinned here rather than left to whoever edits a requires list
    # next. CircleCI accepts "failed" alone just as happily, which would
    # invert the publish into running ONLY on a red gate.
    for entry in workflow["jobs"]:
        if not isinstance(entry, dict):
            continue
        [(_, job_config)] = entry.items()
        for dep in (job_config or {}).get("requires", []):
            if blocks_on(dep):
                continue
            edge = f"{entry_name(entry)} -> {dep_name(dep)}"
            if edge != "publish-nightly -> security-gate":
                raise ValueError(
                    "only publish-nightly may tolerate a failing "
                    f"security-gate; {edge} tolerates "
                    f"{dep_statuses(dep)}"
                )
            if dep_statuses(dep) != ["success", "failed"]:
                raise ValueError(
                    "publish-nightly must require security-gate with "
                    "exactly ['success', 'failed'], got "
                    f"{dep_statuses(dep)}"
                )


def generate(base: Dict[str, Any], args: argparse.Namespace) -> Dict[str, Any]:
    if not any(
        (
            args.build_debian_packages,
            args.build_rpm_packages,
            args.build_tarballs,
            args.build_alpine_images,
            args.build_deb_images,
        )
    ):
        raise ValueError(
            "nothing selected: enable at least one of build-debian-packages, "
            "build-rpm-packages, build-tarballs, build-alpine-images, "
            "build-deb-images"
        )
    drop = disabled_jobs(
        deb=args.build_debian_packages,
        rpm=args.build_rpm_packages,
        tar=args.build_tarballs,
        alpine_image=args.build_alpine_images,
        deb_image=args.build_deb_images,
        sign=args.sign_packages,
        scan=args.scan_viruses,
        security=args.security_check,
    )
    name = workflow_name(args.pr_run)
    if args.pr_run:
        base["workflows"][name] = base["workflows"].pop(WORKFLOW_NAME)
    prune_workflow(base, drop, name)
    check_workflow(base, name)
    return base


def parse_args(argv: List[str]) -> Tuple[argparse.ArgumentParser, argparse.Namespace]:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True, help="path to base_nightly_packages.yml")
    parser.add_argument("-o", "--output", required=True, help="continuation config to write")
    for option in (
        "build-debian-packages",
        "build-rpm-packages",
        "build-tarballs",
        "build-alpine-images",
        "build-deb-images",
        "sign-packages",
        "scan-viruses",
        "security-check",
        "pr-run",
    ):
        parser.add_argument(
            f"--{option}",
            type=parse_bool,
            required=True,
            metavar="true|false",
            help=f"value of the {option} pipeline parameter",
        )
    return parser, parser.parse_args(argv)


def main(argv: List[str]) -> int:
    parser, args = parse_args(argv)
    with open(args.base, encoding="utf-8") as base_file:
        base = yaml.safe_load(base_file)
    try:
        config = generate(base, args)
    except ValueError as err:
        parser.error(str(err))
    with open(args.output, "w", encoding="utf-8") as output_file:
        yaml.safe_dump(config, output_file, sort_keys=False, width=120)
    name = workflow_name(args.pr_run)
    workflow_jobs = ", ".join(
        entry_name(entry) for entry in config["workflows"][name]["jobs"]
    )
    print(f"Generated {args.output} with workflow {name} and jobs: {workflow_jobs}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
