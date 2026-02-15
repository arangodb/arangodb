defmodule Toast.Client do
  @moduledoc "Thin REST client for ArangoDB, designed for test assertions."

  defstruct [:req]

  @type t :: %__MODULE__{req: Req.Request.t()}

  @spec new(String.t(), keyword()) :: t()
  def new(endpoint, opts \\ []) do
    req_opts = [base_url: endpoint, retry: false] ++ opts
    %__MODULE__{req: Req.new(req_opts)}
  end

  # --- Version ---

  @spec version(t()) :: {:ok, map()} | {:error, term()}
  def version(%__MODULE__{req: req}) do
    case Req.get(req, url: "/_api/version") do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body}
      {:ok, resp} -> {:error, response_error(resp)}
      {:error, reason} -> {:error, reason}
    end
  end

  # --- AQL ---

  @spec aql(t(), String.t(), map()) :: {:ok, [term()]} | {:error, term()}
  def aql(%__MODULE__{req: req}, query, bind_vars \\ %{}) do
    payload = %{"query" => query, "bindVars" => bind_vars}

    case Req.post(req, url: "/_api/cursor", json: payload) do
      {:ok, %{status: 201, body: body}} ->
        collect_cursor_results(req, body)

      {:ok, resp} ->
        {:error, response_error(resp)}

      {:error, reason} ->
        {:error, reason}
    end
  end

  @spec aql!(t(), String.t(), map()) :: [term()]
  def aql!(client, query, bind_vars \\ %{}) do
    case aql(client, query, bind_vars) do
      {:ok, results} -> results
      {:error, reason} -> raise "AQL query failed: #{inspect(reason)}"
    end
  end

  # --- Collections ---

  @spec create_collection(t(), String.t(), keyword()) :: {:ok, map()} | {:error, term()}
  def create_collection(%__MODULE__{req: req}, name, opts \\ []) do
    type = if Keyword.get(opts, :edge, false), do: 3, else: 2
    payload = %{"name" => name, "type" => type}

    case Req.post(req, url: "/_api/collection", json: payload) do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body}
      {:ok, resp} -> {:error, response_error(resp)}
      {:error, reason} -> {:error, reason}
    end
  end

  @spec drop_collection(t(), String.t()) :: :ok | {:error, term()}
  def drop_collection(%__MODULE__{req: req}, name) do
    case Req.delete(req, url: "/_api/collection/#{name}") do
      {:ok, %{status: status}} when status in 200..299 -> :ok
      # Already gone -- idempotent for on_exit cleanup
      {:ok, %{status: 404}} -> :ok
      {:ok, resp} -> {:error, response_error(resp)}
      {:error, reason} -> {:error, reason}
    end
  end

  @spec list_collections(t(), keyword()) :: {:ok, [map()]} | {:error, term()}
  def list_collections(%__MODULE__{req: req}, opts \\ []) do
    exclude_system = Keyword.get(opts, :exclude_system, false)
    params = if exclude_system, do: [excludeSystem: true], else: []

    case Req.get(req, url: "/_api/collection", params: params) do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body["result"]}
      {:ok, resp} -> {:error, response_error(resp)}
      {:error, reason} -> {:error, reason}
    end
  end

  # --- Documents ---

  @spec insert_document(t(), String.t(), map()) :: {:ok, map()} | {:error, term()}
  def insert_document(%__MODULE__{req: req}, collection, doc) do
    case Req.post(req, url: "/_api/document/#{collection}", json: doc) do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body}
      {:ok, resp} -> {:error, response_error(resp)}
      {:error, reason} -> {:error, reason}
    end
  end

  @spec get_document(t(), String.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def get_document(%__MODULE__{req: req}, collection, key) do
    case Req.get(req, url: "/_api/document/#{collection}/#{key}") do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body}
      {:ok, resp} -> {:error, response_error(resp)}
      {:error, reason} -> {:error, reason}
    end
  end

  @spec remove_document(t(), String.t(), String.t()) :: :ok | {:error, term()}
  def remove_document(%__MODULE__{req: req}, collection, key) do
    case Req.delete(req, url: "/_api/document/#{collection}/#{key}") do
      {:ok, %{status: status}} when status in 200..299 -> :ok
      {:ok, resp} -> {:error, response_error(resp)}
      {:error, reason} -> {:error, reason}
    end
  end

  # --- Internal ---

  defp collect_cursor_results(req, body) do
    collect_cursor_pages(req, body, [body["result"]])
  end

  defp collect_cursor_pages(req, %{"hasMore" => true, "id" => cursor_id}, pages_acc) do
    case Req.put(req, url: "/_api/cursor/#{cursor_id}") do
      {:ok, %{status: 200, body: next_body}} ->
        collect_cursor_pages(req, next_body, [next_body["result"] | pages_acc])

      {:ok, resp} ->
        {:error, response_error(resp)}

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp collect_cursor_pages(_req, _body, pages_acc) do
    {:ok, pages_acc |> Enum.reverse() |> List.flatten()}
  end

  defp response_error(%{status: status, body: body}) do
    %{status: status, body: body}
  end
end
