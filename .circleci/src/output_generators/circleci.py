"""
CircleCI configuration generator.

Generates CircleCI workflow YAML from test definitions and build configuration.
"""

import json
import sys
import os
from typing import List, Dict, Any, Optional
from enum import Enum

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

from config_lib import (
    TestJob, TestDefinitionFile, BuildConfig, DeploymentType, ResourceSize
)
from output_generators.base import OutputGenerator, GeneratorArgs
import yaml


class CircleCIGenerator(OutputGenerator):
    """Generate CircleCI workflow configuration from test definitions."""

    def __init__(self, args: GeneratorArgs, base_config_path: str):
        """
        Initialize CircleCI generator.

        Args:
            args: Generator arguments
            base_config_path: Path to base CircleCI config YAML
        """
        super().__init__(args)
        self.base_config_path = base_config_path

    def generate(self, test_defs: List[TestDefinitionFile], **kwargs) -> Dict[str, Any]:
        """
        Generate CircleCI configuration.

        Args:
            test_defs: List of test definition files
            **kwargs: Additional arguments (unused)

        Returns:
            Complete CircleCI configuration dict
        """
        # Load base configuration
        with open(self.base_config_path, 'r', encoding='utf-8') as f:
            config = yaml.safe_load(f)

        # Collect all jobs from all test definition files
        all_jobs = []
        for test_def in test_defs:
            filtered = self.filter_jobs(test_def)
            all_jobs.extend(filtered)

        # Generate workflows for different build configurations
        if 'workflows' not in config:
            config['workflows'] = {}

        # X64 Enterprise (always included)
        build_config = BuildConfig(
            architecture="x64",
            enterprise=True,
            sanitizer=self.args.sanitizer,
            nightly=self.args.nightly
        )
        self._add_workflow(config['workflows'], all_jobs, build_config)

        # ARM64 Enterprise (only without sanitizer)
        if self.args.sanitizer == "":
            build_config = BuildConfig(
                architecture="aarch64",
                enterprise=True,
                sanitizer="",
                nightly=self.args.nightly
            )
            self._add_workflow(config['workflows'], all_jobs, build_config)

        return config

    def write_output(self, output: Dict[str, Any], destination: str) -> None:
        """
        Write CircleCI configuration to file.

        Args:
            output: Generated config dict
            destination: Output file path
        """
        with open(destination, 'w', encoding='utf-8') as f:
            yaml.dump(output, f)

    def _add_workflow(self, workflows: Dict[str, Any], jobs: List[TestJob],
                     build_config: BuildConfig) -> None:
        """
        Add a workflow for a specific build configuration.

        Args:
            workflows: Workflows dict to add to
            jobs: List of test jobs
            build_config: Build configuration
        """
        # Generate workflow name
        suffix = "nightly" if build_config.nightly else "pr"

        if build_config.architecture == "x64" and self.args.ui != "" and self.args.ui != "off":
            if self.args.ui == "only":
                suffix = "only_ui_tests-" + suffix
            else:
                suffix = "with_ui_tests-" + suffix
        else:
            suffix = "no_ui_tests-" + suffix

        if build_config.sanitizer != "":
            suffix += "-" + build_config.sanitizer

        if self.args.replication_two:
            suffix += "-repl2"

        edition = "enterprise" if build_config.enterprise else "community"
        workflow_name = f"{build_config.architecture}-{edition}-{suffix}"

        workflow = {"jobs": []}
        workflows[workflow_name] = workflow

        # Add build jobs
        build_job_name = self._add_build_job(workflow, build_config)
        frontend_job_name = self._add_frontend_build_job(workflow, build_config)
        build_jobs = [build_job_name, frontend_job_name]

        # Add cppcheck for x64 non-UI
        if build_config.architecture == "x64" and (not self.args.ui or self.args.ui == "off"):
            self._add_cppcheck_job(workflow, build_job_name)

        # Add docker image creation
        if self.args.create_docker_images:
            self._add_create_docker_image_job(workflow, build_config, build_jobs)

        # Add UI tests if requested
        if build_config.architecture == "x64" and self.args.ui != "" and self.args.ui != "off":
            self._add_rta_ui_test_jobs(workflow, build_config, build_jobs)

        if self.args.ui == "only":
            return  # Skip regular tests

        # Add hotbackup tests for enterprise
        if build_config.enterprise and self.args.arangod_without_v8 != 'true':
            self._add_hotbackup_job(workflow, build_config, build_jobs)

        # Add test jobs
        self._add_test_jobs(workflow, jobs, build_config, build_jobs)

    def _add_build_job(self, workflow: Dict[str, Any], build_config: BuildConfig) -> str:
        """Add compilation job to workflow."""
        edition = "ee" if build_config.enterprise else "ce"
        preset = "enterprise-pr" if build_config.enterprise else "community-pr"

        if build_config.sanitizer != "":
            preset += f"-{build_config.sanitizer}"

        suffix = "" if build_config.sanitizer == "" else f"-{build_config.sanitizer}"
        name = f"build-{edition}-{build_config.architecture}{suffix}"

        params = {
            "context": ["sccache-aws-bucket"],
            "resource-class": self._get_size("2xlarge", build_config.architecture),
            "name": name,
            "preset": preset,
            "enterprise": build_config.enterprise,
            "arch": build_config.architecture,
        }

        if build_config.architecture == "aarch64":
            params["s3-prefix"] = "aarch64"

        workflow["jobs"].append({"compile-linux": params})
        return name

    def _add_frontend_build_job(self, workflow: Dict[str, Any], build_config: BuildConfig) -> str:
        """Add frontend build job to workflow."""
        edition = "ee" if build_config.enterprise else "ce"
        suffix = "" if build_config.sanitizer == "" else f"-{build_config.sanitizer}"
        name = f"build-{edition}-{build_config.architecture}{suffix}-frontend"

        workflow["jobs"].append({"build-frontend": {"name": name}})
        return name

    def _add_cppcheck_job(self, workflow: Dict[str, Any], build_job: str) -> None:
        """Add cppcheck job to workflow."""
        workflow["jobs"].append({
            "run-cppcheck": {
                "name": "cppcheck",
                "requires": [build_job],
            }
        })

    def _add_create_docker_image_job(self, workflow: Dict[str, Any],
                                     build_config: BuildConfig,
                                     build_jobs: List[str]) -> None:
        """Add docker image creation job."""
        from datetime import date
        import re

        edition = "ee" if build_config.enterprise else "ce"
        arch = "amd64" if build_config.architecture == "x64" else "arm64"
        image = (
            "public.ecr.aws/b0b8h2r4/enterprise-preview"
            if build_config.enterprise
            else "public.ecr.aws/b0b8h2r4/arangodb-preview"
        )

        branch = os.environ.get("CIRCLE_BRANCH", "unknown-branch")
        match = re.fullmatch(r"(.+\/)?(.+)", branch)
        if match:
            branch = match.group(2)

        sha1 = os.environ.get("CIRCLE_SHA1", "unknown-sha1")[:7]
        tag = f"{date.today()}-{branch}-{sha1}-{arch}"

        workflow["jobs"].append({
            "create-docker-image": {
                "name": f"create-{edition}-{build_config.architecture}-docker-image",
                "resource-class": self._get_size("large", build_config.architecture),
                "arch": arch,
                "tag": f"{image}:{tag}",
                "requires": build_jobs,
            }
        })

    def _add_hotbackup_job(self, workflow: Dict[str, Any],
                           build_config: BuildConfig,
                           build_jobs: List[str]) -> None:
        """Add hotbackup test job."""
        workflow["jobs"].append({
            "run-hotbackup-tests": {
                "name": f"run-hotbackup-tests-{build_config.architecture}",
                "size": self._get_test_size("medium", build_config, True),
                "requires": build_jobs,
            }
        })

    def _add_rta_ui_test_jobs(self, workflow: Dict[str, Any],
                             build_config: BuildConfig,
                             build_jobs: List[str]) -> None:
        """Add RTA UI test jobs."""
        ui_testsuites = [
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

        if self.args.ui_testsuites:
            ui_testsuites = self.args.ui_testsuites.split(",")

        deployments = ["SG", "CL"]
        if self.args.ui_deployments:
            deployments = self.args.ui_deployments.split(",")

        ui_filter = ""
        for suite in ui_testsuites:
            ui_filter += f"--ui-include-test-suite {suite} "

        for deployment in deployments:
            job = {
                "name": f"test-{deployment}-UI",
                "suiteName": ui_filter,
                "arangosh_args": "",
                "deployment": deployment,
                "browser": "Remote_CHROME",
                "enterprise": "EP" if build_config.enterprise else "C",
                "filterStatement": ui_filter,
                "requires": build_jobs,
                "rta-branch": self.args.rta_branch,
                "buckets": len(ui_testsuites),
            }
            workflow["jobs"].append({"run-rta-tests": job})

    def _add_test_jobs(self, workflow: Dict[str, Any], jobs: List[TestJob],
                      build_config: BuildConfig, build_jobs: List[str]) -> None:
        """Add all test jobs to workflow."""
        for job in jobs:
            deployment_type = job.options.deployment_type

            if deployment_type == DeploymentType.CLUSTER:
                workflow["jobs"].append(
                    self._create_test_job(job, DeploymentType.CLUSTER, build_config, build_jobs)
                )
                if self.args.replication_two:
                    workflow["jobs"].append(
                        self._create_test_job(job, DeploymentType.CLUSTER, build_config, build_jobs, replication_version=2)
                    )
            elif deployment_type == DeploymentType.SINGLE:
                workflow["jobs"].append(
                    self._create_test_job(job, DeploymentType.SINGLE, build_config, build_jobs)
                )
            elif deployment_type == DeploymentType.MIXED:
                workflow["jobs"].append(
                    self._create_test_job(job, DeploymentType.MIXED, build_config, build_jobs)
                )
            else:
                # No deployment type specified - run both
                workflow["jobs"].append(
                    self._create_test_job(job, DeploymentType.CLUSTER, build_config, build_jobs)
                )
                if self.args.replication_two:
                    workflow["jobs"].append(
                        self._create_test_job(job, DeploymentType.CLUSTER, build_config, build_jobs, replication_version=2)
                    )
                workflow["jobs"].append(
                    self._create_test_job(job, DeploymentType.SINGLE, build_config, build_jobs)
                )

    def _create_test_job(self, job: TestJob, deployment_type: DeploymentType,
                        build_config: BuildConfig, build_jobs: List[str],
                        replication_version: int = 1) -> Dict[str, Any]:
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
        # Determine suite name
        suite_names = [suite.name for suite in job.suites]
        suite_str = ','.join(suite_names)

        # Get size
        size = job.options.size or ResourceSize.SMALL
        size_str = size.value

        # Build deployment string
        is_cluster = deployment_type == DeploymentType.CLUSTER
        if is_cluster:
            deployment_str = f"cluster{'-repl2' if replication_version == 2 else ''}"
        elif deployment_type == DeploymentType.SINGLE:
            deployment_str = "single"
        else:
            deployment_str = "mixed"

        # Build job dict
        job_dict = {
            "name": f"test-{deployment_str}-{job.name}-{build_config.architecture}",
            "suiteName": job.name,
            "suites": suite_str,
            "size": self._get_test_size(size_str, build_config, is_cluster),
            "cluster": is_cluster,
            "requires": build_jobs,
            "arangosh_args": "A " + json.dumps(job.arguments.arangosh_args + self.args.arangosh_args),
        }

        # Special cases for specific suites
        if job.name == "chaos" and build_config.nightly:
            job_dict["timeLimit"] = 32 * 5 * 60

        if job.name == "shell_client_aql" and build_config.nightly and not is_cluster:
            job_dict["size"] = self._get_test_size("medium+", build_config, is_cluster)

        # Add extra args
        extra_args = job.arguments.extra_args.copy()
        if is_cluster:
            extra_args.extend(["--replicationVersion", str(replication_version)])
        if build_config.nightly:
            extra_args.extend(["--skipNightly", "false"])
        if extra_args or self.args.extra_args:
            job_dict["extraArgs"] = " ".join(extra_args + self.args.extra_args)

        # Add buckets
        bucket_count = job.get_bucket_count()
        if job.name == "replication_sync":
            # Special case: override bucket count for CircleCI
            bucket_count = 5
        if bucket_count and bucket_count != 1:
            job_dict["buckets"] = bucket_count

        # Add repository info if present
        if job.repository:
            job_dict["docker_image"] = self.args.default_container.replace(
                ':', job.repository.git_branch + ':'
            )
            job_dict["driver-git-repo"] = job.repository.git_repo
            job_dict["driver-git-branch"] = job.repository.git_branch or "main"
            job_dict["init_driver_repo_command"] = ""  # TODO: Get from repository config
        else:
            job_dict["docker_image"] = self.args.default_container
            job_dict["driver-git-repo"] = ""
            job_dict["driver-git-branch"] = ""
            job_dict["init_driver_repo_command"] = ""

        return {"run-linux-tests": job_dict}

    def _get_size(self, size: str, arch: str) -> str:
        """Get CircleCI resource class for size and architecture."""
        aarch64_sizes = {
            "small": "arm.medium",
            "medium": "arm.medium",
            "medium+": "arm.large",
            "large": "arm.large",
            "xlarge": "arm.xlarge",
            "2xlarge": "arm.2xlarge",
        }
        x86_sizes = {
            "small": "small",
            "medium": "medium",
            "medium+": "medium+",
            "large": "large",
            "xlarge": "xlarge",
            "2xlarge": "2xlarge",
        }
        return aarch64_sizes[size] if arch == "aarch64" else x86_sizes[size]

    def _get_test_size(self, size: str, build_config: BuildConfig, is_cluster: bool) -> str:
        """
        Get test resource size considering sanitizer overhead.

        Args:
            size: Base size string
            build_config: Build configuration
            is_cluster: Whether this is a cluster test

        Returns:
            Adjusted size string for CircleCI
        """
        if build_config.sanitizer != "":
            # Sanitizer builds need more resources
            if size == "small":
                size = "xlarge" if build_config.sanitizer == "tsan" and is_cluster else "large"
            elif size in ["medium", "medium+", "large"]:
                size = "xlarge"

        return self._get_size(size, build_config.architecture)
