"""
Tests for multiple build variant workflow generation.

This module tests workflow generation for different build variants
(NORMAL, TSAN, ALUBSAN) across architectures (x64, aarch64).
"""

import pytest
from src.config_lib import (
    TestJob,
    SuiteConfig,
    TestOptions,
    TestDefinitionFile,
    BuildVariant,
)
from src.filters import FilterCriteria
from src.output_generators.base import (
    GeneratorConfig,
)
from src.output_generators.circleci import CircleCIGenerator


class TestBuildVariantWorkflows:
    """Test workflow generation for multiple build variants."""

    def create_test_def(self):
        """Helper to create a minimal test definition."""
        return TestDefinitionFile(
            jobs={
                "test_job": TestJob(
                    name="test_job",
                    suites=[SuiteConfig(name="test_suite")],
                    options=TestOptions(),
                )
            }
        )

    def test_single_sanitizer_config_generates_two_workflows(self):
        """Test that a single sanitizer config generates x64 and aarch64 workflows."""
        config = GeneratorConfig(
            filter_criteria=FilterCriteria(),
            build_variants=[BuildVariant.TSAN],
        )
        gen = CircleCIGenerator(config, base_config={"version": 2.1})
        test_def = self.create_test_def()

        result = gen.generate([test_def])

        workflows = result["workflows"]
        # Should have 2 workflows: x64-pr-tsan and aarch64-pr-tsan
        assert len(workflows) == 2
        assert "x64-pr-tsan" in workflows
        assert "aarch64-pr-tsan" in workflows

    def test_two_sanitizer_configs_generate_four_workflows(self):
        """Test that two sanitizer configs generate four workflows (2 archs × 2 sanitizers)."""
        config = GeneratorConfig(
            filter_criteria=FilterCriteria(),
            build_variants=[
                BuildVariant.TSAN,
                BuildVariant.ALUBSAN,
            ],
        )
        gen = CircleCIGenerator(config, base_config={"version": 2.1})
        test_def = self.create_test_def()

        result = gen.generate([test_def])

        workflows = result["workflows"]
        # Should have 4 workflows: x64/aarch64 × tsan/alubsan
        assert len(workflows) == 4
        assert "x64-pr-tsan" in workflows
        assert "x64-pr-alubsan" in workflows
        assert "aarch64-pr-tsan" in workflows
        assert "aarch64-pr-alubsan" in workflows

    def test_three_configs_generate_six_workflows(self):
        """Test all three configs (none, tsan, alubsan) generate six workflows."""
        config = GeneratorConfig(
            filter_criteria=FilterCriteria(),
            build_variants=[
                BuildVariant.NORMAL,
                BuildVariant.TSAN,
                BuildVariant.ALUBSAN,
            ],
        )
        gen = CircleCIGenerator(config, base_config={"version": 2.1})
        test_def = self.create_test_def()

        result = gen.generate([test_def])

        workflows = result["workflows"]
        # Should have 6 workflows: x64/aarch64 × none/tsan/alubsan
        assert len(workflows) == 6
        assert "x64-pr" in workflows  # non-sanitizer
        assert "x64-pr-tsan" in workflows
        assert "x64-pr-alubsan" in workflows
        assert "aarch64-pr" in workflows  # non-sanitizer
        assert "aarch64-pr-tsan" in workflows
        assert "aarch64-pr-alubsan" in workflows

    def test_non_sanitizer_only_generates_two_workflows(self):
        """Test that non-sanitizer config only generates two workflows."""
        config = GeneratorConfig(
            filter_criteria=FilterCriteria(),
            build_variants=[BuildVariant.NORMAL],
        )
        gen = CircleCIGenerator(config, base_config={"version": 2.1})
        test_def = self.create_test_def()

        result = gen.generate([test_def])

        workflows = result["workflows"]
        # Should have 2 workflows: x64-pr and aarch64-pr (no sanitizer suffix)
        assert len(workflows) == 2
        assert "x64-pr" in workflows
        assert "aarch64-pr" in workflows
        # Should NOT have sanitizer workflows
        assert "x64-pr-tsan" not in workflows
        assert "x64-pr-alubsan" not in workflows

    def test_empty_sanitizer_configs_generates_no_workflows(self):
        """Test that empty build_variants list generates no workflows."""
        config = GeneratorConfig(
            filter_criteria=FilterCriteria(full=True),
            build_variants=[],  # Empty list - no workflows should be generated
        )
        gen = CircleCIGenerator(config, base_config={"version": 2.1})
        test_def = self.create_test_def()

        result = gen.generate([test_def])

        workflows = result["workflows"]
        # Empty build_variants should generate no workflows
        assert len(workflows) == 0

    def test_workflows_have_correct_build_jobs(self):
        """Test that each workflow has correctly named build jobs."""
        config = GeneratorConfig(
            filter_criteria=FilterCriteria(full=True),
            build_variants=[
                BuildVariant.NORMAL,
                BuildVariant.TSAN,
            ],
        )
        gen = CircleCIGenerator(config, base_config={"version": 2.1})
        test_def = self.create_test_def()

        result = gen.generate([test_def])

        workflows = result["workflows"]

        # Check non-sanitizer x64 workflow
        x64_pr_jobs = workflows["x64-nightly"]["jobs"]
        build_jobs = [
            j for j in x64_pr_jobs if "compile-linux" in j
        ]
        assert any(
            j["compile-linux"]["name"] == "build-x64"
            for j in build_jobs
            if "compile-linux" in j
        )

        # Check TSAN x64 workflow
        x64_tsan_jobs = workflows["x64-nightly-tsan"]["jobs"]
        build_jobs_tsan = [
            j for j in x64_tsan_jobs if "compile-linux" in j
        ]
        assert any(
            j["compile-linux"]["name"] == "build-x64-tsan"
            for j in build_jobs_tsan
            if "compile-linux" in j
        )

    def test_workflows_have_independent_test_jobs(self):
        """Test that test jobs are created independently for each workflow."""
        config = GeneratorConfig(
            filter_criteria=FilterCriteria(full=True),
            build_variants=[
                BuildVariant.NORMAL,
                BuildVariant.TSAN,
            ],
        )
        gen = CircleCIGenerator(config, base_config={"version": 2.1})
        test_def = self.create_test_def()

        result = gen.generate([test_def])

        workflows = result["workflows"]

        # Both workflows should have test jobs
        x64_pr_test_jobs = [
            j for j in workflows["x64-nightly"]["jobs"] if "run-linux-tests" in j
        ]
        x64_tsan_test_jobs = [
            j for j in workflows["x64-nightly-tsan"]["jobs"] if "run-linux-tests" in j
        ]

        assert len(x64_pr_test_jobs) > 0, "Non-sanitizer workflow should have test jobs"
        assert len(x64_tsan_test_jobs) > 0, "TSAN workflow should have test jobs"

    def test_nightly_workflows_with_multiple_sanitizers(self):
        """Test that nightly builds work correctly with multiple sanitizers."""
        config = GeneratorConfig(
            filter_criteria=FilterCriteria(full=True),
            build_variants=[
                BuildVariant.NORMAL,
                BuildVariant.TSAN,
            ],
        )
        gen = CircleCIGenerator(config, base_config={"version": 2.1})
        test_def = self.create_test_def()

        result = gen.generate([test_def])

        workflows = result["workflows"]
        # Should have nightly suffix in workflow names
        assert "x64-nightly" in workflows
        assert "x64-nightly-tsan" in workflows
        assert "aarch64-nightly" in workflows
        assert "aarch64-nightly-tsan" in workflows

    def test_cppcheck_only_in_non_sanitizer_x64_workflow(self):
        """Test that cppcheck job only appears in non-sanitizer x64 workflow."""
        config = GeneratorConfig(
            filter_criteria=FilterCriteria(full=True),
            build_variants=[
                BuildVariant.NORMAL,
                BuildVariant.TSAN,
            ],
        )
        gen = CircleCIGenerator(config, base_config={"version": 2.1})
        test_def = self.create_test_def()

        result = gen.generate([test_def])

        workflows = result["workflows"]

        # Check non-sanitizer x64 workflow has cppcheck
        x64_pr_cppcheck = [
            j for j in workflows["x64-nightly"]["jobs"] if "run-cppcheck" in j
        ]
        assert (
            len(x64_pr_cppcheck) == 1
        ), "Non-sanitizer x64 workflow should have cppcheck"

        # Check TSAN x64 workflow does NOT have cppcheck
        x64_tsan_cppcheck = [
            j for j in workflows["x64-nightly-tsan"]["jobs"] if "run-cppcheck" in j
        ]
        assert len(x64_tsan_cppcheck) == 0, "TSAN x64 workflow should NOT have cppcheck"

        # Check aarch64 workflows do NOT have cppcheck
        aarch64_pr_cppcheck = [
            j for j in workflows["aarch64-nightly"]["jobs"] if "run-cppcheck" in j
        ]
        assert (
            len(aarch64_pr_cppcheck) == 0
        ), "aarch64 workflow should NOT have cppcheck"

    def test_job_with_instrumentation_false_excluded_from_sanitizer_workflows(self):
        """Test that jobs with instrumentation=false are excluded from sanitizer workflows."""
        from src.config_lib import TestRequirements

        # Create test definition with one job that requires non-instrumented builds
        test_def = TestDefinitionFile(
            jobs={
                "non_instrumented_job": TestJob(
                    name="non_instrumented_job",
                    suites=[SuiteConfig(name="test_suite")],
                    options=TestOptions(),
                    requires=TestRequirements(instrumentation=False),
                )
            }
        )

        config = GeneratorConfig(
            filter_criteria=FilterCriteria(),
            build_variants=[BuildVariant.NORMAL, BuildVariant.TSAN],
        )
        gen = CircleCIGenerator(config, base_config={"version": 2.1})

        result = gen.generate([test_def])
        workflows = result["workflows"]

        # Job should appear in NORMAL workflow
        normal_test_jobs = [
            j for j in workflows["x64-pr"]["jobs"] if "run-linux-tests" in j
        ]
        assert len(normal_test_jobs) == 2  # single + cluster
        assert any(
            "non_instrumented_job" in j["run-linux-tests"]["name"]
            for j in normal_test_jobs
        )

        # Job should NOT appear in TSAN workflow
        tsan_test_jobs = [
            j for j in workflows["x64-pr-tsan"]["jobs"] if "run-linux-tests" in j
        ]
        assert len(tsan_test_jobs) == 0, "Job with instrumentation=false should not appear in TSAN workflow"

    def test_job_with_instrumentation_true_only_in_sanitizer_workflows(self):
        """Test that jobs with instrumentation=true only appear in sanitizer workflows."""
        from src.config_lib import TestRequirements

        # Create test definition with one job that requires instrumented builds
        test_def = TestDefinitionFile(
            jobs={
                "instrumented_only_job": TestJob(
                    name="instrumented_only_job",
                    suites=[SuiteConfig(name="test_suite")],
                    options=TestOptions(),
                    requires=TestRequirements(instrumentation=True),
                )
            }
        )

        config = GeneratorConfig(
            filter_criteria=FilterCriteria(),
            build_variants=[BuildVariant.NORMAL, BuildVariant.TSAN],
        )
        gen = CircleCIGenerator(config, base_config={"version": 2.1})

        result = gen.generate([test_def])
        workflows = result["workflows"]

        # Job should NOT appear in NORMAL workflow
        normal_test_jobs = [
            j for j in workflows["x64-pr"]["jobs"] if "run-linux-tests" in j
        ]
        assert len(normal_test_jobs) == 0, "Job with instrumentation=true should not appear in NORMAL workflow"

        # Job should appear in TSAN workflow
        tsan_test_jobs = [
            j for j in workflows["x64-pr-tsan"]["jobs"] if "run-linux-tests" in j
        ]
        assert len(tsan_test_jobs) == 2  # single + cluster
        assert any(
            "instrumented_only_job" in j["run-linux-tests"]["name"]
            for j in tsan_test_jobs
        )
