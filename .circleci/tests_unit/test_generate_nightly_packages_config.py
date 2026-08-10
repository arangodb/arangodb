"""
Tests for generate_nightly_packages_config.py (nightly-packages continuation config).

Run from the .circleci directory: python -m pytest tests_unit/test_generate_nightly_packages_config.py
"""

import copy
from pathlib import Path

import pytest
import yaml

import generate_nightly_packages_config as gen

BASE_PATH = Path(__file__).parent.parent / "base_nightly_packages.yml"

ALL_TRUE = {
    "build-debian-packages": "true",
    "build-rpm-packages": "true",
    "build-tarballs": "true",
    "build-alpine-images": "true",
    "build-deb-images": "true",
    "sign-packages": "true",
    "scan-viruses": "true",
    "security-check": "true",
    "pr-run": "false",
}


@pytest.fixture(scope="module")
def base_config():
    with open(BASE_PATH, encoding="utf-8") as f:
        return yaml.safe_load(f)


def run_generate(base_config, **overrides):
    values = {**ALL_TRUE, **{k: v for k, v in overrides.items()}}
    argv = ["--base", str(BASE_PATH), "-o", "unused.yml"]
    for option, value in values.items():
        argv += [f"--{option}", value]
    _, args = gen.parse_args(argv)
    return gen.generate(copy.deepcopy(base_config), args)


def only_workflow(config):
    [(name, workflow)] = config["workflows"].items()
    return name, workflow


def workflow_names(config):
    _, workflow = only_workflow(config)
    return {gen.entry_name(entry) for entry in workflow["jobs"]}


def requires_of(config, name):
    _, workflow = only_workflow(config)
    for entry in workflow["jobs"]:
        if gen.entry_name(entry) == name:
            [(_, job_config)] = entry.items()
            return (job_config or {}).get("requires", [])
    raise AssertionError(f"job {name} not in workflow")


def required_names(config, name):
    """requires entries reduced to job names: status-qualified entries
    ({"job": [statuses]}) and plain strings compare alike."""
    return {gen.dep_name(dep) for dep in requires_of(config, name)}


def test_all_enabled_keeps_every_job(base_config):
    config = run_generate(base_config)
    assert workflow_names(config) == workflow_names(base_config)


def test_no_security_check_drops_all_trivy_jobs(base_config):
    config = run_generate(base_config, **{"security-check": "false"})
    names = workflow_names(config)
    assert not any(name.startswith("security-check-") for name in names)
    assert not any(
        dep.startswith("security-check-")
        for dep in required_names(config, "publish-nightly")
    )


def test_disabled_format_drops_build_and_scan_jobs(base_config):
    config = run_generate(base_config, **{"build-debian-packages": "false"})
    names = workflow_names(config)
    assert "deb-enterprise-amd64" not in names
    assert "deb-enterprise-arm64" not in names
    assert "security-check-deb-amd64" not in names
    assert "security-check-deb-arm64" not in names
    # the other formats stay, and no requires list still mentions deb jobs
    assert "rpm-enterprise-amd64" in names
    assert "deb-enterprise-amd64" not in requires_of(config, "scan-packages")
    assert "deb-enterprise-amd64" not in requires_of(config, "sign-packages")


def test_sign_and_scan_can_be_disabled(base_config):
    config = run_generate(
        base_config, **{"sign-packages": "false", "scan-viruses": "false"}
    )
    names = workflow_names(config)
    assert "sign-packages" not in names
    assert "scan-packages" not in names
    publish_requires = requires_of(config, "publish-nightly")
    assert "sign-packages" not in publish_requires
    assert "scan-packages" not in publish_requires


def test_docker_only_drops_package_pipeline(base_config):
    config = run_generate(
        base_config,
        **{
            "build-debian-packages": "false",
            "build-rpm-packages": "false",
            "build-tarballs": "false",
        },
    )
    names = workflow_names(config)
    # nothing package-related is left, incl. scan/sign which would have
    # nothing to work on ("deb" also names the Ubuntu docker distro, so
    # check the package jobs explicitly)
    for fmt in ("deb", "rpm", "tar"):
        for arch in ("amd64", "arm64"):
            assert f"{fmt}-enterprise-{arch}" not in names
            assert f"security-check-{fmt}-{arch}" not in names
    assert "scan-packages" not in names
    assert "sign-packages" not in names
    # docker builds, their security checks, and both compiles remain
    assert "compile-enterprise-amd64" in names
    assert required_names(config, "publish-nightly") == {
        f"{kind}-{distro}-{arch}"
        for kind in ("docker-enterprise", "security-check-docker")
        for distro in ("alpine", "deb")
        for arch in ("amd64", "arm64")
    } | {"security-gate"}


def test_packages_only_drops_docker_jobs(base_config):
    config = run_generate(
        base_config,
        **{
            "build-alpine-images": "false",
            "build-deb-images": "false",
        },
    )
    names = workflow_names(config)
    for distro in ("alpine", "deb"):
        for arch in ("amd64", "arm64"):
            assert f"docker-enterprise-{distro}-{arch}" not in names
            assert f"security-check-docker-{distro}-{arch}" not in names
    assert "scan-packages" in names
    assert "sign-packages" in names


def test_per_distro_image_flags(base_config):
    config = run_generate(base_config, **{"build-deb-images": "false"})
    names = workflow_names(config)
    for distro in ("deb",):
        for arch in ("amd64", "arm64"):
            assert f"docker-enterprise-{distro}-{arch}" not in names
            assert f"security-check-docker-{distro}-{arch}" not in names
    for arch in ("amd64", "arm64"):
        assert f"docker-enterprise-alpine-{arch}" in names
        assert f"security-check-docker-alpine-{arch}" in names

    config = run_generate(base_config, **{"build-alpine-images": "false"})
    names = workflow_names(config)
    for arch in ("amd64", "arm64"):
        assert f"docker-enterprise-alpine-{arch}" not in names
        assert f"docker-enterprise-deb-{arch}" in names


def test_nothing_selected_is_an_error(base_config):
    with pytest.raises(ValueError, match="nothing selected"):
        run_generate(
            base_config,
            **{
                "build-debian-packages": "false",
                "build-rpm-packages": "false",
                "build-tarballs": "false",
                "build-alpine-images": "false",
                "build-deb-images": "false",
            },
        )


def test_every_generated_graph_is_consistent(base_config):
    # brute-force all 2^N parameter combinations; generate() either raises
    # the explicit nothing-selected error or yields a consistent graph
    # (check_workflow inside generate() raises otherwise)
    options = list(ALL_TRUE)
    for mask in range(2 ** len(options)):
        values = {
            option: "true" if mask & (1 << i) else "false"
            for i, option in enumerate(options)
        }
        try:
            config = run_generate(base_config, **values)
        except ValueError as err:
            assert "nothing selected" in str(err)
            continue
        names = workflow_names(config)
        assert "publish-nightly" in names
        # The tolerated-failure exemption must hold whatever is pruned:
        # exactly security-gate when the scans run, nothing otherwise.
        gated = "security-gate" in names
        assert gated == (values["security-check"] == "true")
        tolerated = [
            gen.dep_name(dep)
            for dep in requires_of(config, "publish-nightly")
            if not gen.blocks_on(dep)
        ]
        assert tolerated == (["security-gate"] if gated else [])
        if gated:
            # a gate with nothing to collect would fail on an empty
            # verdict set and be tolerated into a silent publish
            gate_requires = requires_of(config, "security-gate")
            assert gate_requires
            assert all(
                gen.dep_name(dep).startswith("security-check-")
                for dep in gate_requires
            )


def test_check_workflow_rejects_dangling_requires():
    config = {
        "workflows": {
            gen.WORKFLOW_NAME: {
                "jobs": [
                    {"publish-nightly": {"requires": ["not-there"]}},
                ]
            }
        }
    }
    with pytest.raises(ValueError, match="not-there"):
        gen.check_workflow(config)


def test_publish_requires_packaging_jobs_when_all_gates_disabled(base_config):
    """Regression: publish-nightly used to depend on the packaging jobs only
    transitively via scan/sign/security-check, so disabling all three gates
    let publish race the packaging jobs. It must require them directly."""
    config = run_generate(
        base_config,
        **{
            "sign-packages": "false",
            "scan-viruses": "false",
            "security-check": "false",
        },
    )
    requires = required_names(config, "publish-nightly")
    for job in (
        "deb-enterprise-amd64",
        "rpm-enterprise-amd64",
        "tar-enterprise-amd64",
        "deb-enterprise-arm64",
        "rpm-enterprise-arm64",
        "tar-enterprise-arm64",
        "docker-enterprise-alpine-amd64",
        "docker-enterprise-alpine-arm64",
    ):
        assert job in requires
    # ... and the disabled gate jobs must be gone from the list.
    gate_leftovers = {
        name
        for name in requires
        if name.startswith("security-check-") or name in ("scan-packages", "sign-packages")
    }
    assert not gate_leftovers


def test_publish_tolerates_only_the_security_gate(base_config):
    """A finding must red the workflow without blocking the publish, so
    publish-nightly tolerates security-gate failing. It must tolerate
    NOTHING else: the scan jobs produce the reports and SBOMs published
    next to the artifacts, so a scan that died has to keep blocking.
    "canceled" is not tolerated either, since it means the workflow is
    being torn down."""
    config = run_generate(base_config)
    deps = requires_of(config, "publish-nightly")
    assert {"security-gate": ["success", "failed"]} in deps
    for dep in deps:
        if gen.dep_name(dep) != "security-gate":
            assert dep == gen.dep_name(dep), f"{dep} must be a success-only requires"
    # the scan jobs are still waited for, just not tolerated
    names = required_names(config, "publish-nightly")
    assert len([n for n in names if n.startswith("security-check-")]) == 10


def test_security_gate_collects_every_scan(base_config):
    config = run_generate(base_config)
    assert required_names(config, "security-gate") == {
        f"security-check-{fmt}-{arch}"
        for fmt in ("deb", "rpm", "tar")
        for arch in ("amd64", "arm64")
    } | {
        f"security-check-docker-{distro}-{arch}"
        for distro in ("alpine", "deb")
        for arch in ("amd64", "arm64")
    }


def test_no_security_check_drops_the_gate_job(base_config):
    config = run_generate(base_config, **{"security-check": "false"})
    names = workflow_names(config)
    assert "security-gate" not in names
    assert "security-gate" not in required_names(config, "publish-nightly")


def _tolerance_config(publish_requires, gate_requires=None):
    """Minimal graph exercising check_workflow's tolerated-requires rule."""
    return {
        "workflows": {
            gen.WORKFLOW_NAME: {
                "jobs": [
                    {"deb-enterprise-amd64": {}},
                    {"security-gate": {"requires": gate_requires or []}},
                    {"publish-nightly": {"requires": publish_requires}},
                ]
            }
        }
    }


def test_check_workflow_rejects_tolerating_anything_but_the_gate():
    """Only the verdict may fail. A tolerated packaging job would let
    publish run on missing artifacts."""
    config = _tolerance_config(
        ["security-gate", {"deb-enterprise-amd64": ["success", "failed"]}]
    )
    with pytest.raises(ValueError, match="only publish-nightly may tolerate"):
        gen.check_workflow(config)


def test_check_workflow_rejects_tolerance_on_another_job():
    """A security-gate that tolerated a failed scan would vote on a
    partial verdict set."""
    config = _tolerance_config(
        ["deb-enterprise-amd64", {"security-gate": ["success", "failed"]}],
        gate_requires=[{"deb-enterprise-amd64": ["success", "failed"]}],
    )
    with pytest.raises(ValueError, match="only publish-nightly may tolerate"):
        gen.check_workflow(config)


@pytest.mark.parametrize(
    "statuses",
    [
        ["failed"],  # success dropped: publish would run ONLY on a red gate
        "failed",
        ["success", "failed", "canceled"],  # torn-down workflow must not publish
        ["success", "canceled"],
        [],
    ],
)
def test_check_workflow_pins_the_tolerated_status_set(statuses):
    """CircleCI accepts every one of these, and the wrong one inverts the
    publish instead of just relaxing it."""
    config = _tolerance_config(
        ["deb-enterprise-amd64", {"security-gate": statuses}]
    )
    with pytest.raises(ValueError, match="exactly"):
        gen.check_workflow(config)


def test_check_workflow_accepts_the_intended_tolerance():
    config = _tolerance_config(
        ["deb-enterprise-amd64", {"security-gate": ["success", "failed"]}]
    )
    gen.check_workflow(config)


def test_dep_statuses_rejects_a_malformed_entry():
    with pytest.raises(ValueError, match="malformed status list"):
        gen.dep_statuses({"security-gate": None})


def test_pr_run_renames_workflow_and_keeps_job_graph(base_config):
    """A PR test run is the same workflow under the nightly-packages-pr name:
    identical job set, no leftover workflow under the real name."""
    config = run_generate(base_config, **{"pr-run": "true"})
    name, _ = only_workflow(config)
    assert name == gen.PR_WORKFLOW_NAME
    assert gen.WORKFLOW_NAME not in config["workflows"]
    assert workflow_names(config) == workflow_names(base_config)


def test_pr_run_combines_with_pruning(base_config):
    config = run_generate(
        base_config, **{"pr-run": "true", "security-check": "false"}
    )
    name, _ = only_workflow(config)
    assert name == gen.PR_WORKFLOW_NAME
    names = workflow_names(config)
    assert not any(job.startswith("security-check-") for job in names)
    assert "publish-nightly" in names
