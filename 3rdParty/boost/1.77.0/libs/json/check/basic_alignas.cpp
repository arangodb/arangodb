struct some_struct {
    double d;
    int n;
    void* ptr;
};

alignas(16) char test1[10];
alignas(double) char test2[10];
alignas(some_struct) char test3[10];
