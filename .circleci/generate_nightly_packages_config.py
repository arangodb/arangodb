#!/usr/bin/env python3
"""Generate the nightly-packages continuation config.

Reads base_nightly_packages.yml and removes the workflow jobs disabled by
pipeline parameters, including their entries in other jobs' requires lists,
so a disabled job never occupies an executor (the former in-job
"circleci-agent step halt" skipping kept scheduling a node per skipped job).

Invoked by the generate-nightly-packages-config job in nightly_packages.yml (the
setup config); the boolean pipeline parameters are passed through as
true/false CLI options.
"""

import argparse
import sys
from typing import Any, Dict, List, Set, Tuple, Union

import yaml

WORKFLOW_NAME = "nightly-packages"
ARCHES = ("amd64", "arm64")
PACKAGE_FORMATS = ("deb", "rpm", "tar")
DOCKER_DISTROS = ("alpine", "ubi", "deb")

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
    ubi_image: bool,
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
    distros = {"alpine": alpine_image, "ubi": ubi_image, "deb": deb_image}
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

    return drop


def entry_name(entry: JobEntry) -> str:
    if isinstance(entry, str):
        return entry
    [(job_type, job_config)] = entry.items()
    if job_config and "name" in job_config:
        return job_config["name"]
    return job_type


def prune_workflow(config: Dict[str, Any], drop: Set[str]) -> None:
    workflow = config["workflows"][WORKFLOW_NAME]
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


def check_workflow(config: Dict[str, Any]) -> None:
    """The pruned graph must be self-consistent, or the continuation would
    be rejected by CircleCI after the heavy pipeline has already started."""
    workflow = config["workflows"][WORKFLOW_NAME]
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


def generate(base: Dict[str, Any], args: argparse.Namespace) -> Dict[str, Any]:
    if not any(
        (
            args.build_debian_packages,
            args.build_rpm_packages,
            args.build_tarballs,
            args.build_alpine_images,
            args.build_ubi_images,
            args.build_deb_images,
        )
    ):
        raise ValueError(
            "nothing selected: enable at least one of build-debian-packages, "
            "build-rpm-packages, build-tarballs, build-alpine-images, "
            "build-ubi-images, build-deb-images"
        )
    drop = disabled_jobs(
        deb=args.build_debian_packages,
        rpm=args.build_rpm_packages,
        tar=args.build_tarballs,
        alpine_image=args.build_alpine_images,
        ubi_image=args.build_ubi_images,
        deb_image=args.build_deb_images,
        sign=args.sign_packages,
        scan=args.scan_viruses,
        security=args.security_check,
    )
    prune_workflow(base, drop)
    check_workflow(base)
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
        "build-ubi-images",
        "build-deb-images",
        "sign-packages",
        "scan-viruses",
        "security-check",
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
    workflow_jobs = ", ".join(
        entry_name(entry) for entry in config["workflows"][WORKFLOW_NAME]["jobs"]
    )
    print(f"Generated {args.output} with jobs: {workflow_jobs}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
