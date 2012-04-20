#
#  Hash
#
class Hash
  # 15.2.13.4.8
  def delete(key, &block)
    if block && ! self.has_key?(key)
      block.call(key)
    else
      self.__delete(key)
    end
  end

  # 15.2.13.4.9
  def each(&block)
    self.keys.each{|k| block.call([k, self[k]])}
    self
  end

  # 15.2.13.4.10
  def each_key(&block)
    self.keys.each{|k| block.call(k)}
    self
  end

  # 15.2.13.4.11
  def each_value(&block)
    self.keys.each{|k| block.call(self[k])}
    self
  end

  # 15.2.13.4.16
  def initialize(*args, &block)
    self.__init_core(block, *args)
  end

  # 15.2.13.4.22
  def merge(other, &block)
    h = {}
    raise "can't convert argument into Hash" unless other.respond_to?(:to_hash)
    other = other.to_hash
    self.each_key{|k| h[k] = self[k]}
    if block
      other.each_key{|k|
        h[k] = (self.has_key?(k))? block.call(k, self[k], other[k]): other[k]
      }
    else
      other.each_key{|k| h[k] = other[k]}
    end
    h
  end
end

# include modules
module Enumerable; end
class Hash
  include Enumerable
end
