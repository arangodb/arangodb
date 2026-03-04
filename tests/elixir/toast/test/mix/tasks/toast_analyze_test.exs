defmodule Mix.Tasks.Toast.AnalyzeTest do
  use ExUnit.Case, async: true

  import ExUnit.CaptureIO

  @tag :tmp_dir
  test "reads results.json and prints summary", %{tmp_dir: tmp_dir} do
    path = Path.join(tmp_dir, "results.json")

    results = %{
      "test_run" => %{"duration_seconds" => 10.5},
      "summary" => %{"total" => 5, "passed" => 4, "failed" => 1, "skipped" => 0},
      "modules" => %{}
    }

    File.write!(path, encode_json(results))

    output = capture_io(fn -> Mix.Tasks.Toast.Analyze.run([path]) end)
    assert output =~ "Total: 5 tests"
  end

  @tag :tmp_dir
  test "--failures shows detailed failure info", %{tmp_dir: tmp_dir} do
    path = Path.join(tmp_dir, "results.json")

    results = %{
      "modules" => %{
        "Elixir.Test" => %{
          "tests" => [
            %{
              "module" => "Elixir.Test",
              "name" => "fails",
              "outcome" => "failed",
              "failure" => %{"message" => "assert false"}
            }
          ]
        }
      }
    }

    File.write!(path, encode_json(results))

    output = capture_io(fn -> Mix.Tasks.Toast.Analyze.run([path, "--failures"]) end)
    assert output =~ "1 failure"
    assert output =~ "assert false"
  end

  @tag :tmp_dir
  test "--crashes shows crash diagnostics", %{tmp_dir: tmp_dir} do
    path = Path.join(tmp_dir, "results.json")

    results = %{
      "crash_matching" => %{
        "matched" => [
          %{
            "module" => "Elixir.Test",
            "test" => "crashes",
            "confidence" => "high",
            "crash" => %{"signal_name" => "SIGSEGV", "server_id" => "single"}
          }
        ],
        "unmatched" => []
      }
    }

    File.write!(path, encode_json(results))

    output = capture_io(fn -> Mix.Tasks.Toast.Analyze.run([path, "--crashes"]) end)
    assert output =~ "SIGSEGV"
  end

  @tag :tmp_dir
  test "--slow N shows N slowest tests", %{tmp_dir: tmp_dir} do
    path = Path.join(tmp_dir, "results.json")

    results = %{
      "modules" => %{
        "A" => %{
          "tests" => [
            %{
              "module" => "A",
              "name" => "slow",
              "outcome" => "passed",
              "duration_seconds" => 10.0
            },
            %{
              "module" => "A",
              "name" => "fast",
              "outcome" => "passed",
              "duration_seconds" => 0.1
            }
          ]
        }
      }
    }

    File.write!(path, encode_json(results))

    output = capture_io(fn -> Mix.Tasks.Toast.Analyze.run([path, "--slow", "1"]) end)
    assert output =~ "slow"
    assert output =~ "10.0"
  end

  test "invalid file path produces clear error" do
    assert_raise Mix.Error, ~r/file not found/, fn ->
      Mix.Tasks.Toast.Analyze.run(["/nonexistent/path/results.json"])
    end
  end

  @tag :tmp_dir
  test "malformed JSON produces clear error", %{tmp_dir: tmp_dir} do
    path = Path.join(tmp_dir, "bad.json")
    File.write!(path, "not json{{{")

    assert_raise Mix.Error, ~r/invalid JSON/, fn ->
      Mix.Tasks.Toast.Analyze.run([path])
    end
  end

  test "missing file argument produces usage error" do
    assert_raise Mix.Error, ~r/Usage/, fn ->
      Mix.Tasks.Toast.Analyze.run([])
    end
  end

  defp encode_json(data) do
    data
    |> :json.encode()
    |> IO.iodata_to_binary()
  end
end
