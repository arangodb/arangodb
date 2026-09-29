#!/usr/bin/env python3
"""Convert clang-tidy output into JUnit XML for CircleCI's test-results UI.

store_test_results reads JUnit XML, clang-tidy writes plain text, so the log has
to be translated. Findings are grouped into one testsuite per check: during the
incremental rollout in .clang-tidy the useful question is which check a batch of
findings came from.

Usage: clang-tidy-junit.py <clang-tidy-output.txt> <output.xml>
"""

import collections
import re
import sys
import xml.etree.ElementTree as ET

# A reported diagnostic. The trailing [check] lists several comma-separated
# names when a check has aliases; the first one names the suite.
DIAGNOSTIC = re.compile(
    r"^(?P<file>[^:]+):(?P<line>\d+):(?P<col>\d+): "
    r"(?P<level>warning|error): (?P<msg>.*?) \[(?P<checks>[A-Za-z][\w.,-]*)\]$"
)

CONTAINER_PREFIX = "/root/project/"


def parse(path):
    """Findings in encounter order, without duplicates."""
    seen = set()
    findings = []
    with open(path, errors="replace") as handle:
        for line in handle:
            match = DIAGNOSTIC.match(line.rstrip("\n"))
            if not match:
                continue
            name = match["file"]
            if name.startswith(CONTAINER_PREFIX):
                name = name[len(CONTAINER_PREFIX) :]
            # Every TU including a header re-reports that header's findings, so
            # the same one arrives many times. Keep the first.
            key = (name, match["line"], match["col"], match["checks"])
            if key in seen:
                continue
            seen.add(key)
            findings.append(
                {
                    "file": name,
                    "where": f"{name}:{match['line']}:{match['col']}",
                    "level": match["level"],
                    "msg": match["msg"],
                    "check": match["checks"].split(",")[0],
                }
            )
    return findings


def build(findings):
    by_check = collections.OrderedDict()
    for finding in findings:
        by_check.setdefault(finding["check"], []).append(finding)

    total = str(len(findings))
    root = ET.Element("testsuites", tests=total, failures=total)
    for check, group in by_check.items():
        count = str(len(group))
        suite = ET.SubElement(
            root, "testsuite", name=check, tests=count, failures=count
        )
        for finding in group:
            case = ET.SubElement(
                suite, "testcase", classname=finding["file"], name=finding["where"]
            )
            failure = ET.SubElement(
                case, "failure", type=finding["check"], message=finding["msg"]
            )
            failure.text = (
                f"{finding['where']}: {finding['level']}: {finding['msg']} [{check}]"
            )
    return ET.ElementTree(root)


def main(source, target):
    findings = parse(source)
    build(findings).write(target, encoding="utf-8", xml_declaration=True)
    print(f"{len(findings)} unique findings -> {target}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
