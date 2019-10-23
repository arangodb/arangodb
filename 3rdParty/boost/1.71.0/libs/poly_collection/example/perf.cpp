/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */
  
/* Boost.PolyCollection performance tests */

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numeric> 

std::chrono::high_resolution_clock::time_point measure_start,measure_pause;
    
template<typename F>
double measure(F f)
{
  using namespace std::chrono;
    
  static const int              num_trials=10;
  static const milliseconds     min_time_per_trial(200);
  std::array<double,num_trials> trials;
  volatile decltype(f())        res; /* to avoid optimizing f() away */
    
  for(int i=0;i<num_trials;++i){
    int                               runs=0;
    high_resolution_clock::time_point t2;
    
    measure_start=high_resolution_clock::now();
    do{
      res=f();
      ++runs;
      t2=high_resolution_clock::now();
    }while(t2-measure_start<min_time_per_trial);
    trials[i]=duration_cast<duration<double>>(t2-measure_start).count()/runs;
  }
  (void)res; /* var not used warn */
    
  std::sort(trials.begin(),trials.end());
  return std::accumulate(
    trials.begin()+2,trials.end()-2,0.0)/(trials.size()-4);
}

template<typename F>
double measure(unsigned int n,F f)
{
  double t=measure(f);
  return (t/n)*10E9;
}    

void pause_timing()
{
  measure_pause=std::chrono::high_resolution_clock::now();
}
    
void resume_timing()
{
  measure_start+=std::chrono::high_resolution_clock::now()-measure_pause;
}

#include <algorithm>
#include <array>
#include <boost/poly_collection/algorithm.hpp>
#include <boost/poly_collection/any_collection.hpp>
#include <boost/poly_collection/base_collection.hpp>
#include <boost/poly_collection/function_collection.hpp>
#include <boost/ptr_container/ptr_container.hpp>
#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/callable.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/type_erasure/operators.hpp>
#include <boost/type_erasure/typeid_of.hpp>
#include <functional>
#include <random>
#include <string>
#include <utility>
#include <vector>

//[perf_base_types
struct base
{
  virtual ~base()=default;
  virtual int operator()(int)const=0;
};

struct derived1 final:base
{
  derived1(int n):n{n}{}
  virtual int operator()(int)const{return n;}
  
  int n;
};

struct derived2 final:base
{
  derived2(int n):n{n}{}
  virtual int operator()(int x)const{return x*n;}
  
  int unused,n;
};

struct derived3 final:base
{
  derived3(int n):n{n}{}
  virtual int operator()(int x)const{return x*x*n;}
  
  int unused,n;
};
//]

//[perf_function_types
struct concrete1
{
  concrete1(int n):n{n}{}
  int operator()(int)const{return n;}
  
  int n;
};

struct concrete2
{
  concrete2(int n):n{n}{}
  int operator()(int x)const{return x*n;}
  
  int unused,n;
};

struct concrete3
{
  concrete3(int n):n{n}{}
  int operator()(int x)const{return x*x*n;}
  
  int unused,n;
};
//]

template<typename Base>
struct ptr_vector:boost::ptr_vector<Base>
{
public:
  template<typename T>
  void insert(const T& x)
  {
    this->push_back(new T{x});
  }

  template<typename F>
  void for_each(F f)
  {
    std::for_each(this->begin(),this->end(),f);
  }

  void prepare_for_for_each(){}
};

template<typename Base>
struct sorted_ptr_vector:ptr_vector<Base>
{
  void prepare_for_for_each()
  {
    std::sort(
      this->c_array(),this->c_array()+this->size(),
      [](Base* x,Base* y){return typeid(*x).before(typeid(*y));});
  }
};

template<typename Base>
struct shuffled_ptr_vector:ptr_vector<Base>
{
  void prepare_for_for_each()
  {
    std::shuffle(
     this->c_array(),this->c_array()+this->size(),std::mt19937(1));
  }
};

template<typename Base>
struct base_collection:boost::base_collection<Base>
{
  template<typename F>
  void for_each(F f)
  {
    std::for_each(this->begin(),this->end(),f);
  }

  void prepare_for_for_each(){}
};

template<typename Base,typename... T>
struct poly_for_each_base_collection:base_collection<Base>
{
  template<typename F>
  void for_each(F f)
  {
    boost::poly_collection::for_each<T...>(this->begin(),this->end(),f);
  }
};

template<typename Signature>
struct func_vector:std::vector<std::function<Signature>>
{
  template<typename T> void insert(const T& x)
  {
    this->push_back(x);
  }

  template<typename F>
  void for_each(F f)
  {
    std::for_each(this->begin(),this->end(),f);
  }

  void prepare_for_for_each(){}
};

template<typename Signature>
struct sorted_func_vector:func_vector<Signature>
{
  void prepare_for_for_each()
  {
    using value_type=typename sorted_func_vector::value_type;
    std::sort(
      this->begin(),this->end(),[](const value_type& x,const value_type& y){
        return x.target_type().before(y.target_type());
      });
  }
};

template<typename Signature>
struct shuffled_func_vector:func_vector<Signature>
{
  void prepare_for_for_each()
  {
    std::shuffle(this->begin(),this->end(),std::mt19937(1));
  }
};

template<typename Signature>
struct func_collection:boost::function_collection<Signature>
{
  template<typename F>
  void for_each(F f)
  {
    std::for_each(this->begin(),this->end(),f);
  }

  void prepare_for_for_each(){}
};

template<typename Signature,typename... T>
struct poly_for_each_func_collection:func_collection<Signature>
{
  template<typename F>
  void for_each(F f)
  {
    boost::poly_collection::for_each<T...>(this->begin(),this->end(),f);
  }
};

template<typename Concept>
struct any_vector:std::vector<boost::type_erasure::any<Concept>>
{
  template<typename T> void insert(const T& x)
  {
    this->push_back(x);
  }

  template<typename F>
  void for_each(F f)
  {
    std::for_each(this->begin(),this->end(),f);
  }

  void prepare_for_for_each(){}
};

template<typename Concept>
struct sorted_any_vector:any_vector<Concept>
{
  void prepare_for_for_each()
  {
    using value_type=typename sorted_any_vector::value_type;
    std::sort(
      this->begin(),this->end(),[](const value_type& x,const value_type& y){
        return typeid_of(x).before(typeid_of(y));
      });
  }
};

template<typename Concept>
struct shuffled_any_vector:any_vector<Concept>
{
  void prepare_for_for_each()
  {
    std::shuffle(this->begin(),this->end(),std::mt19937(1));
  }
};

template<typename Concept>
struct any_collection:boost::any_collection<Concept>
{
  template<typename F>
  void for_each(F f)
  {
    std::for_each(this->begin(),this->end(),f);
  }

  void prepare_for_for_each(){}
};

template<typename Concept,typename... T>
struct poly_for_each_any_collection:any_collection<Concept>
{
  template<typename F>
  void for_each(F f)
  {
    boost::poly_collection::for_each<T...>(this->begin(),this->end(),f);
  }
};

#include <iostream>

template<typename... Printables>
void print(Printables... ps)
{
  const char* delim="";
  using seq=int[1+sizeof...(ps)];
  (void)seq{0,(std::cout<<delim<<ps,delim=";",0)...};
  std::cout<<"\n";
}

template<typename T>
struct label
{
  label(const char* str):str{str}{}
  operator const char*()const{return str;}
  const char* str;
};

template<typename... T>
struct element_sequence{};

template<
  typename... Element,
  typename Container
>
void container_fill(unsigned int n,element_sequence<Element...>,Container& c)
{
  auto m=n/sizeof...(Element);
  for(unsigned int i=0;i!=m;++i){
    using seq=int[sizeof...(Element)];
    (void)seq{(c.insert(Element(i)),0)...};
  }
}

struct insert_perf_functor
{
  template<
    typename... Element,
    typename Container
  >
  std::size_t operator()(
    unsigned int n,element_sequence<Element...> elements,label<Container>)const
  {
    pause_timing();
    std::size_t res=0;
    {
      Container c;
      resume_timing();
      container_fill(n,elements,c);
      pause_timing();
      res=c.size();
    }
    resume_timing();
    return res;
  }
};

template<
  typename... Element,
  typename Container
>
double insert_perf(
  unsigned int n,element_sequence<Element...> elements,label<Container> label)
{
  return measure(n,std::bind(insert_perf_functor{},n,elements,label));
}

template<
  typename... Element,
  typename... Container
>
void insert_perf(
  unsigned int n0,unsigned int n1,unsigned int dsav,
  element_sequence<Element...> elements,label<Container>... labels)
{
  std::cout<<"insert:\n";
  print("n",labels...);
  
  for(unsigned int s=0,n=n0;
      (n=(unsigned int)std::round(n0*std::pow(10.0,s/1000.0)))<=n1;
      s+=dsav){
    unsigned int m=(unsigned int)std::round(n/sizeof...(Element)),
                 nn=m*sizeof...(Element);
    print(nn,insert_perf(nn,elements,labels)...);
  }
}

struct for_each_perf_functor
{
  template<typename F,typename Container>
  auto operator()(F f,Container& c)const->decltype(f.res)
  {
    c.for_each(std::ref(f));
    return f.res;
  }
};

template<
  typename... Element,
  typename F,
  typename Container
>
double for_each_perf(
  unsigned int n,
  element_sequence<Element...> elements,F f,label<Container>)
{
  Container c;
  container_fill(n,elements,c);
  c.prepare_for_for_each();
  return measure(n,std::bind(for_each_perf_functor{},f,std::ref(c)));
}

template<
  typename... Element,
  typename F,
  typename... Container
>
void for_each_perf(
  unsigned int n0,unsigned int n1,unsigned int dsav,
  element_sequence<Element...> elements,F f,label<Container>... labels)
{
  std::cout<<"for_each:\n";
  print("n",labels...);
  
  for(unsigned int s=0,n=n0;
      (n=(unsigned int)std::round(n0*std::pow(10.0,s/1000.0)))<=n1;
      s+=dsav){
    unsigned int m=(unsigned int)std::round(n/sizeof...(Element)),
                 nn=m*sizeof...(Element);
    print(nn,for_each_perf(nn,elements,f,labels)...);
  }
}

//[perf_for_each_callable
struct for_each_callable
{
  for_each_callable():res{0}{}

  template<typename T>
  void operator()(T& x){
    res+=x(2);
  }

  int res;
};
//]

//[perf_for_each_incrementable
struct for_each_incrementable
{
  for_each_incrementable():res{0}{}

  template<typename T>
  void operator()(T& x){
    ++x;
    ++res;
  }

  int res;
};
//]

int main(int argc, char *argv[])
{
  using test=std::pair<std::string,bool&>;

  bool all=false,
       insert_base=false,
       for_each_base=false,
       insert_function=false,
       for_each_function=false,
       insert_any=false,
       for_each_any=false;
  std::array<test,7> tests={{
    {"all",all},
    {"insert_base",insert_base},
    {"for_each_base",for_each_base},
    {"insert_function",insert_function},
    {"for_each_function",for_each_function},
    {"insert_any",insert_any},
    {"for_each_any",for_each_any}
  }};

  if(argc<2){
     std::cout<<"specify one or more tests to execute:\n";
     for(const auto& p:tests)std::cout<<"  "<<p.first<<"\n";
     return 1;
  }

  for(int arg=1;arg<argc;++arg){
    auto it=std::find_if(tests.begin(),tests.end(),[&](test& t){
      return t.first==argv[arg];
    });
    if(it==tests.end()){
      std::cout<<"invalid test name\n";
      return 1;
    }
    it->second=true;
  }

  unsigned int n0=100,n1=10000000,dsav=50; /* sav for savart */

  {
    auto seq=  element_sequence<
                 derived1,derived1,derived2,derived2,derived3>{};
    auto f=    for_each_callable{};
    auto pv=   label<ptr_vector<base>>
               {"ptr_vector"};
    auto spv=  label<sorted_ptr_vector<base>>
               {"sorted ptr_vector"};
    auto shpv= label<shuffled_ptr_vector<base>>
               {"shuffled ptr_vector"};
    auto bc=   label<base_collection<base>>
               {"base_collection"};
    auto fbc=  label<poly_for_each_base_collection<base>>
               {"base_collection (poly::for_each)"};
    auto rfbc= label<
                 poly_for_each_base_collection<base,derived1,derived2,derived2>
               >
               {"base_collection (restituted poly::for_each)"};

    if(all||insert_base)insert_perf(n0,n1,dsav,seq,pv,bc);
    if(all||for_each_base)for_each_perf(
      n0,n1,dsav,seq,f,pv,spv,shpv,bc,fbc,rfbc);
  }
  {
    using signature=int(int);

    auto seq=  element_sequence<
                 concrete1,concrete1,concrete2,concrete2,concrete3>{};
    auto f =   for_each_callable{};
    auto fv=   label<func_vector<signature>>
               {"func_vector"};
    auto sfv=  label<sorted_func_vector<signature>>
               {"sorted func_vector"};
    auto shfv= label<shuffled_func_vector<signature>>
               {"shuffled func_vector"};
    auto fc=   label<func_collection<signature>>
               {"function_collection"};
    auto ffc=  label<poly_for_each_func_collection<signature>>
               {"function_collection (poly::for_each)"};
    auto rffc= label<poly_for_each_func_collection<
                 signature,concrete1,concrete2,concrete3>>
               {"function_collection (restituted poly::for_each)"};

    if(all||insert_function)insert_perf(n0,n1,dsav,seq,fv,fc);
    if(all||for_each_function)for_each_perf(
      n0,n1,dsav,seq,f,fv,sfv,shfv,fc,ffc,rffc);
  }
  {
//[perf_any_types
    using concept_=boost::mpl::vector<
      boost::type_erasure::copy_constructible<>,
      boost::type_erasure::relaxed,
      boost::type_erasure::typeid_<>,
      boost::type_erasure::incrementable<>
    >;
//]

    auto seq=  element_sequence<int,int,double,double,char>{};
    auto f=    for_each_incrementable{};
    auto av=   label<any_vector<concept_>>
               {"any_vector"};
    auto sav=  label<sorted_any_vector<concept_>>
               {"sorted any_vector"};
    auto shav= label<shuffled_any_vector<concept_>>
               {"shuffled any_vector"};
    auto ac=   label<any_collection<concept_>>
               {"any_collection"};
    auto fac=  label<poly_for_each_any_collection<concept_>>
               {"any_collection (poly::for_each)"};
    auto rfac= label<poly_for_each_any_collection<concept_,int,double,char>>
               {"any_collection (restituted poly::for_each)"};

    if(all||insert_any)insert_perf(n0,n1,dsav,seq,av,ac);
    if(all||for_each_any)for_each_perf(
      n0,n1,dsav,seq,f,av,sav,shav,ac,fac,rfac);
  }
}
