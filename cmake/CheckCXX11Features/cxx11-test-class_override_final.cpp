class base {
public:
    virtual int foo(int a)
     { return 4 + a; }
    int bar(int a)
     { return a - 2; }
};

class sub final : public base {
public:
    virtual int foo(int a) override
     { return 8 + 2 * a; };
};

class sub2 final : public base {
public:
    virtual int foo(int a) override final
     { return 8 + 2 * a; };
};

int main(void)
{
    base b;
    sub s;
    sub2 t;

    return (b.foo(2) * 2 == s.foo(2) && b.foo(2) * 2 == t.foo(2) ) ? 0 : 1;
}
