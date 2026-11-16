"""
Tests for the run-origin parameter of the benchmarks pipeline (benchmarks.yml)
and for the nightly-packages trigger that sets it.
"""

import os
import re

import yaml

CIRCLECI_DIR = os.path.join(os.path.dirname(__file__), "..")
ORIGINS = ["pr", "nightly"]


def load(name):
    with open(os.path.join(CIRCLECI_DIR, name), encoding="utf-8") as f:
        return yaml.safe_load(f)


def test_run_origin_parameter():
    param = load("benchmarks.yml")["parameters"]["run-origin"]
    assert param["type"] == "enum"
    assert param["enum"] == ORIGINS
    assert param["default"] == "pr"


def test_workflow_names_carry_run_origin():
    workflows = load("benchmarks.yml")["workflows"]
    assert sorted(workflows) == sorted(
        f"ann-benchmark-{kind}-{origin}"
        for kind in ("docker", "local")
        for origin in ORIGINS
    )


def test_each_workflow_runs_only_for_its_origin():
    for name, workflow in load("benchmarks.yml")["workflows"].items():
        origin = name.rsplit("-", 1)[1]
        conditions = workflow["when"]["and"]
        assert {"equal": [origin, "<< pipeline.parameters.run-origin >>"]} in conditions


def test_origin_variants_share_jobs():
    workflows = load("benchmarks.yml")["workflows"]
    for kind in ("docker", "local"):
        assert (
            workflows[f"ann-benchmark-{kind}-pr"]["jobs"]
            == workflows[f"ann-benchmark-{kind}-nightly"]["jobs"]
        )


def test_nightly_trigger_sets_run_origin_nightly():
    job = load("base_nightly_packages.yml")["jobs"]["trigger-benchmarks"]
    command = "\n".join(
        step["run"]["command"] for step in job["steps"] if "run" in step
    )
    assert re.search(r'"run-origin":\s*"nightly"', command)


def test_nightly_packages_has_no_run_origin_parameter():
    for name in ("nightly_packages.yml", "base_nightly_packages.yml"):
        assert "run-origin" not in load(name).get("parameters", {})
