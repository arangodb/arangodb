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
PACKAGE_FORMATS = ("tar",)
DOCKER_IMAGES = ("core", "client-tools")

JobEntry = Union[str, Dict[str, Any]]


def parse_bool(value: str) -> bool:
    if value not in ("true", "false"):
        raise argparse.ArgumentTypeError(f"expected 'true' or 'false', got {value!r}")
    return value == "true"


def disabled_jobs(
    tar: bool,
    alpine_images: bool,
    sign: bool,
    scan: bool,
    security: bool,
) -> Set[str]:
    """Workflow job names (workflow-level "name", not job type) to drop."""
    drop: Set[str] = set()

    if not tar:
        drop.update(f"tar-enterprise-{arch}" for arch in ARCHES)
    if not tar or not security:
        drop.update(f"security-check-tar-{arch}" for arch in ARCHES)

    # One docker job per architecture builds BOTH Ubuntu-based images
    # (core-preview and client-tools-preview); each built image gets its
    # own Trivy job.
    if not alpine_images:
        drop.update(f"docker-enterprise-{arch}" for arch in ARCHES)
    for image in DOCKER_IMAGES:
        if not alpine_images or not security:
            drop.update(
                f"security-check-docker-{image}-{arch}" for arch in ARCHES
            )

    # scan/sign cover packages only; without the tarballs they would find
    # nothing to work on.
    if not scan or not tar:
        drop.add("scan-packages")
    if not sign or not tar:
        drop.add("sign-packages")

    return drop


def entry_name(entry: JobEntry) -> str:
    if isinstance(entry, str):
        return entry
    [(job_type, job_config)] = entry.items()
    if job_config and "name" in job_config:
        return job_config["name"]
    return job_type


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
                    dep for dep in job_config["requires"] if dep not in drop
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
            if dep not in known:
                raise ValueError(
                    f"{entry_name(entry)} requires pruned/unknown job {dep!r}"
                )
    if "publish-nightly" not in known:
        raise ValueError("publish-nightly is missing from the workflow")

    # publish-nightly must gate on every artifact-producing job that is
    # still in the workflow — DIRECTLY, not only transitively through the
    # scan/sign/security jobs, which can all be disabled. Without this,
    # a gates-all-off run would let publish race the packaging jobs.
    artifact_prefixes = (
        "tar-enterprise",
        "docker-enterprise",
    )
    publish_requires: Set[str] = set()
    for entry in workflow["jobs"]:
        if entry_name(entry) == "publish-nightly" and isinstance(entry, dict):
            [(_, job_config)] = entry.items()
            publish_requires = set((job_config or {}).get("requires", []))
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


def generate(base: Dict[str, Any], args: argparse.Namespace) -> Dict[str, Any]:
    if not any((args.build_tarballs, args.build_alpine_images)):
        raise ValueError(
            "nothing selected: enable at least one of build-tarballs, "
            "build-alpine-images"
        )
    drop = disabled_jobs(
        tar=args.build_tarballs,
        alpine_images=args.build_alpine_images,
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
        "build-tarballs",
        "build-alpine-images",
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
