#
#  Struct
#
class Struct
  # 15.2.18.4.4
  def each(&block)
    self.class.members.each{|field|
      block.call(self[field])
    }
    self
  end

  # 15.2.18.4.5
  def each_pair(&block)
    self.class.members.each{|field|
      block.call(field.to_sym, self[field])
    }
    self
  end

  # 15.2.18.4.7
  def select(&block)
    ary = []
    self.class.members.each{|field|
      val = self[field]
      ary.push(val) if block.call(val)
    }
    ary
  end
end
