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

defmodule Toast.Utils.Compression do
  @moduledoc """
  File compression utilities using zstd (preferred) or gzip fallback.
  """

  @doc "Check if zstd is available on the system."
  @spec zstd_available?() :: boolean()
  def zstd_available?, do: System.find_executable("zstd") != nil

  @doc "Check if gzip is available on the system."
  @spec gzip_available?() :: boolean()
  def gzip_available?, do: System.find_executable("gzip") != nil

  @doc "Detect the best available compression tool."
  @spec detect_tool() :: :zstd | :gzip | nil
  def detect_tool do
    cond do
      zstd_available?() -> :zstd
      gzip_available?() -> :gzip
      true -> nil
    end
  end

  @doc """
  Compress a file with zstd, falling back to gzip.

  Returns `{:ok, dest}` on success or `{:error, reason}` on failure.
  """
  @spec compress_file(Path.t(), Path.t()) :: {:ok, Path.t()} | {:error, term()}
  def compress_file(source, dest) do
    cond do
      not File.exists?(source) -> {:error, :enoent}
      zstd_available?() -> compress_with_zstd(source, dest)
      gzip_available?() -> compress_with_gzip(source, dest)
      true -> {:error, :no_compression_tool}
    end
  end

  @doc "Compress a file using zstd."
  @spec compress_with_zstd(Path.t(), Path.t()) :: {:ok, Path.t()} | {:error, term()}
  def compress_with_zstd(source, dest) do
    case System.cmd("zstd", ["-q", "-f", "-o", dest, source], stderr_to_stdout: true) do
      {_, 0} -> {:ok, dest}
      {output, _} -> {:error, {:zstd_failed, output}}
    end
  end

  @doc "Compress a file using gzip."
  @spec compress_with_gzip(Path.t(), Path.t()) :: {:ok, Path.t()} | {:error, term()}
  def compress_with_gzip(source, dest) do
    case System.cmd("gzip", ["-c", source], into: File.stream!(dest)) do
      {_, 0} ->
        {:ok, dest}

      {_, code} ->
        File.rm(dest)
        {:error, {:gzip_failed, code}}
    end
  end
end
