##
# Kernel
#
# ISO 15.3.1
module Kernel
  def print(*a)
    raise NotImplementedError.new('print not available')
  end
  def puts(*a)
    raise NotImplementedError.new('puts not available')
  end
  def p(*a)
    raise NotImplementedError.new('p not available')
  end
  def printf(*args)
    raise NotImplementedError.new('printf not available')
  end
end
