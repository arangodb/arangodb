defmodule Toast.Diagnostics.AgencyDump do
  @moduledoc """
  Capture agency state from a live ArangoDB agent for cluster diagnostics.

  Queries a single responsive agent for configuration, full state, and the
  /arango plan tree. This information is essential for diagnosing cluster
  test failures (shard leadership changes, replication issues, etc.).
  """

  require Logger

  @type t :: %__MODULE__{
          agent_id: String.t(),
          config: map() | nil,
          state: map() | nil,
          plan: map() | nil,
          error: String.t() | nil
        }

  defstruct [:agent_id, :config, :state, :plan, :error]

  @doc """
  Capture agency dump from the first responsive agent.

  Takes a list of agent info maps and queries the first one that responds.
  Returns the dump struct or nil if no agents are available.

  ## Options

    * `:agents` - list of `%{id: String.t(), endpoint: String.t()}` maps
    * `:timeout` - per-request timeout in ms (default: 10_000)
    * `:client_opts` - extra options forwarded to `Toast.Client.new/2` (test use)

  """
  @spec capture(keyword()) :: t() | nil
  def capture(opts) do
    agents = Keyword.fetch!(opts, :agents)
    timeout = Keyword.get(opts, :timeout, 10_000)
    client_opts = Keyword.get(opts, :client_opts, [])

    if agents == [] do
      Logger.warning("AgencyDump: no agents available, skipping dump")
      nil
    else
      try_agents(agents, timeout, client_opts)
    end
  end

  defp try_agents([], _timeout, _client_opts) do
    Logger.warning("AgencyDump: no responsive agents found")
    nil
  end

  defp try_agents([agent | rest], timeout, client_opts) do
    case dump_agent(agent, timeout, client_opts) do
      {:ok, dump} ->
        dump

      {:error, reason} ->
        Logger.warning("AgencyDump: agent #{agent.id} failed: #{inspect(reason)}, trying next")
        try_agents(rest, timeout, client_opts)
    end
  end

  defp dump_agent(agent, timeout, client_opts) do
    client = Toast.Client.new(agent.endpoint, [{:receive_timeout, timeout} | client_opts])

    with {:ok, config} <- fetch_config(client),
         {:ok, state} <- fetch_state(client),
         {:ok, plan} <- fetch_plan(client) do
      {:ok,
       %__MODULE__{
         agent_id: agent.id,
         config: config,
         state: state,
         plan: plan
       }}
    end
  rescue
    e -> {:error, Exception.message(e)}
  end

  defp fetch_config(client) do
    case Toast.Client.get(client, "/_api/agency/config") do
      {:ok, %{status: 200, body: body}} -> {:ok, body}
      {:ok, %{status: status}} -> {:error, "config returned status #{status}"}
      {:error, reason} -> {:error, reason}
    end
  end

  defp fetch_state(client) do
    case Toast.Client.get(client, "/_api/agency/state") do
      {:ok, %{status: 200, body: body}} -> {:ok, body}
      {:ok, %{status: status}} -> {:error, "state returned status #{status}"}
      {:error, reason} -> {:error, reason}
    end
  end

  defp fetch_plan(client) do
    case Toast.Client.post(client, "/_api/agency/read", [["/arango"]]) do
      {:ok, %{status: 200, body: body}} -> {:ok, body}
      {:ok, %{status: status}} -> {:error, "plan returned status #{status}"}
      {:error, reason} -> {:error, reason}
    end
  end
end
