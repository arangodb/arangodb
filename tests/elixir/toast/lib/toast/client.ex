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

  @doc """
  Sends a request with the given method and URL as-is (no api_version/database prefix).
  Body is JSON-encoded by default; pass `nil` to send no body.
  """
  @spec request(t(), atom(), String.t(), term(), keyword()) ::
          {:ok, Req.Response.t()} | {:error, term()}
  def request(%__MODULE__{} = client, method, url, body \\ nil, opts \\ []) do
    opts = if body != nil, do: [{:json, body} | opts], else: opts
    opts = [{:url, url} | apply_auth(client, opts)]
    Req.request(client.req, [{:method, method} | opts])
  end

  @spec get(t(), String.t(), keyword()) :: {:ok, Req.Response.t()} | {:error, term()}
  def get(%__MODULE__{} = client, path, opts \\ []) do
    request(client, :get, build_url(client, path), nil, opts)
  end

  @spec post(t(), String.t(), term(), keyword()) :: {:ok, Req.Response.t()} | {:error, term()}
  def post(%__MODULE__{} = client, path, body \\ nil, opts \\ []) do
    request(client, :post, build_url(client, path), body, opts)
  end

  @spec put(t(), String.t(), term(), keyword()) :: {:ok, Req.Response.t()} | {:error, term()}
  def put(%__MODULE__{} = client, path, body \\ nil, opts \\ []) do
    request(client, :put, build_url(client, path), body, opts)
  end

  @spec delete(t(), String.t(), keyword()) :: {:ok, Req.Response.t()} | {:error, term()}
  def delete(%__MODULE__{} = client, path, opts \\ []) do
    request(client, :delete, build_url(client, path), nil, opts)
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
        ) :: {:ok, term()} | {:error, term()}
  def unwrap(result, expected_status \\ 200..299)

  def unwrap({:ok, %{status: status, body: body}}, expected)
      when is_integer(expected) and status == expected,
      do: {:ok, body}

  def unwrap({:ok, %{status: status, body: body}}, first..last//1)
      when status >= first and status <= last,
      do: {:ok, body}

  def unwrap({:ok, %{status: status, body: body}}, _expected),
    do: {:error, %{status: status, body: body}}

  def unwrap({:error, _} = err, _expected), do: err

  @spec unwrap_ok(
          {:ok, Req.Response.t()} | {:error, term()},
          Range.t() | integer()
        ) :: :ok | {:error, term()}
  def unwrap_ok(result, expected_status \\ 200..299) do
    case unwrap(result, expected_status) do
      {:ok, _body} -> :ok
      {:error, _} = err -> err
    end
  end

  defp apply_auth(%__MODULE__{auth: nil}, opts), do: opts
  defp apply_auth(%__MODULE__{auth: auth}, opts), do: prepend_header(opts, auth_header(auth))

  defp auth_header({:basic, user, password}),
    do: {"authorization", "Basic " <> Base.encode64("#{user}:#{password}")}

  defp auth_header({:jwt, token}),
    do: {"authorization", "Bearer #{token}"}

  defp prepend_header(opts, header) do
    existing = Keyword.get(opts, :headers, [])
    Keyword.put(opts, :headers, [header | existing])
  end
end
