defmodule Toast.Client.Admin do
  @moduledoc """
  Administrative operations for ArangoDB (version, status, shutdown).

      {:ok, %{"version" => version}} = Admin.version(client)
      {:ok, %{"operationMode" => mode}} = Admin.status(client)
  """

  alias Toast.Client
  require Client

  @spec version(Client.t()) :: {:ok, map()} | {:error, term()}
  def version(%Client{} = client) do
    client |> Client.get("/_api/version") |> Client.unwrap()
  end

  @spec status(Client.t()) :: {:ok, map()} | {:error, term()}
  def status(%Client{} = client) do
    client |> Client.get("/_admin/status") |> Client.unwrap()
  end

  def version!(%Client{} = client), do: Client.bang!(version(client))
  def status!(%Client{} = client), do: Client.bang!(status(client))
end
