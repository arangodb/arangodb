defmodule Toast do
  @moduledoc """
  Integration testing framework for ArangoDB.

  Toast manages ArangoDB server deployments (single server and cluster),
  runs tests against them, and provides diagnostics when things go wrong.
  """

  @typedoc "Unix timestamp in microseconds."
  @type timestamp :: integer()

  @doc "Return the current wall-clock time as a `t:timestamp/0`."
  @spec get_timestamp() :: timestamp()
  def get_timestamp, do: :os.system_time(:microsecond)
end
