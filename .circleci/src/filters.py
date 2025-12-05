"""
Filtering logic for test jobs based on environment and CLI arguments.

This module provides functions to filter test jobs based on various criteria
such as deployment type, full runs, platform exclusions, etc.
"""

from typing import List, Optional
from dataclasses import dataclass
from .config_lib import (
    TestJob,
    TestDefinitionFile,
    DeploymentType,
    SuiteConfig,
    Sanitizer,
    Architecture,
    TestRequirements,
)


# Prefix for gtest suite names
GTEST_PREFIX = "gtest"


@dataclass
class FilterCriteria:
    """Criteria for filtering test jobs."""

    # Deployment type filters
    cluster: bool = False
    single: bool = False
    all_tests: bool = False

    # Test type filters
    full: bool = False  # Include full test set (not just PR subset)
    gtest: bool = False

    # Build configuration
    sanitizer: Optional[Sanitizer] = None  # Sanitizer type for this build
    architecture: Optional[Architecture] = None  # Current build architecture

    # Feature flags
    enterprise: bool = True

    @property
    def is_full_run(self) -> bool:
        """Check if this is a full run."""
        return self.full

    @property
    def is_instrumented_build(self) -> bool:
        """Check if this is an instrumented build (TSan/ALUBSan)."""
        return self.sanitizer is not None


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


def _check_requirements_match(
    requires: TestRequirements, criteria: FilterCriteria
) -> bool:
    """
    Check if test requirements match filter criteria.

    This implements the common filtering logic for both jobs and suites:
    - Architecture compatibility (even in all_tests mode)
    - Full flag compatibility
    - Instrumentation flag compatibility

    Args:
        requires: TestRequirements to check
        criteria: FilterCriteria to apply

    Returns:
        True if requirements match criteria, False otherwise
    """
    # Check architecture compatibility FIRST (even in all_tests mode)
    # If test specifies architecture, current architecture must match
    if requires.architecture is not None and criteria.architecture is not None:
        if criteria.architecture != requires.architecture:
            return False

    if criteria.all_tests:
        return True

    # Check full flag compatibility with build type
    # - full=True: Only for full runs
    # - full=False: Only for PR builds
    # - full=None (unspecified): Include in both
    if requires.full is True and not criteria.is_full_run:
        return False  # Requires full run, but we're in PR mode
    if requires.full is False and criteria.is_full_run:
        return False  # PR-only, but we're in full mode

    # Check instrumentation flag compatibility
    # - instrumentation=True: Only for instrumented builds (TSAN/ASAN/coverage)
    # - instrumentation=False: Only for non-instrumented builds
    # - instrumentation=None (unspecified): Include in both
    if requires.instrumentation is True and not criteria.is_instrumented_build:
        return False  # Requires instrumented build, but we're in regular mode
    if requires.instrumentation is False and criteria.is_instrumented_build:
        return False  # Regular-build-only, but we're in instrumented mode

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
    # Check common requirements (architecture, full, instrumentation)
    if not _check_requirements_match(job.requires, criteria):
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


def should_include_suite(suite: SuiteConfig, criteria: FilterCriteria) -> bool:
    """
    Determine if a suite should be included based on filter criteria.

    This applies suite-level filtering that may override job-level settings.

    Args:
        suite: SuiteConfig to check
        criteria: FilterCriteria to apply

    Returns:
        True if suite should be included, False otherwise
    """
    # Check common requirements (architecture, full, instrumentation)
    return _check_requirements_match(suite.requires, criteria)


def filter_suites(job: TestJob, criteria: FilterCriteria) -> List[SuiteConfig]:
    """
    Filter suites from a job based on criteria.

    Args:
        job: TestJob containing suites to filter
        criteria: FilterCriteria to apply

    Returns:
        Filtered list of SuiteConfig objects
    """
    return [suite for suite in job.suites if should_include_suite(suite, criteria)]


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
    for job in test_def.jobs.values():
        if should_include_job(job, criteria):
            filtered.append(job)

    return filtered
