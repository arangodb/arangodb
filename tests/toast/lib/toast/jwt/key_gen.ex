defmodule Toast.JWT.KeyGen do
  @moduledoc """
  Generate JWT signing material and write the ArangoDB keyfile.

  Sole owner of the `<deployment_dir>/jwt-secret` path convention — callers
  should use `keyfile_path/1` rather than re-deriving the path.

  The ArangoDB server auto-detects the algorithm from keyfile content:

    * PEM format → ES256
    * anything else (plain text) → HS256

  No extra server flag is required.
  """

  @keyfile_name "jwt-secret"
  @p256_oid {1, 2, 840, 10045, 3, 1, 7}

  @doc "Path to the keyfile inside `deployment_dir`."
  @spec keyfile_path(Path.t()) :: String.t()
  def keyfile_path(deployment_dir), do: Path.join(deployment_dir, @keyfile_name)

  @doc """
  Generate JWT signing material for the given algorithm and write the keyfile.

  Returns `{signer, keyfile_path}` where `signer` is the tuple accepted by
  `Toast.JWT.mint/3`.
  """
  @spec generate(:hmac | :ecdsa, Path.t()) :: {Toast.JWT.signer(), String.t()}
  def generate(:hmac, deployment_dir) do
    # Hex (not raw bytes) for two reasons: guaranteed UTF-8 for safe keyfile
    # writes, and 64 characters fits within the server's kMaxSecretLength.
    hex = :crypto.strong_rand_bytes(32) |> Base.encode16()
    path = keyfile_path(deployment_dir)
    File.write!(path, hex)
    {{:hmac, hex}, path}
  end

  def generate(:ecdsa, deployment_dir) do
    rec = :public_key.generate_key({:namedCurve, @p256_oid})
    entry = :public_key.pem_entry_encode(:ECPrivateKey, rec)
    pem = :public_key.pem_encode([entry])
    path = keyfile_path(deployment_dir)
    File.write!(path, pem)
    {{:ecdsa, pem}, path}
  end
end
