defmodule Toast.DiagnosticsTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics
  alias Toast.Diagnostics.{Result, ServerDiagnostics}

  describe "to_server_entries/1 with Result struct" do
    test "returns all server map entries" do
      server = %ServerDiagnostics{server: :fake}
      result = %Result{servers: %{"srv1" => server, "srv2" => server}}

      entries = Diagnostics.to_server_entries(result)

      assert length(entries) == 2
      assert {"srv1", ^server} = List.keyfind(entries, "srv1", 0)
      assert {"srv2", ^server} = List.keyfind(entries, "srv2", 0)
    end

    test "returns empty list for empty servers" do
      result = %Result{servers: %{}}

      assert Diagnostics.to_server_entries(result) == []
    end
  end

  describe "to_server_entries/1 with plain map" do
    test "keeps entries with binary keys and map values" do
      diag = %{sanitizer_errors: [], log_report: nil}
      entries = Diagnostics.to_server_entries(%{"srv1" => diag})

      assert entries == [{"srv1", diag}]
    end

    test "filters out atom keys" do
      diag = %{sanitizer_errors: []}
      map = %{"srv1" => diag, agency_dump: "some_dump"}

      entries = Diagnostics.to_server_entries(map)

      assert entries == [{"srv1", diag}]
    end

    test "filters out non-map values" do
      map = %{"srv1" => "not_a_map", "srv2" => %{log_report: nil}}

      entries = Diagnostics.to_server_entries(map)

      assert [{"srv2", _}] = entries
    end

    test "returns empty list for empty map" do
      assert Diagnostics.to_server_entries(%{}) == []
    end
  end
end
