
-- Copyright (C) 2008-2018 Lorenzo Caminiti
-- Distributed under the Boost Software License, Version 1.0 (see accompanying
-- file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
-- See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[meyer97_stack4_e
-- Extra spaces, newlines, etc. for visual alignment with this library code.





indexing
    destription: "Dispenser with LIFO access policy and a fixed max capacity."
class interface STACK4[G] creation make -- Interface only (no implementation).







invariant
    count_non_negative: count >= 0
    count_bounded: count <= capacity
    empty_if_no_elements: empty = (count = 0)



feature -- Initialization

    -- Allocate stack for a maximum of n elements.
    make(n: INTEGER) is
        require
            non_negative_capacity: n >= 0
        ensure
            capacity_set: capacity = n
        end



















































feature -- Access

    -- Max number of stack elements.
    capacity: INTEGER

    
    
    
    
    -- Number of stack elements.
    count: INTEGER

    
    
    
    
    -- Top element.
    item: G is
        require
            not_empty: not empty -- i.e., count > 0
        end






feature -- Status report
    
    -- Is stack empty?
    empty: BOOLEAN is
        ensure
            empty_definition: result = (count = 0)
        end

    
    
    
    
    
    
    
    -- Is stack full?
    full: BOOLEAN is
        ensure
            full_definition: result = (count = capacity)
        end








feature -- Element change

    -- Add x on top.
    put(x: G) is
        require
            not_full: not full
        ensure
            not_empty: not empty
            added_to_top: item = x
            one_more_item: count = old count + 1
        end

    
    
    
    
    
    
    
    -- Remove top element.
    remove is
        require
            not_empty: not empty -- i.e., count > 0
        ensure
            not_full: not full
            one_fewer_item: count = old count - 1

        end























end -- class interface STACK4

-- End.
//]

