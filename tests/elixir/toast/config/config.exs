import Config

# Global log level: :debug — file handler will get everything.
# Console handler is restricted to :info below.
config :logger, level: :debug

# Console handler: only show info and above, with our custom format.
config :logger, :default_handler, level: :info

config :logger, :default_formatter, format: "$time $metadata[$level] $message\n"
