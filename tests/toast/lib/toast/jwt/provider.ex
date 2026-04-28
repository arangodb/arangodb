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

defmodule Toast.JWT.Provider do
  @moduledoc """
  A plain data struct holding a JWT signer. **Not a process.**

  Why no GenServer / cache / refresh timer: minting is microseconds (HS256) or
  a fraction of a millisecond (ES256), negligible next to any network round
  trip. A cache would buy nothing measurable but cost expiry logic, timer
  edge cases (especially with short `lifetime` in expiry tests), a process
  lifecycle, defensive `catch :exit` wrapping on the `Deployment` struct, and
  an inter-process message in the `build_client` hot path.

  The `Inspect` impl redacts the signer so stray `inspect/1` calls (logging,
  telemetry, error messages) cannot leak the secret.
  """

  alias Toast.JWT

  @enforce_keys [:joken_signer]
  defstruct [:joken_signer]

  @type t :: %__MODULE__{joken_signer: Joken.Signer.t()}

  @spec new(JWT.signer()) :: t()
  def new(signer), do: %__MODULE__{joken_signer: JWT.build_signer(signer)}

  @doc """
  Mint a fresh superuser token. Called per-request by the transport layer, so
  long-lived clients never hold a stale token.
  """
  @spec get_token(t()) :: binary()
  def get_token(%__MODULE__{joken_signer: js}), do: JWT.mint(js, :superuser)

  @doc """
  Mint a fresh token with the given kind and opts. Used for test-specific
  tokens (custom lifetimes, extra claims, user tokens).
  """
  @spec create_token(t(), JWT.token_kind(), [JWT.token_opt()]) :: binary()
  def create_token(%__MODULE__{joken_signer: js}, kind, opts \\ []),
    do: JWT.mint(js, kind, opts)

  @doc "Returns `{:jwt, token}` or `nil` — the canonical auth tuple shape."
  @spec maybe_auth(t() | nil) :: {:jwt, binary()} | nil
  def maybe_auth(nil), do: nil
  def maybe_auth(%__MODULE__{} = p), do: {:jwt, get_token(p)}

  @doc "Returns an `{\"authorization\", \"Bearer ...\"}` header tuple, or `nil`."
  @spec auth_header(t() | nil) :: {String.t(), String.t()} | nil
  def auth_header(nil), do: nil
  def auth_header(%__MODULE__{} = p), do: {"authorization", "Bearer " <> get_token(p)}

  defimpl Inspect do
    def inspect(_, _), do: "#Toast.JWT.Provider<>"
  end
end
