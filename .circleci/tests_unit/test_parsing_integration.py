"""
Integration tests for parsing real YAML test definition files.

These tests verify that all actual test definition files can be parsed
successfully and that complex structures work correctly.
"""

import pytest
from pathlib import Path
from src.config_lib import TestDefinitionFile, DeploymentType


TESTS_DIR = Path(__file__).parent.parent.parent / "tests"


class TestParsingIntegration:
    """Integration tests verifying real YAML files parse correctly."""

    def test_all_yaml_files_parse_successfully(self):
        """Verify all YAML test definition files parse without errors."""
        yaml_files = sorted(TESTS_DIR.glob("*.yml"))
        assert len(yaml_files) > 0, "Should find YAML files in tests directory"

        for yaml_file in yaml_files:
            test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))
            assert test_def.jobs, f"{yaml_file.name} should have at least one job"

    def test_main_test_definitions_structure(self):
        """Verify main test definitions file has expected structure."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        # Should have many jobs
        assert len(test_def.jobs) > 50

        # Verify mix of deployment types exist
        single_jobs = [
            j
            for j in test_def.jobs.values()
            if j.options.deployment_type == DeploymentType.SINGLE
        ]
        cluster_jobs = [
            j
            for j in test_def.jobs.values()
            if j.options.deployment_type == DeploymentType.CLUSTER
        ]

        assert len(single_jobs) > 0, "Should have single server jobs"
        assert len(cluster_jobs) > 0, "Should have cluster jobs"

    def test_multi_suite_jobs_with_auto_buckets(self):
        """Verify multi-suite jobs with auto buckets work correctly."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        # Find multi-suite jobs
        multi_suite_jobs = [j for j in test_def.jobs.values() if j.is_multi_suite()]
        assert len(multi_suite_jobs) > 0, "Should have multi-suite jobs"

        # Verify auto bucket behavior
        for job in multi_suite_jobs:
            if job.options.buckets == "auto":
                assert job.get_bucket_count() == len(job.suites)

    def test_driver_tests_have_repository_config(self):
        """Verify driver test files correctly set repository configuration."""
        driver_files = ["arangojs.yml", "go.yml", "java.yml", "kafka.yml"]

        for filename in driver_files:
            yaml_file = TESTS_DIR / filename
            if not yaml_file.exists():
                continue

            test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

            # All driver test jobs should have repository config
            for job in test_def.jobs.values():
                assert (
                    job.repository is not None
                ), f"Job in {filename} should have repository config"
                assert (
                    job.repository.git_repo
                ), f"Job in {filename} should have git_repo"
                assert (
                    job.repository.git_branch
                ), f"Job in {filename} should have git_branch"
