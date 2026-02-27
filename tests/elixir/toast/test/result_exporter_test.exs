defmodule ToastTest.ResultExporterTest do
  use ExUnit.Case, async: false

  alias ToastTest.ResultExporter

  @results_key :__test_results__
  @diagnostics_key :__test_diagnostics__

  setup do
    saved_result_dir = System.get_env("TOAST_RESULT_DIR")
    saved_results = Application.get_env(:toast, @results_key)
    saved_diagnostics = Application.get_env(:toast, @diagnostics_key)

    on_exit(fn ->
      if saved_result_dir,
        do: System.put_env("TOAST_RESULT_DIR", saved_result_dir),
        else: System.delete_env("TOAST_RESULT_DIR")

      if saved_results,
        do: Application.put_env(:toast, @results_key, saved_results),
        else: Application.delete_env(:toast, @results_key)

      if saved_diagnostics,
        do: Application.put_env(:toast, @diagnostics_key, saved_diagnostics),
        else: Application.delete_env(:toast, @diagnostics_key)
    end)

    System.delete_env("TOAST_RESULT_DIR")
    Application.delete_env(:toast, @results_key)
    Application.delete_env(:toast, @diagnostics_key)

    :ok
  end

  defp sample_results do
    %{
      suite_started_at: DateTime.utc_now(),
      suite_finished_at: DateTime.utc_now(),
      times_us: %{async: 0, load: 1000, run: 5000},
      tests: [
        %{
          module: FakeTest,
          name: "test passes",
          outcome: :passed,
          duration_us: 1000,
          failure: nil,
          tags: %{file: "test/fake.exs", line: 1}
        }
      ]
    }
  end

  defp make_tmp_dir do
    path =
      Path.join(System.tmp_dir!(), "toast_exporter_test_#{System.unique_integer([:positive])}")

    File.mkdir_p!(path)

    on_exit(fn -> File.rm_rf!(path) end)

    path
  end

  describe "result_dir/0" do
    test "returns TOAST_RESULT_DIR when set" do
      System.put_env("TOAST_RESULT_DIR", "/custom/result/path")
      assert ResultExporter.result_dir() == "/custom/result/path"
    end

    test "returns default when TOAST_RESULT_DIR is not set" do
      System.delete_env("TOAST_RESULT_DIR")
      assert ResultExporter.result_dir() == "toast-results"
    end
  end

  describe "export/0 when no results in app env" do
    test "is a no-op when results are nil" do
      tmp = make_tmp_dir()
      System.put_env("TOAST_RESULT_DIR", tmp)
      Application.delete_env(:toast, @results_key)

      assert :ok = ResultExporter.export()
      assert File.ls!(tmp) == []
    end
  end

  describe "export/0 with results available" do
    test "writes results.json and results.xml" do
      tmp = make_tmp_dir()
      System.put_env("TOAST_RESULT_DIR", tmp)
      Application.put_env(:toast, @results_key, sample_results())

      assert :ok = ResultExporter.export()

      json_path = Path.join(tmp, "results.json")
      xml_path = Path.join(tmp, "results.xml")

      assert File.exists?(json_path)
      assert File.exists?(xml_path)
    end

    test "written files are non-empty" do
      tmp = make_tmp_dir()
      System.put_env("TOAST_RESULT_DIR", tmp)
      Application.put_env(:toast, @results_key, sample_results())

      ResultExporter.export()

      json_content = File.read!(Path.join(tmp, "results.json"))
      xml_content = File.read!(Path.join(tmp, "results.xml"))

      assert byte_size(json_content) > 0
      assert byte_size(xml_content) > 0
    end

    test "JSON output contains expected structure" do
      tmp = make_tmp_dir()
      System.put_env("TOAST_RESULT_DIR", tmp)
      Application.put_env(:toast, @results_key, sample_results())

      ResultExporter.export()

      json_content = File.read!(Path.join(tmp, "results.json"))
      assert json_content =~ "toast_version"
      assert json_content =~ "test_run"
      assert json_content =~ "test passes"
    end

    test "XML output contains expected structure" do
      tmp = make_tmp_dir()
      System.put_env("TOAST_RESULT_DIR", tmp)
      Application.put_env(:toast, @results_key, sample_results())

      ResultExporter.export()

      xml_content = File.read!(Path.join(tmp, "results.xml"))
      assert xml_content =~ "<?xml"
      assert xml_content =~ "<testsuites"
      assert xml_content =~ "test passes"
    end

    test "creates result directory if it does not exist" do
      tmp = make_tmp_dir()
      nested = Path.join(tmp, "nested/deep/dir")
      System.put_env("TOAST_RESULT_DIR", nested)
      Application.put_env(:toast, @results_key, sample_results())

      ResultExporter.export()

      assert File.exists?(Path.join(nested, "results.json"))
    end

    test "includes diagnostics when available" do
      tmp = make_tmp_dir()
      System.put_env("TOAST_RESULT_DIR", tmp)
      Application.put_env(:toast, @results_key, sample_results())

      Application.put_env(:toast, @diagnostics_key, %{
        sanitizer_errors: [],
        crash_report: nil,
        server_log: nil
      })

      assert :ok = ResultExporter.export()

      json_content = File.read!(Path.join(tmp, "results.json"))
      assert json_content =~ "server_health"
    end
  end

  describe "export/0 error handling" do
    test "returns :ok when writing to an unwritable path" do
      System.put_env("TOAST_RESULT_DIR", "/proc/toast_nonexistent_dir")
      Application.put_env(:toast, @results_key, sample_results())

      # Should not crash, returns :ok via rescue
      assert :ok = ResultExporter.export()
    end
  end
end
