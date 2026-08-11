"""
ArangoDB test configuration library.

Shared data models and utilities for test definitions used by CircleCI and Jenkins.
"""

from .config_lib import (
    DeploymentType,
    ResourceSize,
    TestOptions,
    TestArguments,
    SuiteConfig,
    RepositoryConfig,
    TestJob,
    TestDefinitionFile,
    BuildConfig,
)

__all__ = [
    "DeploymentType",
    "ResourceSize",
    "TestOptions",
    "TestArguments",
    "SuiteConfig",
    "RepositoryConfig",
    "TestJob",
    "TestDefinitionFile",
    "BuildConfig",
]
