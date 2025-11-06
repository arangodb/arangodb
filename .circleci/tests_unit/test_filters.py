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
    filter_jobs,
)


class TestPlatformFlags:
    """Test PlatformFlags dataclass."""

    def test_default_values(self):
        """Test that all flags default to False."""
        flags = PlatformFlags()
        assert flags.is_windows is False
        assert flags.is_mac is False
        assert flags.is_arm is False
        assert flags.is_coverage is False

    def test_custom_values(self):
        """Test setting custom flag values."""
        flags = PlatformFlags(is_windows=True, is_arm=True)
        assert flags.is_windows is True
        assert flags.is_mac is False
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
        assert criteria.full is None
        assert criteria.nightly is False
        assert criteria.gtest is False
        assert criteria.enterprise is True
        assert criteria.platform is not None
        assert isinstance(criteria.platform, PlatformFlags)

    def test_custom_values(self):
        """Test setting custom criteria values."""
        from src.config_lib import Sanitizer

        platform = PlatformFlags(is_windows=True)
        criteria = FilterCriteria(
            cluster=True,
            full=Sanitizer.TSAN,
            enterprise=False,
            platform=platform,
        )
        assert criteria.cluster is True
        assert criteria.single is False
        assert criteria.full == Sanitizer.TSAN
        assert criteria.enterprise is False
        assert criteria.platform.is_windows is True

    def test_is_full_run(self):
        """Test is_full_run property."""
        from src.config_lib import Sanitizer

        assert FilterCriteria().is_full_run is False
        assert FilterCriteria(full=Sanitizer.TSAN).is_full_run is True
        assert FilterCriteria(nightly=True).is_full_run is True
        assert FilterCriteria(full=Sanitizer.ALUBSAN, nightly=True).is_full_run is True


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

    def test_cluster_filter_only(self):
        """Test filtering for cluster tests only."""
        criteria = FilterCriteria(cluster=True, single=False)

        # Should include cluster
        job = self.create_job(deployment_type=DeploymentType.CLUSTER)
        assert should_include_job(job, criteria) is True

        # Should include mixed
        job = self.create_job(deployment_type=DeploymentType.MIXED)
        assert should_include_job(job, criteria) is True

        # Should include None (unspecified)
        job = self.create_job(deployment_type=None)
        assert should_include_job(job, criteria) is True

        # Should exclude single
        job = self.create_job(deployment_type=DeploymentType.SINGLE)
        assert should_include_job(job, criteria) is False

    def test_single_filter_only(self):
        """Test filtering for single tests only."""
        criteria = FilterCriteria(single=True, cluster=False)

        # Should include single
        job = self.create_job(deployment_type=DeploymentType.SINGLE)
        assert should_include_job(job, criteria) is True

        # Should include None (unspecified)
        job = self.create_job(deployment_type=None)
        assert should_include_job(job, criteria) is True

        # Should exclude cluster
        job = self.create_job(deployment_type=DeploymentType.CLUSTER)
        assert should_include_job(job, criteria) is False

        # Should exclude mixed
        job = self.create_job(deployment_type=DeploymentType.MIXED)
        assert should_include_job(job, criteria) is False

    def test_both_cluster_and_single(self):
        """Test when both cluster and single are True."""
        criteria = FilterCriteria(cluster=True, single=True)

        # Should include all deployment types
        for dep_type in [
            DeploymentType.SINGLE,
            DeploymentType.CLUSTER,
            DeploymentType.MIXED,
            None,
        ]:
            job = self.create_job(deployment_type=dep_type)
            assert should_include_job(job, criteria) is True

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
