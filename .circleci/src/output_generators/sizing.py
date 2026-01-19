"""
Resource sizing logic for CircleCI.

Handles mapping of logical sizes to CircleCI resource classes,
accounting for architecture and sanitizer overhead.
"""

from typing import Dict, Optional
from ..config_lib import BuildConfig, ResourceSize, Architecture


class ResourceSizer:
    """Maps logical sizes to CircleCI resource classes."""

    # Resource class mappings by architecture
    AARCH64_SIZES: Dict[ResourceSize, str] = {
        ResourceSize.SMALL: "arm.medium",
        ResourceSize.MEDIUM: "arm.medium",
        ResourceSize.MEDIUM_PLUS: "arm.large",
        ResourceSize.LARGE: "arm.large",
        ResourceSize.XLARGE: "arm.xlarge",
        ResourceSize.XXLARGE: "arm.2xlarge",
    }

    X86_SIZES: Dict[ResourceSize, str] = {
        ResourceSize.SMALL: "small",
        ResourceSize.MEDIUM: "medium",
        ResourceSize.MEDIUM_PLUS: "medium+",
        ResourceSize.LARGE: "large",
        ResourceSize.XLARGE: "xlarge",
        ResourceSize.XXLARGE: "2xlarge",
    }

    @classmethod
    def get_resource_class(cls, size: ResourceSize, arch: Architecture) -> str:
        """
        Get CircleCI resource class for a size and architecture.

        Args:
            size: Logical size enum
            arch: Architecture enum

        Returns:
            CircleCI resource class string
        """
        if arch == Architecture.AARCH64:
            return cls.AARCH64_SIZES[size]
        if arch == Architecture.X64:
            return cls.X86_SIZES[size]

        # This should never happen with a proper Architecture enum
        raise ValueError(f"Unknown architecture: {arch}")

    @classmethod
    def _apply_instrumentation_overhead(
        cls, size: ResourceSize, build_config: BuildConfig, is_cluster: bool
    ) -> ResourceSize:
        """
        Apply instrumentation overhead to a base size.

        Instrumented builds require significantly more resources due to:
        - Memory overhead from tracking/instrumentation
        - Slower execution requiring more CPU time

        Args:
            size: Base size before overhead
            build_config: Build configuration (includes variant info)
            is_cluster: Whether this is a cluster test

        Returns:
            Adjusted size accounting for instrumentation overhead
        """
        # TSAN cluster tests need the most resources - even small tests get bumped to xlarge
        if size == ResourceSize.SMALL:
            if build_config.build_variant.is_tsan and is_cluster:
                return ResourceSize.XLARGE
            return ResourceSize.LARGE

        # Medium/large tests get bumped to xlarge for all instrumented builds
        if size in (ResourceSize.MEDIUM, ResourceSize.MEDIUM_PLUS, ResourceSize.LARGE):
            return ResourceSize.XLARGE

        # xlarge and 2xlarge stay unchanged
        return size

    @classmethod
    def get_test_size(
        cls, size: ResourceSize, build_config: BuildConfig, is_cluster: bool
    ) -> str:
        """
        Get test resource size accounting for sanitizer overhead.

        Args:
            size: Base logical size enum
            build_config: Build configuration (includes sanitizer info)
            is_cluster: Whether this is a cluster test

        Returns:
            CircleCI resource class string
        """
        adjusted_size = size

        if build_config.build_variant.is_instrumented:
            adjusted_size = cls._apply_instrumentation_overhead(
                size, build_config, is_cluster
            )

        return cls.get_resource_class(adjusted_size, build_config.architecture)
