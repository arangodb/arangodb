##
# Kernel
#
# ISO 15.3.1
module Kernel

  ##
  # Invoke method +print+ on STDOUT and passing +*args+
  #
  # ISO 15.3.1.2.10
  def print(*args)
    i = 0
    len = args.size
    while i < len
      __printstr__ args[i].to_s
      i += 1
    end
  end

  ##
  # Invoke method +puts+ on STDOUT and passing +*args*+
  #
  # ISO 15.3.1.2.11
  def puts(*args)
    i = 0
    len = args.size
    while i < len
      __printstr__ args[i].to_s
      __printstr__ "\n"
      i += 1
    end
    __printstr__ "\n" if len == 0
  end

  ##
  # Print human readable object description
  #
  # ISO 15.3.1.3.34
  def p(*args)
    i = 0
    len = args.size
    while i < len
      __printstr__ args[i].inspect
      __printstr__ "\n"
      i += 1
    end
    args[0]
  end
end
