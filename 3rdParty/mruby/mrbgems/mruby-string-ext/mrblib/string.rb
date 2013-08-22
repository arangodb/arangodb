class String
  def lstrip
    a = 0
    z = self.size - 1
    a += 1 while " \f\n\r\t\v".include?(self[a]) and a <= z
    (z >= 0) ? self[a..z] : ""
  end

  def rstrip
    a = 0
    z = self.size - 1
    z -= 1 while " \f\n\r\t\v\0".include?(self[z]) and a <= z
    (z >= 0) ? self[a..z] : ""
  end

  def strip
    a = 0
    z = self.size - 1
    a += 1 while " \f\n\r\t\v".include?(self[a]) and a <= z
    z -= 1 while " \f\n\r\t\v\0".include?(self[z]) and a <= z
    (z >= 0) ? self[a..z] : ""
  end

  def lstrip!
    s = self.lstrip
    (s == self) ? nil : self.replace(s)
  end

  def rstrip!
    s = self.rstrip
    (s == self) ? nil : self.replace(s)
  end

  def strip!
    s = self.strip
    (s == self) ? nil : self.replace(s)
  end

# call-seq:
#    str.casecmp(other_str)   -> -1, 0, +1 or nil
#
# Case-insensitive version of <code>String#<=></code>.
#
#    "abcdef".casecmp("abcde")     #=> 1
#    "aBcDeF".casecmp("abcdef")    #=> 0
#    "abcdef".casecmp("abcdefg")   #=> -1
#    "abcdef".casecmp("ABCDEF")    #=> 0
#
  def casecmp(str)
    self.downcase <=> str.downcase
  end
end
