#!/usr/bin/env python3
"""
Normalize CircleCI configuration YAML for comparison.

This script sorts workflows and jobs within workflows to enable
meaningful diffs between configurations that are semantically identical
but have different ordering.
"""

import sys
import yaml
from pathlib import Path


def normalize_workflow_jobs(workflow):
    """
    Normalize job list in a workflow by sorting.

    Jobs can be either strings (job names) or dicts with job name as key.
    We sort them by the job name to get consistent ordering.
    """
    if "jobs" not in workflow:
        return workflow

    jobs = workflow["jobs"]

    # Sort jobs by name
    # Jobs can be either:
    # 1. String: "job-name"
    # 2. Dict: {"job-name": {params}}
    def get_job_name(job):
        if isinstance(job, str):
            return job
        elif isinstance(job, dict):
            # Dict has single key which is the job name
            return list(job.keys())[0]
        return ""

    workflow["jobs"] = sorted(jobs, key=get_job_name)
    return workflow


def normalize_config(config):
    """
    Normalize a CircleCI configuration for comparison.

    - Sorts workflows by name
    - Sorts jobs within each workflow
    """
    if "workflows" not in config:
        return config

    workflows = config["workflows"]

    # Normalize each workflow
    for workflow_name, workflow_config in workflows.items():
        normalize_workflow_jobs(workflow_config)

    # Sort workflows by name (done during YAML dump with sort_keys=True)
    return config


def main():
    if len(sys.argv) != 3:
        print("Usage: normalize_config.py <input.yml> <output.yml>", file=sys.stderr)
        sys.exit(1)

    input_file = Path(sys.argv[1])
    output_file = Path(sys.argv[2])

    # Load YAML
    with open(input_file, "r") as f:
        config = yaml.safe_load(f)

    # Normalize
    normalized = normalize_config(config)

    # Write back with sorted keys
    with open(output_file, "w") as f:
        yaml.dump(
            normalized,
            f,
            default_flow_style=False,
            sort_keys=True,
            width=120,
        )


if __name__ == "__main__":
    main()
