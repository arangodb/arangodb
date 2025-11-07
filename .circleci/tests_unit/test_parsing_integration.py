"""
Integration tests for parsing real YAML test definition files.

These tests verify that all actual test definition files can be parsed
successfully and that the parsed data matches expected structure.
"""

import pytest
from pathlib import Path
from src.config_lib import TestDefinitionFile, DeploymentType, ResourceSize


# Path to test definition files relative to the tests_unit directory
TESTS_DIR = Path(__file__).parent.parent.parent / "tests"


class TestParseRealYAMLFiles:
    """Test parsing of actual YAML test definition files."""

    def test_parse_main_test_definitions(self):
        """Test parsing the main test-definitions.yml file."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        # Should have many jobs (73 because duplicates with different deployment types are kept)
        assert len(test_def.jobs) == 73

        # Verify some known jobs exist (with deployment type suffix for disambiguation)
        assert "single_server_only_single" in test_def.jobs
        assert "recovery_cluster_cluster" in test_def.jobs
        assert "dump_cluster_large_cluster" in test_def.jobs

        # Check a specific single-server job
        single_job = test_def.jobs["single_server_only_single"]
        assert single_job.options.deployment_type == DeploymentType.SINGLE
        assert len(single_job.suites) == 4
        assert single_job.suites[0].name == "BackupAuthNoSysTests"

        # Check a multi-suite job with auto buckets
        dump_job = test_def.jobs["dump_cluster_large_cluster"]
        assert dump_job.options.buckets == "auto"
        assert dump_job.options.deployment_type == DeploymentType.CLUSTER
        assert dump_job.is_multi_suite()
        assert len(dump_job.suites) >= 2

    def test_parse_arangojs_test_definitions(self):
        """Test parsing arangojs driver test definitions."""
        yaml_file = TESTS_DIR / "arangojs.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        # Should have jobs from the 'tests' section
        # Jobs with same name but different deployment types get unique keys
        assert len(test_def.jobs) >= 1
        js_jobs = [job for key, job in test_def.jobs.items() if job.name == "js_driver"]
        assert len(js_jobs) >= 1

        # Verify repository config is set from jobProperties
        job = js_jobs[0]
        assert job.repository is not None
        assert "arangojs" in job.repository.git_repo
        assert job.repository.git_branch == "main"

    def test_parse_go_test_definitions(self):
        """Test parsing Go driver test definitions."""
        yaml_file = TESTS_DIR / "go.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        # Jobs with same name but different deployment types/suffixes get unique keys
        assert len(test_def.jobs) >= 1
        go_jobs = [job for key, job in test_def.jobs.items() if job.name == "go_driver"]
        assert len(go_jobs) >= 1

        job = go_jobs[0]
        assert job.repository is not None
        assert "go-driver" in job.repository.git_repo
        assert job.repository.git_branch == "master"

    def test_parse_java_test_definitions(self):
        """Test parsing Java driver test definitions."""
        yaml_file = TESTS_DIR / "java.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        # Jobs with same name but different deployment types get unique keys
        assert len(test_def.jobs) >= 1
        java_jobs = [
            job for key, job in test_def.jobs.items() if job.name == "java_driver"
        ]
        assert len(java_jobs) >= 1

        job = java_jobs[0]
        assert job.repository is not None
        assert "java-driver" in job.repository.git_repo

    def test_parse_kafka_test_definitions(self):
        """Test parsing Kafka connector test definitions."""
        yaml_file = TESTS_DIR / "kafka.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        # Kafka test has unique deployment type (MIXED) so gets a unique key
        assert len(test_def.jobs) >= 1
        kafka_jobs = [
            job for key, job in test_def.jobs.items() if job.name == "kafka_driver"
        ]
        assert len(kafka_jobs) >= 1

        job = kafka_jobs[0]
        assert job.repository is not None
        assert "kafka-connect" in job.repository.git_repo
        # Kafka test is MIXED type with multiple suites
        assert job.options.deployment_type == DeploymentType.MIXED
        assert job.is_multi_suite()

    def test_all_files_parse_without_errors(self):
        """Verify all YAML files can be parsed without exceptions."""
        yaml_files = [
            "test-definitions.yml",
            "arangojs.yml",
            "go.yml",
            "java.yml",
            "kafka.yml",
            "spark-datasource.yml",
        ]

        for filename in yaml_files:
            yaml_file = TESTS_DIR / filename
            # Should not raise any exceptions
            test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))
            assert test_def.jobs, f"{filename} should have at least one job"


class TestJobStructures:
    """Test specific job structures and patterns from real YAML files."""

    def test_single_suite_jobs(self):
        """Test jobs with single suites."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        job = test_def.jobs["replication_fuzz_single"]
        assert not job.is_multi_suite()
        assert len(job.suites) == 1
        assert job.suites[0].name == "replication_fuzz"
        assert job.options.deployment_type == DeploymentType.SINGLE

    def test_multi_suite_jobs_with_auto_buckets(self):
        """Test multi-suite jobs must use auto buckets."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        job = test_def.jobs["dump_single_single"]
        assert job.is_multi_suite()
        assert job.options.buckets == "auto"
        assert job.get_bucket_count() == len(job.suites)

    def test_jobs_with_arguments(self):
        """Test jobs that have extra arguments."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        # replication_fuzz has args at job level
        job = test_def.jobs["replication_fuzz_single"]
        # Check job-level arguments were parsed
        assert len(job.arguments.extra_args) > 0
        # Verify it contains log level argument
        assert any("log.level" in arg for arg in job.arguments.extra_args)

    def test_jobs_with_priority(self):
        """Test jobs with priority settings."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        job = test_def.jobs["replication_sync_single"]
        assert job.options.priority == 8000

    def test_jobs_with_size(self):
        """Test jobs with resource size specifications."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        job = test_def.jobs["replication_sync_single"]
        assert job.options.size == ResourceSize.MEDIUM

    def test_cluster_jobs(self):
        """Test cluster deployment type jobs."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        job = test_def.jobs["recovery_cluster_cluster"]
        assert job.options.deployment_type == DeploymentType.CLUSTER

    def test_mixed_deployment_jobs(self):
        """Test mixed deployment type jobs."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        job = test_def.jobs["rta_makedata_mixed"]
        assert job.options.deployment_type == DeploymentType.MIXED
        assert job.is_multi_suite()
        # Mixed jobs have multiple suites, each potentially with different deployment
        assert len(job.suites) >= 2


class TestFiltering:
    """Test filtering functionality on parsed jobs."""

    def test_filter_by_deployment_type(self):
        """Test filtering jobs by deployment type."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        single_jobs = test_def.filter_jobs(deployment_type=DeploymentType.SINGLE)
        cluster_jobs = test_def.filter_jobs(deployment_type=DeploymentType.CLUSTER)

        assert len(single_jobs) > 0
        assert len(cluster_jobs) > 0

        # Verify all filtered jobs have correct type
        for job in single_jobs.values():
            assert job.options.deployment_type == DeploymentType.SINGLE

    def test_filter_by_size(self):
        """Test filtering jobs by resource size."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        large_jobs = test_def.filter_jobs(size=ResourceSize.LARGE)
        assert len(large_jobs) > 0

        for job in large_jobs.values():
            assert job.options.size == ResourceSize.LARGE

    def test_filter_by_repository(self):
        """Test filtering jobs with repository configuration."""
        yaml_file = TESTS_DIR / "arangojs.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        repo_jobs = test_def.filter_jobs(has_repository=True)
        assert len(repo_jobs) > 0

        for job in repo_jobs.values():
            assert job.repository is not None


class TestEdgeCases:
    """Test edge cases and special structures."""

    def test_job_without_explicit_suite_name(self):
        """Test jobs where suite name defaults to job name."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        # Jobs without explicit suite field use job name as suite name
        job = test_def.jobs["replication_fuzz_single"]
        assert len(job.suites) == 1
        assert job.suites[0].name == "replication_fuzz"

    def test_suite_with_options_override(self):
        """Test suites that override job-level options."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        # Find a job with suite-level overrides
        job = test_def.jobs["dump_single_single"]
        resolved = job.get_resolved_suites()

        # At least one suite should have some configuration
        assert len(resolved) > 0

    def test_bucket_count_calculation(self):
        """Test bucket count calculation for auto-buckets."""
        yaml_file = TESTS_DIR / "test-definitions.yml"
        test_def = TestDefinitionFile.from_yaml_file(str(yaml_file))

        # Job with explicit buckets
        explicit_job = test_def.jobs["replication_sync_single"]
        assert explicit_job.get_bucket_count() == 9

        # Job with auto buckets
        auto_job = test_def.jobs["dump_single_single"]
        assert auto_job.get_bucket_count() == len(auto_job.suites)

        # Job without buckets
        no_bucket_job = test_def.jobs["single_server_only_single"]
        assert no_bucket_job.get_bucket_count() is None
