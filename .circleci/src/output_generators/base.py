"""
Base class for output generators.

Output generators take parsed test definitions and build-specific configuration,
then produce output in a specific format (CircleCI YAML, Jenkins launcher format, etc.).
"""

from abc import ABC, abstractmethod
from typing import Any, List, Dict, Optional
from dataclasses import dataclass, field

from ..config_lib import TestJob, TestDefinitionFile, BuildConfig
from ..filters import FilterCriteria, PlatformFlags


@dataclass
class TestExecutionConfig:
    """Configuration for test execution."""

    arangosh_args: List[str] = field(default_factory=list)
    extra_args: List[str] = field(default_factory=list)
    arangod_without_v8: bool = False
    create_report: bool = True
    replication_two: bool = False


@dataclass
class CircleCIConfig:
    """CircleCI-specific configuration."""

    ui: str = ""  # off, on, only, community
    ui_testsuites: str = ""
    ui_deployments: str = ""
    rta_branch: Optional[str] = None  # Use None to serialize as 'null' in YAML
    create_docker_images: bool = False
    default_container: str = ""


@dataclass
class GeneratorConfig:
    """
    Configuration for output generators.

    This consolidates all configuration that generators need, organized
    by concern.
    """

    filter_criteria: FilterCriteria
    test_execution: TestExecutionConfig = field(default_factory=TestExecutionConfig)
    circleci: CircleCIConfig = field(default_factory=CircleCIConfig)
    validate_only: bool = False


class OutputGenerator(ABC):
    """
    Abstract base class for output generators.

    Subclasses implement specific output formats (CircleCI, Jenkins, etc.)
    """

    def __init__(self, config: GeneratorConfig):
        """
        Initialize the generator with configuration.

        Args:
            config: Generator configuration
        """
        self.config = config

    @abstractmethod
    def generate(self, test_defs: List[TestDefinitionFile], **kwargs) -> Any:
        """
        Generate output from test definitions.

        Args:
            test_defs: List of TestDefinitionFile objects to process
            **kwargs: Additional generator-specific arguments

        Returns:
            Generated output in the appropriate format
        """
        pass

    @abstractmethod
    def write_output(self, output: Any, destination: str) -> None:
        """
        Write generated output to a file or stdout.

        Args:
            output: Output from generate()
            destination: Where to write (filename or special value like '-' for stdout)
        """
        pass

    def filter_jobs(self, test_def: TestDefinitionFile) -> List[TestJob]:
        """
        Apply filtering to jobs based on configuration.

        This is a convenience method that subclasses can override or use.

        Args:
            test_def: Test definition file

        Returns:
            Filtered list of jobs
        """
        from ..filters import filter_jobs

        return filter_jobs(test_def, self.config.filter_criteria)
