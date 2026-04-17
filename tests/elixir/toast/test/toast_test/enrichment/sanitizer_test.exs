defmodule ToastTest.Enrichment.SanitizerTest do
  use ExUnit.Case, async: true

  alias ToastTest.Enrichment.Sanitizer

  @tmp_dir Path.join(
             System.tmp_dir!(),
             "toast_sanitizer_test_#{System.unique_integer([:positive])}"
           )

  setup do
    File.mkdir_p!(@tmp_dir)
    on_exit(fn -> File.rm_rf!(@tmp_dir) end)
    {:ok, tmp_dir: @tmp_dir}
  end

  describe "read_all/2" do
    @report_a """
    ==================
    WARNING: ThreadSanitizer: heap-use-after-free (pid=12345)
      Write of size 4 at 0x7204000260b0 by thread T55:
        #0 some::function() file.cpp:54
    ==================\
    """

    @report_b """
    ==================
    WARNING: ThreadSanitizer: data race (pid=12345)
      Write of size 4 at 0x55555fd31078 by thread T219:
        #0 another::function() file.cpp:58
    ==================\
    """

    @sidecar_a "1000000\tSUMMARY: ThreadSanitizer: heap-use-after-free file.cpp:54 in some::function()\n"
    @sidecar_b "2000000\tSUMMARY: ThreadSanitizer: data race file.cpp:58 in another::function()\n"

    test "splits multi-report file into individual results", %{tmp_dir: dir} do
      path = Path.join(dir, "tsan.log.arangod.12345")
      File.write!(path, @report_a <> "\n" <> @report_b)

      assert {:ok, results} = Sanitizer.read_all(path)
      assert length(results) == 2

      assert Enum.at(results, 0).kind == "heap-use-after-free"
      assert Enum.at(results, 1).kind == "data race"
    end

    test "single-report file returns list with one result", %{tmp_dir: dir} do
      path = Path.join(dir, "tsan.log.arangod.12345")
      File.write!(path, @report_a)

      assert {:ok, [result]} = Sanitizer.read_all(path)
      assert result.kind == "heap-use-after-free"
      assert result.type == :tsan
    end

    test "uses sidecar timestamps when count matches", %{tmp_dir: dir} do
      path = Path.join(dir, "tsan.log.arangod.12345")
      File.write!(path, @report_a <> "\n" <> @report_b)

      sidecar = Path.join(dir, "sanitizer_reports.log.12345")
      File.write!(sidecar, @sidecar_a <> @sidecar_b)

      assert {:ok, results} = Sanitizer.read_all(path, sidecar)
      assert Enum.at(results, 0).timestamp == 1_000_000
      assert Enum.at(results, 1).timestamp == 2_000_000
    end

    test "timestamps are nil when sidecar count mismatches", %{tmp_dir: dir} do
      path = Path.join(dir, "tsan.log.arangod.12345")
      File.write!(path, @report_a <> "\n" <> @report_b)

      # Only one sidecar entry for two reports
      sidecar = Path.join(dir, "sanitizer_reports.log.12345")
      File.write!(sidecar, @sidecar_a)

      assert {:ok, results} = Sanitizer.read_all(path, sidecar)
      assert length(results) == 2

      assert Enum.at(results, 0).timestamp == nil
      assert Enum.at(results, 1).timestamp == nil
    end

    test "timestamps are nil when sidecar does not exist", %{tmp_dir: dir} do
      path = Path.join(dir, "tsan.log.arangod.12345")
      File.write!(path, @report_a <> "\n" <> @report_b)

      assert {:ok, results} = Sanitizer.read_all(path, "/nonexistent/sidecar")
      assert length(results) == 2

      assert Enum.at(results, 0).timestamp == nil
      assert Enum.at(results, 1).timestamp == nil
    end

    test "timestamp is nil when no sidecar given", %{tmp_dir: dir} do
      path = Path.join(dir, "tsan.log.arangod.12345")
      File.write!(path, @report_a)

      assert {:ok, [result]} = Sanitizer.read_all(path)
      assert result.timestamp == nil
    end

    test "preserves type from filename", %{tmp_dir: dir} do
      path = Path.join(dir, "alubsan.log.99")

      File.write!(
        path,
        "==================\nERROR: AddressSanitizer: heap-buffer-overflow\n=================="
      )

      assert {:ok, [result]} = Sanitizer.read_all(path)
      assert result.type == :alubsan
      assert result.kind == "heap-buffer-overflow"
    end

    test "returns error for nonexistent file" do
      assert {:error, :enoent} = Sanitizer.read_all("/nonexistent/path/tsan.log.1")
    end

    test "each result has its own content", %{tmp_dir: dir} do
      path = Path.join(dir, "tsan.log.arangod.12345")
      File.write!(path, @report_a <> "\n" <> @report_b)

      assert {:ok, [r1, r2]} = Sanitizer.read_all(path)
      assert r1.content =~ "heap-use-after-free"
      refute r1.content =~ "data race"
      assert r2.content =~ "data race"
      refute r2.content =~ "heap-use-after-free"
    end
  end
end
