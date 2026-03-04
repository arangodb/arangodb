defmodule Toast.Analysis.CrashesTest do
  use ExUnit.Case, async: true

  alias Toast.Analysis.Crashes

  test "no crashes returns message" do
    assert Crashes.format(%{}) == "No crash or sanitizer issues detected."
  end

  test "formats crash attribution" do
    results = %{
      "crash_matching" => %{
        "matched" => [
          %{
            "module" => "Elixir.Smoke.Test",
            "test" => "my_test",
            "confidence" => "high",
            "crash" => %{"signal_name" => "SIGSEGV", "server_id" => "single"}
          }
        ],
        "unmatched" => []
      }
    }

    output = Crashes.format(results)
    assert output =~ "Crash Attribution"
    assert output =~ "Smoke.Test"
    assert output =~ "SIGSEGV"
  end

  test "formats sanitizer attribution" do
    results = %{
      "sanitizer_matching" => %{
        "matched" => [],
        "unmatched" => [
          %{"sanitizer_type" => "asan", "server_id" => "single", "file_path" => "/tmp/asan.log"}
        ]
      }
    }

    output = Crashes.format(results)
    assert output =~ "Sanitizer"
    assert output =~ "ASAN"
  end
end
