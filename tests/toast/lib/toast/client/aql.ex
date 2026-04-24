defmodule Toast.Client.AQL do
  @moduledoc """
  AQL query execution via the ArangoDB cursor API.

  Automatically follows cursor pagination to collect all result pages.

      {:ok, docs} = AQL.execute(client, "FOR d IN users RETURN d")
      {:ok, docs} = AQL.execute(client, "FOR d IN @@coll FILTER d.age > @age RETURN d",
        %{"@coll" => "users", "age" => 21})
  """

  alias Toast.Client
  require Client

  @spec execute(Client.t(), String.t(), map()) :: {:ok, [term()]} | {:error, term()}
  def execute(%Client{} = client, query, bind_vars \\ %{}) do
    body = %{"query" => query, "bindVars" => bind_vars}

    case client |> Client.post("/_api/cursor", body) |> Client.unwrap(201) do
      {:ok, body} -> collect_cursor_pages(client, body, [body["result"]])
      {:error, _} = err -> err
    end
  end

  @spec execute!(Client.t(), String.t(), map()) :: [term()]
  def execute!(%Client{} = client, query, bind_vars \\ %{}) do
    Client.bang!(execute(client, query, bind_vars))
  end

  defp collect_cursor_pages(client, %{"hasMore" => true, "id" => cursor_id}, acc) do
    case client |> Client.put("/_api/cursor/#{cursor_id}") |> Client.unwrap(200) do
      {:ok, next_body} ->
        collect_cursor_pages(client, next_body, [next_body["result"] | acc])

      {:error, _} = err ->
        Client.delete(client, "/_api/cursor/#{cursor_id}")
        err
    end
  end

  defp collect_cursor_pages(_client, _body, acc) do
    {:ok, acc |> Enum.reverse() |> List.flatten()}
  end
end
