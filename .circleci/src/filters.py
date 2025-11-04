"""
Filtering logic for test jobs based on environment and CLI arguments.

This module provides functions to filter test jobs based on various criteria
such as deployment type, full/nightly runs, platform exclusions, etc.
"""

from typing import List, Dict, Any, Optional
from dataclasses import dataclass
from .config_lib import TestJob, TestDefinitionFile, DeploymentType


@dataclass
class PlatformFlags:
    """Platform-specific flags for filtering tests."""

    is_windows: bool = False
    is_mac: bool = False
    is_arm: bool = False
    is_coverage: bool = False


@dataclass
class FilterCriteria:
    """Criteria for filtering test jobs."""

    # Deployment type filters
    cluster: bool = False
    single: bool = False
    all_tests: bool = False

    # Test type filters
    full: bool = False
    nightly: bool = False
    gtest: bool = False

    # Feature flags
    enterprise: bool = True

    # Platform exclusions
    platform: PlatformFlags = None

    def __post_init__(self):
        """Initialize platform flags if not provided."""
        if self.platform is None:
            self.platform = PlatformFlags()

    @property
    def is_full_run(self) -> bool:
        """Check if this is a full or nightly run."""
        return self.full or self.nightly


def should_include_job(job: TestJob, criteria: FilterCriteria) -> bool:
    """
    Determine if a job should be included based on filter criteria.

    Args:
        job: TestJob to check
        criteria: FilterCriteria to apply

    Returns:
        True if job should be included, False otherwise
    """
    if criteria.all_tests:
        return True

    # Check deployment type filters
    deployment_type = job.options.deployment_type

    # If specific deployment type requested
    if criteria.cluster and not criteria.single:
        if deployment_type not in (DeploymentType.CLUSTER, DeploymentType.MIXED, None):
            return False
    elif criteria.single and not criteria.cluster:
        if deployment_type not in (DeploymentType.SINGLE, None):
            return False

    # Check gtest filter
    if criteria.gtest:
        if not any(suite.name.startswith("gtest") for suite in job.suites):
            return False

    # Platform exclusions would be checked at suite level in the original code
    # For now, we accept all jobs at the job level
    # Suite-level filtering would happen during job execution

    return True


def filter_jobs(
    test_def: TestDefinitionFile, criteria: FilterCriteria
) -> List[TestJob]:
    """
    Filter jobs from a test definition file based on criteria.

    Args:
        test_def: TestDefinitionFile containing jobs to filter
        criteria: FilterCriteria to apply

    Returns:
        Filtered list of TestJob objects
    """
    filtered = []
    for job_name, job in test_def.jobs.items():
        if should_include_job(job, criteria):
            filtered.append(job)

    return filtered
