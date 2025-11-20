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
    DeploymentType,
    TestDefinitionFile,
    BuildConfig,
)
from src.filters import (
    PlatformFlags,
    FilterCriteria,
    should_include_job,
    should_include_suite,
    filter_jobs,
    filter_suites,
)


class TestPlatformFlags:
    """Test PlatformFlags dataclass."""

    def test_default_values(self):
        """Test that all flags default to False."""
        flags = PlatformFlags()
        assert flags.is_windows is False
        assert flags.is_arm is False
        assert flags.is_coverage is False

    def test_custom_values(self):
        """Test setting custom flag values."""
        flags = PlatformFlags(is_windows=True, is_arm=True)
        assert flags.is_windows is True
        assert flags.is_arm is True
        assert flags.is_coverage is False


class TestFilterCriteria:
    """Test FilterCriteria dataclass."""

    def test_default_values(self):
        """Test default filter criteria values."""
        criteria = FilterCriteria()
        assert criteria.cluster is False
        assert criteria.single is False
        assert criteria.all_tests is False
        assert criteria.full is False
        assert criteria.nightly is False
        assert criteria.gtest is False
        assert criteria.sanitizer is None
        assert criteria.enterprise is True
        assert criteria.platform is not None
        assert isinstance(criteria.platform, PlatformFlags)

    def test_custom_values(self):
        """Test setting custom criteria values."""
        from src.config_lib import Sanitizer

        platform = PlatformFlags(is_windows=True)
        criteria = FilterCriteria(
            cluster=True,
            full=True,
            sanitizer=Sanitizer.TSAN,
            enterprise=False,
            platform=platform,
        )
        assert criteria.cluster is True
        assert criteria.single is False
        assert criteria.full is True
        assert criteria.sanitizer == Sanitizer.TSAN
        assert criteria.enterprise is False
        assert criteria.platform.is_windows is True

    def test_is_full_run(self):
        """Test is_full_run property."""
        assert FilterCriteria().is_full_run is False
        assert FilterCriteria(full=True).is_full_run is True
        assert FilterCriteria(nightly=True).is_full_run is True
        assert FilterCriteria(full=True, nightly=True).is_full_run is True


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
        suite = SuiteConfig(name="full_suite", options=TestOptions(full=True))
        assert should_include_suite(suite, criteria) is True

        # Suite with full=False
        suite = SuiteConfig(name="pr_suite", options=TestOptions(full=False))
        assert should_include_suite(suite, criteria) is True

        # Suite with full=None
        suite = SuiteConfig(name="any_suite", options=TestOptions())
        assert should_include_suite(suite, criteria) is True

    @pytest.mark.parametrize(
        "full,nightly,suite_full,expected",
        [
            # PR builds (full=False, nightly=False)
            (False, False, False, True),  # PR suite included
            (False, False, None, True),  # Any suite included
            (False, False, True, False),  # Full suite excluded
            # Full builds (full=True)
            (True, False, True, True),  # Full suite included
            (True, False, None, True),  # Any suite included
            (True, False, False, False),  # PR suite excluded
            # Nightly builds (nightly=True, implies full run)
            (False, True, True, True),  # Full suite included
            (False, True, None, True),  # Any suite included
            (False, True, False, False),  # PR suite excluded
        ],
    )
    def test_suite_filtering_by_build_type(self, full, nightly, suite_full, expected):
        """Test suite filtering across PR/full/nightly builds."""
        criteria = FilterCriteria(full=full, nightly=nightly)
        suite = SuiteConfig(name="test", options=TestOptions(full=suite_full))
        assert should_include_suite(suite, criteria) is expected

    def test_suite_without_options(self):
        """Test suite with no options field."""
        # Suite without options should always be included (unless in full build)
        suite = SuiteConfig(name="simple_suite")

        # PR build
        criteria = FilterCriteria(full=False, nightly=False)
        assert should_include_suite(suite, criteria) is True

        # Full build
        criteria = FilterCriteria(full=True)
        assert should_include_suite(suite, criteria) is True

        # Nightly build
        criteria = FilterCriteria(nightly=True)
        assert should_include_suite(suite, criteria) is True


class TestFilterSuites:
    """Test filter_suites function."""

    def test_filter_mixed_suites_in_pr_build(self, mixed_suite_job):
        """Test filtering a mix of PR and full suites in PR build."""
        criteria = FilterCriteria(full=False, nightly=False)
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
            suites=[SuiteConfig(name="only_suite", options=TestOptions(full=True))],
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
            options=TestOptions(architecture=Architecture.X64),
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
            options=TestOptions(architecture=Architecture.AARCH64),
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
            options=TestOptions(architecture=Architecture.X64),
        )
        suite_aarch64_only = SuiteConfig(
            name="suite_aarch64",
            options=TestOptions(architecture=Architecture.AARCH64),
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
            options=TestOptions(architecture=Architecture.X64),
        )

        # Should NOT be included on wrong architecture, even with all_tests=True
        criteria = FilterCriteria(
            all_tests=True,
            architecture=Architecture.AARCH64
        )
        assert not should_include_job(job, criteria)

        # Should be included on correct architecture with all_tests=True
        criteria = FilterCriteria(
            all_tests=True,
            architecture=Architecture.X64
        )
        assert should_include_job(job, criteria)

    def test_architecture_filter_without_criteria_architecture(self):
        """If criteria doesn't specify architecture, don't filter."""
        from src.config_lib import Architecture

        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(architecture=Architecture.X64),
        )

        # No architecture in criteria - should include
        criteria = FilterCriteria(architecture=None)
        assert should_include_job(job, criteria)
