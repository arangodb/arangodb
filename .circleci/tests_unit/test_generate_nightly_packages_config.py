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
    "build-ubi-images": "true",
    "build-deb-images": "true",
    "sign-packages": "true",
    "scan-viruses": "true",
    "security-check": "true",
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


def workflow_names(config):
    jobs = config["workflows"][gen.WORKFLOW_NAME]["jobs"]
    return {gen.entry_name(entry) for entry in jobs}


def requires_of(config, name):
    for entry in config["workflows"][gen.WORKFLOW_NAME]["jobs"]:
        if gen.entry_name(entry) == name:
            [(_, job_config)] = entry.items()
            return (job_config or {}).get("requires", [])
    raise AssertionError(f"job {name} not in workflow")


def test_all_enabled_keeps_every_job(base_config):
    config = run_generate(base_config)
    assert workflow_names(config) == workflow_names(base_config)


def test_no_security_check_drops_all_trivy_jobs(base_config):
    config = run_generate(base_config, **{"security-check": "false"})
    names = workflow_names(config)
    assert not any(name.startswith("security-check-") for name in names)
    assert not any(
        dep.startswith("security-check-") for dep in requires_of(config, "publish-nightly")
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
    assert set(requires_of(config, "publish-nightly")) == {
        f"{kind}-{distro}-{arch}"
        for kind in ("docker-enterprise", "security-check-docker")
        for distro in ("alpine", "ubi", "deb")
        for arch in ("amd64", "arm64")
    }


def test_packages_only_drops_docker_jobs(base_config):
    config = run_generate(
        base_config,
        **{
            "build-alpine-images": "false",
            "build-ubi-images": "false",
            "build-deb-images": "false",
        },
    )
    names = workflow_names(config)
    for distro in ("alpine", "ubi", "deb"):
        for arch in ("amd64", "arm64"):
            assert f"docker-enterprise-{distro}-{arch}" not in names
            assert f"security-check-docker-{distro}-{arch}" not in names
    assert "scan-packages" in names
    assert "sign-packages" in names


def test_per_distro_image_flags(base_config):
    config = run_generate(
        base_config, **{"build-ubi-images": "false", "build-deb-images": "false"}
    )
    names = workflow_names(config)
    for distro in ("ubi", "deb"):
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
        assert f"docker-enterprise-ubi-{arch}" in names
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
                "build-ubi-images": "false",
                "build-deb-images": "false",
            },
        )


def test_every_generated_graph_is_consistent(base_config):
    # brute-force all 2^7 parameter combinations; generate() either raises
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
        assert "publish-nightly" in workflow_names(config)


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
    requires = set(requires_of(config, "publish-nightly"))
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
