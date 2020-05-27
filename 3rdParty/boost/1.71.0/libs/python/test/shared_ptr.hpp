// Copyright David Abrahams 2002.
// Copyright Stefan Seefeld 2016.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_class.hpp"

using namespace boost::python;

typedef test_class<> X;
typedef test_class<1> Y;

template <class T>
struct functions
{
    static int look(shared_ptr<T> const& x)
    {
        return (x.get()) ? x->value() : -1;
    }

    static void store(shared_ptr<T> x)
    {
        storage = x;
    }

    static void release_store()
    {
        store(shared_ptr<T>());
    }
    
    static void modify(shared_ptr<T>& x)
    {
        x.reset();
    }

    static shared_ptr<T> get() { return storage; }
    static shared_ptr<T> &get1() { return storage; }
        
    static int look_store()
    {
        return look(get());
    }

    template <class C>
    static void expose(C const& c)
    {
        def("look", &look);
        def("store", &store);
        def("modify", &modify);
        def("identity", &identity);
        def("null", &null);
            
        const_cast<C&>(c)
            .def("look", &look)
            .staticmethod("look")
            .def("store", &store)
            .staticmethod("store")
            .def("modify", &modify)
            .staticmethod("modify")
            .def("look_store", &look_store)
            .staticmethod("look_store")
            .def("identity", &identity)
            .staticmethod("identity")
            .def("null", &null)
            .staticmethod("null")
            .def("get1", &get1, return_internal_reference<>())
            .staticmethod("get1")
            .def("get", &get)
            .staticmethod("get")
            .def("count", &T::count)
            .staticmethod("count")
            .def("release", &release_store)
            .staticmethod("release")
            ;
    }

    static shared_ptr<T> identity(shared_ptr<T> x) { return x; }
    static shared_ptr<T> null(T const&) { return shared_ptr<T>(); }
    

    static shared_ptr<T> storage;
};

template <class T> shared_ptr<T> functions<T>::storage;

struct Z : test_class<2>
{
    Z(int x) : test_class<2>(x) {}
    virtual int v() { return this->value(); }
};

struct ZWrap : Z
{
    ZWrap(PyObject* self, int x)
        : Z(x), m_self(self) {}

    
    virtual int v() { return call_method<int>(m_self, "v"); }
    int default_v() { return Z::v(); }
    

    PyObject* m_self;
};

struct YY : Y
{
    YY(int n) : Y(n) {}
};

struct YYY : Y
{
    YYY(int n) : Y(n) {}
};

shared_ptr<Y> factory(int n)
{
    return shared_ptr<Y>(n < 42 ? new Y(n) : new YY(n));
}

// regressions from Nicodemus
    struct A
    {
        virtual ~A() {}; // silence compiler warnings
        virtual int f() = 0;
        static int call_f(shared_ptr<A>& a) { return a->f(); }
    };

    struct B: A
    {
        int f() { return 1; }
    };

    shared_ptr<A> New(bool make)
    {
        return shared_ptr<A>( make ? new B() : 0 );
    }

    struct A_Wrapper: A
    {
        A_Wrapper(PyObject* self_):
            A(), self(self_) {}

        int f() {
            return call_method< int >(self, "f");
        }

        PyObject* self;
    };

// ------

// from Neal Becker

struct Test {
  shared_ptr<X> x;
};
// ------


BOOST_PYTHON_MODULE(MODULE)
{
  class_<A, shared_ptr<A_Wrapper>, boost::noncopyable>("A")
      .def("call_f", &A::call_f)
      .staticmethod("call_f")
      ;

    // This is the ugliness required to register a to-python converter
    // for shared_ptr<A>.
    objects::class_value_wrapper<
        shared_ptr<A>
      , objects::make_ptr_instance<A, objects::pointer_holder<shared_ptr<A>,A> >
    >();
        
    def("New", &New);

    def("factory", factory);
    
    functions<X>::expose(
        class_<X, boost::noncopyable>("X", init<int>())
             .def("value", &X::value)
        );

    functions<Y>::expose(
        class_<Y, shared_ptr<Y> >("Y", init<int>())
            .def("value", &Y::value)
        );
    
    class_<YY, bases<Y>, boost::noncopyable>("YY", init<int>())
        ;

    class_<YYY, shared_ptr<YYY>, bases<Y> >("YYY", init<int>())
        ;

    functions<Z>::expose(
        class_<Z, ZWrap>("Z", init<int>())
            .def("value", &Z::value)
            .def("v", &Z::v, &ZWrap::default_v)
        );

// from Neal Becker
    class_<Test> ("Test")
       .def_readonly ("x", &Test::x, "x") 
       ;
// ------
}
