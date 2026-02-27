defmodule Toast.Client.AQL do
  @moduledoc "AQL query execution via the ArangoDB cursor API."

  alias Toast.Client

  @spec execute(Client.t(), String.t(), map()) :: {:ok, [term()]} | {:error, term()}
  def execute(%Client{} = client, query, bind_vars \\ %{}) do
    body = %{"query" => query, "bindVars" => bind_vars}

    case client |> Client.post("/_api/cursor", body) |> Client.unwrap(201) do
      {:ok, body} -> collect_cursor_results(client, body)
      {:error, _} = err -> err
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
    case client |> Client.put("/_api/cursor/#{cursor_id}") |> Client.unwrap(200) do
      {:ok, next_body} ->
        collect_cursor_pages(client, next_body, [next_body["result"] | acc])

      {:error, _} = err ->
        err
    end
  end

  defp collect_cursor_pages(_client, _body, acc) do
    {:ok, acc |> Enum.reverse() |> List.flatten()}
  end
end
