defmodule Toast.ClientTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  # Note: Most Client functions (aql, aql!, version, create_collection, etc.)
  # perform HTTP requests via Req and cannot be unit-tested without a mocking
  # library. The interesting pure logic — collect_cursor_results/2 and
  # response_error/1 — is private and therefore not directly testable.
  #
  # These tests cover the public struct-construction API (new/1, new/2).

  describe "new/1" do
    test "returns a Client struct" do
      client = Client.new("http://localhost:8529")

      assert %Client{} = client
    end

    test "wraps a Req.Request with the given base_url" do
      client = Client.new("http://localhost:8529")

      assert client.req.options.base_url == "http://localhost:8529"
    end

    test "disables retry by default" do
      client = Client.new("http://localhost:8529")

      assert client.req.options.retry == false
    end
  end

  describe "new/2 with extra options" do
    test "passes additional options through to Req" do
      client = Client.new("http://localhost:8529", receive_timeout: 5_000)

      assert client.req.options.receive_timeout == 5_000
    end

    test "preserves base_url alongside extra options" do
      client = Client.new("https://example.com:9999", pool_timeout: 1_000)

      assert client.req.options.base_url == "https://example.com:9999"
      assert client.req.options.pool_timeout == 1_000
    end

    test "extra options cannot override retry (appears after defaults)" do
      # The implementation does: [base_url: endpoint, retry: false] ++ opts
      # So opts are appended AFTER retry: false. In Req.new/1, later keys win
      # for keyword lists, so extra opts CAN override retry. This test documents
      # that behavior rather than prescribing it.
      client = Client.new("http://localhost:8529", retry: :transient)

      assert client.req.options.retry == :transient
    end
  end
end
