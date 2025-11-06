"""
Unit tests for test_config_lib.py

Tests the core data models, validation logic, and parsing functions.
"""

import pytest
from src.config_lib import (
    DeploymentType,
    ResourceSize,
    TestOptions,
    TestArguments,
    SuiteConfig,
    RepositoryConfig,
    TestJob,
    TestDefinitionFile,
    BuildConfig,
)


class TestDeploymentType:
    """Test DeploymentType enum."""

    def test_from_string_valid(self):
        assert DeploymentType.from_string("single") == DeploymentType.SINGLE
        assert DeploymentType.from_string("SINGLE") == DeploymentType.SINGLE
        assert DeploymentType.from_string("cluster") == DeploymentType.CLUSTER
        assert DeploymentType.from_string("mixed") == DeploymentType.MIXED

    def test_from_string_invalid(self):
        with pytest.raises(ValueError, match="Invalid deployment type"):
            DeploymentType.from_string("invalid")


class TestResourceSize:
    """Test ResourceSize enum."""

    def test_from_string_valid(self):
        assert ResourceSize.from_string("small") == ResourceSize.SMALL
        assert ResourceSize.from_string("MEDIUM") == ResourceSize.MEDIUM
        assert ResourceSize.from_string("medium+") == ResourceSize.MEDIUM_PLUS
        assert ResourceSize.from_string("Large") == ResourceSize.LARGE
        assert ResourceSize.from_string("xlarge") == ResourceSize.XLARGE
        assert ResourceSize.from_string("2xlarge") == ResourceSize.XXLARGE

    def test_from_string_invalid(self):
        with pytest.raises(ValueError, match="Invalid resource size"):
            ResourceSize.from_string("tiny")


class TestTestOptions:
    """Test TestOptions dataclass."""

    def test_default_options(self):
        opts = TestOptions()
        assert opts.deployment_type is None
        assert opts.size is None
        assert opts.priority is None

    def test_valid_options(self):
        opts = TestOptions(
            deployment_type=DeploymentType.SINGLE,
            size=ResourceSize.MEDIUM,
            priority=5,
            parallelity=4,
            buckets=3,
        )
        assert opts.deployment_type == DeploymentType.SINGLE
        assert opts.size == ResourceSize.MEDIUM
        assert opts.priority == 5
        assert opts.parallelity == 4
        assert opts.buckets == 3

    def test_buckets_auto(self):
        opts = TestOptions(buckets="auto")
        assert opts.buckets == "auto"

    def test_invalid_priority(self):
        with pytest.raises(ValueError, match="priority must be non-negative"):
            TestOptions(priority=-1)

    def test_invalid_parallelity(self):
        with pytest.raises(ValueError, match="parallelity must be at least 1"):
            TestOptions(parallelity=0)

    def test_invalid_buckets_string(self):
        with pytest.raises(ValueError, match="buckets string must be 'auto'"):
            TestOptions(buckets="invalid")

    def test_invalid_buckets_zero(self):
        with pytest.raises(ValueError, match="buckets must be at least 1"):
            TestOptions(buckets=0)

    def test_invalid_buckets_type(self):
        with pytest.raises(ValueError, match="buckets must be int or 'auto'"):
            TestOptions(buckets=3.5)

    def test_replication_factor_validation(self):
        # Valid range
        opts = TestOptions(min_replication_factor=2, max_replication_factor=3)
        assert opts.min_replication_factor == 2
        assert opts.max_replication_factor == 3

        # Min > Max should fail
        with pytest.raises(ValueError, match="cannot be greater than"):
            TestOptions(min_replication_factor=5, max_replication_factor=3)

        # Negative should fail
        with pytest.raises(ValueError, match="must be at least 1"):
            TestOptions(min_replication_factor=0)

    def test_from_dict_empty(self):
        opts = TestOptions.from_dict(None)
        assert opts.deployment_type is None
        assert opts.size is None

    def test_from_dict_with_values(self):
        data = {
            "deployment_type": "cluster",
            "size": "large",
            "priority": 10,
            "buckets": 5,
            "storage_engine": "rocksdb",
        }
        opts = TestOptions.from_dict(data)
        assert opts.deployment_type == DeploymentType.CLUSTER
        assert opts.size == ResourceSize.LARGE
        assert opts.priority == 10
        assert opts.buckets == 5
        assert opts.storage_engine == "rocksdb"

    def test_merge_with_none(self):
        base = TestOptions(priority=5, buckets=3)
        merged = base.merge_with(None)
        assert merged.priority == 5
        assert merged.buckets == 3

    def test_merge_with_override(self):
        base = TestOptions(deployment_type=DeploymentType.SINGLE, priority=5, buckets=3)
        override = TestOptions(priority=10, size=ResourceSize.LARGE)
        merged = base.merge_with(override)
        assert merged.deployment_type == DeploymentType.SINGLE  # from base
        assert merged.priority == 10  # from override
        assert merged.buckets == 3  # from base
        assert merged.size == ResourceSize.LARGE  # from override

    def test_from_dict_default_size_cluster(self):
        """Test that cluster deployment defaults to medium size."""
        data = {"type": "cluster"}
        opts = TestOptions.from_dict(data)
        assert opts.deployment_type == DeploymentType.CLUSTER
        assert opts.size == ResourceSize.MEDIUM

    def test_from_dict_default_size_mixed(self):
        """Test that mixed deployment defaults to medium size."""
        data = {"type": "mixed"}
        opts = TestOptions.from_dict(data)
        assert opts.deployment_type == DeploymentType.MIXED
        assert opts.size == ResourceSize.MEDIUM

    def test_from_dict_default_size_single(self):
        """Test that single deployment defaults to small size."""
        data = {"type": "single"}
        opts = TestOptions.from_dict(data)
        assert opts.deployment_type == DeploymentType.SINGLE
        assert opts.size == ResourceSize.SMALL

    def test_from_dict_default_size_no_type(self):
        """Test that no deployment type defaults to small size."""
        data = {}
        opts = TestOptions.from_dict(data)
        assert opts.deployment_type is None
        assert opts.size == ResourceSize.SMALL

    def test_from_dict_explicit_size_overrides_default(self):
        """Test that explicit size overrides deployment-based default."""
        data = {"type": "cluster", "size": "xlarge"}
        opts = TestOptions.from_dict(data)
        assert opts.deployment_type == DeploymentType.CLUSTER
        assert opts.size == ResourceSize.XLARGE


class TestTestArguments:
    """Test TestArguments dataclass."""

    def test_default_arguments(self):
        args = TestArguments()
        assert args.extra_args == []
        assert args.arangosh_args == []

    def test_with_values(self):
        args = TestArguments(
            extra_args=["--arg1", "--arg2"], arangosh_args=["--js.arg"]
        )
        assert args.extra_args == ["--arg1", "--arg2"]
        assert args.arangosh_args == ["--js.arg"]

    def test_from_dict_empty(self):
        args = TestArguments.from_dict(None)
        assert args.extra_args == []
        assert args.arangosh_args == []

    def test_from_dict_with_lists(self):
        data = {
            "extra_args": ["--verbose", "--debug"],
            "arangosh_args": ["--javascript.execute"],
        }
        args = TestArguments.from_dict(data)
        assert args.extra_args == ["--verbose", "--debug"]
        assert args.arangosh_args == ["--javascript.execute"]

    def test_from_dict_with_string(self):
        # Single string should be converted to list
        data = {"extra_args": "--single-arg", "arangosh_args": "--js-arg"}
        args = TestArguments.from_dict(data)
        assert args.extra_args == ["--single-arg"]
        assert args.arangosh_args == ["--js-arg"]

    def test_merge_with_none(self):
        base = TestArguments(extra_args=["--base"])
        merged = base.merge_with(None)
        assert merged.extra_args == ["--base"]

    def test_merge_with_override(self):
        base = TestArguments(
            extra_args=["--base1", "--base2"], arangosh_args=["--js-base"]
        )
        override = TestArguments(
            extra_args=["--override"], arangosh_args=["--js-override"]
        )
        merged = base.merge_with(override)
        assert merged.extra_args == ["--base1", "--base2", "--override"]
        assert merged.arangosh_args == ["--js-base", "--js-override"]


class TestSuiteConfigClass:
    """Test SuiteConfig dataclass."""

    def test_simple_suite(self):
        suite = SuiteConfig(name="boost")
        assert suite.name == "boost"
        assert suite.options.deployment_type is None

    def test_suite_with_options(self):
        suite = SuiteConfig(
            name="boost",
            options=TestOptions(priority=10),
            arguments=TestArguments(extra_args=["--verbose"]),
        )
        assert suite.name == "boost"
        assert suite.options.priority == 10
        assert suite.arguments.extra_args == ["--verbose"]

    def test_invalid_name(self):
        with pytest.raises(ValueError, match="Suite name must be a non-empty string"):
            SuiteConfig(name="")

    def test_from_dict(self):
        data = {
            "name": "resilience",
            "options": {"priority": 15},
            "arguments": {"extra_args": ["--arg"]},
        }
        suite = SuiteConfig.from_dict(data)
        assert suite.name == "resilience"
        assert suite.options.priority == 15
        assert suite.arguments.extra_args == ["--arg"]

    def test_from_dict_missing_name(self):
        with pytest.raises(ValueError, match="must have 'name' field"):
            SuiteConfig.from_dict({"options": {}})

    def test_with_merged_options(self):
        suite = SuiteConfig(
            name="boost",
            options=TestOptions(priority=10),
            arguments=TestArguments(extra_args=["--suite-arg"]),
        )
        job_opts = TestOptions(
            deployment_type=DeploymentType.CLUSTER, priority=5, size=ResourceSize.MEDIUM
        )
        job_args = TestArguments(extra_args=["--job-arg"])

        merged = suite.with_merged_options(job_opts, job_args)
        assert merged.name == "boost"
        assert merged.options.deployment_type == DeploymentType.CLUSTER
        assert merged.options.priority == 10  # suite override
        assert merged.options.size == ResourceSize.MEDIUM
        assert merged.arguments.extra_args == ["--job-arg", "--suite-arg"]


class TestRepositoryConfigClass:
    """Test RepositoryConfig dataclass."""

    def test_basic_repo(self):
        repo = RepositoryConfig(git_repo="https://github.com/example/repo")
        assert repo.git_repo == "https://github.com/example/repo"
        assert repo.git_branch is None

    def test_repo_with_branch(self):
        repo = RepositoryConfig(
            git_repo="https://github.com/example/repo", git_branch="main"
        )
        assert repo.git_branch == "main"

    def test_invalid_repo(self):
        with pytest.raises(ValueError, match="git_repo must be a non-empty string"):
            RepositoryConfig(git_repo="")

    def test_from_dict_none(self):
        repo = RepositoryConfig.from_dict(None)
        assert repo is None

    def test_from_dict_with_values(self):
        data = {
            "git_repo": "https://github.com/example/repo",
            "git_branch": "develop",
        }
        repo = RepositoryConfig.from_dict(data)
        assert repo.git_repo == "https://github.com/example/repo"
        assert repo.git_branch == "develop"


class TestTestJob:
    """Test TestJob dataclass."""

    def test_single_suite_job(self):
        job = TestJob(
            name="test-single",
            suites=[SuiteConfig(name="boost")],
            options=TestOptions(deployment_type=DeploymentType.SINGLE),
        )
        assert job.name == "test-single"
        assert len(job.suites) == 1
        assert job.suites[0].name == "boost"
        assert not job.is_multi_suite()

    def test_multi_suite_job(self):
        job = TestJob(
            name="test-multi",
            suites=[SuiteConfig(name="boost"), SuiteConfig(name="resilience")],
            options=TestOptions(buckets="auto"),
        )
        assert job.name == "test-multi"
        assert len(job.suites) == 2
        assert job.is_multi_suite()

    def test_empty_suites_invalid(self):
        with pytest.raises(ValueError, match="must have at least one suite"):
            TestJob(name="test", suites=[])

    def test_mixed_only_for_multi_suite(self):
        with pytest.raises(ValueError, match="only valid for multi-suite jobs"):
            TestJob(
                name="test",
                suites=[SuiteConfig(name="boost")],
                options=TestOptions(deployment_type=DeploymentType.MIXED),
            )

    def test_mixed_allows_suite_without_explicit_deployment_type(self):
        # Mixed jobs now allow suites without explicit deployment_type
        # (deployment type can be inferred from args in practice)
        job = TestJob(
            name="test",
            suites=[SuiteConfig(name="boost"), SuiteConfig(name="resilience")],
            options=TestOptions(deployment_type=DeploymentType.MIXED),
        )
        assert job.options.deployment_type == DeploymentType.MIXED
        assert len(job.suites) == 2

    def test_single_cluster_no_suite_override(self):
        with pytest.raises(ValueError, match="Cannot override deployment_type"):
            TestJob(
                name="test",
                suites=[
                    SuiteConfig(
                        name="boost",
                        options=TestOptions(deployment_type=DeploymentType.CLUSTER),
                    )
                ],
                options=TestOptions(deployment_type=DeploymentType.SINGLE),
            )

    def test_multi_suite_buckets_must_be_auto(self):
        with pytest.raises(ValueError, match="must use buckets='auto'"):
            TestJob(
                name="test",
                suites=[SuiteConfig(name="boost"), SuiteConfig(name="resilience")],
                options=TestOptions(buckets=5),
            )

    def test_single_suite_cannot_use_auto_buckets(self):
        with pytest.raises(ValueError, match="cannot use buckets='auto'"):
            TestJob(
                name="test",
                suites=[SuiteConfig(name="boost")],
                options=TestOptions(buckets="auto"),
            )

    def test_from_dict_single_suite(self):
        data = {
            "suite": "boost",
            "options": {"deployment_type": "single", "priority": 10},
        }
        job = TestJob.from_dict("test-job", data)
        assert job.name == "test-job"
        assert len(job.suites) == 1
        assert job.suites[0].name == "boost"
        assert job.options.deployment_type == DeploymentType.SINGLE
        assert job.options.priority == 10

    def test_from_dict_multi_suite_simple(self):
        data = {"suites": ["boost", "resilience", "replication2"]}
        job = TestJob.from_dict("test-job", data)
        assert job.name == "test-job"
        assert len(job.suites) == 3
        assert job.suites[0].name == "boost"
        assert job.suites[1].name == "resilience"

    def test_from_dict_multi_suite_with_options(self):
        data = {
            "suites": ["boost", {"name": "resilience", "options": {"priority": 20}}],
            "options": {"buckets": "auto"},
        }
        job = TestJob.from_dict("test-job", data)
        assert len(job.suites) == 2
        assert job.suites[1].name == "resilience"
        assert job.suites[1].options.priority == 20

    def test_from_dict_missing_suite_field(self):
        # When neither 'suite' nor 'suites' is specified, job name is used as suite name
        job = TestJob.from_dict("test-job", {})
        assert job.name == "test-job"
        assert len(job.suites) == 1
        assert job.suites[0].name == "test-job"

    def test_get_bucket_count(self):
        # No buckets
        job1 = TestJob(name="test1", suites=[SuiteConfig(name="boost")])
        assert job1.get_bucket_count() is None

        # Explicit buckets
        job2 = TestJob(
            name="test2",
            suites=[SuiteConfig(name="boost")],
            options=TestOptions(buckets=5),
        )
        assert job2.get_bucket_count() == 5

        # Auto buckets
        job3 = TestJob(
            name="test3",
            suites=[
                SuiteConfig(name="boost"),
                SuiteConfig(name="resilience"),
                SuiteConfig(name="replication2"),
            ],
            options=TestOptions(buckets="auto"),
        )
        assert job3.get_bucket_count() == 3

    def test_get_resolved_suites(self):
        job = TestJob(
            name="test",
            suites=[
                SuiteConfig(name="boost"),
                SuiteConfig(name="resilience", options=TestOptions(priority=20)),
            ],
            options=TestOptions(
                deployment_type=DeploymentType.CLUSTER,
                priority=10,
                size=ResourceSize.MEDIUM,
            ),
            arguments=TestArguments(extra_args=["--job-arg"]),
        )

        resolved = job.get_resolved_suites()
        assert len(resolved) == 2

        # First suite inherits all job options
        assert resolved[0].name == "boost"
        assert resolved[0].options.deployment_type == DeploymentType.CLUSTER
        assert resolved[0].options.priority == 10
        assert resolved[0].options.size == ResourceSize.MEDIUM

        # Second suite overrides priority
        assert resolved[1].name == "resilience"
        assert resolved[1].options.priority == 20
        assert resolved[1].options.deployment_type == DeploymentType.CLUSTER


class TestTestDefinitionFile:
    """Test TestDefinitionFile dataclass."""

    def test_empty_jobs_invalid(self):
        with pytest.raises(ValueError, match="must contain at least one job"):
            TestDefinitionFile(jobs={})

    def test_from_dict(self):
        data = {
            "job1": {"suite": "boost", "options": {"deployment_type": "single"}},
            "job2": {
                "suites": ["resilience", "replication2"],
                "options": {"buckets": "auto"},
            },
        }
        test_file = TestDefinitionFile.from_dict(data)
        assert len(test_file.jobs) == 2
        assert "job1" in test_file.jobs
        assert "job2" in test_file.jobs

    def test_get_job_names(self):
        data = {
            "zebra": {"suite": "boost"},
            "alpha": {"suite": "resilience"},
            "beta": {"suite": "replication2"},
        }
        test_file = TestDefinitionFile.from_dict(data)
        names = test_file.get_job_names()
        assert names == ["alpha", "beta", "zebra"]  # sorted

    def test_filter_by_deployment_type(self):
        data = {
            "single-job": {"suite": "boost", "options": {"deployment_type": "single"}},
            "cluster-job": {
                "suite": "resilience",
                "options": {"deployment_type": "cluster"},
            },
        }
        test_file = TestDefinitionFile.from_dict(data)
        filtered = test_file.filter_jobs(deployment_type=DeploymentType.SINGLE)
        assert len(filtered) == 1
        assert "single-job" in filtered

    def test_filter_by_size(self):
        data = {
            "small-job": {"suite": "boost", "options": {"size": "small"}},
            "large-job": {"suite": "resilience", "options": {"size": "large"}},
        }
        test_file = TestDefinitionFile.from_dict(data)
        filtered = test_file.filter_jobs(size=ResourceSize.LARGE)
        assert len(filtered) == 1
        assert "large-job" in filtered

    def test_filter_by_repository(self):
        data = {
            "driver-job": {
                "suite": "boost",
                "repository": {"git_repo": "https://github.com/example/driver"},
            },
            "normal-job": {"suite": "resilience"},
        }
        test_file = TestDefinitionFile.from_dict(data)

        # Jobs with repository
        filtered = test_file.filter_jobs(has_repository=True)
        assert len(filtered) == 1
        assert "driver-job" in filtered

        # Jobs without repository
        filtered = test_file.filter_jobs(has_repository=False)
        assert len(filtered) == 1
        assert "normal-job" in filtered

    def test_filter_by_suite_name(self):
        data = {
            "job1": {"suite": "boost"},
            "job2": {"suites": ["resilience", "replication2"]},
            "job3": {"suite": "aql"},
        }
        test_file = TestDefinitionFile.from_dict(data)
        filtered = test_file.filter_jobs(suite_name="resilience")
        assert len(filtered) == 1
        assert "job2" in filtered


class TestBuildConfigClass:
    """Test BuildConfig dataclass."""

    def test_basic_build(self):
        build = BuildConfig(architecture="amd64", enterprise=True)
        assert build.architecture == "amd64"
        assert build.enterprise is True
        assert build.sanitizer is None
        assert build.nightly is False

    def test_with_sanitizer(self):
        build = BuildConfig(
            architecture="amd64", enterprise=True, sanitizer="asan", nightly=True
        )
        assert build.sanitizer == "asan"
        assert build.nightly is True

    def test_empty_architecture(self):
        with pytest.raises(ValueError, match="architecture must be specified"):
            BuildConfig(architecture="", enterprise=True)
