module Kernel
  def print(*args)
    i = 0
    len = args.size
    while i < len
      __printstr__ args[i].to_s
      i += 1
    end
  end
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
end
