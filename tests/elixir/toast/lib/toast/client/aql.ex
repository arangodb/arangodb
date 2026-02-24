defmodule Toast.Client.AQL do
  alias Toast.Client

  @spec execute(Client.t(), String.t(), map()) :: {:ok, [term()]} | {:error, term()}
  def execute(%Client{} = client, query, bind_vars \\ %{}) do
    body = %{"query" => query, "bindVars" => bind_vars}

    case Client.post(client, "/_api/cursor", body) do
      {:ok, %{status: 201, body: body}} ->
        collect_cursor_results(client, body)

      {:ok, resp} ->
        {:error, %{status: resp.status, body: resp.body}}

      {:error, reason} ->
        {:error, reason}
    end
  end

  @spec execute!(Client.t(), String.t(), map()) :: [term()]
  def execute!(%Client{} = client, query, bind_vars \\ %{}) do
    case execute(client, query, bind_vars) do
      {:ok, results} -> results
      {:error, reason} -> raise "AQL query failed: #{inspect(reason)}"
    end
  end

  defp collect_cursor_results(client, body) do
    collect_cursor_pages(client, body, [body["result"]])
  end

  defp collect_cursor_pages(client, %{"hasMore" => true, "id" => cursor_id}, acc) do
    case Client.put(client, "/_api/cursor/#{cursor_id}") do
      {:ok, %{status: 200, body: next_body}} ->
        collect_cursor_pages(client, next_body, [next_body["result"] | acc])

      {:ok, resp} ->
        {:error, %{status: resp.status, body: resp.body}}

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp collect_cursor_pages(_client, _body, acc) do
    {:ok, acc |> Enum.reverse() |> List.flatten()}
  end
end
