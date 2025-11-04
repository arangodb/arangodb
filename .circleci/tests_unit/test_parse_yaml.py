#!/usr/bin/env python3
"""Test script to verify YAML parsing works correctly."""

import sys
from pathlib import Path

# Add parent directory to path so we can import from src
sys.path.insert(0, str(Path(__file__).parent.parent))

from src.config_lib import TestDefinitionFile


def main():
    # Find the test-definitions.yml file relative to the script location
    script_dir = Path(__file__).parent
    yaml_file = script_dir.parent.parent / "tests" / "test-definitions.yml"

    print(f"Loading test definitions from {yaml_file}...")
    t = TestDefinitionFile.from_yaml_file(str(yaml_file))

    print(f"\n✓ Successfully parsed {len(t.jobs)} jobs")

    print("\nFirst 10 jobs:")
    for i, name in enumerate(list(t.jobs.keys())[:10], 1):
        job = t.jobs[name]
        suite_count = len(job.suites)
        job_type = (
            job.options.deployment_type.value if job.options.deployment_type else "None"
        )
        print(f"  {i:2d}. {name:30s} - {suite_count} suite(s), type={job_type}")

    # Test filtering
    print("\n\nTesting filtering:")
    cluster_jobs = t.filter_jobs(
        deployment_type=t.jobs[list(t.jobs.keys())[0]].options.deployment_type
    )
    print(f"Jobs matching first job's deployment type: {len(cluster_jobs)}")

    # Test multi-suite jobs
    multi_suite_jobs = [name for name, job in t.jobs.items() if job.is_multi_suite()]
    print(f"\nMulti-suite jobs: {len(multi_suite_jobs)}")
    if multi_suite_jobs:
        print(f"Examples: {', '.join(multi_suite_jobs[:5])}")

    print("\n✓ All parsing tests passed!")


if __name__ == "__main__":
    main()
