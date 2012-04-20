#
#  String
#
class String
  # 15.2.10.5.15
  def each_line(&block)
    # expect that str.index accepts an Integer for 1st argument as a byte data
    offset = 0
    while(pos = self.index(0x0a, offset))
      block.call(self[offset, pos + 1 - offset])
      offset = pos + 1
    end
    block.call(self[offset, self.size - offset]) if self.size > offset
    self
  end

  # 15.2.10.5.18
  def gsub(*args, &block)
    unless (args.size == 1 && block) || args.size == 2
      raise ArgumentError, "wrong number of arguments"
    end

    ### *** TODO *** ###
  end

  # 15.2.10.5.19
  def gsub!(*args, &block)
    str = self.gsub(*args, &block)
    if str != self
      self.replace(str)
      self
    else
      nil
    end
  end

  # 15.2.10.5.32
  def scan(reg, &block)
    ### *** TODO *** ###
  end

  # 15.2.10.5.36
  def sub(*args, &block)
    unless (args.size == 1 && block) || args.size == 2
      raise ArgumentError, "wrong number of arguments"
    end

    ### *** TODO *** ###
  end

  # 15.2.10.5.37
  def sub!(*args, &block)
    str = self.sub(*args, &block)
    if str != self
      self.replace(str)
      self
    else
      nil
    end
  end

  def each_char(&block)
    pos = 0
    while(pos < self.size)
      block.call(self[pos])
      pos += 1
    end
    self
  end

  def each_byte(&block)
    bytes = self.unpack("C*")
    pos = 0
    while(pos < bytes.size)
      block.call(bytes[pos])
      pos += 1
    end
    self
  end

  def []=(pos, value)
    b = self[0, pos]
    a = self[pos+1..-1]
    p [b, value, a].join('')
    self.replace([b, value, a].join(''))
  end
end

# include modules
module Comparable; end
class String
  include Comparable
end
