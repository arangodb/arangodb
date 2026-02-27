defmodule Toast.MixProject do
  use Mix.Project

  def project do
    [
      app: :toast,
      version: "0.1.0",
      elixir: "~> 1.19",
      elixirc_paths: elixirc_paths(Mix.env()),
      start_permanent: Mix.env() == :prod,
      deps: deps()
    ]
  end

  defp elixirc_paths(:test), do: ["lib", "test/support"]
  defp elixirc_paths(_), do: ["lib"]

  def application do
    [
      extra_applications: [:logger],
      mod: {Toast.Application, []}
    ]
  end

  defp deps do
    [
      {:req, "~> 0.5"},
      {:erlexec, "~> 2.0"},
      {:joken, "~> 2.6"},
      {:plug, "~> 1.0", only: :test},
      {:credo, "~> 1.7", only: [:dev, :test], runtime: false}
    ]
  end
end
