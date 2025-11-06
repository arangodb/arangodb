# Implementation Plan: Rewrite Test Configuration Scripts

## Overview

This plan details the complete rewrite of `.circleci/generate_config.py` and `/home/mpoeter/dev/arangodb/oskar/jenkins/helper/test_launch_controller.py`. Both scripts read the same YAML test definition files but produce different outputs. The rewrite will create a clean, maintainable, and well-tested architecture with shared generic code that can be duplicated across both repositories.

## Types

### Core Data Models

All data models use Python dataclasses for type safety and immutability where appropriate.

#### `DeploymentType` (Enum)
```python
class DeploymentType(Enum):
    SINGLE = "single"
    CLUSTER = "cluster"
    MIXED = "mixed"
```

#### `ResourceSize` (Enum)
```python
class ResourceSize(Enum):
    SMALL = "small"
    MEDIUM = "medium"
    MEDIUM_PLUS = "medium+"
    LARGE = "large"
    XLARGE = "xlarge"
    XXLARGE = "2xlarge"
```

#### `TestOptions` (dataclass)
Represents test execution options at both job and suite level:
- `deployment_type: Optional[DeploymentType]`
- `size: Optional[ResourceSize]`
- `priority: Optional[int]`
- `parallelity: Optional[int]`
- `buckets: Optional[Union[int, str]]` (int, "auto", or None)
- `suffix: Optional[str]`
- `full: Optional[bool]`
- `coverage: Optional[bool]`
- `sniff: Optional[bool]`

Methods:
- `merge_with(parent: TestOptions) -> TestOptions` - Merge with parent, self takes precedence
- `validate_for_job(is_multi_suite: bool) -> None` - Validate options in job context
- `get_parallelity(is_cluster: bool) -> int` - Get parallelity with defaults
- `from_dict(data: Dict) -> TestOptions` (classmethod) - Parse from YAML dict

#### `TestArguments` (dataclass)
Represents arguments passed to test execution:
- `extra_args: Dict[str, Any]` - Arguments to testing.js
- `arangosh_args: Dict[str, Any]` - Arguments to arangosh

Methods:
- `merge_with(parent: TestArguments) -> TestArguments`
- `to_command_line_args() -> List[str]` - Convert extra_args to CLI format
- `to_arangosh_args() -> List[str]` - Convert arangosh_args to CLI format
- `from_dict(args_dict, arangosh_dict) -> TestArguments` (classmethod)

#### `SuiteConfig` (dataclass)
Configuration for a single suite within a job:
- `name: str`
- `options: TestOptions`
- `arguments: TestArguments`

Methods:
- `get_effective_name() -> str` - Name with optional suffix
- `merge_with_job(job_options, job_arguments) -> tuple[TestOptions, TestArguments]`
- `from_yaml(suite_data: Union[str, Dict]) -> SuiteConfig` (classmethod)

#### `RepositoryConfig` (dataclass)
Configuration for external repository checkout (driver tests):
- `git_repo: str`
- `git_branch: str`
- `init_command: str`
- `container_suffix: str`

Methods:
- `has_external_repo: bool` (property)
- `from_dict(data: Dict) -> RepositoryConfig` (classmethod)

#### `TestJob` (dataclass)
Unified representation for both single and multi-suite jobs:
- `name: str`
- `suites: List[SuiteConfig]`
- `options: TestOptions`
- `arguments: TestArguments`
- `repository_config: RepositoryConfig`
- `run_job_type: str = "run-linux-tests"`
- `prefix: str = ""` (runtime property)

Methods:
- `validate() -> None` - Validate entire job configuration
- `get_effective_buckets() -> int` - Handle auto-buckets
- `get_options_json() -> Optional[str]` - Generate optionsJson for multi-suite
- `should_run_suite(suite, cli_full, is_coverage) -> bool`
- `get_filtered_suites(cli_full, is_coverage) -> List[SuiteConfig]`
- `from_yaml(job_name, job_data, repo_config) -> TestJob` (classmethod)

#### `TestDefinitionFile` (dataclass)
Represents a complete test definition YAML file:
- `jobs: List[TestJob]`
- `repository_config: RepositoryConfig`
- `add_yaml_config: Dict[str, Any]` (ignored per requirements)

Methods:
- `from_yaml_file(filepath, override_branch) -> TestDefinitionFile` (classmethod)

#### `BuildConfig` (dataclass, was namedtuple)
Build configuration context:
- `arch: str` ("x64" or "aarch64")
- `enterprise: bool`
- `sanitizer: str` ("", "tsan", or "alubsan")
- `is_nightly: bool`

## Files

### New Files to Create

#### `.circleci/test_config_lib.py` (Shared Generic Library)
**Purpose**: Generic, reusable code for parsing and processing test definitions.

**Contents**:
- All data model classes (TestOptions, TestArguments, SuiteConfig, TestJob, etc.)
- YAML parsing functions
- Validation logic
- Filtering functions
- Utility functions for merging options/arguments

**Key Functions**:
- `parse_test_definition_file(filepath: str, override_branch: Optional[str]) -> TestDefinitionFile`
- `filter_jobs(jobs: List[TestJob], cli_args, environment_flags) -> List[TestJob]`
- `validate_test_job(job: TestJob) -> None`

This file will be duplicated (identical copies) in both repositories.

#### `.circleci/generate_config_new.py` (CircleCI Config Generator)
**Purpose**: Generate CircleCI YAML configuration from test definitions.

**Contents**:
- Import from `test_config_lib`
- CircleCI-specific output generation
- Workflow creation
- Job creation for different architectures/editions
- Build job creation
- Docker image creation

**Key Functions**:
- `create_circleci_test_job(job: TestJob, deployment_variant: DeploymentType, build_config: BuildConfig, ...) -> Dict`
- `add_jobs_to_workflow(workflow: Dict, jobs: List[TestJob], build_config: BuildConfig, ...)`
- `generate_workflows(base_config: Dict, test_jobs: List[TestJob], cli_args) -> Dict`
- `main()`

#### `/home/mpoeter/dev/arangodb/oskar/jenkins/helper/test_config_lib.py` (Duplicate)
**Purpose**: Exact duplicate of `.circleci/test_config_lib.py` for the oskar repository.

#### `/home/mpoeter/dev/arangodb/oskar/jenkins/helper/test_launch_controller_new.py` (Test Launcher)
**Purpose**: Launch or dump test definitions for Jenkins.

**Contents**:
- Import from `test_config_lib`
- Test filtering for Jenkins environment
- Output formatting (dump format)
- Test execution orchestration (launch format)

**Key Functions**:
- `filter_tests_for_jenkins(jobs: List[TestJob], cli_args) -> List[TestJob]`
- `generate_dump_output(jobs: List[TestJob])`
- `launch_tests(jobs: List[TestJob], site_config: SiteConfig)`
- `main()`

### Existing Files to Modify

#### `.circleci/generate_config.py`
- Will be replaced by `generate_config_new.py` after testing
- Keep temporarily for comparison

#### `/home/mpoeter/dev/arangodb/oskar/jenkins/helper/test_launch_controller.py`
- Will be replaced by `test_launch_controller_new.py` after testing
- Keep temporarily for comparison

#### `/home/mpoeter/dev/arangodb/oskar/jenkins/helper/dump_handler.py`
- Modify to work with new `TestJob` data model
- Simplify since data is now structured

#### `/home/mpoeter/dev/arangodb/oskar/jenkins/helper/launch_handler.py`
- Modify to work with new `TestJob` data model
- May need adjustment to TestingRunner integration

### Files to Delete (Eventually)

After successful migration and testing:
- Old versions of `generate_config.py` and `test_launch_controller.py`

## Functions

### Parsing Layer (`test_config_lib.py`)

#### `parse_test_definition_file(filepath: str, override_branch: Optional[str] = None) -> TestDefinitionFile`
Parse complete YAML file into structured data model.

#### `parse_test_job(job_name: str, job_data: Dict, repo_config: RepositoryConfig) -> TestJob`
Parse single test job from YAML.

#### `parse_suite_config(suite_data: Union[str, Dict]) -> SuiteConfig`
Parse suite configuration (handles both string and dict forms).

#### `parse_test_options(options_dict: Dict) -> TestOptions`
Parse options dictionary into TestOptions dataclass.

#### `parse_test_arguments(args_dict: Dict, arangosh_dict: Dict) -> TestArguments`
Parse arguments into TestArguments dataclass.

### Validation Layer (`test_config_lib.py`)

#### `validate_test_job(job: TestJob) -> None`
Validate entire job:
- Bucket rules (auto vs int)
- Deployment type consistency
- Suite option overrides

#### `validate_test_options(options: TestOptions, is_multi_suite: bool) -> None`
Validate options in context.

#### `validate_buckets(buckets: Union[int, str, None], is_multi_suite: bool) -> None`
Validate bucket configuration.

#### `validate_deployment_types(job: TestJob) -> None`
Validate deployment type consistency between job and suites.

### Filtering Layer (`test_config_lib.py`)

#### `filter_jobs_by_deployment(jobs: List[TestJob], cluster: bool, single: bool) -> List[TestJob]`
Filter jobs by deployment type.

#### `filter_jobs_by_full(jobs: List[TestJob], is_full: bool) -> List[TestJob]`
Filter jobs based on full flag.

#### `filter_jobs_by_environment(jobs: List[TestJob], is_windows: bool, is_mac: bool, is_arm: bool, is_coverage: bool) -> List[TestJob]`
Filter jobs based on environment exclusions.

#### `filter_suites_within_job(job: TestJob, cli_args) -> TestJob`
Filter individual suites within a job based on their options.

### CircleCI Generation Layer (`generate_config_new.py`)

#### `create_circleci_test_job(job: TestJob, deployment_type: DeploymentType, build_config: BuildConfig, build_jobs: List[str], cli_args) -> Dict`
Create a single CircleCI test job definition.

#### `get_test_resource_size(size: ResourceSize, build_config: BuildConfig, is_cluster: bool) -> str`
Determine actual resource size considering sanitizers and architecture.

#### `add_workflow(workflows: Dict, test_jobs: List[TestJob], build_config: BuildConfig, cli_args) -> Dict`
Add complete workflow for one architecture/edition.

#### `add_build_jobs(workflow: Dict, build_config: BuildConfig) -> List[str]`
Add compilation jobs to workflow.

#### `add_rta_ui_test_jobs(workflow: Dict, build_config: BuildConfig, build_jobs: List[str], cli_args)`
Add RTA UI test jobs.

#### `generate_circleci_config(base_config_path: str, test_definition_files: List[str], cli_args) -> Dict`
Main function to generate complete CircleCI config.

### Jenkins/Oskar Layer (`test_launch_controller_new.py`)

#### `filter_tests_for_jenkins(jobs: List[TestJob], cli_args) -> List[TestJob]`
Apply Jenkins-specific filtering.

#### `add_sg_cl_prefixes(jobs: List[TestJob], single_cluster: bool) -> List[TestJob]`
Add sg_/cl_ prefixes when running both single and cluster.

#### `generate_dump_output(jobs: List[TestJob]) -> None`
Print test definitions in text format.

#### `prepare_jobs_for_launch(jobs: List[TestJob], site_config: SiteConfig) -> List[Dict]`
Convert TestJob objects to format expected by TestingRunner.

## Dependencies

### Python Standard Library
- `dataclasses` - Data models
- `enum` - Enumerations
- `typing` - Type hints
- `argparse` - CLI argument parsing
- `json` - JSON handling
- `yaml` (PyYAML) - YAML parsing
- `copy` - Deep copying
- `sys`, `os` - System operations
- `traceback` - Error handling

### External Dependencies
- `PyYAML` - Already used in current code

### Internal Dependencies (for oskar repository)
- `site_config` - Environment configuration
- `dump_handler` - Dump output formatting  
- `launch_handler` - Test launching
- `testing_runner` - Test execution

## Testing

### Unit Tests

Create `tests/test_config_lib_test.py`:

**Test Coverage**:
1. **Parsing Tests**:
   - Parse simple job (single suite)
   - Parse multi-suite job
   - Parse job with auto-buckets
   - Parse job with suite-level options
   - Parse job with repository config
   - Parse job with mixed deployment type
   - Handle malformed YAML gracefully

2. **Validation Tests**:
   - Reject multi-suite job with numeric buckets
   - Reject single-suite job with auto-buckets
   - Reject type override in single/cluster jobs
   - Reject mixed type in single-suite job
   - Accept valid configurations

3. **Merging Tests**:
   - Merge suite options with job options
   - Merge suite arguments with job arguments
   - Verify precedence (suite > job)

4. **Filtering Tests**:
   - Filter by deployment type
   - Filter by full flag
   - Filter by environment flags
   - Filter suites within jobs

5. **Utility Tests**:
   - Get effective buckets (auto → suite count)
   - Generate optionsJson correctly
   - Convert arguments to command line

### Integration Tests

Create `tests/test_generate_config_integration.py`:

**Test Coverage**:
1. Load actual test-definitions.yml
2. Generate CircleCI config
3. Validate output structure
4. Compare with known-good output (regression)

Create `tests/test_launch_controller_integration.py`:

**Test Coverage**:
1. Load actual test-definitions.yml
2. Filter for Jenkins environment
3. Generate dump output
4. Validate format

### Comparison Testing

Create temporary scripts to:
1. Run old generate_config.py
2. Run new generate_config_new.py
3. Diff the outputs
4. Identify and document differences
5. Verify differences are expected improvements

## Implementation Order

### Phase 1: Core Data Models (test_config_lib.py)
1. Create `test_config_lib.py` with all dataclasses
2. Implement `__post_init__` validation
3. Implement `from_dict` and `from_yaml` classmethods
4. Implement `merge_with` methods
5. Write unit tests for data models

**Validation**: All data model unit tests pass

### Phase 2: Parsing Layer
1. Implement `TestDefinitionFile.from_yaml_file()`
2. Implement YAML parsing for all structures
3. Handle edge cases (simple vs multi-suite, string vs dict)
4. Write parsing unit tests

**Validation**: Can parse all existing test definition files without errors

### Phase 3: Validation Layer
1. Implement all validation functions
2. Add comprehensive validation tests
3. Test with malformed configurations

**Validation**: All validation tests pass, malformed configs are rejected

### Phase 4: Filtering Layer
1. Implement filtering functions
2. Test filtering logic
3. Verify filter combinations work correctly

**Validation**: Filtering tests pass

### Phase 5: CircleCI Generator (generate_config_new.py)
1. Create `generate_config_new.py`
2. Implement job creation functions
3. Implement workflow creation
4. Implement main entry point
5. Test with actual base_config.yml

**Validation**: Generate valid CircleCI config that matches old output (with documented differences)

### Phase 6: Jenkins/Oskar Controller (test_launch_controller_new.py)
1. Duplicate `test_config_lib.py` to oskar repository
2. Create `test_launch_controller_new.py`
3. Implement Jenkins-specific filtering
4. Adapt dump_handler.py and launch_handler.py
5. Test with actual test definitions

**Validation**: Can launch tests successfully, dump output is correct

### Phase 7: Integration and Migration
1. Run both old and new scripts side-by-side
2. Compare outputs and validate differences
3. Update CI/CD pipelines to use new scripts
4. Monitor for issues
5. Remove old scripts after confirmation

**Validation**: New scripts work in production, no regressions

### Phase 8: Documentation and Cleanup
1. Document new architecture in README
2. Add inline documentation
3. Create migration guide
4. Clean up old code
5. Archive old scripts for reference

**Validation**: Documentation is complete and accurate

## Quality Standards

### Code Quality
- Use type hints throughout
- Follow PEP 8 style guidelines
- Maximum line length: 100 characters
- Use docstrings for all public functions and classes
- Keep functions focused and small (< 50 lines where possible)

### Testing Standards
- Aim for >80% code coverage
- Test both success and failure paths
- Use descriptive test names
- Include edge case tests
- Mock external dependencies in unit tests

### Design Principles
- Single Responsibility: Each class/function has one clear purpose
- Open/Closed: Easy to extend without modifying existing code
- Dependency Inversion: Depend on abstractions (dataclasses) not implementations
- Explicit over implicit: Clear, readable code over clever tricks
- Type safety: Leverage Python's type system

### Documentation Standards
- Every module has a module-level docstring
- Every class has a docstring explaining its purpose
- Every public method has a docstring with parameters and return value
- Complex logic has inline comments explaining "why" not "what"

## Notes and Considerations

### Special Cases to Handle

1. **Multiplier notation**: `buckets: "*2"` multiplies a default value
   - Currently only used for legacy CI compatibility
   - Document but implement as future enhancement

2. **Special test overrides**: Some tests have hardcoded logic
   - `replication_sync`: 5 buckets in CircleCI (vs 2 in YAML)
   - `chaos`: Special timeout in nightly
   - `shell_client_aql`: Size increase in nightly
   - Consider making these configurable in YAML instead

3. **Add-yaml feature**: Currently supported for driver tests
   - Per requirements, ignore this in rewrite
   - Document removal

4. **Resource sizing**: Depends on arch + sanitizer + cluster
   - Encapsulate in `get_test_resource_size()` function
   - Keep sizing logic in one place

5. **Replication version**: Parameter for cluster tests (v1 vs v2)
   - Add to TestJob as optional parameter if needed
   - Or handle in job creation layer

### Maintenance Strategy

1. **Keep shared code synchronized**: 
   - Use identical copies of `test_config_lib.py` in both repos
   - Consider git subtree or submodule in future
   - Document synchronization process

2. **Backward compatibility**:
   - YAML format should not change
   - Old test definitions work with new code
   - Add new features as optional

3. **Testing strategy**:
   - Run both old and new in parallel initially
   - Automated comparison of outputs
   - Gradual migration per-workflow

4. **Error handling**:
   - Validate early, fail fast
   - Provide clear error messages
   - Include file/line information in YAML errors

### Future Enhancements

1. JSON Schema for test definitions
2. Interactive test definition editor/validator
3. Visualization of test dependency graph
4. Performance metrics and timing predictions
5. Auto-generation of test definition from templates
