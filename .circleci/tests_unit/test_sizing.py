"""
Tests for resource sizing logic.

This module tests the mapping of logical sizes to CircleCI resource classes,
accounting for architecture and sanitizer overhead.
"""

import pytest
from src.config_lib import BuildConfig, ResourceSize, Sanitizer, Architecture
from src.output_generators.sizing import ResourceSizer


class TestGetResourceClass:
    """Test get_resource_class method."""

    def test_x64_sizes(self):
        """Test x64 architecture resource class mappings."""
        assert (
            ResourceSizer.get_resource_class(ResourceSize.SMALL, Architecture.X64)
            == "small"
        )
        assert (
            ResourceSizer.get_resource_class(ResourceSize.MEDIUM, Architecture.X64)
            == "medium"
        )
        assert (
            ResourceSizer.get_resource_class(ResourceSize.MEDIUM_PLUS, Architecture.X64)
            == "medium+"
        )
        assert (
            ResourceSizer.get_resource_class(ResourceSize.LARGE, Architecture.X64)
            == "large"
        )
        assert (
            ResourceSizer.get_resource_class(ResourceSize.XLARGE, Architecture.X64)
            == "xlarge"
        )
        assert (
            ResourceSizer.get_resource_class(ResourceSize.XXLARGE, Architecture.X64)
            == "2xlarge"
        )

    def test_aarch64_sizes(self):
        """Test aarch64 architecture resource class mappings."""
        assert (
            ResourceSizer.get_resource_class(ResourceSize.SMALL, Architecture.AARCH64)
            == "arm.medium"
        )
        assert (
            ResourceSizer.get_resource_class(ResourceSize.MEDIUM, Architecture.AARCH64)
            == "arm.medium"
        )
        assert (
            ResourceSizer.get_resource_class(
                ResourceSize.MEDIUM_PLUS, Architecture.AARCH64
            )
            == "arm.large"
        )
        assert (
            ResourceSizer.get_resource_class(ResourceSize.LARGE, Architecture.AARCH64)
            == "arm.large"
        )
        assert (
            ResourceSizer.get_resource_class(ResourceSize.XLARGE, Architecture.AARCH64)
            == "arm.xlarge"
        )
        assert (
            ResourceSizer.get_resource_class(ResourceSize.XXLARGE, Architecture.AARCH64)
            == "arm.2xlarge"
        )


class TestGetTestSize:
    """Test get_test_size method with sanitizer overhead."""

    def test_no_sanitizer_no_adjustment(self):
        """Test that sizes are not adjusted without sanitizers."""
        config = BuildConfig(architecture=Architecture.X64, enterprise=True)

        assert (
            ResourceSizer.get_test_size(ResourceSize.SMALL, config, is_cluster=False)
            == "small"
        )
        assert (
            ResourceSizer.get_test_size(ResourceSize.MEDIUM, config, is_cluster=False)
            == "medium"
        )
        assert (
            ResourceSizer.get_test_size(ResourceSize.LARGE, config, is_cluster=False)
            == "large"
        )
        assert (
            ResourceSizer.get_test_size(ResourceSize.XLARGE, config, is_cluster=False)
            == "xlarge"
        )

    def test_tsan_cluster_small_to_xlarge(self):
        """Test TSAN cluster small tests need xlarge."""
        config = BuildConfig(
            architecture=Architecture.X64, enterprise=True, sanitizer=Sanitizer.TSAN
        )

        result = ResourceSizer.get_test_size(
            ResourceSize.SMALL, config, is_cluster=True
        )
        assert result == "xlarge"

    def test_tsan_single_small_to_large(self):
        """Test TSAN single small tests need large (not xlarge)."""
        config = BuildConfig(
            architecture=Architecture.X64, enterprise=True, sanitizer=Sanitizer.TSAN
        )

        result = ResourceSizer.get_test_size(
            ResourceSize.SMALL, config, is_cluster=False
        )
        assert result == "large"

    def test_alubsan_small_to_large(self):
        """Test non-TSAN sanitizers bump small to large."""
        config = BuildConfig(
            architecture=Architecture.X64, enterprise=True, sanitizer=Sanitizer.ALUBSAN
        )

        # Both cluster and single get large for alubsan
        assert (
            ResourceSizer.get_test_size(ResourceSize.SMALL, config, is_cluster=True)
            == "large"
        )
        assert (
            ResourceSizer.get_test_size(ResourceSize.SMALL, config, is_cluster=False)
            == "large"
        )

    def test_sanitizer_medium_to_xlarge(self):
        """Test sanitizers bump medium/medium+/large to xlarge."""
        for sanitizer in [Sanitizer.TSAN, Sanitizer.ALUBSAN]:
            config = BuildConfig(
                architecture=Architecture.X64, enterprise=True, sanitizer=sanitizer
            )

            for size in [
                ResourceSize.MEDIUM,
                ResourceSize.MEDIUM_PLUS,
                ResourceSize.LARGE,
            ]:
                result_cluster = ResourceSizer.get_test_size(
                    size, config, is_cluster=True
                )
                result_single = ResourceSizer.get_test_size(
                    size, config, is_cluster=False
                )

                assert result_cluster == "xlarge", (
                    f"Expected xlarge for {sanitizer} {size.value} cluster, "
                    f"got {result_cluster}"
                )
                assert result_single == "xlarge", (
                    f"Expected xlarge for {sanitizer} {size.value} single, "
                    f"got {result_single}"
                )

    def test_sanitizer_xlarge_unchanged(self):
        """Test that xlarge stays xlarge even with sanitizers."""
        config = BuildConfig(
            architecture=Architecture.X64, enterprise=True, sanitizer=Sanitizer.TSAN
        )

        assert (
            ResourceSizer.get_test_size(ResourceSize.XLARGE, config, is_cluster=True)
            == "xlarge"
        )
        assert (
            ResourceSizer.get_test_size(ResourceSize.XLARGE, config, is_cluster=False)
            == "xlarge"
        )

    def test_sanitizer_2xlarge_unchanged(self):
        """Test that 2xlarge stays 2xlarge even with sanitizers."""
        config = BuildConfig(
            architecture=Architecture.X64, enterprise=True, sanitizer=Sanitizer.TSAN
        )

        assert (
            ResourceSizer.get_test_size(ResourceSize.XXLARGE, config, is_cluster=True)
            == "2xlarge"
        )
        assert (
            ResourceSizer.get_test_size(ResourceSize.XXLARGE, config, is_cluster=False)
            == "2xlarge"
        )

    def test_aarch64_with_sanitizer(self):
        """Test ARM architecture with sanitizer adjustments."""
        config = BuildConfig(
            architecture=Architecture.AARCH64, enterprise=True, sanitizer=Sanitizer.TSAN
        )

        # Small on TSAN cluster -> xlarge -> arm.xlarge
        assert (
            ResourceSizer.get_test_size(ResourceSize.SMALL, config, is_cluster=True)
            == "arm.xlarge"
        )

        # Small on TSAN single -> large -> arm.large
        assert (
            ResourceSizer.get_test_size(ResourceSize.SMALL, config, is_cluster=False)
            == "arm.large"
        )

        # Medium on TSAN -> xlarge -> arm.xlarge
        assert (
            ResourceSizer.get_test_size(ResourceSize.MEDIUM, config, is_cluster=True)
            == "arm.xlarge"
        )

    def test_all_sanitizers_apply_same_logic(self):
        """Test that all sanitizer types follow the same sizing logic."""
        sanitizers = [Sanitizer.TSAN, Sanitizer.ALUBSAN]

        for sanitizer in sanitizers:
            config = BuildConfig(
                architecture=Architecture.X64, enterprise=True, sanitizer=sanitizer
            )

            # Small TSAN cluster is special case
            if sanitizer == Sanitizer.TSAN:
                assert (
                    ResourceSizer.get_test_size(
                        ResourceSize.SMALL, config, is_cluster=True
                    )
                    == "xlarge"
                )
            else:
                # Other sanitizers: small -> large
                result = ResourceSizer.get_test_size(
                    ResourceSize.SMALL, config, is_cluster=True
                )
                assert result == "large", (
                    f"Expected large for {sanitizer} small cluster, " f"got {result}"
                )

            # All sanitizers: medium -> xlarge
            assert (
                ResourceSizer.get_test_size(
                    ResourceSize.MEDIUM, config, is_cluster=False
                )
                == "xlarge"
            )

    def test_enterprise_and_community_same_sizing(self):
        """Test that enterprise flag doesn't affect sizing."""
        config_ent = BuildConfig(
            architecture=Architecture.X64, sanitizer=Sanitizer.TSAN, enterprise=True
        )
        config_comm = BuildConfig(
            architecture=Architecture.X64, sanitizer=Sanitizer.TSAN, enterprise=False
        )

        # Should give same results
        assert ResourceSizer.get_test_size(
            ResourceSize.SMALL, config_ent, is_cluster=True
        ) == ResourceSizer.get_test_size(
            ResourceSize.SMALL, config_comm, is_cluster=True
        )

        assert ResourceSizer.get_test_size(
            ResourceSize.MEDIUM, config_ent, is_cluster=False
        ) == ResourceSizer.get_test_size(
            ResourceSize.MEDIUM, config_comm, is_cluster=False
        )


class TestSanitizerOverhead:
    """Integration tests for sanitizer overhead calculations."""

    def test_tsan_overhead_matrix(self):
        """Test TSAN overhead across all size/cluster combinations."""
        config = BuildConfig(
            architecture=Architecture.X64, enterprise=True, sanitizer=Sanitizer.TSAN
        )

        expected = {
            (ResourceSize.SMALL, True): "xlarge",  # Special case: TSAN cluster small
            (ResourceSize.SMALL, False): "large",
            (ResourceSize.MEDIUM, True): "xlarge",
            (ResourceSize.MEDIUM, False): "xlarge",
            (ResourceSize.MEDIUM_PLUS, True): "xlarge",
            (ResourceSize.MEDIUM_PLUS, False): "xlarge",
            (ResourceSize.LARGE, True): "xlarge",
            (ResourceSize.LARGE, False): "xlarge",
            (ResourceSize.XLARGE, True): "xlarge",
            (ResourceSize.XLARGE, False): "xlarge",
            (ResourceSize.XXLARGE, True): "2xlarge",
            (ResourceSize.XXLARGE, False): "2xlarge",
        }

        for (size, is_cluster), expected_result in expected.items():
            result = ResourceSizer.get_test_size(size, config, is_cluster)
            assert result == expected_result, (
                f"TSAN {size.value} cluster={is_cluster}: "
                f"expected {expected_result}, got {result}"
            )

    def test_alubsan_overhead_matrix(self):
        """Test ALUBSAN overhead across all size/cluster combinations."""
        config = BuildConfig(
            architecture=Architecture.X64, enterprise=True, sanitizer=Sanitizer.ALUBSAN
        )

        expected = {
            (ResourceSize.SMALL, True): "large",
            (ResourceSize.SMALL, False): "large",
            (ResourceSize.MEDIUM, True): "xlarge",
            (ResourceSize.MEDIUM, False): "xlarge",
            (ResourceSize.MEDIUM_PLUS, True): "xlarge",
            (ResourceSize.MEDIUM_PLUS, False): "xlarge",
            (ResourceSize.LARGE, True): "xlarge",
            (ResourceSize.LARGE, False): "xlarge",
            (ResourceSize.XLARGE, True): "xlarge",
            (ResourceSize.XLARGE, False): "xlarge",
            (ResourceSize.XXLARGE, True): "2xlarge",
            (ResourceSize.XXLARGE, False): "2xlarge",
        }

        for (size, is_cluster), expected_result in expected.items():
            result = ResourceSizer.get_test_size(size, config, is_cluster)
            assert result == expected_result, (
                f"ALUBSAN {size.value} cluster={is_cluster}: "
                f"expected {expected_result}, got {result}"
            )
