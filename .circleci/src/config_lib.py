"""
Shared library for parsing and processing ArangoDB test configuration files.

This module provides type-safe data models and utilities for working with YAML
test definition files used by both CircleCI and Jenkins test systems.

Architecture:
- Data models defined as dataclasses with validation
- Parsing functions to convert YAML dicts to typed objects
- Validation logic in __post_init__ methods
- Merging logic for option inheritance
"""

from dataclasses import dataclass, field
from enum import Enum
from typing import Dict, List, Optional, Any, Union, TypeVar, Mapping
import yaml


# ============================================================================
# Enumerations
# ============================================================================

E = TypeVar("E", bound=Enum)


def _enum_from_string(
    enum_class: type[E], value: str, aliases: Optional[Mapping[str, E]] = None
) -> E:
    """
    Generic helper to parse enum from string, case-insensitive.

    Args:
        enum_class: The enum class to parse into
        value: String value to parse
        aliases: Optional dict mapping alias strings to enum values

    Returns:
        Enum member

    Raises:
        ValueError: If value doesn't match any enum member or alias
    """
    normalized = value.lower()

    # Check aliases first if provided
    if aliases and normalized in aliases:
        return aliases[normalized]

    # Check enum members
    for member in enum_class:
        if member.value == normalized:
            return member

    # Build error message
    valid_values = [m.value for m in enum_class]
    if aliases:
        valid_values.extend(f"{alias} (alias)" for alias in aliases.keys())

    raise ValueError(
        f"Invalid {enum_class.__name__}: {value}. Valid options: {', '.join(valid_values)}"
    )


class DeploymentType(Enum):
    """Test deployment type: single server, cluster, or mixed."""

    SINGLE = "single"
    CLUSTER = "cluster"
    MIXED = "mixed"

    @classmethod
    def from_string(cls, value: str) -> "DeploymentType":
        """Parse deployment type from string, case-insensitive."""
        return _enum_from_string(cls, value)


class ResourceSize(Enum):
    """Resource allocation size for test execution."""

    SMALL = "small"
    MEDIUM = "medium"
    MEDIUM_PLUS = "medium+"
    LARGE = "large"
    XLARGE = "xlarge"
    XXLARGE = "2xlarge"

    @classmethod
    def from_string(cls, value: str) -> "ResourceSize":
        """Parse resource size from string, case-insensitive."""
        return _enum_from_string(cls, value)


class Sanitizer(Enum):
    """
    Sanitizer types used in production builds.

    TSAN = Thread Sanitizer
    ALUBSAN = Address + Leak + UndefinedBehavior Sanitizer combined
    """

    TSAN = "tsan"
    ALUBSAN = "alubsan"

    @classmethod
    def from_string(cls, value: str) -> Optional["Sanitizer"]:
        """Parse sanitizer from string, case-insensitive."""
        if value == "none":
            return None
        return _enum_from_string(cls, value)


class Architecture(Enum):
    """
    CPU architecture for builds.

    X64 = x86-64 / AMD64 architecture
    AARCH64 = ARM64 architecture
    """

    X64 = "x64"
    AARCH64 = "aarch64"

    @classmethod
    def from_string(cls, value: str) -> "Architecture":
        """Parse architecture from string, supporting common aliases."""
        aliases = {
            "x86_64": cls.X64,
            "amd64": cls.X64,
            "arm64": cls.AARCH64,
        }
        return _enum_from_string(cls, value, aliases)


# ============================================================================
# Data Models
# ============================================================================


@dataclass
class TestOptions:
    """
    Test execution options that can be specified at job or suite level.

    Suite-level options override job-level options. All fields are optional
    to support partial overrides.
    """

    deployment_type: Optional[DeploymentType] = None
    size: Optional[ResourceSize] = None
    priority: Optional[int] = None
    parallelity: Optional[int] = None
    buckets: Optional[Union[int, str]] = None  # int or "auto"
    replication_version: Optional[str] = None
    suffix: Optional[str] = None  # Job name suffix for disambiguation
    full: Optional[bool] = None  # Only run in full/nightly builds if True
    coverage: Optional[bool] = None  # Run coverage analysis
    time_limit: Optional[int] = None  # Time limit in seconds
    architecture: Optional[Architecture] = None  # Allowed architecture (None = all)

    def __post_init__(self):
        """Validate option values."""
        # Validate priority
        if self.priority is not None and self.priority < 0:
            raise ValueError(f"priority must be non-negative, got: {self.priority}")

        # Validate parallelity
        if self.parallelity is not None and self.parallelity < 1:
            raise ValueError(f"parallelity must be at least 1, got: {self.parallelity}")

        # Validate buckets
        if self.buckets is not None:
            if isinstance(self.buckets, str):
                if self.buckets != "auto":
                    raise ValueError(
                        f"buckets string must be 'auto', got: {self.buckets}"
                    )
            elif isinstance(self.buckets, int):
                if self.buckets < 1:
                    raise ValueError(f"buckets must be at least 1, got: {self.buckets}")
            else:
                raise ValueError(
                    f"buckets must be int or 'auto', got: {type(self.buckets)}"
                )

    @classmethod
    def from_dict(cls, data: Optional[Dict[str, Any]]) -> "TestOptions":
        """
        Create TestOptions from a dictionary, handling type conversions.

        Args:
            data: Dictionary from YAML, or None

        Returns:
            TestOptions instance with all None fields if data is None
        """
        if data is None:
            return cls()

        # Handle type conversions and field name mapping
        kwargs: Dict[str, Any] = {}

        # Map YAML field names to our field names
        if "type" in data and data["type"] is not None:
            kwargs["deployment_type"] = DeploymentType.from_string(data["type"])
        elif "deployment_type" in data and data["deployment_type"] is not None:
            kwargs["deployment_type"] = DeploymentType.from_string(
                data["deployment_type"]
            )

        # Handle size with deployment type-based defaults (matching old generator logic)
        if "size" in data and data["size"] is not None:
            kwargs["size"] = ResourceSize.from_string(data["size"])
        else:
            # Default size based on deployment type (old generator compatibility)
            # cluster and mixed default to medium, single defaults to small
            deployment_type = kwargs.get("deployment_type")
            if deployment_type in (DeploymentType.CLUSTER, DeploymentType.MIXED):
                kwargs["size"] = ResourceSize.MEDIUM
            else:
                kwargs["size"] = ResourceSize.SMALL

        # Direct field mappings (only actual TestOptions fields)
        field_mappings = {
            "priority": "priority",
            "parallelity": "parallelity",
            "buckets": "buckets",
            "replication_version": "replication_version",
            "suffix": "suffix",
            "full": "full",
            "coverage": "coverage",
            "timeLimit": "time_limit",
        }

        for yaml_field, our_field in field_mappings.items():
            if yaml_field in data:
                kwargs[our_field] = data[yaml_field]

        # Handle arch field (parse single architecture string)
        if "arch" in data and data["arch"] is not None:
            kwargs["architecture"] = Architecture.from_string(data["arch"])

        return cls(**kwargs)

    def merge_with(self, override: Optional["TestOptions"]) -> "TestOptions":
        """
        Create a new TestOptions with values from override taking precedence.

        Args:
            override: TestOptions that should override self's values

        Returns:
            New TestOptions with merged values
        """
        if override is None:
            # Create a copy of self
            return TestOptions(
                deployment_type=self.deployment_type,
                size=self.size,
                priority=self.priority,
                parallelity=self.parallelity,
                buckets=self.buckets,
                replication_version=self.replication_version,
                suffix=self.suffix,
                full=self.full,
                coverage=self.coverage,
                time_limit=self.time_limit,
                architecture=self.architecture,
            )

        # Helper to merge a single field
        def merge_field(override_val, self_val):
            return override_val if override_val is not None else self_val

        return TestOptions(
            deployment_type=merge_field(override.deployment_type, self.deployment_type),
            size=merge_field(override.size, self.size),
            priority=merge_field(override.priority, self.priority),
            parallelity=merge_field(override.parallelity, self.parallelity),
            buckets=merge_field(override.buckets, self.buckets),
            replication_version=merge_field(
                override.replication_version, self.replication_version
            ),
            suffix=merge_field(override.suffix, self.suffix),
            full=merge_field(override.full, self.full),
            coverage=merge_field(override.coverage, self.coverage),
            time_limit=merge_field(override.time_limit, self.time_limit),
            architecture=merge_field(override.architecture, self.architecture),
        )


@dataclass
class TestArguments:
    """Additional command-line arguments for test execution."""

    extra_args: Dict[str, Any] = field(default_factory=dict)
    arangosh_args: List[str] = field(default_factory=list)

    @staticmethod
    def parse_args_string(args_str: str, add_skip_server_js: bool = False) -> List[str]:
        """
        Parse extra arguments string into list.

        This handles the legacy format used by the old generator where arguments
        can be space-separated strings, with "A" representing an empty argument list.
        The legacy format may have a leading character (space or "A") that needs to be stripped.

        Args:
            args_str: Space-separated argument string, or "A" for empty
            add_skip_server_js: Whether to add --skipServerJS flag

        Returns:
            List of argument strings
        """
        if not args_str or args_str == "A":
            args = []
        else:
            # Remove leading character if present (legacy format: " arg1 arg2" or "Aarg1 arg2")
            if args_str[0] in [" ", "A"]:
                args_str = args_str[1:]
            args = args_str.split(" ") if args_str else []

        if add_skip_server_js:
            args.extend(["--skipServerJS", "true"])

        return args

    @classmethod
    def from_dict(cls, data: Optional[Dict[str, Any]]) -> "TestArguments":
        """Create TestArguments from a dictionary."""
        if data is None:
            return cls()

        # Data can be:
        # 1. A dict with extraArgs:key format (from YAML args section)
        # 2. A dict with 'extra_args' dict and 'arangosh_args' list
        # 3. A dict with nested 'args' key

        extra_args = {}
        arangosh_args = []

        # Check if we have direct 'extra_args' field
        if "extra_args" in data:
            extra_args = data["extra_args"]
        # Otherwise parse the dict directly for argument patterns
        else:
            for key, value in data.items():
                if key == "arangosh_args":
                    # Handle arangosh_args separately
                    continue
                if key == "moreArgv":
                    # Special case: moreArgv value is stored as-is
                    extra_args["moreArgv"] = value
                elif key.startswith("extraArgs:"):
                    # Handle "extraArgs:log.level: replication=trace" format
                    # Keep the full key including "extraArgs:" prefix
                    extra_args[key] = value
                else:
                    # Regular key-value arguments
                    extra_args[key] = value

        # Handle arangosh_args (convert dict to list)
        if "arangosh_args" in data:
            arangosh_data = data["arangosh_args"]
            if isinstance(arangosh_data, dict):
                # Convert dict to list format
                for key, value in arangosh_data.items():
                    arangosh_args.append(f"--{key}")
                    if isinstance(value, bool):
                        arangosh_args.append("true" if value else "false")
                    else:
                        arangosh_args.append(str(value))
            elif isinstance(arangosh_data, list):
                arangosh_args = list(arangosh_data)

        return cls(
            extra_args=dict(extra_args) if extra_args else {},
            arangosh_args=list(arangosh_args) if arangosh_args else [],
        )

    def merge_with(self, override: Optional["TestArguments"]) -> "TestArguments":
        """
        Create a new TestArguments with combined arguments.

        Args:
            override: TestArguments to merge with self's arguments (override wins for dict, appends for list)

        Returns:
            New TestArguments with merged args
        """
        if override is None:
            return TestArguments(
                extra_args=dict(self.extra_args), arangosh_args=list(self.arangosh_args)
            )

        return TestArguments(
            extra_args={**self.extra_args, **override.extra_args},
            arangosh_args=self.arangosh_args + override.arangosh_args,
        )


@dataclass
class SuiteConfig:
    """
    Configuration for a single test suite within a job.

    For single-suite jobs, this contains the suite name and any overrides.
    For multi-suite jobs, this represents one suite in the list.
    """

    name: str
    options: TestOptions = field(default_factory=TestOptions)
    arguments: TestArguments = field(default_factory=TestArguments)

    def __post_init__(self):
        """Validate suite configuration."""
        if not self.name or not isinstance(self.name, str):
            raise ValueError(f"Suite name must be a non-empty string, got: {self.name}")

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "SuiteConfig":
        """Create SuiteConfig from a dictionary."""
        if not isinstance(data, dict):
            raise ValueError(f"SuiteConfig data must be a dict, got: {type(data)}")

        if "name" not in data:
            raise ValueError("SuiteConfig must have 'name' field")

        return cls(
            name=data["name"],
            options=TestOptions.from_dict(data.get("options")),
            arguments=TestArguments.from_dict(data.get("arguments")),
        )

    def with_merged_options(
        self, job_options: TestOptions, job_arguments: TestArguments
    ) -> "SuiteConfig":
        """
        Create a new SuiteConfig with job-level options as defaults.

        Args:
            job_options: TestJob-level options to use as defaults
            job_arguments: TestJob-level arguments to prepend

        Returns:
            New SuiteConfig with merged options
        """
        return SuiteConfig(
            name=self.name,
            options=job_options.merge_with(self.options),
            arguments=job_arguments.merge_with(self.arguments),
        )


@dataclass
class RepositoryConfig:
    """Git repository configuration for driver tests."""

    git_repo: str
    git_branch: Optional[str] = None
    init_command: Optional[str] = None
    container_suffix: Optional[str] = None  # Docker image suffix (e.g., '-js:')

    def __post_init__(self):
        """Validate repository configuration."""
        if not self.git_repo or not isinstance(self.git_repo, str):
            raise ValueError(
                f"git_repo must be a non-empty string, got: {self.git_repo}"
            )

    @classmethod
    def from_dict(cls, data: Optional[Dict[str, Any]]) -> Optional["RepositoryConfig"]:
        """Create RepositoryConfig from a dictionary."""
        if data is None:
            return None

        if "git_repo" not in data:
            raise ValueError("RepositoryConfig must have 'git_repo' field")

        return cls(
            git_repo=data["git_repo"],
            git_branch=data.get("git_branch"),
            init_command=data.get("init_command"),
            container_suffix=data.get("container_suffix"),
        )


@dataclass
class TestJob:
    """
    Complete specification for a test job.

    This is a unified representation that handles both single-suite and
    multi-suite jobs. Every job has a list of suites (even if length 1).
    """

    name: str
    suites: List[SuiteConfig]
    options: TestOptions = field(default_factory=TestOptions)
    arguments: TestArguments = field(default_factory=TestArguments)
    repository: Optional[RepositoryConfig] = None
    job_type: str = "run-linux-tests"  # CircleCI job template to use

    def __post_init__(self):
        """Validate test job configuration."""
        if not self.name or not isinstance(self.name, str):
            raise ValueError(
                f"TestJob name must be a non-empty string, got: {self.name}"
            )

        if not self.suites:
            raise ValueError(f"TestJob '{self.name}' must have at least one suite")

        # Validate deployment type constraints
        self._validate_deployment_type_constraints()

        # Validate buckets constraints
        self._validate_buckets_constraints()

    def _validate_deployment_type_constraints(self):
        """Validate deployment type usage rules."""
        job_type = self.options.deployment_type

        if job_type == DeploymentType.MIXED:
            # MIXED is only allowed for multi-suite jobs
            if len(self.suites) == 1:
                raise ValueError(
                    f"TestJob '{self.name}': deployment_type 'mixed' is only valid for multi-suite jobs"
                )
        elif job_type in (DeploymentType.SINGLE, DeploymentType.CLUSTER):
            # Single/Cluster jobs cannot have suite-level deployment_type overrides
            for suite in self.suites:
                if suite.options.deployment_type is not None:
                    raise ValueError(
                        f"TestJob '{self.name}', suite '{suite.name}': Cannot override deployment_type "
                        f"when job deployment_type is {job_type.value}"
                    )

        # For MIXED jobs, ensure all suites have a deployment type (either explicit or None to inherit)
        # NOTE: In practice, MIXED jobs may infer deployment type from args (e.g., cluster: true)
        # rather than explicit options. We allow None for now and leave inference to output generators.
        if job_type == DeploymentType.MIXED:
            for suite in self.suites:
                if suite.options.deployment_type == DeploymentType.MIXED:
                    raise ValueError(
                        f"TestJob '{self.name}', suite '{suite.name}': Suite deployment_type cannot be 'mixed'"
                    )

    def _validate_buckets_constraints(self):
        """Validate buckets usage rules."""
        job_buckets = self.options.buckets

        # Multi-suite jobs with explicit buckets must use "auto"
        if len(self.suites) > 1 and job_buckets is not None:
            if job_buckets != "auto":
                raise ValueError(
                    f"TestJob '{self.name}': Multi-suite jobs with buckets must use buckets='auto', "
                    f"got: {job_buckets}"
                )

        # Single-suite jobs cannot use "auto"
        if len(self.suites) == 1:
            if job_buckets == "auto":
                raise ValueError(
                    f"TestJob '{self.name}': Single-suite jobs cannot use buckets='auto'"
                )
            for suite in self.suites:
                if suite.options.buckets == "auto":
                    raise ValueError(
                        f"TestJob '{self.name}', suite '{suite.name}': Cannot use buckets='auto' "
                        f"in single-suite job"
                    )

    @classmethod
    def from_dict(cls, name: str, data: Dict[str, Any]) -> "TestJob":
        """
        Create TestJob from a dictionary.

        Handles both single-suite jobs (with 'suite' field) and multi-suite
        jobs (with 'suites' field).

        Args:
            name: TestJob name
            data: Dictionary from YAML

        Returns:
            TestJob instance
        """
        if not isinstance(data, dict):
            raise ValueError(f"TestJob '{name}' data must be a dict, got: {type(data)}")

        # Parse suites - handle both 'suite' (single) and 'suites' (multi)
        # If neither is specified, use the job name as the suite name
        suites = []
        if "suite" in data:
            # Single-suite job with explicit suite name
            suite_name = data["suite"]
            if not isinstance(suite_name, str):
                raise ValueError(
                    f"TestJob '{name}': 'suite' must be a string, got: {type(suite_name)}"
                )
            suites = [SuiteConfig(name=suite_name)]
        elif "suites" in data:
            # Multi-suite job
            suite_list = data["suites"]
            if not isinstance(suite_list, list):
                raise ValueError(
                    f"TestJob '{name}': 'suites' must be a list, got: {type(suite_list)}"
                )

            for suite_data in suite_list:
                if isinstance(suite_data, str):
                    # Simple string form
                    suites.append(SuiteConfig(name=suite_data))
                elif isinstance(suite_data, dict):
                    # Dict form - check if suite name is a key or a field
                    if "name" in suite_data:
                        # Format: {name: 'suite_name', options: {...}, ...}
                        suites.append(SuiteConfig.from_dict(suite_data))
                    elif len(suite_data) == 1:
                        # Format: {suite_name: {options: {...}, args: {...}}}
                        suite_name, suite_config = next(iter(suite_data.items()))
                        # Merge the name into the config
                        full_config = {"name": suite_name}
                        if isinstance(suite_config, dict):
                            # Convert 'args' to 'arguments' if present
                            for key, value in suite_config.items():
                                if key == "args":
                                    full_config["arguments"] = value
                                else:
                                    full_config[key] = value
                        suites.append(SuiteConfig.from_dict(full_config))
                    else:
                        raise ValueError(
                            f"TestJob '{name}': Dict suite must have 'name' field or be single-key dict, got: {suite_data}"
                        )
                else:
                    raise ValueError(
                        f"TestJob '{name}': Suite must be string or dict, got: {type(suite_data)}"
                    )
        else:
            # No suite/suites specified - use job name as suite name
            suites = [SuiteConfig(name=name)]

        # Parse arguments - handle both 'args'/'arguments' dict and top-level fields
        args_data = data.get("args") or data.get("arguments") or {}

        # If arangosh_args exists at job level (not in args dict), merge it in
        if "arangosh_args" in data and "arangosh_args" not in args_data:
            args_data = {**args_data, "arangosh_args": data["arangosh_args"]}

        # Get job type (defaults to 'run-linux-tests')
        job_type = data.get("job", "run-linux-tests")

        return cls(
            name=name,
            suites=suites,
            options=TestOptions.from_dict(data.get("options")),
            arguments=TestArguments.from_dict(args_data),
            repository=RepositoryConfig.from_dict(data.get("repository")),
            job_type=job_type,
        )

    def is_multi_suite(self) -> bool:
        """Check if this is a multi-suite job."""
        return len(self.suites) > 1

    def get_resolved_suites(self) -> List[SuiteConfig]:
        """
        Get suites with job-level options merged in.

        Returns:
            List of SuiteConfig with merged options
        """
        return [
            suite.with_merged_options(self.options, self.arguments)
            for suite in self.suites
        ]

    def get_bucket_count(self) -> Optional[int]:
        """
        Calculate the number of buckets for this job.

        Returns:
            Number of buckets, or None if not using buckets
        """
        if self.options.buckets is None:
            return None

        if self.options.buckets == "auto":
            return len(self.suites)

        # At this point we know buckets is an int (not None, not "auto")
        assert isinstance(self.options.buckets, int)
        return self.options.buckets


@dataclass
class TestDefinitionFile:
    """Complete representation of a test definition YAML file."""

    jobs: Dict[str, TestJob]

    def __post_init__(self):
        """Validate test definition file."""
        if not self.jobs:
            raise ValueError("Test definition file must contain at least one job")

    @classmethod
    def from_yaml_file(cls, filepath: str) -> "TestDefinitionFile":
        """
        Load and parse a test definition YAML file.

        Args:
            filepath: Path to YAML file

        Returns:
            TestDefinitionFile instance
        """
        with open(filepath, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f)

        return cls.from_dict(data)

    @staticmethod
    def _generate_unique_key(job: TestJob, existing_keys) -> str:
        """
        Generate a unique key for a job based on its properties.

        Jobs with the same name but different deployment types or suffixes
        get unique keys. If conflicts still occur, append a counter.

        Args:
            job: TestJob to generate key for
            existing_keys: Set of already-used keys

        Returns:
            Unique key string
        """
        # Start with job name
        unique_key = job.name

        # Add deployment type if specified
        if job.options.deployment_type:
            unique_key = f"{job.name}_{job.options.deployment_type.value}"

        # Add suffix if specified
        if job.options.suffix:
            unique_key = f"{unique_key}_{job.options.suffix}"

        # If still conflicts, append counter
        if unique_key in existing_keys:
            counter = 2
            while f"{unique_key}_{counter}" in existing_keys:
                counter += 1
            unique_key = f"{unique_key}_{counter}"

        return unique_key

    @classmethod
    def from_dict(
        cls, data: Union[Dict[str, Any], List[Dict[str, Any]]]
    ) -> "TestDefinitionFile":
        """
        Create TestDefinitionFile from a dictionary or list.

        Handles two YAML formats:
        1. Flat list of jobs: [{'job_name': job_data}, ...]
        2. Driver test format: {jobProperties: {...}, tests: [...]}

        Args:
            data: Dictionary or list loaded from YAML

        Returns:
            TestDefinitionFile instance
        """
        jobs: Dict[str, TestJob] = {}
        repo_config = None

        # Check if this is the driver test format with 'jobProperties' and 'tests'
        if isinstance(data, dict) and "tests" in data:
            # Extract jobProperties for repository configuration
            job_properties = data.get("jobProperties", {})

            # Convert jobProperties to repository config if present
            if job_properties and "second_repo" in job_properties:
                repo_config = {
                    "git_repo": job_properties["second_repo"],
                    "git_branch": job_properties.get("branch"),
                    "init_command": job_properties.get("init_command"),
                    "container_suffix": job_properties.get("container_suffix"),
                }

            # Parse the tests list
            tests_data = data["tests"]
            if not isinstance(tests_data, list):
                raise ValueError(
                    f"'tests' key must contain a list, got: {type(tests_data)}"
                )

            for item in tests_data:
                if not isinstance(item, dict):
                    raise ValueError(
                        f"Each item in 'tests' list must be a dict, got: {type(item)}"
                    )
                if len(item) != 1:
                    raise ValueError(
                        f"Each item in 'tests' list must have exactly one key-value pair, got: {len(item)}"
                    )

                job_name, job_data = next(iter(item.items()))

                # Add repository config to job data if not already present
                if repo_config:
                    if not isinstance(job_data, dict):
                        job_data = {}
                    if "repository" not in job_data:
                        job_data["repository"] = repo_config

                # Parse job and generate unique key
                job = TestJob.from_dict(job_name, job_data)
                unique_key = cls._generate_unique_key(job, jobs.keys())
                jobs[unique_key] = job

        elif isinstance(data, list):
            # Handle flat list format: [{'job_name': job_data}, ...]
            for item in data:
                if not isinstance(item, dict):
                    raise ValueError(
                        f"Each item in test definition list must be a dict, got: {type(item)}"
                    )
                if len(item) != 1:
                    raise ValueError(
                        f"Each item in test definition list must have exactly one key-value pair, got: {len(item)}"
                    )

                job_name, job_data = next(iter(item.items()))

                # Parse job and generate unique key
                job = TestJob.from_dict(job_name, job_data)
                unique_key = cls._generate_unique_key(job, jobs.keys())
                jobs[unique_key] = job

        elif isinstance(data, dict):
            # Handle dict format: {'job_name': job_data, ...}
            # Skip special metadata keys
            for job_name, job_data in data.items():
                if job_name in ("jobProperties", "add-yaml"):
                    # Skip metadata keys (add-yaml is ignored per requirements)
                    continue
                jobs[job_name] = TestJob.from_dict(job_name, job_data)

        else:
            raise ValueError(
                f"Test definition data must be a dict or list, got: {type(data)}"
            )

        return cls(jobs=jobs)

    def get_job_names(self) -> List[str]:
        """Get sorted list of all job names."""
        return sorted(self.jobs.keys())

    def filter_jobs(self, **criteria) -> Dict[str, TestJob]:
        """
        Filter jobs based on criteria.

        Supported criteria:
        - deployment_type: Filter by deployment type
        - size: Filter by resource size
        - has_repository: Filter jobs with/without repository config
        - suite_name: Filter jobs containing specific suite

        Returns:
            Dict of job_name -> TestJob matching criteria
        """
        filtered = {}

        for job_name, job in self.jobs.items():
            match = True

            # Filter by deployment type
            if "deployment_type" in criteria:
                expected = criteria["deployment_type"]
                if job.options.deployment_type != expected:
                    match = False

            # Filter by size
            if "size" in criteria:
                expected = criteria["size"]
                if job.options.size != expected:
                    match = False

            # Filter by repository presence
            if "has_repository" in criteria:
                expected = criteria["has_repository"]
                has_repo = job.repository is not None
                if has_repo != expected:
                    match = False

            # Filter by suite name
            if "suite_name" in criteria:
                expected = criteria["suite_name"]
                suite_names = [s.name for s in job.suites]
                if expected not in suite_names:
                    match = False

            if match:
                filtered[job_name] = job

        return filtered


@dataclass
class BuildConfig:
    """Build context information for test execution (enterprise-only)."""

    architecture: Architecture
    sanitizer: Optional[Sanitizer] = None
    nightly: bool = False
