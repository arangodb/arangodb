"""
CircleCI configuration generator.

Generates CircleCI workflow YAML from test definitions and build configuration.
"""

import json
import os
from typing import List, Dict, Any, Optional, Callable
from datetime import date
import re
import yaml

from ..config_lib import (
    TestJob,
    TestDefinitionFile,
    BuildConfig,
    DeploymentType,
    ResourceSize,
    Architecture,
    SuiteConfig,
)
from ..filters import filter_suites
from .base import OutputGenerator, GeneratorConfig
from .sizing import ResourceSizer


class CircleCIGenerator(OutputGenerator):
    """Generate CircleCI workflow configuration from test definitions."""

    # ========================================================================
    # Special Case Overrides
    #
    # These jobs require CircleCI-specific overrides that differ from their
    # YAML definitions or need runtime-dependent adjustments.
    # ========================================================================

    # Job-specific bucket overrides
    # replication_sync: YAML specifies 2 buckets (for Jenkins compatibility),
    # but CircleCI needs 5 for better parallelization. See tests/test-definitions.yml:74
    BUCKET_OVERRIDES = {
        "replication_sync": 5,
    }

    # Job-specific size overrides (applied conditionally)
    # shell_client_aql: Nightly single-server runs need more memory due to
    # extended test coverage and data volume
    def _get_size_override(
        self, job_name: str, build_config: BuildConfig, is_cluster: bool
    ) -> Optional[ResourceSize]:
        """Get size override for specific jobs based on runtime conditions."""
        if job_name == "shell_client_aql" and build_config.nightly and not is_cluster:
            return ResourceSize.MEDIUM_PLUS
        return None

    # ========================================================================

    def __init__(
        self,
        config: GeneratorConfig,
        base_config_path: Optional[str] = None,
        base_config: Optional[Dict[str, Any]] = None,
        env_getter: Optional[Callable[[str, str], str]] = None,
        date_provider: Optional[Callable[[], date]] = None,
    ):
        """
        Initialize CircleCI generator.

        Args:
            config: Generator configuration
            base_config_path: Path to base CircleCI config YAML (optional if base_config provided)
            base_config: Pre-loaded base config dict (for testing)
            env_getter: Function to get environment variables (defaults to os.environ.get)
            date_provider: Function to get current date (defaults to date.today)
        """
        super().__init__(config)
        self.base_config_path = base_config_path
        self._base_config = base_config
        self.env_getter = env_getter or os.environ.get
        self.date_provider = date_provider or date.today
        self.sizer = ResourceSizer()

    def generate(self, test_defs: List[TestDefinitionFile], **kwargs) -> Dict[str, Any]:
        """
        Generate CircleCI configuration.

        Args:
            test_defs: List of test definition files
            **kwargs: Additional arguments (unused)

        Returns:
            Complete CircleCI configuration dict
        """
        # Load or use base configuration
        if self._base_config is not None:
            circleci_config = self._base_config.copy()
        elif self.base_config_path:
            with open(self.base_config_path, "r", encoding="utf-8") as f:
                circleci_config = yaml.safe_load(f)
        else:
            raise ValueError("Either base_config or base_config_path must be provided")

        # Collect all jobs from all test definition files (no architecture filtering yet)
        all_jobs: List[TestJob] = []
        for test_def in test_defs:
            all_jobs.extend(test_def.jobs.values())

        # Generate workflows for different build configurations
        if "workflows" not in circleci_config:
            circleci_config["workflows"] = {}

        # Generate workflows for each build variant and architecture
        build_variants = self.config.build_variants
        architectures = [Architecture.X64, Architecture.AARCH64]

        for build_variant in build_variants:
            for architecture in architectures:
                build_config = BuildConfig(
                    architecture=architecture,
                    build_variant=build_variant,
                    nightly=self.config.filter_criteria.is_full_run,
                )
                self._add_workflow(circleci_config["workflows"], all_jobs, build_config)

        return circleci_config

    def write_output(self, output: Dict[str, Any], destination: str) -> None:
        """
        Write CircleCI configuration to file.

        Args:
            output: Generated config dict
            destination: Output file path
        """
        with open(destination, "w", encoding="utf-8") as f:
            yaml.dump(output, f)

    def _add_workflow(
        self, workflows: Dict[str, Any], jobs: List[TestJob], build_config: BuildConfig
    ) -> None:
        """
        Add a workflow for a specific build configuration.

        Args:
            workflows: Workflows dict to add to
            jobs: List of test jobs
            build_config: Build configuration
        """
        workflow_name = self._generate_workflow_name(build_config)
        workflow: Dict[str, Any] = {"jobs": []}
        workflows[workflow_name] = workflow

        # Add build jobs
        build_jobs = self._add_build_jobs(workflow, build_config)

        # Add optional jobs
        self._add_optional_jobs(workflow, build_config, build_jobs)

        # Add test jobs
        self._add_test_jobs(workflow, jobs, build_config, build_jobs)

    def _generate_workflow_name(self, build_config: BuildConfig) -> str:
        """Generate workflow name from build configuration."""
        suffix = "nightly" if build_config.nightly else "pr"

        suffix += build_config.build_variant.get_suffix()

        if self.config.test_execution.replication_two:
            suffix += "-repl2"

        return f"{build_config.architecture.value}-{suffix}"

    def _add_build_jobs(
        self, workflow: Dict[str, Any], build_config: BuildConfig
    ) -> List[str]:
        """
        Add compilation and frontend build jobs.

        Returns:
            List of build job names that tests depend on
        """
        build_job = self._create_build_job(build_config)
        frontend_job = self._create_frontend_build_job(build_config)

        workflow["jobs"].append(build_job)
        workflow["jobs"].append(frontend_job)

        # Add non-maintainer build for x64 (non-instrumented builds only)
        if (
            build_config.architecture == Architecture.X64
            and not build_config.build_variant.is_instrumented
        ):
            non_maintainer_job = self._create_non_maintainer_build_job(build_config)
            workflow["jobs"].append(non_maintainer_job)

        # Extract job names from the dicts
        build_job_name = build_job["compile-linux"]["name"]
        frontend_job_name = frontend_job["build-frontend"]["name"]

        return [build_job_name, frontend_job_name]

    def _create_build_job(self, build_config: BuildConfig) -> Dict[str, Any]:
        """Create compilation job definition."""
        preset = "enterprise-pr"

        preset += build_config.build_variant.get_suffix()

        suffix = build_config.build_variant.get_suffix()
        name = f"build-{build_config.architecture.value}{suffix}"

        params = {
            "context": ["sccache-aws-bucket"],
            "resource-class": self.sizer.get_resource_class(
                ResourceSize.XXLARGE, build_config.architecture
            ),
            "name": name,
            "preset": preset,
            "enterprise": True,
            "arch": build_config.architecture.value,
        }

        if build_config.architecture == Architecture.AARCH64:
            params["s3-prefix"] = "aarch64"

        return {"compile-linux": params}

    def _create_frontend_build_job(self, build_config: BuildConfig) -> Dict[str, Any]:
        """Create frontend build job definition."""
        suffix = build_config.build_variant.get_suffix()
        name = f"build-{build_config.architecture.value}{suffix}-frontend"

        return {"build-frontend": {"name": name}}

    def _create_non_maintainer_build_job(
        self, build_config: BuildConfig
    ) -> Dict[str, Any]:
        """Create non-maintainer build job for faster CI feedback."""
        return {
            "compile-linux": {
                "context": ["sccache-aws-bucket"],
                "resource-class": self.sizer.get_resource_class(
                    ResourceSize.XXLARGE, build_config.architecture
                ),
                "name": "build-non-maintainer-x64",
                "preset": "enterprise-pr-non-maintainer",
                "enterprise": True,
                "arch": "x64",
                "publish-artifacts": False,
                "build-tests": False,
            }
        }

    def _add_optional_jobs(
        self, workflow: Dict[str, Any], build_config: BuildConfig, build_jobs: List[str]
    ) -> None:
        """Add optional jobs (cppcheck, docker)."""
        # Cppcheck for x64 (non-instrumented builds only)
        if (
            build_config.architecture == Architecture.X64
            and not build_config.build_variant.is_instrumented
        ):
            workflow["jobs"].append(
                {"run-cppcheck": {"name": "cppcheck", "requires": [build_jobs[0]]}}
            )

        # Docker image creation
        if self.config.circleci.create_docker_images:
            self._add_docker_image_job(workflow, build_config, build_jobs)

    def _add_docker_image_job(
        self, workflow: Dict[str, Any], build_config: BuildConfig, build_jobs: List[str]
    ) -> None:
        """Add docker image creation job."""
        arch = "amd64" if build_config.architecture == Architecture.X64 else "arm64"
        image = "public.ecr.aws/b0b8h2r4/enterprise-preview"

        branch = self.env_getter("CIRCLE_BRANCH", "unknown-branch")
        match = re.fullmatch(r"(.+\/)?(.+)", branch)
        if match:
            branch = match.group(2)

        sha1 = self.env_getter("CIRCLE_SHA1", "unknown-sha1")[:7]
        tag = f"{self.date_provider()}-{branch}-{sha1}-{arch}"

        workflow["jobs"].append(
            {
                "create-docker-image": {
                    "name": f"create-{build_config.architecture.value}-docker-image",
                    "resource-class": self.sizer.get_resource_class(
                        ResourceSize.LARGE, build_config.architecture
                    ),
                    "arch": arch,
                    "tag": f"{image}:{tag}",
                    "requires": build_jobs,
                }
            }
        )

    def _add_test_jobs(
        self,
        workflow: Dict[str, Any],
        jobs: List[TestJob],
        build_config: BuildConfig,
        build_jobs: List[str],
    ) -> None:
        """Add all test jobs to workflow."""
        # Filter jobs based on workflow's architecture and build variant
        from dataclasses import replace
        from ..filters import should_include_job

        criteria = replace(
            self.config.filter_criteria,
            architecture=build_config.architecture,
            build_variant=build_config.build_variant,
        )

        for job in jobs:
            # Skip jobs that don't match this workflow's architecture
            if not should_include_job(job, criteria):
                continue

            test_jobs = self._create_test_jobs_for_deployment(
                job, build_config, build_jobs
            )
            workflow["jobs"].extend(test_jobs)

    def _create_test_jobs_for_deployment(
        self, job: TestJob, build_config: BuildConfig, build_jobs: List[str]
    ) -> List[Dict[str, Any]]:
        """
        Create test job(s) based on deployment type.

        A single TestJob may result in multiple CircleCI jobs if:
        - Deployment type is None (both single and cluster)
        - Replication version 2 is enabled (additional cluster job)
        - RTA UI tests (one job per deployment: SG, CL)

        Returns:
            List of job definitions (excludes jobs with no suites after filtering)
        """
        # Handle RTA UI tests specially - they generate multiple jobs
        if job.job_type == "run-rta-tests":
            return self._create_rta_test_jobs(job, build_config, build_jobs)

        result = []
        deployment_type = job.options.deployment_type
        add_job_to_result = lambda job_def: result.append(job_def) if job_def else None

        if deployment_type is None or deployment_type == DeploymentType.CLUSTER:
            add_job_to_result(
                self._create_test_job(
                    job, DeploymentType.CLUSTER, build_config, build_jobs
                )
            )

            if self.config.test_execution.replication_two:
                add_job_to_result(
                    self._create_test_job(
                        job,
                        DeploymentType.CLUSTER,
                        build_config,
                        build_jobs,
                        replication_version=2,
                    )
                )

        if deployment_type is None or deployment_type == DeploymentType.SINGLE:
            add_job_to_result(
                self._create_test_job(
                    job, DeploymentType.SINGLE, build_config, build_jobs
                )
            )

        if deployment_type == DeploymentType.MIXED:
            add_job_to_result(
                self._create_test_job(
                    job, DeploymentType.MIXED, build_config, build_jobs
                )
            )

        return result

    def _generate_test_job_names(
        self,
        job: TestJob,
        deployment_type: DeploymentType,
        build_config: BuildConfig,
        replication_version: int,
    ) -> tuple[str, str]:
        """Generate TEST job and suite names (applying suffix if present)."""
        deployment_str = self._get_deployment_string(
            deployment_type, replication_version
        )

        suite_name = job.name
        if job.options.suffix:
            suite_name = f"{job.name}-{job.options.suffix}"

        # Build job name with architecture and sanitizer suffix for global uniqueness
        sanitizer_suffix = build_config.build_variant.get_suffix()
        job_name = f"test-{deployment_str}-{suite_name}-{build_config.architecture.value}{sanitizer_suffix}"

        return job_name, suite_name

    @staticmethod
    def _dict_to_args_string(args_dict: Dict[str, Any]) -> str:
        """Convert args dict directly to space-separated command-line string."""
        parts = []
        for key, value in args_dict.items():
            if key == "moreArgv":
                # Special case: moreArgv value is appended directly
                parts.append(str(value))
            else:
                parts.append(f"--{key}")
                # Always append value (boolean values become "true"/"false" strings)
                if isinstance(value, bool):
                    parts.append("true" if value else "false")
                else:
                    parts.append(str(value))
        return " ".join(parts)

    @staticmethod
    def _dict_to_options_json(args_dict: Dict[str, Any]) -> Dict[str, Any]:
        """
        Convert args dict to optionsJson format.

        Handles colon-separated keys by creating nested structures:
        - "extraArgs:key" -> {"extraArgs": {"key": value}}
        - "regular" -> {"regular": value}
        """
        result: Dict[str, Any] = {}
        for key, value in args_dict.items():
            if key == "moreArgv":
                # moreArgv is not included in optionsJson
                continue

            # Handle colon-separated keys (create nested structure)
            if ":" in key:
                key_parts = key.split(":", 1)  # Split only on first colon
                parent_key = key_parts[0]
                child_key = key_parts[1]

                # Create nested dict if parent doesn't exist
                if parent_key not in result:
                    result[parent_key] = {}

                result[parent_key][child_key] = value
            else:
                result[key] = value

        return result

    def _build_options_json(self, suites: List[SuiteConfig]) -> List[Dict[str, Any]]:
        """Convert suite arguments to optionsJson format."""
        return [
            (
                self._dict_to_options_json(suite.arguments.extra_args)
                if suite.arguments and suite.arguments.extra_args
                else {}
            )
            for suite in suites
        ]

    def _build_extra_args_string(
        self,
        job: TestJob,
        filtered_suites: List[SuiteConfig],
        build_config: BuildConfig,
        is_cluster: bool,
        replication_version: int,
    ) -> str:
        """
        Build extra args string in correct order: job args, optionsJson, replicationVersion, skipNightly.

        Order matters for compatibility with old generator.
        """
        parts = []

        # Add job-level args
        if job.arguments.extra_args:
            parts.append(self._dict_to_args_string(job.arguments.extra_args))

        # Add optionsJson for multi-suite jobs
        if len(filtered_suites) > 1:
            options_json = self._build_options_json(filtered_suites)
            parts.append(
                f"--optionsJson {json.dumps(options_json, separators=(',', ':'))}"
            )

        # Add replicationVersion for cluster tests
        if is_cluster:
            parts.append(f"--replicationVersion {replication_version}")

        # Add skipNightly for nightly builds
        if build_config.nightly:
            parts.append("--skipNightly false")

        return " ".join(parts)

    def _apply_job_overrides(
        self, job: TestJob, job_dict: Dict[str, Any], filtered_suites: List[SuiteConfig]
    ) -> None:
        """Apply CircleCI bucket/time overrides (modifies job_dict in place)."""
        bucket_count = self._get_bucket_count(job)

        if job.options.buckets == "auto" and len(filtered_suites) != len(job.suites):
            bucket_count = len(filtered_suites)

        if job.name in self.BUCKET_OVERRIDES:
            bucket_count = self.BUCKET_OVERRIDES[job.name]

        if bucket_count and bucket_count != 1:
            job_dict["buckets"] = bucket_count

        if job.options.time_limit:
            job_dict["timeLimit"] = job.options.time_limit

    def _create_test_job(
        self,
        job: TestJob,
        deployment_type: DeploymentType,
        build_config: BuildConfig,
        build_jobs: List[str],
        replication_version: int = 1,
    ) -> Optional[Dict[str, Any]]:
        """
        Create CircleCI test job definition.

        Returns:
            Job definition dict, or None if all suites were filtered out.
        """
        is_cluster = deployment_type == DeploymentType.CLUSTER

        job_name, suite_name = self._generate_test_job_names(
            job, deployment_type, build_config, replication_version
        )

        # Create filter criteria with current workflow's architecture and build variant
        from dataclasses import replace

        criteria = replace(
            self.config.filter_criteria,
            architecture=build_config.architecture,
            build_variant=build_config.build_variant,
        )

        filtered_suites = filter_suites(job, criteria)

        # Skip job creation if no suites remain after filtering
        if not filtered_suites:
            return None

        suite_str = ",".join(suite.name for suite in filtered_suites)

        size_override = self._get_size_override(job.name, build_config, is_cluster)
        size = size_override or job.options.size or ResourceSize.SMALL
        resource_class = self.sizer.get_test_size(size, build_config, is_cluster)

        job_dict = {
            "name": job_name,
            "suiteName": suite_name,
            "suites": suite_str,
            "size": resource_class,
            "cluster": is_cluster,
            "requires": build_jobs,
            "sanitizer": self._get_sanitizer_param(build_config),
            "arangosh_args": "A "
            + json.dumps(
                job.arguments.arangosh_args + self.config.test_execution.arangosh_args
            ),
        }

        extra_args_str = self._build_extra_args_string(
            job, filtered_suites, build_config, is_cluster, replication_version
        )

        # Combine job args with command-line extra args
        if extra_args_str or self.config.test_execution.extra_args:
            all_args = []
            if extra_args_str:
                all_args.append(extra_args_str)
            if self.config.test_execution.extra_args:
                all_args.append(" ".join(self.config.test_execution.extra_args))
            job_dict["extraArgs"] = " ".join(all_args)

        self._apply_job_overrides(job, job_dict, filtered_suites)
        self._add_repository_config(job_dict, job)

        return {job.job_type: job_dict}

    def _get_deployment_string(
        self, deployment_type: DeploymentType, replication_version: int
    ) -> str:
        """Get deployment string for job name."""
        if deployment_type == DeploymentType.CLUSTER:
            return f"cluster{'-repl2' if replication_version == 2 else ''}"
        if deployment_type == DeploymentType.SINGLE:
            return "single"
        return "mixed"

    def _get_bucket_count(self, job: TestJob) -> Optional[int]:
        """Get bucket count from job definition (before applying overrides)."""
        return job.get_bucket_count()

    def _get_sanitizer_param(self, build_config: BuildConfig) -> str:
        """Get sanitizer parameter string based on build variant."""
        if build_config.build_variant.is_tsan:
            return "tsan"
        elif build_config.build_variant.is_alubsan:
            return "alubsan"
        return ""

    def _create_rta_test_jobs(
        self,
        job: TestJob,
        build_config: BuildConfig,
        build_jobs: List[str],
    ) -> List[Dict[str, Any]]:
        """
        Create RTA UI test job definitions.

        RTA tests use run-rta-tests job with different parameters than regular tests.

        Returns:
            List of job definitions (one per deployment).
        """
        # Build filter string with trailing space (matches old generator behavior)
        ui_filter = "".join(
            f"--ui-include-test-suite {suite.name} " for suite in job.suites
        )
        # Ensure trailing space is preserved (old generator compatibility)
        if ui_filter and not ui_filter.endswith(" "):
            ui_filter += " "

        # Calculate bucket count
        bucket_count = job.get_bucket_count()
        if bucket_count is None or bucket_count == "auto":
            bucket_count = len(job.suites)

        # Get RTA branch from repository config
        rta_branch = "main"
        if job.repository and job.repository.git_branch:
            rta_branch = job.repository.git_branch

        deployments = ["single", "cluster"]
        sanitizer_suffix = build_config.build_variant.get_suffix()

        result_jobs = []
        for deployment in deployments:
            job_dict = {
                "name": f"test-{deployment}-UI-{build_config.architecture.value}{sanitizer_suffix}",
                "suiteName": f"{deployment}-UI",
                "arangosh_args": "",
                "deployment": "SG" if deployment == "single" else "CL",
                "browser": "Remote_CHROME",
                "enterprise": "EP",
                "filterStatement": ui_filter,
                "requires": build_jobs,
                "rta-branch": rta_branch,
                "buckets": bucket_count,
                "sanitizer": self._get_sanitizer_param(build_config),
            }
            result_jobs.append({"run-rta-tests": job_dict})

        return result_jobs

    def _add_repository_config(self, job_dict: Dict[str, Any], job: TestJob) -> None:
        """Add repository configuration to job dict if job has external repo."""
        if job.repository:
            # Build docker image name with container_suffix if present
            container = self.config.circleci.test_image
            if ":" in container:
                base, tag = container.rsplit(":", 1)
                # Apply container_suffix if present (e.g., test-ubuntu -> test-ubuntu-js)
                if job.repository.container_suffix:
                    # Remove trailing colon from suffix if present
                    suffix = job.repository.container_suffix.rstrip(":")
                    base = base + suffix
                job_dict["docker_image"] = f"{base}:{tag}"
            else:
                job_dict["docker_image"] = container

            job_dict["driver-git-repo"] = job.repository.git_repo
            job_dict["driver-git-branch"] = job.repository.git_branch or "main"
            # Add init_command if the field exists (even if empty string)
            if job.repository.init_command is not None:
                job_dict["init_command"] = job.repository.init_command
        else:
            job_dict["docker_image"] = self.config.circleci.test_image
