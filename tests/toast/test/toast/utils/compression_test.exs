################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule Toast.Utils.CompressionTest do
  use ExUnit.Case, async: true

  alias Toast.Utils.Compression

  describe "zstd_available?/0" do
    test "returns a boolean" do
      assert is_boolean(Compression.zstd_available?())
    end
  end

  describe "gzip_available?/0" do
    test "returns a boolean" do
      assert is_boolean(Compression.gzip_available?())
    end
  end

  describe "detect_tool/0" do
    test "returns :zstd, :gzip, or nil" do
      assert Compression.detect_tool() in [:zstd, :gzip, nil]
    end
  end

  describe "compress_file/2" do
    @tag :tmp_dir
    test "compresses a file", %{tmp_dir: tmp_dir} do
      source = Path.join(tmp_dir, "test.txt")
      File.write!(source, String.duplicate("hello world\n", 1000))
      dest = Path.join(tmp_dir, "test.txt.compressed")

      assert {:ok, compressed_path} = Compression.compress_file(source, dest)
      assert File.exists?(compressed_path)
      assert File.stat!(compressed_path).size < File.stat!(source).size
    end

    @tag :tmp_dir
    test "returns error for nonexistent source", %{tmp_dir: tmp_dir} do
      source = Path.join(tmp_dir, "nonexistent.txt")
      dest = Path.join(tmp_dir, "out.compressed")

      assert {:error, :enoent} = Compression.compress_file(source, dest)
    end
  end
end
