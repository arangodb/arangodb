defmodule Toast.Diagnostics.Coredump.Debugger do
  @moduledoc "Behaviour for coredump debugger backends."

  @type frame :: %{
          function: String.t(),
          file: String.t() | nil,
          line: integer() | nil
        }

  @type thread :: %{
          id: integer(),
          frames: [frame()]
        }

  @type result :: %{
          signal: String.t() | nil,
          faulting_address: String.t() | nil,
          threads: [thread()],
          crash_thread: integer() | nil
        }

  @doc "Return the debugger executable name."
  @callback executable() :: String.t()

  @doc "Build the command-line arguments for non-interactive stack trace extraction."
  @callback command(binary_path :: Path.t(), core_path :: Path.t()) :: [String.t()]

  @doc "Parse debugger output into a structured result."
  @callback parse_output(output :: String.t()) :: result()

  # --- Shared thread/frame utilities ---

  @internal_frames ~w(__libc_start_main __libc_start_call_main _start clone start_thread)
  @internal_prefixes ["__libc_", "__GI_"]

  @doc false
  @spec flush_current_thread(map()) :: map()
  def flush_current_thread(%{current: nil} = acc), do: acc

  def flush_current_thread(%{current: current} = acc) do
    thread = %{current | frames: Enum.reverse(current.frames)}
    %{acc | threads: [thread | acc.threads], current: nil}
  end

  @doc false
  @spec filter_threads([thread()], integer() | nil) :: [thread()]
  def filter_threads(threads, crash_thread) do
    Enum.map(threads, fn
      %{id: ^crash_thread} = thread ->
        thread

      thread ->
        %{thread | frames: Enum.reject(thread.frames, &internal_frame?/1)}
    end)
  end

  defp internal_frame?(%{function: func}) do
    func in @internal_frames ||
      Enum.any?(@internal_prefixes, &String.starts_with?(func, &1))
  end
end
