defmodule Toast.JWT.KeyGenTest do
  use ExUnit.Case, async: true

  alias Toast.JWT
  alias Toast.JWT.KeyGen

  setup do
    dir = Path.join(System.tmp_dir!(), "toast-keygen-#{System.unique_integer([:positive])}")
    File.mkdir_p!(dir)
    on_exit(fn -> File.rm_rf(dir) end)
    %{dir: dir}
  end

  describe "generate/2 :hmac" do
    test "writes a 64-char hex secret to the keyfile", %{dir: dir} do
      {{:hmac, hex}, path} = KeyGen.generate(:hmac, dir)

      assert path == KeyGen.keyfile_path(dir)
      assert File.exists?(path)
      assert File.read!(path) == hex
      assert byte_size(hex) == 64
      assert hex =~ ~r/^[0-9A-F]{64}$/
    end

    test "generated secret produces tokens verifiable by Joken HS256", %{dir: dir} do
      {{:hmac, _hex} = signer, _path} = KeyGen.generate(:hmac, dir)

      token = JWT.mint(signer, :superuser)
      {_, secret} = signer
      joken = Joken.Signer.create("HS256", secret)
      assert {:ok, claims} = Joken.verify_and_validate(%{}, token, joken)
      assert claims["iss"] == "arangodb"
    end
  end

  describe "generate/2 :ecdsa" do
    test "writes a PEM-encoded private key to the keyfile", %{dir: dir} do
      {{:ecdsa, pem}, path} = KeyGen.generate(:ecdsa, dir)

      assert path == KeyGen.keyfile_path(dir)
      assert File.exists?(path)
      assert File.read!(path) == pem
      assert String.starts_with?(pem, "-----BEGIN EC PRIVATE KEY-----")
      assert String.contains?(pem, "-----END EC PRIVATE KEY-----")
    end

    test "generated PEM produces tokens verifiable by Joken ES256", %{dir: dir} do
      {{:ecdsa, pem} = signer, _path} = KeyGen.generate(:ecdsa, dir)

      token = JWT.mint(signer, :superuser)
      joken = Joken.Signer.create("ES256", %{"pem" => pem})
      assert {:ok, claims} = Joken.verify_and_validate(%{}, token, joken)
      assert claims["server_id"] == "toast"
    end
  end
end
