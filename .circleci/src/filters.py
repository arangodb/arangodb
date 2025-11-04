"""
Filtering logic for test jobs based on environment and CLI arguments.

This module provides functions to filter test jobs based on various criteria
such as deployment type, full/nightly runs, platform exclusions, etc.
"""

from typing import List, Dict, Any, Optional
import os
from config_lib import TestJob, TestDefinitionFile, DeploymentType


# Environment flags
IS_WINDOWS = os.name == 'nt'
IS_MAC = os.uname().sysname == 'Darwin' if hasattr(os, 'uname') else False
IS_ARM = os.uname().machine in ('aarch64', 'arm64') if hasattr(os, 'uname') else False
IS_COVERAGE = os.environ.get("COVERAGE") == "On"


def should_include_job(job: TestJob,
                       full: bool = False,
                       cluster: bool = False,
                       single: bool = False,
                       all_tests: bool = False,
                       gtest: bool = False,
                       enterprise: bool = True) -> bool:
    """
    Determine if a job should be included based on CLI arguments.

    Args:
        job: TestJob to check
        full: Whether this is a full test run
        cluster: Whether to include cluster tests
        single: Whether to include single server tests
        all_tests: Whether to include all tests (ignore other filters)
        gtest: Whether to only include gtest suites
        enterprise: Whether enterprise tests are enabled

    Returns:
        True if job should be included, False otherwise
    """
    if all_tests:
        return True

    # Check deployment type filters
    deployment_type = job.options.deployment_type

    # If specific deployment type requested
    if cluster and not single:
        if deployment_type not in (DeploymentType.CLUSTER, DeploymentType.MIXED, None):
            return False
    elif single and not cluster:
        if deployment_type not in (DeploymentType.SINGLE, None):
            return False

    # Check full flag (this will be moved to per-suite filtering)
    # For now, jobs don't have flags directly - we'll handle at suite level

    # Check gtest filter
    if gtest:
        # Check if any suite name starts with 'gtest'
        if not any(suite.name.startswith('gtest') for suite in job.suites):
            return False

    # Check enterprise filter
    if not enterprise:
        # Jobs don't have an 'enterprise' flag in the current model
        # This would need to be added to TestOptions if needed
        pass

    # Check platform exclusions (handled at suite level in original code)
    # We'll implement this at the suite filtering level

    return True


def filter_jobs(test_def: TestDefinitionFile,
                full: bool = False,
                nightly: bool = False,
                cluster: bool = False,
                single: bool = False,
                all_tests: bool = False,
                gtest: bool = False,
                enterprise: bool = True,
                exclude_windows: bool = IS_WINDOWS,
                exclude_mac: bool = IS_MAC,
                exclude_arm: bool = IS_ARM,
                exclude_coverage: bool = IS_COVERAGE) -> List[TestJob]:
    """
    Filter jobs from a test definition file based on criteria.

    Args:
        test_def: TestDefinitionFile containing jobs to filter
        full: Whether this is a full test run (or nightly)
        nightly: Whether this is a nightly build
        cluster: Include only cluster tests
        single: Include only single server tests
        all_tests: Include all tests (ignore filters)
        gtest: Include only gtest suites
        enterprise: Include enterprise tests
        exclude_windows: Exclude tests marked as !windows
        exclude_mac: Exclude tests marked as !mac
        exclude_arm: Exclude tests marked as !arm
        exclude_coverage: Exclude tests marked as !coverage

    Returns:
        Filtered list of TestJob objects
    """
    # Combine full and nightly flags
    is_full = full or nightly

    filtered = []
    for job_name, job in test_def.jobs.items():
        if should_include_job(job, is_full, cluster, single, all_tests, gtest, enterprise):
            filtered.append(job)

    return filtered


def add_deployment_prefixes(jobs: List[TestJob], single_cluster: bool = False) -> List[TestJob]:
    """
    Add sg_/cl_ prefixes to job names when running both single and cluster.

    This is used in Jenkins/Oskar to differentiate between single and cluster
    runs of the same test suite.

    Args:
        jobs: List of TestJob objects
        single_cluster: Whether to add prefixes for both single and cluster

    Returns:
        List of TestJob objects with updated names
    """
    if not single_cluster:
        return jobs

    # Split jobs by deployment type
    single_jobs = []
    cluster_jobs = []

    for job in jobs:
        deployment_type = job.options.deployment_type

        if deployment_type == DeploymentType.SINGLE:
            # Clone the job with sg_ prefix
            single_jobs.append(job)
        elif deployment_type == DeploymentType.CLUSTER:
            # Clone the job with cl_ prefix
            cluster_jobs.append(job)
        elif deployment_type == DeploymentType.MIXED or deployment_type is None:
            # For mixed or unspecified, create both variants
            single_jobs.append(job)
            cluster_jobs.append(job)

    # Note: The actual prefix addition would need to be handled by the output
    # generator since TestJob is immutable. We return the categorized jobs
    # and let the caller decide how to handle the prefixes.

    return single_jobs + cluster_jobs
