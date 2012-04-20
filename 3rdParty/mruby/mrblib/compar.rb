### move to compar.c
# module Comparable
  # def == other
  #   cmp = self <=> other
  #   if cmp == 0
  #     true
  #   else
  #     false
  #   end
  # end

  # def < other
  #   cmp = self <=> other
  #   if cmp.nil?
  #     false
  #   elsif cmp < 0
  #     true
  #   else
  #     false
  #   end
  # end

  # def <= other
  #   cmp = self <=> other
  #   if cmp.nil?
  #     false
  #   elsif cmp <= 0
  #     true
  #   else
  #     false
  #   end
  # end

  # def > other
  #   cmp = self <=> other
  #   if cmp.nil?
  #     false
  #   elsif cmp > 0
  #     true
  #   else
  #     false
  #   end
  # end

  # def >= other
  #   cmp = self <=> other
  #   if cmp.nil?
  #     false
  #   elsif cmp >= 0
  #     true
  #   else
  #     false
  #   end
  # end

  # def between?(min,max)
  #   if self < min or self > max
  #     false
  #   else
  #     true
  #   end
  # end
# end
