"""
Tests for filtering logic.

This module tests the job filtering functionality based on various criteria
such as deployment type, test type, platform exclusions, etc.
"""

import pytest
from src.config_lib import (
    TestJob,
    SuiteConfig,
    TestOptions,
    TestRequirements,
    DeploymentType,
    TestDefinitionFile,
)
from src.filters import (
    FilterCriteria,
    should_include_job,
    should_include_suite,
    filter_jobs,
    filter_suites,
)


class TestFilterCriteria:
    """Test FilterCriteria dataclass."""

    def test_default_values(self):
        """Test default filter criteria values."""
        criteria = FilterCriteria()
        assert criteria.full is False
        assert criteria.gtest is False
        assert criteria.sanitizer is None
        assert criteria.coverage is False
        assert criteria.deployment_type is None
        assert criteria.enterprise is True

    def test_custom_values(self):
        """Test setting custom criteria values."""
        from src.config_lib import Sanitizer

        criteria = FilterCriteria(
            full=True,
            sanitizer=Sanitizer.TSAN,
            enterprise=False,
        )
        assert criteria.full is True
        assert criteria.sanitizer == Sanitizer.TSAN
        assert criteria.enterprise is False

    def test_is_full_run(self):
        """Test is_full_run property."""
        assert FilterCriteria().is_full_run is False
        assert FilterCriteria(full=True).is_full_run is True
        assert FilterCriteria(full=False).is_full_run is False


class TestShouldIncludeJob:
    """Test should_include_job function."""

    def create_job(self, deployment_type=None, suite_names=None, priority=1):
        """Helper to create a test job."""
        if suite_names is None:
            # Use multiple suites for MIXED deployment type
            if deployment_type == DeploymentType.MIXED:
                suite_names = ["suite1", "suite2"]
            else:
                suite_names = ["test_suite"]

        suites = [SuiteConfig(name=name) for name in suite_names]
        options = TestOptions(deployment_type=deployment_type, priority=priority)
        return TestJob(
            name="test_job",
            suites=suites,
            options=options,
        )

    def test_gtest_filter(self):
        """Test filtering for gtest suites."""
        criteria = FilterCriteria(gtest=True)

        # Should include jobs with gtest suites
        job = self.create_job(suite_names=["gtest_foo", "gtest_bar"])
        assert should_include_job(job, criteria) is True

        # Should include jobs with at least one gtest suite
        job = self.create_job(suite_names=["regular_test", "gtest_foo"])
        assert should_include_job(job, criteria) is True

        # Should exclude jobs without gtest suites
        job = self.create_job(suite_names=["regular_test", "another_test"])
        assert should_include_job(job, criteria) is False


class TestFilterJobs:
    """Test filter_jobs function."""

    def create_test_definition(self, jobs_dict):
        """Helper to create a TestDefinitionFile."""
        return TestDefinitionFile(jobs=jobs_dict)


    def test_gtest_filtering(self):
        """Test filtering for gtest jobs."""
        jobs = {
            "gtest_job": TestJob(
                name="gtest_job",
                suites=[SuiteConfig(name="gtest_basics")],
                options=TestOptions(),
            ),
            "regular_job": TestJob(
                name="regular_job",
                suites=[SuiteConfig(name="shell_server")],
                options=TestOptions(),
            ),
            "mixed_job": TestJob(
                name="mixed_job",
                suites=[
                    SuiteConfig(name="gtest_foo"),
                    SuiteConfig(name="regular_test"),
                ],
                options=TestOptions(),
            ),
        }
        test_def = self.create_test_definition(jobs)

        criteria = FilterCriteria(gtest=True)
        result = filter_jobs(test_def, criteria)
        result_names = {job.name for job in result}
        assert "gtest_job" in result_names
        assert "mixed_job" in result_names
        assert "regular_job" not in result_names

    def test_order_preserved(self):
        """Test that job order is preserved in results."""
        jobs = {
            f"job{i}": TestJob(
                name=f"job{i}",
                suites=[SuiteConfig(name="test")],
                options=TestOptions(),
            )
            for i in range(5)
        }
        test_def = self.create_test_definition(jobs)

        criteria = FilterCriteria()
        result = filter_jobs(test_def, criteria)

        # Check that we got all jobs
        assert len(result) == 5


class TestShouldIncludeSuite:
    """Test should_include_suite function."""

    @pytest.mark.parametrize(
        "full,suite_full,expected",
        [
            # PR builds (full=False)
            (False, False, True),  # PR suite included
            (False, None, True),  # Any suite included
            (False, True, False),  # Full suite excluded
            # Full builds (full=True, covers both --full and --nightly CLI flags)
            (True, True, True),  # Full suite included
            (True, None, True),  # Any suite included
            (True, False, False),  # PR suite excluded
        ],
    )
    def test_suite_filtering_by_build_type(self, full, suite_full, expected):
        """Test suite filtering across PR/full builds."""
        criteria = FilterCriteria(full=full)
        suite = SuiteConfig(name="test", requires=TestRequirements(full=suite_full))
        assert should_include_suite(suite, criteria) is expected

    def test_suite_without_options(self):
        """Test suite with no options field."""
        # Suite without options should always be included
        suite = SuiteConfig(name="simple_suite")

        # PR build
        criteria = FilterCriteria(full=False)
        assert should_include_suite(suite, criteria) is True

        # Full build
        criteria = FilterCriteria(full=True)
        assert should_include_suite(suite, criteria) is True


class TestFilterSuites:
    """Test filter_suites function."""

    def test_filter_mixed_suites_in_pr_build(self, mixed_suite_job):
        """Test filtering a mix of PR and full suites in PR build."""
        criteria = FilterCriteria(full=False)
        result = filter_suites(mixed_suite_job, criteria)

        # Should include PR suite and any suite, exclude full suite
        assert len(result) == 2
        result_names = {suite.name for suite in result}
        assert "pr_suite" in result_names
        assert "any_suite" in result_names
        assert "full_suite" not in result_names

    def test_filter_mixed_suites_in_full_build(self, mixed_suite_job):
        """Test filtering a mix of PR and full suites in full build."""
        criteria = FilterCriteria(full=True)
        result = filter_suites(mixed_suite_job, criteria)

        # Should include full suite and any suite, exclude PR suite
        assert len(result) == 2
        result_names = {suite.name for suite in result}
        assert "full_suite" in result_names
        assert "any_suite" in result_names
        assert "pr_suite" not in result_names

    def test_filter_job_with_single_suite(self):
        """Test filtering job with only one suite."""
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(name="only_suite", requires=TestRequirements(full=True))
            ],
            options=TestOptions(),
        )

        # PR build - should exclude the suite
        criteria = FilterCriteria(full=False)
        result = filter_suites(job, criteria)
        assert len(result) == 0

        # Full build - should include the suite
        criteria = FilterCriteria(full=True)
        result = filter_suites(job, criteria)
        assert len(result) == 1
        assert result[0].name == "only_suite"

    def test_filter_job_with_no_suite_options(self):
        """Test filtering job where suites have no options."""
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(name="suite1"),
                SuiteConfig(name="suite2"),
                SuiteConfig(name="suite3"),
            ],
            options=TestOptions(),
        )

        # PR build - should include all suites
        criteria = FilterCriteria(full=False)
        result = filter_suites(job, criteria)
        assert len(result) == 3

        # Full build - should include all suites
        criteria = FilterCriteria(full=True)
        result = filter_suites(job, criteria)
        assert len(result) == 3

    def test_order_preserved(self):
        """Test that suite order is preserved in results."""
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(name="suite1", options=TestOptions()),
                SuiteConfig(name="suite2", options=TestOptions()),
                SuiteConfig(name="suite3", options=TestOptions()),
                SuiteConfig(name="suite4", options=TestOptions()),
            ],
            options=TestOptions(),
        )

        criteria = FilterCriteria()
        result = filter_suites(job, criteria)

        # Check that order is preserved
        assert len(result) == 4
        assert result[0].name == "suite1"
        assert result[1].name == "suite2"
        assert result[2].name == "suite3"
        assert result[3].name == "suite4"

    def test_job_level_requires_inherited_by_suites(self):
        """Test that job-level requirements are inherited by suites without their own requires.

        Regression test for: When a job has requires: {full: true} but individual
        suites have no requires section, those suites should inherit the job's
        full=True requirement and be filtered out in PR builds.
        """
        # Job with full=True at job level, suites with no requires
        job = TestJob(
            name="test_job",
            requires=TestRequirements(full=True),
            suites=[
                SuiteConfig(name="suite1"),  # No requires - should inherit job's full=True
                SuiteConfig(name="suite2"),  # No requires - should inherit job's full=True
            ],
            options=TestOptions(),
        )

        # PR build (full=False) - should filter out all suites
        pr_criteria = FilterCriteria(full=False)
        pr_result = filter_suites(job, pr_criteria)
        assert len(pr_result) == 0, "Suites should inherit job's full=True and be filtered in PR builds"

        # Full build (full=True) - should include all suites
        full_criteria = FilterCriteria(full=True)
        full_result = filter_suites(job, full_criteria)
        assert len(full_result) == 2
        assert full_result[0].name == "suite1"
        assert full_result[1].name == "suite2"
        # Verify resolved suites have inherited requirements
        assert full_result[0].requires.full is True
        assert full_result[1].requires.full is True


class TestArchitectureFiltering:
    """Test architecture filtering at job and suite level."""

    def test_job_without_architecture_constraint_included_on_all_archs(self):
        """Job without architecture field should run on all architectures."""
        from src.config_lib import Architecture

        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(),
        )

        # Should be included on x64
        criteria_x64 = FilterCriteria(architecture=Architecture.X64)
        assert should_include_job(job, criteria_x64)

        # Should be included on aarch64
        criteria_aarch64 = FilterCriteria(architecture=Architecture.AARCH64)
        assert should_include_job(job, criteria_aarch64)

    def test_job_with_x64_only_filtered_on_aarch64(self):
        """Job constrained to x64 should be filtered out on aarch64."""
        from src.config_lib import Architecture

        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(architecture=Architecture.X64),
        )

        # Should be included on x64
        criteria_x64 = FilterCriteria(architecture=Architecture.X64)
        assert should_include_job(job, criteria_x64)

        # Should be excluded on aarch64
        criteria_aarch64 = FilterCriteria(architecture=Architecture.AARCH64)
        assert not should_include_job(job, criteria_aarch64)

    def test_job_with_aarch64_only_filtered_on_x64(self):
        """Job constrained to aarch64 should be filtered out on x64."""
        from src.config_lib import Architecture

        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(architecture=Architecture.AARCH64),
        )

        # Should be excluded on x64
        criteria_x64 = FilterCriteria(architecture=Architecture.X64)
        assert not should_include_job(job, criteria_x64)

        # Should be included on aarch64
        criteria_aarch64 = FilterCriteria(architecture=Architecture.AARCH64)
        assert should_include_job(job, criteria_aarch64)

    def test_suite_level_architecture_override(self):
        """Suite can override job-level architecture constraints."""
        from src.config_lib import Architecture

        suite_x64_only = SuiteConfig(
            name="suite_x64",
            requires=TestRequirements(architecture=Architecture.X64),
        )
        suite_aarch64_only = SuiteConfig(
            name="suite_aarch64",
            requires=TestRequirements(architecture=Architecture.AARCH64),
        )

        # x64-only suite
        criteria_x64 = FilterCriteria(architecture=Architecture.X64)
        assert should_include_suite(suite_x64_only, criteria_x64)

        criteria_aarch64 = FilterCriteria(architecture=Architecture.AARCH64)
        assert not should_include_suite(suite_x64_only, criteria_aarch64)

        # aarch64-only suite
        assert not should_include_suite(suite_aarch64_only, criteria_x64)
        assert should_include_suite(suite_aarch64_only, criteria_aarch64)

    def test_architecture_filter_without_criteria_architecture(self):
        """If criteria doesn't specify architecture, don't filter."""
        from src.config_lib import Architecture

        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(architecture=Architecture.X64),
        )

        # No architecture in criteria - should include
        criteria = FilterCriteria(architecture=None)
        assert should_include_job(job, criteria)


class TestInstrumentationFiltering:
    """Test instrumentation filtering at job and suite level."""

    def test_job_without_instrumentation_constraint_included_in_all_builds(self):
        """Job without instrumentation field should run in all build types."""
        from src.config_lib import Sanitizer

        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(),
        )

        # Should be included in regular build
        criteria_regular = FilterCriteria()
        assert should_include_job(job, criteria_regular)

        # Should be included in TSAN build
        criteria_tsan = FilterCriteria(sanitizer=Sanitizer.TSAN)
        assert should_include_job(job, criteria_tsan)

    def test_job_with_instrumentation_true_only_in_instrumented_builds(self):
        """Job with instrumentation=True should only run in instrumented builds."""
        from src.config_lib import Sanitizer

        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(instrumentation=True),
        )

        # Should be excluded from regular build
        criteria_regular = FilterCriteria()
        assert not should_include_job(job, criteria_regular)

        # Should be included in TSAN build
        criteria_tsan = FilterCriteria(sanitizer=Sanitizer.TSAN)
        assert should_include_job(job, criteria_tsan)

        # Should be included in ALUBSAN build
        criteria_alubsan = FilterCriteria(sanitizer=Sanitizer.ALUBSAN)
        assert should_include_job(job, criteria_alubsan)

    def test_job_with_instrumentation_false_only_in_regular_builds(self):
        """Job with instrumentation=False should only run in regular builds."""
        from src.config_lib import Sanitizer

        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(instrumentation=False),
        )

        # Should be included in regular build
        criteria_regular = FilterCriteria()
        assert should_include_job(job, criteria_regular)

        # Should be excluded from TSAN build
        criteria_tsan = FilterCriteria(sanitizer=Sanitizer.TSAN)
        assert not should_include_job(job, criteria_tsan)

        # Should be excluded from ALUBSAN build
        criteria_alubsan = FilterCriteria(sanitizer=Sanitizer.ALUBSAN)
        assert not should_include_job(job, criteria_alubsan)

    def test_suite_without_instrumentation_constraint_included_in_all_builds(self):
        """Suite without instrumentation field should run in all build types."""
        from src.config_lib import Sanitizer

        suite = SuiteConfig(name="suite1")

        # Should be included in regular build
        criteria_regular = FilterCriteria()
        assert should_include_suite(suite, criteria_regular)

        # Should be included in TSAN build
        criteria_tsan = FilterCriteria(sanitizer=Sanitizer.TSAN)
        assert should_include_suite(suite, criteria_tsan)

    def test_suite_with_instrumentation_true_only_in_instrumented_builds(self):
        """Suite with instrumentation=True should only run in instrumented builds."""
        from src.config_lib import Sanitizer

        suite = SuiteConfig(
            name="suite1", requires=TestRequirements(instrumentation=True)
        )

        # Should be excluded from regular build
        criteria_regular = FilterCriteria()
        assert not should_include_suite(suite, criteria_regular)

        # Should be included in TSAN build
        criteria_tsan = FilterCriteria(sanitizer=Sanitizer.TSAN)
        assert should_include_suite(suite, criteria_tsan)

    def test_suite_with_instrumentation_false_only_in_regular_builds(self):
        """Suite with instrumentation=False should only run in regular builds."""
        from src.config_lib import Sanitizer

        suite = SuiteConfig(
            name="suite1", requires=TestRequirements(instrumentation=False)
        )

        # Should be included in regular build
        criteria_regular = FilterCriteria()
        assert should_include_suite(suite, criteria_regular)

        # Should be excluded from TSAN build
        criteria_tsan = FilterCriteria(sanitizer=Sanitizer.TSAN)
        assert not should_include_suite(suite, criteria_tsan)

    def test_filter_suites_by_instrumentation(self):
        """Test filtering mixed instrumentation suites in different build types."""
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(
                    name="instrumented_suite",
                    requires=TestRequirements(instrumentation=True),
                ),
                SuiteConfig(
                    name="regular_suite",
                    requires=TestRequirements(instrumentation=False),
                ),
                SuiteConfig(name="any_suite"),
            ],
            options=TestOptions(),
        )

        # Regular build - should include regular_suite and any_suite, exclude instrumented_suite
        criteria_regular = FilterCriteria()
        result = filter_suites(job, criteria_regular)
        assert len(result) == 2
        result_names = {suite.name for suite in result}
        assert "regular_suite" in result_names
        assert "any_suite" in result_names
        assert "instrumented_suite" not in result_names

        # TSAN build - should include instrumented_suite and any_suite, exclude regular_suite
        from src.config_lib import Sanitizer

        criteria_tsan = FilterCriteria(sanitizer=Sanitizer.TSAN)
        result = filter_suites(job, criteria_tsan)
        assert len(result) == 2
        result_names = {suite.name for suite in result}
        assert "instrumented_suite" in result_names
        assert "any_suite" in result_names
        assert "regular_suite" not in result_names

    def test_suite_instrumentation_overrides_job_instrumentation(self):
        """Suite-level instrumentation should override job-level instrumentation."""
        from src.config_lib import Sanitizer

        # Job with instrumentation=False, but suite overrides to True
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(
                    name="override_suite",
                    requires=TestRequirements(instrumentation=True),
                )
            ],
            requires=TestRequirements(instrumentation=False),
        )

        # Job should be excluded from TSAN build (job-level filter)
        criteria_tsan = FilterCriteria(sanitizer=Sanitizer.TSAN)
        assert not should_include_job(job, criteria_tsan)

        # But if we check the suite directly, it should be included
        suite = job.suites[0]
        assert should_include_suite(suite, criteria_tsan)


class TestCoverageFiltering:
    """Test coverage filtering at job and suite level."""

    def test_job_without_coverage_constraint_included_in_all_builds(self):
        """Job without coverage field should run in all build types."""
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(),
        )

        # Should be included in regular build
        criteria_regular = FilterCriteria(coverage=False)
        assert should_include_job(job, criteria_regular)

        # Should be included in coverage build
        criteria_coverage = FilterCriteria(coverage=True)
        assert should_include_job(job, criteria_coverage)

    def test_job_with_coverage_true_only_in_coverage_builds(self):
        """Job with coverage=True should only run in coverage builds."""
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(coverage=True),
        )

        # Should be excluded from regular build
        criteria_regular = FilterCriteria(coverage=False)
        assert not should_include_job(job, criteria_regular)

        # Should be included in coverage build
        criteria_coverage = FilterCriteria(coverage=True)
        assert should_include_job(job, criteria_coverage)

    def test_job_with_coverage_false_only_in_regular_builds(self):
        """Job with coverage=False should only run in non-coverage builds."""
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(coverage=False),
        )

        # Should be included in regular build
        criteria_regular = FilterCriteria(coverage=False)
        assert should_include_job(job, criteria_regular)

        # Should be excluded from coverage build
        criteria_coverage = FilterCriteria(coverage=True)
        assert not should_include_job(job, criteria_coverage)

    def test_suite_without_coverage_constraint_included_in_all_builds(self):
        """Suite without coverage field should run in all build types."""
        suite = SuiteConfig(name="suite1")

        # Should be included in regular build
        criteria_regular = FilterCriteria(coverage=False)
        assert should_include_suite(suite, criteria_regular)

        # Should be included in coverage build
        criteria_coverage = FilterCriteria(coverage=True)
        assert should_include_suite(suite, criteria_coverage)

    def test_suite_with_coverage_true_only_in_coverage_builds(self):
        """Suite with coverage=True should only run in coverage builds."""
        suite = SuiteConfig(name="suite1", requires=TestRequirements(coverage=True))

        # Should be excluded from regular build
        criteria_regular = FilterCriteria(coverage=False)
        assert not should_include_suite(suite, criteria_regular)

        # Should be included in coverage build
        criteria_coverage = FilterCriteria(coverage=True)
        assert should_include_suite(suite, criteria_coverage)

    def test_suite_with_coverage_false_only_in_regular_builds(self):
        """Suite with coverage=False should only run in regular builds."""
        suite = SuiteConfig(name="suite1", requires=TestRequirements(coverage=False))

        # Should be included in regular build
        criteria_regular = FilterCriteria(coverage=False)
        assert should_include_suite(suite, criteria_regular)

        # Should be excluded from coverage build
        criteria_coverage = FilterCriteria(coverage=True)
        assert not should_include_suite(suite, criteria_coverage)

    def test_filter_suites_by_coverage(self):
        """Test filtering mixed coverage suites in different build types."""
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(
                    name="coverage_suite", requires=TestRequirements(coverage=True)
                ),
                SuiteConfig(
                    name="no_coverage_suite", requires=TestRequirements(coverage=False)
                ),
                SuiteConfig(name="any_suite"),
            ],
            options=TestOptions(),
        )

        # Regular build - should include no_coverage_suite and any_suite
        criteria_regular = FilterCriteria(coverage=False)
        result = filter_suites(job, criteria_regular)
        assert len(result) == 2
        result_names = {suite.name for suite in result}
        assert "no_coverage_suite" in result_names
        assert "any_suite" in result_names
        assert "coverage_suite" not in result_names

        # Coverage build - should include coverage_suite and any_suite
        criteria_coverage = FilterCriteria(coverage=True)
        result = filter_suites(job, criteria_coverage)
        assert len(result) == 2
        result_names = {suite.name for suite in result}
        assert "coverage_suite" in result_names
        assert "any_suite" in result_names
        assert "no_coverage_suite" not in result_names

    def test_suite_coverage_overrides_job_coverage(self):
        """Suite-level coverage should override job-level coverage."""
        # Job with coverage=False, but suite overrides to True
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(
                    name="override_suite", requires=TestRequirements(coverage=True)
                )
            ],
            requires=TestRequirements(coverage=False),
        )

        # Job should be excluded from coverage build (job-level filter)
        criteria_coverage = FilterCriteria(coverage=True)
        assert not should_include_job(job, criteria_coverage)

        # But if we check the suite directly, it should be included
        suite = job.suites[0]
        assert should_include_suite(suite, criteria_coverage)

    def test_coverage_build_is_instrumented(self):
        """Coverage builds should be considered instrumented builds."""
        criteria = FilterCriteria(coverage=True)
        assert criteria.is_instrumented_build
        assert criteria.is_coverage_build


class TestV8Filtering:
    """Test v8 filtering at job and suite level."""

    def test_job_without_v8_constraint_included_in_all_builds(self):
        """Job without v8 field should run in all build types."""
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(),
        )

        # Should be included in V8 build
        criteria_v8 = FilterCriteria(v8=True)
        assert should_include_job(job, criteria_v8)

        # Should be included in non-V8 build
        criteria_no_v8 = FilterCriteria(v8=False)
        assert should_include_job(job, criteria_no_v8)

    def test_job_with_v8_true_only_in_v8_builds(self):
        """Job with v8=True should only run in V8-enabled builds."""
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(v8=True),
        )

        # Should be included in V8 build
        criteria_v8 = FilterCriteria(v8=True)
        assert should_include_job(job, criteria_v8)

        # Should be excluded from non-V8 build
        criteria_no_v8 = FilterCriteria(v8=False)
        assert not should_include_job(job, criteria_no_v8)

    def test_job_with_v8_false_only_in_non_v8_builds(self):
        """Job with v8=False should only run in non-V8 builds."""
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(v8=False),
        )

        # Should be excluded from V8 build
        criteria_v8 = FilterCriteria(v8=True)
        assert not should_include_job(job, criteria_v8)

        # Should be included in non-V8 build
        criteria_no_v8 = FilterCriteria(v8=False)
        assert should_include_job(job, criteria_no_v8)

    def test_suite_without_v8_constraint_included_in_all_builds(self):
        """Suite without v8 field should run in all build types."""
        suite = SuiteConfig(name="suite1")

        # Should be included in V8 build
        criteria_v8 = FilterCriteria(v8=True)
        assert should_include_suite(suite, criteria_v8)

        # Should be included in non-V8 build
        criteria_no_v8 = FilterCriteria(v8=False)
        assert should_include_suite(suite, criteria_no_v8)

    def test_suite_with_v8_true_only_in_v8_builds(self):
        """Suite with v8=True should only run in V8-enabled builds."""
        suite = SuiteConfig(name="suite1", requires=TestRequirements(v8=True))

        # Should be included in V8 build
        criteria_v8 = FilterCriteria(v8=True)
        assert should_include_suite(suite, criteria_v8)

        # Should be excluded from non-V8 build
        criteria_no_v8 = FilterCriteria(v8=False)
        assert not should_include_suite(suite, criteria_no_v8)

    def test_suite_with_v8_false_only_in_non_v8_builds(self):
        """Suite with v8=False should only run in non-V8 builds."""
        suite = SuiteConfig(name="suite1", requires=TestRequirements(v8=False))

        # Should be excluded from V8 build
        criteria_v8 = FilterCriteria(v8=True)
        assert not should_include_suite(suite, criteria_v8)

        # Should be included in non-V8 build
        criteria_no_v8 = FilterCriteria(v8=False)
        assert should_include_suite(suite, criteria_no_v8)

    def test_filter_suites_by_v8(self):
        """Test filtering mixed v8 suites in different build types."""
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(name="v8_suite", requires=TestRequirements(v8=True)),
                SuiteConfig(name="no_v8_suite", requires=TestRequirements(v8=False)),
                SuiteConfig(name="any_suite"),
            ],
            options=TestOptions(),
        )

        # V8 build - should include v8_suite and any_suite, exclude no_v8_suite
        criteria_v8 = FilterCriteria(v8=True)
        result = filter_suites(job, criteria_v8)
        assert len(result) == 2
        result_names = {suite.name for suite in result}
        assert "v8_suite" in result_names
        assert "any_suite" in result_names
        assert "no_v8_suite" not in result_names

        # Non-V8 build - should include no_v8_suite and any_suite, exclude v8_suite
        criteria_no_v8 = FilterCriteria(v8=False)
        result = filter_suites(job, criteria_no_v8)
        assert len(result) == 2
        result_names = {suite.name for suite in result}
        assert "no_v8_suite" in result_names
        assert "any_suite" in result_names
        assert "v8_suite" not in result_names

    def test_suite_v8_overrides_job_v8(self):
        """Suite-level v8 should override job-level v8."""
        # Job with v8=False, but suite overrides to True
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(name="override_suite", requires=TestRequirements(v8=True))
            ],
            requires=TestRequirements(v8=False),
        )

        # Job should be excluded from V8 build (job-level filter)
        criteria_v8 = FilterCriteria(v8=True)
        assert not should_include_job(job, criteria_v8)

        # But if we check the suite directly, it should be included
        suite = job.suites[0]
        assert should_include_suite(suite, criteria_v8)


class TestDeploymentTypeFiltering:
    """Test deployment type filtering at job level."""

    def test_job_without_deployment_type_included_in_all_filters(self):
        """Job without deployment_type field should run in all filters."""
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(deployment_type=None),
        )

        # Should be included with no filter
        criteria_no_filter = FilterCriteria(deployment_type=None)
        assert should_include_job(job, criteria_no_filter)

        # Should be included in single filter
        criteria_single = FilterCriteria(deployment_type=DeploymentType.SINGLE)
        assert should_include_job(job, criteria_single)

        # Should be included in cluster filter
        criteria_cluster = FilterCriteria(deployment_type=DeploymentType.CLUSTER)
        assert should_include_job(job, criteria_cluster)

    def test_job_with_single_deployment_filtered_on_cluster(self):
        """Job with deployment_type=SINGLE should be filtered out in cluster filter."""
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(deployment_type=DeploymentType.SINGLE),
        )

        # Should be included with no filter
        criteria_no_filter = FilterCriteria(deployment_type=None)
        assert should_include_job(job, criteria_no_filter)

        # Should be included in single filter
        criteria_single = FilterCriteria(deployment_type=DeploymentType.SINGLE)
        assert should_include_job(job, criteria_single)

        # Should be excluded in cluster filter
        criteria_cluster = FilterCriteria(deployment_type=DeploymentType.CLUSTER)
        assert not should_include_job(job, criteria_cluster)

    def test_job_with_cluster_deployment_filtered_on_single(self):
        """Job with deployment_type=CLUSTER should be filtered out in single filter."""
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(deployment_type=DeploymentType.CLUSTER),
        )

        # Should be included with no filter
        criteria_no_filter = FilterCriteria(deployment_type=None)
        assert should_include_job(job, criteria_no_filter)

        # Should be excluded in single filter
        criteria_single = FilterCriteria(deployment_type=DeploymentType.SINGLE)
        assert not should_include_job(job, criteria_single)

        # Should be included in cluster filter
        criteria_cluster = FilterCriteria(deployment_type=DeploymentType.CLUSTER)
        assert should_include_job(job, criteria_cluster)

    def test_job_with_mixed_deployment_included_in_all_filters(self):
        """Job with deployment_type=MIXED should be included in both single and cluster filters."""
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1"), SuiteConfig(name="suite2")],
            options=TestOptions(deployment_type=DeploymentType.MIXED),
        )

        # Should be included with no filter
        criteria_no_filter = FilterCriteria(deployment_type=None)
        assert should_include_job(job, criteria_no_filter)

        # Should be included in single filter
        criteria_single = FilterCriteria(deployment_type=DeploymentType.SINGLE)
        assert should_include_job(job, criteria_single)

        # Should be included in cluster filter
        criteria_cluster = FilterCriteria(deployment_type=DeploymentType.CLUSTER)
        assert should_include_job(job, criteria_cluster)

    def test_filter_jobs_by_deployment_type(self):
        """Test filtering multiple jobs by deployment type."""
        jobs = {
            "single_job": TestJob(
                name="single_job",
                suites=[SuiteConfig(name="suite1")],
                options=TestOptions(deployment_type=DeploymentType.SINGLE),
            ),
            "cluster_job": TestJob(
                name="cluster_job",
                suites=[SuiteConfig(name="suite2")],
                options=TestOptions(deployment_type=DeploymentType.CLUSTER),
            ),
            "mixed_job": TestJob(
                name="mixed_job",
                suites=[SuiteConfig(name="suite3"), SuiteConfig(name="suite4")],
                options=TestOptions(deployment_type=DeploymentType.MIXED),
            ),
            "any_job": TestJob(
                name="any_job",
                suites=[SuiteConfig(name="suite5")],
                options=TestOptions(deployment_type=None),
            ),
        }
        test_def = TestDefinitionFile(jobs=jobs)

        # No filter - should include all jobs
        criteria_no_filter = FilterCriteria(deployment_type=None)
        result = filter_jobs(test_def, criteria_no_filter)
        result_names = {job.name for job in result}
        assert len(result) == 4
        assert result_names == {"single_job", "cluster_job", "mixed_job", "any_job"}

        # Single filter - should include single_job, mixed_job, and any_job
        criteria_single = FilterCriteria(deployment_type=DeploymentType.SINGLE)
        result = filter_jobs(test_def, criteria_single)
        result_names = {job.name for job in result}
        assert len(result) == 3
        assert result_names == {"single_job", "mixed_job", "any_job"}

        # Cluster filter - should include cluster_job, mixed_job, and any_job
        criteria_cluster = FilterCriteria(deployment_type=DeploymentType.CLUSTER)
        result = filter_jobs(test_def, criteria_cluster)
        result_names = {job.name for job in result}
        assert len(result) == 3
        assert result_names == {"cluster_job", "mixed_job", "any_job"}
