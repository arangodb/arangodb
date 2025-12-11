"""
Filtering logic for test jobs based on environment and CLI arguments.

This module provides functions to filter test jobs based on various criteria
such as full runs, test types, platform exclusions, etc.
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

    # Test type filters
    full: bool = False  # Include full test set (not just PR subset)
    gtest: bool = False

    # Build configuration
    sanitizer: Optional[Sanitizer] = None  # Sanitizer type for this build
    architecture: Optional[Architecture] = None  # Current build architecture
    v8: bool = True  # Whether V8 (JavaScript) is enabled in this build

    # Deployment type filter (None = accept all deployment types)
    deployment_type: Optional[DeploymentType] = None

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

    @property
    def is_v8_build(self) -> bool:
        """Check if this is a V8-enabled build."""
        return self.v8


def is_gtest_suite(suite: SuiteConfig) -> bool:
    """
    Check if a suite is a gtest suite.

    Args:
        suite: Suite configuration to check

    Returns:
        True if suite name starts with 'gtest'
    """
    return suite.name.startswith(GTEST_PREFIX)


def _check_requirements_match(
    requires: TestRequirements, criteria: FilterCriteria
) -> bool:
    """
    Check if test requirements match filter criteria.

    This implements the common filtering logic for both jobs and suites:
    - Architecture compatibility
    - Full flag compatibility
    - Instrumentation flag compatibility

    Args:
        requires: TestRequirements to check
        criteria: FilterCriteria to apply

    Returns:
        True if requirements match criteria, False otherwise
    """
    # Check architecture compatibility FIRST
    # If test specifies architecture, current architecture must match
    if requires.architecture is not None and criteria.architecture is not None:
        if criteria.architecture != requires.architecture:
            return False

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

    # Check v8 flag compatibility
    # - v8=True: Only for V8-enabled builds
    # - v8=False: Only for non-V8 builds (JavaScript disabled)
    # - v8=None (unspecified): Include in both
    if requires.v8 is True and not criteria.is_v8_build:
        return False  # Requires V8 build, but we're in non-V8 mode
    if requires.v8 is False and criteria.is_v8_build:
        return False  # Non-V8-only, but we're in V8 mode

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
    if criteria.deployment_type is not None:
        job_deployment = job.options.deployment_type

        # If job has no deployment type specified, it runs in all modes
        if job_deployment is None:
            pass  # Include job
        # If job specifies MIXED, it should be included in both single and cluster filters
        elif job_deployment == DeploymentType.MIXED:
            pass  # Include job
        # Otherwise, deployment types must match
        elif job_deployment != criteria.deployment_type:
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

    This uses job.get_resolved_suites() to ensure job-level requirements
    (like full, instrumentation, etc.) are properly inherited by suites
    that don't have their own requirements specified.

    Args:
        job: TestJob containing suites to filter
        criteria: FilterCriteria to apply

    Returns:
        Filtered list of SuiteConfig objects with job-level options merged
    """
    return [
        suite
        for suite in job.get_resolved_suites()
        if should_include_suite(suite, criteria)
    ]


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
