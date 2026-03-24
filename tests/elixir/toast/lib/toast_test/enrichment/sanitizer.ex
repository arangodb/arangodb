defmodule ToastTest.Enrichment.Sanitizer do
  @moduledoc """
  Read and classify sanitizer report files.

  Detects the sanitizer type from the filename and extracts the file's
  modification time as a timestamp for attribution.
  """

  @type result :: %{
          content: String.t(),
          timestamp: Toast.timestamp(),
          type: atom(),
          kind: String.t() | nil
        }

  @doc """
  Read a sanitizer log file and return its content, timestamp, type, and kind.

  The `kind` is extracted from the first warning/error line in the report
  (e.g., "data race", "heap-buffer-overflow", "use-after-free").
  """
  @spec read(Path.t()) :: {:ok, result()} | {:error, term()}
  def read(path) do
    with {:ok, timestamp} <- Toast.Utils.Filesystem.file_mtime_us(path),
         {:ok, content} <- File.read(path) do
      {:ok,
       %{
         content: content,
         timestamp: timestamp,
         type: detect_type(path),
         kind: detect_kind(content)
       }}
    end
  end

  @kind_pattern ~r/(?:WARNING|ERROR): \w+Sanitizer: (.+?)(?:\s*\(|$)/m

  defp detect_kind(content) do
    case Regex.run(@kind_pattern, content) do
      [_, kind] -> kind
      _ -> nil
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
