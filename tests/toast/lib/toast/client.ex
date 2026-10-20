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

defmodule Toast.Client do
  @moduledoc "Thin REST client for ArangoDB, designed for test use."

  @type auth_t ::
          {:basic, String.t(), String.t()}
          | {:jwt, String.t()}
          | {:jwt_provider, Toast.JWT.Provider.t()}

  @type content_type_t :: :json | :vpack

  @type protocol_t :: :http1 | :http2

  @type t :: %__MODULE__{
          base_url: String.t(),
          database: String.t() | nil,
          api_version: non_neg_integer() | String.t() | nil,
          auth: auth_t() | nil,
          content_type: content_type_t(),
          protocol: protocol_t(),
          trx_id: String.t() | nil,
          req: Req.Request.t()
        }

  @enforce_keys [:base_url, :req]
  defstruct [
    :base_url,
    :database,
    :api_version,
    :auth,
    :trx_id,
    :req,
    content_type: :vpack,
    protocol: :http1
  ]

  @spec new(String.t(), keyword()) :: t()
  def new(base_url, opts \\ []) do
    {database, opts} = Keyword.pop(opts, :database)
    {api_version, opts} = Keyword.pop(opts, :api_version)
    {auth, opts} = Keyword.pop(opts, :auth)
    {content_type, opts} = Keyword.pop(opts, :content_type, :vpack)
    {protocol, req_opts} = Keyword.pop(opts, :protocol, :http1)

    req_opts =
      req_opts
      |> apply_protocol(protocol)
      |> apply_ssl(base_url)

    req =
      Req.new([base_url: base_url, retry: false] ++ req_opts)
      |> Req.Request.append_response_steps(vpack_decode: &decode_vpack_response/1)

    %__MODULE__{
      base_url: base_url,
      database: database,
      api_version: api_version,
      auth: auth,
      content_type: content_type,
      protocol: protocol,
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

  @spec with_content_type(t(), content_type_t()) :: t()
  def with_content_type(%__MODULE__{} = client, content_type) do
    %{client | content_type: content_type}
  end

  @doc """
  Sends a request with the given method and URL as-is (no api_version/database prefix).
  Body is encoded according to `content_type` (vpack by default); pass `nil` to send no body.
  """
  @spec request(t(), atom(), String.t(), term(), keyword()) ::
          {:ok, Req.Response.t()} | {:error, term()}
  def request(%__MODULE__{} = client, method, url, body \\ nil, opts \\ []) do
    opts = encode_body(client.content_type, body, opts)
    opts = [{:url, url} | apply_auth(client, apply_trx(client, opts))]
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

  @spec patch(t(), String.t(), term(), keyword()) :: {:ok, Req.Response.t()} | {:error, term()}
  def patch(%__MODULE__{} = client, path, body \\ nil, opts \\ []) do
    request(client, :patch, build_url(client, path), body, opts)
  end

  @spec delete(t(), String.t(), keyword()) :: {:ok, Req.Response.t()} | {:error, term()}
  def delete(%__MODULE__{} = client, path, opts \\ []) do
    {body, opts} = Keyword.pop(opts, :body)
    request(client, :delete, build_url(client, path), body, opts)
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

  @spec idempotent_delete(t(), String.t(), keyword()) :: :ok | {:error, term()}
  def idempotent_delete(%__MODULE__{} = client, path, opts \\ []) do
    case delete(client, path, opts) do
      {:ok, %{status: 404}} -> :ok
      other -> unwrap_ok(other)
    end
  end

  @doc false
  defmacro bang!(expr) do
    {fun_name, _arity} = __CALLER__.function
    mod = __CALLER__.module |> Module.split() |> List.last()
    label = "#{mod}.#{fun_name}" |> String.trim_trailing("!")

    quote generated: true do
      case unquote(expr) do
        {:ok, value} -> value
        :ok -> :ok
        {:error, reason} -> raise "#{unquote(label)} failed: #{inspect(reason)}"
      end
    end
  end

  defp apply_trx(%__MODULE__{trx_id: nil}, opts), do: opts
  defp apply_trx(%__MODULE__{trx_id: id}, opts), do: prepend_header(opts, {"x-arango-trx-id", id})

  defp apply_auth(%__MODULE__{auth: nil}, opts), do: opts
  defp apply_auth(%__MODULE__{auth: auth}, opts), do: prepend_header(opts, auth_header(auth))

  defp auth_header({:basic, user, password}),
    do: {"authorization", "Basic " <> Base.encode64("#{user}:#{password}")}

  defp auth_header({:jwt, token}),
    do: {"authorization", "Bearer #{token}"}

  # Resolve the token per-request so a client reused for hours still carries
  # a valid token on every call — the key property for long-running tests.
  defp auth_header({:jwt_provider, provider}), do: Toast.JWT.Provider.auth_header(provider)

  defp prepend_header(opts, header) do
    existing = Keyword.get(opts, :headers, [])
    Keyword.put(opts, :headers, [header | existing])
  end

  defp encode_body(_content_type, nil, opts), do: opts

  defp encode_body(:json, body, opts), do: [{:json, body} | opts]

  defp encode_body(:vpack, body, opts) do
    existing = Keyword.get(opts, :headers, [])

    opts
    |> Keyword.put(:body, VelocyPack.encode!(body))
    |> Keyword.put(:headers, [
      {"content-type", "application/x-velocypack"},
      {"accept", "application/x-velocypack"} | existing
    ])
  end

  @doc false
  @spec protocol_connect_options(protocol_t()) :: keyword()
  def protocol_connect_options(:http1), do: []
  def protocol_connect_options(:http2), do: [connect_options: [protocols: [:http2]]]

  defp apply_protocol(req_opts, protocol) do
    merge_connect_options(req_opts, protocol_connect_options(protocol))
  end

  # Self-signed test certs require disabling certificate verification.
  defp apply_ssl(req_opts, "https://" <> _) do
    merge_connect_options(req_opts, connect_options: [transport_opts: [verify: :verify_none]])
  end

  defp apply_ssl(req_opts, _base_url), do: req_opts

  defp merge_connect_options(req_opts, []), do: req_opts

  defp merge_connect_options(req_opts, new_opts) do
    Keyword.merge(req_opts, new_opts, fn
      :connect_options, existing, new -> Keyword.merge(existing, new)
      _key, _existing, new -> new
    end)
  end

  defp decode_vpack_response({request, response}) do
    content_type = Req.Response.get_header(response, "content-type")

    if match?(["application/x-velocypack" <> _ | _], content_type) do
      {request, %{response | body: VelocyPack.decode!(response.body)}}
    else
      {request, response}
    end
  end

  defimpl Inspect do
    import Inspect.Algebra

    def inspect(client, opts) do
      fields =
        [
          if(client.database, do: {"db", client.database}),
          {"protocol", client.protocol},
          {"content_type", client.content_type},
          if(client.auth, do: {"auth", auth_label(client.auth)}),
          if(client.trx_id, do: {"trx_id", client.trx_id})
        ]
        |> Toast.Utils.compact()
        |> Enum.map(fn {k, v} -> concat([k, ": ", to_doc(v, opts)]) end)

      inner = fold_doc([client.base_url | fields], &glue/2)
      concat(["#Toast.Client<", nest(inner, 2), ">"])
    end

    defp auth_label({kind, _}), do: kind
    defp auth_label({kind, _, _}), do: kind
  end
end
