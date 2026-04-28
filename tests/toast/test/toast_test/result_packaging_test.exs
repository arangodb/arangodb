################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule ToastTest.ResultPackagingTest do
  use ExUnit.Case, async: true

  alias ToastTest.ResultPackaging

  defp green_results do
    %{
      test_failures: 0,
      server_crashed: false,
      infrastructure_failure: false,
      sanitizer_errors: false
    }
  end

  defp failure_results do
    %{
      test_failures: 1,
      server_crashed: false,
      infrastructure_failure: false,
      sanitizer_errors: false
    }
  end

  defp sanitizer_results do
    %{
      test_failures: 0,
      server_crashed: false,
      infrastructure_failure: false,
      sanitizer_errors: true
    }
  end

  defp crash_results do
    %{
      test_failures: 0,
      server_crashed: true,
      infrastructure_failure: false,
      sanitizer_errors: false
    }
  end

  defp infra_results do
    %{
      test_failures: 0,
      server_crashed: false,
      infrastructure_failure: true,
      sanitizer_errors: false
    }
  end

  # Sets up a single server log file and returns {result_dir, suite_diag}.
  defp setup_tier2_fixture(tmp_dir) do
    result_dir = Path.join(tmp_dir, "results")
    File.mkdir_p!(result_dir)

    log_path = Path.join(tmp_dir, "suite1/server1/arangod.log")
    File.mkdir_p!(Path.dirname(log_path))
    File.write!(log_path, "server log content")

    suite_diag = %{
      name: "suite1",
      log_files: [log_path],
      sanitizer_files: [],
      crash_reports: []
    }

    {result_dir, suite_diag}
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
    test "tier 1: always created even on green run", %{tmp_dir: tmp_dir} do
      base_dir = Path.join(tmp_dir, "work")
      result_dir = Path.join(tmp_dir, "results")
      File.mkdir_p!(base_dir)
      File.mkdir_p!(result_dir)

      File.write!(Path.join(result_dir, "results.json"), "{}")
      File.write!(Path.join(result_dir, "results.xml"), "<testsuites/>")

      log_dir = Path.join(base_dir, "logs")
      File.mkdir_p!(log_dir)
      File.write!(Path.join(log_dir, "toast.log"), "log content")

      assert :ok =
               ResultPackaging.package(
                 ci: true,
                 run_results: green_results(),
                 result_dir: result_dir,
                 base_dir: base_dir,
                 suite_diagnostics: [],
                 log_file: Path.join(log_dir, "toast.log")
               )

      assert File.exists?(Path.join(result_dir, "results.json"))
      assert File.exists?(Path.join(result_dir, "results.xml"))
      assert File.exists?(Path.join(result_dir, "toast.log"))
    end

    # --- Tier 2 gating ---

    @tag :tmp_dir
    test "tier 2: skipped on green run", %{tmp_dir: tmp_dir} do
      {result_dir, suite_diag} = setup_tier2_fixture(tmp_dir)

      ResultPackaging.package(
        ci: true,
        run_results: green_results(),
        result_dir: result_dir,
        suite_diagnostics: [suite_diag]
      )

      refute File.exists?(Path.join(result_dir, "toast-logs.tar.gz"))
    end

    @tag :tmp_dir
    test "tier 2: created on any failure cause", %{tmp_dir: tmp_dir} do
      for {label, run_results} <- [
            {"test failure", failure_results()},
            {"sanitizer errors", sanitizer_results()},
            {"infrastructure failure", infra_results()},
            {"server crash", crash_results()}
          ] do
        sub_dir = Path.join(tmp_dir, label)
        File.mkdir_p!(sub_dir)
        {result_dir, suite_diag} = setup_tier2_fixture(sub_dir)

        ResultPackaging.package(
          ci: true,
          run_results: run_results,
          result_dir: result_dir,
          suite_diagnostics: [suite_diag]
        )

        assert File.exists?(Path.join(result_dir, "toast-logs.tar.gz")),
               "tier 2 archive missing for #{label}"
      end
    end

    # --- Tier 3 gating ---

    @tag :tmp_dir
    test "tier 3: skipped on any non-crash outcome", %{tmp_dir: tmp_dir} do
      for {label, run_results} <- [
            {"green", green_results()},
            {"test failure", failure_results()},
            {"sanitizer errors", sanitizer_results()},
            {"infrastructure failure", infra_results()}
          ] do
        sub_dir = Path.join(tmp_dir, label)
        base_dir = Path.join(sub_dir, "work")
        result_dir = Path.join(sub_dir, "results")
        server_dir = Path.join(base_dir, "suite1/dbserver-0")
        File.mkdir_p!(server_dir)
        File.mkdir_p!(result_dir)
        File.write!(Path.join(server_dir, "data.db"), "db content")

        ResultPackaging.package(
          ci: true,
          run_results: run_results,
          result_dir: result_dir,
          base_dir: base_dir,
          suite_diagnostics: []
        )

        refute File.exists?(Path.join(result_dir, "work-dir.tar.gz")),
               "tier 3 archive unexpectedly created for #{label}"
      end
    end

    @tag :tmp_dir
    test "tier 3: created on server crash", %{tmp_dir: tmp_dir} do
      base_dir = Path.join(tmp_dir, "work")
      result_dir = Path.join(tmp_dir, "results")
      File.mkdir_p!(base_dir)
      File.mkdir_p!(result_dir)

      core_path = Path.join(base_dir, "core.12345")
      File.write!(core_path, String.duplicate("x", 10_000))

      suite_diag = %{
        name: "suite1",
        log_files: [],
        sanitizer_files: [],
        crash_reports: [],
        core_dumps: [core_path]
      }

      ResultPackaging.package(
        ci: true,
        run_results: crash_results(),
        result_dir: result_dir,
        base_dir: base_dir,
        suite_diagnostics: [suite_diag]
      )

      compressed = Path.wildcard(Path.join(result_dir, "core.12345.*"))
      assert [compressed_path] = compressed
      assert Path.extname(compressed_path) in [".zst", ".gz"]
      assert File.stat!(compressed_path).size > 0
    end

    @tag :tmp_dir
    test "tier 3: archives work dir on crash", %{tmp_dir: tmp_dir} do
      base_dir = Path.join(tmp_dir, "work")
      result_dir = Path.join(tmp_dir, "results")
      File.mkdir_p!(result_dir)

      server_dir = Path.join(base_dir, "suite1/dbserver-0")
      File.mkdir_p!(server_dir)
      File.write!(Path.join(server_dir, "arangod.log"), "log content")

      ResultPackaging.package(
        ci: true,
        run_results: crash_results(),
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
                 run_results: crash_results(),
                 result_dir: result_dir,
                 suite_diagnostics: []
               )

      refute File.exists?(Path.join(result_dir, "work-dir.tar.gz"))
    end

    # --- force_all_tiers override ---

    @tag :tmp_dir
    test "force_all_tiers: packages tier 2 and 3 on green run", %{tmp_dir: tmp_dir} do
      base_dir = Path.join(tmp_dir, "work")
      result_dir = Path.join(tmp_dir, "results")
      File.mkdir_p!(result_dir)

      log_path = Path.join(base_dir, "suite1/server1/arangod.log")
      File.mkdir_p!(Path.dirname(log_path))
      File.write!(log_path, "server log")

      suite_diag = %{
        name: "suite1",
        log_files: [log_path],
        sanitizer_files: [],
        crash_reports: []
      }

      ResultPackaging.package(
        ci: true,
        run_results: green_results(),
        force_all_tiers: true,
        result_dir: result_dir,
        base_dir: base_dir,
        suite_diagnostics: [suite_diag]
      )

      assert File.exists?(Path.join(result_dir, "toast-logs.tar.gz"))
      assert File.exists?(Path.join(result_dir, "work-dir.tar.gz"))
    end
  end
end
