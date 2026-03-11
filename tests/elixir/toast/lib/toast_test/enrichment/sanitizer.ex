defmodule ToastTest.Enrichment.Sanitizer do
  @moduledoc """
  Read and classify sanitizer report files.

  Detects the sanitizer type from the filename and extracts the file's
  modification time as a timestamp for attribution.
  """

  @type result :: %{content: String.t(), timestamp: DateTime.t(), type: atom()}

  @doc """
  Read a sanitizer log file and return its content, timestamp, and type.
  """
  @spec read(Path.t()) :: {:ok, result()} | {:error, term()}
  def read(path) do
    with {:ok, stat} <- File.stat(path, time: :posix),
         {:ok, content} <- File.read(path) do
      {:ok,
       %{
         content: content,
         timestamp: DateTime.from_unix!(stat.mtime),
         type: detect_type(path)
       }}
    end
  end

  defp detect_type(path) do
    basename = Path.basename(path)

    cond do
      String.starts_with?(basename, "alubsan.log") -> :alubsan
      String.starts_with?(basename, "tsan.log") -> :tsan
      true -> :unknown
    end
  end
end
