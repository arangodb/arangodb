#!/usr/bin/env python3
"""
Generate CircleCI configuration from test definitions.

This script reads test definition YAML files and generates a complete
CircleCI configuration file with workflows for different architectures
and build configurations.
"""

import argparse
import sys
import traceback
from pathlib import Path
from typing import List

from src.config_lib import TestDefinitionFile, BuildConfig, Sanitizer, TestArguments
from src.filters import FilterCriteria, filter_jobs
from src.output_generators.base import (
    GeneratorConfig,
    TestExecutionConfig,
    CircleCIConfig,
)
from src.output_generators.circleci import CircleCIGenerator


def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Generate CircleCI configuration from test definitions"
    )

    # Required positional arguments
    parser.add_argument(
        "base_config",
        type=str,
        help="Path to base CircleCI config YAML file",
    )
    parser.add_argument(
        "definitions",
        type=str,
        nargs="*",
        help="Test definition YAML file(s)",
    )

    # Output configuration
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        required=True,
        help="Output filename for generated config",
    )

    # Build configuration
    parser.add_argument(
        "-s",
        "--sanitizer",
        type=str,
        help="Sanitizer to use (tsan, asan, ubsan)",
    )
    parser.add_argument(
        "-d",
        "--default-container",
        type=str,
        required=True,
        help="Default container to be used",
    )

    # Repository configuration
    parser.add_argument(
        "-b",
        "--test-branches",
        type=str,
        help="Colon-separated list of test-definition-prefix=branch",
    )

    # UI test configuration
    parser.add_argument(
        "--ui",
        type=str,
        help="Whether to run UI tests [off|on|only|community]",
    )
    parser.add_argument(
        "--ui-testsuites",
        type=str,
        help="Which UI test suites to run",
    )
    parser.add_argument(
        "--ui-deployments",
        type=str,
        help="Which deployments to run [CL, SG, ...]",
    )
    parser.add_argument(
        "--rta-branch",
        type=str,
        help="Branch for UI tests to check out",
    )

    # Test execution arguments
    parser.add_argument(
        "--arangosh-args",
        type=str,
        help="Additional arguments to append to arangosh",
    )
    parser.add_argument(
        "--extra-args",
        type=str,
        help="Additional arguments to append to testing.js",
    )
    parser.add_argument(
        "--arangod-without-v8",
        type=str,
        help="Whether to run without JavaScript (true/false)",
    )

    # Test filtering
    parser.add_argument(
        "--single",
        action="store_true",
        help="Include single server tests",
    )
    parser.add_argument(
        "--cluster",
        action="store_true",
        help="Include cluster tests",
    )
    parser.add_argument(
        "--gtest",
        action="store_true",
        help="Only run gtest tests",
    )
    parser.add_argument(
        "--full",
        action="store_true",
        help="Include full test set",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Include all tests, ignore other filters",
    )

    # Build flags
    parser.add_argument(
        "-rt",
        "--replication_two",
        action="store_true",
        help="Enable replication version 2 tests",
    )
    parser.add_argument(
        "--nightly",
        action="store_true",
        help="This is a nightly build",
    )
    parser.add_argument(
        "--create-docker-images",
        action="store_true",
        help="Create docker images from build results",
    )

    # Validation
    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="Validate test definitions without generating config",
    )

    return parser.parse_args()


def parse_test_branches(test_branches_arg: str) -> dict:
    """
    Parse test-branches argument into a dictionary.

    Args:
        test_branches_arg: Colon-separated list like "main=branch1:driver=branch2"

    Returns:
        Dictionary mapping prefix to branch name

    Raises:
        ValueError: If the format is invalid
    """
    if not test_branches_arg:
        return {}

    result = {}
    try:
        for pair in test_branches_arg.split(":"):
            if not pair:
                continue
            name, branch = pair.split("=")
            result[name] = branch
    except ValueError as ex:
        raise ValueError(
            f"Invalid --test-branches format: '{pair}'. "
            "Expected format: 'name=branch:name2=branch2'"
        ) from ex

    return result


def load_test_definitions(
    definition_files: List[str], test_branches: dict
) -> List[TestDefinitionFile]:
    """
    Load test definition files with optional branch overrides.

    The --test-branches feature allows overriding git branches for driver tests.
    For example: --test-branches=go=feature-branch:js=main

    When a test definition filename contains a key from test_branches (e.g., "go.yml"
    contains "go"), the corresponding branch overrides the repository's default branch.

    Args:
        definition_files: List of paths to test definition YAML files
        test_branches: Dictionary mapping file prefix to branch override

    Returns:
        List of loaded TestDefinitionFile objects with branch overrides applied
    """
    from dataclasses import replace

    test_defs = []

    for filepath in definition_files:
        # Load the test definition file
        test_def = TestDefinitionFile.from_yaml_file(filepath)

        # Apply branch overrides if any match the filename
        if test_branches:
            # Extract filename from path for matching
            filename = filepath.split("/")[-1] if "/" in filepath else filepath

            # Check if any test_branches key matches this filename
            for prefix, branch in test_branches.items():
                if prefix in filename:
                    # Override branch for all jobs with repository config
                    for job in test_def.jobs.values():
                        if job.repository:
                            # Create new RepositoryConfig with overridden branch
                            job.repository = replace(job.repository, git_branch=branch)
                    break  # Only apply first matching prefix

        test_defs.append(test_def)

    return test_defs


def create_generator_config(args, sanitizer: Sanitizer = None) -> GeneratorConfig:
    """
    Create GeneratorConfig from command line arguments.

    Args:
        args: Parsed command line arguments
        sanitizer: Parsed sanitizer enum value

    Returns:
        GeneratorConfig object
    """
    # Create filter criteria
    filter_criteria = FilterCriteria(
        single=args.single,
        cluster=args.cluster,
        gtest=args.gtest,
        full=args.full,
        all_tests=args.all,
        nightly=args.nightly,
        sanitizer=sanitizer,
    )

    # Create test execution config
    arangosh_args = TestArguments.parse_args_string(args.arangosh_args or "")
    extra_args = TestArguments.parse_args_string(
        args.extra_args or "",
        add_skip_server_js=(args.arangod_without_v8 == "true"),
    )

    test_execution = TestExecutionConfig(
        arangosh_args=arangosh_args,
        extra_args=extra_args,
        arangod_without_v8=(args.arangod_without_v8 == "true"),
        replication_two=args.replication_two,
    )

    # Create CircleCI-specific config
    circleci_config = CircleCIConfig(
        ui=args.ui or "",
        ui_testsuites=args.ui_testsuites or "",
        ui_deployments=args.ui_deployments or "",
        rta_branch=args.rta_branch,  # Keep None as None for YAML serialization
        create_docker_images=args.create_docker_images,
        default_container=args.default_container,
    )

    return GeneratorConfig(
        filter_criteria=filter_criteria,
        test_execution=test_execution,
        circleci=circleci_config,
        validate_only=args.validate_only,
    )


def main():
    """Main entry point."""
    try:
        # Parse arguments
        args = parse_arguments()

        # Parse and validate sanitizer
        sanitizer = None
        if args.sanitizer:
            sanitizer = Sanitizer.from_string(args.sanitizer)

        # Parse test branches
        test_branches = parse_test_branches(args.test_branches or "")

        # Load test definitions
        print(f"Loading {len(args.definitions)} test definition file(s)...")
        test_defs = load_test_definitions(args.definitions, test_branches)

        # Create generator config
        config = create_generator_config(args, sanitizer)

        # If validate-only, we're done
        if args.validate_only:
            print("Validation successful!")
            return 0

        # Create generator
        generator = CircleCIGenerator(config, base_config_path=args.base_config)

        # Generate configuration
        print(f"Generating CircleCI configuration...")
        circleci_config = generator.generate(test_defs)

        # Write output
        print(f"Writing to {args.output}...")
        generator.write_output(circleci_config, args.output)

        print("Done!")
        return 0

    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        traceback.print_exc(file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
