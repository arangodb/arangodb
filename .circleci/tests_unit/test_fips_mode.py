"""
Tests for the use-fips mode of the test pipeline (generate_config.py --fips).

FIPS mode compiles every workflow with USE_FIPS, runs the tests with
--server.fips-enable, adds -fips to the workflow names and, for the
docker-images workflows, builds a FIPS server image (core-fips-<flavor>) next
to a client-tools image from a regular, fully static compile.
"""

import json
from pathlib import Path
from typing import Any, Dict, List

import yaml

from src.config_lib import (
    TestJob,
    SuiteConfig,
    TestOptions,
    TestDefinitionFile,
    BuildConfig,
    BuildVariant,
    Architecture,
)
from src.filters import FilterCriteria
from src.output_generators.base import GeneratorConfig, CircleCIConfig
from src.output_generators.circleci import CircleCIGenerator

CIRCLECI_DIR = Path(__file__).parent.parent
REPO_DIR = CIRCLECI_DIR.parent

FIPS_ARG = "--extraArgs:server.fips-enable true"


def make_generator(
    fips: bool,
    build_variants: List[BuildVariant] = None,
    full: bool = False,
    docker: str = "none",
) -> CircleCIGenerator:
    config = GeneratorConfig(
        filter_criteria=FilterCriteria(full=full),
        circleci=CircleCIConfig(
            create_test_docker_images=docker,
            create_preview_docker_images=docker,
            test_image="default",
        ),
        build_variants=build_variants or [BuildVariant.NORMAL],
        fips=fips,
    )
    env = {"CIRCLE_SHA1": "deadbee123456", "ENTERPRISE_COMMIT": "cafe42abcdef"}
    return CircleCIGenerator(
        config,
        base_config={"workflows": {}},
        env_getter=lambda k, default: env.get(k, default),
    )


def sample_definitions() -> List[TestDefinitionFile]:
    return [
        TestDefinitionFile(
            jobs={
                "shell_api": TestJob(
                    name="shell_api",
                    suites=[SuiteConfig(name="shell_api")],
                    options=TestOptions(),
                )
            }
        )
    ]


def jobs_of(workflow: Dict[str, Any], job_type: str) -> List[Dict[str, Any]]:
    return [job[job_type] for job in workflow["jobs"] if job_type in job]


def suite_jobs_of(workflow: Dict[str, Any]) -> List[Dict[str, Any]]:
    return [
        params
        for job in workflow["jobs"]
        for job_type, params in job.items()
        if job_type not in ("compile-linux", "run-cppcheck", "run-clang-tidy")
    ]


class TestWorkflowNames:
    def test_fips_goes_right_after_the_architecture(self):
        gen = make_generator(fips=True)
        cases = [
            (BuildConfig(architecture=Architecture.X64), "x64-fips-pr"),
            (
                BuildConfig(architecture=Architecture.X64, nightly=True),
                "x64-fips-nightly",
            ),
            (
                BuildConfig(
                    architecture=Architecture.X64, build_variant=BuildVariant.TSAN
                ),
                "x64-fips-pr-tsan",
            ),
            (
                BuildConfig(
                    architecture=Architecture.AARCH64,
                    build_variant=BuildVariant.ALUBSAN,
                    nightly=True,
                ),
                "aarch64-fips-nightly-alubsan",
            ),
        ]
        for build_config, expected in cases:
            assert gen._generate_workflow_name(build_config) == expected

    def test_no_fips_keeps_the_names(self):
        gen = make_generator(fips=False)
        build_config = BuildConfig(
            architecture=Architecture.X64, build_variant=BuildVariant.TSAN
        )
        assert gen._generate_workflow_name(build_config) == "x64-pr-tsan"

    def test_generate_names_every_workflow_fips(self):
        gen = make_generator(
            fips=True,
            build_variants=[
                BuildVariant.NORMAL,
                BuildVariant.TSAN,
                BuildVariant.ALUBSAN,
                BuildVariant.COVERAGE,
            ],
            full=True,
        )
        names = set(gen.generate(sample_definitions())["workflows"])
        assert names == {
            f"{arch}-fips-nightly{suffix}"
            for arch in ("x64", "aarch64")
            for suffix in ("", "-tsan", "-alubsan", "-coverage")
        }


class TestCompileAndTests:
    VARIANTS = [BuildVariant.NORMAL, BuildVariant.TSAN, BuildVariant.ALUBSAN]

    def test_every_compile_is_fips(self):
        """Sanitizer builds included: their presets are dynamic anyway, so
        USE_FIPS only compiles FipsFeature in."""
        result = make_generator(fips=True, build_variants=self.VARIANTS).generate(
            sample_definitions()
        )
        compiles = [
            params
            for workflow in result["workflows"].values()
            for params in jobs_of(workflow, "compile-linux")
        ]
        # 3 variants x 2 archs, plus the non-maintainer smoke build of the
        # 2 non-instrumented workflows
        assert len(compiles) == 8
        for params in compiles:
            assert params["fips"] is True, params["name"]
            # FIPS comes from the fips parameter, not from separate presets
            assert not params["preset"].endswith("-fips")

    def test_tests_enable_fips_once(self):
        result = make_generator(fips=True, build_variants=self.VARIANTS).generate(
            sample_definitions()
        )
        test_jobs = [
            params
            for workflow in result["workflows"].values()
            for params in suite_jobs_of(workflow)
        ]
        assert test_jobs
        for params in test_jobs:
            assert params["extraArgs"].count(FIPS_ARG) == 1, params["name"]

    def test_regular_mode_is_unchanged(self):
        result = make_generator(fips=False, build_variants=self.VARIANTS).generate(
            sample_definitions()
        )
        for workflow in result["workflows"].values():
            for params in jobs_of(workflow, "compile-linux"):
                assert "fips" not in params
            for params in suite_jobs_of(workflow):
                assert "server.fips-enable" not in params.get("extraArgs", "")


class TestDockerImages:
    def test_fips_workflow_structure(self):
        workflows: Dict[str, Any] = {}
        make_generator(fips=True)._add_docker_images_workflow(workflows, "test")

        assert list(workflows) == ["test-fips-docker-images"]
        jobs = workflows["test-fips-docker-images"]["jobs"]
        assert [(t, p["name"]) for job in jobs for t, p in job.items()] == [
            ("compile-linux", "build-x64-for-test-image"),
            ("compile-linux", "build-x64-for-test-client-tools-image"),
            ("build-docker-image", "build-x64-test-image"),
            ("compile-linux", "build-aarch64-for-test-image"),
            ("compile-linux", "build-aarch64-for-test-client-tools-image"),
            ("build-docker-image", "build-aarch64-test-image"),
            ("push-docker-manifest", "push-test-manifest"),
        ]

    def test_server_compile_is_fips_client_compile_is_regular(self):
        for flavor in ("test", "preview"):
            workflows: Dict[str, Any] = {}
            make_generator(fips=True)._add_docker_images_workflow(workflows, flavor)
            workflow = workflows[f"{flavor}-fips-docker-images"]
            compiles = {p["name"]: p for p in jobs_of(workflow, "compile-linux")}

            for arch, preset_arch in (("x64", "x64"), ("aarch64", "arm64")):
                server = compiles[f"build-{arch}-for-{flavor}-image"]
                assert server["preset"] == f"{flavor}-docker-image-{preset_arch}-fips"
                assert "install-package-name" not in server  # install-<arch>
                assert "persist-docker-context" not in server  # default true

                client = compiles[f"build-{arch}-for-{flavor}-client-tools-image"]
                assert client["preset"] == f"{flavor}-docker-image-{preset_arch}"
                assert client["install-package-name"] == "install-client"
                assert client["persist-docker-context"] is False
                assert client["create-install-package"] is True
                assert "fips" not in client

    def test_image_and_manifest_jobs(self):
        workflows: Dict[str, Any] = {}
        make_generator(fips=True)._add_docker_images_workflow(workflows, "preview")
        workflow = workflows["preview-fips-docker-images"]

        for params in jobs_of(workflow, "build-docker-image"):
            arch = params["build-arch"]
            assert params["fips"] is True
            assert params["name-suffix"] == "preview"
            assert params["requires"] == [
                f"build-{arch}-for-preview-image",
                f"build-{arch}-for-preview-client-tools-image",
            ]
        [manifest] = jobs_of(workflow, "push-docker-manifest")
        assert manifest["fips"] is True
        assert manifest["tag"] == "deadbee_cafe42a"

    def test_regular_docker_workflow_is_unchanged(self):
        workflows: Dict[str, Any] = {}
        make_generator(fips=False)._add_docker_images_workflow(workflows, "test")
        workflow = workflows["test-docker-images"]
        assert len(jobs_of(workflow, "compile-linux")) == 2
        for job in workflow["jobs"]:
            for params in job.values():
                assert "fips" not in params

    def test_generate_with_docker_images(self):
        result = make_generator(fips=True, docker="ubuntu").generate([])
        assert set(result["workflows"]) == {
            "x64-fips-pr",
            "aarch64-fips-pr",
            "test-fips-docker-images",
            "preview-fips-docker-images",
        }


class TestPresetsAndCircleCIConfig:
    def load_presets(self) -> Dict[str, Dict[str, Any]]:
        with open(REPO_DIR / "CMakePresets.json", encoding="utf-8") as f:
            presets = json.load(f)
        return {
            "configure": {p["name"]: p for p in presets["configurePresets"]},
            "build": {p["name"]: p for p in presets["buildPresets"]},
        }

    def test_fips_docker_presets(self):
        """Without -static, -llapack -lgfortran would link the shared
        libraries; like nightly-package-*-fips, the FIPS image presets drop
        them and CMakeLists.txt links the static archives itself."""
        presets = self.load_presets()
        for flavor in ("test", "preview"):
            for arch in ("x64", "arm64"):
                name = f"{flavor}-docker-image-{arch}-fips"
                configure = presets["configure"][name]
                assert configure["inherits"] == ["fips", f"{flavor}-docker-image-{arch}"]
                flags = configure["cacheVariables"]["CMAKE_EXE_LINKER_FLAGS"]
                assert "-llapack" not in flags and "-lgfortran" not in flags
                assert presets["build"][name]["configurePreset"] == name

    def test_base_config_parameters(self):
        with open(CIRCLECI_DIR / "base_config.yml", encoding="utf-8") as f:
            base = yaml.safe_load(f)
        assert base["parameters"]["use-fips"]["default"] is False
        jobs = base["jobs"]
        assert jobs["compile-linux"]["parameters"]["fips"]["default"] is False
        assert jobs["compile-linux"]["parameters"]["install-package-name"]["default"] == "install"
        assert jobs["compile-linux"]["parameters"]["persist-docker-context"]["default"] is True
        for job in ("build-docker-image", "push-docker-manifest"):
            assert jobs[job]["parameters"]["fips"]["default"] is False

    def test_setup_pipeline_passes_fips(self):
        with open(CIRCLECI_DIR / "config.yml", encoding="utf-8") as f:
            text = f.read()
        setup = yaml.safe_load(text)
        assert setup["parameters"]["use-fips"]["default"] is False
        assert (
            "<<# pipeline.parameters.use-fips >> --fips <</ pipeline.parameters.use-fips >>"
            in text
        )
