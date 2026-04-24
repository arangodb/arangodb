defmodule Toast.JWTTest do
  use ExUnit.Case, async: true

  alias Toast.JWT

  @hmac_secret Base.encode16(:crypto.strong_rand_bytes(32))

  defp ecdsa_pem do
    dir = Path.join(System.tmp_dir!(), "toast-jwt-test-#{System.unique_integer([:positive])}")
    File.mkdir_p!(dir)
    {{:ecdsa, pem}, _path} = Toast.JWT.KeyGen.generate(:ecdsa, dir)
    File.rm_rf(dir)
    pem
  end

  describe "mint/3 HS256" do
    test "mints a valid superuser token decodable by Joken with the same secret" do
      token = JWT.mint({:hmac, @hmac_secret}, :superuser)

      signer = Joken.Signer.create("HS256", @hmac_secret)
      {:ok, claims} = Joken.verify_and_validate(%{}, token, signer)

      assert claims["iss"] == "arangodb"
      assert claims["server_id"] == "toast"
      assert is_integer(claims["iat"])
      assert is_integer(claims["exp"])
      assert claims["exp"] > claims["iat"]
      # default lifetime ~3600
      assert_in_delta claims["exp"] - claims["iat"], 3600, 5
    end

    test "mints a user token with preferred_username claim" do
      token = JWT.mint({:hmac, @hmac_secret}, {:user, "alice"})

      signer = Joken.Signer.create("HS256", @hmac_secret)
      {:ok, claims} = Joken.verify_and_validate(%{}, token, signer)

      assert claims["iss"] == "arangodb"
      assert claims["preferred_username"] == "alice"
      refute Map.has_key?(claims, "server_id")
    end

    test "lifetime option shrinks exp" do
      token = JWT.mint({:hmac, @hmac_secret}, :superuser, lifetime: 1)

      signer = Joken.Signer.create("HS256", @hmac_secret)
      {:ok, claims} = Joken.verify_and_validate(%{}, token, signer)

      assert claims["exp"] - claims["iat"] == 1
    end

    test "extra_claims merge into payload" do
      token =
        JWT.mint({:hmac, @hmac_secret}, :superuser,
          extra_claims: %{"iss" => "wrong", "kid" => "x"}
        )

      signer = Joken.Signer.create("HS256", @hmac_secret)
      {:ok, claims} = Joken.verify_and_validate(%{}, token, signer)

      assert claims["iss"] == "wrong"
      assert claims["kid"] == "x"
    end

    test "tokens signed with a different secret do not verify" do
      token = JWT.mint({:hmac, @hmac_secret}, :superuser)
      other = Joken.Signer.create("HS256", Base.encode16(:crypto.strong_rand_bytes(32)))
      assert {:error, _} = Joken.verify_and_validate(%{}, token, other)
    end
  end

  describe "mint/3 ES256" do
    setup do
      {:ok, pem: ecdsa_pem()}
    end

    test "mints a superuser token decodable via ES256", %{pem: pem} do
      token = JWT.mint({:ecdsa, pem}, :superuser)

      signer = Joken.Signer.create("ES256", %{"pem" => pem})
      {:ok, claims} = Joken.verify_and_validate(%{}, token, signer)

      assert claims["iss"] == "arangodb"
      assert claims["server_id"] == "toast"
    end
  end
end
