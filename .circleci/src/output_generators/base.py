"""
Base class for output generators.

Output generators take parsed test definitions and build-specific configuration,
then produce output in a specific format (CircleCI YAML, Jenkins launcher format, etc.).
"""

from abc import ABC, abstractmethod
from typing import Any, List, Dict
from dataclasses import dataclass

import sys
import os

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

from config_lib import TestJob, TestDefinitionFile, BuildConfig


@dataclass
class GeneratorArgs:
    """
    Common arguments passed to all output generators.

    This consolidates CLI arguments and other configuration that generators need.
    """
    # Test filtering
    full: bool = False
    nightly: bool = False
    cluster: bool = False
    single: bool = False
    all_tests: bool = False
    gtest: bool = False
    single_cluster: bool = False  # Run both single and cluster (Jenkins)

    # Build configuration
    enterprise: bool = True
    sanitizer: str = ""

    # Test execution options
    arangosh_args: List[str] = None
    extra_args: List[str] = None
    arangod_without_v8: str = "false"

    # Replication
    replication_two: bool = False

    # UI tests (CircleCI specific)
    ui: str = ""  # off, on, only, community
    ui_testsuites: str = ""
    ui_deployments: str = ""
    rta_branch: str = ""

    # Docker images
    create_docker_images: bool = False
    default_container: str = ""

    # Output control
    validate_only: bool = False
    create_report: bool = True

    def __post_init__(self):
        """Initialize mutable default values."""
        if self.arangosh_args is None:
            self.arangosh_args = []
        if self.extra_args is None:
            self.extra_args = []


class OutputGenerator(ABC):
    """
    Abstract base class for output generators.

    Subclasses implement specific output formats (CircleCI, Jenkins, etc.)
    """

    def __init__(self, args: GeneratorArgs):
        """
        Initialize the generator with arguments.

        Args:
            args: Generator arguments and configuration
        """
        self.args = args

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
        Apply filtering to jobs based on args.

        This is a convenience method that subclasses can override or use.

        Args:
            test_def: Test definition file

        Returns:
            Filtered list of jobs
        """
        from filters import filter_jobs

        return filter_jobs(
            test_def,
            full=self.args.full,
            nightly=self.args.nightly,
            cluster=self.args.cluster,
            single=self.args.single,
            all_tests=self.args.all_tests,
            gtest=self.args.gtest,
            enterprise=self.args.enterprise,
        )
