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

defmodule Toast.JWT.ProviderTest do
  use ExUnit.Case, async: true

  alias Toast.JWT.Provider

  @secret Base.encode16(:crypto.strong_rand_bytes(32))
  @signer {:hmac, @secret}

  test "get_token/1 mints a fresh superuser token" do
    p = Provider.new(@signer)
    token = Provider.get_token(p)

    joken = Joken.Signer.create("HS256", @secret)
    assert {:ok, claims} = Joken.verify_and_validate(%{}, token, joken)
    assert claims["server_id"] == "toast"
  end

  test "create_token/3 respects lifetime and extra_claims" do
    p = Provider.new(@signer)

    token =
      Provider.create_token(p, :superuser,
        lifetime: 1,
        extra_claims: %{"kid" => "x"}
      )

    joken = Joken.Signer.create("HS256", @secret)
    assert {:ok, claims} = Joken.verify_and_validate(%{}, token, joken)
    assert claims["exp"] - claims["iat"] == 1
    assert claims["kid"] == "x"
  end

  test "Inspect redacts the signer" do
    p = Provider.new(@signer)
    out = inspect(p)
    assert out == "#Toast.JWT.Provider<>"
    refute out =~ @secret
  end
end
