defmodule Toast.Client.Admin do
  alias Toast.Client

  @spec version(Client.t()) :: {:ok, map()} | {:error, term()}
  def version(%Client{} = client) do
    client |> Client.get("/_api/version") |> Client.unwrap()
  end

  @spec status(Client.t()) :: {:ok, map()} | {:error, term()}
  def status(%Client{} = client) do
    client |> Client.get("/_admin/status") |> Client.unwrap()
  end
end
