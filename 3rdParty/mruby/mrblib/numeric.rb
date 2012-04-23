#
#  Integer
#
class Integer
  # 15.2.8.3.15
  def downto(num, &block)
    raise TypeError, "expected Integer" unless num.kind_of? Integer
    i = self
    while(i >= num)
      block.call(i)
      i -= 1
    end
    self
  end

  # 15.2.8.3.22
  def times(&block)
    i = 0
    while(i < self)
      block.call(i)
      i += 1
    end
    self
  end

  # 15.2.8.3.27
  def upto(num, &block)
    raise TypeError, "expected Integer" unless num.kind_of? Integer
    i = self
    while(i <= num)
      block.call(i)
      i += 1
    end
    self
  end
end

# include modules
module Comparable; end
class Numeric
  include Comparable
end
