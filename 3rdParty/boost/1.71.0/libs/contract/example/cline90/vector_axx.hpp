
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[cline90_vector_axx
// Extra spaces, newlines, etc. for visual alignment with this library code.





template<typename T>
class vector {








legal: // Class invariants (legal).
    size() >= 0;


public:
    explicit vector(int count = 10) :
        data_(new T[count]),
        size_(count)
    {
        for(int i = 0; i < size_; ++i) data_[i] = T();
    }

    
    
    
    
    
    
    
    
    
    virtual ~vector() { delete[] data_; }

    
    
    
    int size() const { return size_; }
    
    
    
    
    void resize(int count) {
        T* slice = new T[count];
        for(int i = 0; i < count && i < size_; ++i) slice[i] = data_[i];
        delete[] data_;
        data_ = slice;
        size_ = count;
    }

    
    
    
    
    
    
    
    
    
    T& operator[](int index) { return data_[index]; }
    
    
    
    
    
    
    
    
    
    
    T& operator[](int index) const { return data_[index]; }




axioms: // Preconditions (require) and postconditions (promise) for each func.
    [int count; require count >= 0; promise size() == count] vector(count);
    [int count; require count >= 0; promise size() == count] resize(count);
    [int index; require index >= 0 && index < size()] (*this)[x];       // Op[].
    [int index; require index >= 0 && index < size()] (*this)[x] const; // Op[].

private:
    T* data_;
    int size_;
};

// End.
//]

