defmodule ToastTest.ResultExporterTest do
  use ExUnit.Case, async: true

  alias ToastTest.ResultExporter

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

  describe "export when no results" do
    test "is a no-op when results are nil" do
      tmp = make_tmp_dir()

      assert :ok = ResultExporter.export("test", nil, tmp)
      assert File.ls!(tmp) == []
    end
  end

  describe "export with results" do
    test "writes test.json and test.xml" do
      tmp = make_tmp_dir()

      assert :ok = ResultExporter.export("test", sample_results(), tmp)

      assert File.exists?(Path.join(tmp, "test.json"))
      assert File.exists?(Path.join(tmp, "test.xml"))
    end

    test "written files are non-empty" do
      tmp = make_tmp_dir()

      ResultExporter.export("test", sample_results(), tmp)

      json_content = File.read!(Path.join(tmp, "test.json"))
      xml_content = File.read!(Path.join(tmp, "test.xml"))

      assert byte_size(json_content) > 0
      assert byte_size(xml_content) > 0
    end

    test "JSON output contains expected structure" do
      tmp = make_tmp_dir()

      ResultExporter.export("test", sample_results(), tmp)

      json_content = File.read!(Path.join(tmp, "test.json"))
      assert json_content =~ "toast_version"
      assert json_content =~ "test_run"
      assert json_content =~ "test passes"
    end

    test "XML output contains expected structure" do
      tmp = make_tmp_dir()

      ResultExporter.export("test", sample_results(), tmp)

      xml_content = File.read!(Path.join(tmp, "test.xml"))
      assert xml_content =~ "<?xml"
      assert xml_content =~ "<testsuites"
      assert xml_content =~ "test passes"
    end

    test "creates result directory if it does not exist" do
      tmp = make_tmp_dir()
      nested = Path.join(tmp, "nested/deep/dir")

      ResultExporter.export("test", sample_results(), nested)

      assert File.exists?(Path.join(nested, "test.json"))
    end

    test "includes diagnostics when available" do
      tmp = make_tmp_dir()

      diagnostics = %{
        sanitizer_errors: [],
        crash_report: nil,
        server_log: nil
      }

      analysis = %ToastTest.ResultExporter.AnalysisData{diagnostics: diagnostics}
      assert :ok = ResultExporter.export("test", sample_results(), analysis, tmp)

      json_content = File.read!(Path.join(tmp, "test.json"))
      assert json_content =~ "server_health"
    end
  end

  describe "export error handling" do
    test "returns :ok when writing to an unwritable path" do
      assert :ok = ResultExporter.export("test", sample_results(), "/proc/toast_nonexistent_dir")
    end
  end
end
