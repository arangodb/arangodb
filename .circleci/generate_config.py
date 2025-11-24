#!/usr/bin/env python3
"""
Generate CircleCI configuration from test definitions.

This script reads test definition YAML files and generates a complete
CircleCI configuration file with workflows for different architectures
and build configurations.
"""

import sys
import traceback
from dataclasses import replace
from typing import List
import click

from src.config_lib import TestDefinitionFile, Sanitizer, TestArguments
from src.filters import FilterCriteria
from src.output_generators.base import (
    GeneratorConfig,
    TestExecutionConfig,
    CircleCIConfig,
)
from src.output_generators.circleci import CircleCIGenerator


def parse_driver_branches(driver_branch_overrides: str) -> dict:
    """
    Parse driver-branch-overrides argument into a dictionary.

    Args:
        driver_branch_overrides: Colon-separated list like "main=branch1:driver=branch2"

    Returns:
        Dictionary mapping prefix to branch name

    Raises:
        ValueError: If the format is invalid
    """
    if not driver_branch_overrides:
        return {}

    result = {}
    try:
        for pair in driver_branch_overrides.split(":"):
            if not pair:
                continue
            name, branch = pair.split("=")
            result[name] = branch
    except ValueError as ex:
        raise ValueError(
            f"Invalid --driver-branch-overrides format: '{pair}'. "
            "Expected format: 'name=branch:name2=branch2'"
        ) from ex

    return result


def apply_branch_overrides(
    test_def: TestDefinitionFile,
    filename: str,
    branch_overrides: dict
) -> TestDefinitionFile:
    """
    Apply git branch overrides to a test definition.

    The --driver_branch_overrides feature allows overriding git branches for driver tests.
    For example: --driver_branch_overrides go=feature-branch:js=main

    When a test definition filename contains a key from branch_overrides (e.g., "go.yml"
    contains "go"), the corresponding branch overrides the repository's default branch.

    Args:
        test_def: TestDefinitionFile to apply overrides to
        filename: Filename to match against override keys
        branch_overrides: Dictionary mapping filename prefix to branch override

    Returns:
        New TestDefinitionFile with branch overrides applied (immutable)
    """
    if not branch_overrides:
        return test_def

    # Extract base filename from path for matching
    base_filename = filename.split("/")[-1] if "/" in filename else filename

    # Find matching override (only apply first match)
    override_branch = None
    for prefix, branch in branch_overrides.items():
        if prefix in base_filename:
            override_branch = branch
            break

    if not override_branch:
        return test_def  # No matching override

    # Create new jobs dict with branch overrides applied
    updated_jobs = {}
    for job_name, job in test_def.jobs.items():
        if job.repository:
            # Create new job with overridden repository branch
            updated_job = replace(
                job,
                repository=replace(job.repository, git_branch=override_branch)
            )
            updated_jobs[job_name] = updated_job
        else:
            updated_jobs[job_name] = job

    return TestDefinitionFile(jobs=updated_jobs)

def load_test_definitions(
    definition_files: List[str], test_branches: dict
) -> List[TestDefinitionFile]:
    """
    Load test definition files with optional branch overrides.

    Args:
        definition_files: List of paths to test definition YAML files
        test_branches: Dictionary mapping file prefix to branch override

    Returns:
        List of loaded TestDefinitionFile objects with branch overrides applied
    """
    test_defs = []

    for filepath in definition_files:
        # Add "tests/" prefix if path doesn't contain a directory separator
        if "/" not in filepath:
            filepath = f"tests/{filepath}"

        # Load the test definition file
        test_def = TestDefinitionFile.from_yaml_file(filepath)

        # Apply branch overrides as separate step
        test_def = apply_branch_overrides(test_def, filepath, test_branches)

        test_defs.append(test_def)

    return test_defs


def create_generator_config(
    sanitizer: str,
    default_container: str,
    ui: str,
    ui_testsuites: str,
    ui_deployments: str,
    rta_branch: str,
    arangosh_args: str,
    extra_args: str,
    arangod_without_v8: str,
    single: bool,
    cluster: bool,
    gtest: bool,
    full: bool,
    all_tests: bool,
    replication_two: bool,
    nightly: bool,
    create_docker_images: bool,
    validate_only: bool,
) -> GeneratorConfig:
    """
    Create GeneratorConfig from command line arguments.

    Args:
        All CLI parameters as individual arguments

    Returns:
        GeneratorConfig object
    """
    # Parse and validate sanitizer
    sanitizer_enum = None
    if sanitizer:
        sanitizer_enum = Sanitizer.from_string(sanitizer)

    # Create filter criteria
    filter_criteria = FilterCriteria(
        single=single,
        cluster=cluster,
        gtest=gtest,
        full=full,
        all_tests=all_tests,
        nightly=nightly,
        sanitizer=sanitizer_enum,
    )

    # Create test execution config
    arangosh_args_list = TestArguments.parse_args_string(arangosh_args or "")
    extra_args_list = TestArguments.parse_args_string(
        extra_args or "",
        add_skip_server_js=(arangod_without_v8 == "true"),
    )

    test_execution = TestExecutionConfig(
        arangosh_args=arangosh_args_list,
        extra_args=extra_args_list,
        arangod_without_v8=(arangod_without_v8 == "true"),
        replication_two=replication_two,
    )

    # Create CircleCI-specific config
    circleci_config = CircleCIConfig(
        ui=ui or "",
        ui_testsuites=ui_testsuites or "",
        ui_deployments=ui_deployments or "",
        rta_branch=rta_branch,
        create_docker_images=create_docker_images,
        default_container=default_container,
    )

    return GeneratorConfig(
        filter_criteria=filter_criteria,
        test_execution=test_execution,
        circleci=circleci_config,
        validate_only=validate_only,
    )


@click.command()
@click.argument("base_config", type=click.Path(exists=True))
@click.argument("definitions", nargs=-1, required=False)
@click.option(
    "-o",
    "--output",
    required=True,
    type=click.Path(),
    help="Output filename for generated config",
)
@click.option(
    "-s",
    "--sanitizer",
    help="Sanitizer to use (tsan, asan, ubsan, alubsan)",
)
@click.option(
    "-d",
    "--default-container",
    required=True,
    help="Default container to be used",
)
@click.option(
    "-b",
    "--driver-branch-overrides",
    help="Colon-separated list of driver=branch (e.g., 'go=feature:java=main')",
)
@click.option(
    "--ui",
    help="Whether to run UI tests (off, on, only, community)",
)
@click.option(
    "--ui-testsuites",
    help="Which UI test suites to run",
)
@click.option(
    "--ui-deployments",
    help="Which deployments to run [CL, SG, ...]",
)
@click.option(
    "--rta-branch",
    help="Branch for UI tests to check out",
)
@click.option(
    "--arangosh-args",
    help="Additional arguments to append to arangosh",
)
@click.option(
    "--extra-args",
    help="Additional arguments to append to testing.js",
)
@click.option(
    "--arangod-without-v8",
    help="Whether to run without JavaScript (true, false)",
)
@click.option(
    "--single",
    is_flag=True,
    help="Include single server tests",
)
@click.option(
    "--cluster",
    is_flag=True,
    help="Include cluster tests",
)
@click.option(
    "--gtest",
    is_flag=True,
    help="Only run gtest tests",
)
@click.option(
    "--full",
    is_flag=True,
    help="Include full test set",
)
@click.option(
    "--all",
    "all_tests",
    is_flag=True,
    help="Include all tests, ignore other filters",
)
@click.option(
    "-rt",
    "--replication-two",
    is_flag=True,
    help="Enable replication version 2 tests",
)
@click.option(
    "--nightly",
    is_flag=True,
    help="This is a nightly build",
)
@click.option(
    "--create-docker-images",
    is_flag=True,
    help="Create docker images from build results",
)
@click.option(
    "--validate-only",
    is_flag=True,
    help="Validate test definitions without generating config",
)
def main(
    base_config: str,
    definitions: tuple,
    output: str,
    sanitizer: str,
    default_container: str,
    driver_branch_overrides: str,
    ui: str,
    ui_testsuites: str,
    ui_deployments: str,
    rta_branch: str,
    arangosh_args: str,
    extra_args: str,
    arangod_without_v8: str,
    single: bool,
    cluster: bool,
    gtest: bool,
    full: bool,
    all_tests: bool,
    replication_two: bool,
    nightly: bool,
    create_docker_images: bool,
    validate_only: bool,
):
    """
    Generate CircleCI configuration from test definitions.

    BASE_CONFIG is the path to base CircleCI config YAML file.
    DEFINITIONS are test definition YAML file(s).
    """
    try:
        driver_branches_dict = parse_driver_branches(driver_branch_overrides or "")

        # Load test definitions
        definition_list = list(definitions)
        if definition_list:
            click.echo(f"Loading {len(definition_list)} test definition file(s)...")
            test_defs = load_test_definitions(definition_list, driver_branches_dict)
        else:
            test_defs = []

        # Create generator config
        config = create_generator_config(
            sanitizer=sanitizer,
            default_container=default_container,
            ui=ui,
            ui_testsuites=ui_testsuites,
            ui_deployments=ui_deployments,
            rta_branch=rta_branch,
            arangosh_args=arangosh_args,
            extra_args=extra_args,
            arangod_without_v8=arangod_without_v8,
            single=single,
            cluster=cluster,
            gtest=gtest,
            full=full,
            all_tests=all_tests,
            replication_two=replication_two,
            nightly=nightly,
            create_docker_images=create_docker_images,
            validate_only=validate_only,
        )

        # If validate-only, we're done
        if validate_only:
            click.echo("Validation successful!")
            sys.exit(0)

        # Create generator
        generator = CircleCIGenerator(config, base_config_path=base_config)

        # Generate configuration
        click.echo("Generating CircleCI configuration...")
        circleci_config = generator.generate(test_defs)

        # Write output
        click.echo(f"Writing to {output}...")
        generator.write_output(circleci_config, output)

        click.echo("Done!")
        sys.exit(0)

    except Exception as exc:
        click.echo(f"Error: {exc}", err=True)
        traceback.print_exc(file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()  # pylint: disable=no-value-for-parameter
