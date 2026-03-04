defmodule ToastTest.ResultExporterTest do
  use ExUnit.Case, async: false

  alias ToastTest.ResultExporter

  setup do
    saved_result_dir = System.get_env("TOAST_RESULT_DIR")

    on_exit(fn ->
      if saved_result_dir,
        do: System.put_env("TOAST_RESULT_DIR", saved_result_dir),
        else: System.delete_env("TOAST_RESULT_DIR")
    end)

    System.delete_env("TOAST_RESULT_DIR")

    :ok
  end

  defp sample_results do
    %{
      started_at: DateTime.utc_now(),
      finished_at: DateTime.utc_now(),
      times_us: %{async: 0, load: 1000, run: 5000},
      modules: %{
        FakeTest => %{
          tests: [
            %{
              module: FakeTest,
              name: "test passes",
              outcome: :passed,
              duration_us: 1000,
              failure: nil,
              tags: %{file: "test/fake.exs", line: 1}
            }
          ],
          started_at: DateTime.utc_now(),
          finished_at: DateTime.utc_now()
        }
      }
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

  describe "export/2 when no results" do
    test "is a no-op when results are nil" do
      tmp = make_tmp_dir()
      System.put_env("TOAST_RESULT_DIR", tmp)

      assert :ok = ResultExporter.export("test", nil)
      assert File.ls!(tmp) == []
    end
  end

  describe "export/2 with results" do
    test "writes test.json and test.xml" do
      tmp = make_tmp_dir()
      System.put_env("TOAST_RESULT_DIR", tmp)

      assert :ok = ResultExporter.export("test", sample_results())

      json_path = Path.join(tmp, "test.json")
      xml_path = Path.join(tmp, "test.xml")

      assert File.exists?(json_path)
      assert File.exists?(xml_path)
    end

    test "written files are non-empty" do
      tmp = make_tmp_dir()
      System.put_env("TOAST_RESULT_DIR", tmp)

      ResultExporter.export("test", sample_results())

      json_content = File.read!(Path.join(tmp, "test.json"))
      xml_content = File.read!(Path.join(tmp, "test.xml"))

      assert byte_size(json_content) > 0
      assert byte_size(xml_content) > 0
    end

    test "JSON output contains expected structure" do
      tmp = make_tmp_dir()
      System.put_env("TOAST_RESULT_DIR", tmp)

      ResultExporter.export("test", sample_results())

      json_content = File.read!(Path.join(tmp, "test.json"))
      assert json_content =~ "toast_version"
      assert json_content =~ "test_run"
      assert json_content =~ "test passes"
    end

    test "XML output contains expected structure" do
      tmp = make_tmp_dir()
      System.put_env("TOAST_RESULT_DIR", tmp)

      ResultExporter.export("test", sample_results())

      xml_content = File.read!(Path.join(tmp, "test.xml"))
      assert xml_content =~ "<?xml"
      assert xml_content =~ "<testsuites"
      assert xml_content =~ "test passes"
    end

    test "creates result directory if it does not exist" do
      tmp = make_tmp_dir()
      nested = Path.join(tmp, "nested/deep/dir")
      System.put_env("TOAST_RESULT_DIR", nested)

      ResultExporter.export("test", sample_results())

      assert File.exists?(Path.join(nested, "test.json"))
    end

    test "includes diagnostics when available" do
      tmp = make_tmp_dir()
      System.put_env("TOAST_RESULT_DIR", tmp)

      diagnostics = %{
        sanitizer_errors: [],
        crash_report: nil,
        server_log: nil
      }

      assert :ok = ResultExporter.export("test", sample_results(), diagnostics)

      json_content = File.read!(Path.join(tmp, "test.json"))
      assert json_content =~ "server_health"
    end
  end

  describe "export/2 error handling" do
    test "returns :ok when writing to an unwritable path" do
      System.put_env("TOAST_RESULT_DIR", "/proc/toast_nonexistent_dir")

      # Should not crash, returns :ok via rescue
      assert :ok = ResultExporter.export("test", sample_results())
    end
  end
end
