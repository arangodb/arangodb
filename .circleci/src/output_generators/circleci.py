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
)
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

    # Job-specific time limit overrides
    # chaos: Nightly runs execute 32 test combinations, each taking ~5 minutes
    def _get_time_limit_override(
        self, job_name: str, build_config: BuildConfig
    ) -> Optional[int]:
        """Get time limit override (in seconds) for specific jobs."""
        if job_name == "chaos" and build_config.nightly:
            return 32 * 5 * 60  # 32 combinations × 5 min each = 9600 seconds
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

        # Collect all jobs from all test definition files
        all_jobs = []
        for test_def in test_defs:
            filtered = self.filter_jobs(test_def)
            all_jobs.extend(filtered)

        # Generate workflows for different build configurations
        if "workflows" not in circleci_config:
            circleci_config["workflows"] = {}

        # X64 Enterprise (always included)
        build_config = BuildConfig(
            architecture=Architecture.X64,
            enterprise=True,
            sanitizer=self.config.filter_criteria.sanitizer,
            nightly=self.config.filter_criteria.nightly,
        )
        self._add_workflow(circleci_config["workflows"], all_jobs, build_config)

        # ARM64 Enterprise (with or without sanitizer - changed in merge ec7f7ef91ade)
        build_config = BuildConfig(
            architecture=Architecture.AARCH64,
            enterprise=True,
            sanitizer=self.config.filter_criteria.sanitizer,
            nightly=self.config.filter_criteria.nightly,
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
        workflow = {"jobs": []}
        workflows[workflow_name] = workflow

        # Add build jobs
        build_jobs = self._add_build_jobs(workflow, build_config)

        # Add optional jobs
        self._add_optional_jobs(workflow, build_config, build_jobs)

        # Add test jobs
        if self.config.circleci.ui != "only":
            self._add_test_jobs(workflow, jobs, build_config, build_jobs)

    def _generate_workflow_name(self, build_config: BuildConfig) -> str:
        """Generate workflow name from build configuration."""
        suffix = "nightly" if build_config.nightly else "pr"

        if (
            build_config.architecture == Architecture.X64
            and self.config.circleci.ui
            not in (
                "",
                "off",
            )
        ):
            if self.config.circleci.ui == "only":
                suffix = f"only_ui_tests-{suffix}"
            else:
                suffix = f"with_ui_tests-{suffix}"
        else:
            suffix = f"no_ui_tests-{suffix}"

        if build_config.sanitizer:
            suffix += f"-{build_config.sanitizer.value}"

        if self.config.test_execution.replication_two:
            suffix += "-repl2"

        edition = "enterprise" if build_config.enterprise else "community"
        return f"{build_config.architecture.value}-{edition}-{suffix}"

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

        # Add non-maintainer build for x64 enterprise (no sanitizer, not ui-only)
        if (
            build_config.architecture == Architecture.X64
            and build_config.enterprise
            and not build_config.sanitizer
            and self.config.circleci.ui != "only"
        ):
            non_maintainer_job = self._create_non_maintainer_build_job(build_config)
            workflow["jobs"].append(non_maintainer_job)

        # Extract job names from the dicts
        build_job_name = build_job["compile-linux"]["name"]
        frontend_job_name = frontend_job["build-frontend"]["name"]

        return [build_job_name, frontend_job_name]

    def _create_build_job(self, build_config: BuildConfig) -> Dict[str, Any]:
        """Create compilation job definition."""
        edition = "ee" if build_config.enterprise else "ce"
        preset = f"{'enterprise' if build_config.enterprise else 'community'}-pr"

        if build_config.sanitizer:
            preset += f"-{build_config.sanitizer.value}"

        suffix = f"-{build_config.sanitizer.value}" if build_config.sanitizer else ""
        name = f"build-{edition}-{build_config.architecture.value}{suffix}"

        params = {
            "context": ["sccache-aws-bucket"],
            "resource-class": self.sizer.get_resource_class(
                ResourceSize.XXLARGE, build_config.architecture
            ),
            "name": name,
            "preset": preset,
            "enterprise": build_config.enterprise,
            "arch": build_config.architecture.value,
        }

        if build_config.architecture == Architecture.AARCH64:
            params["s3-prefix"] = "aarch64"

        return {"compile-linux": params}

    def _create_frontend_build_job(self, build_config: BuildConfig) -> Dict[str, Any]:
        """Create frontend build job definition."""
        edition = "ee" if build_config.enterprise else "ce"
        suffix = f"-{build_config.sanitizer.value}" if build_config.sanitizer else ""
        name = f"build-{edition}-{build_config.architecture.value}{suffix}-frontend"

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
                "name": "build-ee-non-maintainer-x64",
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
        """Add optional jobs (cppcheck, docker, hotbackup, UI tests)."""
        # Cppcheck for x64 non-UI
        if (
            build_config.architecture == Architecture.X64
            and self.config.circleci.ui
            in (
                "",
                "off",
            )
        ):
            workflow["jobs"].append(
                {"run-cppcheck": {"name": "cppcheck", "requires": [build_jobs[0]]}}
            )

        # Docker image creation
        if self.config.circleci.create_docker_images:
            self._add_docker_image_job(workflow, build_config, build_jobs)

        # UI tests
        if (
            build_config.architecture == Architecture.X64
            and self.config.circleci.ui
            not in (
                "",
                "off",
            )
        ):
            self._add_ui_test_jobs(workflow, build_config, build_jobs)

        # Hotbackup tests
        if (
            build_config.enterprise
            and not self.config.test_execution.arangod_without_v8
        ):
            self._add_hotbackup_job(workflow, build_config, build_jobs)

    def _add_docker_image_job(
        self, workflow: Dict[str, Any], build_config: BuildConfig, build_jobs: List[str]
    ) -> None:
        """Add docker image creation job."""
        edition = "ee" if build_config.enterprise else "ce"
        arch = "amd64" if build_config.architecture == Architecture.X64 else "arm64"
        image = (
            "public.ecr.aws/b0b8h2r4/enterprise-preview"
            if build_config.enterprise
            else "public.ecr.aws/b0b8h2r4/arangodb-preview"
        )

        branch = self.env_getter("CIRCLE_BRANCH", "unknown-branch")
        match = re.fullmatch(r"(.+\/)?(.+)", branch)
        if match:
            branch = match.group(2)

        sha1 = self.env_getter("CIRCLE_SHA1", "unknown-sha1")[:7]
        tag = f"{self.date_provider()}-{branch}-{sha1}-{arch}"

        workflow["jobs"].append(
            {
                "create-docker-image": {
                    "name": f"create-{edition}-{build_config.architecture.value}-docker-image",
                    "resource-class": self.sizer.get_resource_class(
                        ResourceSize.LARGE, build_config.architecture
                    ),
                    "arch": arch,
                    "tag": f"{image}:{tag}",
                    "requires": build_jobs,
                }
            }
        )

    def _add_hotbackup_job(
        self, workflow: Dict[str, Any], build_config: BuildConfig, build_jobs: List[str]
    ) -> None:
        """Add hotbackup test job."""
        workflow["jobs"].append(
            {
                "run-hotbackup-tests": {
                    "name": f"run-hotbackup-tests-{build_config.architecture.value}",
                    "size": self.sizer.get_test_size(
                        ResourceSize.MEDIUM, build_config, True
                    ),
                    "requires": build_jobs,
                }
            }
        )

    def _add_ui_test_jobs(
        self, workflow: Dict[str, Any], build_config: BuildConfig, build_jobs: List[str]
    ) -> None:
        """Add RTA UI test jobs."""
        ui_testsuites = (
            self.config.circleci.ui_testsuites.split(",")
            if self.config.circleci.ui_testsuites
            else [
                "UserPageTestSuite",
                "CollectionsTestSuite",
                "ViewsTestSuite",
                "GraphTestSuite",
                "QueryTestSuite",
                "AnalyzersTestSuite",
                "AnalyzersTestSuite2",
                "DatabaseTestSuite",
                "LogInTestSuite",
                "DashboardTestSuite",
                "SupportTestSuite",
                "ServiceTestSuite",
            ]
        )

        deployments = (
            self.config.circleci.ui_deployments.split(",")
            if self.config.circleci.ui_deployments
            else ["SG", "CL"]
        )

        # Build filter string with trailing space (matches old generator behavior)
        ui_filter = "".join(
            f"--ui-include-test-suite {suite} " for suite in ui_testsuites
        )

        for deployment in deployments:
            workflow["jobs"].append(
                {
                    "run-rta-tests": {
                        "name": f"test-{deployment}-UI",
                        "suiteName": ui_filter,
                        "arangosh_args": "",
                        "deployment": deployment,
                        "browser": "Remote_CHROME",
                        "enterprise": "EP" if build_config.enterprise else "C",
                        "filterStatement": ui_filter,
                        "requires": build_jobs,
                        "rta-branch": self.config.circleci.rta_branch,
                        "buckets": len(ui_testsuites),
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
        for job in jobs:
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

        Returns:
            List of job definitions
        """
        result = []
        deployment_type = job.options.deployment_type

        if deployment_type == DeploymentType.CLUSTER:
            result.append(
                self._create_test_job(
                    job, DeploymentType.CLUSTER, build_config, build_jobs
                )
            )
            if self.config.test_execution.replication_two:
                result.append(
                    self._create_test_job(
                        job,
                        DeploymentType.CLUSTER,
                        build_config,
                        build_jobs,
                        replication_version=2,
                    )
                )
        elif deployment_type == DeploymentType.SINGLE:
            result.append(
                self._create_test_job(
                    job, DeploymentType.SINGLE, build_config, build_jobs
                )
            )
        elif deployment_type == DeploymentType.MIXED:
            result.append(
                self._create_test_job(
                    job, DeploymentType.MIXED, build_config, build_jobs
                )
            )
        else:
            # No deployment type - run both
            result.append(
                self._create_test_job(
                    job, DeploymentType.CLUSTER, build_config, build_jobs
                )
            )
            if self.config.test_execution.replication_two:
                result.append(
                    self._create_test_job(
                        job,
                        DeploymentType.CLUSTER,
                        build_config,
                        build_jobs,
                        replication_version=2,
                    )
                )
            result.append(
                self._create_test_job(
                    job, DeploymentType.SINGLE, build_config, build_jobs
                )
            )

        return result

    def _create_test_job(
        self,
        job: TestJob,
        deployment_type: DeploymentType,
        build_config: BuildConfig,
        build_jobs: List[str],
        replication_version: int = 1,
    ) -> Dict[str, Any]:
        """
        Create a single test job definition.

        Args:
            job: TestJob to create
            deployment_type: Deployment type for this job
            build_config: Build configuration
            build_jobs: List of build job names this depends on
            replication_version: Replication version (1 or 2)

        Returns:
            Job definition dict for CircleCI
        """
        is_cluster = deployment_type == DeploymentType.CLUSTER

        # Build job name and suite name
        deployment_str = self._get_deployment_string(
            deployment_type, replication_version
        )

        # Add suffix to job/suite name if present
        suite_name = job.name
        if job.options.suffix:
            suite_name = f"{job.name}-{job.options.suffix}"

        job_name = (
            f"test-{deployment_str}-{suite_name}-{build_config.architecture.value}"
        )

        # Filter suites based on full flag (suite-level filtering)
        filtered_suites = [
            suite
            for suite in job.suites
            if not (suite.options.full and not build_config.nightly)
        ]

        # Build suite string
        suite_str = ",".join(suite.name for suite in filtered_suites)

        # Get size (with special case overrides)
        size_override = self._get_size_override(job.name, build_config, is_cluster)
        size = size_override or job.options.size or ResourceSize.SMALL
        resource_class = self.sizer.get_test_size(size, build_config, is_cluster)

        # Build job dict
        job_dict = {
            "name": job_name,
            "suiteName": suite_name,
            "suites": suite_str,
            "size": resource_class,
            "cluster": is_cluster,
            "requires": build_jobs,
            "arangosh_args": "A "
            + json.dumps(
                job.arguments.arangosh_args + self.config.test_execution.arangosh_args
            ),
        }

        # Add extra args - must match old generator's order:
        # 1. job.arguments.extra_args (base args)
        # 2. optionsJson (for multi-suite jobs)
        # 3. replicationVersion (for cluster)
        # 4. skipNightly (for nightly builds)
        # 5. CLI extra_args
        extra_args = list(job.arguments.extra_args)

        # For multi-suite jobs, add optionsJson BEFORE replicationVersion/skipNightly
        # Use filtered_suites to match what will actually run
        if len(filtered_suites) > 1:
            options_json = []
            for suite in filtered_suites:
                # Convert suite arguments to dict format for optionsJson
                # Uses same logic as old generator's get_args() function
                suite_args = {}
                if suite.arguments and suite.arguments.extra_args:
                    # Parse extra_args back into dict format
                    i = 0
                    while i < len(suite.arguments.extra_args):
                        arg = suite.arguments.extra_args[i]
                        if arg.startswith("--"):
                            key = arg[2:]  # Remove -- prefix

                            # Get the value
                            value = None
                            if i + 1 < len(suite.arguments.extra_args):
                                next_arg = suite.arguments.extra_args[i + 1]
                                if not next_arg.startswith("--"):
                                    # Convert string booleans back to bool
                                    if next_arg == "true":
                                        value = True
                                    elif next_arg == "false":
                                        value = False
                                    else:
                                        value = next_arg
                                    i += 2
                                else:
                                    # Boolean flag without explicit value
                                    value = True
                                    i += 1
                            else:
                                # Boolean flag at end
                                value = True
                                i += 1

                            # Handle colon-separated keys (nest them)
                            if ":" in key:
                                keyparts = key.split(":", 1)
                                parent_key, child_key = keyparts[0], keyparts[1]
                                if parent_key not in suite_args:
                                    suite_args[parent_key] = {}
                                suite_args[parent_key][child_key] = value
                            else:
                                suite_args[key] = value
                        else:
                            i += 1

                options_json.append(suite_args)

            # Add optionsJson as a command-line argument
            extra_args.extend(
                ["--optionsJson", json.dumps(options_json, separators=(",", ":"))]
            )

        # Add replicationVersion and skipNightly AFTER optionsJson
        if is_cluster:
            extra_args.extend(["--replicationVersion", str(replication_version)])
        if build_config.nightly:
            extra_args.extend(["--skipNightly", "false"])

        if extra_args or self.config.test_execution.extra_args:
            job_dict["extraArgs"] = " ".join(
                extra_args + self.config.test_execution.extra_args
            )

        # Add buckets (with special case overrides)
        bucket_count = self._get_bucket_count(job)
        # Override bucket count if using auto buckets and suites were filtered
        if job.options.buckets == "auto" and len(filtered_suites) != len(job.suites):
            bucket_count = len(filtered_suites)
        # Apply CircleCI-specific bucket overrides
        if job.name in self.BUCKET_OVERRIDES:
            bucket_count = self.BUCKET_OVERRIDES[job.name]
        if bucket_count and bucket_count != 1:
            job_dict["buckets"] = bucket_count

        # Apply time limit overrides
        time_limit = self._get_time_limit_override(job.name, build_config)
        if time_limit:
            job_dict["timeLimit"] = time_limit

        # Add repository info if present
        self._add_repository_config(job_dict, job)

        # Use job_type from job definition (defaults to 'run-linux-tests')
        return {job.job_type: job_dict}

    def _get_deployment_string(
        self, deployment_type: DeploymentType, replication_version: int
    ) -> str:
        """Get deployment string for job name."""
        if deployment_type == DeploymentType.CLUSTER:
            return f"cluster{'-repl2' if replication_version == 2 else ''}"
        elif deployment_type == DeploymentType.SINGLE:
            return "single"
        else:
            return "mixed"

    def _get_bucket_count(self, job: TestJob) -> Optional[int]:
        """Get bucket count from job definition (before applying overrides)."""
        return job.get_bucket_count()

    def _add_repository_config(self, job_dict: Dict[str, Any], job: TestJob) -> None:
        """Add repository configuration to job dict if job has external repo."""
        if job.repository:
            # Build docker image name with container_suffix if present
            container = self.config.circleci.default_container
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
            job_dict["docker_image"] = self.config.circleci.default_container
