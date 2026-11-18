"""
All pipelines must pin the same build image and the same test image tag, as
written by the infrastructure pipeline's commit-image-tags job. A partly
resolved merge or rebase of that commit otherwise leaves some pipelines on an
older image.

Run from the .circleci directory: python -m pytest tests_unit/test_image_pins.py
"""

import re
from pathlib import Path

import pytest

CIRCLECI = Path(__file__).parent.parent
IMAGES = {
    "build": re.compile(r'public\.ecr\.aws/b0b8h2r4/arangodb/ubuntubuildarangodb-\d+:[^ "]+'),
    "test": re.compile(r'public\.ecr\.aws/b0b8h2r4/test-ubuntu:[^ "]+'),
}


def pins(pattern):
    found = {}
    for path in sorted(CIRCLECI.glob("*.yml")):
        if path.name == "infrastructure.yml":
            continue
        for ref in pattern.findall(path.read_text(encoding="utf-8")):
            found.setdefault(ref, []).append(path.name)
    return found


@pytest.mark.parametrize("kind", sorted(IMAGES))
def test_single_image_pin(kind):
    found = pins(IMAGES[kind])
    assert found, f"no pipeline pins a {kind} image"
    assert len(found) == 1, f"pipelines pin different {kind} images: {found}"
