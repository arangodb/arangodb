"""
Tests for resource sizing logic.

This module tests the mapping of logical sizes to CircleCI resource classes,
accounting for architecture and sanitizer overhead.
"""

import pytest
from src.config_lib import BuildConfig, ResourceSize, BuildVariant, Architecture
from src.output_generators.sizing import ResourceSizer


@pytest.mark.parametrize(
    "size,x64_class,aarch64_class",
    [
        (ResourceSize.SMALL, "small", "arm.medium"),
        (ResourceSize.MEDIUM, "medium", "arm.medium"),
        (ResourceSize.MEDIUM_PLUS, "medium+", "arm.large"),
        (ResourceSize.LARGE, "large", "arm.large"),
        (ResourceSize.XLARGE, "xlarge", "arm.xlarge"),
        (ResourceSize.XXLARGE, "2xlarge", "arm.2xlarge"),
    ],
)
class TestGetResourceClass:
    """Test get_resource_class method for all sizes and architectures."""

    def test_x64_mapping(self, size, x64_class, aarch64_class):
        """Test x64 architecture resource class mapping."""
        assert ResourceSizer.get_resource_class(size, Architecture.X64) == x64_class

    def test_aarch64_mapping(self, size, x64_class, aarch64_class):
        """Test aarch64 architecture resource class mapping."""
        assert (
            ResourceSizer.get_resource_class(size, Architecture.AARCH64)
            == aarch64_class
        )


class TestGetTestSize:
    """Test get_test_size method with sanitizer overhead."""

    def test_no_sanitizer_no_adjustment(self, x64_enterprise_build):
        """Test that sizes are not adjusted without sanitizers."""
        config = x64_enterprise_build

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

    def test_tsan_cluster_small_to_xlarge(self, x64_tsan_build):
        """Test TSAN cluster small tests need xlarge."""
        config = x64_tsan_build

        result = ResourceSizer.get_test_size(
            ResourceSize.SMALL, config, is_cluster=True
        )
        assert result == "xlarge"

    def test_tsan_single_small_to_large(self):
        """Test TSAN single small tests need large (not xlarge)."""
        config = BuildConfig(
            architecture=Architecture.X64,
            build_variant=BuildVariant.TSAN,
        )

        result = ResourceSizer.get_test_size(
            ResourceSize.SMALL, config, is_cluster=False
        )
        assert result == "large"

    def test_alubsan_small_to_large(self):
        """Test non-TSAN sanitizers bump small to large."""
        config = BuildConfig(
            architecture=Architecture.X64,
            build_variant=BuildVariant.ALUBSAN,
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
        sanitizer_configs = [
            ("TSAN", BuildVariant.TSAN),
            ("ALUBSAN", BuildVariant.ALUBSAN),
        ]

        for name, build_variant in sanitizer_configs:
            config = BuildConfig(
                architecture=Architecture.X64, build_variant=build_variant
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
                    f"Expected xlarge for {name} {size.value} cluster, "
                    f"got {result_cluster}"
                )
                assert result_single == "xlarge", (
                    f"Expected xlarge for {name} {size.value} single, "
                    f"got {result_single}"
                )

    def test_sanitizer_xlarge_unchanged(self):
        """Test that xlarge stays xlarge even with sanitizers."""
        config = BuildConfig(
            architecture=Architecture.X64,
            build_variant=BuildVariant.TSAN,
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
            architecture=Architecture.X64,
            build_variant=BuildVariant.TSAN,
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
            architecture=Architecture.AARCH64,
            build_variant=BuildVariant.TSAN,
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
        sanitizer_configs = [
            ("TSAN", BuildVariant.TSAN),
            ("ALUBSAN", BuildVariant.ALUBSAN),
        ]

        for name, build_variant in sanitizer_configs:
            config = BuildConfig(
                architecture=Architecture.X64, build_variant=build_variant
            )

            # Small TSAN cluster is special case
            if build_variant.is_tsan:
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
                    f"Expected large for {name} small cluster, " f"got {result}"
                )

            # All sanitizers: medium -> xlarge
            assert (
                ResourceSizer.get_test_size(
                    ResourceSize.MEDIUM, config, is_cluster=False
                )
                == "xlarge"
            )


class TestSanitizerOverhead:
    """Integration tests for sanitizer overhead calculations."""

    def test_tsan_overhead_matrix(self):
        """Test TSAN overhead across all size/cluster combinations."""
        config = BuildConfig(
            architecture=Architecture.X64,
            build_variant=BuildVariant.TSAN,
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
            architecture=Architecture.X64,
            build_variant=BuildVariant.ALUBSAN,
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
