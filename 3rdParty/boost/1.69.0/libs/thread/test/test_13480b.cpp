// comment if you want to see the boost::move issue
#define BOOST_THREAD_VERSION 3

#include <boost/type_traits/is_same.hpp>
#include <boost/variant/detail/apply_visitor_binary.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/variant.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>

#define WANT_BUILD_TO_FAIL
#ifdef WANT_BUILD_TO_FAIL
#include <boost/thread.hpp>
#endif

class AType
{

};

class BType
{
};

typedef boost::shared_ptr<AType> IATypePtr;
typedef boost::shared_ptr<BType> IBTypePtr;

typedef boost::variant<IATypePtr, IBTypePtr> ABVar;

struct ServiceTimeTableEvent
{
public:

    ServiceTimeTableEvent(ABVar abType)
        : m_abType{ abType }
        {}

        inline ABVar const& GetABType() const { return m_abType; }
        inline ABVar & GetABType() { return m_abType; }

private:
    ABVar           m_abType;
};

class ab_are_equals
    : public boost::static_visitor<bool>
{
public:
    template<typename T, typename U>
    bool operator()(T, U) const
    {
        return false; // cannot compare different types
    }
    template<typename T>
    bool operator()(T lhs, T rhs) const
    {
        return  lhs == rhs;
    }
};

int main(int argc, char* argv[])
{

    auto aTypePtr = IATypePtr{ new AType };
    auto bTypePtr = IBTypePtr{ new BType };

    ABVar aType = aTypePtr;
    ABVar bType = bTypePtr;

    ServiceTimeTableEvent timeTableEvent1{ aType };
    ServiceTimeTableEvent timeTableEvent2{ bType };

    auto xx = boost::apply_visitor(ab_are_equals(), timeTableEvent1.GetABType(), timeTableEvent2.GetABType());
    xx = boost::apply_visitor(ab_are_equals(), timeTableEvent1.GetABType(), timeTableEvent1.GetABType());;
    xx = boost::apply_visitor(ab_are_equals(), timeTableEvent2.GetABType(), timeTableEvent2.GetABType());;
}


