"""
Resource sizing logic for CircleCI.

Handles mapping of logical sizes to CircleCI resource classes,
accounting for architecture and sanitizer overhead.
"""

from ..config_lib import BuildConfig


class ResourceSizer:
    """Maps logical sizes to CircleCI resource classes."""

    # Resource class mappings by architecture
    AARCH64_SIZES = {
        "small": "arm.medium",
        "medium": "arm.medium",
        "medium+": "arm.large",
        "large": "arm.large",
        "xlarge": "arm.xlarge",
        "2xlarge": "arm.2xlarge",
    }

    X86_SIZES = {
        "small": "small",
        "medium": "medium",
        "medium+": "medium+",
        "large": "large",
        "xlarge": "xlarge",
        "2xlarge": "2xlarge",
    }

    @classmethod
    def get_resource_class(cls, size: str, arch: str) -> str:
        """
        Get CircleCI resource class for a size and architecture.

        Args:
            size: Logical size (small, medium, large, etc.)
            arch: Architecture (x64 or aarch64)

        Returns:
            CircleCI resource class string

        Raises:
            ValueError: If size or architecture is invalid
        """
        if arch == "aarch64":
            size_map = cls.AARCH64_SIZES
        elif arch == "x64":
            size_map = cls.X86_SIZES
        else:
            raise ValueError(f"Unknown architecture: {arch}")

        if size not in size_map:
            raise ValueError(f"Unknown size: {size}")

        return size_map[size]

    @classmethod
    def get_test_size(
        cls, size: str, build_config: BuildConfig, is_cluster: bool
    ) -> str:
        """
        Get test resource size accounting for sanitizer overhead.

        Sanitizer builds require significantly more resources:
        - TSAN on cluster needs the most (xlarge for small tests)
        - Other sanitizers need large for small tests, xlarge for medium+

        Args:
            size: Base logical size
            build_config: Build configuration (includes sanitizer info)
            is_cluster: Whether this is a cluster test

        Returns:
            CircleCI resource class string
        """
        adjusted_size = size

        if build_config.sanitizer:
            # Sanitizer builds need more resources
            if size == "small":
                # TSAN cluster tests need xlarge, others need large
                adjusted_size = (
                    "xlarge"
                    if build_config.sanitizer == "tsan" and is_cluster
                    else "large"
                )
            elif size in ["medium", "medium+", "large"]:
                # All sanitizers need xlarge for medium+ tests
                adjusted_size = "xlarge"

        return cls.get_resource_class(adjusted_size, build_config.architecture)
