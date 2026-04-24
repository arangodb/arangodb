import Config

# Run exec-port in its own process group so it doesn't receive terminal
# SIGINT. This ensures child process cleanup when the VM terminates.
config :erlexec, portexe: Path.expand("../priv/exec-port-wrapper", __DIR__)

# Global log level: :debug — file handler will get everything.
# Console handler is restricted to :info below.
config :logger, level: :debug

# Console handler: only show info and above, with our custom format.
config :logger, :default_handler, level: :info

config :logger, :default_formatter, format: "$time $metadata[$level] $message\n"
