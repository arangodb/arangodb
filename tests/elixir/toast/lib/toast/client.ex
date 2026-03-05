defmodule Toast.Client do
  @moduledoc "Thin REST client for ArangoDB, designed for test use."

  @type auth_t :: {:basic, String.t(), String.t()} | {:jwt, String.t()}

  @type t :: %__MODULE__{
          base_url: String.t(),
          database: String.t() | nil,
          api_version: non_neg_integer() | String.t() | nil,
          auth: auth_t() | nil,
          req: Req.Request.t()
        }

  @enforce_keys [:base_url, :req]
  defstruct [:base_url, :database, :api_version, :auth, :req]

  @spec new(String.t(), keyword()) :: t()
  def new(base_url, opts \\ []) do
    {database, opts} = Keyword.pop(opts, :database)
    {api_version, opts} = Keyword.pop(opts, :api_version)
    {auth, req_opts} = Keyword.pop(opts, :auth)

    req = Req.new([base_url: base_url, retry: false] ++ req_opts)

    %__MODULE__{
      base_url: base_url,
      database: database,
      api_version: api_version,
      auth: auth,
      req: req
    }
  end

  @spec with_database(t(), String.t() | nil) :: t()
  def with_database(%__MODULE__{} = client, database) do
    %{client | database: database}
  end

  @spec with_auth(t(), auth_t() | nil) :: t()
  def with_auth(%__MODULE__{} = client, auth) do
    %{client | auth: auth}
  end

  @spec with_api_version(t(), non_neg_integer() | String.t() | nil) :: t()
  def with_api_version(%__MODULE__{} = client, version) do
    %{client | api_version: version}
  end

  @spec get(t(), String.t(), keyword()) :: {:ok, Req.Response.t()} | {:error, term()}
  def get(%__MODULE__{} = client, path, opts \\ []) do
    request(client, :get, path, opts)
  end

  def post(client, path, body \\ nil, opts \\ [])

  @spec post(t(), String.t(), term(), keyword()) :: {:ok, Req.Response.t()} | {:error, term()}
  def post(%__MODULE__{} = client, path, body, opts) do
    opts = if body != nil, do: [{:json, body} | opts], else: opts
    request(client, :post, path, opts)
  end

  def put(client, path, body \\ nil, opts \\ [])

  @spec put(t(), String.t(), term(), keyword()) :: {:ok, Req.Response.t()} | {:error, term()}
  def put(%__MODULE__{} = client, path, body, opts) do
    opts = if body != nil, do: [{:json, body} | opts], else: opts
    request(client, :put, path, opts)
  end

  @spec delete(t(), String.t(), keyword()) :: {:ok, Req.Response.t()} | {:error, term()}
  def delete(%__MODULE__{} = client, path, opts \\ []) do
    request(client, :delete, path, opts)
  end

  defp request(%__MODULE__{} = client, method, path, opts) do
    url = build_url(client, path)
    opts = [{:url, url} | apply_auth(client, opts)]
    Req.request(client.req, [method: method] ++ opts)
  end

  defp build_url(%__MODULE__{} = client, path) do
    [
      if(client.api_version, do: version_prefix(client.api_version)),
      if(client.database, do: "/_db/#{client.database}"),
      path
    ]
    |> Toast.Utils.compact_join()
  end

  defp version_prefix(version) when is_integer(version), do: "/_arango/v#{version}"
  defp version_prefix(version) when is_binary(version), do: "/_arango/#{version}"

  @spec unwrap(
          {:ok, Req.Response.t()} | {:error, term()},
          Range.t() | integer()
        ) :: {:ok, map()} | :ok | {:error, term()}
  def unwrap(result, expected_status \\ 200..299)

  def unwrap({:ok, %{status: status, body: body}}, expected)
      when is_integer(expected) and status == expected,
      do: {:ok, body}

  def unwrap({:ok, %{status: status, body: body}}, %Range{} = expected) do
    if status in expected, do: {:ok, body}, else: {:error, %{status: status, body: body}}
  end

  def unwrap({:ok, resp}, _expected), do: {:error, %{status: resp.status, body: resp.body}}
  def unwrap({:error, _} = err, _expected), do: err

  @spec unwrap_ok(
          {:ok, Req.Response.t()} | {:error, term()},
          Range.t() | integer()
        ) :: :ok | {:error, term()}
  def unwrap_ok(result, expected_status \\ 200..299)

  def unwrap_ok({:ok, %{status: status}}, expected)
      when is_integer(expected) and status == expected,
      do: :ok

  def unwrap_ok({:ok, %{status: status}}, %Range{} = expected) do
    if status in expected, do: :ok, else: {:error, %{status: status}}
  end

  def unwrap_ok({:ok, resp}, _expected), do: {:error, %{status: resp.status, body: resp.body}}
  def unwrap_ok({:error, _} = err, _expected), do: err

  defp apply_auth(%__MODULE__{auth: nil}, opts), do: opts

  defp apply_auth(%__MODULE__{auth: auth}, opts) do
    header =
      case auth do
        {:basic, user, password} ->
          {"authorization", "Basic " <> Base.encode64("#{user}:#{password}")}

        {:jwt, token} ->
          {"authorization", "Bearer #{token}"}
      end

    existing = Keyword.get(opts, :headers, [])
    Keyword.put(opts, :headers, [header | existing])
  end
end
