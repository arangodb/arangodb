#pragma once
/*
 *  FakeIt - A Simplified C++ Mocking Framework
 *  Copyright (c) Eran Pe'er 2013
 *  Generated: 2017-02-28 13:49:37.806594
 *  Distributed under the MIT License. Please refer to the LICENSE file at:
 *  https://github.com/eranpeer/FakeIt
 */

#ifndef fakeit_h__
#define fakeit_h__



#include <functional>
#include <memory>
#include <set>
#include <vector>
#include <stdexcept>
#if defined (__GNUG__) || _MSC_VER >= 1900
#define THROWS noexcept(false)
#define NO_THROWS noexcept(true)
#elif defined (_MSC_VER)
#define THROWS throw(...)
#define NO_THROWS
#endif
#include <typeinfo>
#include <unordered_set>
#include <tuple>
#include <string>
#include <iosfwd>
#include <atomic>
#include <tuple>


namespace fakeit {

    template<class C>
    struct naked_type {
        typedef typename std::remove_cv<typename std::remove_reference<C>::type>::type type;
    };

    template< class T > struct tuple_arg         { typedef T  type; };
    template< class T > struct tuple_arg < T& >  { typedef T& type; };
    template< class T > struct tuple_arg < T&& > { typedef T&&  type; };




    template<typename... arglist>
    using ArgumentsTuple = std::tuple < arglist... > ;

    template< class T > struct test_arg         { typedef T& type; };
    template< class T > struct test_arg< T& >   { typedef T& type; };
    template< class T > struct test_arg< T&& >  { typedef T& type; };

    template< class T > struct production_arg         { typedef T& type; };
    template< class T > struct production_arg< T& >   { typedef T& type; };
    template< class T > struct production_arg< T&& >  { typedef T&&  type; };

    template <typename T>
    class is_ostreamable {
        struct no {};
#if defined(_MSC_VER) && _MSC_VER < 1900
        template <typename T1>
        static decltype(operator<<(std::declval<std::ostream&>(), std::declval<const T1>())) test(std::ostream &s, const T1 &t);
#else
        template <typename T1>
        static auto test(std::ostream &s, const T1 &t) -> decltype(s << t);
#endif
        static no test(...);
    public:

        static const bool value =
            std::is_arithmetic<T>::value ||
            std::is_pointer<T>::value ||
            std::is_same<decltype(test(*(std::ostream *)nullptr,
                std::declval<T>())), std::ostream &>::value;
    };


    template <>
    class is_ostreamable<std::ios_base& (*)(std::ios_base&)> {
    public:
        static const bool value = true;
    };

    template <typename CharT, typename Traits>
    class is_ostreamable<std::basic_ios<CharT,Traits>& (*)(std::basic_ios<CharT,Traits>&)> {
    public:
        static const bool value = true;
    };

    template <typename CharT, typename Traits>
    class is_ostreamable<std::basic_ostream<CharT,Traits>& (*)(std::basic_ostream<CharT,Traits>&)> {
    public:
        static const bool value = true;
    };

    template<typename R, typename... arglist>
    struct VTableMethodType {
#if defined (__GNUG__)
        typedef R(*type)(void *, arglist...);
#elif defined (_MSC_VER)
        typedef R(__thiscall *type)(void *, arglist...);
#endif
    };
}
#include <typeinfo>
#include <tuple>
#include <string>
#include <iosfwd>
#include <sstream>
#include <string>

namespace fakeit {

    struct FakeitContext;

    template<typename C>
    struct MockObject {
        virtual ~MockObject() THROWS { };

        virtual C &get() = 0;

        virtual FakeitContext &getFakeIt() = 0;
    };

    struct MethodInfo {

        static unsigned int nextMethodOrdinal() {
            static std::atomic_uint ordinal{0};
            return ++ordinal;
        }

        MethodInfo(unsigned int anId, std::string aName) :
                _id(anId), _name(aName) { }

        unsigned int id() const {
            return _id;
        }

        std::string name() const {
            return _name;
        }

        void setName(const std::string &value) {
            _name = value;
        }

    private:
        unsigned int _id;
        std::string _name;
    };

    struct UnknownMethod {

        static MethodInfo &instance() {
            static MethodInfo instance(MethodInfo::nextMethodOrdinal(), "unknown");
            return instance;
        }

    };

}
namespace fakeit {
    class Destructible {
    public:
        virtual ~Destructible() {}
    };
}

namespace fakeit {

    struct Invocation : Destructible {

        static unsigned int nextInvocationOrdinal() {
            static std::atomic_uint invocationOrdinal{0};
            return ++invocationOrdinal;
        }

        struct Matcher {

            virtual ~Matcher() THROWS {
            }

            virtual bool matches(Invocation &invocation) = 0;

            virtual std::string format() const = 0;
        };

        Invocation(unsigned int ordinal, MethodInfo &method) :
                _ordinal(ordinal), _method(method), _isVerified(false) {
        }

        virtual ~Invocation() override = default;

        unsigned int getOrdinal() const {
            return _ordinal;
        }

        MethodInfo &getMethod() const {
            return _method;
        }

        void markAsVerified() {
            _isVerified = true;
        }

        bool isVerified() const {
            return _isVerified;
        }

        virtual std::string format() const = 0;

    private:
        const unsigned int _ordinal;
        MethodInfo &_method;
        bool _isVerified;
    };

}
#include <iosfwd>
#include <tuple>
#include <string>
#include <sstream>
#include <ostream>

namespace fakeit {

	template<typename T, class Enable = void>
	struct Formatter;

	template <>
	struct Formatter<bool>
	{
		static std::string format(bool const &val)
		{
			return val ? "true" : "false";
		}
	};

	template <>
	struct Formatter<char>
	{
		static std::string format(char const &val)
		{
			std::string s;
			s += "'";
			s += val;
			s += "'";
			return s;
		}
	};

	template<class C>
	struct Formatter<C, typename std::enable_if<!is_ostreamable<C>::value>::type> {
		static std::string format(C const &)
		{
			return "?";
		}
	};

	template<class C>
	struct Formatter<C, typename std::enable_if<is_ostreamable<C>::value>::type> {
		static std::string format(C const &val)
		{
			std::ostringstream os;
			os << val;
			return os.str();
		}
	};


	template <typename T>
	using TypeFormatter = Formatter<typename fakeit::naked_type<T>::type>;
}

namespace fakeit {


    template<class Tuple, std::size_t N>
    struct TuplePrinter {
        static void print(std::ostream &strm, const Tuple &t) {
            TuplePrinter<Tuple, N - 1>::print(strm, t);
            strm << ", " << fakeit::TypeFormatter<decltype(std::get<N - 1>(t))>::format(std::get<N - 1>(t));
        }
    };

    template<class Tuple>
    struct TuplePrinter<Tuple, 1> {
        static void print(std::ostream &strm, const Tuple &t) {
            strm << fakeit::TypeFormatter<decltype(std::get<0>(t))>::format(std::get<0>(t));
        }
    };

    template<class Tuple>
    struct TuplePrinter<Tuple, 0> {
        static void print(std::ostream &, const Tuple &) {
        }
    };

    template<class ... Args>
    void print(std::ostream &strm, const std::tuple<Args...> &t) {
        strm << "(";
        TuplePrinter<decltype(t), sizeof...(Args)>::print(strm, t);
        strm << ")";
    }

    template<class ... Args>
    std::ostream &operator<<(std::ostream &strm, const std::tuple<Args...> &t) {
        print(strm, t);
        return strm;
    }

}


namespace fakeit {

    template<typename ... arglist>
    struct ActualInvocation : public Invocation {

        struct Matcher : public virtual Destructible {
            virtual bool matches(ActualInvocation<arglist...> &actualInvocation) = 0;

            virtual std::string format() const = 0;
        };

        ActualInvocation(unsigned int ordinal, MethodInfo &method, const typename fakeit::production_arg<arglist>::type... args) :
            Invocation(ordinal, method), _matcher{ nullptr }
            , actualArguments{ std::forward<arglist>(args)... }
        {
        }

        ArgumentsTuple<arglist...> & getActualArguments() {
            return actualArguments;
        }


        void setActualMatcher(Matcher *matcher) {
            this->_matcher = matcher;
        }

        Matcher *getActualMatcher() {
            return _matcher;
        }

        virtual std::string format() const override {
            std::ostringstream out;
            out << getMethod().name();
            print(out, actualArguments);
            return out.str();
        }

    private:

        Matcher *_matcher;
        ArgumentsTuple<arglist...> actualArguments;
    };

    template<typename ... arglist>
    std::ostream &operator<<(std::ostream &strm, const ActualInvocation<arglist...> &ai) {
        strm << ai.format();
        return strm;
    }

}





#include <unordered_set>

namespace fakeit {

    struct ActualInvocationsSource {
        virtual void getActualInvocations(std::unordered_set<fakeit::Invocation *> &into) const = 0;

        virtual ~ActualInvocationsSource() NO_THROWS { };
    };

    struct InvocationsSourceProxy : public ActualInvocationsSource {

        InvocationsSourceProxy(ActualInvocationsSource *inner) :
                _inner(inner) {
        }

        void getActualInvocations(std::unordered_set<fakeit::Invocation *> &into) const override {
            _inner->getActualInvocations(into);
        }

    private:
        std::shared_ptr<ActualInvocationsSource> _inner;
    };

    struct UnverifiedInvocationsSource : public ActualInvocationsSource {

        UnverifiedInvocationsSource(InvocationsSourceProxy decorated) : _decorated(decorated) {
        }

        void getActualInvocations(std::unordered_set<fakeit::Invocation *> &into) const override {
            std::unordered_set<fakeit::Invocation *> all;
            _decorated.getActualInvocations(all);
            for (fakeit::Invocation *i : all) {
                if (!i->isVerified()) {
                    into.insert(i);
                }
            }
        }

    private:
        InvocationsSourceProxy _decorated;
    };

    struct AggregateInvocationsSource : public ActualInvocationsSource {

        AggregateInvocationsSource(std::vector<ActualInvocationsSource *> &sources) : _sources(sources) {
        }

        void getActualInvocations(std::unordered_set<fakeit::Invocation *> &into) const override {
            std::unordered_set<fakeit::Invocation *> tmp;
            for (ActualInvocationsSource *source : _sources) {
                source->getActualInvocations(tmp);
            }
            filter(tmp, into);
        }

    protected:
        bool shouldInclude(fakeit::Invocation *) const {
            return true;
        }

    private:
        std::vector<ActualInvocationsSource *> _sources;

        void filter(std::unordered_set<Invocation *> &source, std::unordered_set<Invocation *> &target) const {
            for (Invocation *i:source) {
                if (shouldInclude(i)) {
                    target.insert(i);
                }
            }
        }
    };
}

namespace fakeit {

    class Sequence {
    private:

    protected:

        Sequence() {
        }

        virtual ~Sequence() THROWS {
        }

    public:


        virtual void getExpectedSequence(std::vector<Invocation::Matcher *> &into) const = 0;


        virtual void getInvolvedMocks(std::vector<ActualInvocationsSource *> &into) const = 0;

        virtual unsigned int size() const = 0;

        friend class VerifyFunctor;
    };

    class ConcatenatedSequence : public virtual Sequence {
    private:
        const Sequence &s1;
        const Sequence &s2;

    protected:
        ConcatenatedSequence(const Sequence &seq1, const Sequence &seq2) :
                s1(seq1), s2(seq2) {
        }

    public:

        virtual ~ConcatenatedSequence() {
        }

        unsigned int size() const override {
            return s1.size() + s2.size();
        }

        const Sequence &getLeft() const {
            return s1;
        }

        const Sequence &getRight() const {
            return s2;
        }

        void getExpectedSequence(std::vector<Invocation::Matcher *> &into) const override {
            s1.getExpectedSequence(into);
            s2.getExpectedSequence(into);
        }

        virtual void getInvolvedMocks(std::vector<ActualInvocationsSource *> &into) const override {
            s1.getInvolvedMocks(into);
            s2.getInvolvedMocks(into);
        }

        friend inline ConcatenatedSequence operator+(const Sequence &s1, const Sequence &s2);
    };

    class RepeatedSequence : public virtual Sequence {
    private:
        const Sequence &_s;
        const int times;

    protected:
        RepeatedSequence(const Sequence &s, const int t) :
                _s(s), times(t) {
        }

    public:

        ~RepeatedSequence() {
        }

        unsigned int size() const override {
            return _s.size() * times;
        }

        friend inline RepeatedSequence operator*(const Sequence &s, int times);

        friend inline RepeatedSequence operator*(int times, const Sequence &s);

        void getInvolvedMocks(std::vector<ActualInvocationsSource *> &into) const override {
            _s.getInvolvedMocks(into);
        }

        void getExpectedSequence(std::vector<Invocation::Matcher *> &into) const override {
            for (int i = 0; i < times; i++)
                _s.getExpectedSequence(into);
        }

        int getTimes() const {
            return times;
        }

        const Sequence &getSequence() const {
            return _s;
        }
    };

    inline ConcatenatedSequence operator+(const Sequence &s1, const Sequence &s2) {
        return ConcatenatedSequence(s1, s2);
    }

    inline RepeatedSequence operator*(const Sequence &s, int times) {
        if (times <= 0)
            throw std::invalid_argument("times");
        return RepeatedSequence(s, times);
    }

    inline RepeatedSequence operator*(int times, const Sequence &s) {
        if (times <= 0)
            throw std::invalid_argument("times");
        return RepeatedSequence(s, times);
    }

}

namespace fakeit {

    enum class VerificationType {
        Exact, AtLeast, NoMoreInvocations
    };

    enum class UnexpectedType {
        Unmocked, Unmatched
    };

    struct VerificationEvent {

        VerificationEvent(VerificationType aVerificationType) :
                _verificationType(aVerificationType), _line(0) {
        }

        virtual ~VerificationEvent() = default;

        VerificationType verificationType() const {
            return _verificationType;
        }

        void setFileInfo(std::string aFile, int aLine, std::string aCallingMethod) {
            _file = aFile;
            _callingMethod = aCallingMethod;
            _line = aLine;
        }

        std::string file() const {
            return _file;
        }

        int line() const {
            return _line;
        }

        const std::string &callingMethod() const {
            return _callingMethod;
        }

    private:
        VerificationType _verificationType;
        std::string _file;
        int _line;
        std::string _callingMethod;
    };

    struct NoMoreInvocationsVerificationEvent : public VerificationEvent {

        ~NoMoreInvocationsVerificationEvent() = default;

        NoMoreInvocationsVerificationEvent(
                std::vector<Invocation *> &allTheIvocations,
                std::vector<Invocation *> &anUnverifedIvocations) :
                VerificationEvent(VerificationType::NoMoreInvocations),
                _allIvocations(allTheIvocations),
                _unverifedIvocations(anUnverifedIvocations) {
        }

        const std::vector<Invocation *> &allIvocations() const {
            return _allIvocations;
        }

        const std::vector<Invocation *> &unverifedIvocations() const {
            return _unverifedIvocations;
        }

    private:
        const std::vector<Invocation *> _allIvocations;
        const std::vector<Invocation *> _unverifedIvocations;
    };

    struct SequenceVerificationEvent : public VerificationEvent {

        ~SequenceVerificationEvent() = default;

        SequenceVerificationEvent(VerificationType aVerificationType,
                                  std::vector<Sequence *> &anExpectedPattern,
                                  std::vector<Invocation *> &anActualSequence,
                                  int anExpectedCount,
                                  int anActualCount) :
                VerificationEvent(aVerificationType),
                _expectedPattern(anExpectedPattern),
                _actualSequence(anActualSequence),
                _expectedCount(anExpectedCount),
                _actualCount(anActualCount)
        {
        }

        const std::vector<Sequence *> &expectedPattern() const {
            return _expectedPattern;
        }

        const std::vector<Invocation *> &actualSequence() const {
            return _actualSequence;
        }

        int expectedCount() const {
            return _expectedCount;
        }

        int actualCount() const {
            return _actualCount;
        }

    private:
        const std::vector<Sequence *> _expectedPattern;
        const std::vector<Invocation *> _actualSequence;
        const int _expectedCount;
        const int _actualCount;
    };

    struct UnexpectedMethodCallEvent {
        UnexpectedMethodCallEvent(UnexpectedType unexpectedType, const Invocation &invocation) :
                _unexpectedType(unexpectedType), _invocation(invocation) {
        }

        const Invocation &getInvocation() const {
            return _invocation;
        }

        UnexpectedType getUnexpectedType() const {
            return _unexpectedType;
        }

        const UnexpectedType _unexpectedType;
        const Invocation &_invocation;
    };

}

namespace fakeit {

    struct VerificationEventHandler {
        virtual void handle(const SequenceVerificationEvent &e) = 0;

        virtual void handle(const NoMoreInvocationsVerificationEvent &e) = 0;
    };

    struct EventHandler : public VerificationEventHandler {
        using VerificationEventHandler::handle;

        virtual void handle(const UnexpectedMethodCallEvent &e) = 0;
    };

}
#include <vector>
#include <string>

namespace fakeit {

    struct UnexpectedMethodCallEvent;
    struct SequenceVerificationEvent;
    struct NoMoreInvocationsVerificationEvent;

    struct EventFormatter {

        virtual std::string format(const fakeit::UnexpectedMethodCallEvent &e) = 0;

        virtual std::string format(const fakeit::SequenceVerificationEvent &e) = 0;

        virtual std::string format(const fakeit::NoMoreInvocationsVerificationEvent &e) = 0;

    };

}

namespace fakeit {

    struct FakeitContext : public EventHandler, protected EventFormatter {

        virtual ~FakeitContext() = default;

        void handle(const UnexpectedMethodCallEvent &e) override {
            fireEvent(e);
            auto &eh = getTestingFrameworkAdapter();
            eh.handle(e);
        }

        void handle(const SequenceVerificationEvent &e) override {
            fireEvent(e);
            auto &eh = getTestingFrameworkAdapter();
            return eh.handle(e);
        }

        void handle(const NoMoreInvocationsVerificationEvent &e) override {
            fireEvent(e);
            auto &eh = getTestingFrameworkAdapter();
            return eh.handle(e);
        }

        std::string format(const UnexpectedMethodCallEvent &e) override {
            auto &eventFormatter = getEventFormatter();
            return eventFormatter.format(e);
        }

        std::string format(const SequenceVerificationEvent &e) override {
            auto &eventFormatter = getEventFormatter();
            return eventFormatter.format(e);
        }

        std::string format(const NoMoreInvocationsVerificationEvent &e) override {
            auto &eventFormatter = getEventFormatter();
            return eventFormatter.format(e);
        }

        void addEventHandler(EventHandler &eventListener) {
            _eventListeners.push_back(&eventListener);
        }

        void clearEventHandlers() {
            _eventListeners.clear();
        }

    protected:
        virtual EventHandler &getTestingFrameworkAdapter() = 0;

        virtual EventFormatter &getEventFormatter() = 0;

    private:
        std::vector<EventHandler *> _eventListeners;

        void fireEvent(const NoMoreInvocationsVerificationEvent &evt) {
            for (auto listener : _eventListeners)
                listener->handle(evt);
        }

        void fireEvent(const UnexpectedMethodCallEvent &evt) {
            for (auto listener : _eventListeners)
                listener->handle(evt);
        }

        void fireEvent(const SequenceVerificationEvent &evt) {
            for (auto listener : _eventListeners)
                listener->handle(evt);
        }

    };

}
#include <iostream>
#include <iosfwd>

namespace fakeit {

    struct DefaultEventFormatter : public EventFormatter {

        virtual std::string format(const UnexpectedMethodCallEvent &e) override {
            std::ostringstream out;
            out << "Unexpected method invocation: ";
            out << e.getInvocation().format() << std::endl;
            if (UnexpectedType::Unmatched == e.getUnexpectedType()) {
                out << "  Could not find Any recorded behavior to support this method call.";
            } else {
                out << "  An unmocked method was invoked. All used virtual methods must be stubbed!";
            }
            return out.str();
        }


        virtual std::string format(const SequenceVerificationEvent &e) override {
            std::ostringstream out;
            out << "Verification error" << std::endl;

            out << "Expected pattern: ";
            const std::vector<fakeit::Sequence *> expectedPattern = e.expectedPattern();
            out << formatExpectedPattern(expectedPattern) << std::endl;

            out << "Expected matches: ";
            formatExpectedCount(out, e.verificationType(), e.expectedCount());
            out << std::endl;

            out << "Actual matches  : " << e.actualCount() << std::endl;

            auto actualSequence = e.actualSequence();
            out << "Actual sequence : total of " << actualSequence.size() << " actual invocations";
            if (actualSequence.size() == 0) {
                out << ".";
            } else {
                out << ":" << std::endl;
            }
            formatInvocationList(out, actualSequence);

            return out.str();
        }

        virtual std::string format(const NoMoreInvocationsVerificationEvent &e) override {
            std::ostringstream out;
            out << "Verification error" << std::endl;
            out << "Expected no more invocations!! But the following unverified invocations were found:" << std::endl;
            formatInvocationList(out, e.unverifedIvocations());
            return out.str();
        }

    private:

        static std::string formatSequence(const Sequence &val) {
            const ConcatenatedSequence *cs = dynamic_cast<const ConcatenatedSequence *>(&val);
            if (cs) {
                return format(*cs);
            }
            const RepeatedSequence *rs = dynamic_cast<const RepeatedSequence *>(&val);
            if (rs) {
                return format(*rs);
            }


            std::vector<Invocation::Matcher *> vec;
            val.getExpectedSequence(vec);
            return vec[0]->format();
        }

        static void formatExpectedCount(std::ostream &out, fakeit::VerificationType verificationType,
                                        int expectedCount) {
            if (verificationType == fakeit::VerificationType::Exact)
                out << "exactly ";

            if (verificationType == fakeit::VerificationType::AtLeast)
                out << "at least ";

            out << expectedCount;
        }

        static void formatInvocationList(std::ostream &out, const std::vector<fakeit::Invocation *> &actualSequence) {
            size_t max_size = actualSequence.size();
            if (max_size > 5)
                max_size = 5;

            for (unsigned int i = 0; i < max_size; i++) {
                out << "  ";
                auto invocation = actualSequence[i];
                out << invocation->format();
                if (i < max_size - 1)
                    out << std::endl;
            }

            if (actualSequence.size() > max_size)
                out << std::endl << "  ...";
        }

        static std::string format(const ConcatenatedSequence &val) {
            std::ostringstream out;
            out << formatSequence(val.getLeft()) << " + " << formatSequence(val.getRight());
            return out.str();
        }

        static std::string format(const RepeatedSequence &val) {
            std::ostringstream out;
            const ConcatenatedSequence *cs = dynamic_cast<const ConcatenatedSequence *>(&val.getSequence());
            const RepeatedSequence *rs = dynamic_cast<const RepeatedSequence *>(&val.getSequence());
            if (rs || cs)
                out << '(';
            out << formatSequence(val.getSequence());
            if (rs || cs)
                out << ')';

            out << " * " << val.getTimes();
            return out.str();
        }

        static std::string formatExpectedPattern(const std::vector<fakeit::Sequence *> &expectedPattern) {
            std::string expectedPatternStr;
            for (unsigned int i = 0; i < expectedPattern.size(); i++) {
                Sequence *s = expectedPattern[i];
                expectedPatternStr += formatSequence(*s);
                if (i < expectedPattern.size() - 1)
                    expectedPatternStr += " ... ";
            }
            return expectedPatternStr;
        }
    };
}
namespace fakeit {

    struct FakeitException {
        std::exception err;

        virtual ~FakeitException() = default;

        virtual std::string what() const = 0;

        friend std::ostream &operator<<(std::ostream &os, const FakeitException &val) {
            os << val.what();
            return os;
        }
    };




    struct UnexpectedMethodCallException : public FakeitException {

        UnexpectedMethodCallException(std::string format) :
                _format(format) {
        }

        virtual std::string what() const override {
            return _format;
        }

    private:
        std::string _format;
    };

}

namespace fakeit {

    struct DefaultEventLogger : public fakeit::EventHandler {

        DefaultEventLogger(EventFormatter &formatter) : _formatter(formatter), _out(std::cout) { }

        virtual void handle(const UnexpectedMethodCallEvent &e) override {
            _out << _formatter.format(e) << std::endl;
        }

        virtual void handle(const SequenceVerificationEvent &e) override {
            _out << _formatter.format(e) << std::endl;
        }

        virtual void handle(const NoMoreInvocationsVerificationEvent &e) override {
            _out << _formatter.format(e) << std::endl;
        }

    private:
        EventFormatter &_formatter;
        std::ostream &_out;
    };

}

namespace fakeit {

    class AbstractFakeit : public FakeitContext {
    public:
        virtual ~AbstractFakeit() = default;

    protected:

        virtual fakeit::EventHandler &accessTestingFrameworkAdapter() = 0;

        virtual EventFormatter &accessEventFormatter() = 0;
    };

    class DefaultFakeit : public AbstractFakeit {
        DefaultEventFormatter _formatter;
        fakeit::EventFormatter *_customFormatter;
        fakeit::EventHandler *_testingFrameworkAdapter;

    public:

        DefaultFakeit() : _formatter(),
                          _customFormatter(nullptr),
                          _testingFrameworkAdapter(nullptr) {
        }

        virtual ~DefaultFakeit() = default;

        void setCustomEventFormatter(fakeit::EventFormatter &customEventFormatter) {
            _customFormatter = &customEventFormatter;
        }

        void resetCustomEventFormatter() {
            _customFormatter = nullptr;
        }

        void setTestingFrameworkAdapter(fakeit::EventHandler &testingFrameforkAdapter) {
            _testingFrameworkAdapter = &testingFrameforkAdapter;
        }

        void resetTestingFrameworkAdapter() {
            _testingFrameworkAdapter = nullptr;
        }

    protected:

        fakeit::EventHandler &getTestingFrameworkAdapter() override {
            if (_testingFrameworkAdapter)
                return *_testingFrameworkAdapter;
            return accessTestingFrameworkAdapter();
        }

        EventFormatter &getEventFormatter() override {
            if (_customFormatter)
                return *_customFormatter;
            return accessEventFormatter();
        }

        EventFormatter &accessEventFormatter() override {
            return _formatter;
        }

    };
}


#ifndef TWOBLUECUBES_SINGLE_INCLUDE_CATCH_HPP_INCLUDED
#define TWOBLUECUBES_SINGLE_INCLUDE_CATCH_HPP_INCLUDED

#define TWOBLUECUBES_CATCH_HPP_INCLUDED

#ifdef __clang__
#    pragma clang system_header
#elif defined __GNUC__
#    pragma GCC system_header
#endif



#ifdef __clang__
#   ifdef __ICC
#       pragma warning(push)
#       pragma warning(disable: 161 1682)
#   else
#       pragma clang diagnostic ignored "-Wglobal-constructors"
#       pragma clang diagnostic ignored "-Wvariadic-macros"
#       pragma clang diagnostic ignored "-Wc99-extensions"
#       pragma clang diagnostic ignored "-Wunused-variable"
#       pragma clang diagnostic push
#       pragma clang diagnostic ignored "-Wpadded"
#       pragma clang diagnostic ignored "-Wc++98-compat"
#       pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#       pragma clang diagnostic ignored "-Wswitch-enum"
#       pragma clang diagnostic ignored "-Wcovered-switch-default"
#    endif
#elif defined __GNUC__
#    pragma GCC diagnostic ignored "-Wvariadic-macros"
#    pragma GCC diagnostic ignored "-Wunused-variable"
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wpadded"
#endif
#if defined(CATCH_CONFIG_MAIN) || defined(CATCH_CONFIG_RUNNER)
#  define CATCH_IMPL
#endif

#ifdef CATCH_IMPL
#  ifndef CLARA_CONFIG_MAIN
#    define CLARA_CONFIG_MAIN_NOT_DEFINED
#    define CLARA_CONFIG_MAIN
#  endif
#endif


#define TWOBLUECUBES_CATCH_NOTIMPLEMENTED_EXCEPTION_H_INCLUDED


#define TWOBLUECUBES_CATCH_COMMON_H_INCLUDED

#define INTERNAL_CATCH_UNIQUE_NAME_LINE2( name, line ) name##line
#define INTERNAL_CATCH_UNIQUE_NAME_LINE( name, line ) INTERNAL_CATCH_UNIQUE_NAME_LINE2( name, line )
#define INTERNAL_CATCH_UNIQUE_NAME( name ) INTERNAL_CATCH_UNIQUE_NAME_LINE( name, __LINE__ )

#define INTERNAL_CATCH_STRINGIFY2( expr ) #expr
#define INTERNAL_CATCH_STRINGIFY( expr ) INTERNAL_CATCH_STRINGIFY2( expr )

#include <sstream>
#include <stdexcept>
#include <algorithm>


#define TWOBLUECUBES_CATCH_COMPILER_CAPABILITIES_HPP_INCLUDED





























#ifdef __clang__

#  if __has_feature(cxx_nullptr)
#    define CATCH_INTERNAL_CONFIG_CPP11_NULLPTR
#  endif

#  if __has_feature(cxx_noexcept)
#    define CATCH_INTERNAL_CONFIG_CPP11_NOEXCEPT
#  endif

#endif



#ifdef __BORLANDC__

#endif



#ifdef __EDG_VERSION__

#endif



#ifdef __DMC__

#endif



#ifdef __GNUC__

#if __GNUC__ == 4 && __GNUC_MINOR__ >= 6 && defined(__GXX_EXPERIMENTAL_CXX0X__)
#   define CATCH_INTERNAL_CONFIG_CPP11_NULLPTR
#endif




#endif



#ifdef _MSC_VER

#if (_MSC_VER >= 1600)
#   define CATCH_INTERNAL_CONFIG_CPP11_NULLPTR
#   define CATCH_INTERNAL_CONFIG_CPP11_UNIQUE_PTR
#endif

#if (_MSC_VER >= 1900 )
#define CATCH_INTERNAL_CONFIG_CPP11_NOEXCEPT
#define CATCH_INTERNAL_CONFIG_CPP11_GENERATED_METHODS
#endif

#endif




#if ( defined _MSC_VER && _MSC_VER > 1400 && !defined __EDGE__) || \
    ( defined __WAVE__ && __WAVE_HAS_VARIADICS ) || \
    ( defined __GNUC__ && __GNUC__ >= 3 ) || \
    ( !defined __cplusplus && __STDC_VERSION__ >= 199901L || __cplusplus >= 201103L )

#define CATCH_INTERNAL_CONFIG_VARIADIC_MACROS

#endif





#if defined(__cplusplus) && __cplusplus >= 201103L

#  define CATCH_CPP11_OR_GREATER

#  if !defined(CATCH_INTERNAL_CONFIG_CPP11_NULLPTR)
#    define CATCH_INTERNAL_CONFIG_CPP11_NULLPTR
#  endif

#  ifndef CATCH_INTERNAL_CONFIG_CPP11_NOEXCEPT
#    define CATCH_INTERNAL_CONFIG_CPP11_NOEXCEPT
#  endif

#  ifndef CATCH_INTERNAL_CONFIG_CPP11_GENERATED_METHODS
#    define CATCH_INTERNAL_CONFIG_CPP11_GENERATED_METHODS
#  endif

#  ifndef CATCH_INTERNAL_CONFIG_CPP11_IS_ENUM
#    define CATCH_INTERNAL_CONFIG_CPP11_IS_ENUM
#  endif

#  ifndef CATCH_INTERNAL_CONFIG_CPP11_TUPLE
#    define CATCH_INTERNAL_CONFIG_CPP11_TUPLE
#  endif

#  ifndef CATCH_INTERNAL_CONFIG_VARIADIC_MACROS
#    define CATCH_INTERNAL_CONFIG_VARIADIC_MACROS
#  endif

#  if !defined(CATCH_INTERNAL_CONFIG_CPP11_LONG_LONG)
#    define CATCH_INTERNAL_CONFIG_CPP11_LONG_LONG
#  endif

#  if !defined(CATCH_INTERNAL_CONFIG_CPP11_OVERRIDE)
#    define CATCH_INTERNAL_CONFIG_CPP11_OVERRIDE
#  endif
#  if !defined(CATCH_INTERNAL_CONFIG_CPP11_UNIQUE_PTR)
#    define CATCH_INTERNAL_CONFIG_CPP11_UNIQUE_PTR
#  endif

#endif


#if defined(CATCH_INTERNAL_CONFIG_CPP11_NULLPTR) && !defined(CATCH_CONFIG_CPP11_NO_NULLPTR) && !defined(CATCH_CONFIG_CPP11_NULLPTR) && !defined(CATCH_CONFIG_NO_CPP11)
#   define CATCH_CONFIG_CPP11_NULLPTR
#endif
#if defined(CATCH_INTERNAL_CONFIG_CPP11_NOEXCEPT) && !defined(CATCH_CONFIG_CPP11_NO_NOEXCEPT) && !defined(CATCH_CONFIG_CPP11_NOEXCEPT) && !defined(CATCH_CONFIG_NO_CPP11)
#   define CATCH_CONFIG_CPP11_NOEXCEPT
#endif
#if defined(CATCH_INTERNAL_CONFIG_CPP11_GENERATED_METHODS) && !defined(CATCH_CONFIG_CPP11_NO_GENERATED_METHODS) && !defined(CATCH_CONFIG_CPP11_GENERATED_METHODS) && !defined(CATCH_CONFIG_NO_CPP11)
#   define CATCH_CONFIG_CPP11_GENERATED_METHODS
#endif
#if defined(CATCH_INTERNAL_CONFIG_CPP11_IS_ENUM) && !defined(CATCH_CONFIG_CPP11_NO_IS_ENUM) && !defined(CATCH_CONFIG_CPP11_IS_ENUM) && !defined(CATCH_CONFIG_NO_CPP11)
#   define CATCH_CONFIG_CPP11_IS_ENUM
#endif
#if defined(CATCH_INTERNAL_CONFIG_CPP11_TUPLE) && !defined(CATCH_CONFIG_CPP11_NO_TUPLE) && !defined(CATCH_CONFIG_CPP11_TUPLE) && !defined(CATCH_CONFIG_NO_CPP11)
#   define CATCH_CONFIG_CPP11_TUPLE
#endif
#if defined(CATCH_INTERNAL_CONFIG_VARIADIC_MACROS) && !defined(CATCH_CONFIG_NO_VARIADIC_MACROS) && !defined(CATCH_CONFIG_VARIADIC_MACROS)
#   define CATCH_CONFIG_VARIADIC_MACROS
#endif
#if defined(CATCH_INTERNAL_CONFIG_CPP11_LONG_LONG) && !defined(CATCH_CONFIG_NO_LONG_LONG) && !defined(CATCH_CONFIG_CPP11_LONG_LONG) && !defined(CATCH_CONFIG_NO_CPP11)
#   define CATCH_CONFIG_CPP11_LONG_LONG
#endif
#if defined(CATCH_INTERNAL_CONFIG_CPP11_OVERRIDE) && !defined(CATCH_CONFIG_NO_OVERRIDE) && !defined(CATCH_CONFIG_CPP11_OVERRIDE) && !defined(CATCH_CONFIG_NO_CPP11)
#   define CATCH_CONFIG_CPP11_OVERRIDE
#endif
#if defined(CATCH_INTERNAL_CONFIG_CPP11_UNIQUE_PTR) && !defined(CATCH_CONFIG_NO_UNIQUE_PTR) && !defined(CATCH_CONFIG_CPP11_UNIQUE_PTR) && !defined(CATCH_CONFIG_NO_CPP11)
#   define CATCH_CONFIG_CPP11_UNIQUE_PTR
#endif


#if defined(CATCH_CONFIG_CPP11_NOEXCEPT) && !defined(CATCH_NOEXCEPT)
#  define CATCH_NOEXCEPT noexcept
#  define CATCH_NOEXCEPT_IS(x) noexcept(x)
#else
#  define CATCH_NOEXCEPT throw()
#  define CATCH_NOEXCEPT_IS(x)
#endif


#ifdef CATCH_CONFIG_CPP11_NULLPTR
#   define CATCH_NULL nullptr
#else
#   define CATCH_NULL NULL
#endif


#ifdef CATCH_CONFIG_CPP11_OVERRIDE
#   define CATCH_OVERRIDE override
#else
#   define CATCH_OVERRIDE
#endif


#ifdef CATCH_CONFIG_CPP11_UNIQUE_PTR
#   define CATCH_AUTO_PTR( T ) std::unique_ptr<T>
#else
#   define CATCH_AUTO_PTR( T ) std::auto_ptr<T>
#endif

namespace Catch {

    struct IConfig;

    struct CaseSensitive { enum Choice {
        Yes,
        No
    }; };

    class NonCopyable {
#ifdef CATCH_CONFIG_CPP11_GENERATED_METHODS
        NonCopyable( NonCopyable const& )              = delete;
        NonCopyable( NonCopyable && )                  = delete;
        NonCopyable& operator = ( NonCopyable const& ) = delete;
        NonCopyable& operator = ( NonCopyable && )     = delete;
#else
        NonCopyable( NonCopyable const& info );
        NonCopyable& operator = ( NonCopyable const& );
#endif

    protected:
        NonCopyable() {}
        virtual ~NonCopyable();
    };

    class SafeBool {
    public:
        typedef void (SafeBool::*type)() const;

        static type makeSafe( bool value ) {
            return value ? &SafeBool::trueValue : 0;
        }
    private:
        void trueValue() const {}
    };

    template<typename ContainerT>
    inline void deleteAll( ContainerT& container ) {
        typename ContainerT::const_iterator it = container.begin();
        typename ContainerT::const_iterator itEnd = container.end();
        for(; it != itEnd; ++it )
            delete *it;
    }
    template<typename AssociativeContainerT>
    inline void deleteAllValues( AssociativeContainerT& container ) {
        typename AssociativeContainerT::const_iterator it = container.begin();
        typename AssociativeContainerT::const_iterator itEnd = container.end();
        for(; it != itEnd; ++it )
            delete it->second;
    }

    bool startsWith( std::string const& s, std::string const& prefix );
    bool endsWith( std::string const& s, std::string const& suffix );
    bool contains( std::string const& s, std::string const& infix );
    void toLowerInPlace( std::string& s );
    std::string toLower( std::string const& s );
    std::string trim( std::string const& str );
    bool replaceInPlace( std::string& str, std::string const& replaceThis, std::string const& withThis );

    struct pluralise {
        pluralise( std::size_t count, std::string const& label );

        friend std::ostream& operator << ( std::ostream& os, pluralise const& pluraliser );

        std::size_t m_count;
        std::string m_label;
    };

    struct SourceLineInfo {

        SourceLineInfo();
        SourceLineInfo( char const* _file, std::size_t _line );
        SourceLineInfo( SourceLineInfo const& other );
#  ifdef CATCH_CONFIG_CPP11_GENERATED_METHODS
        SourceLineInfo( SourceLineInfo && )                  = default;
        SourceLineInfo& operator = ( SourceLineInfo const& ) = default;
        SourceLineInfo& operator = ( SourceLineInfo && )     = default;
#  endif
        bool empty() const;
        bool operator == ( SourceLineInfo const& other ) const;
        bool operator < ( SourceLineInfo const& other ) const;

        std::string file;
        std::size_t line;
    };

    std::ostream& operator << ( std::ostream& os, SourceLineInfo const& info );


    inline bool isTrue( bool value ){ return value; }
    inline bool alwaysTrue() { return true; }
    inline bool alwaysFalse() { return false; }

    void throwLogicError( std::string const& message, SourceLineInfo const& locationInfo );

    void seedRng( IConfig const& config );
    unsigned int rngSeed();





    struct StreamEndStop {
        std::string operator+() {
            return std::string();
        }
    };
    template<typename T>
    T const& operator + ( T const& value, StreamEndStop ) {
        return value;
    }
}

#define CATCH_INTERNAL_LINEINFO ::Catch::SourceLineInfo( __FILE__, static_cast<std::size_t>( __LINE__ ) )
#define CATCH_INTERNAL_ERROR( msg ) ::Catch::throwLogicError( msg, CATCH_INTERNAL_LINEINFO );

#include <ostream>

namespace Catch {

    class NotImplementedException : public std::exception
    {
    public:
        NotImplementedException( SourceLineInfo const& lineInfo );
        NotImplementedException( NotImplementedException const& ) {}

        virtual ~NotImplementedException() CATCH_NOEXCEPT {}

        virtual const char* what() const CATCH_NOEXCEPT;

    private:
        std::string m_what;
        SourceLineInfo m_lineInfo;
    };

}


#define CATCH_NOT_IMPLEMENTED throw Catch::NotImplementedException( CATCH_INTERNAL_LINEINFO )


#define TWOBLUECUBES_CATCH_CONTEXT_H_INCLUDED


#define TWOBLUECUBES_CATCH_INTERFACES_GENERATORS_H_INCLUDED

#include <string>

namespace Catch {

    struct IGeneratorInfo {
        virtual ~IGeneratorInfo();
        virtual bool moveNext() = 0;
        virtual std::size_t getCurrentIndex() const = 0;
    };

    struct IGeneratorsForTest {
        virtual ~IGeneratorsForTest();

        virtual IGeneratorInfo& getGeneratorInfo( std::string const& fileInfo, std::size_t size ) = 0;
        virtual bool moveNext() = 0;
    };

    IGeneratorsForTest* createGeneratorsForTest();

}


#define TWOBLUECUBES_CATCH_PTR_HPP_INCLUDED

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace Catch {




    template<typename T>
    class Ptr {
    public:
        Ptr() : m_p( CATCH_NULL ){}
        Ptr( T* p ) : m_p( p ){
            if( m_p )
                m_p->addRef();
        }
        Ptr( Ptr const& other ) : m_p( other.m_p ){
            if( m_p )
                m_p->addRef();
        }
        ~Ptr(){
            if( m_p )
                m_p->release();
        }
        void reset() {
            if( m_p )
                m_p->release();
            m_p = CATCH_NULL;
        }
        Ptr& operator = ( T* p ){
            Ptr temp( p );
            swap( temp );
            return *this;
        }
        Ptr& operator = ( Ptr const& other ){
            Ptr temp( other );
            swap( temp );
            return *this;
        }
        void swap( Ptr& other ) { std::swap( m_p, other.m_p ); }
        T* get() const{ return m_p; }
        T& operator*() const { return *m_p; }
        T* operator->() const { return m_p; }
        bool operator !() const { return m_p == CATCH_NULL; }
        operator SafeBool::type() const { return SafeBool::makeSafe( m_p != CATCH_NULL ); }

    private:
        T* m_p;
    };

    struct IShared : NonCopyable {
        virtual ~IShared();
        virtual void addRef() const = 0;
        virtual void release() const = 0;
    };

    template<typename T = IShared>
    struct SharedImpl : T {

        SharedImpl() : m_rc( 0 ){}

        virtual void addRef() const {
            ++m_rc;
        }
        virtual void release() const {
            if( --m_rc == 0 )
                delete this;
        }

        mutable unsigned int m_rc;
    };

}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <memory>
#include <vector>
#include <stdlib.h>

namespace Catch {

    class TestCase;
    class Stream;
    struct IResultCapture;
    struct IRunner;
    struct IGeneratorsForTest;
    struct IConfig;

    struct IContext
    {
        virtual ~IContext();

        virtual IResultCapture* getResultCapture() = 0;
        virtual IRunner* getRunner() = 0;
        virtual size_t getGeneratorIndex( std::string const& fileInfo, size_t totalSize ) = 0;
        virtual bool advanceGeneratorsForCurrentTest() = 0;
        virtual Ptr<IConfig const> getConfig() const = 0;
    };

    struct IMutableContext : IContext
    {
        virtual ~IMutableContext();
        virtual void setResultCapture( IResultCapture* resultCapture ) = 0;
        virtual void setRunner( IRunner* runner ) = 0;
        virtual void setConfig( Ptr<IConfig const> const& config ) = 0;
    };

    IContext& getCurrentContext();
    IMutableContext& getCurrentMutableContext();
    void cleanUpContext();
    Stream createStream( std::string const& streamName );

}


#define TWOBLUECUBES_CATCH_TEST_REGISTRY_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_INTERFACES_TESTCASE_H_INCLUDED

#include <vector>

namespace Catch {

    class TestSpec;

    struct ITestCase : IShared {
        virtual void invoke () const = 0;
    protected:
        virtual ~ITestCase();
    };

    class TestCase;
    struct IConfig;

    struct ITestCaseRegistry {
        virtual ~ITestCaseRegistry();
        virtual std::vector<TestCase> const& getAllTests() const = 0;
        virtual std::vector<TestCase> const& getAllTestsSorted( IConfig const& config ) const = 0;
    };

    bool matchTest( TestCase const& testCase, TestSpec const& testSpec, IConfig const& config );
    std::vector<TestCase> filterTests( std::vector<TestCase> const& testCases, TestSpec const& testSpec, IConfig const& config );
    std::vector<TestCase> const& getAllTestCasesSorted( IConfig const& config );

}

namespace Catch {

template<typename C>
class MethodTestCase : public SharedImpl<ITestCase> {

public:
    MethodTestCase( void (C::*method)() ) : m_method( method ) {}

    virtual void invoke() const {
        C obj;
        (obj.*m_method)();
    }

private:
    virtual ~MethodTestCase() {}

    void (C::*m_method)();
};

typedef void(*TestFunction)();

struct NameAndDesc {
    NameAndDesc( const char* _name = "", const char* _description= "" )
    : name( _name ), description( _description )
    {}

    const char* name;
    const char* description;
};

void registerTestCase
    (   ITestCase* testCase,
        char const* className,
        NameAndDesc const& nameAndDesc,
        SourceLineInfo const& lineInfo );

struct AutoReg {

    AutoReg
        (   TestFunction function,
            SourceLineInfo const& lineInfo,
            NameAndDesc const& nameAndDesc );

    template<typename C>
    AutoReg
        (   void (C::*method)(),
            char const* className,
            NameAndDesc const& nameAndDesc,
            SourceLineInfo const& lineInfo ) {

        registerTestCase
            (   new MethodTestCase<C>( method ),
                className,
                nameAndDesc,
                lineInfo );
    }

    ~AutoReg();

private:
    AutoReg( AutoReg const& );
    void operator= ( AutoReg const& );
};

void registerTestCaseFunction
    (   TestFunction function,
        SourceLineInfo const& lineInfo,
        NameAndDesc const& nameAndDesc );

}

#ifdef CATCH_CONFIG_VARIADIC_MACROS

    #define INTERNAL_CATCH_TESTCASE( ... ) \
        static void INTERNAL_CATCH_UNIQUE_NAME( ____C_A_T_C_H____T_E_S_T____ )(); \
        namespace{ Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar )( &INTERNAL_CATCH_UNIQUE_NAME(  ____C_A_T_C_H____T_E_S_T____ ), CATCH_INTERNAL_LINEINFO, Catch::NameAndDesc( __VA_ARGS__ ) ); }\
        static void INTERNAL_CATCH_UNIQUE_NAME(  ____C_A_T_C_H____T_E_S_T____ )()


    #define INTERNAL_CATCH_METHOD_AS_TEST_CASE( QualifiedMethod, ... ) \
        namespace{ Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar )( &QualifiedMethod, "&" #QualifiedMethod, Catch::NameAndDesc( __VA_ARGS__ ), CATCH_INTERNAL_LINEINFO ); }


    #define INTERNAL_CATCH_TEST_CASE_METHOD( ClassName, ... )\
        namespace{ \
            struct INTERNAL_CATCH_UNIQUE_NAME( ____C_A_T_C_H____T_E_S_T____ ) : ClassName{ \
                void test(); \
            }; \
            Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar ) ( &INTERNAL_CATCH_UNIQUE_NAME( ____C_A_T_C_H____T_E_S_T____ )::test, #ClassName, Catch::NameAndDesc( __VA_ARGS__ ), CATCH_INTERNAL_LINEINFO ); \
        } \
        void INTERNAL_CATCH_UNIQUE_NAME( ____C_A_T_C_H____T_E_S_T____ )::test()


    #define INTERNAL_CATCH_REGISTER_TESTCASE( Function, ... ) \
        Catch::AutoReg( Function, CATCH_INTERNAL_LINEINFO, Catch::NameAndDesc( __VA_ARGS__ ) );

#else

    #define INTERNAL_CATCH_TESTCASE( Name, Desc ) \
        static void INTERNAL_CATCH_UNIQUE_NAME( ____C_A_T_C_H____T_E_S_T____ )(); \
        namespace{ Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar )( &INTERNAL_CATCH_UNIQUE_NAME(  ____C_A_T_C_H____T_E_S_T____ ), CATCH_INTERNAL_LINEINFO, Catch::NameAndDesc( Name, Desc ) ); }\
        static void INTERNAL_CATCH_UNIQUE_NAME(  ____C_A_T_C_H____T_E_S_T____ )()


    #define INTERNAL_CATCH_METHOD_AS_TEST_CASE( QualifiedMethod, Name, Desc ) \
        namespace{ Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar )( &QualifiedMethod, "&" #QualifiedMethod, Catch::NameAndDesc( Name, Desc ), CATCH_INTERNAL_LINEINFO ); }


    #define INTERNAL_CATCH_TEST_CASE_METHOD( ClassName, TestName, Desc )\
        namespace{ \
            struct INTERNAL_CATCH_UNIQUE_NAME( ____C_A_T_C_H____T_E_S_T____ ) : ClassName{ \
                void test(); \
            }; \
            Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar ) ( &INTERNAL_CATCH_UNIQUE_NAME( ____C_A_T_C_H____T_E_S_T____ )::test, #ClassName, Catch::NameAndDesc( TestName, Desc ), CATCH_INTERNAL_LINEINFO ); \
        } \
        void INTERNAL_CATCH_UNIQUE_NAME( ____C_A_T_C_H____T_E_S_T____ )::test()


    #define INTERNAL_CATCH_REGISTER_TESTCASE( Function, Name, Desc ) \
        Catch::AutoReg( Function, CATCH_INTERNAL_LINEINFO, Catch::NameAndDesc( Name, Desc ) );
#endif


#define TWOBLUECUBES_CATCH_CAPTURE_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_RESULT_BUILDER_H_INCLUDED


#define TWOBLUECUBES_CATCH_RESULT_TYPE_H_INCLUDED

namespace Catch {


    struct ResultWas { enum OfType {
        Unknown = -1,
        Ok = 0,
        Info = 1,
        Warning = 2,

        FailureBit = 0x10,

        ExpressionFailed = FailureBit | 1,
        ExplicitFailure = FailureBit | 2,

        Exception = 0x100 | FailureBit,

        ThrewException = Exception | 1,
        DidntThrowException = Exception | 2,

        FatalErrorCondition = 0x200 | FailureBit

    }; };

    inline bool isOk( ResultWas::OfType resultType ) {
        return ( resultType & ResultWas::FailureBit ) == 0;
    }
    inline bool isJustInfo( int flags ) {
        return flags == ResultWas::Info;
    }


    struct ResultDisposition { enum Flags {
        Normal = 0x01,

        ContinueOnFailure = 0x02,
        FalseTest = 0x04,
        SuppressFail = 0x08
    }; };

    inline ResultDisposition::Flags operator | ( ResultDisposition::Flags lhs, ResultDisposition::Flags rhs ) {
        return static_cast<ResultDisposition::Flags>( static_cast<int>( lhs ) | static_cast<int>( rhs ) );
    }

    inline bool shouldContinueOnFailure( int flags )    { return ( flags & ResultDisposition::ContinueOnFailure ) != 0; }
    inline bool isFalseTest( int flags )                { return ( flags & ResultDisposition::FalseTest ) != 0; }
    inline bool shouldSuppressFailure( int flags )      { return ( flags & ResultDisposition::SuppressFail ) != 0; }

}


#define TWOBLUECUBES_CATCH_ASSERTIONRESULT_H_INCLUDED

#include <string>

namespace Catch {

    struct AssertionInfo
    {
        AssertionInfo() {}
        AssertionInfo(  std::string const& _macroName,
                        SourceLineInfo const& _lineInfo,
                        std::string const& _capturedExpression,
                        ResultDisposition::Flags _resultDisposition );

        std::string macroName;
        SourceLineInfo lineInfo;
        std::string capturedExpression;
        ResultDisposition::Flags resultDisposition;
    };

    struct AssertionResultData
    {
        AssertionResultData() : resultType( ResultWas::Unknown ) {}

        std::string reconstructedExpression;
        std::string message;
        ResultWas::OfType resultType;
    };

    class AssertionResult {
    public:
        AssertionResult();
        AssertionResult( AssertionInfo const& info, AssertionResultData const& data );
        ~AssertionResult();
#  ifdef CATCH_CONFIG_CPP11_GENERATED_METHODS
         AssertionResult( AssertionResult const& )              = default;
         AssertionResult( AssertionResult && )                  = default;
         AssertionResult& operator = ( AssertionResult const& ) = default;
         AssertionResult& operator = ( AssertionResult && )     = default;
#  endif

        bool isOk() const;
        bool succeeded() const;
        ResultWas::OfType getResultType() const;
        bool hasExpression() const;
        bool hasMessage() const;
        std::string getExpression() const;
        std::string getExpressionInMacro() const;
        bool hasExpandedExpression() const;
        std::string getExpandedExpression() const;
        std::string getMessage() const;
        SourceLineInfo getSourceInfo() const;
        std::string getTestMacroName() const;

    protected:
        AssertionInfo m_info;
        AssertionResultData m_resultData;
    };

}


#define TWOBLUECUBES_CATCH_MATCHERS_HPP_INCLUDED

namespace Catch {
namespace Matchers {
    namespace Impl {

    namespace Generic {
        template<typename ExpressionT> class AllOf;
        template<typename ExpressionT> class AnyOf;
        template<typename ExpressionT> class Not;
    }

    template<typename ExpressionT>
    struct Matcher : SharedImpl<IShared>
    {
        typedef ExpressionT ExpressionType;

        virtual ~Matcher() {}
        virtual Ptr<Matcher> clone() const = 0;
        virtual bool match( ExpressionT const& expr ) const = 0;
        virtual std::string toString() const = 0;

        Generic::AllOf<ExpressionT> operator && ( Matcher<ExpressionT> const& other ) const;
        Generic::AnyOf<ExpressionT> operator || ( Matcher<ExpressionT> const& other ) const;
        Generic::Not<ExpressionT> operator ! () const;
    };

    template<typename DerivedT, typename ExpressionT>
    struct MatcherImpl : Matcher<ExpressionT> {

        virtual Ptr<Matcher<ExpressionT> > clone() const {
            return Ptr<Matcher<ExpressionT> >( new DerivedT( static_cast<DerivedT const&>( *this ) ) );
        }
    };

    namespace Generic {
        template<typename ExpressionT>
        class Not : public MatcherImpl<Not<ExpressionT>, ExpressionT> {
        public:
            explicit Not( Matcher<ExpressionT> const& matcher ) : m_matcher(matcher.clone()) {}
            Not( Not const& other ) : m_matcher( other.m_matcher ) {}

            virtual bool match( ExpressionT const& expr ) const CATCH_OVERRIDE {
                return !m_matcher->match( expr );
            }

            virtual std::string toString() const CATCH_OVERRIDE {
                return "not " + m_matcher->toString();
            }
        private:
            Ptr< Matcher<ExpressionT> > m_matcher;
        };

        template<typename ExpressionT>
        class AllOf : public MatcherImpl<AllOf<ExpressionT>, ExpressionT> {
        public:

            AllOf() {}
            AllOf( AllOf const& other ) : m_matchers( other.m_matchers ) {}

            AllOf& add( Matcher<ExpressionT> const& matcher ) {
                m_matchers.push_back( matcher.clone() );
                return *this;
            }
            virtual bool match( ExpressionT const& expr ) const
            {
                for( std::size_t i = 0; i < m_matchers.size(); ++i )
                    if( !m_matchers[i]->match( expr ) )
                        return false;
                return true;
            }
            virtual std::string toString() const {
                std::ostringstream oss;
                oss << "( ";
                for( std::size_t i = 0; i < m_matchers.size(); ++i ) {
                    if( i != 0 )
                        oss << " and ";
                    oss << m_matchers[i]->toString();
                }
                oss << " )";
                return oss.str();
            }

            AllOf operator && ( Matcher<ExpressionT> const& other ) const {
                AllOf allOfExpr( *this );
                allOfExpr.add( other );
                return allOfExpr;
            }

        private:
            std::vector<Ptr<Matcher<ExpressionT> > > m_matchers;
        };

        template<typename ExpressionT>
        class AnyOf : public MatcherImpl<AnyOf<ExpressionT>, ExpressionT> {
        public:

            AnyOf() {}
            AnyOf( AnyOf const& other ) : m_matchers( other.m_matchers ) {}

            AnyOf& add( Matcher<ExpressionT> const& matcher ) {
                m_matchers.push_back( matcher.clone() );
                return *this;
            }
            virtual bool match( ExpressionT const& expr ) const
            {
                for( std::size_t i = 0; i < m_matchers.size(); ++i )
                    if( m_matchers[i]->match( expr ) )
                        return true;
                return false;
            }
            virtual std::string toString() const {
                std::ostringstream oss;
                oss << "( ";
                for( std::size_t i = 0; i < m_matchers.size(); ++i ) {
                    if( i != 0 )
                        oss << " or ";
                    oss << m_matchers[i]->toString();
                }
                oss << " )";
                return oss.str();
            }

            AnyOf operator || ( Matcher<ExpressionT> const& other ) const {
                AnyOf anyOfExpr( *this );
                anyOfExpr.add( other );
                return anyOfExpr;
            }

        private:
            std::vector<Ptr<Matcher<ExpressionT> > > m_matchers;
        };

    }

    template<typename ExpressionT>
    Generic::AllOf<ExpressionT> Matcher<ExpressionT>::operator && ( Matcher<ExpressionT> const& other ) const {
        Generic::AllOf<ExpressionT> allOfExpr;
        allOfExpr.add( *this );
        allOfExpr.add( other );
        return allOfExpr;
    }

    template<typename ExpressionT>
    Generic::AnyOf<ExpressionT> Matcher<ExpressionT>::operator || ( Matcher<ExpressionT> const& other ) const {
        Generic::AnyOf<ExpressionT> anyOfExpr;
        anyOfExpr.add( *this );
        anyOfExpr.add( other );
        return anyOfExpr;
    }

    template<typename ExpressionT>
    Generic::Not<ExpressionT> Matcher<ExpressionT>::operator ! () const {
        return Generic::Not<ExpressionT>( *this );
    }

    namespace StdString {

        inline std::string makeString( std::string const& str ) { return str; }
        inline std::string makeString( const char* str ) { return str ? std::string( str ) : std::string(); }

        struct CasedString
        {
            CasedString( std::string const& str, CaseSensitive::Choice caseSensitivity )
            :   m_caseSensitivity( caseSensitivity ),
                m_str( adjustString( str ) )
            {}
            std::string adjustString( std::string const& str ) const {
                return m_caseSensitivity == CaseSensitive::No
                    ? toLower( str )
                    : str;

            }
            std::string toStringSuffix() const
            {
                return m_caseSensitivity == CaseSensitive::No
                    ? " (case insensitive)"
                    : "";
            }
            CaseSensitive::Choice m_caseSensitivity;
            std::string m_str;
        };

        struct Equals : MatcherImpl<Equals, std::string> {
            Equals( std::string const& str, CaseSensitive::Choice caseSensitivity = CaseSensitive::Yes )
            :   m_data( str, caseSensitivity )
            {}
            Equals( Equals const& other ) : m_data( other.m_data ){}

            virtual ~Equals();

            virtual bool match( std::string const& expr ) const {
                return m_data.m_str == m_data.adjustString( expr );;
            }
            virtual std::string toString() const {
                return "equals: \"" + m_data.m_str + "\"" + m_data.toStringSuffix();
            }

            CasedString m_data;
        };

        struct Contains : MatcherImpl<Contains, std::string> {
            Contains( std::string const& substr, CaseSensitive::Choice caseSensitivity = CaseSensitive::Yes )
            : m_data( substr, caseSensitivity ){}
            Contains( Contains const& other ) : m_data( other.m_data ){}

            virtual ~Contains();

            virtual bool match( std::string const& expr ) const {
                return m_data.adjustString( expr ).find( m_data.m_str ) != std::string::npos;
            }
            virtual std::string toString() const {
                return "contains: \"" + m_data.m_str  + "\"" + m_data.toStringSuffix();
            }

            CasedString m_data;
        };

        struct StartsWith : MatcherImpl<StartsWith, std::string> {
            StartsWith( std::string const& substr, CaseSensitive::Choice caseSensitivity = CaseSensitive::Yes )
            : m_data( substr, caseSensitivity ){}

            StartsWith( StartsWith const& other ) : m_data( other.m_data ){}

            virtual ~StartsWith();

            virtual bool match( std::string const& expr ) const {
                return startsWith( m_data.adjustString( expr ), m_data.m_str );
            }
            virtual std::string toString() const {
                return "starts with: \"" + m_data.m_str + "\"" + m_data.toStringSuffix();
            }

            CasedString m_data;
        };

        struct EndsWith : MatcherImpl<EndsWith, std::string> {
            EndsWith( std::string const& substr, CaseSensitive::Choice caseSensitivity = CaseSensitive::Yes )
            : m_data( substr, caseSensitivity ){}
            EndsWith( EndsWith const& other ) : m_data( other.m_data ){}

            virtual ~EndsWith();

            virtual bool match( std::string const& expr ) const {
                return endsWith( m_data.adjustString( expr ), m_data.m_str );
            }
            virtual std::string toString() const {
                return "ends with: \"" + m_data.m_str + "\"" + m_data.toStringSuffix();
            }

            CasedString m_data;
        };
    }
    }



    template<typename ExpressionT>
    inline Impl::Generic::Not<ExpressionT> Not( Impl::Matcher<ExpressionT> const& m ) {
        return Impl::Generic::Not<ExpressionT>( m );
    }

    template<typename ExpressionT>
    inline Impl::Generic::AllOf<ExpressionT> AllOf( Impl::Matcher<ExpressionT> const& m1,
                                                    Impl::Matcher<ExpressionT> const& m2 ) {
        return Impl::Generic::AllOf<ExpressionT>().add( m1 ).add( m2 );
    }
    template<typename ExpressionT>
    inline Impl::Generic::AllOf<ExpressionT> AllOf( Impl::Matcher<ExpressionT> const& m1,
                                                    Impl::Matcher<ExpressionT> const& m2,
                                                    Impl::Matcher<ExpressionT> const& m3 ) {
        return Impl::Generic::AllOf<ExpressionT>().add( m1 ).add( m2 ).add( m3 );
    }
    template<typename ExpressionT>
    inline Impl::Generic::AnyOf<ExpressionT> AnyOf( Impl::Matcher<ExpressionT> const& m1,
                                                    Impl::Matcher<ExpressionT> const& m2 ) {
        return Impl::Generic::AnyOf<ExpressionT>().add( m1 ).add( m2 );
    }
    template<typename ExpressionT>
    inline Impl::Generic::AnyOf<ExpressionT> AnyOf( Impl::Matcher<ExpressionT> const& m1,
                                                    Impl::Matcher<ExpressionT> const& m2,
                                                    Impl::Matcher<ExpressionT> const& m3 ) {
        return Impl::Generic::AnyOf<ExpressionT>().add( m1 ).add( m2 ).add( m3 );
    }

    inline Impl::StdString::Equals      Equals( std::string const& str, CaseSensitive::Choice caseSensitivity = CaseSensitive::Yes ) {
        return Impl::StdString::Equals( str, caseSensitivity );
    }
    inline Impl::StdString::Equals      Equals( const char* str, CaseSensitive::Choice caseSensitivity = CaseSensitive::Yes ) {
        return Impl::StdString::Equals( Impl::StdString::makeString( str ), caseSensitivity );
    }
    inline Impl::StdString::Contains    Contains( std::string const& substr, CaseSensitive::Choice caseSensitivity = CaseSensitive::Yes ) {
        return Impl::StdString::Contains( substr, caseSensitivity );
    }
    inline Impl::StdString::Contains    Contains( const char* substr, CaseSensitive::Choice caseSensitivity = CaseSensitive::Yes ) {
        return Impl::StdString::Contains( Impl::StdString::makeString( substr ), caseSensitivity );
    }
    inline Impl::StdString::StartsWith  StartsWith( std::string const& substr ) {
        return Impl::StdString::StartsWith( substr );
    }
    inline Impl::StdString::StartsWith  StartsWith( const char* substr ) {
        return Impl::StdString::StartsWith( Impl::StdString::makeString( substr ) );
    }
    inline Impl::StdString::EndsWith    EndsWith( std::string const& substr ) {
        return Impl::StdString::EndsWith( substr );
    }
    inline Impl::StdString::EndsWith    EndsWith( const char* substr ) {
        return Impl::StdString::EndsWith( Impl::StdString::makeString( substr ) );
    }

}

using namespace Matchers;

}

namespace Catch {

    struct TestFailureException{};

    template<typename T> class ExpressionLhs;

    struct STATIC_ASSERT_Expression_Too_Complex_Please_Rewrite_As_Binary_Comparison;

    struct CopyableStream {
        CopyableStream() {}
        CopyableStream( CopyableStream const& other ) {
            oss << other.oss.str();
        }
        CopyableStream& operator=( CopyableStream const& other ) {
            oss.str("");
            oss << other.oss.str();
            return *this;
        }
        std::ostringstream oss;
    };

    class ResultBuilder {
    public:
        ResultBuilder(  char const* macroName,
                        SourceLineInfo const& lineInfo,
                        char const* capturedExpression,
                        ResultDisposition::Flags resultDisposition,
                        char const* secondArg = "" );

        template<typename T>
        ExpressionLhs<T const&> operator <= ( T const& operand );
        ExpressionLhs<bool> operator <= ( bool value );

        template<typename T>
        ResultBuilder& operator << ( T const& value ) {
            m_stream.oss << value;
            return *this;
        }

        template<typename RhsT> STATIC_ASSERT_Expression_Too_Complex_Please_Rewrite_As_Binary_Comparison& operator && ( RhsT const& );
        template<typename RhsT> STATIC_ASSERT_Expression_Too_Complex_Please_Rewrite_As_Binary_Comparison& operator || ( RhsT const& );

        ResultBuilder& setResultType( ResultWas::OfType result );
        ResultBuilder& setResultType( bool result );
        ResultBuilder& setLhs( std::string const& lhs );
        ResultBuilder& setRhs( std::string const& rhs );
        ResultBuilder& setOp( std::string const& op );

        void endExpression();

        std::string reconstructExpression() const;
        AssertionResult build() const;

        void useActiveException( ResultDisposition::Flags resultDisposition = ResultDisposition::Normal );
        void captureResult( ResultWas::OfType resultType );
        void captureExpression();
        void captureExpectedException( std::string const& expectedMessage );
        void captureExpectedException( Matchers::Impl::Matcher<std::string> const& matcher );
        void handleResult( AssertionResult const& result );
        void react();
        bool shouldDebugBreak() const;
        bool allowThrows() const;

    private:
        AssertionInfo m_assertionInfo;
        AssertionResultData m_data;
        struct ExprComponents {
            ExprComponents() : testFalse( false ) {}
            bool testFalse;
            std::string lhs, rhs, op;
        } m_exprComponents;
        CopyableStream m_stream;

        bool m_shouldDebugBreak;
        bool m_shouldThrow;
    };

}



#define TWOBLUECUBES_CATCH_EXPRESSION_LHS_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_EVALUATE_HPP_INCLUDED

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4389)
#endif

#include <cstddef>

namespace Catch {
namespace Internal {

    enum Operator {
        IsEqualTo,
        IsNotEqualTo,
        IsLessThan,
        IsGreaterThan,
        IsLessThanOrEqualTo,
        IsGreaterThanOrEqualTo
    };

    template<Operator Op> struct OperatorTraits             { static const char* getName(){ return "*error*"; } };
    template<> struct OperatorTraits<IsEqualTo>             { static const char* getName(){ return "=="; } };
    template<> struct OperatorTraits<IsNotEqualTo>          { static const char* getName(){ return "!="; } };
    template<> struct OperatorTraits<IsLessThan>            { static const char* getName(){ return "<"; } };
    template<> struct OperatorTraits<IsGreaterThan>         { static const char* getName(){ return ">"; } };
    template<> struct OperatorTraits<IsLessThanOrEqualTo>   { static const char* getName(){ return "<="; } };
    template<> struct OperatorTraits<IsGreaterThanOrEqualTo>{ static const char* getName(){ return ">="; } };

    template<typename T>
    inline T& opCast(T const& t) { return const_cast<T&>(t); }


#ifdef CATCH_CONFIG_CPP11_NULLPTR
    inline std::nullptr_t opCast(std::nullptr_t) { return nullptr; }
#endif



    template<typename T1, typename T2, Operator Op>
    class Evaluator{};

    template<typename T1, typename T2>
    struct Evaluator<T1, T2, IsEqualTo> {
        static bool evaluate( T1 const& lhs, T2 const& rhs) {
            return opCast( lhs ) ==  opCast( rhs );
        }
    };
    template<typename T1, typename T2>
    struct Evaluator<T1, T2, IsNotEqualTo> {
        static bool evaluate( T1 const& lhs, T2 const& rhs ) {
            return opCast( lhs ) != opCast( rhs );
        }
    };
    template<typename T1, typename T2>
    struct Evaluator<T1, T2, IsLessThan> {
        static bool evaluate( T1 const& lhs, T2 const& rhs ) {
            return opCast( lhs ) < opCast( rhs );
        }
    };
    template<typename T1, typename T2>
    struct Evaluator<T1, T2, IsGreaterThan> {
        static bool evaluate( T1 const& lhs, T2 const& rhs ) {
            return opCast( lhs ) > opCast( rhs );
        }
    };
    template<typename T1, typename T2>
    struct Evaluator<T1, T2, IsGreaterThanOrEqualTo> {
        static bool evaluate( T1 const& lhs, T2 const& rhs ) {
            return opCast( lhs ) >= opCast( rhs );
        }
    };
    template<typename T1, typename T2>
    struct Evaluator<T1, T2, IsLessThanOrEqualTo> {
        static bool evaluate( T1 const& lhs, T2 const& rhs ) {
            return opCast( lhs ) <= opCast( rhs );
        }
    };

    template<Operator Op, typename T1, typename T2>
    bool applyEvaluator( T1 const& lhs, T2 const& rhs ) {
        return Evaluator<T1, T2, Op>::evaluate( lhs, rhs );
    }





    template<Operator Op, typename T1, typename T2>
    bool compare( T1 const& lhs, T2 const& rhs ) {
        return Evaluator<T1, T2, Op>::evaluate( lhs, rhs );
    }


    template<Operator Op> bool compare( unsigned int lhs, int rhs ) {
        return applyEvaluator<Op>( lhs, static_cast<unsigned int>( rhs ) );
    }
    template<Operator Op> bool compare( unsigned long lhs, int rhs ) {
        return applyEvaluator<Op>( lhs, static_cast<unsigned int>( rhs ) );
    }
    template<Operator Op> bool compare( unsigned char lhs, int rhs ) {
        return applyEvaluator<Op>( lhs, static_cast<unsigned int>( rhs ) );
    }


    template<Operator Op> bool compare( unsigned int lhs, long rhs ) {
        return applyEvaluator<Op>( lhs, static_cast<unsigned long>( rhs ) );
    }
    template<Operator Op> bool compare( unsigned long lhs, long rhs ) {
        return applyEvaluator<Op>( lhs, static_cast<unsigned long>( rhs ) );
    }
    template<Operator Op> bool compare( unsigned char lhs, long rhs ) {
        return applyEvaluator<Op>( lhs, static_cast<unsigned long>( rhs ) );
    }


    template<Operator Op> bool compare( int lhs, unsigned int rhs ) {
        return applyEvaluator<Op>( static_cast<unsigned int>( lhs ), rhs );
    }
    template<Operator Op> bool compare( int lhs, unsigned long rhs ) {
        return applyEvaluator<Op>( static_cast<unsigned int>( lhs ), rhs );
    }
    template<Operator Op> bool compare( int lhs, unsigned char rhs ) {
        return applyEvaluator<Op>( static_cast<unsigned int>( lhs ), rhs );
    }


    template<Operator Op> bool compare( long lhs, unsigned int rhs ) {
        return applyEvaluator<Op>( static_cast<unsigned long>( lhs ), rhs );
    }
    template<Operator Op> bool compare( long lhs, unsigned long rhs ) {
        return applyEvaluator<Op>( static_cast<unsigned long>( lhs ), rhs );
    }
    template<Operator Op> bool compare( long lhs, unsigned char rhs ) {
        return applyEvaluator<Op>( static_cast<unsigned long>( lhs ), rhs );
    }


    template<Operator Op, typename T> bool compare( long lhs, T* rhs ) {
        return Evaluator<T*, T*, Op>::evaluate( reinterpret_cast<T*>( lhs ), rhs );
    }
    template<Operator Op, typename T> bool compare( T* lhs, long rhs ) {
        return Evaluator<T*, T*, Op>::evaluate( lhs, reinterpret_cast<T*>( rhs ) );
    }


    template<Operator Op, typename T> bool compare( int lhs, T* rhs ) {
        return Evaluator<T*, T*, Op>::evaluate( reinterpret_cast<T*>( lhs ), rhs );
    }
    template<Operator Op, typename T> bool compare( T* lhs, int rhs ) {
        return Evaluator<T*, T*, Op>::evaluate( lhs, reinterpret_cast<T*>( rhs ) );
    }

#ifdef CATCH_CONFIG_CPP11_LONG_LONG

    template<Operator Op> bool compare( long long lhs, unsigned int rhs ) {
        return applyEvaluator<Op>( static_cast<unsigned long>( lhs ), rhs );
    }
    template<Operator Op> bool compare( long long lhs, unsigned long rhs ) {
        return applyEvaluator<Op>( static_cast<unsigned long>( lhs ), rhs );
    }
    template<Operator Op> bool compare( long long lhs, unsigned long long rhs ) {
        return applyEvaluator<Op>( static_cast<unsigned long>( lhs ), rhs );
    }
    template<Operator Op> bool compare( long long lhs, unsigned char rhs ) {
        return applyEvaluator<Op>( static_cast<unsigned long>( lhs ), rhs );
    }


    template<Operator Op> bool compare( unsigned long long lhs, int rhs ) {
        return applyEvaluator<Op>( static_cast<long>( lhs ), rhs );
    }
    template<Operator Op> bool compare( unsigned long long lhs, long rhs ) {
        return applyEvaluator<Op>( static_cast<long>( lhs ), rhs );
    }
    template<Operator Op> bool compare( unsigned long long lhs, long long rhs ) {
        return applyEvaluator<Op>( static_cast<long>( lhs ), rhs );
    }
    template<Operator Op> bool compare( unsigned long long lhs, char rhs ) {
        return applyEvaluator<Op>( static_cast<long>( lhs ), rhs );
    }


    template<Operator Op, typename T> bool compare( long long lhs, T* rhs ) {
        return Evaluator<T*, T*, Op>::evaluate( reinterpret_cast<T*>( lhs ), rhs );
    }
    template<Operator Op, typename T> bool compare( T* lhs, long long rhs ) {
        return Evaluator<T*, T*, Op>::evaluate( lhs, reinterpret_cast<T*>( rhs ) );
    }
#endif

#ifdef CATCH_CONFIG_CPP11_NULLPTR

    template<Operator Op, typename T> bool compare( std::nullptr_t, T* rhs ) {
        return Evaluator<T*, T*, Op>::evaluate( nullptr, rhs );
    }
    template<Operator Op, typename T> bool compare( T* lhs, std::nullptr_t ) {
        return Evaluator<T*, T*, Op>::evaluate( lhs, nullptr );
    }
#endif

}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif


#define TWOBLUECUBES_CATCH_TOSTRING_H_INCLUDED

#include <sstream>
#include <iomanip>
#include <limits>
#include <vector>
#include <cstddef>

#ifdef __OBJC__

#define TWOBLUECUBES_CATCH_OBJC_ARC_HPP_INCLUDED

#import <Foundation/Foundation.h>

#ifdef __has_feature
#define CATCH_ARC_ENABLED __has_feature(objc_arc)
#else
#define CATCH_ARC_ENABLED 0
#endif

void arcSafeRelease( NSObject* obj );
id performOptionalSelector( id obj, SEL sel );

#if !CATCH_ARC_ENABLED
inline void arcSafeRelease( NSObject* obj ) {
    [obj release];
}
inline id performOptionalSelector( id obj, SEL sel ) {
    if( [obj respondsToSelector: sel] )
        return [obj performSelector: sel];
    return nil;
}
#define CATCH_UNSAFE_UNRETAINED
#define CATCH_ARC_STRONG
#else
inline void arcSafeRelease( NSObject* ){}
inline id performOptionalSelector( id obj, SEL sel ) {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
#endif
    if( [obj respondsToSelector: sel] )
        return [obj performSelector: sel];
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    return nil;
}
#define CATCH_UNSAFE_UNRETAINED __unsafe_unretained
#define CATCH_ARC_STRONG __strong
#endif

#endif

#ifdef CATCH_CONFIG_CPP11_TUPLE
#include <tuple>
#endif

#ifdef CATCH_CONFIG_CPP11_IS_ENUM
#include <type_traits>
#endif

namespace Catch {


template<typename T>
std::string toString( T const& value );



std::string toString( std::string const& value );
std::string toString( std::wstring const& value );
std::string toString( const char* const value );
std::string toString( char* const value );
std::string toString( const wchar_t* const value );
std::string toString( wchar_t* const value );
std::string toString( int value );
std::string toString( unsigned long value );
std::string toString( unsigned int value );
std::string toString( const double value );
std::string toString( const float value );
std::string toString( bool value );
std::string toString( char value );
std::string toString( signed char value );
std::string toString( unsigned char value );

#ifdef CATCH_CONFIG_CPP11_LONG_LONG
std::string toString( long long value );
std::string toString( unsigned long long value );
#endif

#ifdef CATCH_CONFIG_CPP11_NULLPTR
std::string toString( std::nullptr_t );
#endif

#ifdef __OBJC__
    std::string toString( NSString const * const& nsstring );
    std::string toString( NSString * CATCH_ARC_STRONG const& nsstring );
    std::string toString( NSObject* const& nsObject );
#endif

namespace Detail {

    extern const std::string unprintableString;

    struct BorgType {
        template<typename T> BorgType( T const& );
    };

    struct TrueType { char sizer[1]; };
    struct FalseType { char sizer[2]; };

    TrueType& testStreamable( std::ostream& );
    FalseType testStreamable( FalseType );

    FalseType operator<<( std::ostream const&, BorgType const& );

    template<typename T>
    struct IsStreamInsertable {
        static std::ostream &s;
        static T  const&t;
        enum { value = sizeof( testStreamable(s << t) ) == sizeof( TrueType ) };
    };

#if defined(CATCH_CONFIG_CPP11_IS_ENUM)
    template<typename T,
             bool IsEnum = std::is_enum<T>::value
             >
    struct EnumStringMaker
    {
        static std::string convert( T const& ) { return unprintableString; }
    };

    template<typename T>
    struct EnumStringMaker<T,true>
    {
        static std::string convert( T const& v )
        {
            return ::Catch::toString(
                static_cast<typename std::underlying_type<T>::type>(v)
                );
        }
    };
#endif
    template<bool C>
    struct StringMakerBase {
#if defined(CATCH_CONFIG_CPP11_IS_ENUM)
        template<typename T>
        static std::string convert( T const& v )
        {
            return EnumStringMaker<T>::convert( v );
        }
#else
        template<typename T>
        static std::string convert( T const& ) { return unprintableString; }
#endif
    };

    template<>
    struct StringMakerBase<true> {
        template<typename T>
        static std::string convert( T const& _value ) {
            std::ostringstream oss;
            oss << _value;
            return oss.str();
        }
    };

    std::string rawMemoryToString( const void *object, std::size_t size );

    template<typename T>
    inline std::string rawMemoryToString( const T& object ) {
      return rawMemoryToString( &object, sizeof(object) );
    }

}

template<typename T>
struct StringMaker :
    Detail::StringMakerBase<Detail::IsStreamInsertable<T>::value> {};

template<typename T>
struct StringMaker<T*> {
    template<typename U>
    static std::string convert( U* p ) {
        if( !p )
            return "NULL";
        else
            return Detail::rawMemoryToString( p );
    }
};

template<typename R, typename C>
struct StringMaker<R C::*> {
    static std::string convert( R C::* p ) {
        if( !p )
            return "NULL";
        else
            return Detail::rawMemoryToString( p );
    }
};

namespace Detail {
    template<typename InputIterator>
    std::string rangeToString( InputIterator first, InputIterator last );
}








template<typename T, typename Allocator>
std::string toString( std::vector<T,Allocator> const& v ) {
    return Detail::rangeToString( v.begin(), v.end() );
}

#ifdef CATCH_CONFIG_CPP11_TUPLE


namespace TupleDetail {
  template<
      typename Tuple,
      std::size_t N = 0,
      bool = (N < std::tuple_size<Tuple>::value)
      >
  struct ElementPrinter {
      static void print( const Tuple& tuple, std::ostream& os )
      {
          os << ( N ? ", " : " " )
             << Catch::toString(std::get<N>(tuple));
          ElementPrinter<Tuple,N+1>::print(tuple,os);
      }
  };

  template<
      typename Tuple,
      std::size_t N
      >
  struct ElementPrinter<Tuple,N,false> {
      static void print( const Tuple&, std::ostream& ) {}
  };

}

template<typename ...Types>
struct StringMaker<std::tuple<Types...>> {

    static std::string convert( const std::tuple<Types...>& tuple )
    {
        std::ostringstream os;
        os << '{';
        TupleDetail::ElementPrinter<std::tuple<Types...>>::print( tuple, os );
        os << " }";
        return os.str();
    }
};
#endif

namespace Detail {
    template<typename T>
    std::string makeString( T const& value ) {
        return StringMaker<T>::convert( value );
    }
}








template<typename T>
std::string toString( T const& value ) {
    return StringMaker<T>::convert( value );
}

    namespace Detail {
    template<typename InputIterator>
    std::string rangeToString( InputIterator first, InputIterator last ) {
        std::ostringstream oss;
        oss << "{ ";
        if( first != last ) {
            oss << Catch::toString( *first );
            for( ++first ; first != last ; ++first )
                oss << ", " << Catch::toString( *first );
        }
        oss << " }";
        return oss.str();
    }
}

}

namespace Catch {



template<typename T>
class ExpressionLhs {
    ExpressionLhs& operator = ( ExpressionLhs const& );
#  ifdef CATCH_CONFIG_CPP11_GENERATED_METHODS
    ExpressionLhs& operator = ( ExpressionLhs && ) = delete;
#  endif

public:
    ExpressionLhs( ResultBuilder& rb, T lhs ) : m_rb( rb ), m_lhs( lhs ) {}
#  ifdef CATCH_CONFIG_CPP11_GENERATED_METHODS
    ExpressionLhs( ExpressionLhs const& ) = default;
    ExpressionLhs( ExpressionLhs && )     = default;
#  endif

    template<typename RhsT>
    ResultBuilder& operator == ( RhsT const& rhs ) {
        return captureExpression<Internal::IsEqualTo>( rhs );
    }

    template<typename RhsT>
    ResultBuilder& operator != ( RhsT const& rhs ) {
        return captureExpression<Internal::IsNotEqualTo>( rhs );
    }

    template<typename RhsT>
    ResultBuilder& operator < ( RhsT const& rhs ) {
        return captureExpression<Internal::IsLessThan>( rhs );
    }

    template<typename RhsT>
    ResultBuilder& operator > ( RhsT const& rhs ) {
        return captureExpression<Internal::IsGreaterThan>( rhs );
    }

    template<typename RhsT>
    ResultBuilder& operator <= ( RhsT const& rhs ) {
        return captureExpression<Internal::IsLessThanOrEqualTo>( rhs );
    }

    template<typename RhsT>
    ResultBuilder& operator >= ( RhsT const& rhs ) {
        return captureExpression<Internal::IsGreaterThanOrEqualTo>( rhs );
    }

    ResultBuilder& operator == ( bool rhs ) {
        return captureExpression<Internal::IsEqualTo>( rhs );
    }

    ResultBuilder& operator != ( bool rhs ) {
        return captureExpression<Internal::IsNotEqualTo>( rhs );
    }

    void endExpression() {
        bool value = m_lhs ? true : false;
        m_rb
            .setLhs( Catch::toString( value ) )
            .setResultType( value )
            .endExpression();
    }



    template<typename RhsT> STATIC_ASSERT_Expression_Too_Complex_Please_Rewrite_As_Binary_Comparison& operator + ( RhsT const& );
    template<typename RhsT> STATIC_ASSERT_Expression_Too_Complex_Please_Rewrite_As_Binary_Comparison& operator - ( RhsT const& );
    template<typename RhsT> STATIC_ASSERT_Expression_Too_Complex_Please_Rewrite_As_Binary_Comparison& operator / ( RhsT const& );
    template<typename RhsT> STATIC_ASSERT_Expression_Too_Complex_Please_Rewrite_As_Binary_Comparison& operator * ( RhsT const& );
    template<typename RhsT> STATIC_ASSERT_Expression_Too_Complex_Please_Rewrite_As_Binary_Comparison& operator && ( RhsT const& );
    template<typename RhsT> STATIC_ASSERT_Expression_Too_Complex_Please_Rewrite_As_Binary_Comparison& operator || ( RhsT const& );

private:
    template<Internal::Operator Op, typename RhsT>
    ResultBuilder& captureExpression( RhsT const& rhs ) {
        return m_rb
            .setResultType( Internal::compare<Op>( m_lhs, rhs ) )
            .setLhs( Catch::toString( m_lhs ) )
            .setRhs( Catch::toString( rhs ) )
            .setOp( Internal::OperatorTraits<Op>::getName() );
    }

private:
    ResultBuilder& m_rb;
    T m_lhs;
};

}


namespace Catch {

    template<typename T>
    inline ExpressionLhs<T const&> ResultBuilder::operator <= ( T const& operand ) {
        return ExpressionLhs<T const&>( *this, operand );
    }

    inline ExpressionLhs<bool> ResultBuilder::operator <= ( bool value ) {
        return ExpressionLhs<bool>( *this, value );
    }

}


#define TWOBLUECUBES_CATCH_MESSAGE_H_INCLUDED

#include <string>

namespace Catch {

    struct MessageInfo {
        MessageInfo(    std::string const& _macroName,
                        SourceLineInfo const& _lineInfo,
                        ResultWas::OfType _type );

        std::string macroName;
        SourceLineInfo lineInfo;
        ResultWas::OfType type;
        std::string message;
        unsigned int sequence;

        bool operator == ( MessageInfo const& other ) const {
            return sequence == other.sequence;
        }
        bool operator < ( MessageInfo const& other ) const {
            return sequence < other.sequence;
        }
    private:
        static unsigned int globalCount;
    };

    struct MessageBuilder {
        MessageBuilder( std::string const& macroName,
                        SourceLineInfo const& lineInfo,
                        ResultWas::OfType type )
        : m_info( macroName, lineInfo, type )
        {}

        template<typename T>
        MessageBuilder& operator << ( T const& value ) {
            m_stream << value;
            return *this;
        }

        MessageInfo m_info;
        std::ostringstream m_stream;
    };

    class ScopedMessage {
    public:
        ScopedMessage( MessageBuilder const& builder );
        ScopedMessage( ScopedMessage const& other );
        ~ScopedMessage();

        MessageInfo m_info;
    };

}


#define TWOBLUECUBES_CATCH_INTERFACES_CAPTURE_H_INCLUDED

#include <string>

namespace Catch {

    class TestCase;
    class AssertionResult;
    struct AssertionInfo;
    struct SectionInfo;
    struct SectionEndInfo;
    struct MessageInfo;
    class ScopedMessageBuilder;
    struct Counts;

    struct IResultCapture {

        virtual ~IResultCapture();

        virtual void assertionEnded( AssertionResult const& result ) = 0;
        virtual bool sectionStarted(    SectionInfo const& sectionInfo,
                                        Counts& assertions ) = 0;
        virtual void sectionEnded( SectionEndInfo const& endInfo ) = 0;
        virtual void sectionEndedEarly( SectionEndInfo const& endInfo ) = 0;
        virtual void pushScopedMessage( MessageInfo const& message ) = 0;
        virtual void popScopedMessage( MessageInfo const& message ) = 0;

        virtual std::string getCurrentTestName() const = 0;
        virtual const AssertionResult* getLastResult() const = 0;

        virtual void handleFatalErrorCondition( std::string const& message ) = 0;
    };

    IResultCapture& getResultCapture();
}


#define TWOBLUECUBES_CATCH_DEBUGGER_H_INCLUDED


#define TWOBLUECUBES_CATCH_PLATFORM_H_INCLUDED

#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED)
#define CATCH_PLATFORM_MAC
#elif  defined(__IPHONE_OS_VERSION_MIN_REQUIRED)
#define CATCH_PLATFORM_IPHONE
#elif defined(WIN32) || defined(__WIN32__) || defined(_WIN32) || defined(_MSC_VER)
#define CATCH_PLATFORM_WINDOWS
#endif

#include <string>

namespace Catch{

    bool isDebuggerActive();
    void writeToDebugConsole( std::string const& text );
}

#ifdef CATCH_PLATFORM_MAC



    #ifdef DEBUG
        #if defined(__ppc64__) || defined(__ppc__)
            #define CATCH_BREAK_INTO_DEBUGGER() \
                if( Catch::isDebuggerActive() ) { \
                    __asm__("li r0, 20\nsc\nnop\nli r0, 37\nli r4, 2\nsc\nnop\n" \
                    : : : "memory","r0","r3","r4" ); \
                }
        #else
            #define CATCH_BREAK_INTO_DEBUGGER() if( Catch::isDebuggerActive() ) {__asm__("int $3\n" : : );}
        #endif
    #endif

#elif defined(_MSC_VER)
    #define CATCH_BREAK_INTO_DEBUGGER() if( Catch::isDebuggerActive() ) { __debugbreak(); }
#elif defined(__MINGW32__)
    extern "C" __declspec(dllimport) void __stdcall DebugBreak();
    #define CATCH_BREAK_INTO_DEBUGGER() if( Catch::isDebuggerActive() ) { DebugBreak(); }
#endif

#ifndef CATCH_BREAK_INTO_DEBUGGER
#define CATCH_BREAK_INTO_DEBUGGER() Catch::alwaysTrue();
#endif


#define TWOBLUECUBES_CATCH_INTERFACES_RUNNER_H_INCLUDED

namespace Catch {
    class TestCase;

    struct IRunner {
        virtual ~IRunner();
        virtual bool aborting() const = 0;
    };
}






#define INTERNAL_CATCH_REACT( resultBuilder ) \
    if( resultBuilder.shouldDebugBreak() ) CATCH_BREAK_INTO_DEBUGGER(); \
    resultBuilder.react();


#define INTERNAL_CATCH_TEST( expr, resultDisposition, macroName ) \
    do { \
        Catch::ResultBuilder __catchResult( macroName, CATCH_INTERNAL_LINEINFO, #expr, resultDisposition ); \
        try { \
            ( __catchResult <= expr ).endExpression(); \
        } \
        catch( ... ) { \
            __catchResult.useActiveException( Catch::ResultDisposition::Normal ); \
        } \
        INTERNAL_CATCH_REACT( __catchResult ) \
    } while( Catch::isTrue( false && static_cast<bool>(expr) ) )


#define INTERNAL_CATCH_IF( expr, resultDisposition, macroName ) \
    INTERNAL_CATCH_TEST( expr, resultDisposition, macroName ); \
    if( Catch::getResultCapture().getLastResult()->succeeded() )


#define INTERNAL_CATCH_ELSE( expr, resultDisposition, macroName ) \
    INTERNAL_CATCH_TEST( expr, resultDisposition, macroName ); \
    if( !Catch::getResultCapture().getLastResult()->succeeded() )


#define INTERNAL_CATCH_NO_THROW( expr, resultDisposition, macroName ) \
    do { \
        Catch::ResultBuilder __catchResult( macroName, CATCH_INTERNAL_LINEINFO, #expr, resultDisposition ); \
        try { \
            expr; \
            __catchResult.captureResult( Catch::ResultWas::Ok ); \
        } \
        catch( ... ) { \
            __catchResult.useActiveException( resultDisposition ); \
        } \
        INTERNAL_CATCH_REACT( __catchResult ) \
    } while( Catch::alwaysFalse() )


#define INTERNAL_CATCH_THROWS( expr, resultDisposition, matcher, macroName ) \
    do { \
        Catch::ResultBuilder __catchResult( macroName, CATCH_INTERNAL_LINEINFO, #expr, resultDisposition, #matcher ); \
        if( __catchResult.allowThrows() ) \
            try { \
                expr; \
                __catchResult.captureResult( Catch::ResultWas::DidntThrowException ); \
            } \
            catch( ... ) { \
                __catchResult.captureExpectedException( matcher ); \
            } \
        else \
            __catchResult.captureResult( Catch::ResultWas::Ok ); \
        INTERNAL_CATCH_REACT( __catchResult ) \
    } while( Catch::alwaysFalse() )


#define INTERNAL_CATCH_THROWS_AS( expr, exceptionType, resultDisposition, macroName ) \
    do { \
        Catch::ResultBuilder __catchResult( macroName, CATCH_INTERNAL_LINEINFO, #expr, resultDisposition ); \
        if( __catchResult.allowThrows() ) \
            try { \
                expr; \
                __catchResult.captureResult( Catch::ResultWas::DidntThrowException ); \
            } \
            catch( exceptionType ) { \
                __catchResult.captureResult( Catch::ResultWas::Ok ); \
            } \
            catch( ... ) { \
                __catchResult.useActiveException( resultDisposition ); \
            } \
        else \
            __catchResult.captureResult( Catch::ResultWas::Ok ); \
        INTERNAL_CATCH_REACT( __catchResult ) \
    } while( Catch::alwaysFalse() )


#ifdef CATCH_CONFIG_VARIADIC_MACROS
    #define INTERNAL_CATCH_MSG( messageType, resultDisposition, macroName, ... ) \
        do { \
            Catch::ResultBuilder __catchResult( macroName, CATCH_INTERNAL_LINEINFO, "", resultDisposition ); \
            __catchResult << __VA_ARGS__ + ::Catch::StreamEndStop(); \
            __catchResult.captureResult( messageType ); \
            INTERNAL_CATCH_REACT( __catchResult ) \
        } while( Catch::alwaysFalse() )
#else
    #define INTERNAL_CATCH_MSG( messageType, resultDisposition, macroName, log ) \
        do { \
            Catch::ResultBuilder __catchResult( macroName, CATCH_INTERNAL_LINEINFO, "", resultDisposition ); \
            __catchResult << log + ::Catch::StreamEndStop(); \
            __catchResult.captureResult( messageType ); \
            INTERNAL_CATCH_REACT( __catchResult ) \
        } while( Catch::alwaysFalse() )
#endif


#define INTERNAL_CATCH_INFO( log, macroName ) \
    Catch::ScopedMessage INTERNAL_CATCH_UNIQUE_NAME( scopedMessage ) = Catch::MessageBuilder( macroName, CATCH_INTERNAL_LINEINFO, Catch::ResultWas::Info ) << log;


#define INTERNAL_CHECK_THAT( arg, matcher, resultDisposition, macroName ) \
    do { \
        Catch::ResultBuilder __catchResult( macroName, CATCH_INTERNAL_LINEINFO, #arg ", " #matcher, resultDisposition ); \
        try { \
            std::string matcherAsString = (matcher).toString(); \
            __catchResult \
                .setLhs( Catch::toString( arg ) ) \
                .setRhs( matcherAsString == Catch::Detail::unprintableString ? #matcher : matcherAsString ) \
                .setOp( "matches" ) \
                .setResultType( (matcher).match( arg ) ); \
            __catchResult.captureExpression(); \
        } catch( ... ) { \
            __catchResult.useActiveException( resultDisposition | Catch::ResultDisposition::ContinueOnFailure ); \
        } \
        INTERNAL_CATCH_REACT( __catchResult ) \
    } while( Catch::alwaysFalse() )


#define TWOBLUECUBES_CATCH_SECTION_H_INCLUDED


#define TWOBLUECUBES_CATCH_SECTION_INFO_H_INCLUDED


#define TWOBLUECUBES_CATCH_TOTALS_HPP_INCLUDED

#include <cstddef>

namespace Catch {

    struct Counts {
        Counts() : passed( 0 ), failed( 0 ), failedButOk( 0 ) {}

        Counts operator - ( Counts const& other ) const {
            Counts diff;
            diff.passed = passed - other.passed;
            diff.failed = failed - other.failed;
            diff.failedButOk = failedButOk - other.failedButOk;
            return diff;
        }
        Counts& operator += ( Counts const& other ) {
            passed += other.passed;
            failed += other.failed;
            failedButOk += other.failedButOk;
            return *this;
        }

        std::size_t total() const {
            return passed + failed + failedButOk;
        }
        bool allPassed() const {
            return failed == 0 && failedButOk == 0;
        }
        bool allOk() const {
            return failed == 0;
        }

        std::size_t passed;
        std::size_t failed;
        std::size_t failedButOk;
    };

    struct Totals {

        Totals operator - ( Totals const& other ) const {
            Totals diff;
            diff.assertions = assertions - other.assertions;
            diff.testCases = testCases - other.testCases;
            return diff;
        }

        Totals delta( Totals const& prevTotals ) const {
            Totals diff = *this - prevTotals;
            if( diff.assertions.failed > 0 )
                ++diff.testCases.failed;
            else if( diff.assertions.failedButOk > 0 )
                ++diff.testCases.failedButOk;
            else
                ++diff.testCases.passed;
            return diff;
        }

        Totals& operator += ( Totals const& other ) {
            assertions += other.assertions;
            testCases += other.testCases;
            return *this;
        }

        Counts assertions;
        Counts testCases;
    };
}

namespace Catch {

    struct SectionInfo {
        SectionInfo
            (   SourceLineInfo const& _lineInfo,
                std::string const& _name,
                std::string const& _description = std::string() );

        std::string name;
        std::string description;
        SourceLineInfo lineInfo;
    };

    struct SectionEndInfo {
        SectionEndInfo( SectionInfo const& _sectionInfo, Counts const& _prevAssertions, double _durationInSeconds )
        : sectionInfo( _sectionInfo ), prevAssertions( _prevAssertions ), durationInSeconds( _durationInSeconds )
        {}

        SectionInfo sectionInfo;
        Counts prevAssertions;
        double durationInSeconds;
    };

}


#define TWOBLUECUBES_CATCH_TIMER_H_INCLUDED

#ifdef CATCH_PLATFORM_WINDOWS
typedef unsigned long long uint64_t;
#else
#include <stdint.h>
#endif

namespace Catch {

    class Timer {
    public:
        Timer() : m_ticks( 0 ) {}
        void start();
        unsigned int getElapsedMicroseconds() const;
        unsigned int getElapsedMilliseconds() const;
        double getElapsedSeconds() const;

    private:
        uint64_t m_ticks;
    };

}

#include <string>

namespace Catch {

    class Section : NonCopyable {
    public:
        Section( SectionInfo const& info );
        ~Section();


        operator bool() const;

    private:
        SectionInfo m_info;

        std::string m_name;
        Counts m_assertions;
        bool m_sectionIncluded;
        Timer m_timer;
    };

}

#ifdef CATCH_CONFIG_VARIADIC_MACROS
    #define INTERNAL_CATCH_SECTION( ... ) \
        if( Catch::Section const& INTERNAL_CATCH_UNIQUE_NAME( catch_internal_Section ) = Catch::SectionInfo( CATCH_INTERNAL_LINEINFO, __VA_ARGS__ ) )
#else
    #define INTERNAL_CATCH_SECTION( name, desc ) \
        if( Catch::Section const& INTERNAL_CATCH_UNIQUE_NAME( catch_internal_Section ) = Catch::SectionInfo( CATCH_INTERNAL_LINEINFO, name, desc ) )
#endif


#define TWOBLUECUBES_CATCH_GENERATORS_HPP_INCLUDED

#include <iterator>
#include <vector>
#include <string>
#include <stdlib.h>

namespace Catch {

template<typename T>
struct IGenerator {
    virtual ~IGenerator() {}
    virtual T getValue( std::size_t index ) const = 0;
    virtual std::size_t size () const = 0;
};

template<typename T>
class BetweenGenerator : public IGenerator<T> {
public:
    BetweenGenerator( T from, T to ) : m_from( from ), m_to( to ){}

    virtual T getValue( std::size_t index ) const {
        return m_from+static_cast<int>( index );
    }

    virtual std::size_t size() const {
        return static_cast<std::size_t>( 1+m_to-m_from );
    }

private:

    T m_from;
    T m_to;
};

template<typename T>
class ValuesGenerator : public IGenerator<T> {
public:
    ValuesGenerator(){}

    void add( T value ) {
        m_values.push_back( value );
    }

    virtual T getValue( std::size_t index ) const {
        return m_values[index];
    }

    virtual std::size_t size() const {
        return m_values.size();
    }

private:
    std::vector<T> m_values;
};

template<typename T>
class CompositeGenerator {
public:
    CompositeGenerator() : m_totalSize( 0 ) {}


    CompositeGenerator( CompositeGenerator& other )
    :   m_fileInfo( other.m_fileInfo ),
        m_totalSize( 0 )
    {
        move( other );
    }

    CompositeGenerator& setFileInfo( const char* fileInfo ) {
        m_fileInfo = fileInfo;
        return *this;
    }

    ~CompositeGenerator() {
        deleteAll( m_composed );
    }

    operator T () const {
        size_t overallIndex = getCurrentContext().getGeneratorIndex( m_fileInfo, m_totalSize );

        typename std::vector<const IGenerator<T>*>::const_iterator it = m_composed.begin();
        typename std::vector<const IGenerator<T>*>::const_iterator itEnd = m_composed.end();
        for( size_t index = 0; it != itEnd; ++it )
        {
            const IGenerator<T>* generator = *it;
            if( overallIndex >= index && overallIndex < index + generator->size() )
            {
                return generator->getValue( overallIndex-index );
            }
            index += generator->size();
        }
        CATCH_INTERNAL_ERROR( "Indexed past end of generated range" );
        return T();
    }

    void add( const IGenerator<T>* generator ) {
        m_totalSize += generator->size();
        m_composed.push_back( generator );
    }

    CompositeGenerator& then( CompositeGenerator& other ) {
        move( other );
        return *this;
    }

    CompositeGenerator& then( T value ) {
        ValuesGenerator<T>* valuesGen = new ValuesGenerator<T>();
        valuesGen->add( value );
        add( valuesGen );
        return *this;
    }

private:

    void move( CompositeGenerator& other ) {
        std::copy( other.m_composed.begin(), other.m_composed.end(), std::back_inserter( m_composed ) );
        m_totalSize += other.m_totalSize;
        other.m_composed.clear();
    }

    std::vector<const IGenerator<T>*> m_composed;
    std::string m_fileInfo;
    size_t m_totalSize;
};

namespace Generators
{
    template<typename T>
    CompositeGenerator<T> between( T from, T to ) {
        CompositeGenerator<T> generators;
        generators.add( new BetweenGenerator<T>( from, to ) );
        return generators;
    }

    template<typename T>
    CompositeGenerator<T> values( T val1, T val2 ) {
        CompositeGenerator<T> generators;
        ValuesGenerator<T>* valuesGen = new ValuesGenerator<T>();
        valuesGen->add( val1 );
        valuesGen->add( val2 );
        generators.add( valuesGen );
        return generators;
    }

    template<typename T>
    CompositeGenerator<T> values( T val1, T val2, T val3 ){
        CompositeGenerator<T> generators;
        ValuesGenerator<T>* valuesGen = new ValuesGenerator<T>();
        valuesGen->add( val1 );
        valuesGen->add( val2 );
        valuesGen->add( val3 );
        generators.add( valuesGen );
        return generators;
    }

    template<typename T>
    CompositeGenerator<T> values( T val1, T val2, T val3, T val4 ) {
        CompositeGenerator<T> generators;
        ValuesGenerator<T>* valuesGen = new ValuesGenerator<T>();
        valuesGen->add( val1 );
        valuesGen->add( val2 );
        valuesGen->add( val3 );
        valuesGen->add( val4 );
        generators.add( valuesGen );
        return generators;
    }

}

using namespace Generators;

}

#define INTERNAL_CATCH_LINESTR2( line ) #line
#define INTERNAL_CATCH_LINESTR( line ) INTERNAL_CATCH_LINESTR2( line )

#define INTERNAL_CATCH_GENERATE( expr ) expr.setFileInfo( __FILE__ "(" INTERNAL_CATCH_LINESTR( __LINE__ ) ")" )


#define TWOBLUECUBES_CATCH_INTERFACES_EXCEPTION_H_INCLUDED

#include <string>
#include <vector>


#define TWOBLUECUBES_CATCH_INTERFACES_REGISTRY_HUB_H_INCLUDED

#include <string>

namespace Catch {

    class TestCase;
    struct ITestCaseRegistry;
    struct IExceptionTranslatorRegistry;
    struct IExceptionTranslator;
    struct IReporterRegistry;
    struct IReporterFactory;

    struct IRegistryHub {
        virtual ~IRegistryHub();

        virtual IReporterRegistry const& getReporterRegistry() const = 0;
        virtual ITestCaseRegistry const& getTestCaseRegistry() const = 0;
        virtual IExceptionTranslatorRegistry& getExceptionTranslatorRegistry() = 0;
    };

    struct IMutableRegistryHub {
        virtual ~IMutableRegistryHub();
        virtual void registerReporter( std::string const& name, Ptr<IReporterFactory> const& factory ) = 0;
        virtual void registerListener( Ptr<IReporterFactory> const& factory ) = 0;
        virtual void registerTest( TestCase const& testInfo ) = 0;
        virtual void registerTranslator( const IExceptionTranslator* translator ) = 0;
    };

    IRegistryHub& getRegistryHub();
    IMutableRegistryHub& getMutableRegistryHub();
    void cleanUp();
    std::string translateActiveException();

}

namespace Catch {

    typedef std::string(*exceptionTranslateFunction)();

    struct IExceptionTranslator;
    typedef std::vector<const IExceptionTranslator*> ExceptionTranslators;

    struct IExceptionTranslator {
        virtual ~IExceptionTranslator();
        virtual std::string translate( ExceptionTranslators::const_iterator it, ExceptionTranslators::const_iterator itEnd ) const = 0;
    };

    struct IExceptionTranslatorRegistry {
        virtual ~IExceptionTranslatorRegistry();

        virtual std::string translateActiveException() const = 0;
    };

    class ExceptionTranslatorRegistrar {
        template<typename T>
        class ExceptionTranslator : public IExceptionTranslator {
        public:

            ExceptionTranslator( std::string(*translateFunction)( T& ) )
            : m_translateFunction( translateFunction )
            {}

            virtual std::string translate( ExceptionTranslators::const_iterator it, ExceptionTranslators::const_iterator itEnd ) const CATCH_OVERRIDE {
                try {
                    if( it == itEnd )
                        throw;
                    else
                        return (*it)->translate( it+1, itEnd );
                }
                catch( T& ex ) {
                    return m_translateFunction( ex );
                }
            }

        protected:
            std::string(*m_translateFunction)( T& );
        };

    public:
        template<typename T>
        ExceptionTranslatorRegistrar( std::string(*translateFunction)( T& ) ) {
            getMutableRegistryHub().registerTranslator
                ( new ExceptionTranslator<T>( translateFunction ) );
        }
    };
}


#define INTERNAL_CATCH_TRANSLATE_EXCEPTION( signature ) \
    static std::string INTERNAL_CATCH_UNIQUE_NAME( catch_internal_ExceptionTranslator )( signature ); \
    namespace{ Catch::ExceptionTranslatorRegistrar INTERNAL_CATCH_UNIQUE_NAME( catch_internal_ExceptionRegistrar )( &INTERNAL_CATCH_UNIQUE_NAME( catch_internal_ExceptionTranslator ) ); }\
    static std::string INTERNAL_CATCH_UNIQUE_NAME(  catch_internal_ExceptionTranslator )( signature )


#define TWOBLUECUBES_CATCH_APPROX_HPP_INCLUDED

#include <cmath>
#include <limits>

namespace Catch {
namespace Detail {

    class Approx {
    public:
        explicit Approx ( double value )
        :   m_epsilon( std::numeric_limits<float>::epsilon()*100 ),
            m_scale( 1.0 ),
            m_value( value )
        {}

        Approx( Approx const& other )
        :   m_epsilon( other.m_epsilon ),
            m_scale( other.m_scale ),
            m_value( other.m_value )
        {}

        static Approx custom() {
            return Approx( 0 );
        }

        Approx operator()( double value ) {
            Approx approx( value );
            approx.epsilon( m_epsilon );
            approx.scale( m_scale );
            return approx;
        }

        friend bool operator == ( double lhs, Approx const& rhs ) {

            return fabs( lhs - rhs.m_value ) < rhs.m_epsilon * (rhs.m_scale + (std::max)( fabs(lhs), fabs(rhs.m_value) ) );
        }

        friend bool operator == ( Approx const& lhs, double rhs ) {
            return operator==( rhs, lhs );
        }

        friend bool operator != ( double lhs, Approx const& rhs ) {
            return !operator==( lhs, rhs );
        }

        friend bool operator != ( Approx const& lhs, double rhs ) {
            return !operator==( rhs, lhs );
        }

        Approx& epsilon( double newEpsilon ) {
            m_epsilon = newEpsilon;
            return *this;
        }

        Approx& scale( double newScale ) {
            m_scale = newScale;
            return *this;
        }

        std::string toString() const {
            std::ostringstream oss;
            oss << "Approx( " << Catch::toString( m_value ) << " )";
            return oss.str();
        }

    private:
        double m_epsilon;
        double m_scale;
        double m_value;
    };
}

template<>
inline std::string toString<Detail::Approx>( Detail::Approx const& value ) {
    return value.toString();
}

}


#define TWOBLUECUBES_CATCH_INTERFACES_TAG_ALIAS_REGISTRY_H_INCLUDED


#define TWOBLUECUBES_CATCH_TAG_ALIAS_H_INCLUDED

#include <string>

namespace Catch {

    struct TagAlias {
        TagAlias( std::string _tag, SourceLineInfo _lineInfo ) : tag( _tag ), lineInfo( _lineInfo ) {}

        std::string tag;
        SourceLineInfo lineInfo;
    };

    struct RegistrarForTagAliases {
        RegistrarForTagAliases( char const* alias, char const* tag, SourceLineInfo const& lineInfo );
    };

}

#define CATCH_REGISTER_TAG_ALIAS( alias, spec ) namespace{ Catch::RegistrarForTagAliases INTERNAL_CATCH_UNIQUE_NAME( AutoRegisterTagAlias )( alias, spec, CATCH_INTERNAL_LINEINFO ); }

#define TWOBLUECUBES_CATCH_OPTION_HPP_INCLUDED

namespace Catch {


    template<typename T>
    class Option {
    public:
        Option() : nullableValue( CATCH_NULL ) {}
        Option( T const& _value )
        : nullableValue( new( storage ) T( _value ) )
        {}
        Option( Option const& _other )
        : nullableValue( _other ? new( storage ) T( *_other ) : CATCH_NULL )
        {}

        ~Option() {
            reset();
        }

        Option& operator= ( Option const& _other ) {
            if( &_other != this ) {
                reset();
                if( _other )
                    nullableValue = new( storage ) T( *_other );
            }
            return *this;
        }
        Option& operator = ( T const& _value ) {
            reset();
            nullableValue = new( storage ) T( _value );
            return *this;
        }

        void reset() {
            if( nullableValue )
                nullableValue->~T();
            nullableValue = CATCH_NULL;
        }

        T& operator*() { return *nullableValue; }
        T const& operator*() const { return *nullableValue; }
        T* operator->() { return nullableValue; }
        const T* operator->() const { return nullableValue; }

        T valueOr( T const& defaultValue ) const {
            return nullableValue ? *nullableValue : defaultValue;
        }

        bool some() const { return nullableValue != CATCH_NULL; }
        bool none() const { return nullableValue == CATCH_NULL; }

        bool operator !() const { return nullableValue == CATCH_NULL; }
        operator SafeBool::type() const {
            return SafeBool::makeSafe( some() );
        }

    private:
        T* nullableValue;
        char storage[sizeof(T)];
    };

}

namespace Catch {

    struct ITagAliasRegistry {
        virtual ~ITagAliasRegistry();
        virtual Option<TagAlias> find( std::string const& alias ) const = 0;
        virtual std::string expandAliases( std::string const& unexpandedTestSpec ) const = 0;

        static ITagAliasRegistry const& get();
    };

}




#define TWOBLUECUBES_CATCH_TEST_CASE_INFO_H_INCLUDED

#include <string>
#include <set>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

namespace Catch {

    struct ITestCase;

    struct TestCaseInfo {
        enum SpecialProperties{
            None = 0,
            IsHidden = 1 << 1,
            ShouldFail = 1 << 2,
            MayFail = 1 << 3,
            Throws = 1 << 4
        };

        TestCaseInfo(   std::string const& _name,
                        std::string const& _className,
                        std::string const& _description,
                        std::set<std::string> const& _tags,
                        SourceLineInfo const& _lineInfo );

        TestCaseInfo( TestCaseInfo const& other );

        friend void setTags( TestCaseInfo& testCaseInfo, std::set<std::string> const& tags );

        bool isHidden() const;
        bool throws() const;
        bool okToFail() const;
        bool expectedToFail() const;

        std::string name;
        std::string className;
        std::string description;
        std::set<std::string> tags;
        std::set<std::string> lcaseTags;
        std::string tagsAsString;
        SourceLineInfo lineInfo;
        SpecialProperties properties;
    };

    class TestCase : public TestCaseInfo {
    public:

        TestCase( ITestCase* testCase, TestCaseInfo const& info );
        TestCase( TestCase const& other );

        TestCase withName( std::string const& _newName ) const;

        void invoke() const;

        TestCaseInfo const& getTestCaseInfo() const;

        void swap( TestCase& other );
        bool operator == ( TestCase const& other ) const;
        bool operator < ( TestCase const& other ) const;
        TestCase& operator = ( TestCase const& other );

    private:
        Ptr<ITestCase> test;
    };

    TestCase makeTestCase(  ITestCase* testCase,
                            std::string const& className,
                            std::string const& name,
                            std::string const& description,
                            SourceLineInfo const& lineInfo );
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif


#ifdef __OBJC__

#define TWOBLUECUBES_CATCH_OBJC_HPP_INCLUDED

#import <objc/runtime.h>

#include <string>








@protocol OcFixture

@optional

-(void) setUp;
-(void) tearDown;

@end

namespace Catch {

    class OcMethod : public SharedImpl<ITestCase> {

    public:
        OcMethod( Class cls, SEL sel ) : m_cls( cls ), m_sel( sel ) {}

        virtual void invoke() const {
            id obj = [[m_cls alloc] init];

            performOptionalSelector( obj, @selector(setUp)  );
            performOptionalSelector( obj, m_sel );
            performOptionalSelector( obj, @selector(tearDown)  );

            arcSafeRelease( obj );
        }
    private:
        virtual ~OcMethod() {}

        Class m_cls;
        SEL m_sel;
    };

    namespace Detail{

        inline std::string getAnnotation(   Class cls,
                                            std::string const& annotationName,
                                            std::string const& testCaseName ) {
            NSString* selStr = [[NSString alloc] initWithFormat:@"Catch_%s_%s", annotationName.c_str(), testCaseName.c_str()];
            SEL sel = NSSelectorFromString( selStr );
            arcSafeRelease( selStr );
            id value = performOptionalSelector( cls, sel );
            if( value )
                return [(NSString*)value UTF8String];
            return "";
        }
    }

    inline size_t registerTestMethods() {
        size_t noTestMethods = 0;
        int noClasses = objc_getClassList( CATCH_NULL, 0 );

        Class* classes = (CATCH_UNSAFE_UNRETAINED Class *)malloc( sizeof(Class) * noClasses);
        objc_getClassList( classes, noClasses );

        for( int c = 0; c < noClasses; c++ ) {
            Class cls = classes[c];
            {
                u_int count;
                Method* methods = class_copyMethodList( cls, &count );
                for( u_int m = 0; m < count ; m++ ) {
                    SEL selector = method_getName(methods[m]);
                    std::string methodName = sel_getName(selector);
                    if( startsWith( methodName, "Catch_TestCase_" ) ) {
                        std::string testCaseName = methodName.substr( 15 );
                        std::string name = Detail::getAnnotation( cls, "Name", testCaseName );
                        std::string desc = Detail::getAnnotation( cls, "Description", testCaseName );
                        const char* className = class_getName( cls );

                        getMutableRegistryHub().registerTest( makeTestCase( new OcMethod( cls, selector ), className, name.c_str(), desc.c_str(), SourceLineInfo() ) );
                        noTestMethods++;
                    }
                }
                free(methods);
            }
        }
        return noTestMethods;
    }

    namespace Matchers {
        namespace Impl {
        namespace NSStringMatchers {

            template<typename MatcherT>
            struct StringHolder : MatcherImpl<MatcherT, NSString*>{
                StringHolder( NSString* substr ) : m_substr( [substr copy] ){}
                StringHolder( StringHolder const& other ) : m_substr( [other.m_substr copy] ){}
                StringHolder() {
                    arcSafeRelease( m_substr );
                }

                NSString* m_substr;
            };

            struct Equals : StringHolder<Equals> {
                Equals( NSString* substr ) : StringHolder( substr ){}

                virtual bool match( ExpressionType const& str ) const {
                    return  (str != nil || m_substr == nil ) &&
                            [str isEqualToString:m_substr];
                }

                virtual std::string toString() const {
                    return "equals string: " + Catch::toString( m_substr );
                }
            };

            struct Contains : StringHolder<Contains> {
                Contains( NSString* substr ) : StringHolder( substr ){}

                virtual bool match( ExpressionType const& str ) const {
                    return  (str != nil || m_substr == nil ) &&
                            [str rangeOfString:m_substr].location != NSNotFound;
                }

                virtual std::string toString() const {
                    return "contains string: " + Catch::toString( m_substr );
                }
            };

            struct StartsWith : StringHolder<StartsWith> {
                StartsWith( NSString* substr ) : StringHolder( substr ){}

                virtual bool match( ExpressionType const& str ) const {
                    return  (str != nil || m_substr == nil ) &&
                            [str rangeOfString:m_substr].location == 0;
                }

                virtual std::string toString() const {
                    return "starts with: " + Catch::toString( m_substr );
                }
            };
            struct EndsWith : StringHolder<EndsWith> {
                EndsWith( NSString* substr ) : StringHolder( substr ){}

                virtual bool match( ExpressionType const& str ) const {
                    return  (str != nil || m_substr == nil ) &&
                            [str rangeOfString:m_substr].location == [str length] - [m_substr length];
                }

                virtual std::string toString() const {
                    return "ends with: " + Catch::toString( m_substr );
                }
            };

        }
        }

        inline Impl::NSStringMatchers::Equals
            Equals( NSString* substr ){ return Impl::NSStringMatchers::Equals( substr ); }

        inline Impl::NSStringMatchers::Contains
            Contains( NSString* substr ){ return Impl::NSStringMatchers::Contains( substr ); }

        inline Impl::NSStringMatchers::StartsWith
            StartsWith( NSString* substr ){ return Impl::NSStringMatchers::StartsWith( substr ); }

        inline Impl::NSStringMatchers::EndsWith
            EndsWith( NSString* substr ){ return Impl::NSStringMatchers::EndsWith( substr ); }

    }

    using namespace Matchers;

}


#define OC_TEST_CASE( name, desc )\
+(NSString*) INTERNAL_CATCH_UNIQUE_NAME( Catch_Name_test ) \
{\
return @ name; \
}\
+(NSString*) INTERNAL_CATCH_UNIQUE_NAME( Catch_Description_test ) \
{ \
return @ desc; \
} \
-(void) INTERNAL_CATCH_UNIQUE_NAME( Catch_TestCase_test )

#endif

#ifdef CATCH_IMPL

#define TWOBLUECUBES_CATCH_IMPL_HPP_INCLUDED




#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif


#define TWOBLUECUBES_CATCH_RUNNER_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_COMMANDLINE_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_CONFIG_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_TEST_SPEC_PARSER_HPP_INCLUDED

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif


#define TWOBLUECUBES_CATCH_TEST_SPEC_HPP_INCLUDED

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif


#define TWOBLUECUBES_CATCH_WILDCARD_PATTERN_HPP_INCLUDED

namespace Catch
{
    class WildcardPattern {
        enum WildcardPosition {
            NoWildcard = 0,
            WildcardAtStart = 1,
            WildcardAtEnd = 2,
            WildcardAtBothEnds = WildcardAtStart | WildcardAtEnd
        };

    public:

        WildcardPattern( std::string const& pattern, CaseSensitive::Choice caseSensitivity )
        :   m_caseSensitivity( caseSensitivity ),
            m_wildcard( NoWildcard ),
            m_pattern( adjustCase( pattern ) )
        {
            if( startsWith( m_pattern, "*" ) ) {
                m_pattern = m_pattern.substr( 1 );
                m_wildcard = WildcardAtStart;
            }
            if( endsWith( m_pattern, "*" ) ) {
                m_pattern = m_pattern.substr( 0, m_pattern.size()-1 );
                m_wildcard = static_cast<WildcardPosition>( m_wildcard | WildcardAtEnd );
            }
        }
        virtual ~WildcardPattern();
        virtual bool matches( std::string const& str ) const {
            switch( m_wildcard ) {
                case NoWildcard:
                    return m_pattern == adjustCase( str );
                case WildcardAtStart:
                    return endsWith( adjustCase( str ), m_pattern );
                case WildcardAtEnd:
                    return startsWith( adjustCase( str ), m_pattern );
                case WildcardAtBothEnds:
                    return contains( adjustCase( str ), m_pattern );
            }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif
            throw std::logic_error( "Unknown enum" );
#ifdef __clang__
#pragma clang diagnostic pop
#endif
        }
    private:
        std::string adjustCase( std::string const& str ) const {
            return m_caseSensitivity == CaseSensitive::No ? toLower( str ) : str;
        }
        CaseSensitive::Choice m_caseSensitivity;
        WildcardPosition m_wildcard;
        std::string m_pattern;
    };
}

#include <string>
#include <vector>

namespace Catch {

    class TestSpec {
        struct Pattern : SharedImpl<> {
            virtual ~Pattern();
            virtual bool matches( TestCaseInfo const& testCase ) const = 0;
        };
        class NamePattern : public Pattern {
        public:
            NamePattern( std::string const& name )
            : m_wildcardPattern( toLower( name ), CaseSensitive::No )
            {}
            virtual ~NamePattern();
            virtual bool matches( TestCaseInfo const& testCase ) const {
                return m_wildcardPattern.matches( toLower( testCase.name ) );
            }
        private:
            WildcardPattern m_wildcardPattern;
        };

        class TagPattern : public Pattern {
        public:
            TagPattern( std::string const& tag ) : m_tag( toLower( tag ) ) {}
            virtual ~TagPattern();
            virtual bool matches( TestCaseInfo const& testCase ) const {
                return testCase.lcaseTags.find( m_tag ) != testCase.lcaseTags.end();
            }
        private:
            std::string m_tag;
        };

        class ExcludedPattern : public Pattern {
        public:
            ExcludedPattern( Ptr<Pattern> const& underlyingPattern ) : m_underlyingPattern( underlyingPattern ) {}
            virtual ~ExcludedPattern();
            virtual bool matches( TestCaseInfo const& testCase ) const { return !m_underlyingPattern->matches( testCase ); }
        private:
            Ptr<Pattern> m_underlyingPattern;
        };

        struct Filter {
            std::vector<Ptr<Pattern> > m_patterns;

            bool matches( TestCaseInfo const& testCase ) const {

                for( std::vector<Ptr<Pattern> >::const_iterator it = m_patterns.begin(), itEnd = m_patterns.end(); it != itEnd; ++it )
                    if( !(*it)->matches( testCase ) )
                        return false;
                    return true;
            }
        };

    public:
        bool hasFilters() const {
            return !m_filters.empty();
        }
        bool matches( TestCaseInfo const& testCase ) const {

            for( std::vector<Filter>::const_iterator it = m_filters.begin(), itEnd = m_filters.end(); it != itEnd; ++it )
                if( it->matches( testCase ) )
                    return true;
            return false;
        }

    private:
        std::vector<Filter> m_filters;

        friend class TestSpecParser;
    };
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Catch {

    class TestSpecParser {
        enum Mode{ None, Name, QuotedName, Tag };
        Mode m_mode;
        bool m_exclusion;
        std::size_t m_start, m_pos;
        std::string m_arg;
        TestSpec::Filter m_currentFilter;
        TestSpec m_testSpec;
        ITagAliasRegistry const* m_tagAliases;

    public:
        TestSpecParser( ITagAliasRegistry const& tagAliases ) : m_tagAliases( &tagAliases ) {}

        TestSpecParser& parse( std::string const& arg ) {
            m_mode = None;
            m_exclusion = false;
            m_start = std::string::npos;
            m_arg = m_tagAliases->expandAliases( arg );
            for( m_pos = 0; m_pos < m_arg.size(); ++m_pos )
                visitChar( m_arg[m_pos] );
            if( m_mode == Name )
                addPattern<TestSpec::NamePattern>();
            return *this;
        }
        TestSpec testSpec() {
            addFilter();
            return m_testSpec;
        }
    private:
        void visitChar( char c ) {
            if( m_mode == None ) {
                switch( c ) {
                case ' ': return;
                case '~': m_exclusion = true; return;
                case '[': return startNewMode( Tag, ++m_pos );
                case '"': return startNewMode( QuotedName, ++m_pos );
                default: startNewMode( Name, m_pos ); break;
                }
            }
            if( m_mode == Name ) {
                if( c == ',' ) {
                    addPattern<TestSpec::NamePattern>();
                    addFilter();
                }
                else if( c == '[' ) {
                    if( subString() == "exclude:" )
                        m_exclusion = true;
                    else
                        addPattern<TestSpec::NamePattern>();
                    startNewMode( Tag, ++m_pos );
                }
            }
            else if( m_mode == QuotedName && c == '"' )
                addPattern<TestSpec::NamePattern>();
            else if( m_mode == Tag && c == ']' )
                addPattern<TestSpec::TagPattern>();
        }
        void startNewMode( Mode mode, std::size_t start ) {
            m_mode = mode;
            m_start = start;
        }
        std::string subString() const { return m_arg.substr( m_start, m_pos - m_start ); }
        template<typename T>
        void addPattern() {
            std::string token = subString();
            if( startsWith( token, "exclude:" ) ) {
                m_exclusion = true;
                token = token.substr( 8 );
            }
            if( !token.empty() ) {
                Ptr<TestSpec::Pattern> pattern = new T( token );
                if( m_exclusion )
                    pattern = new TestSpec::ExcludedPattern( pattern );
                m_currentFilter.m_patterns.push_back( pattern );
            }
            m_exclusion = false;
            m_mode = None;
        }
        void addFilter() {
            if( !m_currentFilter.m_patterns.empty() ) {
                m_testSpec.m_filters.push_back( m_currentFilter );
                m_currentFilter = TestSpec::Filter();
            }
        }
    };
    inline TestSpec parseTestSpec( std::string const& arg ) {
        return TestSpecParser( ITagAliasRegistry::get() ).parse( arg ).testSpec();
    }

}

#ifdef __clang__
#pragma clang diagnostic pop
#endif


#define TWOBLUECUBES_CATCH_INTERFACES_CONFIG_H_INCLUDED

#include <iostream>
#include <string>
#include <vector>

namespace Catch {

    struct Verbosity { enum Level {
        NoOutput = 0,
        Quiet,
        Normal
    }; };

    struct WarnAbout { enum What {
        Nothing = 0x00,
        NoAssertions = 0x01
    }; };

    struct ShowDurations { enum OrNot {
        DefaultForReporter,
        Always,
        Never
    }; };
    struct RunTests { enum InWhatOrder {
        InDeclarationOrder,
        InLexicographicalOrder,
        InRandomOrder
    }; };

    class TestSpec;

    struct IConfig : IShared {

        virtual ~IConfig();

        virtual bool allowThrows() const = 0;
        virtual std::ostream& stream() const = 0;
        virtual std::string name() const = 0;
        virtual bool includeSuccessfulResults() const = 0;
        virtual bool shouldDebugBreak() const = 0;
        virtual bool warnAboutMissingAssertions() const = 0;
        virtual int abortAfter() const = 0;
        virtual bool showInvisibles() const = 0;
        virtual ShowDurations::OrNot showDurations() const = 0;
        virtual TestSpec const& testSpec() const = 0;
        virtual RunTests::InWhatOrder runOrder() const = 0;
        virtual unsigned int rngSeed() const = 0;
        virtual bool forceColour() const = 0;
    };
}


#define TWOBLUECUBES_CATCH_STREAM_H_INCLUDED


#define TWOBLUECUBES_CATCH_STREAMBUF_H_INCLUDED

#include <streambuf>

namespace Catch {

    class StreamBufBase : public std::streambuf {
    public:
        virtual ~StreamBufBase() CATCH_NOEXCEPT;
    };
}

#include <streambuf>
#include <ostream>
#include <fstream>

namespace Catch {

    std::ostream& cout();
    std::ostream& cerr();

    struct IStream {
        virtual ~IStream() CATCH_NOEXCEPT;
        virtual std::ostream& stream() const = 0;
    };

    class FileStream : public IStream {
        mutable std::ofstream m_ofs;
    public:
        FileStream( std::string const& filename );
        virtual ~FileStream() CATCH_NOEXCEPT;
    public:
        virtual std::ostream& stream() const CATCH_OVERRIDE;
    };

    class CoutStream : public IStream {
        mutable std::ostream m_os;
    public:
        CoutStream();
        virtual ~CoutStream() CATCH_NOEXCEPT;

    public:
        virtual std::ostream& stream() const CATCH_OVERRIDE;
    };

    class DebugOutStream : public IStream {
        std::auto_ptr<StreamBufBase> m_streamBuf;
        mutable std::ostream m_os;
    public:
        DebugOutStream();
        virtual ~DebugOutStream() CATCH_NOEXCEPT;

    public:
        virtual std::ostream& stream() const CATCH_OVERRIDE;
    };
}

#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <ctime>

#ifndef CATCH_CONFIG_CONSOLE_WIDTH
#define CATCH_CONFIG_CONSOLE_WIDTH 80
#endif

namespace Catch {

    struct ConfigData {

        ConfigData()
        :   listTests( false ),
            listTags( false ),
            listReporters( false ),
            listTestNamesOnly( false ),
            showSuccessfulTests( false ),
            shouldDebugBreak( false ),
            noThrow( false ),
            showHelp( false ),
            showInvisibles( false ),
            forceColour( false ),
            filenamesAsTags( false ),
            abortAfter( -1 ),
            rngSeed( 0 ),
            verbosity( Verbosity::Normal ),
            warnings( WarnAbout::Nothing ),
            showDurations( ShowDurations::DefaultForReporter ),
            runOrder( RunTests::InDeclarationOrder )
        {}

        bool listTests;
        bool listTags;
        bool listReporters;
        bool listTestNamesOnly;

        bool showSuccessfulTests;
        bool shouldDebugBreak;
        bool noThrow;
        bool showHelp;
        bool showInvisibles;
        bool forceColour;
        bool filenamesAsTags;

        int abortAfter;
        unsigned int rngSeed;

        Verbosity::Level verbosity;
        WarnAbout::What warnings;
        ShowDurations::OrNot showDurations;
        RunTests::InWhatOrder runOrder;

        std::string outputFilename;
        std::string name;
        std::string processName;

        std::vector<std::string> reporterNames;
        std::vector<std::string> testsOrTags;
    };

    class Config : public SharedImpl<IConfig> {
    private:
        Config( Config const& other );
        Config& operator = ( Config const& other );
        virtual void dummy();
    public:

        Config()
        {}

        Config( ConfigData const& data )
        :   m_data( data ),
            m_stream( openStream() )
        {
            if( !data.testsOrTags.empty() ) {
                TestSpecParser parser( ITagAliasRegistry::get() );
                for( std::size_t i = 0; i < data.testsOrTags.size(); ++i )
                    parser.parse( data.testsOrTags[i] );
                m_testSpec = parser.testSpec();
            }
        }

        virtual ~Config() {
        }

        std::string const& getFilename() const {
            return m_data.outputFilename ;
        }

        bool listTests() const { return m_data.listTests; }
        bool listTestNamesOnly() const { return m_data.listTestNamesOnly; }
        bool listTags() const { return m_data.listTags; }
        bool listReporters() const { return m_data.listReporters; }

        std::string getProcessName() const { return m_data.processName; }

        bool shouldDebugBreak() const { return m_data.shouldDebugBreak; }

        std::vector<std::string> getReporterNames() const { return m_data.reporterNames; }

        int abortAfter() const { return m_data.abortAfter; }

        TestSpec const& testSpec() const { return m_testSpec; }

        bool showHelp() const { return m_data.showHelp; }
        bool showInvisibles() const { return m_data.showInvisibles; }


        virtual bool allowThrows() const        { return !m_data.noThrow; }
        virtual std::ostream& stream() const    { return m_stream->stream(); }
        virtual std::string name() const        { return m_data.name.empty() ? m_data.processName : m_data.name; }
        virtual bool includeSuccessfulResults() const   { return m_data.showSuccessfulTests; }
        virtual bool warnAboutMissingAssertions() const { return m_data.warnings & WarnAbout::NoAssertions; }
        virtual ShowDurations::OrNot showDurations() const { return m_data.showDurations; }
        virtual RunTests::InWhatOrder runOrder() const  { return m_data.runOrder; }
        virtual unsigned int rngSeed() const    { return m_data.rngSeed; }
        virtual bool forceColour() const { return m_data.forceColour; }

    private:

        IStream const* openStream() {
            if( m_data.outputFilename.empty() )
                return new CoutStream();
            else if( m_data.outputFilename[0] == '%' ) {
                if( m_data.outputFilename == "%debug" )
                    return new DebugOutStream();
                else
                    throw std::domain_error( "Unrecognised stream: " + m_data.outputFilename );
            }
            else
                return new FileStream( m_data.outputFilename );
        }
        ConfigData m_data;

        std::auto_ptr<IStream const> m_stream;
        TestSpec m_testSpec;
    };

}


#define TWOBLUECUBES_CATCH_CLARA_H_INCLUDED


#ifdef CLARA_CONFIG_CONSOLE_WIDTH
#define CATCH_TEMP_CLARA_CONFIG_CONSOLE_WIDTH CLARA_CONFIG_CONSOLE_WIDTH
#undef CLARA_CONFIG_CONSOLE_WIDTH
#endif
#define CLARA_CONFIG_CONSOLE_WIDTH CATCH_CONFIG_CONSOLE_WIDTH


#define STITCH_CLARA_OPEN_NAMESPACE namespace Catch {





#if !defined(TWOBLUECUBES_CLARA_H_INCLUDED) || defined(STITCH_CLARA_OPEN_NAMESPACE)

#ifndef STITCH_CLARA_OPEN_NAMESPACE
#define TWOBLUECUBES_CLARA_H_INCLUDED
#define STITCH_CLARA_OPEN_NAMESPACE
#define STITCH_CLARA_CLOSE_NAMESPACE
#else
#define STITCH_CLARA_CLOSE_NAMESPACE }
#endif

#define STITCH_TBC_TEXT_FORMAT_OPEN_NAMESPACE STITCH_CLARA_OPEN_NAMESPACE




#if !defined(TBC_TEXT_FORMAT_H_INCLUDED) || defined(STITCH_TBC_TEXT_FORMAT_OUTER_NAMESPACE)
#ifndef STITCH_TBC_TEXT_FORMAT_OUTER_NAMESPACE
#define TBC_TEXT_FORMAT_H_INCLUDED
#endif

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>


#ifdef STITCH_TBC_TEXT_FORMAT_OUTER_NAMESPACE
namespace STITCH_TBC_TEXT_FORMAT_OUTER_NAMESPACE {
#endif

namespace Tbc {

#ifdef TBC_TEXT_FORMAT_CONSOLE_WIDTH
    const unsigned int consoleWidth = TBC_TEXT_FORMAT_CONSOLE_WIDTH;
#else
    const unsigned int consoleWidth = 80;
#endif

    struct TextAttributes {
        TextAttributes()
        :   initialIndent( std::string::npos ),
            indent( 0 ),
            width( consoleWidth-1 ),
            tabChar( '\t' )
        {}

        TextAttributes& setInitialIndent( std::size_t _value )  { initialIndent = _value; return *this; }
        TextAttributes& setIndent( std::size_t _value )         { indent = _value; return *this; }
        TextAttributes& setWidth( std::size_t _value )          { width = _value; return *this; }
        TextAttributes& setTabChar( char _value )               { tabChar = _value; return *this; }

        std::size_t initialIndent;
        std::size_t indent;
        std::size_t width;
        char tabChar;
    };

    class Text {
    public:
        Text( std::string const& _str, TextAttributes const& _attr = TextAttributes() )
        : attr( _attr )
        {
            std::string wrappableChars = " [({.,/|\\-";
            std::size_t indent = _attr.initialIndent != std::string::npos
                ? _attr.initialIndent
                : _attr.indent;
            std::string remainder = _str;

            while( !remainder.empty() ) {
                if( lines.size() >= 1000 ) {
                    lines.push_back( "... message truncated due to excessive size" );
                    return;
                }
                std::size_t tabPos = std::string::npos;
                std::size_t width = (std::min)( remainder.size(), _attr.width - indent );
                std::size_t pos = remainder.find_first_of( '\n' );
                if( pos <= width ) {
                    width = pos;
                }
                pos = remainder.find_last_of( _attr.tabChar, width );
                if( pos != std::string::npos ) {
                    tabPos = pos;
                    if( remainder[width] == '\n' )
                        width--;
                    remainder = remainder.substr( 0, tabPos ) + remainder.substr( tabPos+1 );
                }

                if( width == remainder.size() ) {
                    spliceLine( indent, remainder, width );
                }
                else if( remainder[width] == '\n' ) {
                    spliceLine( indent, remainder, width );
                    if( width <= 1 || remainder.size() != 1 )
                        remainder = remainder.substr( 1 );
                    indent = _attr.indent;
                }
                else {
                    pos = remainder.find_last_of( wrappableChars, width );
                    if( pos != std::string::npos && pos > 0 ) {
                        spliceLine( indent, remainder, pos );
                        if( remainder[0] == ' ' )
                            remainder = remainder.substr( 1 );
                    }
                    else {
                        spliceLine( indent, remainder, width-1 );
                        lines.back() += "-";
                    }
                    if( lines.size() == 1 )
                        indent = _attr.indent;
                    if( tabPos != std::string::npos )
                        indent += tabPos;
                }
            }
        }

        void spliceLine( std::size_t _indent, std::string& _remainder, std::size_t _pos ) {
            lines.push_back( std::string( _indent, ' ' ) + _remainder.substr( 0, _pos ) );
            _remainder = _remainder.substr( _pos );
        }

        typedef std::vector<std::string>::const_iterator const_iterator;

        const_iterator begin() const { return lines.begin(); }
        const_iterator end() const { return lines.end(); }
        std::string const& last() const { return lines.back(); }
        std::size_t size() const { return lines.size(); }
        std::string const& operator[]( std::size_t _index ) const { return lines[_index]; }
        std::string toString() const {
            std::ostringstream oss;
            oss << *this;
            return oss.str();
        }

        inline friend std::ostream& operator << ( std::ostream& _stream, Text const& _text ) {
            for( Text::const_iterator it = _text.begin(), itEnd = _text.end();
                it != itEnd; ++it ) {
                if( it != _text.begin() )
                    _stream << "\n";
                _stream << *it;
            }
            return _stream;
        }

    private:
        std::string str;
        TextAttributes attr;
        std::vector<std::string> lines;
    };

}

#ifdef STITCH_TBC_TEXT_FORMAT_OUTER_NAMESPACE
}
#endif

#endif




#undef STITCH_TBC_TEXT_FORMAT_OPEN_NAMESPACE



#ifndef TWOBLUECUBES_CLARA_COMPILERS_H_INCLUDED
#define TWOBLUECUBES_CLARA_COMPILERS_H_INCLUDED





















#ifdef __clang__

#if __has_feature(cxx_nullptr)
#define CLARA_INTERNAL_CONFIG_CPP11_NULLPTR
#endif

#if __has_feature(cxx_noexcept)
#define CLARA_INTERNAL_CONFIG_CPP11_NOEXCEPT
#endif

#endif



#ifdef __GNUC__

#if __GNUC__ == 4 && __GNUC_MINOR__ >= 6 && defined(__GXX_EXPERIMENTAL_CXX0X__)
#define CLARA_INTERNAL_CONFIG_CPP11_NULLPTR
#endif




#endif



#ifdef _MSC_VER

#if (_MSC_VER >= 1600)
#define CLARA_INTERNAL_CONFIG_CPP11_NULLPTR
#define CLARA_INTERNAL_CONFIG_CPP11_UNIQUE_PTR
#endif

#if (_MSC_VER >= 1900 )
#define CLARA_INTERNAL_CONFIG_CPP11_NOEXCEPT
#define CLARA_INTERNAL_CONFIG_CPP11_GENERATED_METHODS
#endif

#endif





#if defined(__cplusplus) && __cplusplus >= 201103L

#define CLARA_CPP11_OR_GREATER

#if !defined(CLARA_INTERNAL_CONFIG_CPP11_NULLPTR)
#define CLARA_INTERNAL_CONFIG_CPP11_NULLPTR
#endif

#ifndef CLARA_INTERNAL_CONFIG_CPP11_NOEXCEPT
#define CLARA_INTERNAL_CONFIG_CPP11_NOEXCEPT
#endif

#ifndef CLARA_INTERNAL_CONFIG_CPP11_GENERATED_METHODS
#define CLARA_INTERNAL_CONFIG_CPP11_GENERATED_METHODS
#endif

#if !defined(CLARA_INTERNAL_CONFIG_CPP11_OVERRIDE)
#define CLARA_INTERNAL_CONFIG_CPP11_OVERRIDE
#endif
#if !defined(CLARA_INTERNAL_CONFIG_CPP11_UNIQUE_PTR)
#define CLARA_INTERNAL_CONFIG_CPP11_UNIQUE_PTR
#endif

#endif


#if defined(CLARA_INTERNAL_CONFIG_CPP11_NULLPTR) && !defined(CLARA_CONFIG_CPP11_NO_NULLPTR) && !defined(CLARA_CONFIG_CPP11_NULLPTR) && !defined(CLARA_CONFIG_NO_CPP11)
#define CLARA_CONFIG_CPP11_NULLPTR
#endif
#if defined(CLARA_INTERNAL_CONFIG_CPP11_NOEXCEPT) && !defined(CLARA_CONFIG_CPP11_NO_NOEXCEPT) && !defined(CLARA_CONFIG_CPP11_NOEXCEPT) && !defined(CLARA_CONFIG_NO_CPP11)
#define CLARA_CONFIG_CPP11_NOEXCEPT
#endif
#if defined(CLARA_INTERNAL_CONFIG_CPP11_GENERATED_METHODS) && !defined(CLARA_CONFIG_CPP11_NO_GENERATED_METHODS) && !defined(CLARA_CONFIG_CPP11_GENERATED_METHODS) && !defined(CLARA_CONFIG_NO_CPP11)
#define CLARA_CONFIG_CPP11_GENERATED_METHODS
#endif
#if defined(CLARA_INTERNAL_CONFIG_CPP11_OVERRIDE) && !defined(CLARA_CONFIG_NO_OVERRIDE) && !defined(CLARA_CONFIG_CPP11_OVERRIDE) && !defined(CLARA_CONFIG_NO_CPP11)
#define CLARA_CONFIG_CPP11_OVERRIDE
#endif
#if defined(CLARA_INTERNAL_CONFIG_CPP11_UNIQUE_PTR) && !defined(CLARA_CONFIG_NO_UNIQUE_PTR) && !defined(CLARA_CONFIG_CPP11_UNIQUE_PTR) && !defined(CLARA_CONFIG_NO_CPP11)
#define CLARA_CONFIG_CPP11_UNIQUE_PTR
#endif


#if defined(CLARA_CONFIG_CPP11_NOEXCEPT) && !defined(CLARA_NOEXCEPT)
#define CLARA_NOEXCEPT noexcept
#  define CLARA_NOEXCEPT_IS(x) noexcept(x)
#else
#define CLARA_NOEXCEPT throw()
#  define CLARA_NOEXCEPT_IS(x)
#endif


#ifdef CLARA_CONFIG_CPP11_NULLPTR
#define CLARA_NULL nullptr
#else
#define CLARA_NULL NULL
#endif


#ifdef CLARA_CONFIG_CPP11_OVERRIDE
#define CLARA_OVERRIDE override
#else
#define CLARA_OVERRIDE
#endif


#ifdef CLARA_CONFIG_CPP11_UNIQUE_PTR
#   define CLARA_AUTO_PTR( T ) std::unique_ptr<T>
#else
#   define CLARA_AUTO_PTR( T ) std::auto_ptr<T>
#endif

#endif




#include <map>
#include <stdexcept>
#include <memory>


#ifdef STITCH_CLARA_OPEN_NAMESPACE
STITCH_CLARA_OPEN_NAMESPACE
#endif

namespace Clara {

    struct UnpositionalTag {};

    extern UnpositionalTag _;

#ifdef CLARA_CONFIG_MAIN
    UnpositionalTag _;
#endif

    namespace Detail {

#ifdef CLARA_CONSOLE_WIDTH
    const unsigned int consoleWidth = CLARA_CONFIG_CONSOLE_WIDTH;
#else
    const unsigned int consoleWidth = 80;
#endif


        inline bool isTrue( bool value ) { return value; }

        using namespace Tbc;

        inline bool startsWith( std::string const& str, std::string const& prefix ) {
            return str.size() >= prefix.size() && str.substr( 0, prefix.size() ) == prefix;
        }

        template<typename T> struct RemoveConstRef{ typedef T type; };
        template<typename T> struct RemoveConstRef<T&>{ typedef T type; };
        template<typename T> struct RemoveConstRef<T const&>{ typedef T type; };
        template<typename T> struct RemoveConstRef<T const>{ typedef T type; };

        template<typename T>    struct IsBool       { static const bool value = false; };
        template<>              struct IsBool<bool> { static const bool value = true; };

        template<typename T>
        void convertInto( std::string const& _source, T& _dest ) {
            std::stringstream ss;
            ss << _source;
            ss >> _dest;
            if( ss.fail() )
                throw std::runtime_error( "Unable to convert " + _source + " to destination type" );
        }
        inline void convertInto( std::string const& _source, std::string& _dest ) {
            _dest = _source;
        }
        inline void convertInto( std::string const& _source, bool& _dest ) {
            std::string sourceLC = _source;
            std::transform( sourceLC.begin(), sourceLC.end(), sourceLC.begin(), ::tolower );
            if( sourceLC == "y" || sourceLC == "1" || sourceLC == "true" || sourceLC == "yes" || sourceLC == "on" )
                _dest = true;
            else if( sourceLC == "n" || sourceLC == "0" || sourceLC == "false" || sourceLC == "no" || sourceLC == "off" )
                _dest = false;
            else
                throw std::runtime_error( "Expected a boolean value but did not recognise:\n  '" + _source + "'" );
        }
        inline void convertInto( bool _source, bool& _dest ) {
            _dest = _source;
        }
        template<typename T>
        inline void convertInto( bool, T& ) {
            if( isTrue( true ) )
                throw std::runtime_error( "Invalid conversion" );
        }

        template<typename ConfigT>
        struct IArgFunction {
            virtual ~IArgFunction() {}
#ifdef CLARA_CONFIG_CPP11_GENERATED_METHODS
            IArgFunction()                      = default;
            IArgFunction( IArgFunction const& ) = default;
#endif
            virtual void set( ConfigT& config, std::string const& value ) const = 0;
            virtual void setFlag( ConfigT& config ) const = 0;
            virtual bool takesArg() const = 0;
            virtual IArgFunction* clone() const = 0;
        };

        template<typename ConfigT>
        class BoundArgFunction {
        public:
            BoundArgFunction() : functionObj( CLARA_NULL ) {}
            BoundArgFunction( IArgFunction<ConfigT>* _functionObj ) : functionObj( _functionObj ) {}
            BoundArgFunction( BoundArgFunction const& other ) : functionObj( other.functionObj ? other.functionObj->clone() : CLARA_NULL ) {}
            BoundArgFunction& operator = ( BoundArgFunction const& other ) {
                IArgFunction<ConfigT>* newFunctionObj = other.functionObj ? other.functionObj->clone() : CLARA_NULL;
                delete functionObj;
                functionObj = newFunctionObj;
                return *this;
            }
            ~BoundArgFunction() { delete functionObj; }

            void set( ConfigT& config, std::string const& value ) const {
                functionObj->set( config, value );
            }
            void setFlag( ConfigT& config ) const {
                functionObj->setFlag( config );
            }
            bool takesArg() const { return functionObj->takesArg(); }

            bool isSet() const {
                return functionObj != CLARA_NULL;
            }
        private:
            IArgFunction<ConfigT>* functionObj;
        };

        template<typename C>
        struct NullBinder : IArgFunction<C>{
            virtual void set( C&, std::string const& ) const {}
            virtual void setFlag( C& ) const {}
            virtual bool takesArg() const { return true; }
            virtual IArgFunction<C>* clone() const { return new NullBinder( *this ); }
        };

        template<typename C, typename M>
        struct BoundDataMember : IArgFunction<C>{
            BoundDataMember( M C::* _member ) : member( _member ) {}
            virtual void set( C& p, std::string const& stringValue ) const {
                convertInto( stringValue, p.*member );
            }
            virtual void setFlag( C& p ) const {
                convertInto( true, p.*member );
            }
            virtual bool takesArg() const { return !IsBool<M>::value; }
            virtual IArgFunction<C>* clone() const { return new BoundDataMember( *this ); }
            M C::* member;
        };
        template<typename C, typename M>
        struct BoundUnaryMethod : IArgFunction<C>{
            BoundUnaryMethod( void (C::*_member)( M ) ) : member( _member ) {}
            virtual void set( C& p, std::string const& stringValue ) const {
                typename RemoveConstRef<M>::type value;
                convertInto( stringValue, value );
                (p.*member)( value );
            }
            virtual void setFlag( C& p ) const {
                typename RemoveConstRef<M>::type value;
                convertInto( true, value );
                (p.*member)( value );
            }
            virtual bool takesArg() const { return !IsBool<M>::value; }
            virtual IArgFunction<C>* clone() const { return new BoundUnaryMethod( *this ); }
            void (C::*member)( M );
        };
        template<typename C>
        struct BoundNullaryMethod : IArgFunction<C>{
            BoundNullaryMethod( void (C::*_member)() ) : member( _member ) {}
            virtual void set( C& p, std::string const& stringValue ) const {
                bool value;
                convertInto( stringValue, value );
                if( value )
                    (p.*member)();
            }
            virtual void setFlag( C& p ) const {
                (p.*member)();
            }
            virtual bool takesArg() const { return false; }
            virtual IArgFunction<C>* clone() const { return new BoundNullaryMethod( *this ); }
            void (C::*member)();
        };

        template<typename C>
        struct BoundUnaryFunction : IArgFunction<C>{
            BoundUnaryFunction( void (*_function)( C& ) ) : function( _function ) {}
            virtual void set( C& obj, std::string const& stringValue ) const {
                bool value;
                convertInto( stringValue, value );
                if( value )
                    function( obj );
            }
            virtual void setFlag( C& p ) const {
                function( p );
            }
            virtual bool takesArg() const { return false; }
            virtual IArgFunction<C>* clone() const { return new BoundUnaryFunction( *this ); }
            void (*function)( C& );
        };

        template<typename C, typename T>
        struct BoundBinaryFunction : IArgFunction<C>{
            BoundBinaryFunction( void (*_function)( C&, T ) ) : function( _function ) {}
            virtual void set( C& obj, std::string const& stringValue ) const {
                typename RemoveConstRef<T>::type value;
                convertInto( stringValue, value );
                function( obj, value );
            }
            virtual void setFlag( C& obj ) const {
                typename RemoveConstRef<T>::type value;
                convertInto( true, value );
                function( obj, value );
            }
            virtual bool takesArg() const { return !IsBool<T>::value; }
            virtual IArgFunction<C>* clone() const { return new BoundBinaryFunction( *this ); }
            void (*function)( C&, T );
        };

    }

    struct Parser {
        Parser() : separators( " \t=:" ) {}

        struct Token {
            enum Type { Positional, ShortOpt, LongOpt };
            Token( Type _type, std::string const& _data ) : type( _type ), data( _data ) {}
            Type type;
            std::string data;
        };

        void parseIntoTokens( int argc, char const* const argv[], std::vector<Parser::Token>& tokens ) const {
            const std::string doubleDash = "--";
            for( int i = 1; i < argc && argv[i] != doubleDash; ++i )
                parseIntoTokens( argv[i] , tokens);
        }
        void parseIntoTokens( std::string arg, std::vector<Parser::Token>& tokens ) const {
            while( !arg.empty() ) {
                Parser::Token token( Parser::Token::Positional, arg );
                arg = "";
                if( token.data[0] == '-' ) {
                    if( token.data.size() > 1 && token.data[1] == '-' ) {
                        token = Parser::Token( Parser::Token::LongOpt, token.data.substr( 2 ) );
                    }
                    else {
                        token = Parser::Token( Parser::Token::ShortOpt, token.data.substr( 1 ) );
                        if( token.data.size() > 1 && separators.find( token.data[1] ) == std::string::npos ) {
                            arg = "-" + token.data.substr( 1 );
                            token.data = token.data.substr( 0, 1 );
                        }
                    }
                }
                if( token.type != Parser::Token::Positional ) {
                    std::size_t pos = token.data.find_first_of( separators );
                    if( pos != std::string::npos ) {
                        arg = token.data.substr( pos+1 );
                        token.data = token.data.substr( 0, pos );
                    }
                }
                tokens.push_back( token );
            }
        }
        std::string separators;
    };

    template<typename ConfigT>
    struct CommonArgProperties {
        CommonArgProperties() {}
        CommonArgProperties( Detail::BoundArgFunction<ConfigT> const& _boundField ) : boundField( _boundField ) {}

        Detail::BoundArgFunction<ConfigT> boundField;
        std::string description;
        std::string detail;
        std::string placeholder;

        bool takesArg() const {
            return !placeholder.empty();
        }
        void validate() const {
            if( !boundField.isSet() )
                throw std::logic_error( "option not bound" );
        }
    };
    struct OptionArgProperties {
        std::vector<std::string> shortNames;
        std::string longName;

        bool hasShortName( std::string const& shortName ) const {
            return std::find( shortNames.begin(), shortNames.end(), shortName ) != shortNames.end();
        }
        bool hasLongName( std::string const& _longName ) const {
            return _longName == longName;
        }
    };
    struct PositionalArgProperties {
        PositionalArgProperties() : position( -1 ) {}
        int position;

        bool isFixedPositional() const {
            return position != -1;
        }
    };

    template<typename ConfigT>
    class CommandLine {

        struct Arg : CommonArgProperties<ConfigT>, OptionArgProperties, PositionalArgProperties {
            Arg() {}
            Arg( Detail::BoundArgFunction<ConfigT> const& _boundField ) : CommonArgProperties<ConfigT>( _boundField ) {}

            using CommonArgProperties<ConfigT>::placeholder;

            std::string dbgName() const {
                if( !longName.empty() )
                    return "--" + longName;
                if( !shortNames.empty() )
                    return "-" + shortNames[0];
                return "positional args";
            }
            std::string commands() const {
                std::ostringstream oss;
                bool first = true;
                std::vector<std::string>::const_iterator it = shortNames.begin(), itEnd = shortNames.end();
                for(; it != itEnd; ++it ) {
                    if( first )
                        first = false;
                    else
                        oss << ", ";
                    oss << "-" << *it;
                }
                if( !longName.empty() ) {
                    if( !first )
                        oss << ", ";
                    oss << "--" << longName;
                }
                if( !placeholder.empty() )
                    oss << " <" << placeholder << ">";
                return oss.str();
            }
        };

        typedef CLARA_AUTO_PTR( Arg ) ArgAutoPtr;

        friend void addOptName( Arg& arg, std::string const& optName )
        {
            if( optName.empty() )
                return;
            if( Detail::startsWith( optName, "--" ) ) {
                if( !arg.longName.empty() )
                    throw std::logic_error( "Only one long opt may be specified. '"
                        + arg.longName
                        + "' already specified, now attempting to add '"
                        + optName + "'" );
                arg.longName = optName.substr( 2 );
            }
            else if( Detail::startsWith( optName, "-" ) )
                arg.shortNames.push_back( optName.substr( 1 ) );
            else
                throw std::logic_error( "option must begin with - or --. Option was: '" + optName + "'" );
        }
        friend void setPositionalArg( Arg& arg, int position )
        {
            arg.position = position;
        }

        class ArgBuilder {
        public:
            ArgBuilder( Arg* arg ) : m_arg( arg ) {}


            template<typename C, typename M>
            void bind( M C::* field, std::string const& placeholder ) {
                m_arg->boundField = new Detail::BoundDataMember<C,M>( field );
                m_arg->placeholder = placeholder;
            }

            template<typename C>
            void bind( bool C::* field ) {
                m_arg->boundField = new Detail::BoundDataMember<C,bool>( field );
            }


            template<typename C, typename M>
            void bind( void (C::* unaryMethod)( M ), std::string const& placeholder ) {
                m_arg->boundField = new Detail::BoundUnaryMethod<C,M>( unaryMethod );
                m_arg->placeholder = placeholder;
            }


            template<typename C>
            void bind( void (C::* unaryMethod)( bool ) ) {
                m_arg->boundField = new Detail::BoundUnaryMethod<C,bool>( unaryMethod );
            }


            template<typename C>
            void bind( void (C::* nullaryMethod)() ) {
                m_arg->boundField = new Detail::BoundNullaryMethod<C>( nullaryMethod );
            }


            template<typename C>
            void bind( void (* unaryFunction)( C& ) ) {
                m_arg->boundField = new Detail::BoundUnaryFunction<C>( unaryFunction );
            }


            template<typename C, typename T>
            void bind( void (* binaryFunction)( C&, T ), std::string const& placeholder ) {
                m_arg->boundField = new Detail::BoundBinaryFunction<C, T>( binaryFunction );
                m_arg->placeholder = placeholder;
            }

            ArgBuilder& describe( std::string const& description ) {
                m_arg->description = description;
                return *this;
            }
            ArgBuilder& detail( std::string const& detail ) {
                m_arg->detail = detail;
                return *this;
            }

        protected:
            Arg* m_arg;
        };

        class OptBuilder : public ArgBuilder {
        public:
            OptBuilder( Arg* arg ) : ArgBuilder( arg ) {}
            OptBuilder( OptBuilder& other ) : ArgBuilder( other ) {}

            OptBuilder& operator[]( std::string const& optName ) {
                addOptName( *ArgBuilder::m_arg, optName );
                return *this;
            }
        };

    public:

        CommandLine()
        :   m_boundProcessName( new Detail::NullBinder<ConfigT>() ),
            m_highestSpecifiedArgPosition( 0 ),
            m_throwOnUnrecognisedTokens( false )
        {}
        CommandLine( CommandLine const& other )
        :   m_boundProcessName( other.m_boundProcessName ),
            m_options ( other.m_options ),
            m_positionalArgs( other.m_positionalArgs ),
            m_highestSpecifiedArgPosition( other.m_highestSpecifiedArgPosition ),
            m_throwOnUnrecognisedTokens( other.m_throwOnUnrecognisedTokens )
        {
            if( other.m_floatingArg.get() )
                m_floatingArg.reset( new Arg( *other.m_floatingArg ) );
        }

        CommandLine& setThrowOnUnrecognisedTokens( bool shouldThrow = true ) {
            m_throwOnUnrecognisedTokens = shouldThrow;
            return *this;
        }

        OptBuilder operator[]( std::string const& optName ) {
            m_options.push_back( Arg() );
            addOptName( m_options.back(), optName );
            OptBuilder builder( &m_options.back() );
            return builder;
        }

        ArgBuilder operator[]( int position ) {
            m_positionalArgs.insert( std::make_pair( position, Arg() ) );
            if( position > m_highestSpecifiedArgPosition )
                m_highestSpecifiedArgPosition = position;
            setPositionalArg( m_positionalArgs[position], position );
            ArgBuilder builder( &m_positionalArgs[position] );
            return builder;
        }


        ArgBuilder operator[]( UnpositionalTag ) {
            if( m_floatingArg.get() )
                throw std::logic_error( "Only one unpositional argument can be added" );
            m_floatingArg.reset( new Arg() );
            ArgBuilder builder( m_floatingArg.get() );
            return builder;
        }

        template<typename C, typename M>
        void bindProcessName( M C::* field ) {
            m_boundProcessName = new Detail::BoundDataMember<C,M>( field );
        }
        template<typename C, typename M>
        void bindProcessName( void (C::*_unaryMethod)( M ) ) {
            m_boundProcessName = new Detail::BoundUnaryMethod<C,M>( _unaryMethod );
        }

        void optUsage( std::ostream& os, std::size_t indent = 0, std::size_t width = Detail::consoleWidth ) const {
            typename std::vector<Arg>::const_iterator itBegin = m_options.begin(), itEnd = m_options.end(), it;
            std::size_t maxWidth = 0;
            for( it = itBegin; it != itEnd; ++it )
                maxWidth = (std::max)( maxWidth, it->commands().size() );

            for( it = itBegin; it != itEnd; ++it ) {
                Detail::Text usage( it->commands(), Detail::TextAttributes()
                                                        .setWidth( maxWidth+indent )
                                                        .setIndent( indent ) );
                Detail::Text desc( it->description, Detail::TextAttributes()
                                                        .setWidth( width - maxWidth - 3 ) );

                for( std::size_t i = 0; i < (std::max)( usage.size(), desc.size() ); ++i ) {
                    std::string usageCol = i < usage.size() ? usage[i] : "";
                    os << usageCol;

                    if( i < desc.size() && !desc[i].empty() )
                        os  << std::string( indent + 2 + maxWidth - usageCol.size(), ' ' )
                            << desc[i];
                    os << "\n";
                }
            }
        }
        std::string optUsage() const {
            std::ostringstream oss;
            optUsage( oss );
            return oss.str();
        }

        void argSynopsis( std::ostream& os ) const {
            for( int i = 1; i <= m_highestSpecifiedArgPosition; ++i ) {
                if( i > 1 )
                    os << " ";
                typename std::map<int, Arg>::const_iterator it = m_positionalArgs.find( i );
                if( it != m_positionalArgs.end() )
                    os << "<" << it->second.placeholder << ">";
                else if( m_floatingArg.get() )
                    os << "<" << m_floatingArg->placeholder << ">";
                else
                    throw std::logic_error( "non consecutive positional arguments with no floating args" );
            }

            if( m_floatingArg.get() ) {
                if( m_highestSpecifiedArgPosition > 1 )
                    os << " ";
                os << "[<" << m_floatingArg->placeholder << "> ...]";
            }
        }
        std::string argSynopsis() const {
            std::ostringstream oss;
            argSynopsis( oss );
            return oss.str();
        }

        void usage( std::ostream& os, std::string const& procName ) const {
            validate();
            os << "usage:\n  " << procName << " ";
            argSynopsis( os );
            if( !m_options.empty() ) {
                os << " [options]\n\nwhere options are: \n";
                optUsage( os, 2 );
            }
            os << "\n";
        }
        std::string usage( std::string const& procName ) const {
            std::ostringstream oss;
            usage( oss, procName );
            return oss.str();
        }

        ConfigT parse( int argc, char const* const argv[] ) const {
            ConfigT config;
            parseInto( argc, argv, config );
            return config;
        }

        std::vector<Parser::Token> parseInto( int argc, char const* argv[], ConfigT& config ) const {
            std::string processName = argv[0];
            std::size_t lastSlash = processName.find_last_of( "/\\" );
            if( lastSlash != std::string::npos )
                processName = processName.substr( lastSlash+1 );
            m_boundProcessName.set( config, processName );
            std::vector<Parser::Token> tokens;
            Parser parser;
            parser.parseIntoTokens( argc, argv, tokens );
            return populate( tokens, config );
        }

        std::vector<Parser::Token> populate( std::vector<Parser::Token> const& tokens, ConfigT& config ) const {
            validate();
            std::vector<Parser::Token> unusedTokens = populateOptions( tokens, config );
            unusedTokens = populateFixedArgs( unusedTokens, config );
            unusedTokens = populateFloatingArgs( unusedTokens, config );
            return unusedTokens;
        }

        std::vector<Parser::Token> populateOptions( std::vector<Parser::Token> const& tokens, ConfigT& config ) const {
            std::vector<Parser::Token> unusedTokens;
            std::vector<std::string> errors;
            for( std::size_t i = 0; i < tokens.size(); ++i ) {
                Parser::Token const& token = tokens[i];
                typename std::vector<Arg>::const_iterator it = m_options.begin(), itEnd = m_options.end();
                for(; it != itEnd; ++it ) {
                    Arg const& arg = *it;

                    try {
                        if( ( token.type == Parser::Token::ShortOpt && arg.hasShortName( token.data ) ) ||
                            ( token.type == Parser::Token::LongOpt && arg.hasLongName( token.data ) ) ) {
                            if( arg.takesArg() ) {
                                if( i == tokens.size()-1 || tokens[i+1].type != Parser::Token::Positional )
                                    errors.push_back( "Expected argument to option: " + token.data );
                                else
                                    arg.boundField.set( config, tokens[++i].data );
                            }
                            else {
                                arg.boundField.setFlag( config );
                            }
                            break;
                        }
                    }
                    catch( std::exception& ex ) {
                        errors.push_back( std::string( ex.what() ) + "\n- while parsing: (" + arg.commands() + ")" );
                    }
                }
                if( it == itEnd ) {
                    if( token.type == Parser::Token::Positional || !m_throwOnUnrecognisedTokens )
                        unusedTokens.push_back( token );
                    else if( errors.empty() && m_throwOnUnrecognisedTokens )
                        errors.push_back( "unrecognised option: " + token.data );
                }
            }
            if( !errors.empty() ) {
                std::ostringstream oss;
                for( std::vector<std::string>::const_iterator it = errors.begin(), itEnd = errors.end();
                        it != itEnd;
                        ++it ) {
                    if( it != errors.begin() )
                        oss << "\n";
                    oss << *it;
                }
                throw std::runtime_error( oss.str() );
            }
            return unusedTokens;
        }
        std::vector<Parser::Token> populateFixedArgs( std::vector<Parser::Token> const& tokens, ConfigT& config ) const {
            std::vector<Parser::Token> unusedTokens;
            int position = 1;
            for( std::size_t i = 0; i < tokens.size(); ++i ) {
                Parser::Token const& token = tokens[i];
                typename std::map<int, Arg>::const_iterator it = m_positionalArgs.find( position );
                if( it != m_positionalArgs.end() )
                    it->second.boundField.set( config, token.data );
                else
                    unusedTokens.push_back( token );
                if( token.type == Parser::Token::Positional )
                    position++;
            }
            return unusedTokens;
        }
        std::vector<Parser::Token> populateFloatingArgs( std::vector<Parser::Token> const& tokens, ConfigT& config ) const {
            if( !m_floatingArg.get() )
                return tokens;
            std::vector<Parser::Token> unusedTokens;
            for( std::size_t i = 0; i < tokens.size(); ++i ) {
                Parser::Token const& token = tokens[i];
                if( token.type == Parser::Token::Positional )
                    m_floatingArg->boundField.set( config, token.data );
                else
                    unusedTokens.push_back( token );
            }
            return unusedTokens;
        }

        void validate() const
        {
            if( m_options.empty() && m_positionalArgs.empty() && !m_floatingArg.get() )
                throw std::logic_error( "No options or arguments specified" );

            for( typename std::vector<Arg>::const_iterator  it = m_options.begin(),
                                                            itEnd = m_options.end();
                    it != itEnd; ++it )
                it->validate();
        }

    private:
        Detail::BoundArgFunction<ConfigT> m_boundProcessName;
        std::vector<Arg> m_options;
        std::map<int, Arg> m_positionalArgs;
        ArgAutoPtr m_floatingArg;
        int m_highestSpecifiedArgPosition;
        bool m_throwOnUnrecognisedTokens;
    };

}

STITCH_CLARA_CLOSE_NAMESPACE
#undef STITCH_CLARA_OPEN_NAMESPACE
#undef STITCH_CLARA_CLOSE_NAMESPACE

#endif
#undef STITCH_CLARA_OPEN_NAMESPACE


#ifdef CATCH_TEMP_CLARA_CONFIG_CONSOLE_WIDTH
#define CLARA_CONFIG_CONSOLE_WIDTH CATCH_TEMP_CLARA_CONFIG_CONSOLE_WIDTH
#undef CATCH_TEMP_CLARA_CONFIG_CONSOLE_WIDTH
#endif

#include <fstream>

namespace Catch {

    inline void abortAfterFirst( ConfigData& config ) { config.abortAfter = 1; }
    inline void abortAfterX( ConfigData& config, int x ) {
        if( x < 1 )
            throw std::runtime_error( "Value after -x or --abortAfter must be greater than zero" );
        config.abortAfter = x;
    }
    inline void addTestOrTags( ConfigData& config, std::string const& _testSpec ) { config.testsOrTags.push_back( _testSpec ); }
    inline void addReporterName( ConfigData& config, std::string const& _reporterName ) { config.reporterNames.push_back( _reporterName ); }

    inline void addWarning( ConfigData& config, std::string const& _warning ) {
        if( _warning == "NoAssertions" )
            config.warnings = static_cast<WarnAbout::What>( config.warnings | WarnAbout::NoAssertions );
        else
            throw std::runtime_error( "Unrecognised warning: '" + _warning + "'" );
    }
    inline void setOrder( ConfigData& config, std::string const& order ) {
        if( startsWith( "declared", order ) )
            config.runOrder = RunTests::InDeclarationOrder;
        else if( startsWith( "lexical", order ) )
            config.runOrder = RunTests::InLexicographicalOrder;
        else if( startsWith( "random", order ) )
            config.runOrder = RunTests::InRandomOrder;
        else
            throw std::runtime_error( "Unrecognised ordering: '" + order + "'" );
    }
    inline void setRngSeed( ConfigData& config, std::string const& seed ) {
        if( seed == "time" ) {
            config.rngSeed = static_cast<unsigned int>( std::time(0) );
        }
        else {
            std::stringstream ss;
            ss << seed;
            ss >> config.rngSeed;
            if( ss.fail() )
                throw std::runtime_error( "Argment to --rng-seed should be the word 'time' or a number" );
        }
    }
    inline void setVerbosity( ConfigData& config, int level ) {

        config.verbosity = static_cast<Verbosity::Level>( level );
    }
    inline void setShowDurations( ConfigData& config, bool _showDurations ) {
        config.showDurations = _showDurations
            ? ShowDurations::Always
            : ShowDurations::Never;
    }
    inline void loadTestNamesFromFile( ConfigData& config, std::string const& _filename ) {
        std::ifstream f( _filename.c_str() );
        if( !f.is_open() )
            throw std::domain_error( "Unable to load input file: " + _filename );

        std::string line;
        while( std::getline( f, line ) ) {
            line = trim(line);
            if( !line.empty() && !startsWith( line, "#" ) )
                addTestOrTags( config, "\"" + line + "\"," );
        }
    }

    inline Clara::CommandLine<ConfigData> makeCommandLineParser() {

        using namespace Clara;
        CommandLine<ConfigData> cli;

        cli.bindProcessName( &ConfigData::processName );

        cli["-?"]["-h"]["--help"]
            .describe( "display usage information" )
            .bind( &ConfigData::showHelp );

        cli["-l"]["--list-tests"]
            .describe( "list all/matching test cases" )
            .bind( &ConfigData::listTests );

        cli["-t"]["--list-tags"]
            .describe( "list all/matching tags" )
            .bind( &ConfigData::listTags );

        cli["-s"]["--success"]
            .describe( "include successful tests in output" )
            .bind( &ConfigData::showSuccessfulTests );

        cli["-b"]["--break"]
            .describe( "break into debugger on failure" )
            .bind( &ConfigData::shouldDebugBreak );

        cli["-e"]["--nothrow"]
            .describe( "skip exception tests" )
            .bind( &ConfigData::noThrow );

        cli["-i"]["--invisibles"]
            .describe( "show invisibles (tabs, newlines)" )
            .bind( &ConfigData::showInvisibles );

        cli["-o"]["--out"]
            .describe( "output filename" )
            .bind( &ConfigData::outputFilename, "filename" );

        cli["-r"]["--reporter"]

            .describe( "reporter to use (defaults to console)" )
            .bind( &addReporterName, "name" );

        cli["-n"]["--name"]
            .describe( "suite name" )
            .bind( &ConfigData::name, "name" );

        cli["-a"]["--abort"]
            .describe( "abort at first failure" )
            .bind( &abortAfterFirst );

        cli["-x"]["--abortx"]
            .describe( "abort after x failures" )
            .bind( &abortAfterX, "no. failures" );

        cli["-w"]["--warn"]
            .describe( "enable warnings" )
            .bind( &addWarning, "warning name" );








        cli[_]
            .describe( "which test or tests to use" )
            .bind( &addTestOrTags, "test name, pattern or tags" );

        cli["-d"]["--durations"]
            .describe( "show test durations" )
            .bind( &setShowDurations, "yes/no" );

        cli["-f"]["--input-file"]
            .describe( "load test names to run from a file" )
            .bind( &loadTestNamesFromFile, "filename" );

        cli["-#"]["--filenames-as-tags"]
            .describe( "adds a tag for the filename" )
            .bind( &ConfigData::filenamesAsTags );


        cli["--list-test-names-only"]
            .describe( "list all/matching test cases names only" )
            .bind( &ConfigData::listTestNamesOnly );

        cli["--list-reporters"]
            .describe( "list all reporters" )
            .bind( &ConfigData::listReporters );

        cli["--order"]
            .describe( "test case order (defaults to decl)" )
            .bind( &setOrder, "decl|lex|rand" );

        cli["--rng-seed"]
            .describe( "set a specific seed for random numbers" )
            .bind( &setRngSeed, "'time'|number" );

        cli["--force-colour"]
            .describe( "force colourised output" )
            .bind( &ConfigData::forceColour );

        return cli;
    }

}


#define TWOBLUECUBES_CATCH_LIST_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_TEXT_H_INCLUDED

#define TBC_TEXT_FORMAT_CONSOLE_WIDTH CATCH_CONFIG_CONSOLE_WIDTH

#define CLICHE_TBC_TEXT_FORMAT_OUTER_NAMESPACE Catch


#ifndef CLICHE_TBC_TEXT_FORMAT_OUTER_NAMESPACE
# ifdef TWOBLUECUBES_TEXT_FORMAT_H_INCLUDED
#  ifndef TWOBLUECUBES_TEXT_FORMAT_H_ALREADY_INCLUDED
#   define TWOBLUECUBES_TEXT_FORMAT_H_ALREADY_INCLUDED
#  endif
# else
#  define TWOBLUECUBES_TEXT_FORMAT_H_INCLUDED
# endif
#endif
#ifndef TWOBLUECUBES_TEXT_FORMAT_H_ALREADY_INCLUDED
#include <string>
#include <vector>
#include <sstream>


#ifdef CLICHE_TBC_TEXT_FORMAT_OUTER_NAMESPACE
namespace CLICHE_TBC_TEXT_FORMAT_OUTER_NAMESPACE {
#endif

namespace Tbc {

#ifdef TBC_TEXT_FORMAT_CONSOLE_WIDTH
    const unsigned int consoleWidth = TBC_TEXT_FORMAT_CONSOLE_WIDTH;
#else
    const unsigned int consoleWidth = 80;
#endif

    struct TextAttributes {
        TextAttributes()
        :   initialIndent( std::string::npos ),
            indent( 0 ),
            width( consoleWidth-1 ),
            tabChar( '\t' )
        {}

        TextAttributes& setInitialIndent( std::size_t _value )  { initialIndent = _value; return *this; }
        TextAttributes& setIndent( std::size_t _value )         { indent = _value; return *this; }
        TextAttributes& setWidth( std::size_t _value )          { width = _value; return *this; }
        TextAttributes& setTabChar( char _value )               { tabChar = _value; return *this; }

        std::size_t initialIndent;
        std::size_t indent;
        std::size_t width;
        char tabChar;
    };

    class Text {
    public:
        Text( std::string const& _str, TextAttributes const& _attr = TextAttributes() )
        : attr( _attr )
        {
            std::string wrappableChars = " [({.,/|\\-";
            std::size_t indent = _attr.initialIndent != std::string::npos
                ? _attr.initialIndent
                : _attr.indent;
            std::string remainder = _str;

            while( !remainder.empty() ) {
                if( lines.size() >= 1000 ) {
                    lines.push_back( "... message truncated due to excessive size" );
                    return;
                }
                std::size_t tabPos = std::string::npos;
                std::size_t width = (std::min)( remainder.size(), _attr.width - indent );
                std::size_t pos = remainder.find_first_of( '\n' );
                if( pos <= width ) {
                    width = pos;
                }
                pos = remainder.find_last_of( _attr.tabChar, width );
                if( pos != std::string::npos ) {
                    tabPos = pos;
                    if( remainder[width] == '\n' )
                        width--;
                    remainder = remainder.substr( 0, tabPos ) + remainder.substr( tabPos+1 );
                }

                if( width == remainder.size() ) {
                    spliceLine( indent, remainder, width );
                }
                else if( remainder[width] == '\n' ) {
                    spliceLine( indent, remainder, width );
                    if( width <= 1 || remainder.size() != 1 )
                        remainder = remainder.substr( 1 );
                    indent = _attr.indent;
                }
                else {
                    pos = remainder.find_last_of( wrappableChars, width );
                    if( pos != std::string::npos && pos > 0 ) {
                        spliceLine( indent, remainder, pos );
                        if( remainder[0] == ' ' )
                            remainder = remainder.substr( 1 );
                    }
                    else {
                        spliceLine( indent, remainder, width-1 );
                        lines.back() += "-";
                    }
                    if( lines.size() == 1 )
                        indent = _attr.indent;
                    if( tabPos != std::string::npos )
                        indent += tabPos;
                }
            }
        }

        void spliceLine( std::size_t _indent, std::string& _remainder, std::size_t _pos ) {
            lines.push_back( std::string( _indent, ' ' ) + _remainder.substr( 0, _pos ) );
            _remainder = _remainder.substr( _pos );
        }

        typedef std::vector<std::string>::const_iterator const_iterator;

        const_iterator begin() const { return lines.begin(); }
        const_iterator end() const { return lines.end(); }
        std::string const& last() const { return lines.back(); }
        std::size_t size() const { return lines.size(); }
        std::string const& operator[]( std::size_t _index ) const { return lines[_index]; }
        std::string toString() const {
            std::ostringstream oss;
            oss << *this;
            return oss.str();
        }

        inline friend std::ostream& operator << ( std::ostream& _stream, Text const& _text ) {
            for( Text::const_iterator it = _text.begin(), itEnd = _text.end();
                it != itEnd; ++it ) {
                if( it != _text.begin() )
                    _stream << "\n";
                _stream << *it;
            }
            return _stream;
        }

    private:
        std::string str;
        TextAttributes attr;
        std::vector<std::string> lines;
    };

}

#ifdef CLICHE_TBC_TEXT_FORMAT_OUTER_NAMESPACE
}
#endif

#endif
#undef CLICHE_TBC_TEXT_FORMAT_OUTER_NAMESPACE

namespace Catch {
    using Tbc::Text;
    using Tbc::TextAttributes;
}


#define TWOBLUECUBES_CATCH_CONSOLE_COLOUR_HPP_INCLUDED

namespace Catch {

    struct Colour {
        enum Code {
            None = 0,

            White,
            Red,
            Green,
            Blue,
            Cyan,
            Yellow,
            Grey,

            Bright = 0x10,

            BrightRed = Bright | Red,
            BrightGreen = Bright | Green,
            LightGrey = Bright | Grey,
            BrightWhite = Bright | White,


            FileName = LightGrey,
            Warning = Yellow,
            ResultError = BrightRed,
            ResultSuccess = BrightGreen,
            ResultExpectedFailure = Warning,

            Error = BrightRed,
            Success = Green,

            OriginalExpression = Cyan,
            ReconstructedExpression = Yellow,

            SecondaryText = LightGrey,
            Headers = White
        };


        Colour( Code _colourCode );
        Colour( Colour const& other );
        ~Colour();


        static void use( Code _colourCode );

    private:
        bool m_moved;
    };

    inline std::ostream& operator << ( std::ostream& os, Colour const& ) { return os; }

}


#define TWOBLUECUBES_CATCH_INTERFACES_REPORTER_H_INCLUDED

#include <string>
#include <ostream>
#include <map>
#include <assert.h>

namespace Catch
{
    struct ReporterConfig {
        explicit ReporterConfig( Ptr<IConfig const> const& _fullConfig )
        :   m_stream( &_fullConfig->stream() ), m_fullConfig( _fullConfig ) {}

        ReporterConfig( Ptr<IConfig const> const& _fullConfig, std::ostream& _stream )
        :   m_stream( &_stream ), m_fullConfig( _fullConfig ) {}

        std::ostream& stream() const    { return *m_stream; }
        Ptr<IConfig const> fullConfig() const { return m_fullConfig; }

    private:
        std::ostream* m_stream;
        Ptr<IConfig const> m_fullConfig;
    };

    struct ReporterPreferences {
        ReporterPreferences()
        : shouldRedirectStdOut( false )
        {}

        bool shouldRedirectStdOut;
    };

    template<typename T>
    struct LazyStat : Option<T> {
        LazyStat() : used( false ) {}
        LazyStat& operator=( T const& _value ) {
            Option<T>::operator=( _value );
            used = false;
            return *this;
        }
        void reset() {
            Option<T>::reset();
            used = false;
        }
        bool used;
    };

    struct TestRunInfo {
        TestRunInfo( std::string const& _name ) : name( _name ) {}
        std::string name;
    };
    struct GroupInfo {
        GroupInfo(  std::string const& _name,
                    std::size_t _groupIndex,
                    std::size_t _groupsCount )
        :   name( _name ),
            groupIndex( _groupIndex ),
            groupsCounts( _groupsCount )
        {}

        std::string name;
        std::size_t groupIndex;
        std::size_t groupsCounts;
    };

    struct AssertionStats {
        AssertionStats( AssertionResult const& _assertionResult,
                        std::vector<MessageInfo> const& _infoMessages,
                        Totals const& _totals )
        :   assertionResult( _assertionResult ),
            infoMessages( _infoMessages ),
            totals( _totals )
        {
            if( assertionResult.hasMessage() ) {


                MessageBuilder builder( assertionResult.getTestMacroName(), assertionResult.getSourceInfo(), assertionResult.getResultType() );
                builder << assertionResult.getMessage();
                builder.m_info.message = builder.m_stream.str();

                infoMessages.push_back( builder.m_info );
            }
        }
        virtual ~AssertionStats();

#  ifdef CATCH_CONFIG_CPP11_GENERATED_METHODS
        AssertionStats( AssertionStats const& )              = default;
        AssertionStats( AssertionStats && )                  = default;
        AssertionStats& operator = ( AssertionStats const& ) = default;
        AssertionStats& operator = ( AssertionStats && )     = default;
#  endif

        AssertionResult assertionResult;
        std::vector<MessageInfo> infoMessages;
        Totals totals;
    };

    struct SectionStats {
        SectionStats(   SectionInfo const& _sectionInfo,
                        Counts const& _assertions,
                        double _durationInSeconds,
                        bool _missingAssertions )
        :   sectionInfo( _sectionInfo ),
            assertions( _assertions ),
            durationInSeconds( _durationInSeconds ),
            missingAssertions( _missingAssertions )
        {}
        virtual ~SectionStats();
#  ifdef CATCH_CONFIG_CPP11_GENERATED_METHODS
        SectionStats( SectionStats const& )              = default;
        SectionStats( SectionStats && )                  = default;
        SectionStats& operator = ( SectionStats const& ) = default;
        SectionStats& operator = ( SectionStats && )     = default;
#  endif

        SectionInfo sectionInfo;
        Counts assertions;
        double durationInSeconds;
        bool missingAssertions;
    };

    struct TestCaseStats {
        TestCaseStats(  TestCaseInfo const& _testInfo,
                        Totals const& _totals,
                        std::string const& _stdOut,
                        std::string const& _stdErr,
                        bool _aborting )
        : testInfo( _testInfo ),
            totals( _totals ),
            stdOut( _stdOut ),
            stdErr( _stdErr ),
            aborting( _aborting )
        {}
        virtual ~TestCaseStats();

#  ifdef CATCH_CONFIG_CPP11_GENERATED_METHODS
        TestCaseStats( TestCaseStats const& )              = default;
        TestCaseStats( TestCaseStats && )                  = default;
        TestCaseStats& operator = ( TestCaseStats const& ) = default;
        TestCaseStats& operator = ( TestCaseStats && )     = default;
#  endif

        TestCaseInfo testInfo;
        Totals totals;
        std::string stdOut;
        std::string stdErr;
        bool aborting;
    };

    struct TestGroupStats {
        TestGroupStats( GroupInfo const& _groupInfo,
                        Totals const& _totals,
                        bool _aborting )
        :   groupInfo( _groupInfo ),
            totals( _totals ),
            aborting( _aborting )
        {}
        TestGroupStats( GroupInfo const& _groupInfo )
        :   groupInfo( _groupInfo ),
            aborting( false )
        {}
        virtual ~TestGroupStats();

#  ifdef CATCH_CONFIG_CPP11_GENERATED_METHODS
        TestGroupStats( TestGroupStats const& )              = default;
        TestGroupStats( TestGroupStats && )                  = default;
        TestGroupStats& operator = ( TestGroupStats const& ) = default;
        TestGroupStats& operator = ( TestGroupStats && )     = default;
#  endif

        GroupInfo groupInfo;
        Totals totals;
        bool aborting;
    };

    struct TestRunStats {
        TestRunStats(   TestRunInfo const& _runInfo,
                        Totals const& _totals,
                        bool _aborting )
        :   runInfo( _runInfo ),
            totals( _totals ),
            aborting( _aborting )
        {}
        virtual ~TestRunStats();

#  ifndef CATCH_CONFIG_CPP11_GENERATED_METHODS
        TestRunStats( TestRunStats const& _other )
        :   runInfo( _other.runInfo ),
            totals( _other.totals ),
            aborting( _other.aborting )
        {}
#  else
        TestRunStats( TestRunStats const& )              = default;
        TestRunStats( TestRunStats && )                  = default;
        TestRunStats& operator = ( TestRunStats const& ) = default;
        TestRunStats& operator = ( TestRunStats && )     = default;
#  endif

        TestRunInfo runInfo;
        Totals totals;
        bool aborting;
    };

    struct IStreamingReporter : IShared {
        virtual ~IStreamingReporter();




        virtual ReporterPreferences getPreferences() const = 0;

        virtual void noMatchingTestCases( std::string const& spec ) = 0;

        virtual void testRunStarting( TestRunInfo const& testRunInfo ) = 0;
        virtual void testGroupStarting( GroupInfo const& groupInfo ) = 0;

        virtual void testCaseStarting( TestCaseInfo const& testInfo ) = 0;
        virtual void sectionStarting( SectionInfo const& sectionInfo ) = 0;

        virtual void assertionStarting( AssertionInfo const& assertionInfo ) = 0;


        virtual bool assertionEnded( AssertionStats const& assertionStats ) = 0;

        virtual void sectionEnded( SectionStats const& sectionStats ) = 0;
        virtual void testCaseEnded( TestCaseStats const& testCaseStats ) = 0;
        virtual void testGroupEnded( TestGroupStats const& testGroupStats ) = 0;
        virtual void testRunEnded( TestRunStats const& testRunStats ) = 0;

        virtual void skipTest( TestCaseInfo const& testInfo ) = 0;
    };

    struct IReporterFactory : IShared {
        virtual ~IReporterFactory();
        virtual IStreamingReporter* create( ReporterConfig const& config ) const = 0;
        virtual std::string getDescription() const = 0;
    };

    struct IReporterRegistry {
        typedef std::map<std::string, Ptr<IReporterFactory> > FactoryMap;
        typedef std::vector<Ptr<IReporterFactory> > Listeners;

        virtual ~IReporterRegistry();
        virtual IStreamingReporter* create( std::string const& name, Ptr<IConfig const> const& config ) const = 0;
        virtual FactoryMap const& getFactories() const = 0;
        virtual Listeners const& getListeners() const = 0;
    };

    Ptr<IStreamingReporter> addReporter( Ptr<IStreamingReporter> const& existingReporter, Ptr<IStreamingReporter> const& additionalReporter );

}

#include <limits>
#include <algorithm>

namespace Catch {

    inline std::size_t listTests( Config const& config ) {

        TestSpec testSpec = config.testSpec();
        if( config.testSpec().hasFilters() )
            Catch::cout() << "Matching test cases:\n";
        else {
            Catch::cout() << "All available test cases:\n";
            testSpec = TestSpecParser( ITagAliasRegistry::get() ).parse( "*" ).testSpec();
        }

        std::size_t matchedTests = 0;
        TextAttributes nameAttr, tagsAttr;
        nameAttr.setInitialIndent( 2 ).setIndent( 4 );
        tagsAttr.setIndent( 6 );

        std::vector<TestCase> matchedTestCases = filterTests( getAllTestCasesSorted( config ), testSpec, config );
        for( std::vector<TestCase>::const_iterator it = matchedTestCases.begin(), itEnd = matchedTestCases.end();
                it != itEnd;
                ++it ) {
            matchedTests++;
            TestCaseInfo const& testCaseInfo = it->getTestCaseInfo();
            Colour::Code colour = testCaseInfo.isHidden()
                ? Colour::SecondaryText
                : Colour::None;
            Colour colourGuard( colour );

            Catch::cout() << Text( testCaseInfo.name, nameAttr ) << std::endl;
            if( !testCaseInfo.tags.empty() )
                Catch::cout() << Text( testCaseInfo.tagsAsString, tagsAttr ) << std::endl;
        }

        if( !config.testSpec().hasFilters() )
            Catch::cout() << pluralise( matchedTests, "test case" ) << "\n" << std::endl;
        else
            Catch::cout() << pluralise( matchedTests, "matching test case" ) << "\n" << std::endl;
        return matchedTests;
    }

    inline std::size_t listTestsNamesOnly( Config const& config ) {
        TestSpec testSpec = config.testSpec();
        if( !config.testSpec().hasFilters() )
            testSpec = TestSpecParser( ITagAliasRegistry::get() ).parse( "*" ).testSpec();
        std::size_t matchedTests = 0;
        std::vector<TestCase> matchedTestCases = filterTests( getAllTestCasesSorted( config ), testSpec, config );
        for( std::vector<TestCase>::const_iterator it = matchedTestCases.begin(), itEnd = matchedTestCases.end();
                it != itEnd;
                ++it ) {
            matchedTests++;
            TestCaseInfo const& testCaseInfo = it->getTestCaseInfo();
            Catch::cout() << testCaseInfo.name << std::endl;
        }
        return matchedTests;
    }

    struct TagInfo {
        TagInfo() : count ( 0 ) {}
        void add( std::string const& spelling ) {
            ++count;
            spellings.insert( spelling );
        }
        std::string all() const {
            std::string out;
            for( std::set<std::string>::const_iterator it = spellings.begin(), itEnd = spellings.end();
                        it != itEnd;
                        ++it )
                out += "[" + *it + "]";
            return out;
        }
        std::set<std::string> spellings;
        std::size_t count;
    };

    inline std::size_t listTags( Config const& config ) {
        TestSpec testSpec = config.testSpec();
        if( config.testSpec().hasFilters() )
            Catch::cout() << "Tags for matching test cases:\n";
        else {
            Catch::cout() << "All available tags:\n";
            testSpec = TestSpecParser( ITagAliasRegistry::get() ).parse( "*" ).testSpec();
        }

        std::map<std::string, TagInfo> tagCounts;

        std::vector<TestCase> matchedTestCases = filterTests( getAllTestCasesSorted( config ), testSpec, config );
        for( std::vector<TestCase>::const_iterator it = matchedTestCases.begin(), itEnd = matchedTestCases.end();
                it != itEnd;
                ++it ) {
            for( std::set<std::string>::const_iterator  tagIt = it->getTestCaseInfo().tags.begin(),
                                                        tagItEnd = it->getTestCaseInfo().tags.end();
                    tagIt != tagItEnd;
                    ++tagIt ) {
                std::string tagName = *tagIt;
                std::string lcaseTagName = toLower( tagName );
                std::map<std::string, TagInfo>::iterator countIt = tagCounts.find( lcaseTagName );
                if( countIt == tagCounts.end() )
                    countIt = tagCounts.insert( std::make_pair( lcaseTagName, TagInfo() ) ).first;
                countIt->second.add( tagName );
            }
        }

        for( std::map<std::string, TagInfo>::const_iterator countIt = tagCounts.begin(),
                                                            countItEnd = tagCounts.end();
                countIt != countItEnd;
                ++countIt ) {
            std::ostringstream oss;
            oss << "  " << std::setw(2) << countIt->second.count << "  ";
            Text wrapper( countIt->second.all(), TextAttributes()
                                                    .setInitialIndent( 0 )
                                                    .setIndent( oss.str().size() )
                                                    .setWidth( CATCH_CONFIG_CONSOLE_WIDTH-10 ) );
            Catch::cout() << oss.str() << wrapper << "\n";
        }
        Catch::cout() << pluralise( tagCounts.size(), "tag" ) << "\n" << std::endl;
        return tagCounts.size();
    }

    inline std::size_t listReporters( Config const& ) {
        Catch::cout() << "Available reporters:\n";
        IReporterRegistry::FactoryMap const& factories = getRegistryHub().getReporterRegistry().getFactories();
        IReporterRegistry::FactoryMap::const_iterator itBegin = factories.begin(), itEnd = factories.end(), it;
        std::size_t maxNameLen = 0;
        for(it = itBegin; it != itEnd; ++it )
            maxNameLen = (std::max)( maxNameLen, it->first.size() );

        for(it = itBegin; it != itEnd; ++it ) {
            Text wrapper( it->second->getDescription(), TextAttributes()
                                                        .setInitialIndent( 0 )
                                                        .setIndent( 7+maxNameLen )
                                                        .setWidth( CATCH_CONFIG_CONSOLE_WIDTH - maxNameLen-8 ) );
            Catch::cout() << "  "
                    << it->first
                    << ":"
                    << std::string( maxNameLen - it->first.size() + 2, ' ' )
                    << wrapper << "\n";
        }
        Catch::cout() << std::endl;
        return factories.size();
    }

    inline Option<std::size_t> list( Config const& config ) {
        Option<std::size_t> listedCount;
        if( config.listTests() )
            listedCount = listedCount.valueOr(0) + listTests( config );
        if( config.listTestNamesOnly() )
            listedCount = listedCount.valueOr(0) + listTestsNamesOnly( config );
        if( config.listTags() )
            listedCount = listedCount.valueOr(0) + listTags( config );
        if( config.listReporters() )
            listedCount = listedCount.valueOr(0) + listReporters( config );
        return listedCount;
    }

}


#define TWOBLUECUBES_CATCH_RUNNER_IMPL_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_TEST_CASE_TRACKER_HPP_INCLUDED

#include <map>
#include <string>
#include <assert.h>
#include <vector>

namespace Catch {
namespace TestCaseTracking {

    struct ITracker : SharedImpl<> {
        virtual ~ITracker();


        virtual std::string name() const = 0;


        virtual bool isComplete() const = 0;
        virtual bool isSuccessfullyCompleted() const = 0;
        virtual bool isOpen() const = 0;
        virtual bool hasChildren() const = 0;

        virtual ITracker& parent() = 0;


        virtual void close() = 0;
        virtual void fail() = 0;
        virtual void markAsNeedingAnotherRun() = 0;

        virtual void addChild( Ptr<ITracker> const& child ) = 0;
        virtual ITracker* findChild( std::string const& name ) = 0;
        virtual void openChild() = 0;
    };

    class TrackerContext {

        enum RunState {
            NotStarted,
            Executing,
            CompletedCycle
        };

        Ptr<ITracker> m_rootTracker;
        ITracker* m_currentTracker;
        RunState m_runState;

    public:

        static TrackerContext& instance() {
            static TrackerContext s_instance;
            return s_instance;
        }

        TrackerContext()
        :   m_currentTracker( CATCH_NULL ),
            m_runState( NotStarted )
        {}

        ITracker& startRun();

        void endRun() {
            m_rootTracker.reset();
            m_currentTracker = CATCH_NULL;
            m_runState = NotStarted;
        }

        void startCycle() {
            m_currentTracker = m_rootTracker.get();
            m_runState = Executing;
        }
        void completeCycle() {
            m_runState = CompletedCycle;
        }

        bool completedCycle() const {
            return m_runState == CompletedCycle;
        }
        ITracker& currentTracker() {
            return *m_currentTracker;
        }
        void setCurrentTracker( ITracker* tracker ) {
            m_currentTracker = tracker;
        }
    };

    class TrackerBase : public ITracker {
    protected:
        enum CycleState {
            NotStarted,
            Executing,
            ExecutingChildren,
            NeedsAnotherRun,
            CompletedSuccessfully,
            Failed
        };
        class TrackerHasName {
            std::string m_name;
        public:
            TrackerHasName( std::string const& name ) : m_name( name ) {}
            bool operator ()( Ptr<ITracker> const& tracker ) {
                return tracker->name() == m_name;
            }
        };
        typedef std::vector<Ptr<ITracker> > Children;
        std::string m_name;
        TrackerContext& m_ctx;
        ITracker* m_parent;
        Children m_children;
        CycleState m_runState;
    public:
        TrackerBase( std::string const& name, TrackerContext& ctx, ITracker* parent )
        :   m_name( name ),
            m_ctx( ctx ),
            m_parent( parent ),
            m_runState( NotStarted )
        {}
        virtual ~TrackerBase();

        virtual std::string name() const CATCH_OVERRIDE {
            return m_name;
        }
        virtual bool isComplete() const CATCH_OVERRIDE {
            return m_runState == CompletedSuccessfully || m_runState == Failed;
        }
        virtual bool isSuccessfullyCompleted() const CATCH_OVERRIDE {
            return m_runState == CompletedSuccessfully;
        }
        virtual bool isOpen() const CATCH_OVERRIDE {
            return m_runState != NotStarted && !isComplete();
        }
        virtual bool hasChildren() const CATCH_OVERRIDE {
            return !m_children.empty();
        }

        virtual void addChild( Ptr<ITracker> const& child ) CATCH_OVERRIDE {
            m_children.push_back( child );
        }

        virtual ITracker* findChild( std::string const& name ) CATCH_OVERRIDE {
            Children::const_iterator it = std::find_if( m_children.begin(), m_children.end(), TrackerHasName( name ) );
            return( it != m_children.end() )
                ? it->get()
                : CATCH_NULL;
        }
        virtual ITracker& parent() CATCH_OVERRIDE {
            assert( m_parent );
            return *m_parent;
        }

        virtual void openChild() CATCH_OVERRIDE {
            if( m_runState != ExecutingChildren ) {
                m_runState = ExecutingChildren;
                if( m_parent )
                    m_parent->openChild();
            }
        }
        void open() {
            m_runState = Executing;
            moveToThis();
            if( m_parent )
                m_parent->openChild();
        }

        virtual void close() CATCH_OVERRIDE {


            while( &m_ctx.currentTracker() != this )
                m_ctx.currentTracker().close();

            switch( m_runState ) {
                case NotStarted:
                case CompletedSuccessfully:
                case Failed:
                    throw std::logic_error( "Illogical state" );

                case NeedsAnotherRun:
                    break;;

                case Executing:
                    m_runState = CompletedSuccessfully;
                    break;
                case ExecutingChildren:
                    if( m_children.empty() || m_children.back()->isComplete() )
                        m_runState = CompletedSuccessfully;
                    break;

                default:
                    throw std::logic_error( "Unexpected state" );
            }
            moveToParent();
            m_ctx.completeCycle();
        }
        virtual void fail() CATCH_OVERRIDE {
            m_runState = Failed;
            if( m_parent )
                m_parent->markAsNeedingAnotherRun();
            moveToParent();
            m_ctx.completeCycle();
        }
        virtual void markAsNeedingAnotherRun() CATCH_OVERRIDE {
            m_runState = NeedsAnotherRun;
        }
    private:
        void moveToParent() {
            assert( m_parent );
            m_ctx.setCurrentTracker( m_parent );
        }
        void moveToThis() {
            m_ctx.setCurrentTracker( this );
        }
    };

    class SectionTracker : public TrackerBase {
    public:
        SectionTracker( std::string const& name, TrackerContext& ctx, ITracker* parent )
        :   TrackerBase( name, ctx, parent )
        {}
        virtual ~SectionTracker();

        static SectionTracker& acquire( TrackerContext& ctx, std::string const& name ) {
            SectionTracker* section = CATCH_NULL;

            ITracker& currentTracker = ctx.currentTracker();
            if( ITracker* childTracker = currentTracker.findChild( name ) ) {
                section = dynamic_cast<SectionTracker*>( childTracker );
                assert( section );
            }
            else {
                section = new SectionTracker( name, ctx, &currentTracker );
                currentTracker.addChild( section );
            }
            if( !ctx.completedCycle() && !section->isComplete() ) {

                section->open();
            }
            return *section;
        }
    };

    class IndexTracker : public TrackerBase {
        int m_size;
        int m_index;
    public:
        IndexTracker( std::string const& name, TrackerContext& ctx, ITracker* parent, int size )
        :   TrackerBase( name, ctx, parent ),
            m_size( size ),
            m_index( -1 )
        {}
        virtual ~IndexTracker();

        static IndexTracker& acquire( TrackerContext& ctx, std::string const& name, int size ) {
            IndexTracker* tracker = CATCH_NULL;

            ITracker& currentTracker = ctx.currentTracker();
            if( ITracker* childTracker = currentTracker.findChild( name ) ) {
                tracker = dynamic_cast<IndexTracker*>( childTracker );
                assert( tracker );
            }
            else {
                tracker = new IndexTracker( name, ctx, &currentTracker, size );
                currentTracker.addChild( tracker );
            }

            if( !ctx.completedCycle() && !tracker->isComplete() ) {
                if( tracker->m_runState != ExecutingChildren && tracker->m_runState != NeedsAnotherRun )
                    tracker->moveNext();
                tracker->open();
            }

            return *tracker;
        }

        int index() const { return m_index; }

        void moveNext() {
            m_index++;
            m_children.clear();
        }

        virtual void close() CATCH_OVERRIDE {
            TrackerBase::close();
            if( m_runState == CompletedSuccessfully && m_index < m_size-1 )
                m_runState = Executing;
        }
    };

    inline ITracker& TrackerContext::startRun() {
        m_rootTracker = new SectionTracker( "{root}", *this, CATCH_NULL );
        m_currentTracker = CATCH_NULL;
        m_runState = Executing;
        return *m_rootTracker;
    }

}

using TestCaseTracking::ITracker;
using TestCaseTracking::TrackerContext;
using TestCaseTracking::SectionTracker;
using TestCaseTracking::IndexTracker;

}


#define TWOBLUECUBES_CATCH_FATAL_CONDITION_H_INCLUDED

namespace Catch {


    inline void fatal( std::string const& message, int exitCode ) {
        IContext& context = Catch::getCurrentContext();
        IResultCapture* resultCapture = context.getResultCapture();
        resultCapture->handleFatalErrorCondition( message );

		if( Catch::alwaysTrue() )
            exit( exitCode );
    }

}

#if defined ( CATCH_PLATFORM_WINDOWS )

namespace Catch {

    struct FatalConditionHandler {
		void reset() {}
	};

}

#else

#include <signal.h>

namespace Catch {

    struct SignalDefs { int id; const char* name; };
    extern SignalDefs signalDefs[];
    SignalDefs signalDefs[] = {
            { SIGINT,  "SIGINT - Terminal interrupt signal" },
            { SIGILL,  "SIGILL - Illegal instruction signal" },
            { SIGFPE,  "SIGFPE - Floating point error signal" },
            { SIGSEGV, "SIGSEGV - Segmentation violation signal" },
            { SIGTERM, "SIGTERM - Termination request signal" },
            { SIGABRT, "SIGABRT - Abort (abnormal termination) signal" }
        };

    struct FatalConditionHandler {

        static void handleSignal( int sig ) {
            for( std::size_t i = 0; i < sizeof(signalDefs)/sizeof(SignalDefs); ++i )
                if( sig == signalDefs[i].id )
                    fatal( signalDefs[i].name, -sig );
            fatal( "<unknown signal>", -sig );
        }

        FatalConditionHandler() : m_isSet( true ) {
            for( std::size_t i = 0; i < sizeof(signalDefs)/sizeof(SignalDefs); ++i )
                signal( signalDefs[i].id, handleSignal );
        }
        ~FatalConditionHandler() {
            reset();
        }
        void reset() {
            if( m_isSet ) {
                for( std::size_t i = 0; i < sizeof(signalDefs)/sizeof(SignalDefs); ++i )
                    signal( signalDefs[i].id, SIG_DFL );
                m_isSet = false;
            }
        }

        bool m_isSet;
    };

}

#endif

#include <set>
#include <string>

namespace Catch {

    class StreamRedirect {

    public:
        StreamRedirect( std::ostream& stream, std::string& targetString )
        :   m_stream( stream ),
            m_prevBuf( stream.rdbuf() ),
            m_targetString( targetString )
        {
            stream.rdbuf( m_oss.rdbuf() );
        }

        ~StreamRedirect() {
            m_targetString += m_oss.str();
            m_stream.rdbuf( m_prevBuf );
        }

    private:
        std::ostream& m_stream;
        std::streambuf* m_prevBuf;
        std::ostringstream m_oss;
        std::string& m_targetString;
    };



    class RunContext : public IResultCapture, public IRunner {

        RunContext( RunContext const& );
        void operator =( RunContext const& );

    public:

        explicit RunContext( Ptr<IConfig const> const& _config, Ptr<IStreamingReporter> const& reporter )
        :   m_runInfo( _config->name() ),
            m_context( getCurrentMutableContext() ),
            m_activeTestCase( CATCH_NULL ),
            m_config( _config ),
            m_reporter( reporter )
        {
            m_context.setRunner( this );
            m_context.setConfig( m_config );
            m_context.setResultCapture( this );
            m_reporter->testRunStarting( m_runInfo );
        }

        virtual ~RunContext() {
            m_reporter->testRunEnded( TestRunStats( m_runInfo, m_totals, aborting() ) );
        }

        void testGroupStarting( std::string const& testSpec, std::size_t groupIndex, std::size_t groupsCount ) {
            m_reporter->testGroupStarting( GroupInfo( testSpec, groupIndex, groupsCount ) );
        }
        void testGroupEnded( std::string const& testSpec, Totals const& totals, std::size_t groupIndex, std::size_t groupsCount ) {
            m_reporter->testGroupEnded( TestGroupStats( GroupInfo( testSpec, groupIndex, groupsCount ), totals, aborting() ) );
        }

        Totals runTest( TestCase const& testCase ) {
            Totals prevTotals = m_totals;

            std::string redirectedCout;
            std::string redirectedCerr;

            TestCaseInfo testInfo = testCase.getTestCaseInfo();

            m_reporter->testCaseStarting( testInfo );

            m_activeTestCase = &testCase;

            do {
                m_trackerContext.startRun();
                do {
                    m_trackerContext.startCycle();
                    m_testCaseTracker = &SectionTracker::acquire( m_trackerContext, testInfo.name );
                    runCurrentTest( redirectedCout, redirectedCerr );
                }
                while( !m_testCaseTracker->isSuccessfullyCompleted() && !aborting() );
            }

            while( getCurrentContext().advanceGeneratorsForCurrentTest() && !aborting() );

            Totals deltaTotals = m_totals.delta( prevTotals );
            m_totals.testCases += deltaTotals.testCases;
            m_reporter->testCaseEnded( TestCaseStats(   testInfo,
                                                        deltaTotals,
                                                        redirectedCout,
                                                        redirectedCerr,
                                                        aborting() ) );

            m_activeTestCase = CATCH_NULL;
            m_testCaseTracker = CATCH_NULL;

            return deltaTotals;
        }

        Ptr<IConfig const> config() const {
            return m_config;
        }

    private:

        virtual void assertionEnded( AssertionResult const& result ) {
            if( result.getResultType() == ResultWas::Ok ) {
                m_totals.assertions.passed++;
            }
            else if( !result.isOk() ) {
                m_totals.assertions.failed++;
            }

            if( m_reporter->assertionEnded( AssertionStats( result, m_messages, m_totals ) ) )
                m_messages.clear();


            m_lastAssertionInfo = AssertionInfo( "", m_lastAssertionInfo.lineInfo, "{Unknown expression after the reported line}" , m_lastAssertionInfo.resultDisposition );
            m_lastResult = result;
        }

        virtual bool sectionStarted (
            SectionInfo const& sectionInfo,
            Counts& assertions
        )
        {
            std::ostringstream oss;
            oss << sectionInfo.name << "@" << sectionInfo.lineInfo;

            ITracker& sectionTracker = SectionTracker::acquire( m_trackerContext, oss.str() );
            if( !sectionTracker.isOpen() )
                return false;
            m_activeSections.push_back( &sectionTracker );

            m_lastAssertionInfo.lineInfo = sectionInfo.lineInfo;

            m_reporter->sectionStarting( sectionInfo );

            assertions = m_totals.assertions;

            return true;
        }
        bool testForMissingAssertions( Counts& assertions ) {
            if( assertions.total() != 0 )
                return false;
            if( !m_config->warnAboutMissingAssertions() )
                return false;
            if( m_trackerContext.currentTracker().hasChildren() )
                return false;
            m_totals.assertions.failed++;
            assertions.failed++;
            return true;
        }

        virtual void sectionEnded( SectionEndInfo const& endInfo ) {
            Counts assertions = m_totals.assertions - endInfo.prevAssertions;
            bool missingAssertions = testForMissingAssertions( assertions );

            if( !m_activeSections.empty() ) {
                m_activeSections.back()->close();
                m_activeSections.pop_back();
            }

            m_reporter->sectionEnded( SectionStats( endInfo.sectionInfo, assertions, endInfo.durationInSeconds, missingAssertions ) );
            m_messages.clear();
        }

        virtual void sectionEndedEarly( SectionEndInfo const& endInfo ) {
            if( m_unfinishedSections.empty() )
                m_activeSections.back()->fail();
            else
                m_activeSections.back()->close();
            m_activeSections.pop_back();

            m_unfinishedSections.push_back( endInfo );
        }

        virtual void pushScopedMessage( MessageInfo const& message ) {
            m_messages.push_back( message );
        }

        virtual void popScopedMessage( MessageInfo const& message ) {
            m_messages.erase( std::remove( m_messages.begin(), m_messages.end(), message ), m_messages.end() );
        }

        virtual std::string getCurrentTestName() const {
            return m_activeTestCase
                ? m_activeTestCase->getTestCaseInfo().name
                : "";
        }

        virtual const AssertionResult* getLastResult() const {
            return &m_lastResult;
        }

        virtual void handleFatalErrorCondition( std::string const& message ) {
            ResultBuilder resultBuilder = makeUnexpectedResultBuilder();
            resultBuilder.setResultType( ResultWas::FatalErrorCondition );
            resultBuilder << message;
            resultBuilder.captureExpression();

            handleUnfinishedSections();


            TestCaseInfo const& testCaseInfo = m_activeTestCase->getTestCaseInfo();
            SectionInfo testCaseSection( testCaseInfo.lineInfo, testCaseInfo.name, testCaseInfo.description );

            Counts assertions;
            assertions.failed = 1;
            SectionStats testCaseSectionStats( testCaseSection, assertions, 0, false );
            m_reporter->sectionEnded( testCaseSectionStats );

            TestCaseInfo testInfo = m_activeTestCase->getTestCaseInfo();

            Totals deltaTotals;
            deltaTotals.testCases.failed = 1;
            m_reporter->testCaseEnded( TestCaseStats(   testInfo,
                                                        deltaTotals,
                                                        "",
                                                        "",
                                                        false ) );
            m_totals.testCases.failed++;
            testGroupEnded( "", m_totals, 1, 1 );
            m_reporter->testRunEnded( TestRunStats( m_runInfo, m_totals, false ) );
        }

    public:

        bool aborting() const {
            return m_totals.assertions.failed == static_cast<std::size_t>( m_config->abortAfter() );
        }

    private:

        void runCurrentTest( std::string& redirectedCout, std::string& redirectedCerr ) {
            TestCaseInfo const& testCaseInfo = m_activeTestCase->getTestCaseInfo();
            SectionInfo testCaseSection( testCaseInfo.lineInfo, testCaseInfo.name, testCaseInfo.description );
            m_reporter->sectionStarting( testCaseSection );
            Counts prevAssertions = m_totals.assertions;
            double duration = 0;
            try {
                m_lastAssertionInfo = AssertionInfo( "TEST_CASE", testCaseInfo.lineInfo, "", ResultDisposition::Normal );

                seedRng( *m_config );

                Timer timer;
                timer.start();
                if( m_reporter->getPreferences().shouldRedirectStdOut ) {
                    StreamRedirect coutRedir( Catch::cout(), redirectedCout );
                    StreamRedirect cerrRedir( Catch::cerr(), redirectedCerr );
                    invokeActiveTestCase();
                }
                else {
                    invokeActiveTestCase();
                }
                duration = timer.getElapsedSeconds();
            }
            catch( TestFailureException& ) {

            }
            catch(...) {
                makeUnexpectedResultBuilder().useActiveException();
            }
            m_testCaseTracker->close();
            handleUnfinishedSections();
            m_messages.clear();

            Counts assertions = m_totals.assertions - prevAssertions;
            bool missingAssertions = testForMissingAssertions( assertions );

            if( testCaseInfo.okToFail() ) {
                std::swap( assertions.failedButOk, assertions.failed );
                m_totals.assertions.failed -= assertions.failedButOk;
                m_totals.assertions.failedButOk += assertions.failedButOk;
            }

            SectionStats testCaseSectionStats( testCaseSection, assertions, duration, missingAssertions );
            m_reporter->sectionEnded( testCaseSectionStats );
        }

        void invokeActiveTestCase() {
            FatalConditionHandler fatalConditionHandler;
            m_activeTestCase->invoke();
            fatalConditionHandler.reset();
        }

    private:

        ResultBuilder makeUnexpectedResultBuilder() const {
            return ResultBuilder(   m_lastAssertionInfo.macroName.c_str(),
                                    m_lastAssertionInfo.lineInfo,
                                    m_lastAssertionInfo.capturedExpression.c_str(),
                                    m_lastAssertionInfo.resultDisposition );
        }

        void handleUnfinishedSections() {


            for( std::vector<SectionEndInfo>::const_reverse_iterator it = m_unfinishedSections.rbegin(),
                        itEnd = m_unfinishedSections.rend();
                    it != itEnd;
                    ++it )
                sectionEnded( *it );
            m_unfinishedSections.clear();
        }

        TestRunInfo m_runInfo;
        IMutableContext& m_context;
        TestCase const* m_activeTestCase;
        ITracker* m_testCaseTracker;
        ITracker* m_currentSectionTracker;
        AssertionResult m_lastResult;

        Ptr<IConfig const> m_config;
        Totals m_totals;
        Ptr<IStreamingReporter> m_reporter;
        std::vector<MessageInfo> m_messages;
        AssertionInfo m_lastAssertionInfo;
        std::vector<SectionEndInfo> m_unfinishedSections;
        std::vector<ITracker*> m_activeSections;
        TrackerContext m_trackerContext;
    };

    IResultCapture& getResultCapture() {
        if( IResultCapture* capture = getCurrentContext().getResultCapture() )
            return *capture;
        else
            throw std::logic_error( "No result capture instance" );
    }

}


#define TWOBLUECUBES_CATCH_VERSION_H_INCLUDED

namespace Catch {


    struct Version {
        Version(    unsigned int _majorVersion,
                    unsigned int _minorVersion,
                    unsigned int _patchNumber,
                    std::string const& _branchName,
                    unsigned int _buildNumber );

        unsigned int const majorVersion;
        unsigned int const minorVersion;
        unsigned int const patchNumber;


        std::string const branchName;
        unsigned int const buildNumber;

        friend std::ostream& operator << ( std::ostream& os, Version const& version );

    private:
        void operator=( Version const& );
    };

    extern Version libraryVersion;
}

#include <fstream>
#include <stdlib.h>
#include <limits>

namespace Catch {

    Ptr<IStreamingReporter> createReporter( std::string const& reporterName, Ptr<Config> const& config ) {
        Ptr<IStreamingReporter> reporter = getRegistryHub().getReporterRegistry().create( reporterName, config.get() );
        if( !reporter ) {
            std::ostringstream oss;
            oss << "No reporter registered with name: '" << reporterName << "'";
            throw std::domain_error( oss.str() );
        }
        return reporter;
    }

    Ptr<IStreamingReporter> makeReporter( Ptr<Config> const& config ) {
        std::vector<std::string> reporters = config->getReporterNames();
        if( reporters.empty() )
            reporters.push_back( "console" );

        Ptr<IStreamingReporter> reporter;
        for( std::vector<std::string>::const_iterator it = reporters.begin(), itEnd = reporters.end();
                it != itEnd;
                ++it )
            reporter = addReporter( reporter, createReporter( *it, config ) );
        return reporter;
    }
    Ptr<IStreamingReporter> addListeners( Ptr<IConfig const> const& config, Ptr<IStreamingReporter> reporters ) {
        IReporterRegistry::Listeners listeners = getRegistryHub().getReporterRegistry().getListeners();
        for( IReporterRegistry::Listeners::const_iterator it = listeners.begin(), itEnd = listeners.end();
                it != itEnd;
                ++it )
            reporters = addReporter(reporters, (*it)->create( ReporterConfig( config ) ) );
        return reporters;
    }

    Totals runTests( Ptr<Config> const& config ) {

        Ptr<IConfig const> iconfig = config.get();

        Ptr<IStreamingReporter> reporter = makeReporter( config );
        reporter = addListeners( iconfig, reporter );

        RunContext context( iconfig, reporter );

        Totals totals;

        context.testGroupStarting( config->name(), 1, 1 );

        TestSpec testSpec = config->testSpec();
        if( !testSpec.hasFilters() )
            testSpec = TestSpecParser( ITagAliasRegistry::get() ).parse( "~[.]" ).testSpec();

        std::vector<TestCase> const& allTestCases = getAllTestCasesSorted( *iconfig );
        for( std::vector<TestCase>::const_iterator it = allTestCases.begin(), itEnd = allTestCases.end();
                it != itEnd;
                ++it ) {
            if( !context.aborting() && matchTest( *it, testSpec, *iconfig ) )
                totals += context.runTest( *it );
            else
                reporter->skipTest( *it );
        }

        context.testGroupEnded( iconfig->name(), totals, 1, 1 );
        return totals;
    }

    void applyFilenamesAsTags( IConfig const& config ) {
        std::vector<TestCase> const& tests = getAllTestCasesSorted( config );
        for(std::size_t i = 0; i < tests.size(); ++i ) {
            TestCase& test = const_cast<TestCase&>( tests[i] );
            std::set<std::string> tags = test.tags;

            std::string filename = test.lineInfo.file;
            std::string::size_type lastSlash = filename.find_last_of( "\\/" );
            if( lastSlash != std::string::npos )
                filename = filename.substr( lastSlash+1 );

            std::string::size_type lastDot = filename.find_last_of( "." );
            if( lastDot != std::string::npos )
                filename = filename.substr( 0, lastDot );

            tags.insert( "#" + filename );
            setTags( test, tags );
        }
    }

    class Session : NonCopyable {
        static bool alreadyInstantiated;

    public:

        struct OnUnusedOptions { enum DoWhat { Ignore, Fail }; };

        Session()
        : m_cli( makeCommandLineParser() ) {
            if( alreadyInstantiated ) {
                std::string msg = "Only one instance of Catch::Session can ever be used";
                Catch::cerr() << msg << std::endl;
                throw std::logic_error( msg );
            }
            alreadyInstantiated = true;
        }
        ~Session() {
            Catch::cleanUp();
        }

        void showHelp( std::string const& processName ) {
            Catch::cout() << "\nCatch v" << libraryVersion << "\n";

            m_cli.usage( Catch::cout(), processName );
            Catch::cout() << "For more detail usage please see the project docs\n" << std::endl;
        }

        int applyCommandLine( int argc, char const* argv[], OnUnusedOptions::DoWhat unusedOptionBehaviour = OnUnusedOptions::Fail ) {
            try {
                m_cli.setThrowOnUnrecognisedTokens( unusedOptionBehaviour == OnUnusedOptions::Fail );
                m_unusedTokens = m_cli.parseInto( argc, argv, m_configData );
                if( m_configData.showHelp )
                    showHelp( m_configData.processName );
                m_config.reset();
            }
            catch( std::exception& ex ) {
                {
                    Colour colourGuard( Colour::Red );
                    Catch::cerr()
                        << "\nError(s) in input:\n"
                        << Text( ex.what(), TextAttributes().setIndent(2) )
                        << "\n\n";
                }
                m_cli.usage( Catch::cout(), m_configData.processName );
                return (std::numeric_limits<int>::max)();
            }
            return 0;
        }

        void useConfigData( ConfigData const& _configData ) {
            m_configData = _configData;
            m_config.reset();
        }

        int run( int argc, char const* argv[] ) {

            int returnCode = applyCommandLine( argc, argv );
            if( returnCode == 0 )
                returnCode = run();
            return returnCode;
        }
        int run( int argc, char* argv[] ) {
            return run( argc, const_cast<char const**>( argv ) );
        }

        int run() {
            if( m_configData.showHelp )
                return 0;

            try
            {
                config();

                seedRng( *m_config );

                if( m_configData.filenamesAsTags )
                    applyFilenamesAsTags( *m_config );


                if( Option<std::size_t> listed = list( config() ) )
                    return static_cast<int>( *listed );

                return static_cast<int>( runTests( m_config ).assertions.failed );
            }
            catch( std::exception& ex ) {
                Catch::cerr() << ex.what() << std::endl;
                return (std::numeric_limits<int>::max)();
            }
        }

        Clara::CommandLine<ConfigData> const& cli() const {
            return m_cli;
        }
        std::vector<Clara::Parser::Token> const& unusedTokens() const {
            return m_unusedTokens;
        }
        ConfigData& configData() {
            return m_configData;
        }
        Config& config() {
            if( !m_config )
                m_config = new Config( m_configData );
            return *m_config;
        }
    private:
        Clara::CommandLine<ConfigData> m_cli;
        std::vector<Clara::Parser::Token> m_unusedTokens;
        ConfigData m_configData;
        Ptr<Config> m_config;
    };

    bool Session::alreadyInstantiated = false;

}


#define TWOBLUECUBES_CATCH_REGISTRY_HUB_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_TEST_CASE_REGISTRY_IMPL_HPP_INCLUDED

#include <vector>
#include <set>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace Catch {

    struct LexSort {
        bool operator() (TestCase i,TestCase j) const { return (i<j);}
    };
    struct RandomNumberGenerator {
        int operator()( int n ) const { return std::rand() % n; }
    };

    inline std::vector<TestCase> sortTests( IConfig const& config, std::vector<TestCase> const& unsortedTestCases ) {

        std::vector<TestCase> sorted = unsortedTestCases;

        switch( config.runOrder() ) {
            case RunTests::InLexicographicalOrder:
                std::sort( sorted.begin(), sorted.end(), LexSort() );
                break;
            case RunTests::InRandomOrder:
                {
                    seedRng( config );

                    RandomNumberGenerator rng;
                    std::random_shuffle( sorted.begin(), sorted.end(), rng );
                }
                break;
            case RunTests::InDeclarationOrder:

                break;
        }
        return sorted;
    }
    bool matchTest( TestCase const& testCase, TestSpec const& testSpec, IConfig const& config ) {
        return testSpec.matches( testCase ) && ( config.allowThrows() || !testCase.throws() );
    }

    void enforceNoDuplicateTestCases( std::vector<TestCase> const& functions ) {
        std::set<TestCase> seenFunctions;
        for( std::vector<TestCase>::const_iterator it = functions.begin(), itEnd = functions.end();
            it != itEnd;
            ++it ) {
            std::pair<std::set<TestCase>::const_iterator, bool> prev = seenFunctions.insert( *it );
            if( !prev.second ){
                Catch::cerr()
                << Colour( Colour::Red )
                << "error: TEST_CASE( \"" << it->name << "\" ) already defined.\n"
                << "\tFirst seen at " << prev.first->getTestCaseInfo().lineInfo << "\n"
                << "\tRedefined at " << it->getTestCaseInfo().lineInfo << std::endl;
                exit(1);
            }
        }
    }

    std::vector<TestCase> filterTests( std::vector<TestCase> const& testCases, TestSpec const& testSpec, IConfig const& config ) {
        std::vector<TestCase> filtered;
        filtered.reserve( testCases.size() );
        for( std::vector<TestCase>::const_iterator it = testCases.begin(), itEnd = testCases.end();
                it != itEnd;
                ++it )
            if( matchTest( *it, testSpec, config ) )
                filtered.push_back( *it );
        return filtered;
    }
    std::vector<TestCase> const& getAllTestCasesSorted( IConfig const& config ) {
        return getRegistryHub().getTestCaseRegistry().getAllTestsSorted( config );
    }

    class TestRegistry : public ITestCaseRegistry {
    public:
        TestRegistry()
        :   m_currentSortOrder( RunTests::InDeclarationOrder ),
            m_unnamedCount( 0 )
        {}
        virtual ~TestRegistry();

        virtual void registerTest( TestCase const& testCase ) {
            std::string name = testCase.getTestCaseInfo().name;
            if( name == "" ) {
                std::ostringstream oss;
                oss << "Anonymous test case " << ++m_unnamedCount;
                return registerTest( testCase.withName( oss.str() ) );
            }
            m_functions.push_back( testCase );
        }

        virtual std::vector<TestCase> const& getAllTests() const {
            return m_functions;
        }
        virtual std::vector<TestCase> const& getAllTestsSorted( IConfig const& config ) const {
            if( m_sortedFunctions.empty() )
                enforceNoDuplicateTestCases( m_functions );

            if(  m_currentSortOrder != config.runOrder() || m_sortedFunctions.empty() ) {
                m_sortedFunctions = sortTests( config, m_functions );
                m_currentSortOrder = config.runOrder();
            }
            return m_sortedFunctions;
        }

    private:
        std::vector<TestCase> m_functions;
        mutable RunTests::InWhatOrder m_currentSortOrder;
        mutable std::vector<TestCase> m_sortedFunctions;
        size_t m_unnamedCount;
        std::ios_base::Init m_ostreamInit;
    };



    class FreeFunctionTestCase : public SharedImpl<ITestCase> {
    public:

        FreeFunctionTestCase( TestFunction fun ) : m_fun( fun ) {}

        virtual void invoke() const {
            m_fun();
        }

    private:
        virtual ~FreeFunctionTestCase();

        TestFunction m_fun;
    };

    inline std::string extractClassName( std::string const& classOrQualifiedMethodName ) {
        std::string className = classOrQualifiedMethodName;
        if( startsWith( className, "&" ) )
        {
            std::size_t lastColons = className.rfind( "::" );
            std::size_t penultimateColons = className.rfind( "::", lastColons-1 );
            if( penultimateColons == std::string::npos )
                penultimateColons = 1;
            className = className.substr( penultimateColons, lastColons-penultimateColons );
        }
        return className;
    }

    void registerTestCase
        (   ITestCase* testCase,
            char const* classOrQualifiedMethodName,
            NameAndDesc const& nameAndDesc,
            SourceLineInfo const& lineInfo ) {

        getMutableRegistryHub().registerTest
            ( makeTestCase
                (   testCase,
                    extractClassName( classOrQualifiedMethodName ),
                    nameAndDesc.name,
                    nameAndDesc.description,
                    lineInfo ) );
    }
    void registerTestCaseFunction
        (   TestFunction function,
            SourceLineInfo const& lineInfo,
            NameAndDesc const& nameAndDesc ) {
        registerTestCase( new FreeFunctionTestCase( function ), "", nameAndDesc, lineInfo );
    }



    AutoReg::AutoReg
        (   TestFunction function,
            SourceLineInfo const& lineInfo,
            NameAndDesc const& nameAndDesc ) {
        registerTestCaseFunction( function, lineInfo, nameAndDesc );
    }

    AutoReg::~AutoReg() {}

}


#define TWOBLUECUBES_CATCH_REPORTER_REGISTRY_HPP_INCLUDED

#include <map>

namespace Catch {

    class ReporterRegistry : public IReporterRegistry {

    public:

        virtual ~ReporterRegistry() CATCH_OVERRIDE {}

        virtual IStreamingReporter* create( std::string const& name, Ptr<IConfig const> const& config ) const CATCH_OVERRIDE {
            FactoryMap::const_iterator it =  m_factories.find( name );
            if( it == m_factories.end() )
                return CATCH_NULL;
            return it->second->create( ReporterConfig( config ) );
        }

        void registerReporter( std::string const& name, Ptr<IReporterFactory> const& factory ) {
            m_factories.insert( std::make_pair( name, factory ) );
        }
        void registerListener( Ptr<IReporterFactory> const& factory ) {
            m_listeners.push_back( factory );
        }

        virtual FactoryMap const& getFactories() const CATCH_OVERRIDE {
            return m_factories;
        }
        virtual Listeners const& getListeners() const CATCH_OVERRIDE {
            return m_listeners;
        }

    private:
        FactoryMap m_factories;
        Listeners m_listeners;
    };
}


#define TWOBLUECUBES_CATCH_EXCEPTION_TRANSLATOR_REGISTRY_HPP_INCLUDED

#ifdef __OBJC__
#import "Foundation/Foundation.h"
#endif

namespace Catch {

    class ExceptionTranslatorRegistry : public IExceptionTranslatorRegistry {
    public:
        ~ExceptionTranslatorRegistry() {
            deleteAll( m_translators );
        }

        virtual void registerTranslator( const IExceptionTranslator* translator ) {
            m_translators.push_back( translator );
        }

        virtual std::string translateActiveException() const {
            try {
#ifdef __OBJC__

                @try {
                    return tryTranslators();
                }
                @catch (NSException *exception) {
                    return Catch::toString( [exception description] );
                }
#else
                return tryTranslators();
#endif
            }
            catch( TestFailureException& ) {
                throw;
            }
            catch( std::exception& ex ) {
                return ex.what();
            }
            catch( std::string& msg ) {
                return msg;
            }
            catch( const char* msg ) {
                return msg;
            }
            catch(...) {
                return "Unknown exception";
            }
        }

        std::string tryTranslators() const {
            if( m_translators.empty() )
                throw;
            else
                return m_translators[0]->translate( m_translators.begin()+1, m_translators.end() );
        }

    private:
        std::vector<const IExceptionTranslator*> m_translators;
    };
}

namespace Catch {

    namespace {

        class RegistryHub : public IRegistryHub, public IMutableRegistryHub {

            RegistryHub( RegistryHub const& );
            void operator=( RegistryHub const& );

        public:
            RegistryHub() {
            }
            virtual IReporterRegistry const& getReporterRegistry() const CATCH_OVERRIDE {
                return m_reporterRegistry;
            }
            virtual ITestCaseRegistry const& getTestCaseRegistry() const CATCH_OVERRIDE {
                return m_testCaseRegistry;
            }
            virtual IExceptionTranslatorRegistry& getExceptionTranslatorRegistry() CATCH_OVERRIDE {
                return m_exceptionTranslatorRegistry;
            }

        public:
            virtual void registerReporter( std::string const& name, Ptr<IReporterFactory> const& factory ) CATCH_OVERRIDE {
                m_reporterRegistry.registerReporter( name, factory );
            }
            virtual void registerListener( Ptr<IReporterFactory> const& factory ) CATCH_OVERRIDE {
                m_reporterRegistry.registerListener( factory );
            }
            virtual void registerTest( TestCase const& testInfo ) CATCH_OVERRIDE {
                m_testCaseRegistry.registerTest( testInfo );
            }
            virtual void registerTranslator( const IExceptionTranslator* translator ) CATCH_OVERRIDE {
                m_exceptionTranslatorRegistry.registerTranslator( translator );
            }

        private:
            TestRegistry m_testCaseRegistry;
            ReporterRegistry m_reporterRegistry;
            ExceptionTranslatorRegistry m_exceptionTranslatorRegistry;
        };


        inline RegistryHub*& getTheRegistryHub() {
            static RegistryHub* theRegistryHub = CATCH_NULL;
            if( !theRegistryHub )
                theRegistryHub = new RegistryHub();
            return theRegistryHub;
        }
    }

    IRegistryHub& getRegistryHub() {
        return *getTheRegistryHub();
    }
    IMutableRegistryHub& getMutableRegistryHub() {
        return *getTheRegistryHub();
    }
    void cleanUp() {
        delete getTheRegistryHub();
        getTheRegistryHub() = CATCH_NULL;
        cleanUpContext();
    }
    std::string translateActiveException() {
        return getRegistryHub().getExceptionTranslatorRegistry().translateActiveException();
    }

}


#define TWOBLUECUBES_CATCH_NOTIMPLEMENTED_EXCEPTION_HPP_INCLUDED

#include <ostream>

namespace Catch {

    NotImplementedException::NotImplementedException( SourceLineInfo const& lineInfo )
    :   m_lineInfo( lineInfo ) {
        std::ostringstream oss;
        oss << lineInfo << ": function ";
        oss << "not implemented";
        m_what = oss.str();
    }

    const char* NotImplementedException::what() const CATCH_NOEXCEPT {
        return m_what.c_str();
    }

}


#define TWOBLUECUBES_CATCH_CONTEXT_IMPL_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_STREAM_HPP_INCLUDED

#include <stdexcept>
#include <cstdio>
#include <iostream>

namespace Catch {

    template<typename WriterF, size_t bufferSize=256>
    class StreamBufImpl : public StreamBufBase {
        char data[bufferSize];
        WriterF m_writer;

    public:
        StreamBufImpl() {
            setp( data, data + sizeof(data) );
        }

        ~StreamBufImpl() CATCH_NOEXCEPT {
            sync();
        }

    private:
        int overflow( int c ) {
            sync();

            if( c != EOF ) {
                if( pbase() == epptr() )
                    m_writer( std::string( 1, static_cast<char>( c ) ) );
                else
                    sputc( static_cast<char>( c ) );
            }
            return 0;
        }

        int sync() {
            if( pbase() != pptr() ) {
                m_writer( std::string( pbase(), static_cast<std::string::size_type>( pptr() - pbase() ) ) );
                setp( pbase(), epptr() );
            }
            return 0;
        }
    };



    FileStream::FileStream( std::string const& filename ) {
        m_ofs.open( filename.c_str() );
        if( m_ofs.fail() ) {
            std::ostringstream oss;
            oss << "Unable to open file: '" << filename << "'";
            throw std::domain_error( oss.str() );
        }
    }

    std::ostream& FileStream::stream() const {
        return m_ofs;
    }

    struct OutputDebugWriter {

        void operator()( std::string const&str ) {
            writeToDebugConsole( str );
        }
    };

    DebugOutStream::DebugOutStream()
    :   m_streamBuf( new StreamBufImpl<OutputDebugWriter>() ),
        m_os( m_streamBuf.get() )
    {}

    std::ostream& DebugOutStream::stream() const {
        return m_os;
    }



    CoutStream::CoutStream()
    :   m_os( Catch::cout().rdbuf() )
    {}

    std::ostream& CoutStream::stream() const {
        return m_os;
    }

#ifndef CATCH_CONFIG_NOSTDOUT
    std::ostream& cout() {
        return std::cout;
    }
    std::ostream& cerr() {
        return std::cerr;
    }
#endif
}

namespace Catch {

    class Context : public IMutableContext {

        Context() : m_config( CATCH_NULL ), m_runner( CATCH_NULL ), m_resultCapture( CATCH_NULL ) {}
        Context( Context const& );
        void operator=( Context const& );

    public:
        virtual IResultCapture* getResultCapture() {
            return m_resultCapture;
        }
        virtual IRunner* getRunner() {
            return m_runner;
        }
        virtual size_t getGeneratorIndex( std::string const& fileInfo, size_t totalSize ) {
            return getGeneratorsForCurrentTest()
            .getGeneratorInfo( fileInfo, totalSize )
            .getCurrentIndex();
        }
        virtual bool advanceGeneratorsForCurrentTest() {
            IGeneratorsForTest* generators = findGeneratorsForCurrentTest();
            return generators && generators->moveNext();
        }

        virtual Ptr<IConfig const> getConfig() const {
            return m_config;
        }

    public:
        virtual void setResultCapture( IResultCapture* resultCapture ) {
            m_resultCapture = resultCapture;
        }
        virtual void setRunner( IRunner* runner ) {
            m_runner = runner;
        }
        virtual void setConfig( Ptr<IConfig const> const& config ) {
            m_config = config;
        }

        friend IMutableContext& getCurrentMutableContext();

    private:
        IGeneratorsForTest* findGeneratorsForCurrentTest() {
            std::string testName = getResultCapture()->getCurrentTestName();

            std::map<std::string, IGeneratorsForTest*>::const_iterator it =
                m_generatorsByTestName.find( testName );
            return it != m_generatorsByTestName.end()
                ? it->second
                : CATCH_NULL;
        }

        IGeneratorsForTest& getGeneratorsForCurrentTest() {
            IGeneratorsForTest* generators = findGeneratorsForCurrentTest();
            if( !generators ) {
                std::string testName = getResultCapture()->getCurrentTestName();
                generators = createGeneratorsForTest();
                m_generatorsByTestName.insert( std::make_pair( testName, generators ) );
            }
            return *generators;
        }

    private:
        Ptr<IConfig const> m_config;
        IRunner* m_runner;
        IResultCapture* m_resultCapture;
        std::map<std::string, IGeneratorsForTest*> m_generatorsByTestName;
    };

    namespace {
        Context* currentContext = CATCH_NULL;
    }
    IMutableContext& getCurrentMutableContext() {
        if( !currentContext )
            currentContext = new Context();
        return *currentContext;
    }
    IContext& getCurrentContext() {
        return getCurrentMutableContext();
    }

    void cleanUpContext() {
        delete currentContext;
        currentContext = CATCH_NULL;
    }
}


#define TWOBLUECUBES_CATCH_CONSOLE_COLOUR_IMPL_HPP_INCLUDED

namespace Catch {
    namespace {

        struct IColourImpl {
            virtual ~IColourImpl() {}
            virtual void use( Colour::Code _colourCode ) = 0;
        };

        struct NoColourImpl : IColourImpl {
            void use( Colour::Code ) {}

            static IColourImpl* instance() {
                static NoColourImpl s_instance;
                return &s_instance;
            }
        };

    }
}

#if !defined( CATCH_CONFIG_COLOUR_NONE ) && !defined( CATCH_CONFIG_COLOUR_WINDOWS ) && !defined( CATCH_CONFIG_COLOUR_ANSI )
#   ifdef CATCH_PLATFORM_WINDOWS
#       define CATCH_CONFIG_COLOUR_WINDOWS
#   else
#       define CATCH_CONFIG_COLOUR_ANSI
#   endif
#endif

#if defined ( CATCH_CONFIG_COLOUR_WINDOWS )

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef __AFXDLL
#include <AfxWin.h>
#else
#include <windows.h>
#endif

namespace Catch {
namespace {

    class Win32ColourImpl : public IColourImpl {
    public:
        Win32ColourImpl() : stdoutHandle( GetStdHandle(STD_OUTPUT_HANDLE) )
        {
            CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
            GetConsoleScreenBufferInfo( stdoutHandle, &csbiInfo );
            originalForegroundAttributes = csbiInfo.wAttributes & ~( BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_BLUE | BACKGROUND_INTENSITY );
            originalBackgroundAttributes = csbiInfo.wAttributes & ~( FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY );
        }

        virtual void use( Colour::Code _colourCode ) {
            switch( _colourCode ) {
                case Colour::None:      return setTextAttribute( originalForegroundAttributes );
                case Colour::White:     return setTextAttribute( FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE );
                case Colour::Red:       return setTextAttribute( FOREGROUND_RED );
                case Colour::Green:     return setTextAttribute( FOREGROUND_GREEN );
                case Colour::Blue:      return setTextAttribute( FOREGROUND_BLUE );
                case Colour::Cyan:      return setTextAttribute( FOREGROUND_BLUE | FOREGROUND_GREEN );
                case Colour::Yellow:    return setTextAttribute( FOREGROUND_RED | FOREGROUND_GREEN );
                case Colour::Grey:      return setTextAttribute( 0 );

                case Colour::LightGrey:     return setTextAttribute( FOREGROUND_INTENSITY );
                case Colour::BrightRed:     return setTextAttribute( FOREGROUND_INTENSITY | FOREGROUND_RED );
                case Colour::BrightGreen:   return setTextAttribute( FOREGROUND_INTENSITY | FOREGROUND_GREEN );
                case Colour::BrightWhite:   return setTextAttribute( FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE );

                case Colour::Bright: throw std::logic_error( "not a colour" );
            }
        }

    private:
        void setTextAttribute( WORD _textAttribute ) {
            SetConsoleTextAttribute( stdoutHandle, _textAttribute | originalBackgroundAttributes );
        }
        HANDLE stdoutHandle;
        WORD originalForegroundAttributes;
        WORD originalBackgroundAttributes;
    };

    IColourImpl* platformColourInstance() {
        static Win32ColourImpl s_instance;
        return &s_instance;
    }

}
}

#elif defined( CATCH_CONFIG_COLOUR_ANSI )

#include <unistd.h>

namespace Catch {
namespace {





    class PosixColourImpl : public IColourImpl {
    public:
        virtual void use( Colour::Code _colourCode ) {
            switch( _colourCode ) {
                case Colour::None:
                case Colour::White:     return setColour( "[0m" );
                case Colour::Red:       return setColour( "[0;31m" );
                case Colour::Green:     return setColour( "[0;32m" );
                case Colour::Blue:      return setColour( "[0:34m" );
                case Colour::Cyan:      return setColour( "[0;36m" );
                case Colour::Yellow:    return setColour( "[0;33m" );
                case Colour::Grey:      return setColour( "[1;30m" );

                case Colour::LightGrey:     return setColour( "[0;37m" );
                case Colour::BrightRed:     return setColour( "[1;31m" );
                case Colour::BrightGreen:   return setColour( "[1;32m" );
                case Colour::BrightWhite:   return setColour( "[1;37m" );

                case Colour::Bright: throw std::logic_error( "not a colour" );
            }
        }
        static IColourImpl* instance() {
            static PosixColourImpl s_instance;
            return &s_instance;
        }

    private:
        void setColour( const char* _escapeCode ) {
            Catch::cout() << '\033' << _escapeCode;
        }
    };

    IColourImpl* platformColourInstance() {
        Ptr<IConfig const> config = getCurrentContext().getConfig();
        return (config && config->forceColour()) || isatty(STDOUT_FILENO)
            ? PosixColourImpl::instance()
            : NoColourImpl::instance();
    }

}
}

#else

namespace Catch {

    static IColourImpl* platformColourInstance() { return NoColourImpl::instance(); }

}

#endif

namespace Catch {

    Colour::Colour( Code _colourCode ) : m_moved( false ) { use( _colourCode ); }
    Colour::Colour( Colour const& _other ) : m_moved( false ) { const_cast<Colour&>( _other ).m_moved = true; }
    Colour::~Colour(){ if( !m_moved ) use( None ); }

    void Colour::use( Code _colourCode ) {
        static IColourImpl* impl = isDebuggerActive()
            ? NoColourImpl::instance()
            : platformColourInstance();
        impl->use( _colourCode );
    }

}


#define TWOBLUECUBES_CATCH_GENERATORS_IMPL_HPP_INCLUDED

#include <vector>
#include <string>
#include <map>

namespace Catch {

    struct GeneratorInfo : IGeneratorInfo {

        GeneratorInfo( std::size_t size )
        :   m_size( size ),
            m_currentIndex( 0 )
        {}

        bool moveNext() {
            if( ++m_currentIndex == m_size ) {
                m_currentIndex = 0;
                return false;
            }
            return true;
        }

        std::size_t getCurrentIndex() const {
            return m_currentIndex;
        }

        std::size_t m_size;
        std::size_t m_currentIndex;
    };



    class GeneratorsForTest : public IGeneratorsForTest {

    public:
        ~GeneratorsForTest() {
            deleteAll( m_generatorsInOrder );
        }

        IGeneratorInfo& getGeneratorInfo( std::string const& fileInfo, std::size_t size ) {
            std::map<std::string, IGeneratorInfo*>::const_iterator it = m_generatorsByName.find( fileInfo );
            if( it == m_generatorsByName.end() ) {
                IGeneratorInfo* info = new GeneratorInfo( size );
                m_generatorsByName.insert( std::make_pair( fileInfo, info ) );
                m_generatorsInOrder.push_back( info );
                return *info;
            }
            return *it->second;
        }

        bool moveNext() {
            std::vector<IGeneratorInfo*>::const_iterator it = m_generatorsInOrder.begin();
            std::vector<IGeneratorInfo*>::const_iterator itEnd = m_generatorsInOrder.end();
            for(; it != itEnd; ++it ) {
                if( (*it)->moveNext() )
                    return true;
            }
            return false;
        }

    private:
        std::map<std::string, IGeneratorInfo*> m_generatorsByName;
        std::vector<IGeneratorInfo*> m_generatorsInOrder;
    };

    IGeneratorsForTest* createGeneratorsForTest()
    {
        return new GeneratorsForTest();
    }

}


#define TWOBLUECUBES_CATCH_ASSERTIONRESULT_HPP_INCLUDED

namespace Catch {

    AssertionInfo::AssertionInfo(   std::string const& _macroName,
                                    SourceLineInfo const& _lineInfo,
                                    std::string const& _capturedExpression,
                                    ResultDisposition::Flags _resultDisposition )
    :   macroName( _macroName ),
        lineInfo( _lineInfo ),
        capturedExpression( _capturedExpression ),
        resultDisposition( _resultDisposition )
    {}

    AssertionResult::AssertionResult() {}

    AssertionResult::AssertionResult( AssertionInfo const& info, AssertionResultData const& data )
    :   m_info( info ),
        m_resultData( data )
    {}

    AssertionResult::~AssertionResult() {}


    bool AssertionResult::succeeded() const {
        return Catch::isOk( m_resultData.resultType );
    }


    bool AssertionResult::isOk() const {
        return Catch::isOk( m_resultData.resultType ) || shouldSuppressFailure( m_info.resultDisposition );
    }

    ResultWas::OfType AssertionResult::getResultType() const {
        return m_resultData.resultType;
    }

    bool AssertionResult::hasExpression() const {
        return !m_info.capturedExpression.empty();
    }

    bool AssertionResult::hasMessage() const {
        return !m_resultData.message.empty();
    }

    std::string AssertionResult::getExpression() const {
        if( isFalseTest( m_info.resultDisposition ) )
            return "!" + m_info.capturedExpression;
        else
            return m_info.capturedExpression;
    }
    std::string AssertionResult::getExpressionInMacro() const {
        if( m_info.macroName.empty() )
            return m_info.capturedExpression;
        else
            return m_info.macroName + "( " + m_info.capturedExpression + " )";
    }

    bool AssertionResult::hasExpandedExpression() const {
        return hasExpression() && getExpandedExpression() != getExpression();
    }

    std::string AssertionResult::getExpandedExpression() const {
        return m_resultData.reconstructedExpression;
    }

    std::string AssertionResult::getMessage() const {
        return m_resultData.message;
    }
    SourceLineInfo AssertionResult::getSourceInfo() const {
        return m_info.lineInfo;
    }

    std::string AssertionResult::getTestMacroName() const {
        return m_info.macroName;
    }

}


#define TWOBLUECUBES_CATCH_TEST_CASE_INFO_HPP_INCLUDED

namespace Catch {

    inline TestCaseInfo::SpecialProperties parseSpecialTag( std::string const& tag ) {
        if( startsWith( tag, "." ) ||
            tag == "hide" ||
            tag == "!hide" )
            return TestCaseInfo::IsHidden;
        else if( tag == "!throws" )
            return TestCaseInfo::Throws;
        else if( tag == "!shouldfail" )
            return TestCaseInfo::ShouldFail;
        else if( tag == "!mayfail" )
            return TestCaseInfo::MayFail;
        else
            return TestCaseInfo::None;
    }
    inline bool isReservedTag( std::string const& tag ) {
        return parseSpecialTag( tag ) == TestCaseInfo::None && tag.size() > 0 && !isalnum( tag[0] );
    }
    inline void enforceNotReservedTag( std::string const& tag, SourceLineInfo const& _lineInfo ) {
        if( isReservedTag( tag ) ) {
            {
                Colour colourGuard( Colour::Red );
                Catch::cerr()
                    << "Tag name [" << tag << "] not allowed.\n"
                    << "Tag names starting with non alpha-numeric characters are reserved\n";
            }
            {
                Colour colourGuard( Colour::FileName );
                Catch::cerr() << _lineInfo << std::endl;
            }
            exit(1);
        }
    }

    TestCase makeTestCase(  ITestCase* _testCase,
                            std::string const& _className,
                            std::string const& _name,
                            std::string const& _descOrTags,
                            SourceLineInfo const& _lineInfo )
    {
        bool isHidden( startsWith( _name, "./" ) );


        std::set<std::string> tags;
        std::string desc, tag;
        bool inTag = false;
        for( std::size_t i = 0; i < _descOrTags.size(); ++i ) {
            char c = _descOrTags[i];
            if( !inTag ) {
                if( c == '[' )
                    inTag = true;
                else
                    desc += c;
            }
            else {
                if( c == ']' ) {
                    TestCaseInfo::SpecialProperties prop = parseSpecialTag( tag );
                    if( prop == TestCaseInfo::IsHidden )
                        isHidden = true;
                    else if( prop == TestCaseInfo::None )
                        enforceNotReservedTag( tag, _lineInfo );

                    tags.insert( tag );
                    tag.clear();
                    inTag = false;
                }
                else
                    tag += c;
            }
        }
        if( isHidden ) {
            tags.insert( "hide" );
            tags.insert( "." );
        }

        TestCaseInfo info( _name, _className, desc, tags, _lineInfo );
        return TestCase( _testCase, info );
    }

    void setTags( TestCaseInfo& testCaseInfo, std::set<std::string> const& tags )
    {
        testCaseInfo.tags = tags;
        testCaseInfo.lcaseTags.clear();

        std::ostringstream oss;
        for( std::set<std::string>::const_iterator it = tags.begin(), itEnd = tags.end(); it != itEnd; ++it ) {
            oss << "[" << *it << "]";
            std::string lcaseTag = toLower( *it );
            testCaseInfo.properties = static_cast<TestCaseInfo::SpecialProperties>( testCaseInfo.properties | parseSpecialTag( lcaseTag ) );
            testCaseInfo.lcaseTags.insert( lcaseTag );
        }
        testCaseInfo.tagsAsString = oss.str();
    }

    TestCaseInfo::TestCaseInfo( std::string const& _name,
                                std::string const& _className,
                                std::string const& _description,
                                std::set<std::string> const& _tags,
                                SourceLineInfo const& _lineInfo )
    :   name( _name ),
        className( _className ),
        description( _description ),
        lineInfo( _lineInfo ),
        properties( None )
    {
        setTags( *this, _tags );
    }

    TestCaseInfo::TestCaseInfo( TestCaseInfo const& other )
    :   name( other.name ),
        className( other.className ),
        description( other.description ),
        tags( other.tags ),
        lcaseTags( other.lcaseTags ),
        tagsAsString( other.tagsAsString ),
        lineInfo( other.lineInfo ),
        properties( other.properties )
    {}

    bool TestCaseInfo::isHidden() const {
        return ( properties & IsHidden ) != 0;
    }
    bool TestCaseInfo::throws() const {
        return ( properties & Throws ) != 0;
    }
    bool TestCaseInfo::okToFail() const {
        return ( properties & (ShouldFail | MayFail ) ) != 0;
    }
    bool TestCaseInfo::expectedToFail() const {
        return ( properties & (ShouldFail ) ) != 0;
    }

    TestCase::TestCase( ITestCase* testCase, TestCaseInfo const& info ) : TestCaseInfo( info ), test( testCase ) {}

    TestCase::TestCase( TestCase const& other )
    :   TestCaseInfo( other ),
        test( other.test )
    {}

    TestCase TestCase::withName( std::string const& _newName ) const {
        TestCase other( *this );
        other.name = _newName;
        return other;
    }

    void TestCase::swap( TestCase& other ) {
        test.swap( other.test );
        name.swap( other.name );
        className.swap( other.className );
        description.swap( other.description );
        tags.swap( other.tags );
        lcaseTags.swap( other.lcaseTags );
        tagsAsString.swap( other.tagsAsString );
        std::swap( TestCaseInfo::properties, static_cast<TestCaseInfo&>( other ).properties );
        std::swap( lineInfo, other.lineInfo );
    }

    void TestCase::invoke() const {
        test->invoke();
    }

    bool TestCase::operator == ( TestCase const& other ) const {
        return  test.get() == other.test.get() &&
                name == other.name &&
                className == other.className;
    }

    bool TestCase::operator < ( TestCase const& other ) const {
        return name < other.name;
    }
    TestCase& TestCase::operator = ( TestCase const& other ) {
        TestCase temp( other );
        swap( temp );
        return *this;
    }

    TestCaseInfo const& TestCase::getTestCaseInfo() const
    {
        return *this;
    }

}


#define TWOBLUECUBES_CATCH_VERSION_HPP_INCLUDED

namespace Catch {

    Version::Version
        (   unsigned int _majorVersion,
            unsigned int _minorVersion,
            unsigned int _patchNumber,
            std::string const& _branchName,
            unsigned int _buildNumber )
    :   majorVersion( _majorVersion ),
        minorVersion( _minorVersion ),
        patchNumber( _patchNumber ),
        branchName( _branchName ),
        buildNumber( _buildNumber )
    {}

    std::ostream& operator << ( std::ostream& os, Version const& version ) {
        os  << version.majorVersion << "."
            << version.minorVersion << "."
            << version.patchNumber;

        if( !version.branchName.empty() ) {
            os  << "-" << version.branchName
                << "." << version.buildNumber;
        }
        return os;
    }

    Version libraryVersion( 1, 3, 4, "", 0 );

}


#define TWOBLUECUBES_CATCH_MESSAGE_HPP_INCLUDED

namespace Catch {

    MessageInfo::MessageInfo(   std::string const& _macroName,
                                SourceLineInfo const& _lineInfo,
                                ResultWas::OfType _type )
    :   macroName( _macroName ),
        lineInfo( _lineInfo ),
        type( _type ),
        sequence( ++globalCount )
    {}


    unsigned int MessageInfo::globalCount = 0;



    ScopedMessage::ScopedMessage( MessageBuilder const& builder )
    : m_info( builder.m_info )
    {
        m_info.message = builder.m_stream.str();
        getResultCapture().pushScopedMessage( m_info );
    }
    ScopedMessage::ScopedMessage( ScopedMessage const& other )
    : m_info( other.m_info )
    {}

    ScopedMessage::~ScopedMessage() {
        getResultCapture().popScopedMessage( m_info );
    }

}


#define TWOBLUECUBES_CATCH_LEGACY_REPORTER_ADAPTER_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_LEGACY_REPORTER_ADAPTER_H_INCLUDED

namespace Catch
{

    struct IReporter : IShared {
        virtual ~IReporter();

        virtual bool shouldRedirectStdout() const = 0;

        virtual void StartTesting() = 0;
        virtual void EndTesting( Totals const& totals ) = 0;
        virtual void StartGroup( std::string const& groupName ) = 0;
        virtual void EndGroup( std::string const& groupName, Totals const& totals ) = 0;
        virtual void StartTestCase( TestCaseInfo const& testInfo ) = 0;
        virtual void EndTestCase( TestCaseInfo const& testInfo, Totals const& totals, std::string const& stdOut, std::string const& stdErr ) = 0;
        virtual void StartSection( std::string const& sectionName, std::string const& description ) = 0;
        virtual void EndSection( std::string const& sectionName, Counts const& assertions ) = 0;
        virtual void NoAssertionsInSection( std::string const& sectionName ) = 0;
        virtual void NoAssertionsInTestCase( std::string const& testName ) = 0;
        virtual void Aborted() = 0;
        virtual void Result( AssertionResult const& result ) = 0;
    };

    class LegacyReporterAdapter : public SharedImpl<IStreamingReporter>
    {
    public:
        LegacyReporterAdapter( Ptr<IReporter> const& legacyReporter );
        virtual ~LegacyReporterAdapter();

        virtual ReporterPreferences getPreferences() const;
        virtual void noMatchingTestCases( std::string const& );
        virtual void testRunStarting( TestRunInfo const& );
        virtual void testGroupStarting( GroupInfo const& groupInfo );
        virtual void testCaseStarting( TestCaseInfo const& testInfo );
        virtual void sectionStarting( SectionInfo const& sectionInfo );
        virtual void assertionStarting( AssertionInfo const& );
        virtual bool assertionEnded( AssertionStats const& assertionStats );
        virtual void sectionEnded( SectionStats const& sectionStats );
        virtual void testCaseEnded( TestCaseStats const& testCaseStats );
        virtual void testGroupEnded( TestGroupStats const& testGroupStats );
        virtual void testRunEnded( TestRunStats const& testRunStats );
        virtual void skipTest( TestCaseInfo const& );

    private:
        Ptr<IReporter> m_legacyReporter;
    };
}

namespace Catch
{
    LegacyReporterAdapter::LegacyReporterAdapter( Ptr<IReporter> const& legacyReporter )
    :   m_legacyReporter( legacyReporter )
    {}
    LegacyReporterAdapter::~LegacyReporterAdapter() {}

    ReporterPreferences LegacyReporterAdapter::getPreferences() const {
        ReporterPreferences prefs;
        prefs.shouldRedirectStdOut = m_legacyReporter->shouldRedirectStdout();
        return prefs;
    }

    void LegacyReporterAdapter::noMatchingTestCases( std::string const& ) {}
    void LegacyReporterAdapter::testRunStarting( TestRunInfo const& ) {
        m_legacyReporter->StartTesting();
    }
    void LegacyReporterAdapter::testGroupStarting( GroupInfo const& groupInfo ) {
        m_legacyReporter->StartGroup( groupInfo.name );
    }
    void LegacyReporterAdapter::testCaseStarting( TestCaseInfo const& testInfo ) {
        m_legacyReporter->StartTestCase( testInfo );
    }
    void LegacyReporterAdapter::sectionStarting( SectionInfo const& sectionInfo ) {
        m_legacyReporter->StartSection( sectionInfo.name, sectionInfo.description );
    }
    void LegacyReporterAdapter::assertionStarting( AssertionInfo const& ) {

    }

    bool LegacyReporterAdapter::assertionEnded( AssertionStats const& assertionStats ) {
        if( assertionStats.assertionResult.getResultType() != ResultWas::Ok ) {
            for( std::vector<MessageInfo>::const_iterator it = assertionStats.infoMessages.begin(), itEnd = assertionStats.infoMessages.end();
                    it != itEnd;
                    ++it ) {
                if( it->type == ResultWas::Info ) {
                    ResultBuilder rb( it->macroName.c_str(), it->lineInfo, "", ResultDisposition::Normal );
                    rb << it->message;
                    rb.setResultType( ResultWas::Info );
                    AssertionResult result = rb.build();
                    m_legacyReporter->Result( result );
                }
            }
        }
        m_legacyReporter->Result( assertionStats.assertionResult );
        return true;
    }
    void LegacyReporterAdapter::sectionEnded( SectionStats const& sectionStats ) {
        if( sectionStats.missingAssertions )
            m_legacyReporter->NoAssertionsInSection( sectionStats.sectionInfo.name );
        m_legacyReporter->EndSection( sectionStats.sectionInfo.name, sectionStats.assertions );
    }
    void LegacyReporterAdapter::testCaseEnded( TestCaseStats const& testCaseStats ) {
        m_legacyReporter->EndTestCase
            (   testCaseStats.testInfo,
                testCaseStats.totals,
                testCaseStats.stdOut,
                testCaseStats.stdErr );
    }
    void LegacyReporterAdapter::testGroupEnded( TestGroupStats const& testGroupStats ) {
        if( testGroupStats.aborting )
            m_legacyReporter->Aborted();
        m_legacyReporter->EndGroup( testGroupStats.groupInfo.name, testGroupStats.totals );
    }
    void LegacyReporterAdapter::testRunEnded( TestRunStats const& testRunStats ) {
        m_legacyReporter->EndTesting( testRunStats.totals );
    }
    void LegacyReporterAdapter::skipTest( TestCaseInfo const& ) {
    }
}



#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++11-long-long"
#endif

#ifdef CATCH_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace Catch {

    namespace {
#ifdef CATCH_PLATFORM_WINDOWS
        uint64_t getCurrentTicks() {
            static uint64_t hz=0, hzo=0;
            if (!hz) {
                QueryPerformanceFrequency( reinterpret_cast<LARGE_INTEGER*>( &hz ) );
                QueryPerformanceCounter( reinterpret_cast<LARGE_INTEGER*>( &hzo ) );
            }
            uint64_t t;
            QueryPerformanceCounter( reinterpret_cast<LARGE_INTEGER*>( &t ) );
            return ((t-hzo)*1000000)/hz;
        }
#else
        uint64_t getCurrentTicks() {
            timeval t;
            gettimeofday(&t,CATCH_NULL);
            return static_cast<uint64_t>( t.tv_sec ) * 1000000ull + static_cast<uint64_t>( t.tv_usec );
        }
#endif
    }

    void Timer::start() {
        m_ticks = getCurrentTicks();
    }
    unsigned int Timer::getElapsedMicroseconds() const {
        return static_cast<unsigned int>(getCurrentTicks() - m_ticks);
    }
    unsigned int Timer::getElapsedMilliseconds() const {
        return static_cast<unsigned int>(getElapsedMicroseconds()/1000);
    }
    double Timer::getElapsedSeconds() const {
        return getElapsedMicroseconds()/1000000.0;
    }

}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#define TWOBLUECUBES_CATCH_COMMON_HPP_INCLUDED

namespace Catch {

    bool startsWith( std::string const& s, std::string const& prefix ) {
        return s.size() >= prefix.size() && s.substr( 0, prefix.size() ) == prefix;
    }
    bool endsWith( std::string const& s, std::string const& suffix ) {
        return s.size() >= suffix.size() && s.substr( s.size()-suffix.size(), suffix.size() ) == suffix;
    }
    bool contains( std::string const& s, std::string const& infix ) {
        return s.find( infix ) != std::string::npos;
    }
    void toLowerInPlace( std::string& s ) {
        std::transform( s.begin(), s.end(), s.begin(), ::tolower );
    }
    std::string toLower( std::string const& s ) {
        std::string lc = s;
        toLowerInPlace( lc );
        return lc;
    }
    std::string trim( std::string const& str ) {
        static char const* whitespaceChars = "\n\r\t ";
        std::string::size_type start = str.find_first_not_of( whitespaceChars );
        std::string::size_type end = str.find_last_not_of( whitespaceChars );

        return start != std::string::npos ? str.substr( start, 1+end-start ) : "";
    }

    bool replaceInPlace( std::string& str, std::string const& replaceThis, std::string const& withThis ) {
        bool replaced = false;
        std::size_t i = str.find( replaceThis );
        while( i != std::string::npos ) {
            replaced = true;
            str = str.substr( 0, i ) + withThis + str.substr( i+replaceThis.size() );
            if( i < str.size()-withThis.size() )
                i = str.find( replaceThis, i+withThis.size() );
            else
                i = std::string::npos;
        }
        return replaced;
    }

    pluralise::pluralise( std::size_t count, std::string const& label )
    :   m_count( count ),
        m_label( label )
    {}

    std::ostream& operator << ( std::ostream& os, pluralise const& pluraliser ) {
        os << pluraliser.m_count << " " << pluraliser.m_label;
        if( pluraliser.m_count != 1 )
            os << "s";
        return os;
    }

    SourceLineInfo::SourceLineInfo() : line( 0 ){}
    SourceLineInfo::SourceLineInfo( char const* _file, std::size_t _line )
    :   file( _file ),
        line( _line )
    {}
    SourceLineInfo::SourceLineInfo( SourceLineInfo const& other )
    :   file( other.file ),
        line( other.line )
    {}
    bool SourceLineInfo::empty() const {
        return file.empty();
    }
    bool SourceLineInfo::operator == ( SourceLineInfo const& other ) const {
        return line == other.line && file == other.file;
    }
    bool SourceLineInfo::operator < ( SourceLineInfo const& other ) const {
        return line < other.line || ( line == other.line  && file < other.file );
    }

    void seedRng( IConfig const& config ) {
        if( config.rngSeed() != 0 )
            std::srand( config.rngSeed() );
    }
    unsigned int rngSeed() {
        return getCurrentContext().getConfig()->rngSeed();
    }

    std::ostream& operator << ( std::ostream& os, SourceLineInfo const& info ) {
#ifndef __GNUG__
        os << info.file << "(" << info.line << ")";
#else
        os << info.file << ":" << info.line;
#endif
        return os;
    }

    void throwLogicError( std::string const& message, SourceLineInfo const& locationInfo ) {
        std::ostringstream oss;
        oss << locationInfo << ": Internal Catch error: '" << message << "'";
        if( alwaysTrue() )
            throw std::logic_error( oss.str() );
    }
}


#define TWOBLUECUBES_CATCH_SECTION_HPP_INCLUDED

namespace Catch {

    SectionInfo::SectionInfo
        (   SourceLineInfo const& _lineInfo,
            std::string const& _name,
            std::string const& _description )
    :   name( _name ),
        description( _description ),
        lineInfo( _lineInfo )
    {}

    Section::Section( SectionInfo const& info )
    :   m_info( info ),
        m_sectionIncluded( getResultCapture().sectionStarted( m_info, m_assertions ) )
    {
        m_timer.start();
    }

    Section::~Section() {
        if( m_sectionIncluded ) {
            SectionEndInfo endInfo( m_info, m_assertions, m_timer.getElapsedSeconds() );
            if( std::uncaught_exception() )
                getResultCapture().sectionEndedEarly( endInfo );
            else
                getResultCapture().sectionEnded( endInfo );
        }
    }


    Section::operator bool() const {
        return m_sectionIncluded;
    }

}


#define TWOBLUECUBES_CATCH_DEBUGGER_HPP_INCLUDED

#include <iostream>

#ifdef CATCH_PLATFORM_MAC

    #include <assert.h>
    #include <stdbool.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <sys/sysctl.h>

    namespace Catch{






        bool isDebuggerActive(){

            int                 mib[4];
            struct kinfo_proc   info;
            size_t              size;




            info.kp_proc.p_flag = 0;




            mib[0] = CTL_KERN;
            mib[1] = KERN_PROC;
            mib[2] = KERN_PROC_PID;
            mib[3] = getpid();



            size = sizeof(info);
            if( sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, CATCH_NULL, 0) != 0 ) {
                Catch::cerr() << "\n** Call to sysctl failed - unable to determine if debugger is active **\n" << std::endl;
                return false;
            }



            return ( (info.kp_proc.p_flag & P_TRACED) != 0 );
        }
    }

#elif defined(_MSC_VER)
    extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
    namespace Catch {
        bool isDebuggerActive() {
            return IsDebuggerPresent() != 0;
        }
    }
#elif defined(__MINGW32__)
    extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
    namespace Catch {
        bool isDebuggerActive() {
            return IsDebuggerPresent() != 0;
        }
    }
#else
    namespace Catch {
       inline bool isDebuggerActive() { return false; }
    }
#endif

#ifdef CATCH_PLATFORM_WINDOWS
    extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA( const char* );
    namespace Catch {
        void writeToDebugConsole( std::string const& text ) {
            ::OutputDebugStringA( text.c_str() );
        }
    }
#else
    namespace Catch {
        void writeToDebugConsole( std::string const& text ) {

            Catch::cout() << text;
        }
    }
#endif


#define TWOBLUECUBES_CATCH_TOSTRING_HPP_INCLUDED

namespace Catch {

namespace Detail {

    const std::string unprintableString = "{?}";

    namespace {
        const int hexThreshold = 255;

        struct Endianness {
            enum Arch { Big, Little };

            static Arch which() {
                union _{
                    int asInt;
                    char asChar[sizeof (int)];
                } u;

                u.asInt = 1;
                return ( u.asChar[sizeof(int)-1] == 1 ) ? Big : Little;
            }
        };
    }

    std::string rawMemoryToString( const void *object, std::size_t size )
    {

        int i = 0, end = static_cast<int>( size ), inc = 1;
        if( Endianness::which() == Endianness::Little ) {
            i = end-1;
            end = inc = -1;
        }

        unsigned char const *bytes = static_cast<unsigned char const *>(object);
        std::ostringstream os;
        os << "0x" << std::setfill('0') << std::hex;
        for( ; i != end; i += inc )
             os << std::setw(2) << static_cast<unsigned>(bytes[i]);
       return os.str();
    }
}

std::string toString( std::string const& value ) {
    std::string s = value;
    if( getCurrentContext().getConfig()->showInvisibles() ) {
        for(size_t i = 0; i < s.size(); ++i ) {
            std::string subs;
            switch( s[i] ) {
            case '\n': subs = "\\n"; break;
            case '\t': subs = "\\t"; break;
            default: break;
            }
            if( !subs.empty() ) {
                s = s.substr( 0, i ) + subs + s.substr( i+1 );
                ++i;
            }
        }
    }
    return "\"" + s + "\"";
}
std::string toString( std::wstring const& value ) {

    std::string s;
    s.reserve( value.size() );
    for(size_t i = 0; i < value.size(); ++i )
        s += value[i] <= 0xff ? static_cast<char>( value[i] ) : '?';
    return Catch::toString( s );
}

std::string toString( const char* const value ) {
    return value ? Catch::toString( std::string( value ) ) : std::string( "{null string}" );
}

std::string toString( char* const value ) {
    return Catch::toString( static_cast<const char*>( value ) );
}

std::string toString( const wchar_t* const value )
{
	return value ? Catch::toString( std::wstring(value) ) : std::string( "{null string}" );
}

std::string toString( wchar_t* const value )
{
	return Catch::toString( static_cast<const wchar_t*>( value ) );
}

std::string toString( int value ) {
    std::ostringstream oss;
    oss << value;
    if( value > Detail::hexThreshold )
        oss << " (0x" << std::hex << value << ")";
    return oss.str();
}

std::string toString( unsigned long value ) {
    std::ostringstream oss;
    oss << value;
    if( value > Detail::hexThreshold )
        oss << " (0x" << std::hex << value << ")";
    return oss.str();
}

std::string toString( unsigned int value ) {
    return Catch::toString( static_cast<unsigned long>( value ) );
}

template<typename T>
std::string fpToString( T value, int precision ) {
    std::ostringstream oss;
    oss << std::setprecision( precision )
        << std::fixed
        << value;
    std::string d = oss.str();
    std::size_t i = d.find_last_not_of( '0' );
    if( i != std::string::npos && i != d.size()-1 ) {
        if( d[i] == '.' )
            i++;
        d = d.substr( 0, i+1 );
    }
    return d;
}

std::string toString( const double value ) {
    return fpToString( value, 10 );
}
std::string toString( const float value ) {
    return fpToString( value, 5 ) + "f";
}

std::string toString( bool value ) {
    return value ? "true" : "false";
}

std::string toString( char value ) {
    return value < ' '
        ? toString( static_cast<unsigned int>( value ) )
        : Detail::makeString( value );
}

std::string toString( signed char value ) {
    return toString( static_cast<char>( value ) );
}

std::string toString( unsigned char value ) {
    return toString( static_cast<char>( value ) );
}

#ifdef CATCH_CONFIG_CPP11_LONG_LONG
std::string toString( long long value ) {
    std::ostringstream oss;
    oss << value;
    if( value > Detail::hexThreshold )
        oss << " (0x" << std::hex << value << ")";
    return oss.str();
}
std::string toString( unsigned long long value ) {
    std::ostringstream oss;
    oss << value;
    if( value > Detail::hexThreshold )
        oss << " (0x" << std::hex << value << ")";
    return oss.str();
}
#endif

#ifdef CATCH_CONFIG_CPP11_NULLPTR
std::string toString( std::nullptr_t ) {
    return "nullptr";
}
#endif

#ifdef __OBJC__
    std::string toString( NSString const * const& nsstring ) {
        if( !nsstring )
            return "nil";
        return "@" + toString([nsstring UTF8String]);
    }
    std::string toString( NSString * CATCH_ARC_STRONG const& nsstring ) {
        if( !nsstring )
            return "nil";
        return "@" + toString([nsstring UTF8String]);
    }
    std::string toString( NSObject* const& nsObject ) {
        return toString( [nsObject description] );
    }
#endif

}


#define TWOBLUECUBES_CATCH_RESULT_BUILDER_HPP_INCLUDED

namespace Catch {

    std::string capturedExpressionWithSecondArgument( std::string const& capturedExpression, std::string const& secondArg ) {
        return secondArg.empty() || secondArg == "\"\""
            ? capturedExpression
            : capturedExpression + ", " + secondArg;
    }
    ResultBuilder::ResultBuilder(   char const* macroName,
                                    SourceLineInfo const& lineInfo,
                                    char const* capturedExpression,
                                    ResultDisposition::Flags resultDisposition,
                                    char const* secondArg )
    :   m_assertionInfo( macroName, lineInfo, capturedExpressionWithSecondArgument( capturedExpression, secondArg ), resultDisposition ),
        m_shouldDebugBreak( false ),
        m_shouldThrow( false )
    {}

    ResultBuilder& ResultBuilder::setResultType( ResultWas::OfType result ) {
        m_data.resultType = result;
        return *this;
    }
    ResultBuilder& ResultBuilder::setResultType( bool result ) {
        m_data.resultType = result ? ResultWas::Ok : ResultWas::ExpressionFailed;
        return *this;
    }
    ResultBuilder& ResultBuilder::setLhs( std::string const& lhs ) {
        m_exprComponents.lhs = lhs;
        return *this;
    }
    ResultBuilder& ResultBuilder::setRhs( std::string const& rhs ) {
        m_exprComponents.rhs = rhs;
        return *this;
    }
    ResultBuilder& ResultBuilder::setOp( std::string const& op ) {
        m_exprComponents.op = op;
        return *this;
    }

    void ResultBuilder::endExpression() {
        m_exprComponents.testFalse = isFalseTest( m_assertionInfo.resultDisposition );
        captureExpression();
    }

    void ResultBuilder::useActiveException( ResultDisposition::Flags resultDisposition ) {
        m_assertionInfo.resultDisposition = resultDisposition;
        m_stream.oss << Catch::translateActiveException();
        captureResult( ResultWas::ThrewException );
    }

    void ResultBuilder::captureResult( ResultWas::OfType resultType ) {
        setResultType( resultType );
        captureExpression();
    }
    void ResultBuilder::captureExpectedException( std::string const& expectedMessage ) {
        if( expectedMessage.empty() )
            captureExpectedException( Matchers::Impl::Generic::AllOf<std::string>() );
        else
            captureExpectedException( Matchers::Equals( expectedMessage ) );
    }

    void ResultBuilder::captureExpectedException( Matchers::Impl::Matcher<std::string> const& matcher ) {

        assert( m_exprComponents.testFalse == false );
        AssertionResultData data = m_data;
        data.resultType = ResultWas::Ok;
        data.reconstructedExpression = m_assertionInfo.capturedExpression;

        std::string actualMessage = Catch::translateActiveException();
        if( !matcher.match( actualMessage ) ) {
            data.resultType = ResultWas::ExpressionFailed;
            data.reconstructedExpression = actualMessage;
        }
        AssertionResult result( m_assertionInfo, data );
        handleResult( result );
    }

    void ResultBuilder::captureExpression() {
        AssertionResult result = build();
        handleResult( result );
    }
    void ResultBuilder::handleResult( AssertionResult const& result )
    {
        getResultCapture().assertionEnded( result );

        if( !result.isOk() ) {
            if( getCurrentContext().getConfig()->shouldDebugBreak() )
                m_shouldDebugBreak = true;
            if( getCurrentContext().getRunner()->aborting() || (m_assertionInfo.resultDisposition & ResultDisposition::Normal) )
                m_shouldThrow = true;
        }
    }
    void ResultBuilder::react() {
        if( m_shouldThrow )
            throw Catch::TestFailureException();
    }

    bool ResultBuilder::shouldDebugBreak() const { return m_shouldDebugBreak; }
    bool ResultBuilder::allowThrows() const { return getCurrentContext().getConfig()->allowThrows(); }

    AssertionResult ResultBuilder::build() const
    {
        assert( m_data.resultType != ResultWas::Unknown );

        AssertionResultData data = m_data;


        if( m_exprComponents.testFalse ) {
            if( data.resultType == ResultWas::Ok )
                data.resultType = ResultWas::ExpressionFailed;
            else if( data.resultType == ResultWas::ExpressionFailed )
                data.resultType = ResultWas::Ok;
        }

        data.message = m_stream.oss.str();
        data.reconstructedExpression = reconstructExpression();
        if( m_exprComponents.testFalse ) {
            if( m_exprComponents.op == "" )
                data.reconstructedExpression = "!" + data.reconstructedExpression;
            else
                data.reconstructedExpression = "!(" + data.reconstructedExpression + ")";
        }
        return AssertionResult( m_assertionInfo, data );
    }
    std::string ResultBuilder::reconstructExpression() const {
        if( m_exprComponents.op == "" )
            return m_exprComponents.lhs.empty() ? m_assertionInfo.capturedExpression : m_exprComponents.op + m_exprComponents.lhs;
        else if( m_exprComponents.op == "matches" )
            return m_exprComponents.lhs + " " + m_exprComponents.rhs;
        else if( m_exprComponents.op != "!" ) {
            if( m_exprComponents.lhs.size() + m_exprComponents.rhs.size() < 40 &&
                m_exprComponents.lhs.find("\n") == std::string::npos &&
                m_exprComponents.rhs.find("\n") == std::string::npos )
                return m_exprComponents.lhs + " " + m_exprComponents.op + " " + m_exprComponents.rhs;
            else
                return m_exprComponents.lhs + "\n" + m_exprComponents.op + "\n" + m_exprComponents.rhs;
        }
        else
            return "{can't expand - use " + m_assertionInfo.macroName + "_FALSE( " + m_assertionInfo.capturedExpression.substr(1) + " ) instead of " + m_assertionInfo.macroName + "( " + m_assertionInfo.capturedExpression + " ) for better diagnostics}";
    }

}


#define TWOBLUECUBES_CATCH_TAG_ALIAS_REGISTRY_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_TAG_ALIAS_REGISTRY_H_INCLUDED

#include <map>

namespace Catch {

    class TagAliasRegistry : public ITagAliasRegistry {
    public:
        virtual ~TagAliasRegistry();
        virtual Option<TagAlias> find( std::string const& alias ) const;
        virtual std::string expandAliases( std::string const& unexpandedTestSpec ) const;
        void add( char const* alias, char const* tag, SourceLineInfo const& lineInfo );
        static TagAliasRegistry& get();

    private:
        std::map<std::string, TagAlias> m_registry;
    };

}

#include <map>
#include <iostream>

namespace Catch {

    TagAliasRegistry::~TagAliasRegistry() {}

    Option<TagAlias> TagAliasRegistry::find( std::string const& alias ) const {
        std::map<std::string, TagAlias>::const_iterator it = m_registry.find( alias );
        if( it != m_registry.end() )
            return it->second;
        else
            return Option<TagAlias>();
    }

    std::string TagAliasRegistry::expandAliases( std::string const& unexpandedTestSpec ) const {
        std::string expandedTestSpec = unexpandedTestSpec;
        for( std::map<std::string, TagAlias>::const_iterator it = m_registry.begin(), itEnd = m_registry.end();
                it != itEnd;
                ++it ) {
            std::size_t pos = expandedTestSpec.find( it->first );
            if( pos != std::string::npos ) {
                expandedTestSpec =  expandedTestSpec.substr( 0, pos ) +
                                    it->second.tag +
                                    expandedTestSpec.substr( pos + it->first.size() );
            }
        }
        return expandedTestSpec;
    }

    void TagAliasRegistry::add( char const* alias, char const* tag, SourceLineInfo const& lineInfo ) {

        if( !startsWith( alias, "[@" ) || !endsWith( alias, "]" ) ) {
            std::ostringstream oss;
            oss << "error: tag alias, \"" << alias << "\" is not of the form [@alias name].\n" << lineInfo;
            throw std::domain_error( oss.str().c_str() );
        }
        if( !m_registry.insert( std::make_pair( alias, TagAlias( tag, lineInfo ) ) ).second ) {
            std::ostringstream oss;
            oss << "error: tag alias, \"" << alias << "\" already registered.\n"
                << "\tFirst seen at " << find(alias)->lineInfo << "\n"
                << "\tRedefined at " << lineInfo;
            throw std::domain_error( oss.str().c_str() );
        }
    }

    TagAliasRegistry& TagAliasRegistry::get() {
        static TagAliasRegistry instance;
        return instance;

    }

    ITagAliasRegistry::~ITagAliasRegistry() {}
    ITagAliasRegistry const& ITagAliasRegistry::get() { return TagAliasRegistry::get(); }

    RegistrarForTagAliases::RegistrarForTagAliases( char const* alias, char const* tag, SourceLineInfo const& lineInfo ) {
        try {
            TagAliasRegistry::get().add( alias, tag, lineInfo );
        }
        catch( std::exception& ex ) {
            Colour colourGuard( Colour::Red );
            Catch::cerr() << ex.what() << std::endl;
            exit(1);
        }
    }

}


#define TWOBLUECUBES_CATCH_REPORTER_MULTI_HPP_INCLUDED

namespace Catch {

class MultipleReporters : public SharedImpl<IStreamingReporter> {
    typedef std::vector<Ptr<IStreamingReporter> > Reporters;
    Reporters m_reporters;

public:
    void add( Ptr<IStreamingReporter> const& reporter ) {
        m_reporters.push_back( reporter );
    }

public:

    virtual ReporterPreferences getPreferences() const CATCH_OVERRIDE {
        return m_reporters[0]->getPreferences();
    }

    virtual void noMatchingTestCases( std::string const& spec ) CATCH_OVERRIDE {
        for( Reporters::const_iterator it = m_reporters.begin(), itEnd = m_reporters.end();
                it != itEnd;
                ++it )
            (*it)->noMatchingTestCases( spec );
    }

    virtual void testRunStarting( TestRunInfo const& testRunInfo ) CATCH_OVERRIDE {
        for( Reporters::const_iterator it = m_reporters.begin(), itEnd = m_reporters.end();
                it != itEnd;
                ++it )
            (*it)->testRunStarting( testRunInfo );
    }

    virtual void testGroupStarting( GroupInfo const& groupInfo ) CATCH_OVERRIDE {
        for( Reporters::const_iterator it = m_reporters.begin(), itEnd = m_reporters.end();
                it != itEnd;
                ++it )
            (*it)->testGroupStarting( groupInfo );
    }

    virtual void testCaseStarting( TestCaseInfo const& testInfo ) CATCH_OVERRIDE {
        for( Reporters::const_iterator it = m_reporters.begin(), itEnd = m_reporters.end();
                it != itEnd;
                ++it )
            (*it)->testCaseStarting( testInfo );
    }

    virtual void sectionStarting( SectionInfo const& sectionInfo ) CATCH_OVERRIDE {
        for( Reporters::const_iterator it = m_reporters.begin(), itEnd = m_reporters.end();
                it != itEnd;
                ++it )
            (*it)->sectionStarting( sectionInfo );
    }

    virtual void assertionStarting( AssertionInfo const& assertionInfo ) CATCH_OVERRIDE {
        for( Reporters::const_iterator it = m_reporters.begin(), itEnd = m_reporters.end();
                it != itEnd;
                ++it )
            (*it)->assertionStarting( assertionInfo );
    }


    virtual bool assertionEnded( AssertionStats const& assertionStats ) CATCH_OVERRIDE {
        bool clearBuffer = false;
        for( Reporters::const_iterator it = m_reporters.begin(), itEnd = m_reporters.end();
                it != itEnd;
                ++it )
            clearBuffer |= (*it)->assertionEnded( assertionStats );
        return clearBuffer;
    }

    virtual void sectionEnded( SectionStats const& sectionStats ) CATCH_OVERRIDE {
        for( Reporters::const_iterator it = m_reporters.begin(), itEnd = m_reporters.end();
                it != itEnd;
                ++it )
            (*it)->sectionEnded( sectionStats );
    }

    virtual void testCaseEnded( TestCaseStats const& testCaseStats ) CATCH_OVERRIDE {
        for( Reporters::const_iterator it = m_reporters.begin(), itEnd = m_reporters.end();
                it != itEnd;
                ++it )
            (*it)->testCaseEnded( testCaseStats );
    }

    virtual void testGroupEnded( TestGroupStats const& testGroupStats ) CATCH_OVERRIDE {
        for( Reporters::const_iterator it = m_reporters.begin(), itEnd = m_reporters.end();
                it != itEnd;
                ++it )
            (*it)->testGroupEnded( testGroupStats );
    }

    virtual void testRunEnded( TestRunStats const& testRunStats ) CATCH_OVERRIDE {
        for( Reporters::const_iterator it = m_reporters.begin(), itEnd = m_reporters.end();
                it != itEnd;
                ++it )
            (*it)->testRunEnded( testRunStats );
    }

    virtual void skipTest( TestCaseInfo const& testInfo ) CATCH_OVERRIDE {
        for( Reporters::const_iterator it = m_reporters.begin(), itEnd = m_reporters.end();
                it != itEnd;
                ++it )
            (*it)->skipTest( testInfo );
    }
};

Ptr<IStreamingReporter> addReporter( Ptr<IStreamingReporter> const& existingReporter, Ptr<IStreamingReporter> const& additionalReporter ) {
    Ptr<IStreamingReporter> resultingReporter;

    if( existingReporter ) {
        MultipleReporters* multi = dynamic_cast<MultipleReporters*>( existingReporter.get() );
        if( !multi ) {
            multi = new MultipleReporters;
            resultingReporter = Ptr<IStreamingReporter>( multi );
            if( existingReporter )
                multi->add( existingReporter );
        }
        else
            resultingReporter = existingReporter;
        multi->add( additionalReporter );
    }
    else
        resultingReporter = additionalReporter;

    return resultingReporter;
}

}


#define TWOBLUECUBES_CATCH_REPORTER_XML_HPP_INCLUDED


#define TWOBLUECUBES_CATCH_REPORTER_BASES_HPP_INCLUDED

#include <cstring>

namespace Catch {

    struct StreamingReporterBase : SharedImpl<IStreamingReporter> {

        StreamingReporterBase( ReporterConfig const& _config )
        :   m_config( _config.fullConfig() ),
            stream( _config.stream() )
        {
            m_reporterPrefs.shouldRedirectStdOut = false;
        }

        virtual ReporterPreferences getPreferences() const CATCH_OVERRIDE {
            return m_reporterPrefs;
        }

        virtual ~StreamingReporterBase() CATCH_OVERRIDE;

        virtual void noMatchingTestCases( std::string const& ) CATCH_OVERRIDE {}

        virtual void testRunStarting( TestRunInfo const& _testRunInfo ) CATCH_OVERRIDE {
            currentTestRunInfo = _testRunInfo;
        }
        virtual void testGroupStarting( GroupInfo const& _groupInfo ) CATCH_OVERRIDE {
            currentGroupInfo = _groupInfo;
        }

        virtual void testCaseStarting( TestCaseInfo const& _testInfo ) CATCH_OVERRIDE {
            currentTestCaseInfo = _testInfo;
        }
        virtual void sectionStarting( SectionInfo const& _sectionInfo ) CATCH_OVERRIDE {
            m_sectionStack.push_back( _sectionInfo );
        }

        virtual void sectionEnded( SectionStats const& ) CATCH_OVERRIDE {
            m_sectionStack.pop_back();
        }
        virtual void testCaseEnded( TestCaseStats const& ) CATCH_OVERRIDE {
            currentTestCaseInfo.reset();
        }
        virtual void testGroupEnded( TestGroupStats const& ) CATCH_OVERRIDE {
            currentGroupInfo.reset();
        }
        virtual void testRunEnded( TestRunStats const& ) CATCH_OVERRIDE {
            currentTestCaseInfo.reset();
            currentGroupInfo.reset();
            currentTestRunInfo.reset();
        }

        virtual void skipTest( TestCaseInfo const& ) CATCH_OVERRIDE {


        }

        Ptr<IConfig const> m_config;
        std::ostream& stream;

        LazyStat<TestRunInfo> currentTestRunInfo;
        LazyStat<GroupInfo> currentGroupInfo;
        LazyStat<TestCaseInfo> currentTestCaseInfo;

        std::vector<SectionInfo> m_sectionStack;
        ReporterPreferences m_reporterPrefs;
    };

    struct CumulativeReporterBase : SharedImpl<IStreamingReporter> {
        template<typename T, typename ChildNodeT>
        struct Node : SharedImpl<> {
            explicit Node( T const& _value ) : value( _value ) {}
            virtual ~Node() {}

            typedef std::vector<Ptr<ChildNodeT> > ChildNodes;
            T value;
            ChildNodes children;
        };
        struct SectionNode : SharedImpl<> {
            explicit SectionNode( SectionStats const& _stats ) : stats( _stats ) {}
            virtual ~SectionNode();

            bool operator == ( SectionNode const& other ) const {
                return stats.sectionInfo.lineInfo == other.stats.sectionInfo.lineInfo;
            }
            bool operator == ( Ptr<SectionNode> const& other ) const {
                return operator==( *other );
            }

            SectionStats stats;
            typedef std::vector<Ptr<SectionNode> > ChildSections;
            typedef std::vector<AssertionStats> Assertions;
            ChildSections childSections;
            Assertions assertions;
            std::string stdOut;
            std::string stdErr;
        };

        struct BySectionInfo {
            BySectionInfo( SectionInfo const& other ) : m_other( other ) {}
			BySectionInfo( BySectionInfo const& other ) : m_other( other.m_other ) {}
            bool operator() ( Ptr<SectionNode> const& node ) const {
                return node->stats.sectionInfo.lineInfo == m_other.lineInfo;
            }
        private:
			void operator=( BySectionInfo const& );
            SectionInfo const& m_other;
        };

        typedef Node<TestCaseStats, SectionNode> TestCaseNode;
        typedef Node<TestGroupStats, TestCaseNode> TestGroupNode;
        typedef Node<TestRunStats, TestGroupNode> TestRunNode;

        CumulativeReporterBase( ReporterConfig const& _config )
        :   m_config( _config.fullConfig() ),
            stream( _config.stream() )
        {
            m_reporterPrefs.shouldRedirectStdOut = false;
        }
        ~CumulativeReporterBase();

        virtual ReporterPreferences getPreferences() const CATCH_OVERRIDE {
            return m_reporterPrefs;
        }

        virtual void testRunStarting( TestRunInfo const& ) CATCH_OVERRIDE {}
        virtual void testGroupStarting( GroupInfo const& ) CATCH_OVERRIDE {}

        virtual void testCaseStarting( TestCaseInfo const& ) CATCH_OVERRIDE {}

        virtual void sectionStarting( SectionInfo const& sectionInfo ) CATCH_OVERRIDE {
            SectionStats incompleteStats( sectionInfo, Counts(), 0, false );
            Ptr<SectionNode> node;
            if( m_sectionStack.empty() ) {
                if( !m_rootSection )
                    m_rootSection = new SectionNode( incompleteStats );
                node = m_rootSection;
            }
            else {
                SectionNode& parentNode = *m_sectionStack.back();
                SectionNode::ChildSections::const_iterator it =
                    std::find_if(   parentNode.childSections.begin(),
                                    parentNode.childSections.end(),
                                    BySectionInfo( sectionInfo ) );
                if( it == parentNode.childSections.end() ) {
                    node = new SectionNode( incompleteStats );
                    parentNode.childSections.push_back( node );
                }
                else
                    node = *it;
            }
            m_sectionStack.push_back( node );
            m_deepestSection = node;
        }

        virtual void assertionStarting( AssertionInfo const& ) CATCH_OVERRIDE {}

        virtual bool assertionEnded( AssertionStats const& assertionStats ) {
            assert( !m_sectionStack.empty() );
            SectionNode& sectionNode = *m_sectionStack.back();
            sectionNode.assertions.push_back( assertionStats );
            return true;
        }
        virtual void sectionEnded( SectionStats const& sectionStats ) CATCH_OVERRIDE {
            assert( !m_sectionStack.empty() );
            SectionNode& node = *m_sectionStack.back();
            node.stats = sectionStats;
            m_sectionStack.pop_back();
        }
        virtual void testCaseEnded( TestCaseStats const& testCaseStats ) CATCH_OVERRIDE {
            Ptr<TestCaseNode> node = new TestCaseNode( testCaseStats );
            assert( m_sectionStack.size() == 0 );
            node->children.push_back( m_rootSection );
            m_testCases.push_back( node );
            m_rootSection.reset();

            assert( m_deepestSection );
            m_deepestSection->stdOut = testCaseStats.stdOut;
            m_deepestSection->stdErr = testCaseStats.stdErr;
        }
        virtual void testGroupEnded( TestGroupStats const& testGroupStats ) CATCH_OVERRIDE {
            Ptr<TestGroupNode> node = new TestGroupNode( testGroupStats );
            node->children.swap( m_testCases );
            m_testGroups.push_back( node );
        }
        virtual void testRunEnded( TestRunStats const& testRunStats ) CATCH_OVERRIDE {
            Ptr<TestRunNode> node = new TestRunNode( testRunStats );
            node->children.swap( m_testGroups );
            m_testRuns.push_back( node );
            testRunEndedCumulative();
        }
        virtual void testRunEndedCumulative() = 0;

        virtual void skipTest( TestCaseInfo const& ) CATCH_OVERRIDE {}

        Ptr<IConfig const> m_config;
        std::ostream& stream;
        std::vector<AssertionStats> m_assertions;
        std::vector<std::vector<Ptr<SectionNode> > > m_sections;
        std::vector<Ptr<TestCaseNode> > m_testCases;
        std::vector<Ptr<TestGroupNode> > m_testGroups;

        std::vector<Ptr<TestRunNode> > m_testRuns;

        Ptr<SectionNode> m_rootSection;
        Ptr<SectionNode> m_deepestSection;
        std::vector<Ptr<SectionNode> > m_sectionStack;
        ReporterPreferences m_reporterPrefs;

    };

    template<char C>
    char const* getLineOfChars() {
        static char line[CATCH_CONFIG_CONSOLE_WIDTH] = {0};
        if( !*line ) {
            memset( line, C, CATCH_CONFIG_CONSOLE_WIDTH-1 );
            line[CATCH_CONFIG_CONSOLE_WIDTH-1] = 0;
        }
        return line;
    }

    struct TestEventListenerBase : StreamingReporterBase {
        TestEventListenerBase( ReporterConfig const& _config )
        :   StreamingReporterBase( _config )
        {}

        virtual void assertionStarting( AssertionInfo const& ) CATCH_OVERRIDE {}
        virtual bool assertionEnded( AssertionStats const& ) CATCH_OVERRIDE {
            return false;
        }
    };

}


#define TWOBLUECUBES_CATCH_REPORTER_REGISTRARS_HPP_INCLUDED

namespace Catch {

    template<typename T>
    class LegacyReporterRegistrar {

        class ReporterFactory : public IReporterFactory {
            virtual IStreamingReporter* create( ReporterConfig const& config ) const {
                return new LegacyReporterAdapter( new T( config ) );
            }

            virtual std::string getDescription() const {
                return T::getDescription();
            }
        };

    public:

        LegacyReporterRegistrar( std::string const& name ) {
            getMutableRegistryHub().registerReporter( name, new ReporterFactory() );
        }
    };

    template<typename T>
    class ReporterRegistrar {

        class ReporterFactory : public SharedImpl<IReporterFactory> {












            virtual IStreamingReporter* create( ReporterConfig const& config ) const {
                return new T( config );
            }

            virtual std::string getDescription() const {
                return T::getDescription();
            }
        };

    public:

        ReporterRegistrar( std::string const& name ) {
            getMutableRegistryHub().registerReporter( name, new ReporterFactory() );
        }
    };

    template<typename T>
    class ListenerRegistrar {

        class ListenerFactory : public SharedImpl<IReporterFactory> {

            virtual IStreamingReporter* create( ReporterConfig const& config ) const {
                return new T( config );
            }
            virtual std::string getDescription() const {
                return "";
            }
        };

    public:

        ListenerRegistrar() {
            getMutableRegistryHub().registerListener( new ListenerFactory() );
        }
    };
}

#define INTERNAL_CATCH_REGISTER_LEGACY_REPORTER( name, reporterType ) \
    namespace{ Catch::LegacyReporterRegistrar<reporterType> catch_internal_RegistrarFor##reporterType( name ); }

#define INTERNAL_CATCH_REGISTER_REPORTER( name, reporterType ) \
    namespace{ Catch::ReporterRegistrar<reporterType> catch_internal_RegistrarFor##reporterType( name ); }

#define INTERNAL_CATCH_REGISTER_LISTENER( listenerType ) \
    namespace{ Catch::ListenerRegistrar<listenerType> catch_internal_RegistrarFor##listenerType; }


#define TWOBLUECUBES_CATCH_XMLWRITER_HPP_INCLUDED

#include <sstream>
#include <string>
#include <vector>
#include <iomanip>

namespace Catch {

    class XmlEncode {
    public:
        enum ForWhat { ForTextNodes, ForAttributes };

        XmlEncode( std::string const& str, ForWhat forWhat = ForTextNodes )
        :   m_str( str ),
            m_forWhat( forWhat )
        {}

        void encodeTo( std::ostream& os ) const {




            for( std::size_t i = 0; i < m_str.size(); ++ i ) {
                char c = m_str[i];
                switch( c ) {
                    case '<':   os << "&lt;"; break;
                    case '&':   os << "&amp;"; break;

                    case '>':

                        if( i > 2 && m_str[i-1] == ']' && m_str[i-2] == ']' )
                            os << "&gt;";
                        else
                            os << c;
                        break;

                    case '\"':
                        if( m_forWhat == ForAttributes )
                            os << "&quot;";
                        else
                            os << c;
                        break;

                    default:

                        if ( ( c < '\x09' ) || ( c > '\x0D' && c < '\x20') || c=='\x7F' )
                            os << "&#x" << std::uppercase << std::hex << static_cast<int>( c );
                        else
                            os << c;
                }
            }
        }

        friend std::ostream& operator << ( std::ostream& os, XmlEncode const& xmlEncode ) {
            xmlEncode.encodeTo( os );
            return os;
        }

    private:
        std::string m_str;
        ForWhat m_forWhat;
    };

    class XmlWriter {
    public:

        class ScopedElement {
        public:
            ScopedElement( XmlWriter* writer )
            :   m_writer( writer )
            {}

            ScopedElement( ScopedElement const& other )
            :   m_writer( other.m_writer ){
                other.m_writer = CATCH_NULL;
            }

            ~ScopedElement() {
                if( m_writer )
                    m_writer->endElement();
            }

            ScopedElement& writeText( std::string const& text, bool indent = true ) {
                m_writer->writeText( text, indent );
                return *this;
            }

            template<typename T>
            ScopedElement& writeAttribute( std::string const& name, T const& attribute ) {
                m_writer->writeAttribute( name, attribute );
                return *this;
            }

        private:
            mutable XmlWriter* m_writer;
        };

        XmlWriter()
        :   m_tagIsOpen( false ),
            m_needsNewline( false ),
            m_os( &Catch::cout() )
        {}

        XmlWriter( std::ostream& os )
        :   m_tagIsOpen( false ),
            m_needsNewline( false ),
            m_os( &os )
        {}

        ~XmlWriter() {
            while( !m_tags.empty() )
                endElement();
        }

        XmlWriter& startElement( std::string const& name ) {
            ensureTagClosed();
            newlineIfNecessary();
            stream() << m_indent << "<" << name;
            m_tags.push_back( name );
            m_indent += "  ";
            m_tagIsOpen = true;
            return *this;
        }

        ScopedElement scopedElement( std::string const& name ) {
            ScopedElement scoped( this );
            startElement( name );
            return scoped;
        }

        XmlWriter& endElement() {
            newlineIfNecessary();
            m_indent = m_indent.substr( 0, m_indent.size()-2 );
            if( m_tagIsOpen ) {
                stream() << "/>\n";
                m_tagIsOpen = false;
            }
            else {
                stream() << m_indent << "</" << m_tags.back() << ">\n";
            }
            m_tags.pop_back();
            return *this;
        }

        XmlWriter& writeAttribute( std::string const& name, std::string const& attribute ) {
            if( !name.empty() && !attribute.empty() )
                stream() << " " << name << "=\"" << XmlEncode( attribute, XmlEncode::ForAttributes ) << "\"";
            return *this;
        }

        XmlWriter& writeAttribute( std::string const& name, bool attribute ) {
            stream() << " " << name << "=\"" << ( attribute ? "true" : "false" ) << "\"";
            return *this;
        }

        template<typename T>
        XmlWriter& writeAttribute( std::string const& name, T const& attribute ) {
            std::ostringstream oss;
            oss << attribute;
            return writeAttribute( name, oss.str() );
        }

        XmlWriter& writeText( std::string const& text, bool indent = true ) {
            if( !text.empty() ){
                bool tagWasOpen = m_tagIsOpen;
                ensureTagClosed();
                if( tagWasOpen && indent )
                    stream() << m_indent;
                stream() << XmlEncode( text );
                m_needsNewline = true;
            }
            return *this;
        }

        XmlWriter& writeComment( std::string const& text ) {
            ensureTagClosed();
            stream() << m_indent << "<!--" << text << "-->";
            m_needsNewline = true;
            return *this;
        }

        XmlWriter& writeBlankLine() {
            ensureTagClosed();
            stream() << "\n";
            return *this;
        }

        void setStream( std::ostream& os ) {
            m_os = &os;
        }

    private:
        XmlWriter( XmlWriter const& );
        void operator=( XmlWriter const& );

        std::ostream& stream() {
            return *m_os;
        }

        void ensureTagClosed() {
            if( m_tagIsOpen ) {
                stream() << ">\n";
                m_tagIsOpen = false;
            }
        }

        void newlineIfNecessary() {
            if( m_needsNewline ) {
                stream() << "\n";
                m_needsNewline = false;
            }
        }

        bool m_tagIsOpen;
        bool m_needsNewline;
        std::vector<std::string> m_tags;
        std::string m_indent;
        std::ostream* m_os;
    };

}


#define TWOBLUECUBES_CATCH_REENABLE_WARNINGS_H_INCLUDED

#ifdef __clang__
#    ifdef __ICC
#        pragma warning(pop)
#    else
#        pragma clang diagnostic pop
#    endif
#elif defined __GNUC__
#    pragma GCC diagnostic pop
#endif


namespace Catch {
    class XmlReporter : public StreamingReporterBase {
    public:
        XmlReporter( ReporterConfig const& _config )
        :   StreamingReporterBase( _config ),
            m_sectionDepth( 0 )
        {
            m_reporterPrefs.shouldRedirectStdOut = true;
        }

        virtual ~XmlReporter() CATCH_OVERRIDE;

        static std::string getDescription() {
            return "Reports test results as an XML document";
        }

    public:

        virtual void noMatchingTestCases( std::string const& s ) CATCH_OVERRIDE {
            StreamingReporterBase::noMatchingTestCases( s );
        }

        virtual void testRunStarting( TestRunInfo const& testInfo ) CATCH_OVERRIDE {
            StreamingReporterBase::testRunStarting( testInfo );
            m_xml.setStream( stream );
            m_xml.startElement( "Catch" );
            if( !m_config->name().empty() )
                m_xml.writeAttribute( "name", m_config->name() );
        }

        virtual void testGroupStarting( GroupInfo const& groupInfo ) CATCH_OVERRIDE {
            StreamingReporterBase::testGroupStarting( groupInfo );
            m_xml.startElement( "Group" )
                .writeAttribute( "name", groupInfo.name );
        }

        virtual void testCaseStarting( TestCaseInfo const& testInfo ) CATCH_OVERRIDE {
            StreamingReporterBase::testCaseStarting(testInfo);
            m_xml.startElement( "TestCase" ).writeAttribute( "name", trim( testInfo.name ) );

            if ( m_config->showDurations() == ShowDurations::Always )
                m_testCaseTimer.start();
        }

        virtual void sectionStarting( SectionInfo const& sectionInfo ) CATCH_OVERRIDE {
            StreamingReporterBase::sectionStarting( sectionInfo );
            if( m_sectionDepth++ > 0 ) {
                m_xml.startElement( "Section" )
                    .writeAttribute( "name", trim( sectionInfo.name ) )
                    .writeAttribute( "description", sectionInfo.description );
            }
        }

        virtual void assertionStarting( AssertionInfo const& ) CATCH_OVERRIDE { }

        virtual bool assertionEnded( AssertionStats const& assertionStats ) CATCH_OVERRIDE {
            const AssertionResult& assertionResult = assertionStats.assertionResult;


            if( assertionStats.assertionResult.getResultType() != ResultWas::Ok ) {
                for( std::vector<MessageInfo>::const_iterator it = assertionStats.infoMessages.begin(), itEnd = assertionStats.infoMessages.end();
                        it != itEnd;
                        ++it ) {
                    if( it->type == ResultWas::Info ) {
                        m_xml.scopedElement( "Info" )
                            .writeText( it->message );
                    } else if ( it->type == ResultWas::Warning ) {
                        m_xml.scopedElement( "Warning" )
                            .writeText( it->message );
                    }
                }
            }


            if( !m_config->includeSuccessfulResults() && isOk(assertionResult.getResultType()) )
                return true;


            if( assertionResult.hasExpression() ) {
                m_xml.startElement( "Expression" )
                    .writeAttribute( "success", assertionResult.succeeded() )
					.writeAttribute( "type", assertionResult.getTestMacroName() )
                    .writeAttribute( "filename", assertionResult.getSourceInfo().file )
                    .writeAttribute( "line", assertionResult.getSourceInfo().line );

                m_xml.scopedElement( "Original" )
                    .writeText( assertionResult.getExpression() );
                m_xml.scopedElement( "Expanded" )
                    .writeText( assertionResult.getExpandedExpression() );
            }


            switch( assertionResult.getResultType() ) {
                case ResultWas::ThrewException:
                    m_xml.scopedElement( "Exception" )
                        .writeAttribute( "filename", assertionResult.getSourceInfo().file )
                        .writeAttribute( "line", assertionResult.getSourceInfo().line )
                        .writeText( assertionResult.getMessage() );
                    break;
                case ResultWas::FatalErrorCondition:
                    m_xml.scopedElement( "Fatal Error Condition" )
                        .writeAttribute( "filename", assertionResult.getSourceInfo().file )
                        .writeAttribute( "line", assertionResult.getSourceInfo().line )
                        .writeText( assertionResult.getMessage() );
                    break;
                case ResultWas::Info:
                    m_xml.scopedElement( "Info" )
                        .writeText( assertionResult.getMessage() );
                    break;
                case ResultWas::Warning:

                    break;
                case ResultWas::ExplicitFailure:
                    m_xml.scopedElement( "Failure" )
                        .writeText( assertionResult.getMessage() );
                    break;
                default:
                    break;
            }

            if( assertionResult.hasExpression() )
                m_xml.endElement();

            return true;
        }

        virtual void sectionEnded( SectionStats const& sectionStats ) CATCH_OVERRIDE {
            StreamingReporterBase::sectionEnded( sectionStats );
            if( --m_sectionDepth > 0 ) {
                XmlWriter::ScopedElement e = m_xml.scopedElement( "OverallResults" );
                e.writeAttribute( "successes", sectionStats.assertions.passed );
                e.writeAttribute( "failures", sectionStats.assertions.failed );
                e.writeAttribute( "expectedFailures", sectionStats.assertions.failedButOk );

                if ( m_config->showDurations() == ShowDurations::Always )
                    e.writeAttribute( "durationInSeconds", sectionStats.durationInSeconds );

                m_xml.endElement();
            }
        }

        virtual void testCaseEnded( TestCaseStats const& testCaseStats ) CATCH_OVERRIDE {
            StreamingReporterBase::testCaseEnded( testCaseStats );
            XmlWriter::ScopedElement e = m_xml.scopedElement( "OverallResult" );
            e.writeAttribute( "success", testCaseStats.totals.assertions.allOk() );

            if ( m_config->showDurations() == ShowDurations::Always )
                e.writeAttribute( "durationInSeconds", m_testCaseTimer.getElapsedSeconds() );

            m_xml.endElement();
        }

        virtual void testGroupEnded( TestGroupStats const& testGroupStats ) CATCH_OVERRIDE {
            StreamingReporterBase::testGroupEnded( testGroupStats );

            m_xml.scopedElement( "OverallResults" )
                .writeAttribute( "successes", testGroupStats.totals.assertions.passed )
                .writeAttribute( "failures", testGroupStats.totals.assertions.failed )
                .writeAttribute( "expectedFailures", testGroupStats.totals.assertions.failedButOk );
            m_xml.endElement();
        }

        virtual void testRunEnded( TestRunStats const& testRunStats ) CATCH_OVERRIDE {
            StreamingReporterBase::testRunEnded( testRunStats );
            m_xml.scopedElement( "OverallResults" )
                .writeAttribute( "successes", testRunStats.totals.assertions.passed )
                .writeAttribute( "failures", testRunStats.totals.assertions.failed )
                .writeAttribute( "expectedFailures", testRunStats.totals.assertions.failedButOk );
            m_xml.endElement();
        }

    private:
        Timer m_testCaseTimer;
        XmlWriter m_xml;
        int m_sectionDepth;
    };

     INTERNAL_CATCH_REGISTER_REPORTER( "xml", XmlReporter )

}


#define TWOBLUECUBES_CATCH_REPORTER_JUNIT_HPP_INCLUDED

#include <assert.h>

namespace Catch {

    class JunitReporter : public CumulativeReporterBase {
    public:
        JunitReporter( ReporterConfig const& _config )
        :   CumulativeReporterBase( _config ),
            xml( _config.stream() )
        {
            m_reporterPrefs.shouldRedirectStdOut = true;
        }

        virtual ~JunitReporter() CATCH_OVERRIDE;

        static std::string getDescription() {
            return "Reports test results in an XML format that looks like Ant's junitreport target";
        }

        virtual void noMatchingTestCases( std::string const& ) CATCH_OVERRIDE {}

        virtual void testRunStarting( TestRunInfo const& runInfo ) CATCH_OVERRIDE {
            CumulativeReporterBase::testRunStarting( runInfo );
            xml.startElement( "testsuites" );
        }

        virtual void testGroupStarting( GroupInfo const& groupInfo ) CATCH_OVERRIDE {
            suiteTimer.start();
            stdOutForSuite.str("");
            stdErrForSuite.str("");
            unexpectedExceptions = 0;
            CumulativeReporterBase::testGroupStarting( groupInfo );
        }

        virtual bool assertionEnded( AssertionStats const& assertionStats ) CATCH_OVERRIDE {
            if( assertionStats.assertionResult.getResultType() == ResultWas::ThrewException )
                unexpectedExceptions++;
            return CumulativeReporterBase::assertionEnded( assertionStats );
        }

        virtual void testCaseEnded( TestCaseStats const& testCaseStats ) CATCH_OVERRIDE {
            stdOutForSuite << testCaseStats.stdOut;
            stdErrForSuite << testCaseStats.stdErr;
            CumulativeReporterBase::testCaseEnded( testCaseStats );
        }

        virtual void testGroupEnded( TestGroupStats const& testGroupStats ) CATCH_OVERRIDE {
            double suiteTime = suiteTimer.getElapsedSeconds();
            CumulativeReporterBase::testGroupEnded( testGroupStats );
            writeGroup( *m_testGroups.back(), suiteTime );
        }

        virtual void testRunEndedCumulative() CATCH_OVERRIDE {
            xml.endElement();
        }

        void writeGroup( TestGroupNode const& groupNode, double suiteTime ) {
            XmlWriter::ScopedElement e = xml.scopedElement( "testsuite" );
            TestGroupStats const& stats = groupNode.value;
            xml.writeAttribute( "name", stats.groupInfo.name );
            xml.writeAttribute( "errors", unexpectedExceptions );
            xml.writeAttribute( "failures", stats.totals.assertions.failed-unexpectedExceptions );
            xml.writeAttribute( "tests", stats.totals.assertions.total() );
            xml.writeAttribute( "hostname", "tbd" );
            if( m_config->showDurations() == ShowDurations::Never )
                xml.writeAttribute( "time", "" );
            else
                xml.writeAttribute( "time", suiteTime );
            xml.writeAttribute( "timestamp", "tbd" );


            for( TestGroupNode::ChildNodes::const_iterator
                    it = groupNode.children.begin(), itEnd = groupNode.children.end();
                    it != itEnd;
                    ++it )
                writeTestCase( **it );

            xml.scopedElement( "system-out" ).writeText( trim( stdOutForSuite.str() ), false );
            xml.scopedElement( "system-err" ).writeText( trim( stdErrForSuite.str() ), false );
        }

        void writeTestCase( TestCaseNode const& testCaseNode ) {
            TestCaseStats const& stats = testCaseNode.value;



            assert( testCaseNode.children.size() == 1 );
            SectionNode const& rootSection = *testCaseNode.children.front();

            std::string className = stats.testInfo.className;

            if( className.empty() ) {
                if( rootSection.childSections.empty() )
                    className = "global";
            }
            writeSection( className, "", rootSection );
        }

        void writeSection(  std::string const& className,
                            std::string const& rootName,
                            SectionNode const& sectionNode ) {
            std::string name = trim( sectionNode.stats.sectionInfo.name );
            if( !rootName.empty() )
                name = rootName + "/" + name;

            if( !sectionNode.assertions.empty() ||
                !sectionNode.stdOut.empty() ||
                !sectionNode.stdErr.empty() ) {
                XmlWriter::ScopedElement e = xml.scopedElement( "testcase" );
                if( className.empty() ) {
                    xml.writeAttribute( "classname", name );
                    xml.writeAttribute( "name", "root" );
                }
                else {
                    xml.writeAttribute( "classname", className );
                    xml.writeAttribute( "name", name );
                }
                xml.writeAttribute( "time", Catch::toString( sectionNode.stats.durationInSeconds ) );

                writeAssertions( sectionNode );

                if( !sectionNode.stdOut.empty() )
                    xml.scopedElement( "system-out" ).writeText( trim( sectionNode.stdOut ), false );
                if( !sectionNode.stdErr.empty() )
                    xml.scopedElement( "system-err" ).writeText( trim( sectionNode.stdErr ), false );
            }
            for( SectionNode::ChildSections::const_iterator
                    it = sectionNode.childSections.begin(),
                    itEnd = sectionNode.childSections.end();
                    it != itEnd;
                    ++it )
                if( className.empty() )
                    writeSection( name, "", **it );
                else
                    writeSection( className, name, **it );
        }

        void writeAssertions( SectionNode const& sectionNode ) {
            for( SectionNode::Assertions::const_iterator
                    it = sectionNode.assertions.begin(), itEnd = sectionNode.assertions.end();
                    it != itEnd;
                    ++it )
                writeAssertion( *it );
        }
        void writeAssertion( AssertionStats const& stats ) {
            AssertionResult const& result = stats.assertionResult;
            if( !result.isOk() ) {
                std::string elementName;
                switch( result.getResultType() ) {
                    case ResultWas::ThrewException:
                    case ResultWas::FatalErrorCondition:
                        elementName = "error";
                        break;
                    case ResultWas::ExplicitFailure:
                        elementName = "failure";
                        break;
                    case ResultWas::ExpressionFailed:
                        elementName = "failure";
                        break;
                    case ResultWas::DidntThrowException:
                        elementName = "failure";
                        break;


                    case ResultWas::Info:
                    case ResultWas::Warning:
                    case ResultWas::Ok:
                    case ResultWas::Unknown:
                    case ResultWas::FailureBit:
                    case ResultWas::Exception:
                        elementName = "internalError";
                        break;
                }

                XmlWriter::ScopedElement e = xml.scopedElement( elementName );

                xml.writeAttribute( "message", result.getExpandedExpression() );
                xml.writeAttribute( "type", result.getTestMacroName() );

                std::ostringstream oss;
                if( !result.getMessage().empty() )
                    oss << result.getMessage() << "\n";
                for( std::vector<MessageInfo>::const_iterator
                        it = stats.infoMessages.begin(),
                        itEnd = stats.infoMessages.end();
                            it != itEnd;
                            ++it )
                    if( it->type == ResultWas::Info )
                        oss << it->message << "\n";

                oss << "at " << result.getSourceInfo();
                xml.writeText( oss.str(), false );
            }
        }

        XmlWriter xml;
        Timer suiteTimer;
        std::ostringstream stdOutForSuite;
        std::ostringstream stdErrForSuite;
        unsigned int unexpectedExceptions;
    };

    INTERNAL_CATCH_REGISTER_REPORTER( "junit", JunitReporter )

}


#define TWOBLUECUBES_CATCH_REPORTER_CONSOLE_HPP_INCLUDED

namespace Catch {

    struct ConsoleReporter : StreamingReporterBase {
        ConsoleReporter( ReporterConfig const& _config )
        :   StreamingReporterBase( _config ),
            m_headerPrinted( false )
        {}

        virtual ~ConsoleReporter() CATCH_OVERRIDE;
        static std::string getDescription() {
            return "Reports test results as plain lines of text";
        }

        virtual void noMatchingTestCases( std::string const& spec ) CATCH_OVERRIDE {
            stream << "No test cases matched '" << spec << "'" << std::endl;
        }

        virtual void assertionStarting( AssertionInfo const& ) CATCH_OVERRIDE {
        }

        virtual bool assertionEnded( AssertionStats const& _assertionStats ) CATCH_OVERRIDE {
            AssertionResult const& result = _assertionStats.assertionResult;

            bool printInfoMessages = true;


            if( !m_config->includeSuccessfulResults() && result.isOk() ) {
                if( result.getResultType() != ResultWas::Warning )
                    return false;
                printInfoMessages = false;
            }

            lazyPrint();

            AssertionPrinter printer( stream, _assertionStats, printInfoMessages );
            printer.print();
            stream << std::endl;
            return true;
        }

        virtual void sectionStarting( SectionInfo const& _sectionInfo ) CATCH_OVERRIDE {
            m_headerPrinted = false;
            StreamingReporterBase::sectionStarting( _sectionInfo );
        }
        virtual void sectionEnded( SectionStats const& _sectionStats ) CATCH_OVERRIDE {
            if( _sectionStats.missingAssertions ) {
                lazyPrint();
                Colour colour( Colour::ResultError );
                if( m_sectionStack.size() > 1 )
                    stream << "\nNo assertions in section";
                else
                    stream << "\nNo assertions in test case";
                stream << " '" << _sectionStats.sectionInfo.name << "'\n" << std::endl;
            }
            if( m_headerPrinted ) {
                if( m_config->showDurations() == ShowDurations::Always )
                    stream << "Completed in " << _sectionStats.durationInSeconds << "s" << std::endl;
                m_headerPrinted = false;
            }
            else {
                if( m_config->showDurations() == ShowDurations::Always )
                    stream << _sectionStats.sectionInfo.name << " completed in " << _sectionStats.durationInSeconds << "s" << std::endl;
            }
            StreamingReporterBase::sectionEnded( _sectionStats );
        }

        virtual void testCaseEnded( TestCaseStats const& _testCaseStats ) CATCH_OVERRIDE {
            StreamingReporterBase::testCaseEnded( _testCaseStats );
            m_headerPrinted = false;
        }
        virtual void testGroupEnded( TestGroupStats const& _testGroupStats ) CATCH_OVERRIDE {
            if( currentGroupInfo.used ) {
                printSummaryDivider();
                stream << "Summary for group '" << _testGroupStats.groupInfo.name << "':\n";
                printTotals( _testGroupStats.totals );
                stream << "\n" << std::endl;
            }
            StreamingReporterBase::testGroupEnded( _testGroupStats );
        }
        virtual void testRunEnded( TestRunStats const& _testRunStats ) CATCH_OVERRIDE {
            printTotalsDivider( _testRunStats.totals );
            printTotals( _testRunStats.totals );
            stream << std::endl;
            StreamingReporterBase::testRunEnded( _testRunStats );
        }

    private:

        class AssertionPrinter {
            void operator= ( AssertionPrinter const& );
        public:
            AssertionPrinter( std::ostream& _stream, AssertionStats const& _stats, bool _printInfoMessages )
            :   stream( _stream ),
                stats( _stats ),
                result( _stats.assertionResult ),
                colour( Colour::None ),
                message( result.getMessage() ),
                messages( _stats.infoMessages ),
                printInfoMessages( _printInfoMessages )
            {
                switch( result.getResultType() ) {
                    case ResultWas::Ok:
                        colour = Colour::Success;
                        passOrFail = "PASSED";

                        if( _stats.infoMessages.size() == 1 )
                            messageLabel = "with message";
                        if( _stats.infoMessages.size() > 1 )
                            messageLabel = "with messages";
                        break;
                    case ResultWas::ExpressionFailed:
                        if( result.isOk() ) {
                            colour = Colour::Success;
                            passOrFail = "FAILED - but was ok";
                        }
                        else {
                            colour = Colour::Error;
                            passOrFail = "FAILED";
                        }
                        if( _stats.infoMessages.size() == 1 )
                            messageLabel = "with message";
                        if( _stats.infoMessages.size() > 1 )
                            messageLabel = "with messages";
                        break;
                    case ResultWas::ThrewException:
                        colour = Colour::Error;
                        passOrFail = "FAILED";
                        messageLabel = "due to unexpected exception with message";
                        break;
                    case ResultWas::FatalErrorCondition:
                        colour = Colour::Error;
                        passOrFail = "FAILED";
                        messageLabel = "due to a fatal error condition";
                        break;
                    case ResultWas::DidntThrowException:
                        colour = Colour::Error;
                        passOrFail = "FAILED";
                        messageLabel = "because no exception was thrown where one was expected";
                        break;
                    case ResultWas::Info:
                        messageLabel = "info";
                        break;
                    case ResultWas::Warning:
                        messageLabel = "warning";
                        break;
                    case ResultWas::ExplicitFailure:
                        passOrFail = "FAILED";
                        colour = Colour::Error;
                        if( _stats.infoMessages.size() == 1 )
                            messageLabel = "explicitly with message";
                        if( _stats.infoMessages.size() > 1 )
                            messageLabel = "explicitly with messages";
                        break;

                    case ResultWas::Unknown:
                    case ResultWas::FailureBit:
                    case ResultWas::Exception:
                        passOrFail = "** internal error **";
                        colour = Colour::Error;
                        break;
                }
            }

            void print() const {
                printSourceInfo();
                if( stats.totals.assertions.total() > 0 ) {
                    if( result.isOk() )
                        stream << "\n";
                    printResultType();
                    printOriginalExpression();
                    printReconstructedExpression();
                }
                else {
                    stream << "\n";
                }
                printMessage();
            }

        private:
            void printResultType() const {
                if( !passOrFail.empty() ) {
                    Colour colourGuard( colour );
                    stream << passOrFail << ":\n";
                }
            }
            void printOriginalExpression() const {
                if( result.hasExpression() ) {
                    Colour colourGuard( Colour::OriginalExpression );
                    stream  << "  ";
                    stream << result.getExpressionInMacro();
                    stream << "\n";
                }
            }
            void printReconstructedExpression() const {
                if( result.hasExpandedExpression() ) {
                    stream << "with expansion:\n";
                    Colour colourGuard( Colour::ReconstructedExpression );
                    stream << Text( result.getExpandedExpression(), TextAttributes().setIndent(2) ) << "\n";
                }
            }
            void printMessage() const {
                if( !messageLabel.empty() )
                    stream << messageLabel << ":" << "\n";
                for( std::vector<MessageInfo>::const_iterator it = messages.begin(), itEnd = messages.end();
                        it != itEnd;
                        ++it ) {

                    if( printInfoMessages || it->type != ResultWas::Info )
                        stream << Text( it->message, TextAttributes().setIndent(2) ) << "\n";
                }
            }
            void printSourceInfo() const {
                Colour colourGuard( Colour::FileName );
                stream << result.getSourceInfo() << ": ";
            }

            std::ostream& stream;
            AssertionStats const& stats;
            AssertionResult const& result;
            Colour::Code colour;
            std::string passOrFail;
            std::string messageLabel;
            std::string message;
            std::vector<MessageInfo> messages;
            bool printInfoMessages;
        };

        void lazyPrint() {

            if( !currentTestRunInfo.used )
                lazyPrintRunInfo();
            if( !currentGroupInfo.used )
                lazyPrintGroupInfo();

            if( !m_headerPrinted ) {
                printTestCaseAndSectionHeader();
                m_headerPrinted = true;
            }
        }
        void lazyPrintRunInfo() {
            stream  << "\n" << getLineOfChars<'~'>() << "\n";
            Colour colour( Colour::SecondaryText );
            stream  << currentTestRunInfo->name
                    << " is a Catch v"  << libraryVersion << " host application.\n"
                    << "Run with -? for options\n\n";

            if( m_config->rngSeed() != 0 )
                stream << "Randomness seeded to: " << m_config->rngSeed() << "\n\n";

            currentTestRunInfo.used = true;
        }
        void lazyPrintGroupInfo() {
            if( !currentGroupInfo->name.empty() && currentGroupInfo->groupsCounts > 1 ) {
                printClosedHeader( "Group: " + currentGroupInfo->name );
                currentGroupInfo.used = true;
            }
        }
        void printTestCaseAndSectionHeader() {
            assert( !m_sectionStack.empty() );
            printOpenHeader( currentTestCaseInfo->name );

            if( m_sectionStack.size() > 1 ) {
                Colour colourGuard( Colour::Headers );

                std::vector<SectionInfo>::const_iterator
                    it = m_sectionStack.begin()+1,
                    itEnd = m_sectionStack.end();
                for( ; it != itEnd; ++it )
                    printHeaderString( it->name, 2 );
            }

            SourceLineInfo lineInfo = m_sectionStack.front().lineInfo;

            if( !lineInfo.empty() ){
                stream << getLineOfChars<'-'>() << "\n";
                Colour colourGuard( Colour::FileName );
                stream << lineInfo << "\n";
            }
            stream << getLineOfChars<'.'>() << "\n" << std::endl;
        }

        void printClosedHeader( std::string const& _name ) {
            printOpenHeader( _name );
            stream << getLineOfChars<'.'>() << "\n";
        }
        void printOpenHeader( std::string const& _name ) {
            stream  << getLineOfChars<'-'>() << "\n";
            {
                Colour colourGuard( Colour::Headers );
                printHeaderString( _name );
            }
        }



        void printHeaderString( std::string const& _string, std::size_t indent = 0 ) {
            std::size_t i = _string.find( ": " );
            if( i != std::string::npos )
                i+=2;
            else
                i = 0;
            stream << Text( _string, TextAttributes()
                                        .setIndent( indent+i)
                                        .setInitialIndent( indent ) ) << "\n";
        }

        struct SummaryColumn {

            SummaryColumn( std::string const& _label, Colour::Code _colour )
            :   label( _label ),
                colour( _colour )
            {}
            SummaryColumn addRow( std::size_t count ) {
                std::ostringstream oss;
                oss << count;
                std::string row = oss.str();
                for( std::vector<std::string>::iterator it = rows.begin(); it != rows.end(); ++it ) {
                    while( it->size() < row.size() )
                        *it = " " + *it;
                    while( it->size() > row.size() )
                        row = " " + row;
                }
                rows.push_back( row );
                return *this;
            }

            std::string label;
            Colour::Code colour;
            std::vector<std::string> rows;

        };

        void printTotals( Totals const& totals ) {
            if( totals.testCases.total() == 0 ) {
                stream << Colour( Colour::Warning ) << "No tests ran\n";
            }
            else if( totals.assertions.total() > 0 && totals.assertions.allPassed() ) {
                stream << Colour( Colour::ResultSuccess ) << "All tests passed";
                stream << " ("
                        << pluralise( totals.assertions.passed, "assertion" ) << " in "
                        << pluralise( totals.testCases.passed, "test case" ) << ")"
                        << "\n";
            }
            else {

                std::vector<SummaryColumn> columns;
                columns.push_back( SummaryColumn( "", Colour::None )
                                        .addRow( totals.testCases.total() )
                                        .addRow( totals.assertions.total() ) );
                columns.push_back( SummaryColumn( "passed", Colour::Success )
                                        .addRow( totals.testCases.passed )
                                        .addRow( totals.assertions.passed ) );
                columns.push_back( SummaryColumn( "failed", Colour::ResultError )
                                        .addRow( totals.testCases.failed )
                                        .addRow( totals.assertions.failed ) );
                columns.push_back( SummaryColumn( "failed as expected", Colour::ResultExpectedFailure )
                                        .addRow( totals.testCases.failedButOk )
                                        .addRow( totals.assertions.failedButOk ) );

                printSummaryRow( "test cases", columns, 0 );
                printSummaryRow( "assertions", columns, 1 );
            }
        }
        void printSummaryRow( std::string const& label, std::vector<SummaryColumn> const& cols, std::size_t row ) {
            for( std::vector<SummaryColumn>::const_iterator it = cols.begin(); it != cols.end(); ++it ) {
                std::string value = it->rows[row];
                if( it->label.empty() ) {
                    stream << label << ": ";
                    if( value != "0" )
                        stream << value;
                    else
                        stream << Colour( Colour::Warning ) << "- none -";
                }
                else if( value != "0" ) {
                    stream  << Colour( Colour::LightGrey ) << " | ";
                    stream  << Colour( it->colour )
                            << value << " " << it->label;
                }
            }
            stream << "\n";
        }

        static std::size_t makeRatio( std::size_t number, std::size_t total ) {
            std::size_t ratio = total > 0 ? CATCH_CONFIG_CONSOLE_WIDTH * number/ total : 0;
            return ( ratio == 0 && number > 0 ) ? 1 : ratio;
        }
        static std::size_t& findMax( std::size_t& i, std::size_t& j, std::size_t& k ) {
            if( i > j && i > k )
                return i;
            else if( j > k )
                return j;
            else
                return k;
        }

        void printTotalsDivider( Totals const& totals ) {
            if( totals.testCases.total() > 0 ) {
                std::size_t failedRatio = makeRatio( totals.testCases.failed, totals.testCases.total() );
                std::size_t failedButOkRatio = makeRatio( totals.testCases.failedButOk, totals.testCases.total() );
                std::size_t passedRatio = makeRatio( totals.testCases.passed, totals.testCases.total() );
                while( failedRatio + failedButOkRatio + passedRatio < CATCH_CONFIG_CONSOLE_WIDTH-1 )
                    findMax( failedRatio, failedButOkRatio, passedRatio )++;
                while( failedRatio + failedButOkRatio + passedRatio > CATCH_CONFIG_CONSOLE_WIDTH-1 )
                    findMax( failedRatio, failedButOkRatio, passedRatio )--;

                stream << Colour( Colour::Error ) << std::string( failedRatio, '=' );
                stream << Colour( Colour::ResultExpectedFailure ) << std::string( failedButOkRatio, '=' );
                if( totals.testCases.allPassed() )
                    stream << Colour( Colour::ResultSuccess ) << std::string( passedRatio, '=' );
                else
                    stream << Colour( Colour::Success ) << std::string( passedRatio, '=' );
            }
            else {
                stream << Colour( Colour::Warning ) << std::string( CATCH_CONFIG_CONSOLE_WIDTH-1, '=' );
            }
            stream << "\n";
        }
        void printSummaryDivider() {
            stream << getLineOfChars<'-'>() << "\n";
        }

    private:
        bool m_headerPrinted;
    };

    INTERNAL_CATCH_REGISTER_REPORTER( "console", ConsoleReporter )

}


#define TWOBLUECUBES_CATCH_REPORTER_COMPACT_HPP_INCLUDED

namespace Catch {

    struct CompactReporter : StreamingReporterBase {

        CompactReporter( ReporterConfig const& _config )
        : StreamingReporterBase( _config )
        {}

        virtual ~CompactReporter();

        static std::string getDescription() {
            return "Reports test results on a single line, suitable for IDEs";
        }

        virtual ReporterPreferences getPreferences() const {
            ReporterPreferences prefs;
            prefs.shouldRedirectStdOut = false;
            return prefs;
        }

        virtual void noMatchingTestCases( std::string const& spec ) {
            stream << "No test cases matched '" << spec << "'" << std::endl;
        }

        virtual void assertionStarting( AssertionInfo const& ) {
        }

        virtual bool assertionEnded( AssertionStats const& _assertionStats ) {
            AssertionResult const& result = _assertionStats.assertionResult;

            bool printInfoMessages = true;


            if( !m_config->includeSuccessfulResults() && result.isOk() ) {
                if( result.getResultType() != ResultWas::Warning )
                    return false;
                printInfoMessages = false;
            }

            AssertionPrinter printer( stream, _assertionStats, printInfoMessages );
            printer.print();

            stream << std::endl;
            return true;
        }

        virtual void testRunEnded( TestRunStats const& _testRunStats ) {
            printTotals( _testRunStats.totals );
            stream << "\n" << std::endl;
            StreamingReporterBase::testRunEnded( _testRunStats );
        }

    private:
        class AssertionPrinter {
            void operator= ( AssertionPrinter const& );
        public:
            AssertionPrinter( std::ostream& _stream, AssertionStats const& _stats, bool _printInfoMessages )
            : stream( _stream )
            , stats( _stats )
            , result( _stats.assertionResult )
            , messages( _stats.infoMessages )
            , itMessage( _stats.infoMessages.begin() )
            , printInfoMessages( _printInfoMessages )
            {}

            void print() {
                printSourceInfo();

                itMessage = messages.begin();

                switch( result.getResultType() ) {
                    case ResultWas::Ok:
                        printResultType( Colour::ResultSuccess, passedString() );
                        printOriginalExpression();
                        printReconstructedExpression();
                        if ( ! result.hasExpression() )
                            printRemainingMessages( Colour::None );
                        else
                            printRemainingMessages();
                        break;
                    case ResultWas::ExpressionFailed:
                        if( result.isOk() )
                            printResultType( Colour::ResultSuccess, failedString() + std::string( " - but was ok" ) );
                        else
                            printResultType( Colour::Error, failedString() );
                        printOriginalExpression();
                        printReconstructedExpression();
                        printRemainingMessages();
                        break;
                    case ResultWas::ThrewException:
                        printResultType( Colour::Error, failedString() );
                        printIssue( "unexpected exception with message:" );
                        printMessage();
                        printExpressionWas();
                        printRemainingMessages();
                        break;
                    case ResultWas::FatalErrorCondition:
                        printResultType( Colour::Error, failedString() );
                        printIssue( "fatal error condition with message:" );
                        printMessage();
                        printExpressionWas();
                        printRemainingMessages();
                        break;
                    case ResultWas::DidntThrowException:
                        printResultType( Colour::Error, failedString() );
                        printIssue( "expected exception, got none" );
                        printExpressionWas();
                        printRemainingMessages();
                        break;
                    case ResultWas::Info:
                        printResultType( Colour::None, "info" );
                        printMessage();
                        printRemainingMessages();
                        break;
                    case ResultWas::Warning:
                        printResultType( Colour::None, "warning" );
                        printMessage();
                        printRemainingMessages();
                        break;
                    case ResultWas::ExplicitFailure:
                        printResultType( Colour::Error, failedString() );
                        printIssue( "explicitly" );
                        printRemainingMessages( Colour::None );
                        break;

                    case ResultWas::Unknown:
                    case ResultWas::FailureBit:
                    case ResultWas::Exception:
                        printResultType( Colour::Error, "** internal error **" );
                        break;
                }
            }

        private:


            static Colour::Code dimColour() { return Colour::FileName; }

#ifdef CATCH_PLATFORM_MAC
            static const char* failedString() { return "FAILED"; }
            static const char* passedString() { return "PASSED"; }
#else
            static const char* failedString() { return "failed"; }
            static const char* passedString() { return "passed"; }
#endif

            void printSourceInfo() const {
                Colour colourGuard( Colour::FileName );
                stream << result.getSourceInfo() << ":";
            }

            void printResultType( Colour::Code colour, std::string passOrFail ) const {
                if( !passOrFail.empty() ) {
                    {
                        Colour colourGuard( colour );
                        stream << " " << passOrFail;
                    }
                    stream << ":";
                }
            }

            void printIssue( std::string issue ) const {
                stream << " " << issue;
            }

            void printExpressionWas() {
                if( result.hasExpression() ) {
                    stream << ";";
                    {
                        Colour colour( dimColour() );
                        stream << " expression was:";
                    }
                    printOriginalExpression();
                }
            }

            void printOriginalExpression() const {
                if( result.hasExpression() ) {
                    stream << " " << result.getExpression();
                }
            }

            void printReconstructedExpression() const {
                if( result.hasExpandedExpression() ) {
                    {
                        Colour colour( dimColour() );
                        stream << " for: ";
                    }
                    stream << result.getExpandedExpression();
                }
            }

            void printMessage() {
                if ( itMessage != messages.end() ) {
                    stream << " '" << itMessage->message << "'";
                    ++itMessage;
                }
            }

            void printRemainingMessages( Colour::Code colour = dimColour() ) {
                if ( itMessage == messages.end() )
                    return;


                std::vector<MessageInfo>::const_iterator itEnd = messages.end();
                const std::size_t N = static_cast<std::size_t>( std::distance( itMessage, itEnd ) );

                {
                    Colour colourGuard( colour );
                    stream << " with " << pluralise( N, "message" ) << ":";
                }

                for(; itMessage != itEnd; ) {

                    if( printInfoMessages || itMessage->type != ResultWas::Info ) {
                        stream << " '" << itMessage->message << "'";
                        if ( ++itMessage != itEnd ) {
                            Colour colourGuard( dimColour() );
                            stream << " and";
                        }
                    }
                }
            }

        private:
            std::ostream& stream;
            AssertionStats const& stats;
            AssertionResult const& result;
            std::vector<MessageInfo> messages;
            std::vector<MessageInfo>::const_iterator itMessage;
            bool printInfoMessages;
        };








        std::string bothOrAll( std::size_t count ) const {
            return count == 1 ? "" : count == 2 ? "both " : "all " ;
        }

        void printTotals( const Totals& totals ) const {
            if( totals.testCases.total() == 0 ) {
                stream << "No tests ran.";
            }
            else if( totals.testCases.failed == totals.testCases.total() ) {
                Colour colour( Colour::ResultError );
                const std::string qualify_assertions_failed =
                    totals.assertions.failed == totals.assertions.total() ?
                        bothOrAll( totals.assertions.failed ) : "";
                stream <<
                    "Failed " << bothOrAll( totals.testCases.failed )
                              << pluralise( totals.testCases.failed, "test case"  ) << ", "
                    "failed " << qualify_assertions_failed <<
                                 pluralise( totals.assertions.failed, "assertion" ) << ".";
            }
            else if( totals.assertions.total() == 0 ) {
                stream <<
                    "Passed " << bothOrAll( totals.testCases.total() )
                              << pluralise( totals.testCases.total(), "test case" )
                              << " (no assertions).";
            }
            else if( totals.assertions.failed ) {
                Colour colour( Colour::ResultError );
                stream <<
                    "Failed " << pluralise( totals.testCases.failed, "test case"  ) << ", "
                    "failed " << pluralise( totals.assertions.failed, "assertion" ) << ".";
            }
            else {
                Colour colour( Colour::ResultSuccess );
                stream <<
                    "Passed " << bothOrAll( totals.testCases.passed )
                              << pluralise( totals.testCases.passed, "test case"  ) <<
                    " with "  << pluralise( totals.assertions.passed, "assertion" ) << ".";
            }
        }
    };

    INTERNAL_CATCH_REGISTER_REPORTER( "compact", CompactReporter )

}

namespace Catch {


    NonCopyable::~NonCopyable() {}
    IShared::~IShared() {}
    IStream::~IStream() CATCH_NOEXCEPT {}
    FileStream::~FileStream() CATCH_NOEXCEPT {}
    CoutStream::~CoutStream() CATCH_NOEXCEPT {}
    DebugOutStream::~DebugOutStream() CATCH_NOEXCEPT {}
    StreamBufBase::~StreamBufBase() CATCH_NOEXCEPT {}
    IContext::~IContext() {}
    IResultCapture::~IResultCapture() {}
    ITestCase::~ITestCase() {}
    ITestCaseRegistry::~ITestCaseRegistry() {}
    IRegistryHub::~IRegistryHub() {}
    IMutableRegistryHub::~IMutableRegistryHub() {}
    IExceptionTranslator::~IExceptionTranslator() {}
    IExceptionTranslatorRegistry::~IExceptionTranslatorRegistry() {}
    IReporter::~IReporter() {}
    IReporterFactory::~IReporterFactory() {}
    IReporterRegistry::~IReporterRegistry() {}
    IStreamingReporter::~IStreamingReporter() {}
    AssertionStats::~AssertionStats() {}
    SectionStats::~SectionStats() {}
    TestCaseStats::~TestCaseStats() {}
    TestGroupStats::~TestGroupStats() {}
    TestRunStats::~TestRunStats() {}
    CumulativeReporterBase::SectionNode::~SectionNode() {}
    CumulativeReporterBase::~CumulativeReporterBase() {}

    StreamingReporterBase::~StreamingReporterBase() {}
    ConsoleReporter::~ConsoleReporter() {}
    CompactReporter::~CompactReporter() {}
    IRunner::~IRunner() {}
    IMutableContext::~IMutableContext() {}
    IConfig::~IConfig() {}
    XmlReporter::~XmlReporter() {}
    JunitReporter::~JunitReporter() {}
    TestRegistry::~TestRegistry() {}
    FreeFunctionTestCase::~FreeFunctionTestCase() {}
    IGeneratorInfo::~IGeneratorInfo() {}
    IGeneratorsForTest::~IGeneratorsForTest() {}
    WildcardPattern::~WildcardPattern() {}
    TestSpec::Pattern::~Pattern() {}
    TestSpec::NamePattern::~NamePattern() {}
    TestSpec::TagPattern::~TagPattern() {}
    TestSpec::ExcludedPattern::~ExcludedPattern() {}

    Matchers::Impl::StdString::Equals::~Equals() {}
    Matchers::Impl::StdString::Contains::~Contains() {}
    Matchers::Impl::StdString::StartsWith::~StartsWith() {}
    Matchers::Impl::StdString::EndsWith::~EndsWith() {}

    void Config::dummy() {}

    namespace TestCaseTracking {
        ITracker::~ITracker() {}
        TrackerBase::~TrackerBase() {}
        SectionTracker::~SectionTracker() {}
        IndexTracker::~IndexTracker() {}
    }
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif

#ifdef CATCH_CONFIG_MAIN

#define TWOBLUECUBES_CATCH_DEFAULT_MAIN_HPP_INCLUDED

#ifndef __OBJC__


int main (int argc, char * argv[]) {
    return Catch::Session().run( argc, argv );
}

#else


int main (int argc, char * const argv[]) {
#if !CATCH_ARC_ENABLED
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
#endif

    Catch::registerTestMethods();
    int result = Catch::Session().run( argc, (char* const*)argv );

#if !CATCH_ARC_ENABLED
    [pool drain];
#endif

    return result;
}

#endif

#endif

#ifdef CLARA_CONFIG_MAIN_NOT_DEFINED
#  undef CLARA_CONFIG_MAIN
#endif




#ifdef CATCH_CONFIG_PREFIX_ALL

#define CATCH_REQUIRE( expr ) INTERNAL_CATCH_TEST( expr, Catch::ResultDisposition::Normal, "CATCH_REQUIRE" )
#define CATCH_REQUIRE_FALSE( expr ) INTERNAL_CATCH_TEST( expr, Catch::ResultDisposition::Normal | Catch::ResultDisposition::FalseTest, "CATCH_REQUIRE_FALSE" )

#define CATCH_REQUIRE_THROWS( expr ) INTERNAL_CATCH_THROWS( expr, Catch::ResultDisposition::Normal, "", "CATCH_REQUIRE_THROWS" )
#define CATCH_REQUIRE_THROWS_AS( expr, exceptionType ) INTERNAL_CATCH_THROWS_AS( expr, exceptionType, Catch::ResultDisposition::Normal, "CATCH_REQUIRE_THROWS_AS" )
#define CATCH_REQUIRE_THROWS_WITH( expr, matcher ) INTERNAL_CATCH_THROWS( expr, Catch::ResultDisposition::Normal, matcher, "CATCH_REQUIRE_THROWS_WITH" )
#define CATCH_REQUIRE_NOTHROW( expr ) INTERNAL_CATCH_NO_THROW( expr, Catch::ResultDisposition::Normal, "CATCH_REQUIRE_NOTHROW" )

#define CATCH_CHECK( expr ) INTERNAL_CATCH_TEST( expr, Catch::ResultDisposition::ContinueOnFailure, "CATCH_CHECK" )
#define CATCH_CHECK_FALSE( expr ) INTERNAL_CATCH_TEST( expr, Catch::ResultDisposition::ContinueOnFailure | Catch::ResultDisposition::FalseTest, "CATCH_CHECK_FALSE" )
#define CATCH_CHECKED_IF( expr ) INTERNAL_CATCH_IF( expr, Catch::ResultDisposition::ContinueOnFailure, "CATCH_CHECKED_IF" )
#define CATCH_CHECKED_ELSE( expr ) INTERNAL_CATCH_ELSE( expr, Catch::ResultDisposition::ContinueOnFailure, "CATCH_CHECKED_ELSE" )
#define CATCH_CHECK_NOFAIL( expr ) INTERNAL_CATCH_TEST( expr, Catch::ResultDisposition::ContinueOnFailure | Catch::ResultDisposition::SuppressFail, "CATCH_CHECK_NOFAIL" )

#define CATCH_CHECK_THROWS( expr )  INTERNAL_CATCH_THROWS( expr, Catch::ResultDisposition::ContinueOnFailure, "CATCH_CHECK_THROWS" )
#define CATCH_CHECK_THROWS_AS( expr, exceptionType ) INTERNAL_CATCH_THROWS_AS( expr, exceptionType, Catch::ResultDisposition::ContinueOnFailure, "CATCH_CHECK_THROWS_AS" )
#define CATCH_CHECK_THROWS_WITH( expr, matcher ) INTERNAL_CATCH_THROWS( expr, Catch::ResultDisposition::ContinueOnFailure, matcher, "CATCH_CHECK_THROWS_WITH" )
#define CATCH_CHECK_NOTHROW( expr ) INTERNAL_CATCH_NO_THROW( expr, Catch::ResultDisposition::ContinueOnFailure, "CATCH_CHECK_NOTHROW" )

#define CHECK_THAT( arg, matcher ) INTERNAL_CHECK_THAT( arg, matcher, Catch::ResultDisposition::ContinueOnFailure, "CATCH_CHECK_THAT" )
#define CATCH_REQUIRE_THAT( arg, matcher ) INTERNAL_CHECK_THAT( arg, matcher, Catch::ResultDisposition::Normal, "CATCH_REQUIRE_THAT" )

#define CATCH_INFO( msg ) INTERNAL_CATCH_INFO( msg, "CATCH_INFO" )
#define CATCH_WARN( msg ) INTERNAL_CATCH_MSG( Catch::ResultWas::Warning, Catch::ResultDisposition::ContinueOnFailure, "CATCH_WARN", msg )
#define CATCH_SCOPED_INFO( msg ) INTERNAL_CATCH_INFO( msg, "CATCH_INFO" )
#define CATCH_CAPTURE( msg ) INTERNAL_CATCH_INFO( #msg " := " << msg, "CATCH_CAPTURE" )
#define CATCH_SCOPED_CAPTURE( msg ) INTERNAL_CATCH_INFO( #msg " := " << msg, "CATCH_CAPTURE" )

#ifdef CATCH_CONFIG_VARIADIC_MACROS
    #define CATCH_TEST_CASE( ... ) INTERNAL_CATCH_TESTCASE( __VA_ARGS__ )
    #define CATCH_TEST_CASE_METHOD( className, ... ) INTERNAL_CATCH_TEST_CASE_METHOD( className, __VA_ARGS__ )
    #define CATCH_METHOD_AS_TEST_CASE( method, ... ) INTERNAL_CATCH_METHOD_AS_TEST_CASE( method, __VA_ARGS__ )
    #define CATCH_REGISTER_TEST_CASE( Function, ... ) INTERNAL_CATCH_REGISTER_TESTCASE( Function, __VA_ARGS__ )
    #define CATCH_SECTION( ... ) INTERNAL_CATCH_SECTION( __VA_ARGS__ )
    #define CATCH_FAIL( ... ) INTERNAL_CATCH_MSG( Catch::ResultWas::ExplicitFailure, Catch::ResultDisposition::Normal, "CATCH_FAIL", __VA_ARGS__ )
    #define CATCH_SUCCEED( ... ) INTERNAL_CATCH_MSG( Catch::ResultWas::Ok, Catch::ResultDisposition::ContinueOnFailure, "CATCH_SUCCEED", __VA_ARGS__ )
#else
    #define CATCH_TEST_CASE( name, description ) INTERNAL_CATCH_TESTCASE( name, description )
    #define CATCH_TEST_CASE_METHOD( className, name, description ) INTERNAL_CATCH_TEST_CASE_METHOD( className, name, description )
    #define CATCH_METHOD_AS_TEST_CASE( method, name, description ) INTERNAL_CATCH_METHOD_AS_TEST_CASE( method, name, description )
    #define CATCH_REGISTER_TEST_CASE( function, name, description ) INTERNAL_CATCH_REGISTER_TESTCASE( function, name, description )
    #define CATCH_SECTION( name, description ) INTERNAL_CATCH_SECTION( name, description )
    #define CATCH_FAIL( msg ) INTERNAL_CATCH_MSG( Catch::ResultWas::ExplicitFailure, Catch::ResultDisposition::Normal, "CATCH_FAIL", msg )
    #define CATCH_SUCCEED( msg ) INTERNAL_CATCH_MSG( Catch::ResultWas::Ok, Catch::ResultDisposition::ContinueOnFailure, "CATCH_SUCCEED", msg )
#endif
#define CATCH_ANON_TEST_CASE() INTERNAL_CATCH_TESTCASE( "", "" )

#define CATCH_REGISTER_REPORTER( name, reporterType ) INTERNAL_CATCH_REGISTER_REPORTER( name, reporterType )
#define CATCH_REGISTER_LEGACY_REPORTER( name, reporterType ) INTERNAL_CATCH_REGISTER_LEGACY_REPORTER( name, reporterType )

#define CATCH_GENERATE( expr) INTERNAL_CATCH_GENERATE( expr )


#ifdef CATCH_CONFIG_VARIADIC_MACROS
#define CATCH_SCENARIO( ... ) CATCH_TEST_CASE( "Scenario: " __VA_ARGS__ )
#define CATCH_SCENARIO_METHOD( className, ... ) INTERNAL_CATCH_TEST_CASE_METHOD( className, "Scenario: " __VA_ARGS__ )
#else
#define CATCH_SCENARIO( name, tags ) CATCH_TEST_CASE( "Scenario: " name, tags )
#define CATCH_SCENARIO_METHOD( className, name, tags ) INTERNAL_CATCH_TEST_CASE_METHOD( className, "Scenario: " name, tags )
#endif
#define CATCH_GIVEN( desc )    CATCH_SECTION( std::string( "Given: ") + desc, "" )
#define CATCH_WHEN( desc )     CATCH_SECTION( std::string( " When: ") + desc, "" )
#define CATCH_AND_WHEN( desc ) CATCH_SECTION( std::string( "  And: ") + desc, "" )
#define CATCH_THEN( desc )     CATCH_SECTION( std::string( " Then: ") + desc, "" )
#define CATCH_AND_THEN( desc ) CATCH_SECTION( std::string( "  And: ") + desc, "" )


#else

#define REQUIRE( expr ) INTERNAL_CATCH_TEST( expr, Catch::ResultDisposition::Normal, "REQUIRE" )
#define REQUIRE_FALSE( expr ) INTERNAL_CATCH_TEST( expr, Catch::ResultDisposition::Normal | Catch::ResultDisposition::FalseTest, "REQUIRE_FALSE" )

#define REQUIRE_THROWS( expr ) INTERNAL_CATCH_THROWS( expr, Catch::ResultDisposition::Normal, "", "REQUIRE_THROWS" )
#define REQUIRE_THROWS_AS( expr, exceptionType ) INTERNAL_CATCH_THROWS_AS( expr, exceptionType, Catch::ResultDisposition::Normal, "REQUIRE_THROWS_AS" )
#define REQUIRE_THROWS_WITH( expr, matcher ) INTERNAL_CATCH_THROWS( expr, Catch::ResultDisposition::Normal, matcher, "REQUIRE_THROWS_WITH" )
#define REQUIRE_NOTHROW( expr ) INTERNAL_CATCH_NO_THROW( expr, Catch::ResultDisposition::Normal, "REQUIRE_NOTHROW" )

#define CHECK( expr ) INTERNAL_CATCH_TEST( expr, Catch::ResultDisposition::ContinueOnFailure, "CHECK" )
#define CHECK_FALSE( expr ) INTERNAL_CATCH_TEST( expr, Catch::ResultDisposition::ContinueOnFailure | Catch::ResultDisposition::FalseTest, "CHECK_FALSE" )
#define CHECKED_IF( expr ) INTERNAL_CATCH_IF( expr, Catch::ResultDisposition::ContinueOnFailure, "CHECKED_IF" )
#define CHECKED_ELSE( expr ) INTERNAL_CATCH_ELSE( expr, Catch::ResultDisposition::ContinueOnFailure, "CHECKED_ELSE" )
#define CHECK_NOFAIL( expr ) INTERNAL_CATCH_TEST( expr, Catch::ResultDisposition::ContinueOnFailure | Catch::ResultDisposition::SuppressFail, "CHECK_NOFAIL" )

#define CHECK_THROWS( expr )  INTERNAL_CATCH_THROWS( expr, Catch::ResultDisposition::ContinueOnFailure, "", "CHECK_THROWS" )
#define CHECK_THROWS_AS( expr, exceptionType ) INTERNAL_CATCH_THROWS_AS( expr, exceptionType, Catch::ResultDisposition::ContinueOnFailure, "CHECK_THROWS_AS" )
#define CHECK_THROWS_WITH( expr, matcher ) INTERNAL_CATCH_THROWS( expr, Catch::ResultDisposition::ContinueOnFailure, matcher, "CHECK_THROWS_WITH" )
#define CHECK_NOTHROW( expr ) INTERNAL_CATCH_NO_THROW( expr, Catch::ResultDisposition::ContinueOnFailure, "CHECK_NOTHROW" )

#define CHECK_THAT( arg, matcher ) INTERNAL_CHECK_THAT( arg, matcher, Catch::ResultDisposition::ContinueOnFailure, "CHECK_THAT" )
#define REQUIRE_THAT( arg, matcher ) INTERNAL_CHECK_THAT( arg, matcher, Catch::ResultDisposition::Normal, "REQUIRE_THAT" )

#define INFO( msg ) INTERNAL_CATCH_INFO( msg, "INFO" )
#define WARN( msg ) INTERNAL_CATCH_MSG( Catch::ResultWas::Warning, Catch::ResultDisposition::ContinueOnFailure, "WARN", msg )
#define SCOPED_INFO( msg ) INTERNAL_CATCH_INFO( msg, "INFO" )
#define CAPTURE( msg ) INTERNAL_CATCH_INFO( #msg " := " << msg, "CAPTURE" )
#define SCOPED_CAPTURE( msg ) INTERNAL_CATCH_INFO( #msg " := " << msg, "CAPTURE" )

#ifdef CATCH_CONFIG_VARIADIC_MACROS
    #define TEST_CASE( ... ) INTERNAL_CATCH_TESTCASE( __VA_ARGS__ )
    #define TEST_CASE_METHOD( className, ... ) INTERNAL_CATCH_TEST_CASE_METHOD( className, __VA_ARGS__ )
    #define METHOD_AS_TEST_CASE( method, ... ) INTERNAL_CATCH_METHOD_AS_TEST_CASE( method, __VA_ARGS__ )
    #define REGISTER_TEST_CASE( Function, ... ) INTERNAL_CATCH_REGISTER_TESTCASE( Function, __VA_ARGS__ )
    #define SECTION( ... ) INTERNAL_CATCH_SECTION( __VA_ARGS__ )
    #define FAIL( ... ) INTERNAL_CATCH_MSG( Catch::ResultWas::ExplicitFailure, Catch::ResultDisposition::Normal, "FAIL", __VA_ARGS__ )
    #define SUCCEED( ... ) INTERNAL_CATCH_MSG( Catch::ResultWas::Ok, Catch::ResultDisposition::ContinueOnFailure, "SUCCEED", __VA_ARGS__ )
#else
    #define TEST_CASE( name, description ) INTERNAL_CATCH_TESTCASE( name, description )
    #define TEST_CASE_METHOD( className, name, description ) INTERNAL_CATCH_TEST_CASE_METHOD( className, name, description )
    #define METHOD_AS_TEST_CASE( method, name, description ) INTERNAL_CATCH_METHOD_AS_TEST_CASE( method, name, description )
    #define REGISTER_TEST_CASE( method, name, description ) INTERNAL_CATCH_REGISTER_TESTCASE( method, name, description )
    #define SECTION( name, description ) INTERNAL_CATCH_SECTION( name, description )
    #define FAIL( msg ) INTERNAL_CATCH_MSG( Catch::ResultWas::ExplicitFailure, Catch::ResultDisposition::Normal, "FAIL", msg )
    #define SUCCEED( msg ) INTERNAL_CATCH_MSG( Catch::ResultWas::Ok, Catch::ResultDisposition::ContinueOnFailure, "SUCCEED", msg )
#endif
#define ANON_TEST_CASE() INTERNAL_CATCH_TESTCASE( "", "" )

#define REGISTER_REPORTER( name, reporterType ) INTERNAL_CATCH_REGISTER_REPORTER( name, reporterType )
#define REGISTER_LEGACY_REPORTER( name, reporterType ) INTERNAL_CATCH_REGISTER_LEGACY_REPORTER( name, reporterType )

#define GENERATE( expr) INTERNAL_CATCH_GENERATE( expr )

#endif

#define CATCH_TRANSLATE_EXCEPTION( signature ) INTERNAL_CATCH_TRANSLATE_EXCEPTION( signature )


#ifdef CATCH_CONFIG_VARIADIC_MACROS
#define SCENARIO( ... ) TEST_CASE( "Scenario: " __VA_ARGS__ )
#define SCENARIO_METHOD( className, ... ) INTERNAL_CATCH_TEST_CASE_METHOD( className, "Scenario: " __VA_ARGS__ )
#else
#define SCENARIO( name, tags ) TEST_CASE( "Scenario: " name, tags )
#define SCENARIO_METHOD( className, name, tags ) INTERNAL_CATCH_TEST_CASE_METHOD( className, "Scenario: " name, tags )
#endif
#define GIVEN( desc )    SECTION( std::string("   Given: ") + desc, "" )
#define WHEN( desc )     SECTION( std::string("    When: ") + desc, "" )
#define AND_WHEN( desc ) SECTION( std::string("And when: ") + desc, "" )
#define THEN( desc )     SECTION( std::string("    Then: ") + desc, "" )
#define AND_THEN( desc ) SECTION( std::string("     And: ") + desc, "" )

using Catch::Detail::Approx;

#endif

namespace fakeit {

    struct VerificationException : public FakeitException {
        virtual ~VerificationException() = default;

        void setFileInfo(std::string file, int line, std::string callingMethod) {
            _file = file;
            _callingMethod = callingMethod;
            _line = line;
        }

        const std::string& file() const {
            return _file;
        }
        int line() const {
            return _line;
        }
        const std::string& callingMethod() const {
            return _callingMethod;
        }

    private:
        std::string _file;
        int _line;
        std::string _callingMethod;
    };

    struct NoMoreInvocationsVerificationException : public VerificationException {

        NoMoreInvocationsVerificationException(std::string format) :
            _format(format) {
        }

        virtual std::string what() const override {
            return _format;
        }
    private:
        std::string _format;
    };

    struct SequenceVerificationException : public VerificationException {
        SequenceVerificationException(const std::string& format) :
            _format(format)
        {
        }

        virtual std::string what() const override {
            return _format;
        }

    private:
        std::string _format;
    };

class CatchAdapter: public EventHandler {
    EventFormatter& _formatter;

    std::string formatLineNumner(std::string file, int num){
#ifndef __GNUG__
        return file + std::string("(") + std::to_string(num) + std::string(")");
#else
        return file + std::string(":")+ std::to_string(num);
#endif
    }

public:

	virtual ~CatchAdapter() = default;
    CatchAdapter(EventFormatter& formatter)
        : _formatter(formatter){}

    virtual void handle(const UnexpectedMethodCallEvent &evt) override {
        std::string format = _formatter.format(evt);
        UnexpectedMethodCallException ex(format);
        throw ex;
    }

    virtual void handle(const SequenceVerificationEvent &evt) override {
        std::string format(formatLineNumner(evt.file(),evt.line()) + ": " + _formatter.format(evt));
        SequenceVerificationException e(format);
        e.setFileInfo(evt.file(), evt.line(), evt.callingMethod());
        throw e;
    }

    virtual void handle(const NoMoreInvocationsVerificationEvent &evt) override {
        std::string format(formatLineNumner(evt.file(), evt.line()) + ": " + _formatter.format(evt));
        NoMoreInvocationsVerificationException e(format);
        e.setFileInfo(evt.file(), evt.line(), evt.callingMethod());
        throw e;
    }

};


class CatchFakeit: public DefaultFakeit {


public:

	virtual ~CatchFakeit() = default;

	CatchFakeit() : _formatter(), _catchAdapter(_formatter) {}

	static CatchFakeit &getInstance() {
		static CatchFakeit instance;
		return instance;
	}

protected:

	fakeit::EventHandler &accessTestingFrameworkAdapter() override {
		return _catchAdapter;
	}

	EventFormatter &accessEventFormatter() override {
		return _formatter;
	}

private:

	DefaultEventFormatter _formatter;
	CatchAdapter _catchAdapter;
};

}

CATCH_TRANSLATE_EXCEPTION(fakeit::UnexpectedMethodCallException& ex) {
    return ex.what();
}

CATCH_TRANSLATE_EXCEPTION(fakeit::SequenceVerificationException& ex) {
    return ex.what();
}

CATCH_TRANSLATE_EXCEPTION(fakeit::NoMoreInvocationsVerificationException& ex) {
    return ex.what();
}

static fakeit::DefaultFakeit& Fakeit = fakeit::CatchFakeit::getInstance();


#include <type_traits>
#include <unordered_set>

#include <memory>
#include <functional>
#include <type_traits>
#include <vector>
#include <array>
#include <new>

#include <functional>
#include <type_traits>
namespace fakeit {

    struct VirtualOffsetSelector {

        unsigned int offset;

        virtual unsigned int offset0(int) {
            return offset = 0;
        }

        virtual unsigned int offset1(int) {
            return offset = 1;
        }

        virtual unsigned int offset2(int) {
            return offset = 2;
        }

        virtual unsigned int offset3(int) {
            return offset = 3;
        }

        virtual unsigned int offset4(int) {
            return offset = 4;
        }

        virtual unsigned int offset5(int) {
            return offset = 5;
        }

        virtual unsigned int offset6(int) {
            return offset = 6;
        }

        virtual unsigned int offset7(int) {
            return offset = 7;
        }

        virtual unsigned int offset8(int) {
            return offset = 8;
        }

        virtual unsigned int offset9(int) {
            return offset = 9;
        }

        virtual unsigned int offset10(int) {
            return offset = 10;
        }

        virtual unsigned int offset11(int) {
            return offset = 11;
        }

        virtual unsigned int offset12(int) {
            return offset = 12;
        }

        virtual unsigned int offset13(int) {
            return offset = 13;
        }

        virtual unsigned int offset14(int) {
            return offset = 14;
        }

        virtual unsigned int offset15(int) {
            return offset = 15;
        }

        virtual unsigned int offset16(int) {
            return offset = 16;
        }

        virtual unsigned int offset17(int) {
            return offset = 17;
        }

        virtual unsigned int offset18(int) {
            return offset = 18;
        }

        virtual unsigned int offset19(int) {
            return offset = 19;
        }

        virtual unsigned int offset20(int) {
            return offset = 20;
        }

        virtual unsigned int offset21(int) {
            return offset = 21;
        }

        virtual unsigned int offset22(int) {
            return offset = 22;
        }

        virtual unsigned int offset23(int) {
            return offset = 23;
        }

        virtual unsigned int offset24(int) {
            return offset = 24;
        }

        virtual unsigned int offset25(int) {
            return offset = 25;
        }

        virtual unsigned int offset26(int) {
            return offset = 26;
        }

        virtual unsigned int offset27(int) {
            return offset = 27;
        }

        virtual unsigned int offset28(int) {
            return offset = 28;
        }

        virtual unsigned int offset29(int) {
            return offset = 29;
        }

        virtual unsigned int offset30(int) {
            return offset = 30;
        }

        virtual unsigned int offset31(int) {
            return offset = 31;
        }

        virtual unsigned int offset32(int) {
            return offset = 32;
        }

        virtual unsigned int offset33(int) {
            return offset = 33;
        }

        virtual unsigned int offset34(int) {
            return offset = 34;
        }

        virtual unsigned int offset35(int) {
            return offset = 35;
        }

        virtual unsigned int offset36(int) {
            return offset = 36;
        }

        virtual unsigned int offset37(int) {
            return offset = 37;
        }

        virtual unsigned int offset38(int) {
            return offset = 38;
        }

        virtual unsigned int offset39(int) {
            return offset = 39;
        }

        virtual unsigned int offset40(int) {
            return offset = 40;
        }

        virtual unsigned int offset41(int) {
            return offset = 41;
        }

        virtual unsigned int offset42(int) {
            return offset = 42;
        }

        virtual unsigned int offset43(int) {
            return offset = 43;
        }

        virtual unsigned int offset44(int) {
            return offset = 44;
        }

        virtual unsigned int offset45(int) {
            return offset = 45;
        }

        virtual unsigned int offset46(int) {
            return offset = 46;
        }

        virtual unsigned int offset47(int) {
            return offset = 47;
        }

        virtual unsigned int offset48(int) {
            return offset = 48;
        }

        virtual unsigned int offset49(int) {
            return offset = 49;
        }

        virtual unsigned int offset50(int) {
            return offset = 50;
        }

        virtual unsigned int offset51(int) {
            return offset = 51;
        }

        virtual unsigned int offset52(int) {
            return offset = 52;
        }

        virtual unsigned int offset53(int) {
            return offset = 53;
        }

        virtual unsigned int offset54(int) {
            return offset = 54;
        }

        virtual unsigned int offset55(int) {
            return offset = 55;
        }

        virtual unsigned int offset56(int) {
            return offset = 56;
        }

        virtual unsigned int offset57(int) {
            return offset = 57;
        }

        virtual unsigned int offset58(int) {
            return offset = 58;
        }

        virtual unsigned int offset59(int) {
            return offset = 59;
        }

        virtual unsigned int offset60(int) {
            return offset = 60;
        }

        virtual unsigned int offset61(int) {
            return offset = 61;
        }

        virtual unsigned int offset62(int) {
            return offset = 62;
        }

        virtual unsigned int offset63(int) {
            return offset = 63;
        }

        virtual unsigned int offset64(int) {
            return offset = 64;
        }

        virtual unsigned int offset65(int) {
            return offset = 65;
        }

        virtual unsigned int offset66(int) {
            return offset = 66;
        }

        virtual unsigned int offset67(int) {
            return offset = 67;
        }

        virtual unsigned int offset68(int) {
            return offset = 68;
        }

        virtual unsigned int offset69(int) {
            return offset = 69;
        }

        virtual unsigned int offset70(int) {
            return offset = 70;
        }

        virtual unsigned int offset71(int) {
            return offset = 71;
        }

        virtual unsigned int offset72(int) {
            return offset = 72;
        }

        virtual unsigned int offset73(int) {
            return offset = 73;
        }

        virtual unsigned int offset74(int) {
            return offset = 74;
        }

        virtual unsigned int offset75(int) {
            return offset = 75;
        }

        virtual unsigned int offset76(int) {
            return offset = 76;
        }

        virtual unsigned int offset77(int) {
            return offset = 77;
        }

        virtual unsigned int offset78(int) {
            return offset = 78;
        }

        virtual unsigned int offset79(int) {
            return offset = 79;
        }

        virtual unsigned int offset80(int) {
            return offset = 80;
        }

        virtual unsigned int offset81(int) {
            return offset = 81;
        }

        virtual unsigned int offset82(int) {
            return offset = 82;
        }

        virtual unsigned int offset83(int) {
            return offset = 83;
        }

        virtual unsigned int offset84(int) {
            return offset = 84;
        }

        virtual unsigned int offset85(int) {
            return offset = 85;
        }

        virtual unsigned int offset86(int) {
            return offset = 86;
        }

        virtual unsigned int offset87(int) {
            return offset = 87;
        }

        virtual unsigned int offset88(int) {
            return offset = 88;
        }

        virtual unsigned int offset89(int) {
            return offset = 89;
        }

        virtual unsigned int offset90(int) {
            return offset = 90;
        }

        virtual unsigned int offset91(int) {
            return offset = 91;
        }

        virtual unsigned int offset92(int) {
            return offset = 92;
        }

        virtual unsigned int offset93(int) {
            return offset = 93;
        }

        virtual unsigned int offset94(int) {
            return offset = 94;
        }

        virtual unsigned int offset95(int) {
            return offset = 95;
        }

        virtual unsigned int offset96(int) {
            return offset = 96;
        }

        virtual unsigned int offset97(int) {
            return offset = 97;
        }

        virtual unsigned int offset98(int) {
            return offset = 98;
        }

        virtual unsigned int offset99(int) {
            return offset = 99;
        }

        virtual unsigned int offset100(int) {
            return offset = 100;
        }

        virtual unsigned int offset101(int) {
            return offset = 101;
        }

        virtual unsigned int offset102(int) {
            return offset = 102;
        }

        virtual unsigned int offset103(int) {
            return offset = 103;
        }

        virtual unsigned int offset104(int) {
            return offset = 104;
        }

        virtual unsigned int offset105(int) {
            return offset = 105;
        }

        virtual unsigned int offset106(int) {
            return offset = 106;
        }

        virtual unsigned int offset107(int) {
            return offset = 107;
        }

        virtual unsigned int offset108(int) {
            return offset = 108;
        }

        virtual unsigned int offset109(int) {
            return offset = 109;
        }

        virtual unsigned int offset110(int) {
            return offset = 110;
        }

        virtual unsigned int offset111(int) {
            return offset = 111;
        }

        virtual unsigned int offset112(int) {
            return offset = 112;
        }

        virtual unsigned int offset113(int) {
            return offset = 113;
        }

        virtual unsigned int offset114(int) {
            return offset = 114;
        }

        virtual unsigned int offset115(int) {
            return offset = 115;
        }

        virtual unsigned int offset116(int) {
            return offset = 116;
        }

        virtual unsigned int offset117(int) {
            return offset = 117;
        }

        virtual unsigned int offset118(int) {
            return offset = 118;
        }

        virtual unsigned int offset119(int) {
            return offset = 119;
        }

        virtual unsigned int offset120(int) {
            return offset = 120;
        }

        virtual unsigned int offset121(int) {
            return offset = 121;
        }

        virtual unsigned int offset122(int) {
            return offset = 122;
        }

        virtual unsigned int offset123(int) {
            return offset = 123;
        }

        virtual unsigned int offset124(int) {
            return offset = 124;
        }

        virtual unsigned int offset125(int) {
            return offset = 125;
        }

        virtual unsigned int offset126(int) {
            return offset = 126;
        }

        virtual unsigned int offset127(int) {
            return offset = 127;
        }

        virtual unsigned int offset128(int) {
            return offset = 128;
        }

        virtual unsigned int offset129(int) {
            return offset = 129;
        }

        virtual unsigned int offset130(int) {
            return offset = 130;
        }

        virtual unsigned int offset131(int) {
            return offset = 131;
        }

        virtual unsigned int offset132(int) {
            return offset = 132;
        }

        virtual unsigned int offset133(int) {
            return offset = 133;
        }

        virtual unsigned int offset134(int) {
            return offset = 134;
        }

        virtual unsigned int offset135(int) {
            return offset = 135;
        }

        virtual unsigned int offset136(int) {
            return offset = 136;
        }

        virtual unsigned int offset137(int) {
            return offset = 137;
        }

        virtual unsigned int offset138(int) {
            return offset = 138;
        }

        virtual unsigned int offset139(int) {
            return offset = 139;
        }

        virtual unsigned int offset140(int) {
            return offset = 140;
        }

        virtual unsigned int offset141(int) {
            return offset = 141;
        }

        virtual unsigned int offset142(int) {
            return offset = 142;
        }

        virtual unsigned int offset143(int) {
            return offset = 143;
        }

        virtual unsigned int offset144(int) {
            return offset = 144;
        }

        virtual unsigned int offset145(int) {
            return offset = 145;
        }

        virtual unsigned int offset146(int) {
            return offset = 146;
        }

        virtual unsigned int offset147(int) {
            return offset = 147;
        }

        virtual unsigned int offset148(int) {
            return offset = 148;
        }

        virtual unsigned int offset149(int) {
            return offset = 149;
        }

        virtual unsigned int offset150(int) {
            return offset = 150;
        }

        virtual unsigned int offset151(int) {
            return offset = 151;
        }

        virtual unsigned int offset152(int) {
            return offset = 152;
        }

        virtual unsigned int offset153(int) {
            return offset = 153;
        }

        virtual unsigned int offset154(int) {
            return offset = 154;
        }

        virtual unsigned int offset155(int) {
            return offset = 155;
        }

        virtual unsigned int offset156(int) {
            return offset = 156;
        }

        virtual unsigned int offset157(int) {
            return offset = 157;
        }

        virtual unsigned int offset158(int) {
            return offset = 158;
        }

        virtual unsigned int offset159(int) {
            return offset = 159;
        }

        virtual unsigned int offset160(int) {
            return offset = 160;
        }

        virtual unsigned int offset161(int) {
            return offset = 161;
        }

        virtual unsigned int offset162(int) {
            return offset = 162;
        }

        virtual unsigned int offset163(int) {
            return offset = 163;
        }

        virtual unsigned int offset164(int) {
            return offset = 164;
        }

        virtual unsigned int offset165(int) {
            return offset = 165;
        }

        virtual unsigned int offset166(int) {
            return offset = 166;
        }

        virtual unsigned int offset167(int) {
            return offset = 167;
        }

        virtual unsigned int offset168(int) {
            return offset = 168;
        }

        virtual unsigned int offset169(int) {
            return offset = 169;
        }

        virtual unsigned int offset170(int) {
            return offset = 170;
        }

        virtual unsigned int offset171(int) {
            return offset = 171;
        }

        virtual unsigned int offset172(int) {
            return offset = 172;
        }

        virtual unsigned int offset173(int) {
            return offset = 173;
        }

        virtual unsigned int offset174(int) {
            return offset = 174;
        }

        virtual unsigned int offset175(int) {
            return offset = 175;
        }

        virtual unsigned int offset176(int) {
            return offset = 176;
        }

        virtual unsigned int offset177(int) {
            return offset = 177;
        }

        virtual unsigned int offset178(int) {
            return offset = 178;
        }

        virtual unsigned int offset179(int) {
            return offset = 179;
        }

        virtual unsigned int offset180(int) {
            return offset = 180;
        }

        virtual unsigned int offset181(int) {
            return offset = 181;
        }

        virtual unsigned int offset182(int) {
            return offset = 182;
        }

        virtual unsigned int offset183(int) {
            return offset = 183;
        }

        virtual unsigned int offset184(int) {
            return offset = 184;
        }

        virtual unsigned int offset185(int) {
            return offset = 185;
        }

        virtual unsigned int offset186(int) {
            return offset = 186;
        }

        virtual unsigned int offset187(int) {
            return offset = 187;
        }

        virtual unsigned int offset188(int) {
            return offset = 188;
        }

        virtual unsigned int offset189(int) {
            return offset = 189;
        }

        virtual unsigned int offset190(int) {
            return offset = 190;
        }

        virtual unsigned int offset191(int) {
            return offset = 191;
        }

        virtual unsigned int offset192(int) {
            return offset = 192;
        }

        virtual unsigned int offset193(int) {
            return offset = 193;
        }

        virtual unsigned int offset194(int) {
            return offset = 194;
        }

        virtual unsigned int offset195(int) {
            return offset = 195;
        }

        virtual unsigned int offset196(int) {
            return offset = 196;
        }

        virtual unsigned int offset197(int) {
            return offset = 197;
        }

        virtual unsigned int offset198(int) {
            return offset = 198;
        }

        virtual unsigned int offset199(int) {
            return offset = 199;
        }


        virtual unsigned int offset200(int) {
            return offset = 200;
        }

        virtual unsigned int offset201(int) {
            return offset = 201;
        }

        virtual unsigned int offset202(int) {
            return offset = 202;
        }

        virtual unsigned int offset203(int) {
            return offset = 203;
        }

        virtual unsigned int offset204(int) {
            return offset = 204;
        }

        virtual unsigned int offset205(int) {
            return offset = 205;
        }

        virtual unsigned int offset206(int) {
            return offset = 206;
        }

        virtual unsigned int offset207(int) {
            return offset = 207;
        }

        virtual unsigned int offset208(int) {
            return offset = 208;
        }

        virtual unsigned int offset209(int) {
            return offset = 209;
        }

        virtual unsigned int offset210(int) {
            return offset = 210;
        }

        virtual unsigned int offset211(int) {
            return offset = 211;
        }

        virtual unsigned int offset212(int) {
            return offset = 212;
        }

        virtual unsigned int offset213(int) {
            return offset = 213;
        }

        virtual unsigned int offset214(int) {
            return offset = 214;
        }

        virtual unsigned int offset215(int) {
            return offset = 215;
        }

        virtual unsigned int offset216(int) {
            return offset = 216;
        }

        virtual unsigned int offset217(int) {
            return offset = 217;
        }

        virtual unsigned int offset218(int) {
            return offset = 218;
        }

        virtual unsigned int offset219(int) {
            return offset = 219;
        }

        virtual unsigned int offset220(int) {
            return offset = 220;
        }

        virtual unsigned int offset221(int) {
            return offset = 221;
        }

        virtual unsigned int offset222(int) {
            return offset = 222;
        }

        virtual unsigned int offset223(int) {
            return offset = 223;
        }

        virtual unsigned int offset224(int) {
            return offset = 224;
        }

        virtual unsigned int offset225(int) {
            return offset = 225;
        }

        virtual unsigned int offset226(int) {
            return offset = 226;
        }

        virtual unsigned int offset227(int) {
            return offset = 227;
        }

        virtual unsigned int offset228(int) {
            return offset = 228;
        }

        virtual unsigned int offset229(int) {
            return offset = 229;
        }

        virtual unsigned int offset230(int) {
            return offset = 230;
        }

        virtual unsigned int offset231(int) {
            return offset = 231;
        }

        virtual unsigned int offset232(int) {
            return offset = 232;
        }

        virtual unsigned int offset233(int) {
            return offset = 233;
        }

        virtual unsigned int offset234(int) {
            return offset = 234;
        }

        virtual unsigned int offset235(int) {
            return offset = 235;
        }

        virtual unsigned int offset236(int) {
            return offset = 236;
        }

        virtual unsigned int offset237(int) {
            return offset = 237;
        }

        virtual unsigned int offset238(int) {
            return offset = 238;
        }

        virtual unsigned int offset239(int) {
            return offset = 239;
        }

        virtual unsigned int offset240(int) {
            return offset = 240;
        }

        virtual unsigned int offset241(int) {
            return offset = 241;
        }

        virtual unsigned int offset242(int) {
            return offset = 242;
        }

        virtual unsigned int offset243(int) {
            return offset = 243;
        }

        virtual unsigned int offset244(int) {
            return offset = 244;
        }

        virtual unsigned int offset245(int) {
            return offset = 245;
        }

        virtual unsigned int offset246(int) {
            return offset = 246;
        }

        virtual unsigned int offset247(int) {
            return offset = 247;
        }

        virtual unsigned int offset248(int) {
            return offset = 248;
        }

        virtual unsigned int offset249(int) {
            return offset = 249;
        }

        virtual unsigned int offset250(int) {
            return offset = 250;
        }

        virtual unsigned int offset251(int) {
            return offset = 251;
        }

        virtual unsigned int offset252(int) {
            return offset = 252;
        }

        virtual unsigned int offset253(int) {
            return offset = 253;
        }

        virtual unsigned int offset254(int) {
            return offset = 254;
        }

        virtual unsigned int offset255(int) {
            return offset = 255;
        }

        virtual unsigned int offset256(int) {
            return offset = 256;
        }

        virtual unsigned int offset257(int) {
            return offset = 257;
        }

        virtual unsigned int offset258(int) {
            return offset = 258;
        }

        virtual unsigned int offset259(int) {
            return offset = 259;
        }

        virtual unsigned int offset260(int) {
            return offset = 260;
        }

        virtual unsigned int offset261(int) {
            return offset = 261;
        }

        virtual unsigned int offset262(int) {
            return offset = 262;
        }

        virtual unsigned int offset263(int) {
            return offset = 263;
        }

        virtual unsigned int offset264(int) {
            return offset = 264;
        }

        virtual unsigned int offset265(int) {
            return offset = 265;
        }

        virtual unsigned int offset266(int) {
            return offset = 266;
        }

        virtual unsigned int offset267(int) {
            return offset = 267;
        }

        virtual unsigned int offset268(int) {
            return offset = 268;
        }

        virtual unsigned int offset269(int) {
            return offset = 269;
        }

        virtual unsigned int offset270(int) {
            return offset = 270;
        }

        virtual unsigned int offset271(int) {
            return offset = 271;
        }

        virtual unsigned int offset272(int) {
            return offset = 272;
        }

        virtual unsigned int offset273(int) {
            return offset = 273;
        }

        virtual unsigned int offset274(int) {
            return offset = 274;
        }

        virtual unsigned int offset275(int) {
            return offset = 275;
        }

        virtual unsigned int offset276(int) {
            return offset = 276;
        }

        virtual unsigned int offset277(int) {
            return offset = 277;
        }

        virtual unsigned int offset278(int) {
            return offset = 278;
        }

        virtual unsigned int offset279(int) {
            return offset = 279;
        }

        virtual unsigned int offset280(int) {
            return offset = 280;
        }

        virtual unsigned int offset281(int) {
            return offset = 281;
        }

        virtual unsigned int offset282(int) {
            return offset = 282;
        }

        virtual unsigned int offset283(int) {
            return offset = 283;
        }

        virtual unsigned int offset284(int) {
            return offset = 284;
        }

        virtual unsigned int offset285(int) {
            return offset = 285;
        }

        virtual unsigned int offset286(int) {
            return offset = 286;
        }

        virtual unsigned int offset287(int) {
            return offset = 287;
        }

        virtual unsigned int offset288(int) {
            return offset = 288;
        }

        virtual unsigned int offset289(int) {
            return offset = 289;
        }

        virtual unsigned int offset290(int) {
            return offset = 290;
        }

        virtual unsigned int offset291(int) {
            return offset = 291;
        }

        virtual unsigned int offset292(int) {
            return offset = 292;
        }

        virtual unsigned int offset293(int) {
            return offset = 293;
        }

        virtual unsigned int offset294(int) {
            return offset = 294;
        }

        virtual unsigned int offset295(int) {
            return offset = 295;
        }

        virtual unsigned int offset296(int) {
            return offset = 296;
        }

        virtual unsigned int offset297(int) {
            return offset = 297;
        }

        virtual unsigned int offset298(int) {
            return offset = 298;
        }

        virtual unsigned int offset299(int) {
            return offset = 299;
        }


        virtual unsigned int offset300(int) {
            return offset = 300;
        }

        virtual unsigned int offset301(int) {
            return offset = 301;
        }

        virtual unsigned int offset302(int) {
            return offset = 302;
        }

        virtual unsigned int offset303(int) {
            return offset = 303;
        }

        virtual unsigned int offset304(int) {
            return offset = 304;
        }

        virtual unsigned int offset305(int) {
            return offset = 305;
        }

        virtual unsigned int offset306(int) {
            return offset = 306;
        }

        virtual unsigned int offset307(int) {
            return offset = 307;
        }

        virtual unsigned int offset308(int) {
            return offset = 308;
        }

        virtual unsigned int offset309(int) {
            return offset = 309;
        }

        virtual unsigned int offset310(int) {
            return offset = 310;
        }

        virtual unsigned int offset311(int) {
            return offset = 311;
        }

        virtual unsigned int offset312(int) {
            return offset = 312;
        }

        virtual unsigned int offset313(int) {
            return offset = 313;
        }

        virtual unsigned int offset314(int) {
            return offset = 314;
        }

        virtual unsigned int offset315(int) {
            return offset = 315;
        }

        virtual unsigned int offset316(int) {
            return offset = 316;
        }

        virtual unsigned int offset317(int) {
            return offset = 317;
        }

        virtual unsigned int offset318(int) {
            return offset = 318;
        }

        virtual unsigned int offset319(int) {
            return offset = 319;
        }

        virtual unsigned int offset320(int) {
            return offset = 320;
        }

        virtual unsigned int offset321(int) {
            return offset = 321;
        }

        virtual unsigned int offset322(int) {
            return offset = 322;
        }

        virtual unsigned int offset323(int) {
            return offset = 323;
        }

        virtual unsigned int offset324(int) {
            return offset = 324;
        }

        virtual unsigned int offset325(int) {
            return offset = 325;
        }

        virtual unsigned int offset326(int) {
            return offset = 326;
        }

        virtual unsigned int offset327(int) {
            return offset = 327;
        }

        virtual unsigned int offset328(int) {
            return offset = 328;
        }

        virtual unsigned int offset329(int) {
            return offset = 329;
        }

        virtual unsigned int offset330(int) {
            return offset = 330;
        }

        virtual unsigned int offset331(int) {
            return offset = 331;
        }

        virtual unsigned int offset332(int) {
            return offset = 332;
        }

        virtual unsigned int offset333(int) {
            return offset = 333;
        }

        virtual unsigned int offset334(int) {
            return offset = 334;
        }

        virtual unsigned int offset335(int) {
            return offset = 335;
        }

        virtual unsigned int offset336(int) {
            return offset = 336;
        }

        virtual unsigned int offset337(int) {
            return offset = 337;
        }

        virtual unsigned int offset338(int) {
            return offset = 338;
        }

        virtual unsigned int offset339(int) {
            return offset = 339;
        }

        virtual unsigned int offset340(int) {
            return offset = 340;
        }

        virtual unsigned int offset341(int) {
            return offset = 341;
        }

        virtual unsigned int offset342(int) {
            return offset = 342;
        }

        virtual unsigned int offset343(int) {
            return offset = 343;
        }

        virtual unsigned int offset344(int) {
            return offset = 344;
        }

        virtual unsigned int offset345(int) {
            return offset = 345;
        }

        virtual unsigned int offset346(int) {
            return offset = 346;
        }

        virtual unsigned int offset347(int) {
            return offset = 347;
        }

        virtual unsigned int offset348(int) {
            return offset = 348;
        }

        virtual unsigned int offset349(int) {
            return offset = 349;
        }

        virtual unsigned int offset350(int) {
            return offset = 350;
        }

        virtual unsigned int offset351(int) {
            return offset = 351;
        }

        virtual unsigned int offset352(int) {
            return offset = 352;
        }

        virtual unsigned int offset353(int) {
            return offset = 353;
        }

        virtual unsigned int offset354(int) {
            return offset = 354;
        }

        virtual unsigned int offset355(int) {
            return offset = 355;
        }

        virtual unsigned int offset356(int) {
            return offset = 356;
        }

        virtual unsigned int offset357(int) {
            return offset = 357;
        }

        virtual unsigned int offset358(int) {
            return offset = 358;
        }

        virtual unsigned int offset359(int) {
            return offset = 359;
        }

        virtual unsigned int offset360(int) {
            return offset = 360;
        }

        virtual unsigned int offset361(int) {
            return offset = 361;
        }

        virtual unsigned int offset362(int) {
            return offset = 362;
        }

        virtual unsigned int offset363(int) {
            return offset = 363;
        }

        virtual unsigned int offset364(int) {
            return offset = 364;
        }

        virtual unsigned int offset365(int) {
            return offset = 365;
        }

        virtual unsigned int offset366(int) {
            return offset = 366;
        }

        virtual unsigned int offset367(int) {
            return offset = 367;
        }

        virtual unsigned int offset368(int) {
            return offset = 368;
        }

        virtual unsigned int offset369(int) {
            return offset = 369;
        }

        virtual unsigned int offset370(int) {
            return offset = 370;
        }

        virtual unsigned int offset371(int) {
            return offset = 371;
        }

        virtual unsigned int offset372(int) {
            return offset = 372;
        }

        virtual unsigned int offset373(int) {
            return offset = 373;
        }

        virtual unsigned int offset374(int) {
            return offset = 374;
        }

        virtual unsigned int offset375(int) {
            return offset = 375;
        }

        virtual unsigned int offset376(int) {
            return offset = 376;
        }

        virtual unsigned int offset377(int) {
            return offset = 377;
        }

        virtual unsigned int offset378(int) {
            return offset = 378;
        }

        virtual unsigned int offset379(int) {
            return offset = 379;
        }

        virtual unsigned int offset380(int) {
            return offset = 380;
        }

        virtual unsigned int offset381(int) {
            return offset = 381;
        }

        virtual unsigned int offset382(int) {
            return offset = 382;
        }

        virtual unsigned int offset383(int) {
            return offset = 383;
        }

        virtual unsigned int offset384(int) {
            return offset = 384;
        }

        virtual unsigned int offset385(int) {
            return offset = 385;
        }

        virtual unsigned int offset386(int) {
            return offset = 386;
        }

        virtual unsigned int offset387(int) {
            return offset = 387;
        }

        virtual unsigned int offset388(int) {
            return offset = 388;
        }

        virtual unsigned int offset389(int) {
            return offset = 389;
        }

        virtual unsigned int offset390(int) {
            return offset = 390;
        }

        virtual unsigned int offset391(int) {
            return offset = 391;
        }

        virtual unsigned int offset392(int) {
            return offset = 392;
        }

        virtual unsigned int offset393(int) {
            return offset = 393;
        }

        virtual unsigned int offset394(int) {
            return offset = 394;
        }

        virtual unsigned int offset395(int) {
            return offset = 395;
        }

        virtual unsigned int offset396(int) {
            return offset = 396;
        }

        virtual unsigned int offset397(int) {
            return offset = 397;
        }

        virtual unsigned int offset398(int) {
            return offset = 398;
        }

        virtual unsigned int offset399(int) {
            return offset = 399;
        }


        virtual unsigned int offset400(int) {
            return offset = 400;
        }

        virtual unsigned int offset401(int) {
            return offset = 401;
        }

        virtual unsigned int offset402(int) {
            return offset = 402;
        }

        virtual unsigned int offset403(int) {
            return offset = 403;
        }

        virtual unsigned int offset404(int) {
            return offset = 404;
        }

        virtual unsigned int offset405(int) {
            return offset = 405;
        }

        virtual unsigned int offset406(int) {
            return offset = 406;
        }

        virtual unsigned int offset407(int) {
            return offset = 407;
        }

        virtual unsigned int offset408(int) {
            return offset = 408;
        }

        virtual unsigned int offset409(int) {
            return offset = 409;
        }

        virtual unsigned int offset410(int) {
            return offset = 410;
        }

        virtual unsigned int offset411(int) {
            return offset = 411;
        }

        virtual unsigned int offset412(int) {
            return offset = 412;
        }

        virtual unsigned int offset413(int) {
            return offset = 413;
        }

        virtual unsigned int offset414(int) {
            return offset = 414;
        }

        virtual unsigned int offset415(int) {
            return offset = 415;
        }

        virtual unsigned int offset416(int) {
            return offset = 416;
        }

        virtual unsigned int offset417(int) {
            return offset = 417;
        }

        virtual unsigned int offset418(int) {
            return offset = 418;
        }

        virtual unsigned int offset419(int) {
            return offset = 419;
        }

        virtual unsigned int offset420(int) {
            return offset = 420;
        }

        virtual unsigned int offset421(int) {
            return offset = 421;
        }

        virtual unsigned int offset422(int) {
            return offset = 422;
        }

        virtual unsigned int offset423(int) {
            return offset = 423;
        }

        virtual unsigned int offset424(int) {
            return offset = 424;
        }

        virtual unsigned int offset425(int) {
            return offset = 425;
        }

        virtual unsigned int offset426(int) {
            return offset = 426;
        }

        virtual unsigned int offset427(int) {
            return offset = 427;
        }

        virtual unsigned int offset428(int) {
            return offset = 428;
        }

        virtual unsigned int offset429(int) {
            return offset = 429;
        }

        virtual unsigned int offset430(int) {
            return offset = 430;
        }

        virtual unsigned int offset431(int) {
            return offset = 431;
        }

        virtual unsigned int offset432(int) {
            return offset = 432;
        }

        virtual unsigned int offset433(int) {
            return offset = 433;
        }

        virtual unsigned int offset434(int) {
            return offset = 434;
        }

        virtual unsigned int offset435(int) {
            return offset = 435;
        }

        virtual unsigned int offset436(int) {
            return offset = 436;
        }

        virtual unsigned int offset437(int) {
            return offset = 437;
        }

        virtual unsigned int offset438(int) {
            return offset = 438;
        }

        virtual unsigned int offset439(int) {
            return offset = 439;
        }

        virtual unsigned int offset440(int) {
            return offset = 440;
        }

        virtual unsigned int offset441(int) {
            return offset = 441;
        }

        virtual unsigned int offset442(int) {
            return offset = 442;
        }

        virtual unsigned int offset443(int) {
            return offset = 443;
        }

        virtual unsigned int offset444(int) {
            return offset = 444;
        }

        virtual unsigned int offset445(int) {
            return offset = 445;
        }

        virtual unsigned int offset446(int) {
            return offset = 446;
        }

        virtual unsigned int offset447(int) {
            return offset = 447;
        }

        virtual unsigned int offset448(int) {
            return offset = 448;
        }

        virtual unsigned int offset449(int) {
            return offset = 449;
        }

        virtual unsigned int offset450(int) {
            return offset = 450;
        }

        virtual unsigned int offset451(int) {
            return offset = 451;
        }

        virtual unsigned int offset452(int) {
            return offset = 452;
        }

        virtual unsigned int offset453(int) {
            return offset = 453;
        }

        virtual unsigned int offset454(int) {
            return offset = 454;
        }

        virtual unsigned int offset455(int) {
            return offset = 455;
        }

        virtual unsigned int offset456(int) {
            return offset = 456;
        }

        virtual unsigned int offset457(int) {
            return offset = 457;
        }

        virtual unsigned int offset458(int) {
            return offset = 458;
        }

        virtual unsigned int offset459(int) {
            return offset = 459;
        }

        virtual unsigned int offset460(int) {
            return offset = 460;
        }

        virtual unsigned int offset461(int) {
            return offset = 461;
        }

        virtual unsigned int offset462(int) {
            return offset = 462;
        }

        virtual unsigned int offset463(int) {
            return offset = 463;
        }

        virtual unsigned int offset464(int) {
            return offset = 464;
        }

        virtual unsigned int offset465(int) {
            return offset = 465;
        }

        virtual unsigned int offset466(int) {
            return offset = 466;
        }

        virtual unsigned int offset467(int) {
            return offset = 467;
        }

        virtual unsigned int offset468(int) {
            return offset = 468;
        }

        virtual unsigned int offset469(int) {
            return offset = 469;
        }

        virtual unsigned int offset470(int) {
            return offset = 470;
        }

        virtual unsigned int offset471(int) {
            return offset = 471;
        }

        virtual unsigned int offset472(int) {
            return offset = 472;
        }

        virtual unsigned int offset473(int) {
            return offset = 473;
        }

        virtual unsigned int offset474(int) {
            return offset = 474;
        }

        virtual unsigned int offset475(int) {
            return offset = 475;
        }

        virtual unsigned int offset476(int) {
            return offset = 476;
        }

        virtual unsigned int offset477(int) {
            return offset = 477;
        }

        virtual unsigned int offset478(int) {
            return offset = 478;
        }

        virtual unsigned int offset479(int) {
            return offset = 479;
        }

        virtual unsigned int offset480(int) {
            return offset = 480;
        }

        virtual unsigned int offset481(int) {
            return offset = 481;
        }

        virtual unsigned int offset482(int) {
            return offset = 482;
        }

        virtual unsigned int offset483(int) {
            return offset = 483;
        }

        virtual unsigned int offset484(int) {
            return offset = 484;
        }

        virtual unsigned int offset485(int) {
            return offset = 485;
        }

        virtual unsigned int offset486(int) {
            return offset = 486;
        }

        virtual unsigned int offset487(int) {
            return offset = 487;
        }

        virtual unsigned int offset488(int) {
            return offset = 488;
        }

        virtual unsigned int offset489(int) {
            return offset = 489;
        }

        virtual unsigned int offset490(int) {
            return offset = 490;
        }

        virtual unsigned int offset491(int) {
            return offset = 491;
        }

        virtual unsigned int offset492(int) {
            return offset = 492;
        }

        virtual unsigned int offset493(int) {
            return offset = 493;
        }

        virtual unsigned int offset494(int) {
            return offset = 494;
        }

        virtual unsigned int offset495(int) {
            return offset = 495;
        }

        virtual unsigned int offset496(int) {
            return offset = 496;
        }

        virtual unsigned int offset497(int) {
            return offset = 497;
        }

        virtual unsigned int offset498(int) {
            return offset = 498;
        }

        virtual unsigned int offset499(int) {
            return offset = 499;
        }


        virtual unsigned int offset500(int) {
            return offset = 500;
        }

        virtual unsigned int offset501(int) {
            return offset = 501;
        }

        virtual unsigned int offset502(int) {
            return offset = 502;
        }

        virtual unsigned int offset503(int) {
            return offset = 503;
        }

        virtual unsigned int offset504(int) {
            return offset = 504;
        }

        virtual unsigned int offset505(int) {
            return offset = 505;
        }

        virtual unsigned int offset506(int) {
            return offset = 506;
        }

        virtual unsigned int offset507(int) {
            return offset = 507;
        }

        virtual unsigned int offset508(int) {
            return offset = 508;
        }

        virtual unsigned int offset509(int) {
            return offset = 509;
        }

        virtual unsigned int offset510(int) {
            return offset = 510;
        }

        virtual unsigned int offset511(int) {
            return offset = 511;
        }

        virtual unsigned int offset512(int) {
            return offset = 512;
        }

        virtual unsigned int offset513(int) {
            return offset = 513;
        }

        virtual unsigned int offset514(int) {
            return offset = 514;
        }

        virtual unsigned int offset515(int) {
            return offset = 515;
        }

        virtual unsigned int offset516(int) {
            return offset = 516;
        }

        virtual unsigned int offset517(int) {
            return offset = 517;
        }

        virtual unsigned int offset518(int) {
            return offset = 518;
        }

        virtual unsigned int offset519(int) {
            return offset = 519;
        }

        virtual unsigned int offset520(int) {
            return offset = 520;
        }

        virtual unsigned int offset521(int) {
            return offset = 521;
        }

        virtual unsigned int offset522(int) {
            return offset = 522;
        }

        virtual unsigned int offset523(int) {
            return offset = 523;
        }

        virtual unsigned int offset524(int) {
            return offset = 524;
        }

        virtual unsigned int offset525(int) {
            return offset = 525;
        }

        virtual unsigned int offset526(int) {
            return offset = 526;
        }

        virtual unsigned int offset527(int) {
            return offset = 527;
        }

        virtual unsigned int offset528(int) {
            return offset = 528;
        }

        virtual unsigned int offset529(int) {
            return offset = 529;
        }

        virtual unsigned int offset530(int) {
            return offset = 530;
        }

        virtual unsigned int offset531(int) {
            return offset = 531;
        }

        virtual unsigned int offset532(int) {
            return offset = 532;
        }

        virtual unsigned int offset533(int) {
            return offset = 533;
        }

        virtual unsigned int offset534(int) {
            return offset = 534;
        }

        virtual unsigned int offset535(int) {
            return offset = 535;
        }

        virtual unsigned int offset536(int) {
            return offset = 536;
        }

        virtual unsigned int offset537(int) {
            return offset = 537;
        }

        virtual unsigned int offset538(int) {
            return offset = 538;
        }

        virtual unsigned int offset539(int) {
            return offset = 539;
        }

        virtual unsigned int offset540(int) {
            return offset = 540;
        }

        virtual unsigned int offset541(int) {
            return offset = 541;
        }

        virtual unsigned int offset542(int) {
            return offset = 542;
        }

        virtual unsigned int offset543(int) {
            return offset = 543;
        }

        virtual unsigned int offset544(int) {
            return offset = 544;
        }

        virtual unsigned int offset545(int) {
            return offset = 545;
        }

        virtual unsigned int offset546(int) {
            return offset = 546;
        }

        virtual unsigned int offset547(int) {
            return offset = 547;
        }

        virtual unsigned int offset548(int) {
            return offset = 548;
        }

        virtual unsigned int offset549(int) {
            return offset = 549;
        }

        virtual unsigned int offset550(int) {
            return offset = 550;
        }

        virtual unsigned int offset551(int) {
            return offset = 551;
        }

        virtual unsigned int offset552(int) {
            return offset = 552;
        }

        virtual unsigned int offset553(int) {
            return offset = 553;
        }

        virtual unsigned int offset554(int) {
            return offset = 554;
        }

        virtual unsigned int offset555(int) {
            return offset = 555;
        }

        virtual unsigned int offset556(int) {
            return offset = 556;
        }

        virtual unsigned int offset557(int) {
            return offset = 557;
        }

        virtual unsigned int offset558(int) {
            return offset = 558;
        }

        virtual unsigned int offset559(int) {
            return offset = 559;
        }

        virtual unsigned int offset560(int) {
            return offset = 560;
        }

        virtual unsigned int offset561(int) {
            return offset = 561;
        }

        virtual unsigned int offset562(int) {
            return offset = 562;
        }

        virtual unsigned int offset563(int) {
            return offset = 563;
        }

        virtual unsigned int offset564(int) {
            return offset = 564;
        }

        virtual unsigned int offset565(int) {
            return offset = 565;
        }

        virtual unsigned int offset566(int) {
            return offset = 566;
        }

        virtual unsigned int offset567(int) {
            return offset = 567;
        }

        virtual unsigned int offset568(int) {
            return offset = 568;
        }

        virtual unsigned int offset569(int) {
            return offset = 569;
        }

        virtual unsigned int offset570(int) {
            return offset = 570;
        }

        virtual unsigned int offset571(int) {
            return offset = 571;
        }

        virtual unsigned int offset572(int) {
            return offset = 572;
        }

        virtual unsigned int offset573(int) {
            return offset = 573;
        }

        virtual unsigned int offset574(int) {
            return offset = 574;
        }

        virtual unsigned int offset575(int) {
            return offset = 575;
        }

        virtual unsigned int offset576(int) {
            return offset = 576;
        }

        virtual unsigned int offset577(int) {
            return offset = 577;
        }

        virtual unsigned int offset578(int) {
            return offset = 578;
        }

        virtual unsigned int offset579(int) {
            return offset = 579;
        }

        virtual unsigned int offset580(int) {
            return offset = 580;
        }

        virtual unsigned int offset581(int) {
            return offset = 581;
        }

        virtual unsigned int offset582(int) {
            return offset = 582;
        }

        virtual unsigned int offset583(int) {
            return offset = 583;
        }

        virtual unsigned int offset584(int) {
            return offset = 584;
        }

        virtual unsigned int offset585(int) {
            return offset = 585;
        }

        virtual unsigned int offset586(int) {
            return offset = 586;
        }

        virtual unsigned int offset587(int) {
            return offset = 587;
        }

        virtual unsigned int offset588(int) {
            return offset = 588;
        }

        virtual unsigned int offset589(int) {
            return offset = 589;
        }

        virtual unsigned int offset590(int) {
            return offset = 590;
        }

        virtual unsigned int offset591(int) {
            return offset = 591;
        }

        virtual unsigned int offset592(int) {
            return offset = 592;
        }

        virtual unsigned int offset593(int) {
            return offset = 593;
        }

        virtual unsigned int offset594(int) {
            return offset = 594;
        }

        virtual unsigned int offset595(int) {
            return offset = 595;
        }

        virtual unsigned int offset596(int) {
            return offset = 596;
        }

        virtual unsigned int offset597(int) {
            return offset = 597;
        }

        virtual unsigned int offset598(int) {
            return offset = 598;
        }

        virtual unsigned int offset599(int) {
            return offset = 599;
        }


        virtual unsigned int offset600(int) {
            return offset = 600;
        }

        virtual unsigned int offset601(int) {
            return offset = 601;
        }

        virtual unsigned int offset602(int) {
            return offset = 602;
        }

        virtual unsigned int offset603(int) {
            return offset = 603;
        }

        virtual unsigned int offset604(int) {
            return offset = 604;
        }

        virtual unsigned int offset605(int) {
            return offset = 605;
        }

        virtual unsigned int offset606(int) {
            return offset = 606;
        }

        virtual unsigned int offset607(int) {
            return offset = 607;
        }

        virtual unsigned int offset608(int) {
            return offset = 608;
        }

        virtual unsigned int offset609(int) {
            return offset = 609;
        }

        virtual unsigned int offset610(int) {
            return offset = 610;
        }

        virtual unsigned int offset611(int) {
            return offset = 611;
        }

        virtual unsigned int offset612(int) {
            return offset = 612;
        }

        virtual unsigned int offset613(int) {
            return offset = 613;
        }

        virtual unsigned int offset614(int) {
            return offset = 614;
        }

        virtual unsigned int offset615(int) {
            return offset = 615;
        }

        virtual unsigned int offset616(int) {
            return offset = 616;
        }

        virtual unsigned int offset617(int) {
            return offset = 617;
        }

        virtual unsigned int offset618(int) {
            return offset = 618;
        }

        virtual unsigned int offset619(int) {
            return offset = 619;
        }

        virtual unsigned int offset620(int) {
            return offset = 620;
        }

        virtual unsigned int offset621(int) {
            return offset = 621;
        }

        virtual unsigned int offset622(int) {
            return offset = 622;
        }

        virtual unsigned int offset623(int) {
            return offset = 623;
        }

        virtual unsigned int offset624(int) {
            return offset = 624;
        }

        virtual unsigned int offset625(int) {
            return offset = 625;
        }

        virtual unsigned int offset626(int) {
            return offset = 626;
        }

        virtual unsigned int offset627(int) {
            return offset = 627;
        }

        virtual unsigned int offset628(int) {
            return offset = 628;
        }

        virtual unsigned int offset629(int) {
            return offset = 629;
        }

        virtual unsigned int offset630(int) {
            return offset = 630;
        }

        virtual unsigned int offset631(int) {
            return offset = 631;
        }

        virtual unsigned int offset632(int) {
            return offset = 632;
        }

        virtual unsigned int offset633(int) {
            return offset = 633;
        }

        virtual unsigned int offset634(int) {
            return offset = 634;
        }

        virtual unsigned int offset635(int) {
            return offset = 635;
        }

        virtual unsigned int offset636(int) {
            return offset = 636;
        }

        virtual unsigned int offset637(int) {
            return offset = 637;
        }

        virtual unsigned int offset638(int) {
            return offset = 638;
        }

        virtual unsigned int offset639(int) {
            return offset = 639;
        }

        virtual unsigned int offset640(int) {
            return offset = 640;
        }

        virtual unsigned int offset641(int) {
            return offset = 641;
        }

        virtual unsigned int offset642(int) {
            return offset = 642;
        }

        virtual unsigned int offset643(int) {
            return offset = 643;
        }

        virtual unsigned int offset644(int) {
            return offset = 644;
        }

        virtual unsigned int offset645(int) {
            return offset = 645;
        }

        virtual unsigned int offset646(int) {
            return offset = 646;
        }

        virtual unsigned int offset647(int) {
            return offset = 647;
        }

        virtual unsigned int offset648(int) {
            return offset = 648;
        }

        virtual unsigned int offset649(int) {
            return offset = 649;
        }

        virtual unsigned int offset650(int) {
            return offset = 650;
        }

        virtual unsigned int offset651(int) {
            return offset = 651;
        }

        virtual unsigned int offset652(int) {
            return offset = 652;
        }

        virtual unsigned int offset653(int) {
            return offset = 653;
        }

        virtual unsigned int offset654(int) {
            return offset = 654;
        }

        virtual unsigned int offset655(int) {
            return offset = 655;
        }

        virtual unsigned int offset656(int) {
            return offset = 656;
        }

        virtual unsigned int offset657(int) {
            return offset = 657;
        }

        virtual unsigned int offset658(int) {
            return offset = 658;
        }

        virtual unsigned int offset659(int) {
            return offset = 659;
        }

        virtual unsigned int offset660(int) {
            return offset = 660;
        }

        virtual unsigned int offset661(int) {
            return offset = 661;
        }

        virtual unsigned int offset662(int) {
            return offset = 662;
        }

        virtual unsigned int offset663(int) {
            return offset = 663;
        }

        virtual unsigned int offset664(int) {
            return offset = 664;
        }

        virtual unsigned int offset665(int) {
            return offset = 665;
        }

        virtual unsigned int offset666(int) {
            return offset = 666;
        }

        virtual unsigned int offset667(int) {
            return offset = 667;
        }

        virtual unsigned int offset668(int) {
            return offset = 668;
        }

        virtual unsigned int offset669(int) {
            return offset = 669;
        }

        virtual unsigned int offset670(int) {
            return offset = 670;
        }

        virtual unsigned int offset671(int) {
            return offset = 671;
        }

        virtual unsigned int offset672(int) {
            return offset = 672;
        }

        virtual unsigned int offset673(int) {
            return offset = 673;
        }

        virtual unsigned int offset674(int) {
            return offset = 674;
        }

        virtual unsigned int offset675(int) {
            return offset = 675;
        }

        virtual unsigned int offset676(int) {
            return offset = 676;
        }

        virtual unsigned int offset677(int) {
            return offset = 677;
        }

        virtual unsigned int offset678(int) {
            return offset = 678;
        }

        virtual unsigned int offset679(int) {
            return offset = 679;
        }

        virtual unsigned int offset680(int) {
            return offset = 680;
        }

        virtual unsigned int offset681(int) {
            return offset = 681;
        }

        virtual unsigned int offset682(int) {
            return offset = 682;
        }

        virtual unsigned int offset683(int) {
            return offset = 683;
        }

        virtual unsigned int offset684(int) {
            return offset = 684;
        }

        virtual unsigned int offset685(int) {
            return offset = 685;
        }

        virtual unsigned int offset686(int) {
            return offset = 686;
        }

        virtual unsigned int offset687(int) {
            return offset = 687;
        }

        virtual unsigned int offset688(int) {
            return offset = 688;
        }

        virtual unsigned int offset689(int) {
            return offset = 689;
        }

        virtual unsigned int offset690(int) {
            return offset = 690;
        }

        virtual unsigned int offset691(int) {
            return offset = 691;
        }

        virtual unsigned int offset692(int) {
            return offset = 692;
        }

        virtual unsigned int offset693(int) {
            return offset = 693;
        }

        virtual unsigned int offset694(int) {
            return offset = 694;
        }

        virtual unsigned int offset695(int) {
            return offset = 695;
        }

        virtual unsigned int offset696(int) {
            return offset = 696;
        }

        virtual unsigned int offset697(int) {
            return offset = 697;
        }

        virtual unsigned int offset698(int) {
            return offset = 698;
        }

        virtual unsigned int offset699(int) {
            return offset = 699;
        }


        virtual unsigned int offset700(int) {
            return offset = 700;
        }

        virtual unsigned int offset701(int) {
            return offset = 701;
        }

        virtual unsigned int offset702(int) {
            return offset = 702;
        }

        virtual unsigned int offset703(int) {
            return offset = 703;
        }

        virtual unsigned int offset704(int) {
            return offset = 704;
        }

        virtual unsigned int offset705(int) {
            return offset = 705;
        }

        virtual unsigned int offset706(int) {
            return offset = 706;
        }

        virtual unsigned int offset707(int) {
            return offset = 707;
        }

        virtual unsigned int offset708(int) {
            return offset = 708;
        }

        virtual unsigned int offset709(int) {
            return offset = 709;
        }

        virtual unsigned int offset710(int) {
            return offset = 710;
        }

        virtual unsigned int offset711(int) {
            return offset = 711;
        }

        virtual unsigned int offset712(int) {
            return offset = 712;
        }

        virtual unsigned int offset713(int) {
            return offset = 713;
        }

        virtual unsigned int offset714(int) {
            return offset = 714;
        }

        virtual unsigned int offset715(int) {
            return offset = 715;
        }

        virtual unsigned int offset716(int) {
            return offset = 716;
        }

        virtual unsigned int offset717(int) {
            return offset = 717;
        }

        virtual unsigned int offset718(int) {
            return offset = 718;
        }

        virtual unsigned int offset719(int) {
            return offset = 719;
        }

        virtual unsigned int offset720(int) {
            return offset = 720;
        }

        virtual unsigned int offset721(int) {
            return offset = 721;
        }

        virtual unsigned int offset722(int) {
            return offset = 722;
        }

        virtual unsigned int offset723(int) {
            return offset = 723;
        }

        virtual unsigned int offset724(int) {
            return offset = 724;
        }

        virtual unsigned int offset725(int) {
            return offset = 725;
        }

        virtual unsigned int offset726(int) {
            return offset = 726;
        }

        virtual unsigned int offset727(int) {
            return offset = 727;
        }

        virtual unsigned int offset728(int) {
            return offset = 728;
        }

        virtual unsigned int offset729(int) {
            return offset = 729;
        }

        virtual unsigned int offset730(int) {
            return offset = 730;
        }

        virtual unsigned int offset731(int) {
            return offset = 731;
        }

        virtual unsigned int offset732(int) {
            return offset = 732;
        }

        virtual unsigned int offset733(int) {
            return offset = 733;
        }

        virtual unsigned int offset734(int) {
            return offset = 734;
        }

        virtual unsigned int offset735(int) {
            return offset = 735;
        }

        virtual unsigned int offset736(int) {
            return offset = 736;
        }

        virtual unsigned int offset737(int) {
            return offset = 737;
        }

        virtual unsigned int offset738(int) {
            return offset = 738;
        }

        virtual unsigned int offset739(int) {
            return offset = 739;
        }

        virtual unsigned int offset740(int) {
            return offset = 740;
        }

        virtual unsigned int offset741(int) {
            return offset = 741;
        }

        virtual unsigned int offset742(int) {
            return offset = 742;
        }

        virtual unsigned int offset743(int) {
            return offset = 743;
        }

        virtual unsigned int offset744(int) {
            return offset = 744;
        }

        virtual unsigned int offset745(int) {
            return offset = 745;
        }

        virtual unsigned int offset746(int) {
            return offset = 746;
        }

        virtual unsigned int offset747(int) {
            return offset = 747;
        }

        virtual unsigned int offset748(int) {
            return offset = 748;
        }

        virtual unsigned int offset749(int) {
            return offset = 749;
        }

        virtual unsigned int offset750(int) {
            return offset = 750;
        }

        virtual unsigned int offset751(int) {
            return offset = 751;
        }

        virtual unsigned int offset752(int) {
            return offset = 752;
        }

        virtual unsigned int offset753(int) {
            return offset = 753;
        }

        virtual unsigned int offset754(int) {
            return offset = 754;
        }

        virtual unsigned int offset755(int) {
            return offset = 755;
        }

        virtual unsigned int offset756(int) {
            return offset = 756;
        }

        virtual unsigned int offset757(int) {
            return offset = 757;
        }

        virtual unsigned int offset758(int) {
            return offset = 758;
        }

        virtual unsigned int offset759(int) {
            return offset = 759;
        }

        virtual unsigned int offset760(int) {
            return offset = 760;
        }

        virtual unsigned int offset761(int) {
            return offset = 761;
        }

        virtual unsigned int offset762(int) {
            return offset = 762;
        }

        virtual unsigned int offset763(int) {
            return offset = 763;
        }

        virtual unsigned int offset764(int) {
            return offset = 764;
        }

        virtual unsigned int offset765(int) {
            return offset = 765;
        }

        virtual unsigned int offset766(int) {
            return offset = 766;
        }

        virtual unsigned int offset767(int) {
            return offset = 767;
        }

        virtual unsigned int offset768(int) {
            return offset = 768;
        }

        virtual unsigned int offset769(int) {
            return offset = 769;
        }

        virtual unsigned int offset770(int) {
            return offset = 770;
        }

        virtual unsigned int offset771(int) {
            return offset = 771;
        }

        virtual unsigned int offset772(int) {
            return offset = 772;
        }

        virtual unsigned int offset773(int) {
            return offset = 773;
        }

        virtual unsigned int offset774(int) {
            return offset = 774;
        }

        virtual unsigned int offset775(int) {
            return offset = 775;
        }

        virtual unsigned int offset776(int) {
            return offset = 776;
        }

        virtual unsigned int offset777(int) {
            return offset = 777;
        }

        virtual unsigned int offset778(int) {
            return offset = 778;
        }

        virtual unsigned int offset779(int) {
            return offset = 779;
        }

        virtual unsigned int offset780(int) {
            return offset = 780;
        }

        virtual unsigned int offset781(int) {
            return offset = 781;
        }

        virtual unsigned int offset782(int) {
            return offset = 782;
        }

        virtual unsigned int offset783(int) {
            return offset = 783;
        }

        virtual unsigned int offset784(int) {
            return offset = 784;
        }

        virtual unsigned int offset785(int) {
            return offset = 785;
        }

        virtual unsigned int offset786(int) {
            return offset = 786;
        }

        virtual unsigned int offset787(int) {
            return offset = 787;
        }

        virtual unsigned int offset788(int) {
            return offset = 788;
        }

        virtual unsigned int offset789(int) {
            return offset = 789;
        }

        virtual unsigned int offset790(int) {
            return offset = 790;
        }

        virtual unsigned int offset791(int) {
            return offset = 791;
        }

        virtual unsigned int offset792(int) {
            return offset = 792;
        }

        virtual unsigned int offset793(int) {
            return offset = 793;
        }

        virtual unsigned int offset794(int) {
            return offset = 794;
        }

        virtual unsigned int offset795(int) {
            return offset = 795;
        }

        virtual unsigned int offset796(int) {
            return offset = 796;
        }

        virtual unsigned int offset797(int) {
            return offset = 797;
        }

        virtual unsigned int offset798(int) {
            return offset = 798;
        }

        virtual unsigned int offset799(int) {
            return offset = 799;
        }


        virtual unsigned int offset800(int) {
            return offset = 800;
        }

        virtual unsigned int offset801(int) {
            return offset = 801;
        }

        virtual unsigned int offset802(int) {
            return offset = 802;
        }

        virtual unsigned int offset803(int) {
            return offset = 803;
        }

        virtual unsigned int offset804(int) {
            return offset = 804;
        }

        virtual unsigned int offset805(int) {
            return offset = 805;
        }

        virtual unsigned int offset806(int) {
            return offset = 806;
        }

        virtual unsigned int offset807(int) {
            return offset = 807;
        }

        virtual unsigned int offset808(int) {
            return offset = 808;
        }

        virtual unsigned int offset809(int) {
            return offset = 809;
        }

        virtual unsigned int offset810(int) {
            return offset = 810;
        }

        virtual unsigned int offset811(int) {
            return offset = 811;
        }

        virtual unsigned int offset812(int) {
            return offset = 812;
        }

        virtual unsigned int offset813(int) {
            return offset = 813;
        }

        virtual unsigned int offset814(int) {
            return offset = 814;
        }

        virtual unsigned int offset815(int) {
            return offset = 815;
        }

        virtual unsigned int offset816(int) {
            return offset = 816;
        }

        virtual unsigned int offset817(int) {
            return offset = 817;
        }

        virtual unsigned int offset818(int) {
            return offset = 818;
        }

        virtual unsigned int offset819(int) {
            return offset = 819;
        }

        virtual unsigned int offset820(int) {
            return offset = 820;
        }

        virtual unsigned int offset821(int) {
            return offset = 821;
        }

        virtual unsigned int offset822(int) {
            return offset = 822;
        }

        virtual unsigned int offset823(int) {
            return offset = 823;
        }

        virtual unsigned int offset824(int) {
            return offset = 824;
        }

        virtual unsigned int offset825(int) {
            return offset = 825;
        }

        virtual unsigned int offset826(int) {
            return offset = 826;
        }

        virtual unsigned int offset827(int) {
            return offset = 827;
        }

        virtual unsigned int offset828(int) {
            return offset = 828;
        }

        virtual unsigned int offset829(int) {
            return offset = 829;
        }

        virtual unsigned int offset830(int) {
            return offset = 830;
        }

        virtual unsigned int offset831(int) {
            return offset = 831;
        }

        virtual unsigned int offset832(int) {
            return offset = 832;
        }

        virtual unsigned int offset833(int) {
            return offset = 833;
        }

        virtual unsigned int offset834(int) {
            return offset = 834;
        }

        virtual unsigned int offset835(int) {
            return offset = 835;
        }

        virtual unsigned int offset836(int) {
            return offset = 836;
        }

        virtual unsigned int offset837(int) {
            return offset = 837;
        }

        virtual unsigned int offset838(int) {
            return offset = 838;
        }

        virtual unsigned int offset839(int) {
            return offset = 839;
        }

        virtual unsigned int offset840(int) {
            return offset = 840;
        }

        virtual unsigned int offset841(int) {
            return offset = 841;
        }

        virtual unsigned int offset842(int) {
            return offset = 842;
        }

        virtual unsigned int offset843(int) {
            return offset = 843;
        }

        virtual unsigned int offset844(int) {
            return offset = 844;
        }

        virtual unsigned int offset845(int) {
            return offset = 845;
        }

        virtual unsigned int offset846(int) {
            return offset = 846;
        }

        virtual unsigned int offset847(int) {
            return offset = 847;
        }

        virtual unsigned int offset848(int) {
            return offset = 848;
        }

        virtual unsigned int offset849(int) {
            return offset = 849;
        }

        virtual unsigned int offset850(int) {
            return offset = 850;
        }

        virtual unsigned int offset851(int) {
            return offset = 851;
        }

        virtual unsigned int offset852(int) {
            return offset = 852;
        }

        virtual unsigned int offset853(int) {
            return offset = 853;
        }

        virtual unsigned int offset854(int) {
            return offset = 854;
        }

        virtual unsigned int offset855(int) {
            return offset = 855;
        }

        virtual unsigned int offset856(int) {
            return offset = 856;
        }

        virtual unsigned int offset857(int) {
            return offset = 857;
        }

        virtual unsigned int offset858(int) {
            return offset = 858;
        }

        virtual unsigned int offset859(int) {
            return offset = 859;
        }

        virtual unsigned int offset860(int) {
            return offset = 860;
        }

        virtual unsigned int offset861(int) {
            return offset = 861;
        }

        virtual unsigned int offset862(int) {
            return offset = 862;
        }

        virtual unsigned int offset863(int) {
            return offset = 863;
        }

        virtual unsigned int offset864(int) {
            return offset = 864;
        }

        virtual unsigned int offset865(int) {
            return offset = 865;
        }

        virtual unsigned int offset866(int) {
            return offset = 866;
        }

        virtual unsigned int offset867(int) {
            return offset = 867;
        }

        virtual unsigned int offset868(int) {
            return offset = 868;
        }

        virtual unsigned int offset869(int) {
            return offset = 869;
        }

        virtual unsigned int offset870(int) {
            return offset = 870;
        }

        virtual unsigned int offset871(int) {
            return offset = 871;
        }

        virtual unsigned int offset872(int) {
            return offset = 872;
        }

        virtual unsigned int offset873(int) {
            return offset = 873;
        }

        virtual unsigned int offset874(int) {
            return offset = 874;
        }

        virtual unsigned int offset875(int) {
            return offset = 875;
        }

        virtual unsigned int offset876(int) {
            return offset = 876;
        }

        virtual unsigned int offset877(int) {
            return offset = 877;
        }

        virtual unsigned int offset878(int) {
            return offset = 878;
        }

        virtual unsigned int offset879(int) {
            return offset = 879;
        }

        virtual unsigned int offset880(int) {
            return offset = 880;
        }

        virtual unsigned int offset881(int) {
            return offset = 881;
        }

        virtual unsigned int offset882(int) {
            return offset = 882;
        }

        virtual unsigned int offset883(int) {
            return offset = 883;
        }

        virtual unsigned int offset884(int) {
            return offset = 884;
        }

        virtual unsigned int offset885(int) {
            return offset = 885;
        }

        virtual unsigned int offset886(int) {
            return offset = 886;
        }

        virtual unsigned int offset887(int) {
            return offset = 887;
        }

        virtual unsigned int offset888(int) {
            return offset = 888;
        }

        virtual unsigned int offset889(int) {
            return offset = 889;
        }

        virtual unsigned int offset890(int) {
            return offset = 890;
        }

        virtual unsigned int offset891(int) {
            return offset = 891;
        }

        virtual unsigned int offset892(int) {
            return offset = 892;
        }

        virtual unsigned int offset893(int) {
            return offset = 893;
        }

        virtual unsigned int offset894(int) {
            return offset = 894;
        }

        virtual unsigned int offset895(int) {
            return offset = 895;
        }

        virtual unsigned int offset896(int) {
            return offset = 896;
        }

        virtual unsigned int offset897(int) {
            return offset = 897;
        }

        virtual unsigned int offset898(int) {
            return offset = 898;
        }

        virtual unsigned int offset899(int) {
            return offset = 899;
        }


        virtual unsigned int offset900(int) {
            return offset = 900;
        }

        virtual unsigned int offset901(int) {
            return offset = 901;
        }

        virtual unsigned int offset902(int) {
            return offset = 902;
        }

        virtual unsigned int offset903(int) {
            return offset = 903;
        }

        virtual unsigned int offset904(int) {
            return offset = 904;
        }

        virtual unsigned int offset905(int) {
            return offset = 905;
        }

        virtual unsigned int offset906(int) {
            return offset = 906;
        }

        virtual unsigned int offset907(int) {
            return offset = 907;
        }

        virtual unsigned int offset908(int) {
            return offset = 908;
        }

        virtual unsigned int offset909(int) {
            return offset = 909;
        }

        virtual unsigned int offset910(int) {
            return offset = 910;
        }

        virtual unsigned int offset911(int) {
            return offset = 911;
        }

        virtual unsigned int offset912(int) {
            return offset = 912;
        }

        virtual unsigned int offset913(int) {
            return offset = 913;
        }

        virtual unsigned int offset914(int) {
            return offset = 914;
        }

        virtual unsigned int offset915(int) {
            return offset = 915;
        }

        virtual unsigned int offset916(int) {
            return offset = 916;
        }

        virtual unsigned int offset917(int) {
            return offset = 917;
        }

        virtual unsigned int offset918(int) {
            return offset = 918;
        }

        virtual unsigned int offset919(int) {
            return offset = 919;
        }

        virtual unsigned int offset920(int) {
            return offset = 920;
        }

        virtual unsigned int offset921(int) {
            return offset = 921;
        }

        virtual unsigned int offset922(int) {
            return offset = 922;
        }

        virtual unsigned int offset923(int) {
            return offset = 923;
        }

        virtual unsigned int offset924(int) {
            return offset = 924;
        }

        virtual unsigned int offset925(int) {
            return offset = 925;
        }

        virtual unsigned int offset926(int) {
            return offset = 926;
        }

        virtual unsigned int offset927(int) {
            return offset = 927;
        }

        virtual unsigned int offset928(int) {
            return offset = 928;
        }

        virtual unsigned int offset929(int) {
            return offset = 929;
        }

        virtual unsigned int offset930(int) {
            return offset = 930;
        }

        virtual unsigned int offset931(int) {
            return offset = 931;
        }

        virtual unsigned int offset932(int) {
            return offset = 932;
        }

        virtual unsigned int offset933(int) {
            return offset = 933;
        }

        virtual unsigned int offset934(int) {
            return offset = 934;
        }

        virtual unsigned int offset935(int) {
            return offset = 935;
        }

        virtual unsigned int offset936(int) {
            return offset = 936;
        }

        virtual unsigned int offset937(int) {
            return offset = 937;
        }

        virtual unsigned int offset938(int) {
            return offset = 938;
        }

        virtual unsigned int offset939(int) {
            return offset = 939;
        }

        virtual unsigned int offset940(int) {
            return offset = 940;
        }

        virtual unsigned int offset941(int) {
            return offset = 941;
        }

        virtual unsigned int offset942(int) {
            return offset = 942;
        }

        virtual unsigned int offset943(int) {
            return offset = 943;
        }

        virtual unsigned int offset944(int) {
            return offset = 944;
        }

        virtual unsigned int offset945(int) {
            return offset = 945;
        }

        virtual unsigned int offset946(int) {
            return offset = 946;
        }

        virtual unsigned int offset947(int) {
            return offset = 947;
        }

        virtual unsigned int offset948(int) {
            return offset = 948;
        }

        virtual unsigned int offset949(int) {
            return offset = 949;
        }

        virtual unsigned int offset950(int) {
            return offset = 950;
        }

        virtual unsigned int offset951(int) {
            return offset = 951;
        }

        virtual unsigned int offset952(int) {
            return offset = 952;
        }

        virtual unsigned int offset953(int) {
            return offset = 953;
        }

        virtual unsigned int offset954(int) {
            return offset = 954;
        }

        virtual unsigned int offset955(int) {
            return offset = 955;
        }

        virtual unsigned int offset956(int) {
            return offset = 956;
        }

        virtual unsigned int offset957(int) {
            return offset = 957;
        }

        virtual unsigned int offset958(int) {
            return offset = 958;
        }

        virtual unsigned int offset959(int) {
            return offset = 959;
        }

        virtual unsigned int offset960(int) {
            return offset = 960;
        }

        virtual unsigned int offset961(int) {
            return offset = 961;
        }

        virtual unsigned int offset962(int) {
            return offset = 962;
        }

        virtual unsigned int offset963(int) {
            return offset = 963;
        }

        virtual unsigned int offset964(int) {
            return offset = 964;
        }

        virtual unsigned int offset965(int) {
            return offset = 965;
        }

        virtual unsigned int offset966(int) {
            return offset = 966;
        }

        virtual unsigned int offset967(int) {
            return offset = 967;
        }

        virtual unsigned int offset968(int) {
            return offset = 968;
        }

        virtual unsigned int offset969(int) {
            return offset = 969;
        }

        virtual unsigned int offset970(int) {
            return offset = 970;
        }

        virtual unsigned int offset971(int) {
            return offset = 971;
        }

        virtual unsigned int offset972(int) {
            return offset = 972;
        }

        virtual unsigned int offset973(int) {
            return offset = 973;
        }

        virtual unsigned int offset974(int) {
            return offset = 974;
        }

        virtual unsigned int offset975(int) {
            return offset = 975;
        }

        virtual unsigned int offset976(int) {
            return offset = 976;
        }

        virtual unsigned int offset977(int) {
            return offset = 977;
        }

        virtual unsigned int offset978(int) {
            return offset = 978;
        }

        virtual unsigned int offset979(int) {
            return offset = 979;
        }

        virtual unsigned int offset980(int) {
            return offset = 980;
        }

        virtual unsigned int offset981(int) {
            return offset = 981;
        }

        virtual unsigned int offset982(int) {
            return offset = 982;
        }

        virtual unsigned int offset983(int) {
            return offset = 983;
        }

        virtual unsigned int offset984(int) {
            return offset = 984;
        }

        virtual unsigned int offset985(int) {
            return offset = 985;
        }

        virtual unsigned int offset986(int) {
            return offset = 986;
        }

        virtual unsigned int offset987(int) {
            return offset = 987;
        }

        virtual unsigned int offset988(int) {
            return offset = 988;
        }

        virtual unsigned int offset989(int) {
            return offset = 989;
        }

        virtual unsigned int offset990(int) {
            return offset = 990;
        }

        virtual unsigned int offset991(int) {
            return offset = 991;
        }

        virtual unsigned int offset992(int) {
            return offset = 992;
        }

        virtual unsigned int offset993(int) {
            return offset = 993;
        }

        virtual unsigned int offset994(int) {
            return offset = 994;
        }

        virtual unsigned int offset995(int) {
            return offset = 995;
        }

        virtual unsigned int offset996(int) {
            return offset = 996;
        }

        virtual unsigned int offset997(int) {
            return offset = 997;
        }

        virtual unsigned int offset998(int) {
            return offset = 998;
        }

        virtual unsigned int offset999(int) {
            return offset = 999;
        }

        virtual unsigned int offset1000(int) {
            return offset = 1000;
        }

    };
}
namespace fakeit {

    template<typename TARGET, typename SOURCE>
    TARGET union_cast(SOURCE source) {

        union {
            SOURCE source;
            TARGET target;
        } u;
        u.source = source;
        return u.target;
    }

}

namespace fakeit {
    class NoVirtualDtor {
    };

    class VTUtils {
    public:

        template<typename C, typename R, typename ... arglist>
        static unsigned int getOffset(R (C::*vMethod)(arglist...)) {
            auto sMethod = reinterpret_cast<unsigned int (VirtualOffsetSelector::*)(int)>(vMethod);
            VirtualOffsetSelector offsetSelctor;
            return (offsetSelctor.*sMethod)(0);
        }

        template<typename C>
        static typename std::enable_if<std::has_virtual_destructor<C>::value, unsigned int>::type
        getDestructorOffset() {
            VirtualOffsetSelector offsetSelctor;
            union_cast<C *>(&offsetSelctor)->~C();
            return offsetSelctor.offset;
        }

        template<typename C>
        static typename std::enable_if<!std::has_virtual_destructor<C>::value, unsigned int>::type
        getDestructorOffset() {
            throw NoVirtualDtor();
        }

        template<typename C>
        static unsigned int getVTSize() {
            struct Derrived : public C {
                virtual void endOfVt() {
                }
            };

            unsigned int vtSize = getOffset(&Derrived::endOfVt);
            return vtSize;
        }
    };


}
#ifdef _MSC_VER
namespace fakeit {

    typedef unsigned long DWORD;

    struct TypeDescriptor {
        TypeDescriptor() :
                ptrToVTable(0), spare(0) {

            int **tiVFTPtr = (int **) (&typeid(void));
            int *i = (int *) tiVFTPtr[0];
			char *type_info_vft_ptr = (char *) i;
            ptrToVTable = type_info_vft_ptr;
        }

		char *ptrToVTable;
        DWORD spare;
        char name[8];
    };

    struct PMD {



        int mdisp;

        int pdisp;
        int vdisp;

        PMD() :
                mdisp(0), pdisp(-1), vdisp(0) {
        }
    };

    struct RTTIBaseClassDescriptor {
        RTTIBaseClassDescriptor() :
                pTypeDescriptor(nullptr), numContainedBases(0), attributes(0) {
        }

        const std::type_info *pTypeDescriptor;
        DWORD numContainedBases;
        struct PMD where;
        DWORD attributes;
    };

    template<typename C, typename... baseclasses>
    struct RTTIClassHierarchyDescriptor {
        RTTIClassHierarchyDescriptor() :
                signature(0),
                attributes(0),
                numBaseClasses(0),
                pBaseClassArray(nullptr) {
            pBaseClassArray = new RTTIBaseClassDescriptor *[1 + sizeof...(baseclasses)];
            addBaseClass < C, baseclasses...>();
        }

        ~RTTIClassHierarchyDescriptor() {
            for (int i = 0; i < 1 + sizeof...(baseclasses); i++) {
                RTTIBaseClassDescriptor *desc = pBaseClassArray[i];
                delete desc;
            }
            delete[] pBaseClassArray;
        }

        DWORD signature;
        DWORD attributes;
        DWORD numBaseClasses;
        RTTIBaseClassDescriptor **pBaseClassArray;

        template<typename BaseType>
        void addBaseClass() {
            static_assert(std::is_base_of<BaseType, C>::value, "C must be a derived class of BaseType");
            RTTIBaseClassDescriptor *desc = new RTTIBaseClassDescriptor();
            desc->pTypeDescriptor = &typeid(BaseType);
            pBaseClassArray[numBaseClasses] = desc;
            for (unsigned int i = 0; i < numBaseClasses; i++) {
                pBaseClassArray[i]->numContainedBases++;
            }
            numBaseClasses++;
        }

        template<typename head, typename B1, typename... tail>
        void addBaseClass() {
            static_assert(std::is_base_of<B1, head>::value, "invalid inheritance list");
            addBaseClass<head>();
            addBaseClass<B1, tail...>();
        }

    };

	template<typename C, typename... baseclasses>
	struct RTTICompleteObjectLocator {
#ifdef _WIN64
		RTTICompleteObjectLocator(const std::type_info &unused) :
			signature(0), offset(0), cdOffset(0),
			typeDescriptorOffset(0), classDescriptorOffset(0)
		{
		}

		DWORD signature;
		DWORD offset;
		DWORD cdOffset;
		DWORD typeDescriptorOffset;
		DWORD classDescriptorOffset;
#else
		RTTICompleteObjectLocator(const std::type_info &info) :
			signature(0), offset(0), cdOffset(0),
			pTypeDescriptor(&info),
			pClassDescriptor(new RTTIClassHierarchyDescriptor<C, baseclasses...>()) {
		}

		~RTTICompleteObjectLocator() {
			delete pClassDescriptor;
		}

		DWORD signature;
		DWORD offset;
		DWORD cdOffset;
		const std::type_info *pTypeDescriptor;
		struct RTTIClassHierarchyDescriptor<C, baseclasses...> *pClassDescriptor;
#endif
	};


    struct VirtualTableBase {

        static VirtualTableBase &getVTable(void *instance) {
            fakeit::VirtualTableBase *vt = (fakeit::VirtualTableBase *) (instance);
            return *vt;
        }

        VirtualTableBase(void **firstMethod) : _firstMethod(firstMethod) { }

        void *getCookie(int index) {
            return _firstMethod[-2 - index];
        }

        void setCookie(int index, void *value) {
            _firstMethod[-2 - index] = value;
        }

        void *getMethod(unsigned int index) const {
            return _firstMethod[index];
        }

        void setMethod(unsigned int index, void *method) {
            _firstMethod[index] = method;
        }

    protected:
        void **_firstMethod;
    };

    template<class C, class... baseclasses>
    struct VirtualTable : public VirtualTableBase {

        class Handle {

            friend struct VirtualTable<C, baseclasses...>;

            void **firstMethod;

            Handle(void **method) : firstMethod(method) { }

        public:

            VirtualTable<C, baseclasses...> &restore() {
                VirtualTable<C, baseclasses...> *vt = (VirtualTable<C, baseclasses...> *) this;
                return *vt;
            }
        };

        static VirtualTable<C, baseclasses...> &getVTable(C &instance) {
            fakeit::VirtualTable<C, baseclasses...> *vt = (fakeit::VirtualTable<C, baseclasses...> *) (&instance);
            return *vt;
        }

        void copyFrom(VirtualTable<C, baseclasses...> &from) {
            unsigned int size = VTUtils::getVTSize<C>();
            for (unsigned int i = 0; i < size; i++) {
                _firstMethod[i] = from.getMethod(i);
            }
        }

        VirtualTable() : VirtualTable(buildVTArray()) {
        }

        ~VirtualTable() {

        }

        void dispose() {
            _firstMethod--;
            RTTICompleteObjectLocator<C, baseclasses...> *locator = (RTTICompleteObjectLocator<C, baseclasses...> *) _firstMethod[0];
            delete locator;
            _firstMethod -= numOfCookies;
            delete[] _firstMethod;
        }


        unsigned int dtor(int) {
            C *c = (C *) this;
            C &cRef = *c;
            auto vt = VirtualTable<C, baseclasses...>::getVTable(cRef);
            void *dtorPtr = vt.getCookie(numOfCookies - 1);
            void(*method)(C *) = reinterpret_cast<void (*)(C *)>(dtorPtr);
            method(c);
            return 0;
        }

        void setDtor(void *method) {





            void *dtorPtr = union_cast<void *>(&VirtualTable<C, baseclasses...>::dtor);
            unsigned int index = VTUtils::getDestructorOffset<C>();
            _firstMethod[index] = dtorPtr;
            setCookie(numOfCookies - 1, method);
        }

        unsigned int getSize() {
            return VTUtils::getVTSize<C>();
        }

        void initAll(void *value) {
            auto size = getSize();
            for (unsigned int i = 0; i < size; i++) {
                setMethod(i, value);
            }
        }

        Handle createHandle() {
            Handle h(_firstMethod);
            return h;
        }

    private:

        class SimpleType {
        };

        static_assert(sizeof(unsigned int (SimpleType::*)()) == sizeof(unsigned int (C::*)()),
            "Can't mock a type with multiple inheritance or with non-polymorphic base class");
        static const unsigned int numOfCookies = 3;

        static void **buildVTArray() {
            int vtSize = VTUtils::getVTSize<C>();
            auto array = new void *[vtSize + numOfCookies + 1]{};
            RTTICompleteObjectLocator<C, baseclasses...> *objectLocator = new RTTICompleteObjectLocator<C, baseclasses...>(
                    typeid(C));
            array += numOfCookies;
            array[0] = objectLocator;
            array++;
            return array;
        }

        VirtualTable(void **firstMethod) : VirtualTableBase(firstMethod) {
        }
    };
}
#else
#ifndef __clang__
#include <type_traits>
#include <tr2/type_traits>

namespace fakeit {
    template<typename ... T1>
    class has_one_base {
    };

    template<typename T1, typename T2, typename ... types>
    class has_one_base<std::tr2::__reflection_typelist<T1, T2, types...>> : public std::false_type {
    };

    template<typename T1>
    class has_one_base<std::tr2::__reflection_typelist<T1>>
            : public has_one_base<typename std::tr2::direct_bases<T1>::type> {
    };

    template<>
    class has_one_base<std::tr2::__reflection_typelist<>> : public std::true_type {
    };

    template<typename T>
    class is_simple_inheritance_layout : public has_one_base<typename std::tr2::direct_bases<T>::type> {
    };
}

#endif

namespace fakeit {

    struct VirtualTableBase {

        static VirtualTableBase &getVTable(void *instance) {
            fakeit::VirtualTableBase *vt = (fakeit::VirtualTableBase *) (instance);
            return *vt;
        }

        VirtualTableBase(void **firstMethod) : _firstMethod(firstMethod) { }

        void *getCookie(int index) {
            return _firstMethod[-3 - index];
        }

        void setCookie(int index, void *value) {
            _firstMethod[-3 - index] = value;
        }

        void *getMethod(unsigned int index) const {
            return _firstMethod[index];
        }

        void setMethod(unsigned int index, void *method) {
            _firstMethod[index] = method;
        }

    protected:
        void **_firstMethod;
    };

    template<class C, class ... baseclasses>
    struct VirtualTable : public VirtualTableBase {

#ifndef __clang__
        static_assert(is_simple_inheritance_layout<C>::value, "Can't mock a type with multiple inheritance");
#endif

        class Handle {

            friend struct VirtualTable<C, baseclasses...>;
            void **firstMethod;

            Handle(void **method) :
                    firstMethod(method) {
            }

        public:

            VirtualTable<C, baseclasses...> &restore() {
                VirtualTable<C, baseclasses...> *vt = (VirtualTable<C, baseclasses...> *) this;
                return *vt;
            }
        };

        static VirtualTable<C, baseclasses...> &getVTable(C &instance) {
            fakeit::VirtualTable<C, baseclasses...> *vt = (fakeit::VirtualTable<C, baseclasses...> *) (&instance);
            return *vt;
        }

        void copyFrom(VirtualTable<C, baseclasses...> &from) {
            unsigned int size = VTUtils::getVTSize<C>();

            for (size_t i = 0; i < size; ++i) {
                _firstMethod[i] = from.getMethod(i);
            }
        }

        VirtualTable() :
                VirtualTable(buildVTArray()) {
        }

        void dispose() {
            _firstMethod--;
            _firstMethod--;
            _firstMethod -= numOfCookies;
            delete[] _firstMethod;
        }

        unsigned int dtor(int) {
            C *c = (C *) this;
            C &cRef = *c;
            auto vt = VirtualTable<C, baseclasses...>::getVTable(cRef);
            unsigned int index = VTUtils::getDestructorOffset<C>();
            void *dtorPtr = vt.getMethod(index);
            void(*method)(C *) = union_cast<void (*)(C *)>(dtorPtr);
            method(c);
            return 0;
        }


        void setDtor(void *method) {
            unsigned int index = VTUtils::getDestructorOffset<C>();
            void *dtorPtr = union_cast<void *>(&VirtualTable<C, baseclasses...>::dtor);


            _firstMethod[index] = method;

            _firstMethod[index + 1] = dtorPtr;
        }


        unsigned int getSize() {
            return VTUtils::getVTSize<C>();
        }

        void initAll(void *value) {
            unsigned int size = getSize();
            for (unsigned int i = 0; i < size; i++) {
                setMethod(i, value);
            }
        }

        const std::type_info *getTypeId() {
            return (const std::type_info *) (_firstMethod[-1]);
        }

        Handle createHandle() {
            Handle h(_firstMethod);
            return h;
        }

    private:
        static const unsigned int numOfCookies = 2;

        static void **buildVTArray() {
            int size = VTUtils::getVTSize<C>();
            auto array = new void *[size + 2 + numOfCookies]{};
            array += numOfCookies;
            array++;
            array[0] = const_cast<std::type_info *>(&typeid(C));
            array++;
            return array;
        }

        VirtualTable(void **firstMethod) : VirtualTableBase(firstMethod) {
        }

    };
}
#endif
namespace fakeit {

    struct NoMoreRecordedActionException {
    };

    template<typename R, typename ... arglist>
    struct MethodInvocationHandler : Destructible {
        virtual R handleMethodInvocation(const typename fakeit::production_arg<arglist>::type... args) = 0;
    };

}
#include <new>

namespace fakeit {

#ifdef __GNUG__
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#endif


#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4200 )
#endif


    template<typename C, typename ... baseclasses>
    class FakeObject {

        VirtualTable<C, baseclasses...> vtable;

        static const size_t SIZE = sizeof(C) - sizeof(VirtualTable<C, baseclasses...>);
        char instanceArea[SIZE ? SIZE : 0];

        FakeObject(FakeObject const &) = delete;
        FakeObject &operator=(FakeObject const &) = delete;

    public:

        FakeObject() : vtable() {
            initializeDataMembersArea();
        }

        ~FakeObject() {
            vtable.dispose();
        }

        void initializeDataMembersArea() {
            for (size_t i = 0; i < SIZE; ++i) instanceArea[i] = (char) 0;
        }

        void setMethod(unsigned int index, void *method) {
            vtable.setMethod(index, method);
        }

        VirtualTable<C, baseclasses...> &getVirtualTable() {
            return vtable;
        }

        void setVirtualTable(VirtualTable<C, baseclasses...> &t) {
            vtable = t;
        }

        void setDtor(void *dtor) {
            vtable.setDtor(dtor);
        }
    };

#ifdef _MSC_VER
#pragma warning( pop )
#endif

#ifdef __GNUG__
#ifndef __clang__
#pragma GCC diagnostic pop
#endif
#endif

}
namespace fakeit {

    struct MethodProxy {

        MethodProxy(unsigned int id, unsigned int offset, void *vMethod) :
                _id(id),
                _offset(offset),
                _vMethod(vMethod) {
        }

        unsigned int getOffset() const {
            return _offset;
        }

        unsigned int getId() const {
            return _id;
        }

        void *getProxy() const {
            return union_cast<void *>(_vMethod);
        }

    private:
        unsigned int _id;
        unsigned int _offset;
        void *_vMethod;
    };
}
#include <utility>


namespace fakeit {

    struct InvocationHandlerCollection {
        static const unsigned int VT_COOKIE_INDEX = 0;

        virtual Destructible *getInvocatoinHandlerPtrById(unsigned int index) = 0;

        static InvocationHandlerCollection *getInvocationHandlerCollection(void *instance) {
            VirtualTableBase &vt = VirtualTableBase::getVTable(instance);
            InvocationHandlerCollection *invocationHandlerCollection = (InvocationHandlerCollection *) vt.getCookie(
                    InvocationHandlerCollection::VT_COOKIE_INDEX);
            return invocationHandlerCollection;
        }
    };


    template<typename R, typename ... arglist>
    class MethodProxyCreator {



    public:

        template<unsigned int id>
        MethodProxy createMethodProxy(unsigned int offset) {
            return MethodProxy(id, offset, union_cast<void *>(&MethodProxyCreator::methodProxyX < id > ));
        }

    protected:

        R methodProxy(unsigned int id, const typename fakeit::production_arg<arglist>::type... args) {
            InvocationHandlerCollection *invocationHandlerCollection = InvocationHandlerCollection::getInvocationHandlerCollection(
                    this);
            MethodInvocationHandler<R, arglist...> *invocationHandler =
                    (MethodInvocationHandler<R, arglist...> *) invocationHandlerCollection->getInvocatoinHandlerPtrById(
                            id);
            return invocationHandler->handleMethodInvocation(std::forward<const typename fakeit::production_arg<arglist>::type>(args)...);
        }

        template<int id>
        R methodProxyX(arglist ... args) {
            return methodProxy(id, std::forward<const typename fakeit::production_arg<arglist>::type>(args)...);
        }
    };
}

namespace fakeit {

    class InvocationHandlers : public InvocationHandlerCollection {
        std::vector<std::shared_ptr<Destructible>> &_methodMocks;
        std::vector<unsigned int> &_offsets;

        unsigned int getOffset(unsigned int id) {
            unsigned int offset = 0;
            for (; offset < _offsets.size(); offset++) {
                if (_offsets[offset] == id) {
                    break;
                }
            }
            return offset;
        }

    public:
        InvocationHandlers(
                std::vector<std::shared_ptr<Destructible>> &methodMocks,
                std::vector<unsigned int> &offsets) :
                _methodMocks(methodMocks), _offsets(offsets) {
        }

        Destructible *getInvocatoinHandlerPtrById(unsigned int id) override {
            unsigned int offset = getOffset(id);
            std::shared_ptr<Destructible> ptr = _methodMocks[offset];
            return ptr.get();
        }

    };

    template<typename C, typename ... baseclasses>
    struct DynamicProxy {

        static_assert(std::is_polymorphic<C>::value, "DynamicProxy requires a polymorphic type");

        DynamicProxy(C &inst) :
                instance(inst),
                originalVtHandle(VirtualTable<C, baseclasses...>::getVTable(instance).createHandle()),
                _methodMocks(VTUtils::getVTSize<C>()),
                _offsets(VTUtils::getVTSize<C>()),
                _invocationHandlers(_methodMocks, _offsets) {
            _cloneVt.copyFrom(originalVtHandle.restore());
            _cloneVt.setCookie(InvocationHandlerCollection::VT_COOKIE_INDEX, &_invocationHandlers);
            getFake().setVirtualTable(_cloneVt);
        }

        void detach() {
            getFake().setVirtualTable(originalVtHandle.restore());
        }

        ~DynamicProxy() {
            _cloneVt.dispose();
        }

        C &get() {
            return instance;
        }

        void Reset() {
            _methodMocks = {{}};
            _methodMocks.resize(VTUtils::getVTSize<C>());
            _members = {};
            _offsets = {};
            _offsets.resize(VTUtils::getVTSize<C>());
            _cloneVt.copyFrom(originalVtHandle.restore());
        }

        template<int id, typename R, typename ... arglist>
        void stubMethod(R(C::*vMethod)(arglist...), MethodInvocationHandler<R, arglist...> *methodInvocationHandler) {
            auto offset = VTUtils::getOffset(vMethod);
            MethodProxyCreator<R, arglist...> creator;
            bind(creator.template createMethodProxy<id + 1>(offset), methodInvocationHandler);
        }

        void stubDtor(MethodInvocationHandler<void> *methodInvocationHandler) {
            auto offset = VTUtils::getDestructorOffset<C>();
            MethodProxyCreator<void> creator;
            bindDtor(creator.createMethodProxy<0>(offset), methodInvocationHandler);
        }

        template<typename R, typename ... arglist>
        bool isMethodStubbed(R(C::*vMethod)(arglist...)) {
            unsigned int offset = VTUtils::getOffset(vMethod);
            return isBinded(offset);
        }

        bool isDtorStubbed() {
            unsigned int offset = VTUtils::getDestructorOffset<C>();
            return isBinded(offset);
        }

        template<typename R, typename ... arglist>
        Destructible *getMethodMock(R(C::*vMethod)(arglist...)) {
            auto offset = VTUtils::getOffset(vMethod);
            std::shared_ptr<Destructible> ptr = _methodMocks[offset];
            return ptr.get();
        }

        Destructible *getDtorMock() {
            auto offset = VTUtils::getDestructorOffset<C>();
            std::shared_ptr<Destructible> ptr = _methodMocks[offset];
            return ptr.get();
        }

        template<typename DATA_TYPE, typename ... arglist>
        void stubDataMember(DATA_TYPE C::*member, const arglist &... initargs) {
            DATA_TYPE C::*theMember = (DATA_TYPE C::*) member;
            C &mock = get();
            DATA_TYPE *memberPtr = &(mock.*theMember);
            _members.push_back(
                    std::shared_ptr<DataMemeberWrapper < DATA_TYPE, arglist...> >
                    {new DataMemeberWrapper < DATA_TYPE, arglist...>(memberPtr,
                    initargs...)});
        }

        template<typename DATA_TYPE>
        void getMethodMocks(std::vector<DATA_TYPE> &into) const {
            for (std::shared_ptr<Destructible> ptr : _methodMocks) {
                DATA_TYPE p = dynamic_cast<DATA_TYPE>(ptr.get());
                if (p) {
                    into.push_back(p);
                }
            }
        }

        VirtualTable<C, baseclasses...> &getOriginalVT() {
            VirtualTable<C, baseclasses...> &vt = originalVtHandle.restore();
            return vt;
        }

    private:

        template<typename DATA_TYPE, typename ... arglist>
        class DataMemeberWrapper : public Destructible {
        private:
            DATA_TYPE *dataMember;
        public:
            DataMemeberWrapper(DATA_TYPE *dataMem, const arglist &... initargs) :
                    dataMember(dataMem) {
                new(dataMember) DATA_TYPE{initargs ...};
            }

            ~DataMemeberWrapper() override
            {
                dataMember->~DATA_TYPE();
            }
        };

        static_assert(sizeof(C) == sizeof(FakeObject<C, baseclasses...>), "This is a problem");

        C &instance;
        typename VirtualTable<C, baseclasses...>::Handle originalVtHandle;
        VirtualTable<C, baseclasses...> _cloneVt;

        std::vector<std::shared_ptr<Destructible>> _methodMocks;
        std::vector<std::shared_ptr<Destructible>> _members;
        std::vector<unsigned int> _offsets;
        InvocationHandlers _invocationHandlers;

        FakeObject<C, baseclasses...> &getFake() {
            return reinterpret_cast<FakeObject<C, baseclasses...> &>(instance);
        }

        void bind(const MethodProxy &methodProxy, Destructible *invocationHandler) {
            getFake().setMethod(methodProxy.getOffset(), methodProxy.getProxy());
            _methodMocks[methodProxy.getOffset()].reset(invocationHandler);
            _offsets[methodProxy.getOffset()] = methodProxy.getId();
        }

        void bindDtor(const MethodProxy &methodProxy, Destructible *invocationHandler) {
            getFake().setDtor(methodProxy.getProxy());
            _methodMocks[methodProxy.getOffset()].reset(invocationHandler);
            _offsets[methodProxy.getOffset()] = methodProxy.getId();
        }

        template<typename DATA_TYPE>
        DATA_TYPE getMethodMock(unsigned int offset) {
            std::shared_ptr<Destructible> ptr = _methodMocks[offset];
            return dynamic_cast<DATA_TYPE>(ptr.get());
        }

        template<typename BaseClass>
        void checkMultipleInheritance() {
            C *ptr = (C *) (unsigned int) 1;
            BaseClass *basePtr = ptr;
            int delta = (unsigned long) basePtr - (unsigned long) ptr;
            if (delta > 0) {


                throw std::invalid_argument(std::string("multiple inheritance is not supported"));
            }
        }

        bool isBinded(unsigned int offset) {
            std::shared_ptr<Destructible> ptr = _methodMocks[offset];
            return ptr.get() != nullptr;
        }

    };
}
#include <functional>
#include <type_traits>
#include <memory>
#include <iosfwd>
#include <vector>
#include <functional>
#include <tuple>
#include <tuple>

namespace fakeit {

    template<int N>
    struct apply_func {
        template<typename R, typename ... ArgsF, typename ... ArgsT, typename ... Args>
        static R applyTuple(std::function<R(ArgsF &...)> f, std::tuple<ArgsT...> &t, Args &... args) {
            return apply_func<N - 1>::template applyTuple(f, t, std::get<N - 1>(t), args...);
        }
    };

    template<>
    struct apply_func < 0 > {
        template<typename R, typename ... ArgsF, typename ... ArgsT, typename ... Args>
        static R applyTuple(std::function<R(ArgsF &...)> f, std::tuple<ArgsT...> & , Args &... args) {
            return f(args...);
        }
    };

    struct TupleDispatcher {

        template<typename R, typename ... ArgsF, typename ... ArgsT>
        static R applyTuple(std::function<R(ArgsF &...)> f, std::tuple<ArgsT...> &t) {
            return apply_func<sizeof...(ArgsT)>::template applyTuple(f, t);
        }

        template<typename R, typename ...arglist>
        static R invoke(std::function<R(arglist &...)> func, const std::tuple<arglist...> &arguments) {
            std::tuple<arglist...> &args = const_cast<std::tuple<arglist...> &>(arguments);
            return applyTuple(func, args);
        }

        template<typename TupleType, typename FunctionType>
        static void for_each(TupleType &&, FunctionType &,
            std::integral_constant<size_t, std::tuple_size<typename std::remove_reference<TupleType>::type>::value>) {
        }

        template<std::size_t I, typename TupleType, typename FunctionType, typename = typename std::enable_if<
            I != std::tuple_size<typename std::remove_reference<TupleType>::type>::value>::type>
            static void for_each(TupleType &&t, FunctionType &f, std::integral_constant<size_t, I>) {
            f(I, std::get < I >(t));
            for_each(std::forward < TupleType >(t), f, std::integral_constant<size_t, I + 1>());
        }

        template<typename TupleType, typename FunctionType>
        static void for_each(TupleType &&t, FunctionType &f) {
            for_each(std::forward < TupleType >(t), f, std::integral_constant<size_t, 0>());
        }

        template<typename TupleType1, typename TupleType2, typename FunctionType>
        static void for_each(TupleType1 &&, TupleType2 &&, FunctionType &,
            std::integral_constant<size_t, std::tuple_size<typename std::remove_reference<TupleType1>::type>::value>) {
        }

        template<std::size_t I, typename TupleType1, typename TupleType2, typename FunctionType, typename = typename std::enable_if<
            I != std::tuple_size<typename std::remove_reference<TupleType1>::type>::value>::type>
            static void for_each(TupleType1 &&t, TupleType2 &&t2, FunctionType &f, std::integral_constant<size_t, I>) {
            f(I, std::get < I >(t), std::get < I >(t2));
            for_each(std::forward < TupleType1 >(t), std::forward < TupleType2 >(t2), f, std::integral_constant<size_t, I + 1>());
        }

        template<typename TupleType1, typename TupleType2, typename FunctionType>
        static void for_each(TupleType1 &&t, TupleType2 &&t2, FunctionType &f) {
            for_each(std::forward < TupleType1 >(t), std::forward < TupleType2 >(t2), f, std::integral_constant<size_t, 0>());
        }
    };
}
namespace fakeit {

    template<typename R, typename ... arglist>
    struct ActualInvocationHandler : Destructible {
        virtual R handleMethodInvocation(ArgumentsTuple<arglist...> & args) = 0;
    };

}
#include <functional>
#include <tuple>
#include <string>
#include <iosfwd>
#include <type_traits>
#include <typeinfo>

namespace fakeit {

    struct DefaultValueInstatiationException {
        virtual ~DefaultValueInstatiationException() = default;

        virtual std::string what() const = 0;
    };


    template<class C>
    struct is_constructible_type {
        static const bool value =
                std::is_default_constructible<typename naked_type<C>::type>::value
                && !std::is_abstract<typename naked_type<C>::type>::value;
    };

    template<class C, class Enable = void>
    struct DefaultValue;

    template<class C>
    struct DefaultValue<C, typename std::enable_if<!is_constructible_type<C>::value>::type> {
        static C &value() {
            if (std::is_reference<C>::value) {
                typename naked_type<C>::type *ptr = nullptr;
                return *ptr;
            }

            class Exception : public DefaultValueInstatiationException {
                virtual std::string what() const

                override {
                    return (std::string("Type ") + std::string(typeid(C).name())
                            + std::string(
                            " is not default constructible. Could not instantiate a default return value")).c_str();
                }
            };

            throw Exception();
        }
    };

    template<class C>
    struct DefaultValue<C, typename std::enable_if<is_constructible_type<C>::value>::type> {
        static C &value() {
            static typename naked_type<C>::type val{};
            return val;
        }
    };


    template<>
    struct DefaultValue<void> {
        static void value() {
            return;
        }
    };

    template<>
    struct DefaultValue<bool> {
        static bool &value() {
            static bool value{false};
            return value;
        }
    };

    template<>
    struct DefaultValue<char> {
        static char &value() {
            static char value{0};
            return value;
        }
    };

    template<>
    struct DefaultValue<char16_t> {
        static char16_t &value() {
            static char16_t value{0};
            return value;
        }
    };

    template<>
    struct DefaultValue<char32_t> {
        static char32_t &value() {
            static char32_t value{0};
            return value;
        }
    };

    template<>
    struct DefaultValue<wchar_t> {
        static wchar_t &value() {
            static wchar_t value{0};
            return value;
        }
    };

    template<>
    struct DefaultValue<short> {
        static short &value() {
            static short value{0};
            return value;
        }
    };

    template<>
    struct DefaultValue<int> {
        static int &value() {
            static int value{0};
            return value;
        }
    };

    template<>
    struct DefaultValue<long> {
        static long &value() {
            static long value{0};
            return value;
        }
    };

    template<>
    struct DefaultValue<long long> {
        static long long &value() {
            static long long value{0};
            return value;
        }
    };

    template<>
    struct DefaultValue<std::string> {
        static std::string &value() {
            static std::string value{};
            return value;
        }
    };

}
namespace fakeit {

    struct IMatcher : Destructible {
        ~IMatcher() = default;
        virtual std::string format() const = 0;
    };

    template<typename T>
    struct TypedMatcher : IMatcher {
        virtual bool matches(const T &actual) const = 0;
    };

    template<typename T>
    struct TypedMatcherCreator {

        virtual ~TypedMatcherCreator() = default;

        virtual TypedMatcher<T> *createMatcher() const = 0;
    };

    template<typename T>
    struct ComparisonMatcherCreator : public TypedMatcherCreator<T> {

        virtual ~ComparisonMatcherCreator() = default;

        ComparisonMatcherCreator(const T &arg)
                : _expected(arg) {
        }

        struct Matcher : public TypedMatcher<T> {
            Matcher(const T &expected)
                    : _expected(expected) {
            }

            const T _expected;
        };

        const T &_expected;
    };

    namespace internal {
        template<typename T>
        struct TypedAnyMatcher : public TypedMatcherCreator<T> {

            virtual ~TypedAnyMatcher() = default;

            TypedAnyMatcher() {
            }

            struct Matcher : public TypedMatcher<T> {
                virtual bool matches(const T &) const {
                    return true;
                }

                virtual std::string format() const override {
                    return "Any";
                }
            };

            virtual TypedMatcher<T> *createMatcher() const override {
                return new Matcher();
            }

        };

        template<typename T>
        struct EqMatcherCreator : public ComparisonMatcherCreator<T> {

            virtual ~EqMatcherCreator() = default;

            EqMatcherCreator(const T &expected)
                    : ComparisonMatcherCreator<T>(expected) {
            }

            struct Matcher : public ComparisonMatcherCreator<T>::Matcher {
                Matcher(const T &expected)
                        : ComparisonMatcherCreator<T>::Matcher(expected) {
                }

                virtual std::string format() const override {
                    return TypeFormatter<T>::format(this->_expected);
                }

                virtual bool matches(const T &actual) const override {
                    return actual == this->_expected;
                }
            };

            virtual TypedMatcher<T> *createMatcher() const {
                return new Matcher(this->_expected);
            }

        };

        template<typename T>
        struct GtMatcherCreator : public ComparisonMatcherCreator<T> {

            virtual ~GtMatcherCreator() = default;

            GtMatcherCreator(const T &expected)
                    : ComparisonMatcherCreator<T>(expected) {
            }

            struct Matcher : public ComparisonMatcherCreator<T>::Matcher {
                Matcher(const T &expected)
                        : ComparisonMatcherCreator<T>::Matcher(expected) {
                }

                virtual bool matches(const T &actual) const override {
                    return actual > this->_expected;
                }

                virtual std::string format() const override {
                    return std::string(">") + TypeFormatter<T>::format(this->_expected);
                }
            };

            virtual TypedMatcher<T> *createMatcher() const override {
                return new Matcher(this->_expected);
            }
        };

        template<typename T>
        struct GeMatcherCreator : public ComparisonMatcherCreator<T> {

            virtual ~GeMatcherCreator() = default;

            GeMatcherCreator(const T &expected)
                    : ComparisonMatcherCreator<T>(expected) {
            }

            struct Matcher : public ComparisonMatcherCreator<T>::Matcher {
                Matcher(const T &expected)
                        : ComparisonMatcherCreator<T>::Matcher(expected) {
                }

                virtual bool matches(const T &actual) const override {
                    return actual >= this->_expected;
                }

                virtual std::string format() const override {
                    return std::string(">=") + TypeFormatter<T>::format(this->_expected);
                }
            };

            virtual TypedMatcher<T> *createMatcher() const override {
                return new Matcher(this->_expected);
            }
        };

        template<typename T>
        struct LtMatcherCreator : public ComparisonMatcherCreator<T> {

            virtual ~LtMatcherCreator() = default;

            LtMatcherCreator(const T &expected)
                    : ComparisonMatcherCreator<T>(expected) {
            }

            struct Matcher : public ComparisonMatcherCreator<T>::Matcher {
                Matcher(const T &expected)
                        : ComparisonMatcherCreator<T>::Matcher(expected) {
                }

                virtual bool matches(const T &actual) const override {
                    return actual < this->_expected;
                }

                virtual std::string format() const override {
                    return std::string("<") + TypeFormatter<T>::format(this->_expected);
                }
            };

            virtual TypedMatcher<T> *createMatcher() const override {
                return new Matcher(this->_expected);
            }

        };

        template<typename T>
        struct LeMatcherCreator : public ComparisonMatcherCreator<T> {

            virtual ~LeMatcherCreator() = default;

            LeMatcherCreator(const T &expected)
                    : ComparisonMatcherCreator<T>(expected) {
            }

            struct Matcher : public ComparisonMatcherCreator<T>::Matcher {
                Matcher(const T &expected)
                        : ComparisonMatcherCreator<T>::Matcher(expected) {
                }

                virtual bool matches(const T &actual) const override {
                    return actual <= this->_expected;
                }

                virtual std::string format() const override {
                    return std::string("<=") + TypeFormatter<T>::format(this->_expected);
                }
            };

            virtual TypedMatcher<T> *createMatcher() const override {
                return new Matcher(this->_expected);
            }

        };

        template<typename T>
        struct NeMatcherCreator : public ComparisonMatcherCreator<T> {

            virtual ~NeMatcherCreator() = default;

            NeMatcherCreator(const T &expected)
                    : ComparisonMatcherCreator<T>(expected) {
            }

            struct Matcher : public ComparisonMatcherCreator<T>::Matcher {
                Matcher(const T &expected)
                        : ComparisonMatcherCreator<T>::Matcher(expected) {
                }

                virtual bool matches(const T &actual) const override {
                    return actual != this->_expected;
                }

                virtual std::string format() const override {
                    return std::string("!=") + TypeFormatter<T>::format(this->_expected);
                }

            };

            virtual TypedMatcher<T> *createMatcher() const override {
                return new Matcher(this->_expected);
            }

        };
    }

    struct AnyMatcher {
    } static _;

    template<typename T>
    internal::TypedAnyMatcher<T> Any() {
        internal::TypedAnyMatcher<T> rv;
        return rv;
    }

    template<typename T>
    internal::EqMatcherCreator<T> Eq(const T &arg) {
        internal::EqMatcherCreator<T> rv(arg);
        return rv;
    }

    template<typename T>
    internal::GtMatcherCreator<T> Gt(const T &arg) {
        internal::GtMatcherCreator<T> rv(arg);
        return rv;
    }

    template<typename T>
    internal::GeMatcherCreator<T> Ge(const T &arg) {
        internal::GeMatcherCreator<T> rv(arg);
        return rv;
    }

    template<typename T>
    internal::LtMatcherCreator<T> Lt(const T &arg) {
        internal::LtMatcherCreator<T> rv(arg);
        return rv;
    }

    template<typename T>
    internal::LeMatcherCreator<T> Le(const T &arg) {
        internal::LeMatcherCreator<T> rv(arg);
        return rv;
    }

    template<typename T>
    internal::NeMatcherCreator<T> Ne(const T &arg) {
        internal::NeMatcherCreator<T> rv(arg);
        return rv;
    }

}

namespace fakeit {

    template<typename ... arglist>
    struct ArgumentsMatcherInvocationMatcher : public ActualInvocation<arglist...>::Matcher {

        virtual ~ArgumentsMatcherInvocationMatcher() {
            for (unsigned int i = 0; i < _matchers.size(); i++)
                delete _matchers[i];
        }

        ArgumentsMatcherInvocationMatcher(const std::vector<Destructible *> &args)
                : _matchers(args) {
        }

        virtual bool matches(ActualInvocation<arglist...> &invocation) override {
            if (invocation.getActualMatcher() == this)
                return true;
            return matches(invocation.getActualArguments());
        }

        virtual std::string format() const override {
            std::ostringstream out;
            out << "(";
            for (unsigned int i = 0; i < _matchers.size(); i++) {
                if (i > 0) out << ", ";
                IMatcher *m = dynamic_cast<IMatcher *>(_matchers[i]);
                out << m->format();
            }
            out << ")";
            return out.str();
        }

    private:

        struct MatchingLambda {
            MatchingLambda(const std::vector<Destructible *> &matchers)
                    : _matchers(matchers) {
            }

            template<typename A>
            void operator()(int index, A &actualArg) {
                TypedMatcher<typename naked_type<A>::type> *matcher =
                        dynamic_cast<TypedMatcher<typename naked_type<A>::type> *>(_matchers[index]);
                if (_matching)
                    _matching = matcher->matches(actualArg);
            }

            bool isMatching() {
                return _matching;
            }

        private:
            bool _matching = true;
            const std::vector<Destructible *> &_matchers;
        };

        virtual bool matches(ArgumentsTuple<arglist...>& actualArguments) {
            MatchingLambda l(_matchers);
            fakeit::TupleDispatcher::for_each(actualArguments, l);
            return l.isMatching();
        }

        const std::vector<Destructible *> _matchers;
    };
































    template<typename ... arglist>
    struct UserDefinedInvocationMatcher : ActualInvocation<arglist...>::Matcher {
        virtual ~UserDefinedInvocationMatcher() = default;

        UserDefinedInvocationMatcher(std::function<bool(arglist &...)> match)
                : matcher{match} {
        }

        virtual bool matches(ActualInvocation<arglist...> &invocation) override {
            if (invocation.getActualMatcher() == this)
                return true;
            return matches(invocation.getActualArguments());
        }

        virtual std::string format() const override {
            return {"( user defined matcher )"};
        }

    private:
        virtual bool matches(ArgumentsTuple<arglist...>& actualArguments) {
            return TupleDispatcher::invoke<bool, typename tuple_arg<arglist>::type...>(matcher, actualArguments);
        }

        const std::function<bool(arglist &...)> matcher;
    };

    template<typename ... arglist>
    struct DefaultInvocationMatcher : public ActualInvocation<arglist...>::Matcher {

        virtual ~DefaultInvocationMatcher() = default;

        DefaultInvocationMatcher() {
        }

        virtual bool matches(ActualInvocation<arglist...> &invocation) override {
            return matches(invocation.getActualArguments());
        }

        virtual std::string format() const override {
            return {"( Any arguments )"};
        }

    private:

        virtual bool matches(const ArgumentsTuple<arglist...>&) {
            return true;
        }
    };

}

namespace fakeit {


    template<typename R, typename ... arglist>
    class RecordedMethodBody : public MethodInvocationHandler<R, arglist...>, public ActualInvocationsSource {

        struct MatchedInvocationHandler : ActualInvocationHandler<R, arglist...> {

            virtual ~MatchedInvocationHandler() = default;

            MatchedInvocationHandler(typename ActualInvocation<arglist...>::Matcher *matcher,
                ActualInvocationHandler<R, arglist...> *invocationHandler) :
                    _matcher{matcher}, _invocationHandler{invocationHandler} {
            }

            virtual R handleMethodInvocation(ArgumentsTuple<arglist...> & args) override
            {
                Destructible &destructable = *_invocationHandler;
                ActualInvocationHandler<R, arglist...> &invocationHandler = dynamic_cast<ActualInvocationHandler<R, arglist...> &>(destructable);
                return invocationHandler.handleMethodInvocation(args);
            }

            typename ActualInvocation<arglist...>::Matcher &getMatcher() const {
                Destructible &destructable = *_matcher;
                typename ActualInvocation<arglist...>::Matcher &matcher = dynamic_cast<typename ActualInvocation<arglist...>::Matcher &>(destructable);
                return matcher;
            }

        private:
            std::shared_ptr<Destructible> _matcher;
            std::shared_ptr<Destructible> _invocationHandler;
        };


        FakeitContext &_fakeit;
        MethodInfo _method;

        std::vector<std::shared_ptr<Destructible>> _invocationHandlers;
        std::vector<std::shared_ptr<Destructible>> _actualInvocations;

        MatchedInvocationHandler *buildMatchedInvocationHandler(
                typename ActualInvocation<arglist...>::Matcher *invocationMatcher,
                ActualInvocationHandler<R, arglist...> *invocationHandler) {
            return new MatchedInvocationHandler(invocationMatcher, invocationHandler);
        }

        MatchedInvocationHandler *getInvocationHandlerForActualArgs(ActualInvocation<arglist...> &invocation) {
            for (auto i = _invocationHandlers.rbegin(); i != _invocationHandlers.rend(); ++i) {
                std::shared_ptr<Destructible> curr = *i;
                Destructible &destructable = *curr;
                MatchedInvocationHandler &im = asMatchedInvocationHandler(destructable);
                if (im.getMatcher().matches(invocation)) {
                    return &im;
                }
            }
            return nullptr;
        }

        MatchedInvocationHandler &asMatchedInvocationHandler(Destructible &destructable) {
            MatchedInvocationHandler &im = dynamic_cast<MatchedInvocationHandler &>(destructable);
            return im;
        }

        ActualInvocation<arglist...> &asActualInvocation(Destructible &destructable) const {
            ActualInvocation<arglist...> &invocation = dynamic_cast<ActualInvocation<arglist...> &>(destructable);
            return invocation;
        }

    public:

        RecordedMethodBody(FakeitContext &fakeit, std::string name) :
                _fakeit(fakeit), _method{MethodInfo::nextMethodOrdinal(), name} { }

        virtual ~RecordedMethodBody() NO_THROWS {
        }

        MethodInfo &getMethod() {
            return _method;
        }

        bool isOfMethod(MethodInfo &method) {

            return method.id() == _method.id();
        }

        void addMethodInvocationHandler(typename ActualInvocation<arglist...>::Matcher *matcher,
            ActualInvocationHandler<R, arglist...> *invocationHandler) {
            ActualInvocationHandler<R, arglist...> *mock = buildMatchedInvocationHandler(matcher, invocationHandler);
            std::shared_ptr<Destructible> destructable{mock};
            _invocationHandlers.push_back(destructable);
        }

        void clear() {
            _invocationHandlers.clear();
            _actualInvocations.clear();
        }


        R handleMethodInvocation(const typename fakeit::production_arg<arglist>::type... args) override {
            unsigned int ordinal = Invocation::nextInvocationOrdinal();
            MethodInfo &method = this->getMethod();
            auto actualInvocation = new ActualInvocation<arglist...>(ordinal, method, std::forward<const typename fakeit::production_arg<arglist>::type>(args)...);


            std::shared_ptr<Destructible> actualInvocationDtor{actualInvocation};

            auto invocationHandler = getInvocationHandlerForActualArgs(*actualInvocation);
            if (invocationHandler) {
                auto &matcher = invocationHandler->getMatcher();
                actualInvocation->setActualMatcher(&matcher);
                _actualInvocations.push_back(actualInvocationDtor);
                try {
                    return invocationHandler->handleMethodInvocation(actualInvocation->getActualArguments());
                } catch (NoMoreRecordedActionException &) {
                }
            }

            UnexpectedMethodCallEvent event(UnexpectedType::Unmatched, *actualInvocation);
            _fakeit.handle(event);
            std::string format{_fakeit.format(event)};
            UnexpectedMethodCallException e(format);
            throw e;
        }

        void scanActualInvocations(const std::function<void(ActualInvocation<arglist...> &)> &scanner) {
            for (auto destructablePtr : _actualInvocations) {
                ActualInvocation<arglist...> &invocation = asActualInvocation(*destructablePtr);
                scanner(invocation);
            }
        }

        void getActualInvocations(std::unordered_set<Invocation *> &into) const override {
            for (auto destructablePtr : _actualInvocations) {
                Invocation &invocation = asActualInvocation(*destructablePtr);
                into.insert(&invocation);
            }
        }

        void setMethodDetails(const std::string &mockName, const std::string &methodName) {
            const std::string fullName{mockName + "." + methodName};
            _method.setName(fullName);
        }

    };

}
#include <functional>
#include <type_traits>
#include <stdexcept>
#include <utility>
#include <functional>
#include <type_traits>

namespace fakeit {

    struct Quantity {
        Quantity(const int q) :
                quantity(q) {
        }

        const int quantity;
    } static Once(1);

    template<typename R>
    struct Quantifier : public Quantity {
        Quantifier(const int q, const R &val) :
                Quantity(q), value(val) {
        }

        const R &value;
    };

    template<>
    struct Quantifier<void> : public Quantity {
        explicit Quantifier(const int q) :
                Quantity(q) {
        }
    };

    struct QuantifierFunctor : public Quantifier<void> {
        QuantifierFunctor(const int q) :
                Quantifier<void>(q) {
        }

        template<typename R>
        Quantifier<R> operator()(const R &value) {
            return Quantifier<R>(quantity, value);
        }
    };

    template<int q>
    struct Times : public Quantity {

        Times<q>() : Quantity(q) { }

        template<typename R>
        static Quantifier<R> of(const R &value) {
            return Quantifier<R>(q, value);
        }

        static Quantifier<void> Void() {
            return Quantifier<void>(q);
        }
    };

#if defined (__GNUG__) || (_MSC_VER >= 1900)

    inline QuantifierFunctor operator
    ""

    _Times(unsigned long long n) {
        return QuantifierFunctor((int) n);
    }

    inline QuantifierFunctor operator
    ""

    _Time(unsigned long long n) {
        if (n != 1)
            throw std::invalid_argument("Only 1_Time is supported. Use X_Times (with s) if X is bigger than 1");
        return QuantifierFunctor((int) n);
    }

#endif

}
#include <functional>
#include <atomic>
#include <tuple>
#include <type_traits>


namespace fakeit {

    template<typename R, typename ... arglist>
    struct Action : Destructible {
        virtual R invoke(const ArgumentsTuple<arglist...> &) = 0;

        virtual bool isDone() = 0;
    };

    template<typename R, typename ... arglist>
    struct Repeat : Action<R, arglist...> {
        virtual ~Repeat() = default;

        Repeat(std::function<R(typename fakeit::test_arg<arglist>::type...)> func) :
                f(func), times(1) {
        }

        Repeat(std::function<R(typename fakeit::test_arg<arglist>::type...)> func, long t) :
                f(func), times(t) {
        }

        virtual R invoke(const ArgumentsTuple<arglist...> & args) override {
            times--;
            return TupleDispatcher::invoke<R, arglist...>(f, args);
        }

        virtual bool isDone() override {
            return times == 0;
        }

    private:
        std::function<R(typename fakeit::test_arg<arglist>::type...)> f;
        long times;
    };

    template<typename R, typename ... arglist>
    struct RepeatForever : public Action<R, arglist...> {

        virtual ~RepeatForever() = default;

        RepeatForever(std::function<R(typename fakeit::test_arg<arglist>::type...)> func) :
                f(func) {
        }

        virtual R invoke(const ArgumentsTuple<arglist...> & args) override {
            return TupleDispatcher::invoke<R, arglist...>(f, args);
        }

        virtual bool isDone() override {
            return false;
        }

    private:
        std::function<R(typename fakeit::test_arg<arglist>::type...)> f;
    };

    template<typename R, typename ... arglist>
    struct ReturnDefaultValue : public Action<R, arglist...> {
        virtual ~ReturnDefaultValue() = default;

        virtual R invoke(const ArgumentsTuple<arglist...> &) override {
            return DefaultValue<R>::value();
        }

        virtual bool isDone() override {
            return false;
        }
    };

    template<typename R, typename ... arglist>
    struct ReturnDelegateValue : public Action<R, arglist...> {

        ReturnDelegateValue(std::function<R(const typename fakeit::test_arg<arglist>::type...)> delegate) : _delegate(delegate) { }

        virtual ~ReturnDelegateValue() = default;

        virtual R invoke(const ArgumentsTuple<arglist...> & args) override {
            return TupleDispatcher::invoke<R, arglist...>(_delegate, args);
        }

        virtual bool isDone() override {
            return false;
        }

    private:
        std::function<R(const typename fakeit::test_arg<arglist>::type...)> _delegate;
    };

}

namespace fakeit {

    template<typename R, typename ... arglist>
    struct MethodStubbingProgress {

        virtual ~MethodStubbingProgress() THROWS {
        }

        template<typename U = R>
        typename std::enable_if<!std::is_reference<U>::value, MethodStubbingProgress<R, arglist...> &>::type
        Return(const R &r) {
            return Do([r](const typename fakeit::test_arg<arglist>::type...) -> R { return r; });
        }

        template<typename U = R>
        typename std::enable_if<std::is_reference<U>::value, MethodStubbingProgress<R, arglist...> &>::type
        Return(const R &r) {
            return Do([&r](const typename fakeit::test_arg<arglist>::type...) -> R { return r; });
        }

        MethodStubbingProgress<R, arglist...> &
        Return(const Quantifier<R> &q) {
            const R &value = q.value;
            auto method = [value](const arglist &...) -> R { return value; };
            return DoImpl(new Repeat<R, arglist...>(method, q.quantity));
        }

        template<typename first, typename second, typename ... tail>
        MethodStubbingProgress<R, arglist...> &
        Return(const first &f, const second &s, const tail &... t) {
            Return(f);
            return Return(s, t...);
        }


        template<typename U = R>
        typename std::enable_if<!std::is_reference<U>::value, void>::type
        AlwaysReturn(const R &r) {
            return AlwaysDo([r](const typename fakeit::test_arg<arglist>::type...) -> R { return r; });
        }

        template<typename U = R>
        typename std::enable_if<std::is_reference<U>::value, void>::type
        AlwaysReturn(const R &r) {
            return AlwaysDo([&r](const typename fakeit::test_arg<arglist>::type...) -> R { return r; });
        }

        MethodStubbingProgress<R, arglist...> &
        Return() {
            return Do([](const typename fakeit::test_arg<arglist>::type...) -> R { return DefaultValue<R>::value(); });
        }

        void AlwaysReturn() {
            return AlwaysDo([](const typename fakeit::test_arg<arglist>::type...) -> R { return DefaultValue<R>::value(); });
        }

        template<typename E>
        MethodStubbingProgress<R, arglist...> &Throw(const E &e) {
            return Do([e](const typename fakeit::test_arg<arglist>::type...) -> R { throw e; });
        }

        template<typename E>
        MethodStubbingProgress<R, arglist...> &
        Throw(const Quantifier<E> &q) {
            const E &value = q.value;
            auto method = [value](const arglist &...) -> R { throw value; };
            return DoImpl(new Repeat<R, arglist...>(method, q.quantity));
        }

        template<typename first, typename second, typename ... tail>
        MethodStubbingProgress<R, arglist...> &
        Throw(const first &f, const second &s, const tail &... t) {
            Throw(f);
            return Throw(s, t...);
        }

        template<typename E>
        void AlwaysThrow(const E &e) {
            return AlwaysDo([e](const typename fakeit::test_arg<arglist>::type...) -> R { throw e; });
        }

        virtual MethodStubbingProgress<R, arglist...> &
            Do(std::function<R(const typename fakeit::test_arg<arglist>::type...)> method) {
            return DoImpl(new Repeat<R, arglist...>(method));
        }

        template<typename F>
        MethodStubbingProgress<R, arglist...> &
        Do(const Quantifier<F> &q) {
            return DoImpl(new Repeat<R, arglist...>(q.value, q.quantity));
        }

        template<typename first, typename second, typename ... tail>
        MethodStubbingProgress<R, arglist...> &
        Do(const first &f, const second &s, const tail &... t) {
            Do(f);
            return Do(s, t...);
        }

        virtual void AlwaysDo(std::function<R(const typename fakeit::test_arg<arglist>::type...)> method) {
            DoImpl(new RepeatForever<R, arglist...>(method));
        }

    protected:

        virtual MethodStubbingProgress<R, arglist...> &DoImpl(Action<R, arglist...> *action) = 0;

    private:
        MethodStubbingProgress &operator=(const MethodStubbingProgress &other) = delete;
    };


    template<typename ... arglist>
    struct MethodStubbingProgress<void, arglist...> {

        virtual ~MethodStubbingProgress() THROWS {
        }

        MethodStubbingProgress<void, arglist...> &Return() {
            auto lambda = [](const typename fakeit::test_arg<arglist>::type...) -> void {
                return DefaultValue<void>::value();
            };
            return Do(lambda);
        }

        virtual MethodStubbingProgress<void, arglist...> &Do(
            std::function<void(const typename fakeit::test_arg<arglist>::type...)> method) {
            return DoImpl(new Repeat<void, arglist...>(method));
        }


        void AlwaysReturn() {
            return AlwaysDo([](const typename fakeit::test_arg<arglist>::type...) -> void { return DefaultValue<void>::value(); });
        }

        MethodStubbingProgress<void, arglist...> &
        Return(const Quantifier<void> &q) {
            auto method = [](const arglist &...) -> void { return DefaultValue<void>::value(); };
            return DoImpl(new Repeat<void, arglist...>(method, q.quantity));
        }

        template<typename E>
        MethodStubbingProgress<void, arglist...> &Throw(const E &e) {
            return Do([e](const typename fakeit::test_arg<arglist>::type...) -> void { throw e; });
        }

        template<typename E>
        MethodStubbingProgress<void, arglist...> &
        Throw(const Quantifier<E> &q) {
            const E &value = q.value;
            auto method = [value](const typename fakeit::test_arg<arglist>::type...) -> void { throw value; };
            return DoImpl(new Repeat<void, arglist...>(method, q.quantity));
        }

        template<typename first, typename second, typename ... tail>
        MethodStubbingProgress<void, arglist...> &
        Throw(const first &f, const second &s, const tail &... t) {
            Throw(f);
            return Throw(s, t...);
        }

        template<typename E>
        void AlwaysThrow(const E e) {
            return AlwaysDo([e](const typename fakeit::test_arg<arglist>::type...) -> void { throw e; });
        }

           template<typename F>
        MethodStubbingProgress<void, arglist...> &
        Do(const Quantifier<F> &q) {
            return DoImpl(new Repeat<void, arglist...>(q.value, q.quantity));
        }

        template<typename first, typename second, typename ... tail>
        MethodStubbingProgress<void, arglist...> &
        Do(const first &f, const second &s, const tail &... t) {
            Do(f);
            return Do(s, t...);
        }

        virtual void AlwaysDo(std::function<void(const typename fakeit::test_arg<arglist>::type...)> method) {
            DoImpl(new RepeatForever<void, arglist...>(method));
        }

    protected:

        virtual MethodStubbingProgress<void, arglist...> &DoImpl(Action<void, arglist...> *action) = 0;

    private:
        MethodStubbingProgress &operator=(const MethodStubbingProgress &other) = delete;
    };


}
#include <vector>
#include <functional>

namespace fakeit {

    class Finally {
    private:
        std::function<void()> _finallyClause;

        Finally(const Finally &);

        Finally &operator=(const Finally &);

    public:
        explicit Finally(std::function<void()> f) :
                _finallyClause(f) {
        }

        ~Finally() {
            _finallyClause();
        }
    };
}

namespace fakeit {


    template<typename R, typename ... arglist>
    struct ActionSequence : ActualInvocationHandler<R,arglist...> {

        ActionSequence() {
            clear();
        }

        void AppendDo(Action<R, arglist...> *action) {
            append(action);
        }

        virtual R handleMethodInvocation(ArgumentsTuple<arglist...> & args) override
        {
            std::shared_ptr<Destructible> destructablePtr = _recordedActions.front();
            Destructible &destructable = *destructablePtr;
            Action<R, arglist...> &action = dynamic_cast<Action<R, arglist...> &>(destructable);
            std::function<void()> finallyClause = [&]() -> void {
                if (action.isDone())
                    _recordedActions.erase(_recordedActions.begin());
            };
            Finally onExit(finallyClause);
            return action.invoke(args);
        }

    private:

        struct NoMoreRecordedAction : Action<R, arglist...> {







            virtual R invoke(const ArgumentsTuple<arglist...> &) override {
                throw NoMoreRecordedActionException();
            }

            virtual bool isDone() override {
                return false;
            }
        };

        void append(Action<R, arglist...> *action) {
            std::shared_ptr<Destructible> destructable{action};
            _recordedActions.insert(_recordedActions.end() - 1, destructable);
        }

        void clear() {
            _recordedActions.clear();
            auto actionPtr = std::shared_ptr<Destructible> {new NoMoreRecordedAction()};
            _recordedActions.push_back(actionPtr);
        }

        std::vector<std::shared_ptr<Destructible>> _recordedActions;
    };

}

namespace fakeit {

    template<typename C, typename DATA_TYPE>
    class DataMemberStubbingRoot {
    private:

    public:
        DataMemberStubbingRoot(const DataMemberStubbingRoot &) = default;

        DataMemberStubbingRoot() = default;

        void operator=(const DATA_TYPE&) {
        }
    };

}
#include <functional>
#include <utility>
#include <type_traits>
#include <tuple>
#include <memory>
#include <vector>
#include <unordered_set>
#include <set>
#include <iosfwd>

namespace fakeit {

    struct Xaction {
        virtual void commit() = 0;
    };
}

namespace fakeit {


    template<typename R, typename ... arglist>
    struct SpyingContext : Xaction {
        virtual void appendAction(Action<R, arglist...> *action) = 0;

        virtual std::function<R(arglist&...)> getOriginalMethod() = 0;
    };
}
namespace fakeit {


    template<typename R, typename ... arglist>
    struct StubbingContext : public Xaction {
        virtual void appendAction(Action<R, arglist...> *action) = 0;
    };
}
#include <functional>
#include <type_traits>
#include <tuple>
#include <memory>
#include <vector>
#include <unordered_set>


namespace fakeit {

    template<unsigned int index, typename ... arglist>
    class MatchersCollector {

        std::vector<Destructible *> &_matchers;

    public:


        template<std::size_t N>
        using ArgType = typename std::tuple_element<N, std::tuple<arglist...>>::type;

        template<std::size_t N>
        using NakedArgType = typename naked_type<ArgType<index>>::type;

        template<std::size_t N>
        using ArgMatcherCreatorType = decltype(std::declval<TypedMatcherCreator<NakedArgType<N>>>());

        MatchersCollector(std::vector<Destructible *> &matchers)
                : _matchers(matchers) {
        }

        void CollectMatchers() {
        }

        template<typename Head>
        typename std::enable_if<
                std::is_constructible<NakedArgType<index>, Head>::value, void>
        ::type CollectMatchers(const Head &value) {

            TypedMatcher<NakedArgType<index>> *d = Eq<NakedArgType<index>>(value).createMatcher();
            _matchers.push_back(d);
        }

        template<typename Head, typename ...Tail>
        typename std::enable_if<
                std::is_constructible<NakedArgType<index>, Head>::value
                , void>
        ::type CollectMatchers(const Head &head, const Tail &... tail) {
            CollectMatchers(head);
            MatchersCollector<index + 1, arglist...> c(_matchers);
            c.CollectMatchers(tail...);
        }

        template<typename Head>
        typename std::enable_if<
                std::is_base_of<TypedMatcherCreator<NakedArgType<index>>, Head>::value, void>
        ::type CollectMatchers(const Head &creator) {
            TypedMatcher<NakedArgType<index>> *d = creator.createMatcher();
            _matchers.push_back(d);
        }

        template<typename Head, typename ...Tail>

        typename std::enable_if<
                std::is_base_of<TypedMatcherCreator<NakedArgType<index>>, Head>::value, void>
        ::type CollectMatchers(const Head &head, const Tail &... tail) {
            CollectMatchers(head);
            MatchersCollector<index + 1, arglist...> c(_matchers);
            c.CollectMatchers(tail...);
        }

        template<typename Head>
        typename std::enable_if<
                std::is_same<AnyMatcher, Head>::value, void>
        ::type CollectMatchers(const Head &) {
            TypedMatcher<NakedArgType<index>> *d = Any<NakedArgType<index>>().createMatcher();
            _matchers.push_back(d);
        }

        template<typename Head, typename ...Tail>
        typename std::enable_if<
                std::is_same<AnyMatcher, Head>::value, void>
        ::type CollectMatchers(const Head &head, const Tail &... tail) {
            CollectMatchers(head);
            MatchersCollector<index + 1, arglist...> c(_matchers);
            c.CollectMatchers(tail...);
        }

    };

}

namespace fakeit {

    template<typename R, typename ... arglist>
    class MethodMockingContext :
            public Sequence,
            public ActualInvocationsSource,
            public virtual StubbingContext<R, arglist...>,
            public virtual SpyingContext<R, arglist...>,
            private Invocation::Matcher {
    public:

        struct Context : Destructible {


            virtual typename std::function<R(arglist&...)> getOriginalMethod() = 0;

            virtual std::string getMethodName() = 0;

            virtual void addMethodInvocationHandler(typename ActualInvocation<arglist...>::Matcher *matcher,
                ActualInvocationHandler<R, arglist...> *invocationHandler) = 0;

            virtual void scanActualInvocations(const std::function<void(ActualInvocation<arglist...> &)> &scanner) = 0;

            virtual void setMethodDetails(std::string mockName, std::string methodName) = 0;

            virtual bool isOfMethod(MethodInfo &method) = 0;

            virtual ActualInvocationsSource &getInvolvedMock() = 0;
        };

    private:
        class Implementation {

            Context *_stubbingContext;
            ActionSequence<R, arglist...> *_recordedActionSequence;
            typename ActualInvocation<arglist...>::Matcher *_invocationMatcher;
            bool _commited;

            Context &getStubbingContext() const {
                return *_stubbingContext;
            }

        public:

            Implementation(Context *stubbingContext)
                    : _stubbingContext(stubbingContext),
                      _recordedActionSequence(new ActionSequence<R, arglist...>()),
                      _invocationMatcher
                              {
                                      new DefaultInvocationMatcher<arglist...>()}, _commited(false) {
            }

            ~Implementation() {
                delete _stubbingContext;
                if (!_commited) {

                    delete _recordedActionSequence;
                    delete _invocationMatcher;
                }
            }

            ActionSequence<R, arglist...> &getRecordedActionSequence() {
                return *_recordedActionSequence;
            }

            std::string format() const {
                std::string s = getStubbingContext().getMethodName();
                s += _invocationMatcher->format();
                return s;
            }

            void getActualInvocations(std::unordered_set<Invocation *> &into) const {
                auto scanner = [&](ActualInvocation<arglist...> &a) {
                    if (_invocationMatcher->matches(a)) {
                        into.insert(&a);
                    }
                };
                getStubbingContext().scanActualInvocations(scanner);
            }


            bool matches(Invocation &invocation) {
                MethodInfo &actualMethod = invocation.getMethod();
                if (!getStubbingContext().isOfMethod(actualMethod)) {
                    return false;
                }

                ActualInvocation<arglist...> &actualInvocation = dynamic_cast<ActualInvocation<arglist...> &>(invocation);
                return _invocationMatcher->matches(actualInvocation);
            }

            void commit() {
                getStubbingContext().addMethodInvocationHandler(_invocationMatcher, _recordedActionSequence);
                _commited = true;
            }

            void appendAction(Action<R, arglist...> *action) {
                getRecordedActionSequence().AppendDo(action);
            }

            void setMethodBodyByAssignment(std::function<R(const typename fakeit::test_arg<arglist>::type...)> method) {
                appendAction(new RepeatForever<R, arglist...>(method));
                commit();
            }

            void setMethodDetails(std::string mockName, std::string methodName) {
                getStubbingContext().setMethodDetails(mockName, methodName);
            }

            void getInvolvedMocks(std::vector<ActualInvocationsSource *> &into) const {
                into.push_back(&getStubbingContext().getInvolvedMock());
            }

            typename std::function<R(arglist &...)> getOriginalMethod() {
                return getStubbingContext().getOriginalMethod();
            }

            void setInvocationMatcher(typename ActualInvocation<arglist...>::Matcher *matcher) {
                delete _invocationMatcher;
                _invocationMatcher = matcher;
            }
        };

    protected:

        MethodMockingContext(Context *stubbingContext)
                : _impl{new Implementation(stubbingContext)} {
        }

        MethodMockingContext(MethodMockingContext &) = default;



        MethodMockingContext(MethodMockingContext &&other)
                : _impl(std::move(other._impl)) {
        }

        virtual ~MethodMockingContext() NO_THROWS { }

        std::string format() const override {
            return _impl->format();
        }

        unsigned int size() const override {
            return 1;
        }


        void getInvolvedMocks(std::vector<ActualInvocationsSource *> &into) const override {
            _impl->getInvolvedMocks(into);
        }

        void getExpectedSequence(std::vector<Invocation::Matcher *> &into) const override {
            const Invocation::Matcher *b = this;
            Invocation::Matcher *c = const_cast<Invocation::Matcher *>(b);
            into.push_back(c);
        }


        void getActualInvocations(std::unordered_set<Invocation *> &into) const override {
            _impl->getActualInvocations(into);
        }


        bool matches(Invocation &invocation) override {
            return _impl->matches(invocation);
        }

        void commit() override {
            _impl->commit();
        }

        void setMethodDetails(std::string mockName, std::string methodName) {
            _impl->setMethodDetails(mockName, methodName);
        }

        void setMatchingCriteria(std::function<bool(arglist &...)> predicate) {
            typename ActualInvocation<arglist...>::Matcher *matcher{
                    new UserDefinedInvocationMatcher<arglist...>(predicate)};
            _impl->setInvocationMatcher(matcher);
        }

        void setMatchingCriteria(const std::vector<Destructible *> &matchers) {
            typename ActualInvocation<arglist...>::Matcher *matcher{
                    new ArgumentsMatcherInvocationMatcher<arglist...>(matchers)};
            _impl->setInvocationMatcher(matcher);
        }


        void appendAction(Action<R, arglist...> *action) override {
            _impl->appendAction(action);
        }

        void setMethodBodyByAssignment(std::function<R(const typename fakeit::test_arg<arglist>::type...)> method) {
            _impl->setMethodBodyByAssignment(method);
        }

        template<class ...matcherCreators, class = typename std::enable_if<
                sizeof...(matcherCreators) == sizeof...(arglist)>::type>
        void setMatchingCriteria(const matcherCreators &... matcherCreator) {
            std::vector<Destructible *> matchers;

            MatchersCollector<0, arglist...> c(matchers);
            c.CollectMatchers(matcherCreator...);

            MethodMockingContext<R, arglist...>::setMatchingCriteria(matchers);
        }

    private:

        typename std::function<R(arglist&...)> getOriginalMethod() override {
            return _impl->getOriginalMethod();
        }

        std::shared_ptr<Implementation> _impl;
    };

    template<typename R, typename ... arglist>
    class MockingContext :
            public MethodMockingContext<R, arglist...> {
        MockingContext &operator=(const MockingContext &) = delete;

    public:

        MockingContext(typename MethodMockingContext<R, arglist...>::Context *stubbingContext)
                : MethodMockingContext<R, arglist...>(stubbingContext) {
        }

        MockingContext(MockingContext &) = default;

        MockingContext(MockingContext &&other)
                : MethodMockingContext<R, arglist...>(std::move(other)) {
        }

        MockingContext<R, arglist...> &setMethodDetails(std::string mockName, std::string methodName) {
            MethodMockingContext<R, arglist...>::setMethodDetails(mockName, methodName);
            return *this;
        }

        MockingContext<R, arglist...> &Using(const arglist &... args) {
            MethodMockingContext<R, arglist...>::setMatchingCriteria(args...);
            return *this;
        }

        template<class ...arg_matcher>
        MockingContext<R, arglist...> &Using(const arg_matcher &... arg_matchers) {
            MethodMockingContext<R, arglist...>::setMatchingCriteria(arg_matchers...);
            return *this;
        }

        MockingContext<R, arglist...> &Matching(std::function<bool(arglist &...)> matcher) {
            MethodMockingContext<R, arglist...>::setMatchingCriteria(matcher);
            return *this;
        }

        MockingContext<R, arglist...> &operator()(const arglist &... args) {
            MethodMockingContext<R, arglist...>::setMatchingCriteria(args...);
            return *this;
        }

        MockingContext<R, arglist...> &operator()(std::function<bool(arglist &...)> matcher) {
            MethodMockingContext<R, arglist...>::setMatchingCriteria(matcher);
            return *this;
        }

        void operator=(std::function<R(arglist &...)> method) {
            MethodMockingContext<R, arglist...>::setMethodBodyByAssignment(method);
        }

        template<typename U = R>
        typename std::enable_if<!std::is_reference<U>::value, void>::type operator=(const R &r) {
            auto method = [r](const typename fakeit::test_arg<arglist>::type...) -> R { return r; };
            MethodMockingContext<R, arglist...>::setMethodBodyByAssignment(method);
        }

        template<typename U = R>
        typename std::enable_if<std::is_reference<U>::value, void>::type operator=(const R &r) {
            auto method = [&r](const typename fakeit::test_arg<arglist>::type...) -> R { return r; };
            MethodMockingContext<R, arglist...>::setMethodBodyByAssignment(method);
        }
    };

    template<typename ... arglist>
    class MockingContext<void, arglist...> :
            public MethodMockingContext<void, arglist...> {
        MockingContext &operator=(const MockingContext &) = delete;

    public:

        MockingContext(typename MethodMockingContext<void, arglist...>::Context *stubbingContext)
                : MethodMockingContext<void, arglist...>(stubbingContext) {
        }

        MockingContext(MockingContext &) = default;

        MockingContext(MockingContext &&other)
                : MethodMockingContext<void, arglist...>(std::move(other)) {
        }

        MockingContext<void, arglist...> &setMethodDetails(std::string mockName, std::string methodName) {
            MethodMockingContext<void, arglist...>::setMethodDetails(mockName, methodName);
            return *this;
        }

        MockingContext<void, arglist...> &Using(const arglist &... args) {
            MethodMockingContext<void, arglist...>::setMatchingCriteria(args...);
            return *this;
        }

        template<class ...arg_matcher>
        MockingContext<void, arglist...> &Using(const arg_matcher &... arg_matchers) {
            MethodMockingContext<void, arglist...>::setMatchingCriteria(arg_matchers...);
            return *this;
        }

        MockingContext<void, arglist...> &Matching(std::function<bool(arglist &...)> matcher) {
            MethodMockingContext<void, arglist...>::setMatchingCriteria(matcher);
            return *this;
        }

        MockingContext<void, arglist...> &operator()(const arglist &... args) {
            MethodMockingContext<void, arglist...>::setMatchingCriteria(args...);
            return *this;
        }

        MockingContext<void, arglist...> &operator()(std::function<bool(arglist &...)> matcher) {
            MethodMockingContext<void, arglist...>::setMatchingCriteria(matcher);
            return *this;
        }

        void operator=(std::function<void(arglist &...)> method) {
            MethodMockingContext<void, arglist...>::setMethodBodyByAssignment(method);
        }

    };

    class DtorMockingContext : public MethodMockingContext<void> {
    public:

        DtorMockingContext(MethodMockingContext<void>::Context *stubbingContext)
                : MethodMockingContext<void>(stubbingContext) {
        }

        DtorMockingContext(DtorMockingContext &other) : MethodMockingContext<void>(other) {
        }

        DtorMockingContext(DtorMockingContext &&other) : MethodMockingContext<void>(std::move(other)) {
        }

        void operator=(std::function<void()> method) {
            MethodMockingContext<void>::setMethodBodyByAssignment(method);
        }

        DtorMockingContext &setMethodDetails(std::string mockName, std::string methodName) {
            MethodMockingContext<void>::setMethodDetails(mockName, methodName);
            return *this;
        }
    };

}

namespace fakeit {


    template<typename C, typename ... baseclasses>
    class MockImpl : private MockObject<C>, public virtual ActualInvocationsSource {
    public:

        MockImpl(FakeitContext &fakeit, C &obj)
                : MockImpl<C, baseclasses...>(fakeit, obj, true) {
        }

        MockImpl(FakeitContext &fakeit)
                : MockImpl<C, baseclasses...>(fakeit, *(createFakeInstance()), false) {
            FakeObject<C, baseclasses...> *fake = reinterpret_cast<FakeObject<C, baseclasses...> *>(_instance);
            fake->getVirtualTable().setCookie(1, this);
        }

        virtual ~MockImpl() NO_THROWS {
            _proxy.detach();
            if (_isOwner) {
                FakeObject<C, baseclasses...> *fake = reinterpret_cast<FakeObject<C, baseclasses...> *>(_instance);
                delete fake;
            }
        }

        void detach() {
            _isOwner = false;
            _proxy.detach();
        }


        void getActualInvocations(std::unordered_set<Invocation *> &into) const override {
            std::vector<ActualInvocationsSource *> vec;
            _proxy.getMethodMocks(vec);
            for (ActualInvocationsSource *s : vec) {
                s->getActualInvocations(into);
            }
        }

        void reset() {
            _proxy.Reset();
            if (_isOwner) {
                FakeObject<C, baseclasses...> *fake = reinterpret_cast<FakeObject<C, baseclasses...> *>(_instance);
                fake->initializeDataMembersArea();
            }
        }

        virtual C &get() override {
            return _proxy.get();
        }

        virtual FakeitContext &getFakeIt() override {
            return _fakeit;
        }

        template<class DATA_TYPE, typename T, typename ... arglist, class = typename std::enable_if<std::is_base_of<T, C>::value>::type>
        DataMemberStubbingRoot<C, DATA_TYPE> stubDataMember(DATA_TYPE T::*member, const arglist &... ctorargs) {
            _proxy.stubDataMember(member, ctorargs...);
            return DataMemberStubbingRoot<T, DATA_TYPE>();
        }

        template<int id, typename R, typename T, typename ... arglist, class = typename std::enable_if<std::is_base_of<T, C>::value>::type>
        MockingContext<R, arglist...> stubMethod(R(T::*vMethod)(arglist...)) {
            return MockingContext<R, arglist...>(new UniqueMethodMockingContextImpl < id, R, arglist... >
                   (*this, vMethod));
        }

        DtorMockingContext stubDtor() {
            return DtorMockingContext(new DtorMockingContextImpl(*this));
        }

    private:
        DynamicProxy<C, baseclasses...> _proxy;
        C *_instance;
        bool _isOwner;
        FakeitContext &_fakeit;

        template<typename R, typename ... arglist>
        class MethodMockingContextBase : public MethodMockingContext<R, arglist...>::Context {
        protected:
            MockImpl<C, baseclasses...> &_mock;

            virtual RecordedMethodBody<R, arglist...> &getRecordedMethodBody() = 0;

        public:
            MethodMockingContextBase(MockImpl<C, baseclasses...> &mock) : _mock(mock) { }

            virtual ~MethodMockingContextBase() = default;

            void addMethodInvocationHandler(typename ActualInvocation<arglist...>::Matcher *matcher,
                ActualInvocationHandler<R, arglist...> *invocationHandler) {
                getRecordedMethodBody().addMethodInvocationHandler(matcher, invocationHandler);
            }

            void scanActualInvocations(const std::function<void(ActualInvocation<arglist...> &)> &scanner) {
                getRecordedMethodBody().scanActualInvocations(scanner);
            }

            void setMethodDetails(std::string mockName, std::string methodName) {
                getRecordedMethodBody().setMethodDetails(mockName, methodName);
            }

            bool isOfMethod(MethodInfo &method) {
                return getRecordedMethodBody().isOfMethod(method);
            }

            ActualInvocationsSource &getInvolvedMock() {
                return _mock;
            }

            std::string getMethodName() {
                return getRecordedMethodBody().getMethod().name();
            }

        };

        template<typename R, typename ... arglist>
        class MethodMockingContextImpl : public MethodMockingContextBase<R, arglist...> {
        protected:

            R (C::*_vMethod)(arglist...);

        public:
            virtual ~MethodMockingContextImpl() = default;

            MethodMockingContextImpl(MockImpl<C, baseclasses...> &mock, R (C::*vMethod)(arglist...))
                    : MethodMockingContextBase<R, arglist...>(mock), _vMethod(vMethod) {
            }


            virtual std::function<R(arglist&...)> getOriginalMethod() override {
                void *mPtr = MethodMockingContextBase<R, arglist...>::_mock.getOriginalMethod(_vMethod);
                C * instance = &(MethodMockingContextBase<R, arglist...>::_mock.get());
                return [=](arglist&... args) -> R {
                    auto m = union_cast<typename VTableMethodType<R,arglist...>::type>(mPtr);
                    return m(instance, std::forward<arglist>(args)...);
                };
            }
        };


        template<int id, typename R, typename ... arglist>
        class UniqueMethodMockingContextImpl : public MethodMockingContextImpl<R, arglist...> {
        protected:

            virtual RecordedMethodBody<R, arglist...> &getRecordedMethodBody() override {
                return MethodMockingContextBase<R, arglist...>::_mock.template stubMethodIfNotStubbed<id>(
                        MethodMockingContextBase<R, arglist...>::_mock._proxy,
                        MethodMockingContextImpl<R, arglist...>::_vMethod);
            }

        public:

            UniqueMethodMockingContextImpl(MockImpl<C, baseclasses...> &mock, R (C::*vMethod)(arglist...))
                    : MethodMockingContextImpl<R, arglist...>(mock, vMethod) {
            }
        };

        class DtorMockingContextImpl : public MethodMockingContextBase<void> {

        protected:

            virtual RecordedMethodBody<void> &getRecordedMethodBody() override {
                return MethodMockingContextBase<void>::_mock.stubDtorIfNotStubbed(
                        MethodMockingContextBase<void>::_mock._proxy);
            }

        public:
            virtual ~DtorMockingContextImpl() = default;

            DtorMockingContextImpl(MockImpl<C, baseclasses...> &mock)
                    : MethodMockingContextBase<void>(mock) {
            }

            virtual std::function<void()> getOriginalMethod() override {
                C &instance = MethodMockingContextBase<void>::_mock.get();
                return [=, &instance]() -> void {
                };
            }

        };

        static MockImpl<C, baseclasses...> *getMockImpl(void *instance) {
            FakeObject<C, baseclasses...> *fake = reinterpret_cast<FakeObject<C, baseclasses...> *>(instance);
            MockImpl<C, baseclasses...> *mock = reinterpret_cast<MockImpl<C, baseclasses...> *>(fake->getVirtualTable().getCookie(
                    1));
            return mock;
        }

        void unmocked() {
            ActualInvocation<> invocation(Invocation::nextInvocationOrdinal(), UnknownMethod::instance());
            UnexpectedMethodCallEvent event(UnexpectedType::Unmocked, invocation);
            auto &fakeit = getMockImpl(this)->_fakeit;
            fakeit.handle(event);

            std::string format = fakeit.format(event);
            UnexpectedMethodCallException e(format);
            throw e;
        }

        static C *createFakeInstance() {
            FakeObject<C, baseclasses...> *fake = new FakeObject<C, baseclasses...>();
            void *unmockedMethodStubPtr = union_cast<void *>(&MockImpl<C, baseclasses...>::unmocked);
            fake->getVirtualTable().initAll(unmockedMethodStubPtr);
            return reinterpret_cast<C *>(fake);
        }

        template<typename R, typename ... arglist>
        void *getOriginalMethod(R (C::*vMethod)(arglist...)) {
            auto vt = _proxy.getOriginalVT();
            auto offset = VTUtils::getOffset(vMethod);
            void *origMethodPtr = vt.getMethod(offset);
            return origMethodPtr;
        }

        void *getOriginalDtor() {
            auto vt = _proxy.getOriginalVT();
            auto offset = VTUtils::getDestructorOffset<C>();
            void *origMethodPtr = vt.getMethod(offset);
            return origMethodPtr;
        }

        template<unsigned int id, typename R, typename ... arglist>
        RecordedMethodBody<R, arglist...> &stubMethodIfNotStubbed(DynamicProxy<C, baseclasses...> &proxy,
                                                                  R (C::*vMethod)(arglist...)) {
            if (!proxy.isMethodStubbed(vMethod)) {
                proxy.template stubMethod<id>(vMethod, createRecordedMethodBody < R, arglist... > (*this, vMethod));
            }
            Destructible *d = proxy.getMethodMock(vMethod);
            RecordedMethodBody<R, arglist...> *methodMock = dynamic_cast<RecordedMethodBody<R, arglist...> *>(d);
            return *methodMock;
        }

        RecordedMethodBody<void> &stubDtorIfNotStubbed(DynamicProxy<C, baseclasses...> &proxy) {
            if (!proxy.isDtorStubbed()) {
                proxy.stubDtor(createRecordedDtorBody(*this));
            }
            Destructible *d = proxy.getDtorMock();
            RecordedMethodBody<void> *dtorMock = dynamic_cast<RecordedMethodBody<void> *>(d);
            return *dtorMock;
        }

        MockImpl(FakeitContext &fakeit, C &obj, bool isSpy)
                : _proxy{obj}, _instance(&obj), _isOwner(!isSpy), _fakeit(fakeit) {
        }

        template<typename R, typename ... arglist>
        static RecordedMethodBody<R, arglist...> *createRecordedMethodBody(MockObject<C> &mock,
                                                                           R(C::*vMethod)(arglist...)) {
            return new RecordedMethodBody<R, arglist...>(mock.getFakeIt(), typeid(vMethod).name());
        }

        static RecordedMethodBody<void> *createRecordedDtorBody(MockObject<C> &mock) {
            return new RecordedMethodBody<void>(mock.getFakeIt(), "dtor");
        }

    };
}
namespace fakeit {

    template<typename R, typename... Args>
    struct Prototype;

    template<typename R, typename... Args>
    struct Prototype<R(Args...)> {

        typedef R Type(Args...);

        typedef R ConstType(Args...) const;

        template<class C>
        struct MemberType {

            typedef Type(C::*type);
            typedef ConstType(C::*cosntType);

            static type get(type t) {
                return t;
            }

            static cosntType getconst(cosntType t) {
                return t;
            }

        };

    };

    template<int X, typename R, typename C, typename... arglist>
    struct UniqueMethod {
        R (C::*method)(arglist...);

        UniqueMethod(R (C::*vMethod)(arglist...)) : method(vMethod) { }

        int uniqueId() {
            return X;
        }




    };

}


namespace fakeit {
    namespace internal {
    }
    using namespace fakeit;
    using namespace fakeit::internal;

    template<typename C, typename ... baseclasses>
    class Mock : public ActualInvocationsSource {
        MockImpl<C, baseclasses...> impl;
    public:
        virtual ~Mock() = default;

        static_assert(std::is_polymorphic<C>::value, "Can only mock a polymorphic type");

        Mock() : impl(Fakeit) {
        }

        explicit Mock(C &obj) : impl(Fakeit, obj) {
        }

        virtual C &get() {
            return impl.get();
        }

        C &operator()() {
            return get();
        }

        void Reset() {
            impl.reset();
        }

        template<class DATA_TYPE, typename ... arglist,
                class = typename std::enable_if<std::is_member_object_pointer<DATA_TYPE C::*>::value>::type>
        DataMemberStubbingRoot<C, DATA_TYPE> Stub(DATA_TYPE C::* member, const arglist &... ctorargs) {
            return impl.stubDataMember(member, ctorargs...);
        }

        template<int id, typename R, typename T, typename ... arglist, class = typename std::enable_if<
                !std::is_void<R>::value && std::is_base_of<T, C>::value>::type>
        MockingContext<R, arglist...> stub(R (T::*vMethod)(arglist...) const) {
            auto methodWithoutConstVolatile = reinterpret_cast<R (T::*)(arglist...)>(vMethod);
            return impl.template stubMethod<id>(methodWithoutConstVolatile);
        }

        template<int id, typename R, typename T, typename... arglist, class = typename std::enable_if<
                !std::is_void<R>::value && std::is_base_of<T, C>::value>::type>
        MockingContext<R, arglist...> stub(R(T::*vMethod)(arglist...) volatile) {
            auto methodWithoutConstVolatile = reinterpret_cast<R(T::*)(arglist...)>(vMethod);
            return impl.template stubMethod<id>(methodWithoutConstVolatile);
        }

        template<int id, typename R, typename T, typename... arglist, class = typename std::enable_if<
                !std::is_void<R>::value && std::is_base_of<T, C>::value>::type>
        MockingContext<R, arglist...> stub(R(T::*vMethod)(arglist...) const volatile) {
            auto methodWithoutConstVolatile = reinterpret_cast<R(T::*)(arglist...)>(vMethod);
            return impl.template stubMethod<id>(methodWithoutConstVolatile);
        }

        template<int id, typename R, typename T, typename... arglist, class = typename std::enable_if<
                !std::is_void<R>::value && std::is_base_of<T, C>::value>::type>
        MockingContext<R, arglist...> stub(R(T::*vMethod)(arglist...)) {
            return impl.template stubMethod<id>(vMethod);
        }

        template<int id, typename R, typename T, typename... arglist, class = typename std::enable_if<
                std::is_void<R>::value && std::is_base_of<T, C>::value>::type>
        MockingContext<void, arglist...> stub(R(T::*vMethod)(arglist...) const) {
            auto methodWithoutConstVolatile = reinterpret_cast<void (T::*)(arglist...)>(vMethod);
            return impl.template stubMethod<id>(methodWithoutConstVolatile);
        }

        template<int id, typename R, typename T, typename... arglist, class = typename std::enable_if<
                std::is_void<R>::value && std::is_base_of<T, C>::value>::type>
        MockingContext<void, arglist...> stub(R(T::*vMethod)(arglist...) volatile) {
            auto methodWithoutConstVolatile = reinterpret_cast<void (T::*)(arglist...)>(vMethod);
            return impl.template stubMethod<id>(methodWithoutConstVolatile);
        }

        template<int id, typename R, typename T, typename... arglist, class = typename std::enable_if<
                std::is_void<R>::value && std::is_base_of<T, C>::value>::type>
        MockingContext<void, arglist...> stub(R(T::*vMethod)(arglist...) const volatile) {
            auto methodWithoutConstVolatile = reinterpret_cast<void (T::*)(arglist...)>(vMethod);
            return impl.template stubMethod<id>(methodWithoutConstVolatile);
        }

        template<int id, typename R, typename T, typename... arglist, class = typename std::enable_if<
                std::is_void<R>::value && std::is_base_of<T, C>::value>::type>
        MockingContext<void, arglist...> stub(R(T::*vMethod)(arglist...)) {
            auto methodWithoutConstVolatile = reinterpret_cast<void (T::*)(arglist...)>(vMethod);
            return impl.template stubMethod<id>(methodWithoutConstVolatile);
        }

        DtorMockingContext dtor() {
            return impl.stubDtor();
        }

        void getActualInvocations(std::unordered_set<Invocation *> &into) const override {
            impl.getActualInvocations(into);
        }

    };

}

#include <exception>

namespace fakeit {

    class RefCount {
    private:
        int count;

    public:
        void AddRef() {
            count++;
        }

        int Release() {
            return --count;
        }
    };

    template<typename T>
    class smart_ptr {
    private:
        T *pData;
        RefCount *reference;

    public:
        smart_ptr() : pData(0), reference(0) {
            reference = new RefCount();
            reference->AddRef();
        }

        smart_ptr(T *pValue) : pData(pValue), reference(0) {
            reference = new RefCount();
            reference->AddRef();
        }

        smart_ptr(const smart_ptr<T> &sp) : pData(sp.pData), reference(sp.reference) {
            reference->AddRef();
        }

        ~smart_ptr() THROWS {
            if (reference->Release() == 0) {
                delete reference;
                delete pData;
            }
        }

        T &operator*() {
            return *pData;
        }

        T *operator->() {
            return pData;
        }

        smart_ptr<T> &operator=(const smart_ptr<T> &sp) {
            if (this != &sp) {


                if (reference->Release() == 0) {
                    delete reference;
                    delete pData;
                }



                pData = sp.pData;
                reference = sp.reference;
                reference->AddRef();
            }
            return *this;
        }
    };

}

namespace fakeit {

    class WhenFunctor {

        struct StubbingChange {

            friend class WhenFunctor;

            virtual ~StubbingChange() THROWS {

                if (std::uncaught_exception()) {
                    return;
                }

                _xaction.commit();
            }

            StubbingChange(StubbingChange &other) :
                    _xaction(other._xaction) {
            }

        private:

            StubbingChange(Xaction &xaction)
                    : _xaction(xaction) {
            }

            Xaction &_xaction;
        };

    public:

        template<typename R, typename ... arglist>
        struct MethodProgress : MethodStubbingProgress<R, arglist...> {

            friend class WhenFunctor;

            virtual ~MethodProgress() override = default;

            MethodProgress(MethodProgress &other) :
                    _progress(other._progress), _context(other._context) {
            }

            MethodProgress(StubbingContext<R, arglist...> &xaction) :
                    _progress(new StubbingChange(xaction)), _context(xaction) {
            }

        protected:

            virtual MethodStubbingProgress<R, arglist...> &DoImpl(Action<R, arglist...> *action) override {
                _context.appendAction(action);
                return *this;
            }

        private:
            smart_ptr<StubbingChange> _progress;
            StubbingContext<R, arglist...> &_context;
        };


        WhenFunctor() {
        }

        template<typename R, typename ... arglist>
        MethodProgress<R, arglist...> operator()(const StubbingContext<R, arglist...> &stubbingContext) {
            StubbingContext<R, arglist...> &rootWithoutConst = const_cast<StubbingContext<R, arglist...> &>(stubbingContext);
            MethodProgress<R, arglist...> progress(rootWithoutConst);
            return progress;
        }

    };

}
namespace fakeit {

    class FakeFunctor {
    private:
        template<typename R, typename ... arglist>
        void fake(const StubbingContext<R, arglist...> &root) {
            StubbingContext<R, arglist...> &rootWithoutConst = const_cast<StubbingContext<R, arglist...> &>(root);
            rootWithoutConst.appendAction(new ReturnDefaultValue<R, arglist...>());
            rootWithoutConst.commit();
        }

        void operator()() {
        }

    public:

        template<typename H, typename ... M>
        void operator()(const H &head, const M &... tail) {
            fake(head);
            this->operator()(tail...);
        }

    };

}
#include <set>
#include <set>


namespace fakeit {

    struct InvocationUtils {

        static void sortByInvocationOrder(std::unordered_set<Invocation *> &ivocations,
                                          std::vector<Invocation *> &result) {
            auto comparator = [](Invocation *a, Invocation *b) -> bool {
                return a->getOrdinal() < b->getOrdinal();
            };
            std::set<Invocation *, bool (*)(Invocation *a, Invocation *b)> sortedIvocations(comparator);
            for (auto i : ivocations)
                sortedIvocations.insert(i);

            for (auto i : sortedIvocations)
                result.push_back(i);
        }

        static void collectActualInvocations(std::unordered_set<Invocation *> &actualInvocations,
                                             std::vector<ActualInvocationsSource *> &invocationSources) {
            for (auto source : invocationSources) {
                source->getActualInvocations(actualInvocations);
            }
        }

        static void selectNonVerifiedInvocations(std::unordered_set<Invocation *> &actualInvocations,
                                                 std::unordered_set<Invocation *> &into) {
            for (auto invocation : actualInvocations) {
                if (!invocation->isVerified()) {
                    into.insert(invocation);
                }
            }
        }

        static void collectInvocationSources(std::vector<ActualInvocationsSource *> &) {
        }

        template<typename ... list>
        static void collectInvocationSources(std::vector<ActualInvocationsSource *> &into,
                                             const ActualInvocationsSource &mock,
                                             const list &... tail) {
            into.push_back(const_cast<ActualInvocationsSource *>(&mock));
            collectInvocationSources(into, tail...);
        }

        static void collectSequences(std::vector<Sequence *> &) {
        }

        template<typename ... list>
        static void collectSequences(std::vector<Sequence *> &vec, const Sequence &sequence, const list &... tail) {
            vec.push_back(&const_cast<Sequence &>(sequence));
            collectSequences(vec, tail...);
        }

        static void collectInvolvedMocks(std::vector<Sequence *> &allSequences,
                                         std::vector<ActualInvocationsSource *> &involvedMocks) {
            for (auto sequence : allSequences) {
                sequence->getInvolvedMocks(involvedMocks);
            }
        }

        template<class T>
        static T &remove_const(const T &s) {
            return const_cast<T &>(s);
        }

    };

}

#include <memory>

#include <vector>
#include <unordered_set>

namespace fakeit {
    struct MatchAnalysis {
        std::vector<Invocation *> actualSequence;
        std::vector<Invocation *> matchedInvocations;
        int count;

        void run(InvocationsSourceProxy &involvedInvocationSources, std::vector<Sequence *> &expectedPattern) {
            getActualInvocationSequence(involvedInvocationSources, actualSequence);
            count = countMatches(expectedPattern, actualSequence, matchedInvocations);
        }

    private:
        static void getActualInvocationSequence(InvocationsSourceProxy &involvedMocks,
                                                std::vector<Invocation *> &actualSequence) {
            std::unordered_set<Invocation *> actualInvocations;
            collectActualInvocations(involvedMocks, actualInvocations);
            InvocationUtils::sortByInvocationOrder(actualInvocations, actualSequence);
        }

        static int countMatches(std::vector<Sequence *> &pattern, std::vector<Invocation *> &actualSequence,
                                std::vector<Invocation *> &matchedInvocations) {
            int end = -1;
            int count = 0;
            int startSearchIndex = 0;
            while (findNextMatch(pattern, actualSequence, startSearchIndex, end, matchedInvocations)) {
                count++;
                startSearchIndex = end;
            }
            return count;
        }

        static void collectActualInvocations(InvocationsSourceProxy &involvedMocks,
                                             std::unordered_set<Invocation *> &actualInvocations) {
            involvedMocks.getActualInvocations(actualInvocations);
        }

        static bool findNextMatch(std::vector<Sequence *> &pattern, std::vector<Invocation *> &actualSequence,
                                  int startSearchIndex, int &end,
                                  std::vector<Invocation *> &matchedInvocations) {
            for (auto sequence : pattern) {
                int index = findNextMatch(sequence, actualSequence, startSearchIndex);
                if (index == -1) {
                    return false;
                }
                collectMatchedInvocations(actualSequence, matchedInvocations, index, sequence->size());
                startSearchIndex = index + sequence->size();
            }
            end = startSearchIndex;
            return true;
        }


        static void collectMatchedInvocations(std::vector<Invocation *> &actualSequence,
                                              std::vector<Invocation *> &matchedInvocations, int start,
                                              int length) {
            int indexAfterMatchedPattern = start + length;
            for (; start < indexAfterMatchedPattern; start++) {
                matchedInvocations.push_back(actualSequence[start]);
            }
        }


        static bool isMatch(std::vector<Invocation *> &actualSequence,
                            std::vector<Invocation::Matcher *> &expectedSequence, int start) {
            bool found = true;
            for (unsigned int j = 0; found && j < expectedSequence.size(); j++) {
                Invocation *actual = actualSequence[start + j];
                Invocation::Matcher *expected = expectedSequence[j];
                found = found && expected->matches(*actual);
            }
            return found;
        }

        static int findNextMatch(Sequence *&pattern, std::vector<Invocation *> &actualSequence, int startSearchIndex) {
            std::vector<Invocation::Matcher *> expectedSequence;
            pattern->getExpectedSequence(expectedSequence);
            for (int i = startSearchIndex; i < ((int) actualSequence.size() - (int) expectedSequence.size() + 1); i++) {
                if (isMatch(actualSequence, expectedSequence, i)) {
                    return i;
                }
            }
            return -1;
        }

    };
}

namespace fakeit {

    struct SequenceVerificationExpectation {

        friend class SequenceVerificationProgress;

        ~SequenceVerificationExpectation() THROWS {
            if (std::uncaught_exception()) {
                return;
            }
            VerifyExpectation(_fakeit);
        }

        void setExpectedPattern(std::vector<Sequence *> expectedPattern) {
            _expectedPattern = expectedPattern;
        }

        void setExpectedCount(const int count) {
            _expectedCount = count;
        }

        void setFileInfo(std::string file, int line, std::string callingMethod) {
            _file = file;
            _line = line;
            _testMethod = callingMethod;
        }

    private:

        VerificationEventHandler &_fakeit;
        InvocationsSourceProxy _involvedInvocationSources;
        std::vector<Sequence *> _expectedPattern;
        int _expectedCount;

        std::string _file;
        int _line;
        std::string _testMethod;
        bool _isVerified;

        SequenceVerificationExpectation(
                VerificationEventHandler &fakeit,
                InvocationsSourceProxy mocks,
                std::vector<Sequence *> &expectedPattern) :
                _fakeit(fakeit),
                _involvedInvocationSources(mocks),
                _expectedPattern(expectedPattern),
                _expectedCount(-1),
                _line(0),
                _isVerified(false) {
        }


        void VerifyExpectation(VerificationEventHandler &verificationErrorHandler) {
            if (_isVerified)
                return;
            _isVerified = true;

            MatchAnalysis ma;
            ma.run(_involvedInvocationSources, _expectedPattern);

            if (isAtLeastVerification() && atLeastLimitNotReached(ma.count)) {
                return handleAtLeastVerificationEvent(verificationErrorHandler, ma.actualSequence, ma.count);
            }

            if (isExactVerification() && exactLimitNotMatched(ma.count)) {
                return handleExactVerificationEvent(verificationErrorHandler, ma.actualSequence, ma.count);
            }

            markAsVerified(ma.matchedInvocations);
        }

        std::vector<Sequence *> &collectSequences(std::vector<Sequence *> &vec) {
            return vec;
        }

        template<typename ... list>
        std::vector<Sequence *> &collectSequences(std::vector<Sequence *> &vec, const Sequence &sequence,
                                                  const list &... tail) {
            vec.push_back(&const_cast<Sequence &>(sequence));
            return collectSequences(vec, tail...);
        }


        static void markAsVerified(std::vector<Invocation *> &matchedInvocations) {
            for (auto i : matchedInvocations) {
                i->markAsVerified();
            }
        }

        bool isAtLeastVerification() {

            return _expectedCount < 0;
        }

        bool isExactVerification() {
            return !isAtLeastVerification();
        }

        bool atLeastLimitNotReached(int count) {
            return count < -_expectedCount;
        }

        bool exactLimitNotMatched(int count) {
            return count != _expectedCount;
        }

        void handleExactVerificationEvent(VerificationEventHandler &verificationErrorHandler,
                                          std::vector<Invocation *> actualSequence, int count) {
            SequenceVerificationEvent evt(VerificationType::Exact, _expectedPattern, actualSequence, _expectedCount,
                                          count);
            evt.setFileInfo(_file, _line, _testMethod);
            return verificationErrorHandler.handle(evt);
        }

        void handleAtLeastVerificationEvent(VerificationEventHandler &verificationErrorHandler,
                                            std::vector<Invocation *> actualSequence, int count) {
            SequenceVerificationEvent evt(VerificationType::AtLeast, _expectedPattern, actualSequence, -_expectedCount,
                                          count);
            evt.setFileInfo(_file, _line, _testMethod);
            return verificationErrorHandler.handle(evt);
        }

    };

}
namespace fakeit {
    class ThrowFalseEventHandler : public VerificationEventHandler {

        void handle(const SequenceVerificationEvent &) override {
            throw false;
        }

        void handle(const NoMoreInvocationsVerificationEvent &) override {
            throw false;
        }
    };
}
#include <string>
#include <sstream>
#include <iomanip>

namespace fakeit {

    template<typename T>
    static std::string to_string(const T &n) {
        std::ostringstream stm;
        stm << n;
        return stm.str();
    }

}


namespace fakeit {

    struct FakeitContext;

    class SequenceVerificationProgress {

        friend class UsingFunctor;

        friend class VerifyFunctor;

        friend class UsingProgress;

        smart_ptr<SequenceVerificationExpectation> _expectationPtr;

        SequenceVerificationProgress(SequenceVerificationExpectation *ptr) : _expectationPtr(ptr) {
        }

        SequenceVerificationProgress(
                FakeitContext &fakeit,
                InvocationsSourceProxy sources,
                std::vector<Sequence *> &allSequences) :
                SequenceVerificationProgress(new SequenceVerificationExpectation(fakeit, sources, allSequences)) {
        }

        virtual void verifyInvocations(const int times) {
            _expectationPtr->setExpectedCount(times);
        }

        class Terminator {
            smart_ptr<SequenceVerificationExpectation> _expectationPtr;

            bool toBool() {
                try {
                    ThrowFalseEventHandler eh;
                    _expectationPtr->VerifyExpectation(eh);
                    return true;
                }
                catch (bool e) {
                    return e;
                }
            }

        public:
            Terminator(smart_ptr<SequenceVerificationExpectation> expectationPtr) : _expectationPtr(expectationPtr) { };

            operator bool() {
                return toBool();
            }

            bool operator!() const { return !const_cast<Terminator *>(this)->toBool(); }
        };

    public:

        ~SequenceVerificationProgress() THROWS { };

        operator bool() {
            return Terminator(_expectationPtr);
        }

        bool operator!() const { return !Terminator(_expectationPtr); }

        Terminator Never() {
            Exactly(0);
            return Terminator(_expectationPtr);
        }

        Terminator Once() {
            Exactly(1);
            return Terminator(_expectationPtr);
        }

        Terminator Twice() {
            Exactly(2);
            return Terminator(_expectationPtr);
        }

        Terminator AtLeastOnce() {
            verifyInvocations(-1);
            return Terminator(_expectationPtr);
        }

        Terminator Exactly(const int times) {
            if (times < 0) {
                throw std::invalid_argument(std::string("bad argument times:").append(fakeit::to_string(times)));
            }
            verifyInvocations(times);
            return Terminator(_expectationPtr);
        }

        Terminator Exactly(const Quantity &q) {
            Exactly(q.quantity);
            return Terminator(_expectationPtr);
        }

        Terminator AtLeast(const int times) {
            if (times < 0) {
                throw std::invalid_argument(std::string("bad argument times:").append(fakeit::to_string(times)));
            }
            verifyInvocations(-times);
            return Terminator(_expectationPtr);
        }

        Terminator AtLeast(const Quantity &q) {
            AtLeast(q.quantity);
            return Terminator(_expectationPtr);
        }

        SequenceVerificationProgress setFileInfo(std::string file, int line, std::string callingMethod) {
            _expectationPtr->setFileInfo(file, line, callingMethod);
            return *this;
        }
    };
}

namespace fakeit {

    class UsingProgress {
        fakeit::FakeitContext &_fakeit;
        InvocationsSourceProxy _sources;

        void collectSequences(std::vector<fakeit::Sequence *> &) {
        }

        template<typename ... list>
        void collectSequences(std::vector<fakeit::Sequence *> &vec, const fakeit::Sequence &sequence,
                              const list &... tail) {
            vec.push_back(&const_cast<fakeit::Sequence &>(sequence));
            collectSequences(vec, tail...);
        }

    public:

        UsingProgress(fakeit::FakeitContext &fakeit, InvocationsSourceProxy source) :
                _fakeit(fakeit),
                _sources(source) {
        }

        template<typename ... list>
        SequenceVerificationProgress Verify(const fakeit::Sequence &sequence, const list &... tail) {
            std::vector<fakeit::Sequence *> allSequences;
            collectSequences(allSequences, sequence, tail...);
            SequenceVerificationProgress progress(_fakeit, _sources, allSequences);
            return progress;
        }

    };
}

namespace fakeit {

    class UsingFunctor {

        friend class VerifyFunctor;

        FakeitContext &_fakeit;

    public:

        UsingFunctor(FakeitContext &fakeit) : _fakeit(fakeit) {
        }

        template<typename ... list>
        UsingProgress operator()(const ActualInvocationsSource &head, const list &... tail) {
            std::vector<ActualInvocationsSource *> allMocks{&InvocationUtils::remove_const(head),
                                                            &InvocationUtils::remove_const(tail)...};
            InvocationsSourceProxy aggregateInvocationsSource{new AggregateInvocationsSource(allMocks)};
            UsingProgress progress(_fakeit, aggregateInvocationsSource);
            return progress;
        }

    };
}
#include <set>

namespace fakeit {

    class VerifyFunctor {

        FakeitContext &_fakeit;


    public:

        VerifyFunctor(FakeitContext &fakeit) : _fakeit(fakeit) {
        }

        template<typename ... list>
        SequenceVerificationProgress operator()(const Sequence &sequence, const list &... tail) {
            std::vector<Sequence *> allSequences{&InvocationUtils::remove_const(sequence),
                                                 &InvocationUtils::remove_const(tail)...};

            std::vector<ActualInvocationsSource *> involvedSources;
            InvocationUtils::collectInvolvedMocks(allSequences, involvedSources);
            InvocationsSourceProxy aggregateInvocationsSource{new AggregateInvocationsSource(involvedSources)};

            UsingProgress usingProgress(_fakeit, aggregateInvocationsSource);
            return usingProgress.Verify(sequence, tail...);
        }

    };

}
#include <set>
#include <memory>
namespace fakeit {

    class VerifyNoOtherInvocationsVerificationProgress {

        friend class VerifyNoOtherInvocationsFunctor;

        struct VerifyNoOtherInvocationsExpectation {

            friend class VerifyNoOtherInvocationsVerificationProgress;

            ~VerifyNoOtherInvocationsExpectation() THROWS {
                if (std::uncaught_exception()) {
                    return;
                }

                VerifyExpectation(_fakeit);
            }

            void setFileInfo(std::string file, int line, std::string callingMethod) {
                _file = file;
                _line = line;
                _callingMethod = callingMethod;
            }

        private:

            VerificationEventHandler &_fakeit;
            std::vector<ActualInvocationsSource *> _mocks;

            std::string _file;
            int _line;
            std::string _callingMethod;
            bool _isVerified;

            VerifyNoOtherInvocationsExpectation(VerificationEventHandler &fakeit,
                                                std::vector<ActualInvocationsSource *> mocks) :
                    _fakeit(fakeit),
                    _mocks(mocks),
                    _line(0),
                    _isVerified(false) {
            }

            VerifyNoOtherInvocationsExpectation(VerifyNoOtherInvocationsExpectation &other) = default;

            void VerifyExpectation(VerificationEventHandler &verificationErrorHandler) {
                if (_isVerified)
                    return;
                _isVerified = true;

                std::unordered_set<Invocation *> actualInvocations;
                InvocationUtils::collectActualInvocations(actualInvocations, _mocks);

                std::unordered_set<Invocation *> nonVerifiedInvocations;
                InvocationUtils::selectNonVerifiedInvocations(actualInvocations, nonVerifiedInvocations);

                if (nonVerifiedInvocations.size() > 0) {
                    std::vector<Invocation *> sortedNonVerifiedInvocations;
                    InvocationUtils::sortByInvocationOrder(nonVerifiedInvocations, sortedNonVerifiedInvocations);

                    std::vector<Invocation *> sortedActualInvocations;
                    InvocationUtils::sortByInvocationOrder(actualInvocations, sortedActualInvocations);

                    NoMoreInvocationsVerificationEvent evt(sortedActualInvocations, sortedNonVerifiedInvocations);
                    evt.setFileInfo(_file, _line, _callingMethod);
                    return verificationErrorHandler.handle(evt);
                }
            }

        };

        fakeit::smart_ptr<VerifyNoOtherInvocationsExpectation> _ptr;

        VerifyNoOtherInvocationsVerificationProgress(VerifyNoOtherInvocationsExpectation *ptr) :
                _ptr(ptr) {
        }

        VerifyNoOtherInvocationsVerificationProgress(FakeitContext &fakeit,
                                                     std::vector<ActualInvocationsSource *> &invocationSources)
                : VerifyNoOtherInvocationsVerificationProgress(
                new VerifyNoOtherInvocationsExpectation(fakeit, invocationSources)
        ) {
        }

        bool toBool() {
            try {
                ThrowFalseEventHandler ev;
                _ptr->VerifyExpectation(ev);
                return true;
            }
            catch (bool e) {
                return e;
            }
        }

    public:


        ~VerifyNoOtherInvocationsVerificationProgress() THROWS {
        };

        VerifyNoOtherInvocationsVerificationProgress setFileInfo(std::string file, int line,
                                                                 std::string callingMethod) {
            _ptr->setFileInfo(file, line, callingMethod);
            return *this;
        }

        operator bool() {
            return toBool();
        }

        bool operator!() const { return !const_cast<VerifyNoOtherInvocationsVerificationProgress *>(this)->toBool(); }

    };

}


namespace fakeit {
    class VerifyNoOtherInvocationsFunctor {

        FakeitContext &_fakeit;

    public:

        VerifyNoOtherInvocationsFunctor(FakeitContext &fakeit) : _fakeit(fakeit) {
        }

        void operator()() {
        }

        template<typename ... list>
        VerifyNoOtherInvocationsVerificationProgress operator()(const ActualInvocationsSource &head,
                                                                const list &... tail) {
            std::vector<ActualInvocationsSource *> invocationSources{&InvocationUtils::remove_const(head),
                                                                     &InvocationUtils::remove_const(tail)...};
            VerifyNoOtherInvocationsVerificationProgress progress{_fakeit, invocationSources};
            return progress;
        }
    };

}
namespace fakeit {

    class SpyFunctor {
    private:

        template<typename R, typename ... arglist>
        void spy(const SpyingContext<R, arglist...> &root) {
            SpyingContext<R, arglist...> &rootWithoutConst = const_cast<SpyingContext<R, arglist...> &>(root);
            auto methodFromOriginalVT = rootWithoutConst.getOriginalMethod();
            rootWithoutConst.appendAction(new ReturnDelegateValue<R, arglist...>(methodFromOriginalVT));
            rootWithoutConst.commit();
        }

        void operator()() {
        }

    public:

        template<typename H, typename ... M>
        void operator()(const H &head, const M &... tail) {
            spy(head);
            this->operator()(tail...);
        }

    };

}

#include <vector>
#include <set>

namespace fakeit {
    class VerifyUnverifiedFunctor {

        FakeitContext &_fakeit;

    public:

        VerifyUnverifiedFunctor(FakeitContext &fakeit) : _fakeit(fakeit) {
        }

        template<typename ... list>
        SequenceVerificationProgress operator()(const Sequence &sequence, const list &... tail) {
            std::vector<Sequence *> allSequences{&InvocationUtils::remove_const(sequence),
                                                 &InvocationUtils::remove_const(tail)...};

            std::vector<ActualInvocationsSource *> involvedSources;
            InvocationUtils::collectInvolvedMocks(allSequences, involvedSources);

            InvocationsSourceProxy aggregateInvocationsSource{new AggregateInvocationsSource(involvedSources)};
            InvocationsSourceProxy unverifiedInvocationsSource{
                    new UnverifiedInvocationsSource(aggregateInvocationsSource)};

            UsingProgress usingProgress(_fakeit, unverifiedInvocationsSource);
            return usingProgress.Verify(sequence, tail...);
        }

    };

    class UnverifiedFunctor {
    public:
        UnverifiedFunctor(FakeitContext &fakeit) : Verify(fakeit) {
        }

        VerifyUnverifiedFunctor Verify;

        template<typename ... list>
        UnverifiedInvocationsSource operator()(const ActualInvocationsSource &head, const list &... tail) {
            std::vector<ActualInvocationsSource *> allMocks{&InvocationUtils::remove_const(head),
                                                            &InvocationUtils::remove_const(tail)...};
            InvocationsSourceProxy aggregateInvocationsSource{new AggregateInvocationsSource(allMocks)};
            UnverifiedInvocationsSource unverifiedInvocationsSource{aggregateInvocationsSource};
            return unverifiedInvocationsSource;
        }













    };
}

namespace fakeit {

    static UsingFunctor Using(Fakeit);
    static VerifyFunctor Verify(Fakeit);
    static VerifyNoOtherInvocationsFunctor VerifyNoOtherInvocations(Fakeit);
    static UnverifiedFunctor Unverified(Fakeit);
    static SpyFunctor Spy;
    static FakeFunctor Fake;
    static WhenFunctor When;

    template<class T>
    class SilenceUnusedVariableWarnings {

        void use(void *) {
        }

        SilenceUnusedVariableWarnings() {
            use(&Fake);
            use(&When);
            use(&Spy);
            use(&Using);
            use(&Verify);
            use(&VerifyNoOtherInvocations);
            use(&_);
        }
    };

}
#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif

#define MOCK_TYPE(mock) \
    std::remove_reference<decltype(mock.get())>::type

#define OVERLOADED_METHOD_PTR(mock, method, prototype) \
    fakeit::Prototype<prototype>::MemberType<MOCK_TYPE(mock)>::get(&MOCK_TYPE(mock)::method)

#define CONST_OVERLOADED_METHOD_PTR(mock, method, prototype) \
    fakeit::Prototype<prototype>::MemberType<MOCK_TYPE(mock)>::getconst(&MOCK_TYPE(mock)::method)

#define Dtor(mock) \
    mock.dtor().setMethodDetails(#mock,"destructor")

#define Method(mock, method) \
    mock.template stub<__COUNTER__>(&MOCK_TYPE(mock)::method).setMethodDetails(#mock,#method)

#define OverloadedMethod(mock, method, prototype) \
    mock.template stub<__COUNTER__>(OVERLOADED_METHOD_PTR( mock , method, prototype )).setMethodDetails(#mock,#method)

#define ConstOverloadedMethod(mock, method, prototype) \
    mock.template stub<__COUNTER__>(CONST_OVERLOADED_METHOD_PTR( mock , method, prototype )).setMethodDetails(#mock,#method)

#define Verify(...) \
        Verify( __VA_ARGS__ ).setFileInfo(__FILE__, __LINE__, __func__)

#define Using(...) \
        Using( __VA_ARGS__ )

#define VerifyNoOtherInvocations(...) \
    VerifyNoOtherInvocations( __VA_ARGS__ ).setFileInfo(__FILE__, __LINE__, __func__)

#define Fake(...) \
    Fake( __VA_ARGS__ )

#define When(call) \
    When(call)


#endif
