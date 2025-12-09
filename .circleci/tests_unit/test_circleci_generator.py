"""
Tests for CircleCI configuration generator.

This module tests the CircleCI workflow generation functionality including
workflow creation, job creation, and special case handling.
"""

import pytest
from datetime import date
from src.config_lib import (
    TestJob,
    SuiteConfig,
    TestOptions,
    TestRequirements,
    TestDefinitionFile,
    BuildConfig,
    DeploymentType,
    ResourceSize,
    Sanitizer,
    Architecture,
)
from src.filters import FilterCriteria
from src.output_generators.base import (
    GeneratorConfig,
    TestExecutionConfig,
    CircleCIConfig,
)
from src.output_generators.circleci import CircleCIGenerator


class TestCircleCIGeneratorInit:
    """Test CircleCIGenerator initialization."""

    def test_init_with_base_config_path(self):
        """Test initialization with base config path."""
        config = GeneratorConfig(filter_criteria=FilterCriteria())
        gen = CircleCIGenerator(config, base_config_path="/path/to/config.yml")

        assert gen.base_config_path == "/path/to/config.yml"
        assert gen._base_config is None
        assert gen.env_getter is not None
        assert gen.date_provider is not None

    def test_init_with_base_config_dict(self):
        """Test initialization with pre-loaded config dict."""
        config = GeneratorConfig(filter_criteria=FilterCriteria())
        base_config = {"version": 2.1, "workflows": {}}
        gen = CircleCIGenerator(config, base_config=base_config)

        assert gen._base_config == base_config
        assert gen.base_config_path is None

    def test_init_with_custom_env_getter(self):
        """Test initialization with custom environment getter."""
        config = GeneratorConfig(filter_criteria=FilterCriteria())
        env_getter = lambda k, default: f"test-{k}"
        gen = CircleCIGenerator(config, base_config={}, env_getter=env_getter)

        assert gen.env_getter("CIRCLE_BRANCH", "default") == "test-CIRCLE_BRANCH"

    def test_init_with_custom_date_provider(self):
        """Test initialization with custom date provider."""
        config = GeneratorConfig(filter_criteria=FilterCriteria())
        test_date = date(2025, 1, 15)
        date_provider = lambda: test_date
        gen = CircleCIGenerator(config, base_config={}, date_provider=date_provider)

        assert gen.date_provider() == test_date


class TestGenerateWorkflowName:
    """Test workflow name generation."""

    def create_generator(self, **config_kwargs):
        """Helper to create generator with config."""
        filter_criteria = FilterCriteria(**config_kwargs.get("filter", {}))
        test_exec = TestExecutionConfig(
            replication_two=config_kwargs.get("replication_two", False)
        )
        circleci_config = CircleCIConfig(test_image="default")
        config = GeneratorConfig(
            filter_criteria=filter_criteria,
            test_execution=test_exec,
            circleci=circleci_config,
        )
        return CircleCIGenerator(config, base_config={})

    def test_workflow_name_x64_pr(self):
        """Test workflow name for x64 PR build."""
        gen = self.create_generator()
        build_config = BuildConfig(architecture=Architecture.X64)

        name = gen._generate_workflow_name(build_config)
        assert name == "x64-pr"

    def test_workflow_name_x64_nightly(self):
        """Test workflow name for x64 nightly build."""
        gen = self.create_generator()
        build_config = BuildConfig(architecture=Architecture.X64, nightly=True)

        name = gen._generate_workflow_name(build_config)
        assert name == "x64-nightly"

    def test_workflow_name_with_sanitizer(self):
        """Test workflow name includes sanitizer."""
        gen = self.create_generator()
        build_config = BuildConfig(
            architecture=Architecture.X64, sanitizer=Sanitizer.TSAN
        )

        name = gen._generate_workflow_name(build_config)
        assert name == "x64-pr-tsan"

    def test_workflow_name_aarch64(self):
        """Test workflow name for ARM architecture."""
        gen = self.create_generator()
        build_config = BuildConfig(architecture=Architecture.AARCH64)

        name = gen._generate_workflow_name(build_config)
        assert name == "aarch64-pr"

    def test_workflow_name_with_replication_two(self):
        """Test workflow name with replication version 2."""
        gen = self.create_generator(replication_two=True)
        build_config = BuildConfig(architecture=Architecture.X64)

        name = gen._generate_workflow_name(build_config)
        assert name == "x64-pr-repl2"


class TestCreateBuildJob:
    """Test build job creation."""

    def create_generator(self):
        """Helper to create generator."""
        config = GeneratorConfig(filter_criteria=FilterCriteria())
        return CircleCIGenerator(config, base_config={})

    def test_create_build_job_x64(self):
        """Test creating x64 build job."""
        gen = self.create_generator()
        build_config = BuildConfig(architecture=Architecture.X64)

        job = gen._create_build_job(build_config)

        assert "compile-linux" in job
        params = job["compile-linux"]
        assert params["name"] == "build-x64"
        assert params["preset"] == "enterprise-pr"
        assert params["enterprise"] is True
        assert params["arch"] == "x64"
        assert params["resource-class"] == "2xlarge"
        assert "s3-prefix" not in params

    def test_create_build_job_with_sanitizer(self):
        """Test creating build job with sanitizer."""
        gen = self.create_generator()
        build_config = BuildConfig(
            architecture=Architecture.X64, sanitizer=Sanitizer.TSAN
        )

        job = gen._create_build_job(build_config)

        params = job["compile-linux"]
        assert params["name"] == "build-x64-tsan"
        assert params["preset"] == "enterprise-pr-tsan"

    def test_create_frontend_build_job(self):
        """Test creating frontend build job."""
        gen = self.create_generator()
        build_config = BuildConfig(architecture=Architecture.X64)

        job = gen._create_frontend_build_job(build_config)

        assert "build-frontend" in job
        assert job["build-frontend"]["name"] == "build-x64-frontend"

    def test_create_frontend_build_job_with_sanitizer(self):
        """Test creating frontend build job with sanitizer."""
        gen = self.create_generator()
        build_config = BuildConfig(
            architecture=Architecture.X64, sanitizer=Sanitizer.ALUBSAN
        )

        job = gen._create_frontend_build_job(build_config)

        assert job["build-frontend"]["name"] == "build-x64-alubsan-frontend"


class TestDockerImageJob:
    """Test docker image job creation."""

    def create_generator(self, env_vars=None, test_date=None):
        """Helper to create generator with test environment."""
        config = GeneratorConfig(
            filter_criteria=FilterCriteria(),
            circleci=CircleCIConfig(create_docker_images=True, test_image="default"),
        )
        env_getter = lambda k, default: (
            env_vars.get(k, default) if env_vars else default
        )
        date_provider = lambda: test_date if test_date else date.today()
        return CircleCIGenerator(
            config, base_config={}, env_getter=env_getter, date_provider=date_provider
        )

    def test_docker_image_tag_generation(self):
        """Test docker image tag generation."""
        env_vars = {
            "CIRCLE_BRANCH": "feature/test-branch",
            "CIRCLE_SHA1": "abc1234567890",
        }
        test_date = date(2025, 1, 15)
        gen = self.create_generator(env_vars, test_date)
        build_config = BuildConfig(architecture=Architecture.X64)

        workflow = {"jobs": []}
        gen._add_docker_image_job(workflow, build_config, ["build-job"])

        job = workflow["jobs"][0]["create-docker-image"]
        expected_tag = "public.ecr.aws/b0b8h2r4/enterprise-preview:2025-01-15-test-branch-abc1234-amd64"
        assert job["tag"] == expected_tag

    def test_docker_image_branch_cleanup(self):
        """Test branch name cleanup (removes prefix)."""
        env_vars = {
            "CIRCLE_BRANCH": "pr/123/my-feature",
            "CIRCLE_SHA1": "deadbeef",
        }
        test_date = date(2025, 1, 15)
        gen = self.create_generator(env_vars, test_date)
        build_config = BuildConfig(architecture=Architecture.X64)

        workflow = {"jobs": []}
        gen._add_docker_image_job(workflow, build_config, ["build-job"])

        job = workflow["jobs"][0]["create-docker-image"]
        # Should extract "my-feature" from "pr/123/my-feature"
        assert "my-feature" in job["tag"]
        assert "pr/123/" not in job["tag"]

    def test_docker_image_aarch64_architecture(self):
        """Test ARM64 architecture in docker tag."""
        env_vars = {
            "CIRCLE_BRANCH": "main",
            "CIRCLE_SHA1": "abc1234",
        }
        gen = self.create_generator(env_vars, date(2025, 1, 15))
        build_config = BuildConfig(architecture=Architecture.AARCH64)

        workflow = {"jobs": []}
        gen._add_docker_image_job(workflow, build_config, ["build-job"])

        job = workflow["jobs"][0]["create-docker-image"]
        assert job["arch"] == "arm64"
        assert "arm64" in job["tag"]


class TestGenerateMethod:
    """Test the main generate() method."""

    def test_generate_requires_base_config(self):
        """Test that generate() requires either base_config or base_config_path."""
        config = GeneratorConfig(filter_criteria=FilterCriteria())
        gen = CircleCIGenerator(config)  # Neither provided

        test_def = TestDefinitionFile(
            jobs={
                "test_job": TestJob(
                    name="test_job",
                    suites=[SuiteConfig(name="test")],
                    options=TestOptions(),
                )
            }
        )

        with pytest.raises(ValueError, match="Either base_config or base_config_path"):
            gen.generate([test_def])

    def test_generate_uses_provided_base_config(self):
        """Test that generate() uses provided base config dict."""
        config = GeneratorConfig(filter_criteria=FilterCriteria())
        base_config = {"version": 2.1, "workflows": {}}
        gen = CircleCIGenerator(config, base_config=base_config)

        test_def = TestDefinitionFile(
            jobs={
                "test_job": TestJob(
                    name="test_job",
                    suites=[SuiteConfig(name="test")],
                    options=TestOptions(),
                )
            }
        )

        result = gen.generate([test_def])

        assert result["version"] == 2.1
        assert "workflows" in result

    def test_generate_creates_workflows(self):
        """Test that generate() creates workflows."""
        config = GeneratorConfig(filter_criteria=FilterCriteria())
        base_config = {"version": 2.1}
        gen = CircleCIGenerator(config, base_config=base_config)

        test_def = TestDefinitionFile(
            jobs={
                "test_job": TestJob(
                    name="test_job",
                    suites=[SuiteConfig(name="test")],
                    options=TestOptions(),
                )
            }
        )

        result = gen.generate([test_def])

        assert "workflows" in result
        # Should have x64 workflow
        assert any("x64" in name for name in result["workflows"].keys())


class TestCreateTestJobsForDeployment:
    """Test _create_test_jobs_for_deployment method."""

    def create_generator(self, replication_two=False, **filter_kwargs):
        """Helper to create generator with test config."""
        filter_criteria = FilterCriteria(**filter_kwargs)
        test_exec = TestExecutionConfig(replication_two=replication_two)
        config = GeneratorConfig(
            filter_criteria=filter_criteria,
            test_execution=test_exec,
        )
        return CircleCIGenerator(config, base_config={})

    def test_cluster_deployment_creates_one_job(self):
        """Test that CLUSTER deployment creates one job."""
        gen = self.create_generator()
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(deployment_type=DeploymentType.CLUSTER),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_jobs_for_deployment(job, build_config, ["build-job"])

        assert len(result) == 1
        assert "run-linux-tests" in result[0]

    def test_cluster_deployment_with_repl2_creates_two_jobs(self):
        """Test that CLUSTER deployment with repl2 creates two jobs."""
        gen = self.create_generator(replication_two=True)
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(deployment_type=DeploymentType.CLUSTER),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_jobs_for_deployment(job, build_config, ["build-job"])

        assert len(result) == 2
        # First job is standard cluster
        assert "test-cluster-test_job-x64" in result[0]["run-linux-tests"]["name"]
        # Second job is cluster with repl2
        assert "test-cluster-repl2-test_job-x64" in result[1]["run-linux-tests"]["name"]

    def test_single_deployment_creates_one_job(self):
        """Test that SINGLE deployment creates one job."""
        gen = self.create_generator()
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(deployment_type=DeploymentType.SINGLE),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_jobs_for_deployment(job, build_config, ["build-job"])

        assert len(result) == 1
        assert "test-single-test_job-x64" in result[0]["run-linux-tests"]["name"]

    def test_mixed_deployment_creates_one_job(self):
        """Test that MIXED deployment creates one job (requires multi-suite)."""
        gen = self.create_generator()
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(name="suite1"),
                SuiteConfig(name="suite2"),
            ],
            options=TestOptions(deployment_type=DeploymentType.MIXED),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_jobs_for_deployment(job, build_config, ["build-job"])

        assert len(result) == 1
        assert "test-mixed-test_job-x64" in result[0]["run-linux-tests"]["name"]

    def test_no_deployment_creates_both_cluster_and_single(self):
        """Test that None deployment creates both cluster and single jobs."""
        gen = self.create_generator()
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(deployment_type=None),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_jobs_for_deployment(job, build_config, ["build-job"])

        assert len(result) == 2
        job_names = [list(j.values())[0]["name"] for j in result]
        assert any("cluster" in name for name in job_names)
        assert any("single" in name for name in job_names)

    def test_no_deployment_with_repl2_creates_three_jobs(self):
        """Test that None deployment with repl2 creates cluster, cluster-repl2, and single."""
        gen = self.create_generator(replication_two=True)
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(deployment_type=None),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_jobs_for_deployment(job, build_config, ["build-job"])

        assert len(result) == 3
        job_names = [list(j.values())[0]["name"] for j in result]
        assert any("test-cluster-test_job-x64" == name for name in job_names)
        assert any("test-cluster-repl2-test_job-x64" == name for name in job_names)
        assert any("test-single-test_job-x64" == name for name in job_names)

    def test_filtered_suites_returns_none(self):
        """Test that job with all suites filtered out returns None."""
        gen = self.create_generator(full=False)  # Only PR tests
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(name="suite1", requires=TestRequirements(full=True))
            ],  # Full only
            options=TestOptions(deployment_type=DeploymentType.SINGLE),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_jobs_for_deployment(job, build_config, ["build-job"])

        # Should return empty list since job returned None
        assert len(result) == 0

    def test_partial_filtering_includes_only_valid_jobs(self):
        """Test that partial filtering includes only jobs with remaining suites."""
        gen = self.create_generator(full=False)  # Only PR tests
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(name="pr_suite", requires=TestRequirements(full=False)),
                SuiteConfig(name="full_suite", requires=TestRequirements(full=True)),
            ],
            options=TestOptions(deployment_type=None),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_jobs_for_deployment(job, build_config, ["build-job"])

        # Should create jobs (cluster and single) with only pr_suite
        assert len(result) == 2
        for job_def in result:
            job_data = list(job_def.values())[0]
            assert job_data["suites"] == "pr_suite"


class TestCreateTestJob:
    """Test _create_test_job method in detail."""

    def create_generator(self, **kwargs):
        """Helper to create generator with test config."""
        filter_criteria = FilterCriteria(**kwargs.get("filter", {}))
        test_exec = TestExecutionConfig(
            extra_args=kwargs.get("extra_args", []),
            arangosh_args=kwargs.get("arangosh_args", []),
        )
        config = GeneratorConfig(
            filter_criteria=filter_criteria,
            test_execution=test_exec,
        )
        return CircleCIGenerator(config, base_config={})

    def test_create_test_job_basic_structure(self):
        """Test basic structure of created test job."""
        gen = self.create_generator()
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config, ["build-job"]
        )

        assert result is not None
        assert "run-linux-tests" in result
        job_data = result["run-linux-tests"]
        assert job_data["name"] == "test-single-test_job-x64"
        assert job_data["suiteName"] == "test_job"
        assert job_data["suites"] == "suite1"
        assert job_data["cluster"] is False
        assert job_data["requires"] == ["build-job"]

    def test_create_test_job_cluster(self):
        """Test cluster deployment job."""
        gen = self.create_generator()
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_job(
            job, DeploymentType.CLUSTER, build_config, ["build-job"]
        )

        job_data = result["run-linux-tests"]
        assert job_data["name"] == "test-cluster-test_job-x64"
        assert job_data["cluster"] is True

    def test_create_test_job_with_suffix(self):
        """Test job name includes suffix when present."""
        gen = self.create_generator()
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(suffix="custom"),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config, ["build-job"]
        )

        job_data = result["run-linux-tests"]
        assert job_data["name"] == "test-single-test_job-custom-x64"
        assert job_data["suiteName"] == "test_job-custom"

    def test_create_test_job_replication_version_2(self):
        """Test replication version 2 in job name and extraArgs."""
        gen = self.create_generator()
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_job(
            job,
            DeploymentType.CLUSTER,
            build_config,
            ["build-job"],
            replication_version=2,
        )

        job_data = result["run-linux-tests"]
        assert "cluster-repl2" in job_data["name"]
        assert "--replicationVersion 2" in job_data["extraArgs"]

    def test_create_test_job_with_job_extra_args(self):
        """Job-level extra arguments are passed to test execution."""
        gen = self.create_generator()

        # Use TestArguments properly instead of mocking
        from src.config_lib import TestArguments

        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(),
            arguments=TestArguments(
                extra_args={"testOption": "value", "moreArgv": "--extra-flag"},
                arangosh_args=[],
            ),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_job(
            job, DeploymentType.CLUSTER, build_config, ["build-job"]
        )

        job_data = result["run-linux-tests"]
        # Verify behavior: job-level args are present in extraArgs
        assert "testOption" in job_data["extraArgs"]
        assert "value" in job_data["extraArgs"]
        assert "--extra-flag" in job_data["extraArgs"]
        assert "replicationVersion" in job_data["extraArgs"]

    def test_create_test_job_with_global_extra_args(self):
        """Test that global extra args are appended."""
        gen = self.create_generator(extra_args=["--global-flag", "--another"])
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config, ["build-job"]
        )

        job_data = result["run-linux-tests"]
        assert "--global-flag --another" in job_data["extraArgs"]

    def test_create_test_job_nightly_skip_nightly_false(self):
        """Test that nightly builds include skipNightly false."""
        gen = self.create_generator()
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(),
        )
        build_config = BuildConfig(architecture=Architecture.X64, nightly=True)

        result = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config, ["build-job"]
        )

        job_data = result["run-linux-tests"]
        assert "--skipNightly false" in job_data["extraArgs"]

    def test_create_test_job_multi_suite_options_json(self):
        """Multi-suite jobs pass suite-specific options via optionsJson."""
        gen = self.create_generator()

        # Use TestArguments properly
        from src.config_lib import TestArguments

        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(
                    name="suite1", arguments=TestArguments(extra_args={"opt1": "val1"})
                ),
                SuiteConfig(
                    name="suite2", arguments=TestArguments(extra_args={"opt2": "val2"})
                ),
            ],
            options=TestOptions(),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config, ["build-job"]
        )

        job_data = result["run-linux-tests"]
        # Verify behavior: multi-suite jobs include optionsJson with suite configs
        assert "--optionsJson" in job_data["extraArgs"]
        # Check that both suite options are present (don't check exact JSON format)
        assert "opt1" in job_data["extraArgs"]
        assert "opt2" in job_data["extraArgs"]

    def test_create_test_job_bucket_override(self):
        """Test that bucket override is applied."""
        gen = self.create_generator()
        job = TestJob(
            name="replication_sync",  # Has bucket override to 5
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(buckets=2),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config, ["build-job"]
        )

        job_data = result["run-linux-tests"]
        assert job_data["buckets"] == 5  # Override value

    def test_create_test_job_auto_buckets_with_filtering(self):
        """Test that auto buckets adjusts when suites are filtered."""
        gen = self.create_generator(full=False)  # Only PR tests
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(name="pr_suite1", requires=TestRequirements(full=False)),
                SuiteConfig(name="pr_suite2", requires=TestRequirements(full=False)),
                SuiteConfig(name="full_suite", requires=TestRequirements(full=True)),
            ],
            options=TestOptions(buckets="auto"),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config, ["build-job"]
        )

        job_data = result["run-linux-tests"]
        # Should have 2 buckets (2 PR suites remaining after filter)
        assert job_data["buckets"] == 2

    def test_create_test_job_time_limit(self):
        """Test that time limit is included."""
        gen = self.create_generator()
        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(time_limit=7200),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config, ["build-job"]
        )

        job_data = result["run-linux-tests"]
        assert job_data["timeLimit"] == 7200

    def test_create_test_job_size_override(self):
        """Test shell_client_aql nightly single-server size override."""
        gen = self.create_generator()
        job = TestJob(
            name="shell_client_aql",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(size=ResourceSize.SMALL),
        )
        build_config = BuildConfig(architecture=Architecture.X64, nightly=True)

        result = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config, ["build-job"]
        )

        job_data = result["run-linux-tests"]
        # Should be overridden to medium-plus for nightly single-server
        assert "medium+" in job_data["size"]

    def test_create_test_job_size_no_override_cluster(self):
        """Test shell_client_aql nightly cluster does NOT get size override."""
        gen = self.create_generator()
        job = TestJob(
            name="shell_client_aql",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(size=ResourceSize.SMALL),
        )
        build_config = BuildConfig(architecture=Architecture.X64, nightly=True)

        result = gen._create_test_job(
            job, DeploymentType.CLUSTER, build_config, ["build-job"]
        )

        job_data = result["run-linux-tests"]
        # Should use default small size for cluster (no override)
        assert "small" in job_data["size"]

    def test_create_test_job_with_repository(self):
        """Test job with external repository configuration."""
        # Create generator with a default container that has a tag
        from src.output_generators.base import CircleCIConfig

        filter_criteria = FilterCriteria()
        test_exec = TestExecutionConfig()
        circleci_config = CircleCIConfig(test_image="test-image:latest")
        config = GeneratorConfig(
            filter_criteria=filter_criteria,
            test_execution=test_exec,
            circleci=circleci_config,
        )
        gen = CircleCIGenerator(config, base_config={})

        from src.config_lib import RepositoryConfig

        job = TestJob(
            name="test_job",
            suites=[SuiteConfig(name="suite1")],
            options=TestOptions(),
            repository=RepositoryConfig(
                git_repo="https://github.com/example/repo.git",
                git_branch="feature-branch",
                container_suffix="-js",
                init_command="npm install",
            ),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config, ["build-job"]
        )

        job_data = result["run-linux-tests"]
        assert job_data["driver-git-repo"] == "https://github.com/example/repo.git"
        assert job_data["driver-git-branch"] == "feature-branch"
        assert job_data["init_command"] == "npm install"
        assert (
            job_data["docker_image"] == "test-image-js:latest"
        )  # Suffix applied before tag

    def test_create_test_job_returns_none_when_all_filtered(self):
        """Test that method returns None when all suites filtered out."""
        gen = self.create_generator(full=False)  # Only PR tests
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(name="full_only", requires=TestRequirements(full=True))
            ],
            options=TestOptions(),
        )
        build_config = BuildConfig(architecture=Architecture.X64)

        result = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config, ["build-job"]
        )

        assert result is None

    def test_create_test_job_filters_by_workflow_architecture(self):
        """Test that suites are filtered based on workflow's architecture."""
        gen = self.create_generator()
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(
                    name="x64_only",
                    requires=TestRequirements(architecture=Architecture.X64),
                ),
                SuiteConfig(
                    name="aarch64_only",
                    requires=TestRequirements(architecture=Architecture.AARCH64),
                ),
                SuiteConfig(name="all_archs"),  # No architecture specified
            ],
            options=TestOptions(),
        )

        # Test X64 workflow - should include x64_only and all_archs, exclude aarch64_only
        build_config_x64 = BuildConfig(architecture=Architecture.X64)
        result_x64 = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config_x64, ["build-job"]
        )

        job_data_x64 = result_x64["run-linux-tests"]
        assert job_data_x64["suites"] == "x64_only,all_archs"
        assert "aarch64_only" not in job_data_x64["suites"]

        # Test AARCH64 workflow - should include aarch64_only and all_archs, exclude x64_only
        build_config_aarch64 = BuildConfig(architecture=Architecture.AARCH64)
        result_aarch64 = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config_aarch64, ["build-job"]
        )

        job_data_aarch64 = result_aarch64["run-linux-tests"]
        assert job_data_aarch64["suites"] == "aarch64_only,all_archs"
        assert "x64_only" not in job_data_aarch64["suites"]

    def test_create_test_job_architecture_filter_returns_none(self):
        """Test that job returns None when all suites filtered by architecture."""
        gen = self.create_generator()
        job = TestJob(
            name="test_job",
            suites=[
                SuiteConfig(
                    name="x64_only",
                    requires=TestRequirements(architecture=Architecture.X64),
                ),
            ],
            options=TestOptions(),
        )

        # Running on AARCH64 should filter out the x64_only suite
        build_config = BuildConfig(architecture=Architecture.AARCH64)
        result = gen._create_test_job(
            job, DeploymentType.SINGLE, build_config, ["build-job"]
        )

        assert result is None


class TestJobLevelArchitectureFiltering:
    """Test job-level architecture filtering in _add_test_jobs."""

    def create_generator(self, **filter_kwargs):
        """Helper to create generator with test config."""
        filter_criteria = FilterCriteria(**filter_kwargs)
        config = GeneratorConfig(filter_criteria=filter_criteria)
        return CircleCIGenerator(config, base_config={})

    def test_job_with_x64_architecture_excluded_from_aarch64_workflow(self):
        """Test that job with arch: x64 is excluded from aarch64 workflow."""
        gen = self.create_generator()

        # Create a job with x64 architecture constraint at job level
        job_x64_only = TestJob(
            name="ui_tests",
            suites=[SuiteConfig(name="UserPageTestSuite")],
            requires=TestRequirements(architecture=Architecture.X64),
        )

        # Create a job without architecture constraint
        job_all_archs = TestJob(
            name="regular_tests",
            suites=[SuiteConfig(name="RegularTestSuite")],
            options=TestOptions(),
        )

        jobs = [job_x64_only, job_all_archs]
        build_config_aarch64 = BuildConfig(architecture=Architecture.AARCH64)
        workflow = {"jobs": []}

        # Add test jobs to aarch64 workflow
        gen._add_test_jobs(workflow, jobs, build_config_aarch64, ["build-job"])

        # Extract job names from workflow
        job_names = []
        for job_def in workflow["jobs"]:
            for job_type, params in job_def.items():
                job_names.append(params["name"])

        # x64-only job should NOT appear in aarch64 workflow
        assert not any(
            "ui_tests" in name for name in job_names
        ), f"x64-only job should not appear in aarch64 workflow, but found: {job_names}"

        # Regular job should appear in aarch64 workflow
        assert any(
            "regular_tests" in name for name in job_names
        ), f"Regular job should appear in aarch64 workflow, but not found in: {job_names}"

    def test_job_with_x64_architecture_included_in_x64_workflow(self):
        """Test that job with arch: x64 is included in x64 workflow."""
        gen = self.create_generator()

        # Create a job with x64 architecture constraint at job level
        job_x64_only = TestJob(
            name="ui_tests",
            suites=[SuiteConfig(name="UserPageTestSuite")],
            requires=TestRequirements(architecture=Architecture.X64),
        )

        jobs = [job_x64_only]
        build_config_x64 = BuildConfig(architecture=Architecture.X64)
        workflow = {"jobs": []}

        # Add test jobs to x64 workflow
        gen._add_test_jobs(workflow, jobs, build_config_x64, ["build-job"])

        # Extract job names from workflow
        job_names = []
        for job_def in workflow["jobs"]:
            for job_type, params in job_def.items():
                job_names.append(params["name"])

        # x64-only job SHOULD appear in x64 workflow
        assert any(
            "ui_tests" in name for name in job_names
        ), f"x64 job should appear in x64 workflow, but not found in: {job_names}"

    def test_job_with_aarch64_architecture_excluded_from_x64_workflow(self):
        """Test that job with arch: aarch64 is excluded from x64 workflow."""
        gen = self.create_generator()

        # Create a job with aarch64 architecture constraint
        job_aarch64_only = TestJob(
            name="arm_specific_tests",
            suites=[SuiteConfig(name="ArmTestSuite")],
            requires=TestRequirements(architecture=Architecture.AARCH64),
        )

        jobs = [job_aarch64_only]
        build_config_x64 = BuildConfig(architecture=Architecture.X64)
        workflow = {"jobs": []}

        # Add test jobs to x64 workflow
        gen._add_test_jobs(workflow, jobs, build_config_x64, ["build-job"])

        # aarch64-only job should NOT appear in x64 workflow
        assert (
            len(workflow["jobs"]) == 0
        ), f"aarch64-only job should not appear in x64 workflow, but found {len(workflow['jobs'])} jobs"

    def test_job_without_architecture_included_in_both_workflows(self):
        """Test that job without architecture constraint appears in both workflows."""
        gen = self.create_generator()

        # Create a job without architecture constraint
        job_all_archs = TestJob(
            name="regular_tests",
            suites=[SuiteConfig(name="RegularTestSuite")],
            options=TestOptions(),
        )

        jobs = [job_all_archs]

        # Test X64 workflow
        build_config_x64 = BuildConfig(architecture=Architecture.X64)
        workflow_x64 = {"jobs": []}
        gen._add_test_jobs(workflow_x64, jobs, build_config_x64, ["build-job"])
        assert len(workflow_x64["jobs"]) > 0, "Job should appear in x64 workflow"

        # Test AARCH64 workflow
        build_config_aarch64 = BuildConfig(architecture=Architecture.AARCH64)
        workflow_aarch64 = {"jobs": []}
        gen._add_test_jobs(workflow_aarch64, jobs, build_config_aarch64, ["build-job"])
        assert (
            len(workflow_aarch64["jobs"]) > 0
        ), "Job should appear in aarch64 workflow"
