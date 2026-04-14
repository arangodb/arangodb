defmodule Toast.JWT do
  @moduledoc """
  Mint JWT tokens accepted by ArangoDB.

  Two token kinds:

  - `:superuser` — carries `server_id`; bypasses user permission checks. Used
    for intra-cluster communication and admin APIs that accept any signed
    token regardless of user identity.
  - `{:user, username}` — carries `preferred_username`; subject to ArangoDB's
    user/permission model.

  Two signing algorithms, driven by the `signer` tuple:

  - `{:hmac, secret}` → HS256 with a symmetric shared secret
  - `{:ecdsa, pem}`   → ES256 with a PEM-encoded EC P-256 private key

  Encoding failure is treated as a programming error and raises.
  """

  @type signer :: {:hmac, binary()} | {:ecdsa, binary()}

  @type token_kind :: :superuser | {:user, String.t()}

  @type token_opt ::
          {:lifetime, pos_integer()}
          | {:extra_claims, map()}

  @default_lifetime 3600
  @iss "arangodb"
  @superuser_server_id "toast"

  @doc """
  Mint a JWT of the given kind, signed by `signer`.

  Options:
    * `:lifetime` — seconds until `exp`; default 3600 (matches server default)
    * `:extra_claims` — map merged into the payload, wins on conflict. Used
      for negative tests that need to tamper with standard claims.
  """
  @spec mint(signer() | Joken.Signer.t(), token_kind(), [token_opt()]) :: binary()
  def mint(signer, kind, opts \\ [])

  def mint(%Joken.Signer{} = joken, kind, opts), do: do_mint(joken, kind, opts)
  def mint(signer, kind, opts), do: do_mint(build_signer(signer), kind, opts)

  @doc """
  Build a `Joken.Signer` once for repeated use. Cheap for HS256, but ES256
  involves PEM parsing, so callers that mint many tokens (per-request, per
  health check) should pre-build once and pass the result to `mint/3`.
  """
  @spec build_signer(signer()) :: Joken.Signer.t()
  def build_signer({:hmac, secret}) when is_binary(secret),
    do: Joken.Signer.create("HS256", secret)

  def build_signer({:ecdsa, pem}) when is_binary(pem),
    do: Joken.Signer.create("ES256", %{"pem" => pem})

  defp do_mint(%Joken.Signer{} = joken, kind, opts) do
    now = System.system_time(:second)
    lifetime = Keyword.get(opts, :lifetime, @default_lifetime)
    extra = Keyword.get(opts, :extra_claims, %{})

    claims =
      %{"iss" => @iss, "iat" => now, "exp" => now + lifetime}
      |> Map.merge(kind_claims(kind))
      |> Map.merge(extra)

    case Joken.generate_and_sign(%{}, claims, joken) do
      {:ok, token, _claims} -> token
      {:error, reason} -> raise "Toast.JWT.mint failed: #{inspect(reason)}"
    end
  end

  defp kind_claims(:superuser), do: %{"server_id" => @superuser_server_id}

  defp kind_claims({:user, username}) when is_binary(username),
    do: %{"preferred_username" => username}
end
