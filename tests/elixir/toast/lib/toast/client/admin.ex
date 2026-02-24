defmodule Toast.Client.Admin do
  alias Toast.Client

  @spec version(Client.t()) :: {:ok, map()} | {:error, term()}
  def version(%Client{} = client) do
    case Client.get(client, "/_api/version") do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body}
      {:ok, resp} -> {:error, %{status: resp.status, body: resp.body}}
      {:error, reason} -> {:error, reason}
    end
  end

  @spec status(Client.t()) :: {:ok, map()} | {:error, term()}
  def status(%Client{} = client) do
    case Client.get(client, "/_admin/status") do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body}
      {:ok, resp} -> {:error, %{status: resp.status, body: resp.body}}
      {:error, reason} -> {:error, reason}
    end
  end
end
