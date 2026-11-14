"""
Tests for generate_nightly_packages_config.py (nightly-packages continuation config).

Run from the .circleci directory: python -m pytest tests_unit/test_generate_nightly_packages_config.py
"""

import copy
import json
from pathlib import Path

import pytest
import yaml

import generate_nightly_packages_config as gen

BASE_PATH = Path(__file__).parent.parent / "base_nightly_packages.yml"

ALL_TRUE = {
    "build-tarballs": "true",
    "build-ubuntu-images": "true",
    "build-fips-image": "true",
    "sign-packages": "true",
    "scan-viruses": "true",
    "security-check": "true",
    "pr-run": "false",
    "run-benchmarks-on-success": "true",
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


def publish_job_config(config):
    _, workflow = only_workflow(config)
    for entry in workflow["jobs"]:
        if gen.entry_name(entry) == "publish-nightly":
            [(_, job_config)] = entry.items()
            return job_config or {}
    raise AssertionError("publish-nightly not in workflow")


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
    config = run_generate(base_config, **{"build-tarballs": "false"})
    names = workflow_names(config)
    # nothing package-related is left, incl. scan/sign which would have
    # nothing to work on
    for arch in ("amd64", "arm64"):
        assert f"tar-enterprise-{arch}" not in names
        assert f"security-check-tar-{arch}" not in names
    assert "scan-packages" not in names
    assert "sign-packages" not in names
    # docker builds, their security checks, and both compiles remain
    assert "compile-enterprise-amd64" in names
    assert required_names(config, "publish-nightly") == {
        f"docker-enterprise{variant}-{arch}"
        for variant in ("", "-fips")
        for arch in ("amd64", "arm64")
    } | {
        f"security-check-docker-{image}-{arch}"
        for image in ("core", "client-tools", "core-fips")
        for arch in ("amd64", "arm64")
    }


def test_packages_only_drops_docker_jobs(base_config):
    config = run_generate(
        base_config,
        **{"build-ubuntu-images": "false", "build-fips-image": "false"},
    )
    names = workflow_names(config)
    for arch in ("amd64", "arm64"):
        assert f"compile-enterprise-{arch}" in names
        for job in ("docker-enterprise", "docker-enterprise-fips", "compile-enterprise-fips"):
            assert f"{job}-{arch}" not in names
        for image in ("core", "client-tools", "core-fips"):
            assert f"security-check-docker-{image}-{arch}" not in names
    assert "scan-packages" in names
    assert "sign-packages" in names


def test_ubuntu_images_always_come_in_pairs(base_config):
    """One flag, two images: each per-arch docker job feeds one Trivy gate
    per image name (core and client-tools)."""
    config = run_generate(base_config)
    names = workflow_names(config)
    for arch in ("amd64", "arm64"):
        assert f"docker-enterprise-{arch}" in names
        for image in ("core", "client-tools"):
            gate = f"security-check-docker-{image}-{arch}"
            assert gate in names
            assert requires_of(config, gate) == [f"docker-enterprise-{arch}"]


def test_nothing_selected_is_an_error(base_config):
    with pytest.raises(ValueError, match="nothing selected"):
        run_generate(
            base_config,
            **{
                "build-tarballs": "false",
                "build-ubuntu-images": "false",
                "build-fips-image": "false",
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
        assert (gen.TRIGGER_BENCHMARKS in names) == (
            values["run-benchmarks-on-success"] == "true"
            and values["pr-run"] == "false"
            and values["build-ubuntu-images"] == "true"
        )
        # The tolerated-failure exemption must hold whatever is pruned:
        # exactly the security-check jobs when the scans run, nothing
        # otherwise — and the verdicts publish-nightly verifies must
        # track the same set.
        checks = {n for n in names if n.startswith("security-check-")}
        assert bool(checks) == (values["security-check"] == "true")
        tolerated = {
            gen.dep_name(dep)
            for dep in requires_of(config, "publish-nightly")
            if not gen.blocks_on(dep)
        }
        assert tolerated == checks
        items = publish_job_config(config)["expected-gate-items"].split()
        assert sorted(items) == sorted(
            n[len("security-check-"):] for n in checks
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
        "tar-enterprise-amd64",
        "tar-enterprise-arm64",
        "docker-enterprise-amd64",
        "docker-enterprise-arm64",
    ):
        assert job in requires
    # ... and the disabled gate jobs must be gone from the list.
    gate_leftovers = {
        name
        for name in requires
        if name.startswith("security-check-") or name in ("scan-packages", "sign-packages")
    }
    assert not gate_leftovers


def test_publish_tolerates_exactly_the_security_checks(base_config):
    """A finding must red the workflow without blocking the publish, so
    publish-nightly tolerates every security-check job failing — and
    NOTHING else: packaging, ClamAV and signing decide whether the
    artifacts are publishable at all. "canceled" is not tolerated either,
    since it means the workflow is being torn down."""
    config = run_generate(base_config)
    deps = requires_of(config, "publish-nightly")
    checks = [d for d in deps if gen.dep_name(d).startswith("security-check-")]
    assert len(checks) == 8
    for dep in checks:
        assert gen.dep_statuses(dep) == ["success", "failed"]
    for dep in deps:
        if not gen.dep_name(dep).startswith("security-check-"):
            assert dep == gen.dep_name(dep), f"{dep} must be a success-only requires"
    # the scan jobs are still waited for, just not tolerated
    names = required_names(config, "publish-nightly")
    assert len([n for n in names if n.startswith("security-check-")]) == 8


def test_expected_gate_items_track_the_scan_jobs(base_config):
    """publish-nightly verifies one recorded verdict per security-check
    job; the generator prunes the item list together with the jobs (and
    the tar items must go while the docker ones stay)."""
    config = run_generate(base_config, **{"build-tarballs": "false"})
    items = publish_job_config(config)["expected-gate-items"].split()
    assert sorted(items) == sorted(
        name[len("security-check-"):]
        for name in workflow_names(config)
        if name.startswith("security-check-")
    )
    assert "tar-amd64" not in items
    assert "tar-arm64" not in items
    assert "docker-core-amd64" in items


def test_no_security_check_empties_the_gate_items(base_config):
    config = run_generate(base_config, **{"security-check": "false"})
    assert publish_job_config(config)["expected-gate-items"] == ""


def _tolerance_config(publish_requires, expected_items="tar-amd64", jobs=None):
    """Minimal graph exercising check_workflow's tolerated-requires rules."""
    return {
        "workflows": {
            gen.WORKFLOW_NAME: {
                "jobs": (jobs or [])
                + [
                    {"tar-enterprise-amd64": {}},
                    {"security-check-tar-amd64": {}},
                    {
                        "publish-nightly": {
                            "requires": publish_requires,
                            "expected-gate-items": expected_items,
                        }
                    },
                ]
            }
        }
    }


INTENDED = [
    "tar-enterprise-amd64",
    {"security-check-tar-amd64": ["success", "failed"]},
]


def test_check_workflow_rejects_tolerating_anything_but_a_scan():
    """Only the scans may fail. A tolerated packaging job would let
    publish run on missing artifacts."""
    config = _tolerance_config(
        [
            {"tar-enterprise-amd64": ["success", "failed"]},
            {"security-check-tar-amd64": ["success", "failed"]},
        ]
    )
    with pytest.raises(ValueError, match="only publish-nightly may tolerate"):
        gen.check_workflow(config)


def test_check_workflow_rejects_tolerance_on_another_job():
    """Only publish-nightly re-checks the verdicts, so only it may
    tolerate a red scan."""
    config = _tolerance_config(
        INTENDED,
        jobs=[
            {
                "scan-packages": {
                    "requires": [
                        {"security-check-tar-amd64": ["success", "failed"]}
                    ]
                }
            }
        ],
    )
    with pytest.raises(ValueError, match="only publish-nightly may tolerate"):
        gen.check_workflow(config)


@pytest.mark.parametrize(
    "statuses",
    [
        ["failed"],  # success dropped: publish would run ONLY on red scans
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
        ["tar-enterprise-amd64", {"security-check-tar-amd64": statuses}]
    )
    with pytest.raises(ValueError, match="exactly"):
        gen.check_workflow(config)


def test_check_workflow_rejects_a_success_only_scan_require():
    """Regression guard for the original bug: a plain requires entry on a
    security-check job would turn its findings back into a publish
    blocker."""
    config = _tolerance_config(
        ["tar-enterprise-amd64", "security-check-tar-amd64"]
    )
    with pytest.raises(ValueError, match="exactly"):
        gen.check_workflow(config)


def test_check_workflow_rejects_an_unrequired_scan():
    config = _tolerance_config(["tar-enterprise-amd64"])
    with pytest.raises(ValueError, match="must directly require"):
        gen.check_workflow(config)


@pytest.mark.parametrize(
    "expected_items",
    [
        "",  # a dead scan could publish unreported artifacts
        "tar-amd64 tar-arm64",  # a pruned leftover deadlocks the publish
    ],
)
def test_check_workflow_rejects_gate_item_drift(expected_items):
    config = _tolerance_config(INTENDED, expected_items=expected_items)
    with pytest.raises(ValueError, match="expected-gate-items"):
        gen.check_workflow(config)


def test_check_workflow_accepts_the_intended_tolerance():
    gen.check_workflow(_tolerance_config(INTENDED))


def test_dep_statuses_rejects_a_malformed_entry():
    with pytest.raises(ValueError, match="malformed status list"):
        gen.dep_statuses({"security-check-tar-amd64": None})


def test_pr_run_renames_workflow_and_keeps_job_graph(base_config):
    """A PR test run is the same workflow under the nightly-packages-pr name:
    identical job set, no leftover workflow under the real name."""
    config = run_generate(base_config, **{"pr-run": "true"})
    name, _ = only_workflow(config)
    assert name == gen.PR_WORKFLOW_NAME
    assert gen.WORKFLOW_NAME not in config["workflows"]
    # ... minus the benchmarks trigger: a dry run publishes nothing.
    assert workflow_names(config) == workflow_names(base_config) - {
        gen.TRIGGER_BENCHMARKS
    }


def test_pr_run_combines_with_pruning(base_config):
    config = run_generate(
        base_config, **{"pr-run": "true", "security-check": "false"}
    )
    name, _ = only_workflow(config)
    assert name == gen.PR_WORKFLOW_NAME
    names = workflow_names(config)
    assert not any(job.startswith("security-check-") for job in names)
    assert "publish-nightly" in names


def test_docker_gate_item_must_match_the_job_name(base_config):
    """The verdict a docker scan writes is derived from gate-item-image,
    not the job name; drift between the two deadlocks the publish."""
    broken = copy.deepcopy(base_config)
    for entry in broken["workflows"][gen.WORKFLOW_NAME]["jobs"]:
        if (
            isinstance(entry, dict)
            and "security-check-docker-image" in entry
            and entry["security-check-docker-image"]["name"]
            == "security-check-docker-core-amd64"
        ):
            entry["security-check-docker-image"]["gate-item-image"] = "client-tools"
    with pytest.raises(ValueError, match="must match the job name"):
        gen.check_workflow(broken)


def test_benchmarks_trigger_is_off_by_default(base_config):
    config = run_generate(base_config, **{"run-benchmarks-on-success": "false"})
    assert gen.TRIGGER_BENCHMARKS not in workflow_names(config)


def test_benchmarks_trigger_waits_for_publish(base_config):
    config = run_generate(base_config)
    assert requires_of(config, gen.TRIGGER_BENCHMARKS) == ["publish-nightly"]


@pytest.mark.parametrize(
    "overrides",
    [{"pr-run": "true"}, {"build-ubuntu-images": "false"}],
)
def test_benchmarks_trigger_needs_a_real_publish_of_the_image(base_config, overrides):
    config = run_generate(base_config, **overrides)
    assert gen.TRIGGER_BENCHMARKS not in workflow_names(config)


def test_check_workflow_rejects_benchmarks_not_gated_on_publish(base_config):
    config = run_generate(base_config)
    _, workflow = only_workflow(config)
    for entry in workflow["jobs"]:
        if gen.entry_name(entry) == gen.TRIGGER_BENCHMARKS:
            [(_, job_config)] = entry.items()
            job_config["requires"] = [{"publish-nightly": ["success", "failed"]}]
    with pytest.raises(ValueError, match="trigger-benchmarks must require"):
        gen.check_workflow(config)


def job_config_of(config, name):
    _, workflow = only_workflow(config)
    for entry in workflow["jobs"]:
        if gen.entry_name(entry) == name:
            [(_, job_config)] = entry.items()
            return job_config or {}
    raise AssertionError(f"job {name} not in workflow")


def test_fips_image_is_opt_in():
    """build-fips-image is off unless a trigger sets it, in the setup config
    (where triggers set it) and in the base config (which the publish steps
    read) alike."""
    for path in (BASE_PATH, BASE_PATH.parent / "nightly_packages.yml"):
        with open(path, encoding="utf-8") as f:
            params = yaml.safe_load(f)["parameters"]
        assert params["build-fips-image"]["default"] is False
        assert params["build-ubuntu-images"]["default"] is True


def test_fips_off_leaves_no_fips_job_or_gate_item(base_config):
    config = run_generate(base_config, **{"build-fips-image": "false"})
    assert not any("fips" in name for name in workflow_names(config))
    items = publish_job_config(config)["expected-gate-items"].split()
    assert not any("fips" in item for item in items)


def test_fips_image_is_built_from_its_own_fips_compile(base_config):
    """FipsFeature is only compiled in with USE_FIPS, so the FIPS image must
    never be built from the regular (static) compile."""
    config = run_generate(base_config)
    for arch in ("amd64", "arm64"):
        compile_job = job_config_of(config, f"compile-enterprise-fips-{arch}")
        assert compile_job["fips"] is True
        assert compile_job["preset"].endswith("-fips")
        assert (
            job_config_of(config, f"security-check-docker-core-fips-{arch}")["image"]
            == "core-fips-preview"
        )
        docker_job = job_config_of(config, f"docker-enterprise-fips-{arch}")
        assert docker_job["fips"] is True
        assert docker_job["requires"] == [f"compile-enterprise-fips-{arch}"]
        assert requires_of(config, f"security-check-docker-core-fips-{arch}") == [
            f"docker-enterprise-fips-{arch}"
        ]
        assert f"docker-enterprise-fips-{arch}" in required_names(
            config, "publish-nightly"
        )


def test_fips_only_run_skips_the_regular_compiles(base_config):
    config = run_generate(
        base_config, **{"build-tarballs": "false", "build-ubuntu-images": "false"}
    )
    names = workflow_names(config)
    for arch in ("amd64", "arm64"):
        assert f"compile-enterprise-{arch}" not in names
        assert f"compile-enterprise-fips-{arch}" in names
    assert gen.TRIGGER_BENCHMARKS not in names


def test_compile_presets_exist_in_cmake_presets(base_config):
    """Every preset a nightly compile names must exist as a configure and a
    build preset (the job runs cmake --preset and cmake --build --preset),
    and exactly the -fips ones inherit the fips preset (USE_FIPS)."""
    with open(BASE_PATH.parent.parent / "CMakePresets.json", encoding="utf-8") as f:
        presets = json.load(f)
    configure = {p["name"]: p for p in presets["configurePresets"]}
    build = {p["name"] for p in presets["buildPresets"]}
    _, workflow = only_workflow(run_generate(base_config))
    used = [
        entry["compile-nightly"]["preset"]
        for entry in workflow["jobs"]
        if isinstance(entry, dict) and "compile-nightly" in entry
    ]
    assert len(used) == 4
    for preset in used:
        assert preset in configure and preset in build, preset
        assert ("fips" in configure[preset].get("inherits", [])) == preset.endswith(
            "-fips"
        )


def effective_steps(steps, fips):
    """Steps as CircleCI runs them: when/unless blocks on
    << parameters.fips >> resolved, any other block kept as is."""
    for step in steps:
        if isinstance(step, dict) and ("when" in step or "unless" in step):
            [(kind, block)] = step.items()
            if block["condition"] != "<< parameters.fips >>":
                yield from effective_steps(block["steps"], fips)
            elif (kind == "when") == fips:
                yield from effective_steps(block["steps"], fips)
        else:
            yield step


def editions(base_config, job_type, command, fips):
    return [
        step[command]["edition"]
        for step in effective_steps(base_config["jobs"][job_type]["steps"], fips)
        if isinstance(step, dict) and command in step
    ]


@pytest.mark.parametrize("fips, edition", [(False, "enterprise"), (True, "enterprise-fips")])
def test_fips_and_regular_install_trees_never_mix(base_config, fips, edition):
    """The FIPS compile publishes its install tree under its own name and
    only the FIPS docker job reads it, so neither the packages nor the
    regular images can pick up USE_FIPS binaries (and vice versa)."""
    assert editions(base_config, "compile-nightly", "upload-build-artifact", fips) == [edition]
    assert editions(base_config, "build-nightly-docker-image", "download-artifact", fips) == [edition]
    assert editions(base_config, "build-package", "attach_workspace_from_s3", fips) == ["enterprise"]
