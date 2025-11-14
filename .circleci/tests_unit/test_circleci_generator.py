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
    TestDefinitionFile,
    BuildConfig,
    DeploymentType,
    ResourceSize,
    Sanitizer,
    Architecture,
)
from src.filters import FilterCriteria, PlatformFlags
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
        circleci_config = CircleCIConfig(ui=config_kwargs.get("ui", ""))
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
        assert name == "x64-no_ui_tests-pr"

    def test_workflow_name_x64_nightly(self):
        """Test workflow name for x64 nightly build."""
        gen = self.create_generator()
        build_config = BuildConfig(architecture=Architecture.X64, nightly=True)

        name = gen._generate_workflow_name(build_config)
        assert name == "x64-no_ui_tests-nightly"

    def test_workflow_name_with_sanitizer(self):
        """Test workflow name includes sanitizer."""
        gen = self.create_generator()
        build_config = BuildConfig(
            architecture=Architecture.X64, sanitizer=Sanitizer.TSAN
        )

        name = gen._generate_workflow_name(build_config)
        assert name == "x64-no_ui_tests-pr-tsan"

    def test_workflow_name_with_ui_tests(self):
        """Test workflow name with UI tests enabled."""
        gen = self.create_generator(ui="on")
        build_config = BuildConfig(architecture=Architecture.X64)

        name = gen._generate_workflow_name(build_config)
        assert name == "x64-with_ui_tests-pr"

    def test_workflow_name_with_ui_only(self):
        """Test workflow name with UI tests only."""
        gen = self.create_generator(ui="only")
        build_config = BuildConfig(architecture=Architecture.X64)

        name = gen._generate_workflow_name(build_config)
        assert name == "x64-only_ui_tests-pr"

    def test_workflow_name_aarch64(self):
        """Test workflow name for ARM architecture."""
        gen = self.create_generator()
        build_config = BuildConfig(architecture=Architecture.AARCH64)

        name = gen._generate_workflow_name(build_config)
        assert name == "aarch64-no_ui_tests-pr"

    def test_workflow_name_with_replication_two(self):
        """Test workflow name with replication version 2."""
        gen = self.create_generator(replication_two=True)
        build_config = BuildConfig(architecture=Architecture.X64)

        name = gen._generate_workflow_name(build_config)
        assert name == "x64-no_ui_tests-pr-repl2"


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
            circleci=CircleCIConfig(create_docker_images=True),
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
        config = GeneratorConfig(filter_criteria=FilterCriteria(all_tests=True))
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


class TestHotbackupJob:
    """Test hotbackup job creation."""

    def test_hotbackup_job_sizing(self):
        """Test that hotbackup job uses correct resource size."""
        config = GeneratorConfig(filter_criteria=FilterCriteria())
        gen = CircleCIGenerator(config, base_config={})
        build_config = BuildConfig(
            architecture=Architecture.X64, sanitizer=Sanitizer.TSAN
        )

        workflow = {"jobs": []}
        gen._add_hotbackup_job(workflow, build_config, ["build-job"])

        job = workflow["jobs"][0]["run-hotbackup-tests"]
        # Medium with TSAN cluster should become xlarge
        assert job["size"] == "xlarge"
        assert job["name"] == "run-hotbackup-tests-x64"
        assert job["requires"] == ["build-job"]
