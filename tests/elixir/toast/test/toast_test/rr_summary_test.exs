defmodule ToastTest.Formatting.RrSummaryTest do
  use ExUnit.Case, async: true

  alias ToastTest.Formatting.RrSummary

  setup do
    tmp_dir =
      Path.join(System.tmp_dir!(), "toast_rr_summary_#{System.unique_integer([:positive])}")

    File.mkdir_p!(tmp_dir)
    on_exit(fn -> File.rm_rf!(tmp_dir) end)
    %{tmp_dir: tmp_dir}
  end

  test "print/1 with nil base_dir is a no-op" do
    assert :ok = RrSummary.print(nil)
  end

  test "print/1 with no rr traces produces no output", %{tmp_dir: tmp_dir} do
    # Create a server dir without rr-trace
    File.mkdir_p!(Path.join([tmp_dir, "deployment", "single"]))

    output = capture_io(fn -> RrSummary.print(tmp_dir) end)
    assert output == ""
  end

  test "print/1 lists rr traces for single server", %{tmp_dir: tmp_dir} do
    # Simulate single server layout: base_dir/single/rr-trace
    trace_dir = Path.join([tmp_dir, "single", "rr-trace"])
    File.mkdir_p!(trace_dir)

    output = capture_io(fn -> RrSummary.print(tmp_dir) end)
    assert output =~ "rr RECORDINGS"
    assert output =~ "single"
    assert output =~ "rr replay #{trace_dir}"
  end

  test "print/1 lists rr traces for cluster servers", %{tmp_dir: tmp_dir} do
    # Simulate cluster layout: base_dir/deployment-dir/dbserver-0/rr-trace
    deployment_dir = Path.join(tmp_dir, "cluster-00")

    for server <- ["dbserver-0", "dbserver-1", "coordinator-0"] do
      File.mkdir_p!(Path.join([deployment_dir, server, "rr-trace"]))
    end

    # Agent without rr-trace
    File.mkdir_p!(Path.join([deployment_dir, "agent-0"]))

    output = capture_io(fn -> RrSummary.print(tmp_dir) end)
    assert output =~ "rr RECORDINGS"
    assert output =~ "dbserver-0"
    assert output =~ "dbserver-1"
    assert output =~ "coordinator-0"
    refute output =~ "agent-0"
  end

  test "print/1 with nonexistent base_dir is a no-op" do
    assert :ok = RrSummary.print("/nonexistent/path")
  end

  defp capture_io(fun) do
    ExUnit.CaptureIO.capture_io(fun)
  end
end
