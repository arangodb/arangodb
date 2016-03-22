std::ostream& operator<<(std::ostream& stream, LogLevel level) {
  stream << Logger::translateLogLevel(level);
  return stream;
}

