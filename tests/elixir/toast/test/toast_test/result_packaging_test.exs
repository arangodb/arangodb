defmodule ToastTest.ResultPackagingTest do
  use ExUnit.Case, async: true

  alias ToastTest.ResultPackaging

  describe "exit_code/1" do
    test "returns 0 for all passed" do
      results = %{
        test_failures: 0,
        server_crashed: false,
        infrastructure_failure: false,
        sanitizer_errors: false
      }

      assert ResultPackaging.exit_code(results) == 0
    end

    test "returns 1 for test failures" do
      results = %{
        test_failures: 3,
        server_crashed: false,
        infrastructure_failure: false,
        sanitizer_errors: false
      }

      assert ResultPackaging.exit_code(results) == 1
    end

    test "returns 2 for sanitizer errors" do
      results = %{
        test_failures: 0,
        server_crashed: false,
        infrastructure_failure: false,
        sanitizer_errors: true
      }

      assert ResultPackaging.exit_code(results) == 2
    end

    test "returns 3 for infrastructure failure" do
      results = %{
        test_failures: 0,
        server_crashed: false,
        infrastructure_failure: true,
        sanitizer_errors: false
      }

      assert ResultPackaging.exit_code(results) == 3
    end

    test "returns 4 for server crash" do
      results = %{
        test_failures: 0,
        server_crashed: true,
        infrastructure_failure: false,
        sanitizer_errors: false
      }

      assert ResultPackaging.exit_code(results) == 4
    end

    test "mixed results: highest severity wins (crash > infrastructure)" do
      results = %{
        test_failures: 1,
        server_crashed: true,
        infrastructure_failure: true,
        sanitizer_errors: true
      }

      assert ResultPackaging.exit_code(results) == 4
    end

    test "mixed results: infrastructure > sanitizer" do
      results = %{
        test_failures: 0,
        server_crashed: false,
        infrastructure_failure: true,
        sanitizer_errors: true
      }

      assert ResultPackaging.exit_code(results) == 3
    end

    test "mixed results: sanitizer > test failures" do
      results = %{
        test_failures: 5,
        server_crashed: false,
        infrastructure_failure: false,
        sanitizer_errors: true
      }

      assert ResultPackaging.exit_code(results) == 2
    end
  end

  describe "package/1" do
    test "no-op when ci is false" do
      assert :ok =
               ResultPackaging.package(
                 ci: false,
                 result_dir: "/tmp/nonexistent",
                 base_dir: "/tmp/nonexistent"
               )
    end

    @tag :tmp_dir
    test "creates tier 1 files when ci is true", %{tmp_dir: tmp_dir} do
      base_dir = Path.join(tmp_dir, "work")
      result_dir = Path.join(tmp_dir, "results")
      File.mkdir_p!(base_dir)
      File.mkdir_p!(result_dir)

      # Create the tier 1 source files in result_dir (they're already there from export step)
      File.write!(Path.join(result_dir, "results.json"), "{}")
      File.write!(Path.join(result_dir, "results.xml"), "<testsuites/>")

      # Create a toast.log in base_dir
      log_dir = Path.join(base_dir, "logs")
      File.mkdir_p!(log_dir)
      File.write!(Path.join(log_dir, "toast.log"), "log content")

      assert :ok =
               ResultPackaging.package(
                 ci: true,
                 result_dir: result_dir,
                 base_dir: base_dir,
                 suite_diagnostics: [],
                 log_file: Path.join(log_dir, "toast.log")
               )

      # Tier 1 files should exist in result_dir
      assert File.exists?(Path.join(result_dir, "results.json"))
      assert File.exists?(Path.join(result_dir, "results.xml"))
    end

    @tag :tmp_dir
    test "creates tier 2 archive when server logs exist", %{tmp_dir: tmp_dir} do
      base_dir = Path.join(tmp_dir, "work")
      result_dir = Path.join(tmp_dir, "results")
      File.mkdir_p!(base_dir)
      File.mkdir_p!(result_dir)

      # Create some server log files
      log_dir = Path.join(base_dir, "suite1/server1")
      File.mkdir_p!(log_dir)
      File.write!(Path.join(log_dir, "arangod.log"), "server log content")

      suite_diag = %{
        name: "suite1",
        log_files: [Path.join(log_dir, "arangod.log")],
        sanitizer_files: [],
        crash_reports: []
      }

      assert :ok =
               ResultPackaging.package(
                 ci: true,
                 result_dir: result_dir,
                 base_dir: base_dir,
                 suite_diagnostics: [suite_diag]
               )

      assert File.exists?(Path.join(result_dir, "toast-logs.tar.gz"))
    end

    @tag :tmp_dir
    test "tier 3 only created when core dumps exist", %{tmp_dir: tmp_dir} do
      base_dir = Path.join(tmp_dir, "work")
      result_dir = Path.join(tmp_dir, "results")
      File.mkdir_p!(base_dir)
      File.mkdir_p!(result_dir)

      # No core dumps
      assert :ok =
               ResultPackaging.package(
                 ci: true,
                 result_dir: result_dir,
                 base_dir: base_dir,
                 suite_diagnostics: []
               )

      # No .zst or .gz compressed core files should exist
      compressed = Path.wildcard(Path.join(result_dir, "core.*"))
      assert compressed == []
    end

    @tag :tmp_dir
    test "tier 3 compresses core dumps individually", %{tmp_dir: tmp_dir} do
      base_dir = Path.join(tmp_dir, "work")
      result_dir = Path.join(tmp_dir, "results")
      File.mkdir_p!(base_dir)
      File.mkdir_p!(result_dir)

      # Create a fake core dump
      core_path = Path.join(base_dir, "core.12345")
      File.write!(core_path, String.duplicate("x", 10_000))

      suite_diag = %{
        name: "suite1",
        log_files: [],
        sanitizer_files: [],
        crash_reports: [],
        core_dumps: [core_path]
      }

      assert :ok =
               ResultPackaging.package(
                 ci: true,
                 result_dir: result_dir,
                 base_dir: base_dir,
                 suite_diagnostics: [suite_diag]
               )

      # Should have a compressed core file
      compressed = Path.wildcard(Path.join(result_dir, "core.12345.*"))
      assert length(compressed) == 1
    end

    @tag :tmp_dir
    test "tier 3 archives work dir", %{tmp_dir: tmp_dir} do
      base_dir = Path.join(tmp_dir, "work")
      result_dir = Path.join(tmp_dir, "results")
      File.mkdir_p!(result_dir)

      # Create work dir with some content
      server_dir = Path.join(base_dir, "suite1/dbserver-0")
      File.mkdir_p!(server_dir)
      File.write!(Path.join(server_dir, "arangod.log"), "log content")
      File.write!(Path.join(server_dir, "data.db"), "db content")

      assert :ok =
               ResultPackaging.package(
                 ci: true,
                 result_dir: result_dir,
                 base_dir: base_dir,
                 suite_diagnostics: []
               )

      archive = Path.join(result_dir, "work-dir.tar.gz")
      assert File.exists?(archive)
      assert File.stat!(archive).size > 0
    end

    @tag :tmp_dir
    test "skips work dir archive when base_dir is nil", %{tmp_dir: tmp_dir} do
      result_dir = Path.join(tmp_dir, "results")
      File.mkdir_p!(result_dir)

      assert :ok =
               ResultPackaging.package(
                 ci: true,
                 result_dir: result_dir,
                 suite_diagnostics: []
               )

      refute File.exists?(Path.join(result_dir, "work-dir.tar.gz"))
    end
  end
end
