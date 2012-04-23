#
#  Array
#
class Array
  # 15.2.12.5.10
  def each(&block)
    idx = 0
    while(idx < length)
      block.call(self[idx])
      idx += 1
    end
    self
  end

  # 15.2.12.5.11
  def each_index(&block)
    idx = 0
    while(idx < length)
      block.call(idx)
      idx += 1
    end
    self
  end

  # 15.2.12.5.7
  def collect!(&block)
    self.each_index{|idx|
      self[idx] = block.call(self[idx])
    }
    self
  end

  # 15.2.12.5.20
  # map!(&block)
  alias map! collect!

  # 15.2.12.5.15
  def initialize(size=0, obj=nil, &block)
    raise TypeError, "expected Integer for 1st argument" unless size.kind_of? Integer
    raise ArgumentError, "negative array size" if size < 0

    self.clear
    if size > 0
      self[size - 1] = nil  # allocate

      idx = 0
      while(idx < size)
        self[idx] = (block)? block.call(idx): obj
        idx += 1
      end
    end

    self
  end

  def delete(key, &block)
    while i = self.index(key)
      self.delete_at(i)
      ret = key
    end
    if ret == nil && block
      block.call
    else
      ret
    end
  end
end

# include modules
module Enumerable; end
module Comparable; end
class Array
  include Enumerable
  include Comparable

  def sort!(&block)
    self.replace(self.sort(&block))
  end
end
