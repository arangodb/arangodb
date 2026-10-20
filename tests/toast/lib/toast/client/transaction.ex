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

defmodule Toast.Client.Transaction do
  @moduledoc """
  Stream transaction lifecycle for ArangoDB.

      # Managed transaction with automatic commit/abort
      {:ok, result} = Transaction.run(client, %{write: ["users"]}, fn trx ->
        {:ok, _} = Document.insert(trx, "users", %{name: "Alice"})
        {:ok, :done}
      end)

      # Manual transaction control
      trx_id = Transaction.begin!(client, %{write: ["users"]})
      trx = Transaction.bind(client, trx_id)
      Document.insert!(trx, "users", %{name: "Bob"})
      Transaction.commit!(client, trx_id)
  """

  alias Toast.Client
  require Client
  import Toast.Client.Utils, only: [translate_opts: 2]

  @body_keys %{
    allow_implicit: :allowImplicit,
    wait_for_sync: :waitForSync,
    lock_timeout: :lockTimeout,
    max_transaction_size: :maxTransactionSize
  }

  @spec list(Client.t()) :: {:ok, [map()]} | {:error, term()}
  def list(%Client{} = client) do
    with {:ok, body} <- client |> Client.get("/_api/transaction") |> Client.unwrap() do
      {:ok, body["transactions"]}
    end
  end

  @doc """
  Begins a stream transaction for the given `collections` map.

  ## Options

    * `:allow_implicit` — allow reading from undeclared collections
    * `:wait_for_sync` — wait for fsync before returning
    * `:lock_timeout` — timeout in seconds for acquiring collection locks
    * `:max_transaction_size` — max transaction size in bytes
  """
  @spec begin(Client.t(), map(), keyword()) :: {:ok, String.t()} | {:error, term()}
  def begin(%Client{} = client, collections, opts \\ []) do
    body = Map.merge(%{collections: collections}, to_body(opts))

    with {:ok, body} <-
           client |> Client.post("/_api/transaction/begin", body) |> Client.unwrap(201) do
      {:ok, body["result"]["id"]}
    end
  end

  @spec status(Client.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def status(%Client{} = client, id) do
    with {:ok, body} <- client |> Client.get("/_api/transaction/#{id}") |> Client.unwrap() do
      {:ok, body["result"]}
    end
  end

  @spec commit(Client.t(), String.t()) :: :ok | {:error, term()}
  def commit(%Client{} = client, id) do
    client |> Client.put("/_api/transaction/#{id}") |> Client.unwrap_ok()
  end

  @spec abort(Client.t(), String.t()) :: :ok | {:error, term()}
  def abort(%Client{} = client, id) do
    client |> Client.delete("/_api/transaction/#{id}") |> Client.unwrap_ok()
  end

  @spec bind(Client.t(), String.t()) :: Client.t()
  def bind(%Client{} = client, trx_id) do
    %{client | trx_id: trx_id}
  end

  @doc """
  Runs a function inside a stream transaction, committing on `{:ok, _}` and
  aborting on `{:error, _}` or exception. Accepts the same options as `begin/3`.
  """
  @spec run(Client.t(), map(), keyword() | (Client.t() -> term()), (Client.t() -> term()) | nil) ::
          {:ok, term()} | {:error, term()}
  def run(client, collections, opts_or_fun, fun \\ nil)

  def run(%Client{} = client, collections, fun, nil) when is_function(fun, 1) do
    run(client, collections, [], fun)
  end

  def run(%Client{} = client, collections, opts, fun)
      when is_list(opts) and is_function(fun, 1) do
    case __MODULE__.begin(client, collections, opts) do
      {:ok, trx_id} ->
        trx_client = bind(client, trx_id)
        execute_run(client, trx_id, trx_client, fun)

      {:error, _} = err ->
        err
    end
  end

  def list!(%Client{} = client), do: Client.bang!(list(client))

  def begin!(%Client{} = client, collections, opts \\ []),
    do: Client.bang!(__MODULE__.begin(client, collections, opts))

  def status!(%Client{} = client, id), do: Client.bang!(status(client, id))
  def commit!(%Client{} = client, id), do: Client.bang!(commit(client, id))
  def abort!(%Client{} = client, id), do: Client.bang!(abort(client, id))

  def run!(client, collections, opts_or_fun, fun \\ nil)

  def run!(%Client{} = client, collections, fun, nil) when is_function(fun, 1) do
    run!(client, collections, [], fun)
  end

  def run!(%Client{} = client, collections, opts, fun)
      when is_list(opts) and is_function(fun, 1) do
    Client.bang!(run(client, collections, opts, fun))
  end

  defp execute_run(client, trx_id, trx_client, fun) do
    result =
      try do
        {:returned, fun.(trx_client)}
      rescue
        e -> {:raised, e, __STACKTRACE__}
      end

    case result do
      {:returned, {:ok, value}} ->
        case commit(client, trx_id) do
          :ok ->
            {:ok, value}

          {:error, reason} ->
            abort(client, trx_id)
            {:error, %{phase: :commit, reason: reason}}
        end

      {:returned, {:error, reason}} ->
        abort(client, trx_id)
        {:error, reason}

      {:returned, other} ->
        abort(client, trx_id)
        raise ArgumentError, "expected {:ok, _} or {:error, _}, got: #{inspect(other)}"

      {:raised, exception, stacktrace} ->
        abort(client, trx_id)
        reraise exception, stacktrace
    end
  end

  defp to_body(opts), do: translate_opts(opts, @body_keys) |> Map.new()
end
