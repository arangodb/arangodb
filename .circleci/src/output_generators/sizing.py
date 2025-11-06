"""
Resource sizing logic for CircleCI.

Handles mapping of logical sizes to CircleCI resource classes,
accounting for architecture and sanitizer overhead.
"""

from typing import Dict, Optional
from ..config_lib import BuildConfig, ResourceSize, Sanitizer, Architecture


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

    # Architecture aliases for compatibility
    ARCH_ALIASES = {
        "x86_64": "x64",
        "amd64": "x64",
        "arm64": "aarch64",
    }

    @classmethod
    def _normalize_arch(cls, arch: str) -> str:
        """Normalize architecture string to canonical form."""
        return cls.ARCH_ALIASES.get(arch, arch)

    @classmethod
    def get_resource_class(cls, size: ResourceSize, arch: Architecture | str) -> str:
        """
        Get CircleCI resource class for a size and architecture.

        Args:
            size: Logical size enum
            arch: Architecture enum or string (x64, aarch64, or aliases)

        Returns:
            CircleCI resource class string

        Raises:
            ValueError: If architecture is invalid
        """
        # Convert Architecture enum to string if needed
        if isinstance(arch, Architecture):
            arch = arch.value
        else:
            arch = cls._normalize_arch(arch)

        if arch == "aarch64":
            return cls.AARCH64_SIZES[size]
        elif arch == "x64":
            return cls.X86_SIZES[size]
        else:
            raise ValueError(
                f"Unknown architecture: {arch}. "
                f"Valid options: x64, aarch64 (or aliases: amd64, x86_64, arm64)"
            )

    @classmethod
    def _apply_sanitizer_overhead(
        cls, size: ResourceSize, sanitizer: Optional[Sanitizer], is_cluster: bool
    ) -> ResourceSize:
        """
        Apply sanitizer overhead to a base size.

        Sanitizer builds require significantly more resources due to:
        - Memory overhead from tracking/instrumentation
        - Slower execution requiring more CPU time

        Args:
            size: Base size before overhead
            sanitizer: Sanitizer type (tsan, asan, ubsan)
            is_cluster: Whether this is a cluster test

        Returns:
            Adjusted size accounting for sanitizer overhead
        """
        # TSAN cluster tests need the most resources - even small tests get bumped to xlarge
        if size == ResourceSize.SMALL:
            if sanitizer == Sanitizer.TSAN and is_cluster:
                return ResourceSize.XLARGE
            else:
                return ResourceSize.LARGE

        # Medium/large tests get bumped to xlarge for all sanitizers
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

        if build_config.sanitizer:
            adjusted_size = cls._apply_sanitizer_overhead(
                size, build_config.sanitizer, is_cluster
            )

        return cls.get_resource_class(adjusted_size, build_config.architecture)
