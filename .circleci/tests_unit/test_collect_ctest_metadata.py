"""
Unit tests for collect_ctest_metadata.py.
"""

import pytest
import sys
from pathlib import Path

import collect_ctest_metadata as ccm


def write_ctest_file(path, *test_names):
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = "".join(f'add_test([=[{name}]=] "cmd")\n' for name in test_names)
    path.write_text(lines or "# no tests\n")


class TestTestNames:
    def test_flattens_suites_across_entries(self, tmp_path):
        ctest_yml = tmp_path / "ctest.yml"
        ctest_yml.write_text(
            "- group_one:\n"
            "    suites:\n"
            "      - test_a\n"
            "      - test_b\n"
            "- group_two:\n"
            "    suites:\n"
            "      - test_c\n"
        )

        assert ccm.test_names(ctest_yml) == ["test_a", "test_b", "test_c"]

    def test_entry_without_suites_contributes_nothing(self, tmp_path):
        ctest_yml = tmp_path / "ctest.yml"
        ctest_yml.write_text("- group_one:\n    job: run-ctest-tests\n")

        assert ccm.test_names(ctest_yml) == []


class TestFindDefiningFiles:
    def test_finds_file_defining_the_named_test(self, tmp_path):
        write_ctest_file(tmp_path / "a" / "CTestTestfile.cmake", "my_test")
        write_ctest_file(tmp_path / "b" / "CTestTestfile.cmake", "other_test")

        matches = ccm.find_defining_files(tmp_path, "my_test")

        assert matches == [tmp_path / "a" / "CTestTestfile.cmake"]

    def test_matches_multiple_files_defining_the_same_test(self, tmp_path):
        write_ctest_file(tmp_path / "a" / "CTestTestfile.cmake", "shared_test")
        write_ctest_file(tmp_path / "b" / "CTestTestfile.cmake", "shared_test")

        matches = ccm.find_defining_files(tmp_path, "shared_test")

        assert sorted(matches) == sorted(
            [
                tmp_path / "a" / "CTestTestfile.cmake",
                tmp_path / "b" / "CTestTestfile.cmake",
            ]
        )

    def test_exits_when_no_file_defines_the_test(self, tmp_path):
        write_ctest_file(tmp_path / "a" / "CTestTestfile.cmake", "my_test")

        with pytest.raises(SystemExit):
            ccm.find_defining_files(tmp_path, "missing_test")

    def test_does_not_match_a_name_that_is_only_a_prefix(self, tmp_path):
        write_ctest_file(tmp_path / "a" / "CTestTestfile.cmake", "my_test_extended")

        with pytest.raises(SystemExit):
            ccm.find_defining_files(tmp_path, "my_test")


class TestAncestorChain:
    def test_lists_every_directory_from_build_dir_down_to_the_file(self, tmp_path):
        file_path = tmp_path / "a" / "b" / "CTestTestfile.cmake"

        assert ccm.ancestor_chain(tmp_path, file_path) == [
            tmp_path / "CTestTestfile.cmake",
            tmp_path / "a" / "CTestTestfile.cmake",
            tmp_path / "a" / "b" / "CTestTestfile.cmake",
        ]

    def test_file_directly_in_build_dir_yields_a_single_entry(self, tmp_path):
        file_path = tmp_path / "CTestTestfile.cmake"

        assert ccm.ancestor_chain(tmp_path, file_path) == [
            tmp_path / "CTestTestfile.cmake"
        ]


class TestMain:
    def test_writes_the_minimal_ancestor_chain_for_the_requested_tests(
        self, tmp_path, monkeypatch
    ):
        build_dir = tmp_path / "build"
        write_ctest_file(build_dir / "CTestTestfile.cmake")
        write_ctest_file(build_dir / "wanted" / "CTestTestfile.cmake", "wanted_test")
        write_ctest_file(
            build_dir / "unrelated" / "CTestTestfile.cmake", "other_test"
        )
        ctest_yml = tmp_path / "ctest.yml"
        ctest_yml.write_text("- group:\n    suites:\n      - wanted_test\n")
        output_file = tmp_path / "out.txt"

        monkeypatch.setattr(
            sys,
            "argv",
            ["collect_ctest_metadata.py", str(build_dir), str(ctest_yml), str(output_file)],
        )
        ccm.main()

        assert output_file.read_text().splitlines() == [
            str(build_dir / "CTestTestfile.cmake"),
            str(build_dir / "wanted" / "CTestTestfile.cmake"),
        ]
