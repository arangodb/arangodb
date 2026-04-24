defmodule Mix.Tasks.Toast.Gen.SuiteTest do
  use ExUnit.Case, async: true

  alias Mix.Tasks.Toast.Gen.Suite

  import ExUnit.CaptureIO

  @tag :tmp_dir
  test "generates suite directory with suite.ex and test_example.exs", %{tmp_dir: dir} do
    suite_dir = Path.join(dir, "suites/my_suite")

    capture_io(fn ->
      Suite.generate(["my_suite"], dir)
    end)

    assert File.dir?(suite_dir)
    assert File.exists?(Path.join(suite_dir, "suite.ex"))
    assert File.exists?(Path.join(suite_dir, "test_example.exs"))
  end

  @tag :tmp_dir
  test "suite.ex contains use ToastTest.Suite", %{tmp_dir: dir} do
    capture_io(fn ->
      Suite.generate(["smoke_test"], dir)
    end)

    content = File.read!(Path.join(dir, "suites/smoke_test/suite.ex"))
    assert content =~ "use ToastTest.Suite"
  end

  @tag :tmp_dir
  test "suite.ex uses camelized module name", %{tmp_dir: dir} do
    capture_io(fn ->
      Suite.generate(["smoke_test"], dir)
    end)

    content = File.read!(Path.join(dir, "suites/smoke_test/suite.ex"))
    assert content =~ "defmodule SmokeTest.Suite do"
  end

  @tag :tmp_dir
  test "test_example.exs uses suite module name", %{tmp_dir: dir} do
    capture_io(fn ->
      Suite.generate(["smoke_test"], dir)
    end)

    content = File.read!(Path.join(dir, "suites/smoke_test/test_example.exs"))
    assert content =~ "defmodule SmokeTest.ExampleTest do"
    assert content =~ "use SmokeTest.Suite"
  end

  @tag :tmp_dir
  test "--mode cluster produces mode: :cluster in suite.ex", %{tmp_dir: dir} do
    capture_io(fn ->
      Suite.generate(["cluster_tests", "--mode", "cluster"], dir)
    end)

    content = File.read!(Path.join(dir, "suites/cluster_tests/suite.ex"))
    assert content =~ "mode: :cluster"
  end

  @tag :tmp_dir
  test "--mode single_server produces mode: :single_server in suite.ex", %{tmp_dir: dir} do
    capture_io(fn ->
      Suite.generate(["single_tests", "--mode", "single_server"], dir)
    end)

    content = File.read!(Path.join(dir, "suites/single_tests/suite.ex"))
    assert content =~ "mode: :single_server"
  end

  @tag :tmp_dir
  test "no mode flag produces suite without explicit mode", %{tmp_dir: dir} do
    capture_io(fn ->
      Suite.generate(["no_mode"], dir)
    end)

    content = File.read!(Path.join(dir, "suites/no_mode/suite.ex"))
    refute content =~ "mode:"
  end

  @tag :tmp_dir
  test "raises error on existing directory", %{tmp_dir: dir} do
    suite_dir = Path.join(dir, "suites/existing")
    File.mkdir_p!(suite_dir)

    assert_raise Mix.Error, ~r/already exists/, fn ->
      Suite.generate(["existing"], dir)
    end
  end

  test "raises error on missing name argument" do
    assert_raise Mix.Error, ~r/Usage/, fn ->
      Suite.run([])
    end
  end

  @tag :tmp_dir
  test "invalid mode raises error", %{tmp_dir: dir} do
    assert_raise Mix.Error, ~r/Invalid mode/, fn ->
      Suite.generate(["bad_mode", "--mode", "invalid"], dir)
    end
  end

  @tag :tmp_dir
  test "prints creation messages", %{tmp_dir: dir} do
    output =
      capture_io(fn ->
        Suite.generate(["msg_test"], dir)
      end)

    assert output =~ "creating"
    assert output =~ "suite.ex"
    assert output =~ "test_example.exs"
    assert output =~ "msg_test"
  end

  @tag :tmp_dir
  test "test_example.exs contains a test block", %{tmp_dir: dir} do
    capture_io(fn ->
      Suite.generate(["content_check"], dir)
    end)

    content = File.read!(Path.join(dir, "suites/content_check/test_example.exs"))
    assert content =~ "test \"server is running\""
    assert content =~ "Client.Admin.version"
  end
end
