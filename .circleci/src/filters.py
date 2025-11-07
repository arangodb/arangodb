"""
Filtering logic for test jobs based on environment and CLI arguments.

This module provides functions to filter test jobs based on various criteria
such as deployment type, full/nightly runs, platform exclusions, etc.
"""

from typing import List, Optional
from dataclasses import dataclass, field
from .config_lib import (
    TestJob,
    TestDefinitionFile,
    DeploymentType,
    SuiteConfig,
    Sanitizer,
)


# Prefix for gtest suite names
GTEST_PREFIX = "gtest"


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
    full: bool = False  # Include full test set (not just PR subset)
    nightly: bool = False
    gtest: bool = False

    # Build configuration
    sanitizer: Optional[Sanitizer] = None  # Sanitizer type for this build

    # Feature flags
    enterprise: bool = True

    # Platform exclusions
    platform: PlatformFlags = field(default_factory=PlatformFlags)

    @property
    def is_full_run(self) -> bool:
        """Check if this is a full or nightly run."""
        return self.full or self.nightly


def is_gtest_suite(suite: SuiteConfig) -> bool:
    """
    Check if a suite is a gtest suite.

    Args:
        suite: Suite configuration to check

    Returns:
        True if suite name starts with 'gtest'
    """
    return suite.name.startswith(GTEST_PREFIX)


def matches_deployment_filter(
    deployment_type: DeploymentType | None, criteria: FilterCriteria
) -> bool:
    """
    Check if a deployment type matches the filter criteria.

    Args:
        deployment_type: Deployment type from job options (or None)
        criteria: Filter criteria to check against

    Returns:
        True if deployment type matches the filter
    """
    # If both cluster and single are requested (or neither), accept all
    if criteria.cluster == criteria.single:
        return True

    # Cluster-only filter
    if criteria.cluster:
        return deployment_type in (DeploymentType.CLUSTER, DeploymentType.MIXED, None)

    # Single-only filter
    if criteria.single:
        return deployment_type in (DeploymentType.SINGLE, None)

    return True


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

    # Check full flag compatibility with build type
    # - full=True: Only for nightly/full builds
    # - full=False: Only for PR builds
    # - full=None (unspecified): Include in both
    if job.options.full is True and not criteria.is_full_run:
        # Job requires full run, but we're in PR mode - exclude
        return False
    if job.options.full is False and criteria.is_full_run:
        # Job is PR-only, but we're in full/nightly mode - exclude
        return False

    # Check deployment type filter
    if not matches_deployment_filter(job.options.deployment_type, criteria):
        return False

    # Check gtest filter
    if criteria.gtest and not any(is_gtest_suite(suite) for suite in job.suites):
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
