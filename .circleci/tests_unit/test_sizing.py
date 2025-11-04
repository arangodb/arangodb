"""
Tests for resource sizing logic.

This module tests the mapping of logical sizes to CircleCI resource classes,
accounting for architecture and sanitizer overhead.
"""

import pytest
from src.config_lib import BuildConfig
from src.output_generators.sizing import ResourceSizer


class TestGetResourceClass:
    """Test get_resource_class method."""

    def test_x64_sizes(self):
        """Test x64 architecture resource class mappings."""
        assert ResourceSizer.get_resource_class("small", "x64") == "small"
        assert ResourceSizer.get_resource_class("medium", "x64") == "medium"
        assert ResourceSizer.get_resource_class("medium+", "x64") == "medium+"
        assert ResourceSizer.get_resource_class("large", "x64") == "large"
        assert ResourceSizer.get_resource_class("xlarge", "x64") == "xlarge"
        assert ResourceSizer.get_resource_class("2xlarge", "x64") == "2xlarge"

    def test_aarch64_sizes(self):
        """Test aarch64 architecture resource class mappings."""
        assert ResourceSizer.get_resource_class("small", "aarch64") == "arm.medium"
        assert ResourceSizer.get_resource_class("medium", "aarch64") == "arm.medium"
        assert ResourceSizer.get_resource_class("medium+", "aarch64") == "arm.large"
        assert ResourceSizer.get_resource_class("large", "aarch64") == "arm.large"
        assert ResourceSizer.get_resource_class("xlarge", "aarch64") == "arm.xlarge"
        assert ResourceSizer.get_resource_class("2xlarge", "aarch64") == "arm.2xlarge"

    def test_invalid_architecture(self):
        """Test error handling for invalid architecture."""
        with pytest.raises(ValueError, match="Unknown architecture: foo"):
            ResourceSizer.get_resource_class("medium", "foo")

        with pytest.raises(ValueError, match="Unknown architecture: ARM"):
            ResourceSizer.get_resource_class("medium", "ARM")

    def test_invalid_size(self):
        """Test error handling for invalid size."""
        with pytest.raises(ValueError, match="Unknown size: tiny"):
            ResourceSizer.get_resource_class("tiny", "x64")

        with pytest.raises(ValueError, match="Unknown size: huge"):
            ResourceSizer.get_resource_class("huge", "aarch64")


class TestGetTestSize:
    """Test get_test_size method with sanitizer overhead."""

    def test_no_sanitizer_no_adjustment(self):
        """Test that sizes are not adjusted without sanitizers."""
        config = BuildConfig(architecture="x64", enterprise=True)

        assert ResourceSizer.get_test_size("small", config, is_cluster=False) == "small"
        assert (
            ResourceSizer.get_test_size("medium", config, is_cluster=False) == "medium"
        )
        assert ResourceSizer.get_test_size("large", config, is_cluster=False) == "large"
        assert (
            ResourceSizer.get_test_size("xlarge", config, is_cluster=False) == "xlarge"
        )

    def test_tsan_cluster_small_to_xlarge(self):
        """Test TSAN cluster small tests need xlarge."""
        config = BuildConfig(architecture="x64", enterprise=True, sanitizer="tsan")

        result = ResourceSizer.get_test_size("small", config, is_cluster=True)
        assert result == "xlarge"

    def test_tsan_single_small_to_large(self):
        """Test TSAN single small tests need large (not xlarge)."""
        config = BuildConfig(architecture="x64", enterprise=True, sanitizer="tsan")

        result = ResourceSizer.get_test_size("small", config, is_cluster=False)
        assert result == "large"

    def test_asan_small_to_large(self):
        """Test non-TSAN sanitizers bump small to large."""
        config = BuildConfig(architecture="x64", enterprise=True, sanitizer="asan")

        # Both cluster and single get large for asan
        assert ResourceSizer.get_test_size("small", config, is_cluster=True) == "large"
        assert ResourceSizer.get_test_size("small", config, is_cluster=False) == "large"

    def test_sanitizer_medium_to_xlarge(self):
        """Test sanitizers bump medium/medium+/large to xlarge."""
        for sanitizer in ["tsan", "asan", "ubsan"]:
            config = BuildConfig(
                architecture="x64", enterprise=True, sanitizer=sanitizer
            )

            for size in ["medium", "medium+", "large"]:
                result_cluster = ResourceSizer.get_test_size(
                    size, config, is_cluster=True
                )
                result_single = ResourceSizer.get_test_size(
                    size, config, is_cluster=False
                )

                assert result_cluster == "xlarge", (
                    f"Expected xlarge for {sanitizer} {size} cluster, "
                    f"got {result_cluster}"
                )
                assert result_single == "xlarge", (
                    f"Expected xlarge for {sanitizer} {size} single, "
                    f"got {result_single}"
                )

    def test_sanitizer_xlarge_unchanged(self):
        """Test that xlarge stays xlarge even with sanitizers."""
        config = BuildConfig(architecture="x64", enterprise=True, sanitizer="tsan")

        assert (
            ResourceSizer.get_test_size("xlarge", config, is_cluster=True) == "xlarge"
        )
        assert (
            ResourceSizer.get_test_size("xlarge", config, is_cluster=False) == "xlarge"
        )

    def test_sanitizer_2xlarge_unchanged(self):
        """Test that 2xlarge stays 2xlarge even with sanitizers."""
        config = BuildConfig(architecture="x64", enterprise=True, sanitizer="tsan")

        assert (
            ResourceSizer.get_test_size("2xlarge", config, is_cluster=True) == "2xlarge"
        )
        assert (
            ResourceSizer.get_test_size("2xlarge", config, is_cluster=False)
            == "2xlarge"
        )

    def test_aarch64_with_sanitizer(self):
        """Test ARM architecture with sanitizer adjustments."""
        config = BuildConfig(architecture="aarch64", enterprise=True, sanitizer="tsan")

        # Small on TSAN cluster -> xlarge -> arm.xlarge
        assert (
            ResourceSizer.get_test_size("small", config, is_cluster=True)
            == "arm.xlarge"
        )

        # Small on TSAN single -> large -> arm.large
        assert (
            ResourceSizer.get_test_size("small", config, is_cluster=False)
            == "arm.large"
        )

        # Medium on TSAN -> xlarge -> arm.xlarge
        assert (
            ResourceSizer.get_test_size("medium", config, is_cluster=True)
            == "arm.xlarge"
        )

    def test_all_sanitizers_apply_same_logic(self):
        """Test that all sanitizer types follow the same sizing logic."""
        sanitizers = ["tsan", "asan", "ubsan"]

        for sanitizer in sanitizers:
            config = BuildConfig(
                architecture="x64", enterprise=True, sanitizer=sanitizer
            )

            # Small TSAN cluster is special case
            if sanitizer == "tsan":
                assert (
                    ResourceSizer.get_test_size("small", config, is_cluster=True)
                    == "xlarge"
                )
            else:
                # Other sanitizers: small -> large
                result = ResourceSizer.get_test_size("small", config, is_cluster=True)
                assert result == "large", (
                    f"Expected large for {sanitizer} small cluster, " f"got {result}"
                )

            # All sanitizers: medium -> xlarge
            assert (
                ResourceSizer.get_test_size("medium", config, is_cluster=False)
                == "xlarge"
            )

    def test_enterprise_and_community_same_sizing(self):
        """Test that enterprise flag doesn't affect sizing."""
        config_ent = BuildConfig(architecture="x64", sanitizer="tsan", enterprise=True)
        config_comm = BuildConfig(
            architecture="x64", sanitizer="tsan", enterprise=False
        )

        # Should give same results
        assert ResourceSizer.get_test_size(
            "small", config_ent, is_cluster=True
        ) == ResourceSizer.get_test_size("small", config_comm, is_cluster=True)

        assert ResourceSizer.get_test_size(
            "medium", config_ent, is_cluster=False
        ) == ResourceSizer.get_test_size("medium", config_comm, is_cluster=False)


class TestSanitizerOverhead:
    """Integration tests for sanitizer overhead calculations."""

    def test_tsan_overhead_matrix(self):
        """Test TSAN overhead across all size/cluster combinations."""
        config = BuildConfig(architecture="x64", enterprise=True, sanitizer="tsan")

        expected = {
            ("small", True): "xlarge",  # Special case: TSAN cluster small
            ("small", False): "large",
            ("medium", True): "xlarge",
            ("medium", False): "xlarge",
            ("medium+", True): "xlarge",
            ("medium+", False): "xlarge",
            ("large", True): "xlarge",
            ("large", False): "xlarge",
            ("xlarge", True): "xlarge",
            ("xlarge", False): "xlarge",
            ("2xlarge", True): "2xlarge",
            ("2xlarge", False): "2xlarge",
        }

        for (size, is_cluster), expected_result in expected.items():
            result = ResourceSizer.get_test_size(size, config, is_cluster)
            assert result == expected_result, (
                f"TSAN {size} cluster={is_cluster}: "
                f"expected {expected_result}, got {result}"
            )

    def test_asan_overhead_matrix(self):
        """Test ASAN overhead across all size/cluster combinations."""
        config = BuildConfig(architecture="x64", enterprise=True, sanitizer="asan")

        expected = {
            ("small", True): "large",
            ("small", False): "large",
            ("medium", True): "xlarge",
            ("medium", False): "xlarge",
            ("medium+", True): "xlarge",
            ("medium+", False): "xlarge",
            ("large", True): "xlarge",
            ("large", False): "xlarge",
            ("xlarge", True): "xlarge",
            ("xlarge", False): "xlarge",
            ("2xlarge", True): "2xlarge",
            ("2xlarge", False): "2xlarge",
        }

        for (size, is_cluster), expected_result in expected.items():
            result = ResourceSizer.get_test_size(size, config, is_cluster)
            assert result == expected_result, (
                f"ASAN {size} cluster={is_cluster}: "
                f"expected {expected_result}, got {result}"
            )
