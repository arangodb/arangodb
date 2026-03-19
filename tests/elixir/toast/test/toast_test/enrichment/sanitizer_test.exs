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

  describe "read/1" do
    test "reads alubsan log file", %{tmp_dir: dir} do
      path = Path.join(dir, "alubsan.log.12345")
      content = "ERROR: AddressSanitizer: heap-buffer-overflow\nsome details"
      File.write!(path, content)

      assert {:ok, result} = Sanitizer.read(path)
      assert result.content == content
      assert result.type == :alubsan
      assert result.kind == "heap-buffer-overflow"
      assert %DateTime{} = result.timestamp
    end

    test "reads tsan log file", %{tmp_dir: dir} do
      path = Path.join(dir, "tsan.log.67890")
      content = "WARNING: ThreadSanitizer: data race (pid=16449)"
      File.write!(path, content)

      assert {:ok, result} = Sanitizer.read(path)
      assert result.content == content
      assert result.type == :tsan
      assert result.kind == "data race"
    end

    test "returns unknown type for unrecognized filename", %{tmp_dir: dir} do
      path = Path.join(dir, "something.log.99")
      File.write!(path, "data")

      assert {:ok, result} = Sanitizer.read(path)
      assert result.type == :unknown
      assert result.kind == nil
    end

    test "returns error for nonexistent file" do
      assert {:error, :enoent} = Sanitizer.read("/nonexistent/path/alubsan.log.1")
    end

    test "timestamp reflects file modification time", %{tmp_dir: dir} do
      path = Path.join(dir, "alubsan.log.1")
      File.write!(path, "test")

      assert {:ok, result} = Sanitizer.read(path)

      # Timestamp should be close to now (within a few seconds)
      diff = DateTime.diff(DateTime.utc_now(), result.timestamp, :second)
      assert diff >= 0 and diff < 10
    end
  end
end
