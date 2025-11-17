"""
Shared pytest fixtures for unit tests.
"""

import pytest
from datetime import date
from src.config_lib import (
    BuildConfig,
    Architecture,
    Sanitizer,
    TestJob,
    SuiteConfig,
    TestOptions,
)
from src.filters import FilterCriteria
from src.output_generators.base import (
    GeneratorConfig,
    TestExecutionConfig,
    CircleCIConfig,
)
from src.output_generators.circleci import CircleCIGenerator


@pytest.fixture
def x64_enterprise_build():
    """Standard x64 build config."""
    return BuildConfig(architecture=Architecture.X64)


@pytest.fixture
def x64_tsan_build():
    """x64 with TSAN sanitizer."""
    return BuildConfig(architecture=Architecture.X64, sanitizer=Sanitizer.TSAN)


@pytest.fixture
def x64_alubsan_build():
    """x64 with ALUBSAN sanitizer."""
    return BuildConfig(architecture=Architecture.X64, sanitizer=Sanitizer.ALUBSAN)


@pytest.fixture
def aarch64_enterprise_build():
    """ARM64 build config."""
    return BuildConfig(architecture=Architecture.AARCH64)


@pytest.fixture
def mixed_suite_job():
    """Job with PR, full, and any suites for filter testing."""
    return TestJob(
        name="test_job",
        suites=[
            SuiteConfig(name="pr_suite", options=TestOptions(full=False)),
            SuiteConfig(name="full_suite", options=TestOptions(full=True)),
            SuiteConfig(name="any_suite", options=TestOptions()),
        ],
        options=TestOptions(),
    )


@pytest.fixture
def generator_factory():
    """Factory for creating CircleCIGenerator with custom config."""

    def _create(
        ui="",
        replication_two=False,
        create_docker_images=False,
        env_vars=None,
        test_date=None,
        **filter_kwargs,
    ):
        filter_criteria = FilterCriteria(**filter_kwargs)
        test_exec = TestExecutionConfig(replication_two=replication_two)
        circleci_config = CircleCIConfig(
            ui=ui, create_docker_images=create_docker_images
        )
        config = GeneratorConfig(
            filter_criteria=filter_criteria,
            test_execution=test_exec,
            circleci=circleci_config,
        )

        env_getter = lambda k, default: (
            env_vars.get(k, default) if env_vars else default
        )
        date_provider = lambda: test_date if test_date else date.today()

        return CircleCIGenerator(
            config, base_config={}, env_getter=env_getter, date_provider=date_provider
        )

    return _create
