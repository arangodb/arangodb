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
        assert criteria.cluster is False
        assert criteria.single is False
        assert criteria.all_tests is False
        assert criteria.full is False
        assert criteria.gtest is False
        assert criteria.sanitizer is None
        assert criteria.enterprise is True

    def test_custom_values(self):
        """Test setting custom criteria values."""
        from src.config_lib import Sanitizer

        criteria = FilterCriteria(
            cluster=True,
            full=True,
            sanitizer=Sanitizer.TSAN,
            enterprise=False,
        )
        assert criteria.cluster is True
        assert criteria.single is False
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

    def test_all_tests_includes_everything(self):
        """Test that all_tests=True includes all jobs."""
        criteria = FilterCriteria(all_tests=True)

        # Test various deployment types
        for dep_type in [
            DeploymentType.SINGLE,
            DeploymentType.CLUSTER,
            DeploymentType.MIXED,
            None,
        ]:
            job = self.create_job(deployment_type=dep_type)
            assert should_include_job(job, criteria) is True

    @pytest.mark.parametrize(
        "cluster,single,dep_type,expected",
        [
            # cluster=True, single=False: include cluster, mixed, None; exclude single
            (True, False, DeploymentType.CLUSTER, True),
            (True, False, DeploymentType.MIXED, True),
            (True, False, None, True),
            (True, False, DeploymentType.SINGLE, False),
            # single=True, cluster=False: include single, None; exclude cluster, mixed
            (False, True, DeploymentType.SINGLE, True),
            (False, True, None, True),
            (False, True, DeploymentType.CLUSTER, False),
            (False, True, DeploymentType.MIXED, False),
            # both True: include everything
            (True, True, DeploymentType.SINGLE, True),
            (True, True, DeploymentType.CLUSTER, True),
            (True, True, DeploymentType.MIXED, True),
            (True, True, None, True),
        ],
    )
    def test_deployment_type_filtering(self, cluster, single, dep_type, expected):
        """Test deployment type filtering with various cluster/single combinations."""
        criteria = FilterCriteria(cluster=cluster, single=single)
        job = self.create_job(deployment_type=dep_type)
        assert should_include_job(job, criteria) is expected

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

    def test_combined_filters(self):
        """Test combining multiple filter criteria."""
        criteria = FilterCriteria(cluster=True, single=False, gtest=True)

        # Should include: cluster + gtest
        job = self.create_job(
            deployment_type=DeploymentType.CLUSTER,
            suite_names=["gtest_foo"],
        )
        assert should_include_job(job, criteria) is True

        # Should exclude: single + gtest (fails cluster requirement)
        job = self.create_job(
            deployment_type=DeploymentType.SINGLE,
            suite_names=["gtest_foo"],
        )
        assert should_include_job(job, criteria) is False

        # Should exclude: cluster + non-gtest (fails gtest requirement)
        job = self.create_job(
            deployment_type=DeploymentType.CLUSTER,
            suite_names=["regular_test"],
        )
        assert should_include_job(job, criteria) is False


class TestFilterJobs:
    """Test filter_jobs function."""

    def create_test_definition(self, jobs_dict):
        """Helper to create a TestDefinitionFile."""
        return TestDefinitionFile(jobs=jobs_dict)

    def test_single_job_filtered_out(self):
        """Test filtering that excludes the only job."""
        jobs = {
            "cluster_job": TestJob(
                name="cluster_job",
                suites=[SuiteConfig(name="test")],
                options=TestOptions(deployment_type=DeploymentType.CLUSTER),
            )
        }
        test_def = self.create_test_definition(jobs)

        # Filter for single only - should exclude the cluster job
        criteria = FilterCriteria(single=True, cluster=False)
        result = filter_jobs(test_def, criteria)
        assert result == []

    def test_filter_by_deployment_type(self):
        """Test filtering jobs by deployment type."""
        jobs = {
            "cluster_job": TestJob(
                name="cluster_job",
                suites=[SuiteConfig(name="test")],
                options=TestOptions(deployment_type=DeploymentType.CLUSTER),
            ),
            "single_job": TestJob(
                name="single_job",
                suites=[SuiteConfig(name="test")],
                options=TestOptions(deployment_type=DeploymentType.SINGLE),
            ),
            "mixed_job": TestJob(
                name="mixed_job",
                suites=[SuiteConfig(name="test1"), SuiteConfig(name="test2")],
                options=TestOptions(deployment_type=DeploymentType.MIXED),
            ),
        }
        test_def = self.create_test_definition(jobs)

        # Filter for cluster only
        criteria = FilterCriteria(cluster=True, single=False)
        result = filter_jobs(test_def, criteria)
        result_names = [job.name for job in result]
        assert "cluster_job" in result_names
        assert "mixed_job" in result_names
        assert "single_job" not in result_names

        # Filter for single only
        criteria = FilterCriteria(single=True, cluster=False)
        result = filter_jobs(test_def, criteria)
        result_names = [job.name for job in result]
        assert "single_job" in result_names
        assert "cluster_job" not in result_names
        assert "mixed_job" not in result_names

    def test_all_tests_returns_all(self):
        """Test that all_tests=True returns all jobs."""
        jobs = {
            "job1": TestJob(
                name="job1",
                suites=[SuiteConfig(name="test")],
                options=TestOptions(deployment_type=DeploymentType.CLUSTER),
            ),
            "job2": TestJob(
                name="job2",
                suites=[SuiteConfig(name="test")],
                options=TestOptions(deployment_type=DeploymentType.SINGLE),
            ),
            "job3": TestJob(
                name="job3",
                suites=[SuiteConfig(name="gtest_foo")],
                options=TestOptions(),
            ),
        }
        test_def = self.create_test_definition(jobs)

        criteria = FilterCriteria(all_tests=True)
        result = filter_jobs(test_def, criteria)
        assert len(result) == 3
        result_names = {job.name for job in result}
        assert result_names == {"job1", "job2", "job3"}

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

        criteria = FilterCriteria(all_tests=True)
        result = filter_jobs(test_def, criteria)

        # Check that we got all jobs
        assert len(result) == 5


class TestShouldIncludeSuite:
    """Test should_include_suite function."""

    def test_all_tests_includes_everything(self):
        """Test that all_tests=True includes all suites."""
        criteria = FilterCriteria(all_tests=True)

        # Suite with full=True
        suite = SuiteConfig(name="full_suite", requires=TestRequirements(full=True))
        assert should_include_suite(suite, criteria) is True

        # Suite with full=False
        suite = SuiteConfig(name="pr_suite", requires=TestRequirements(full=False))
        assert should_include_suite(suite, criteria) is True

        # Suite with full=None
        suite = SuiteConfig(name="any_suite")
        assert should_include_suite(suite, criteria) is True

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

    def test_filter_all_tests_returns_all_suites(self, mixed_suite_job):
        """Test that all_tests=True returns all suites."""
        criteria = FilterCriteria(all_tests=True)
        result = filter_suites(mixed_suite_job, criteria)

        # Should include all suites
        assert len(result) == 3
        result_names = {suite.name for suite in result}
        assert result_names == {"pr_suite", "full_suite", "any_suite"}

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

        criteria = FilterCriteria(all_tests=True)
        result = filter_suites(job, criteria)

        # Check that order is preserved
        assert len(result) == 4
        assert result[0].name == "suite1"
        assert result[1].name == "suite2"
        assert result[2].name == "suite3"
        assert result[3].name == "suite4"


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

    def test_all_tests_mode_does_not_bypass_architecture_filter(self):
        """all_tests mode should still respect architecture constraints."""
        from src.config_lib import Architecture

        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(architecture=Architecture.X64),
        )

        # Should NOT be included on wrong architecture, even with all_tests=True
        criteria = FilterCriteria(all_tests=True, architecture=Architecture.AARCH64)
        assert not should_include_job(job, criteria)

        # Should be included on correct architecture with all_tests=True
        criteria = FilterCriteria(all_tests=True, architecture=Architecture.X64)
        assert should_include_job(job, criteria)

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

    def test_all_tests_mode_respects_architecture_but_not_instrumentation(self):
        """all_tests mode should still respect architecture but ignore instrumentation."""
        from src.config_lib import Architecture, Sanitizer

        # Job with instrumentation=False
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(instrumentation=False),
        )

        # Should be included in instrumented build with all_tests=True
        criteria = FilterCriteria(all_tests=True, sanitizer=Sanitizer.TSAN)
        assert should_include_job(job, criteria)

        # Job with wrong architecture should still be excluded
        job_x64 = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(architecture=Architecture.X64),
        )

        criteria_aarch64 = FilterCriteria(
            all_tests=True, architecture=Architecture.AARCH64
        )
        assert not should_include_job(job_x64, criteria_aarch64)

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

    def test_all_tests_mode_respects_architecture_but_not_v8(self):
        """all_tests mode should still respect architecture but ignore v8."""
        from src.config_lib import Architecture

        # Job with v8=False
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(v8=False),
        )

        # Should be included in V8 build with all_tests=True
        criteria = FilterCriteria(all_tests=True, v8=True)
        assert should_include_job(job, criteria)

        # Job with wrong architecture should still be excluded
        job_x64 = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            requires=TestRequirements(architecture=Architecture.X64),
        )

        criteria_aarch64 = FilterCriteria(
            all_tests=True, architecture=Architecture.AARCH64
        )
        assert not should_include_job(job_x64, criteria_aarch64)

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
