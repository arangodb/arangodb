#include <functional>
#include <vector>
#include <memory>
#include <type_traits>
#include <tuple>

namespace arangodb {
namespace agency {

template<typename B>
struct envelope;

template<typename T>
struct buffer_mapper;


template<typename B, typename T, typename... Vs>
struct read_trx {
    using buffer = buffer_mapper<B>;

    template<typename K>
    read_trx key(K key) && {
        _buffer.setValue(key);
        return std::move(_buffer);
    };

    T done() && {
        _buffer.closeArray();
        return std::move(_buffer);
    };
private:
    friend T;
    read_trx(buffer &&b) : _buffer(std::move(b)) {}
    read_trx(buffer &&b, std::tuple<Vs...> v) : _buffer(std::move(b)), _v(std::forward_as_tuple(v)) {}

    buffer _buffer;
    std::tuple<Vs...> _v;
};

template<typename B, typename T>
struct write_trx;

template<typename B, typename T>
struct precs_trx {
    using buffer = buffer_mapper<B>;

    template<typename K, typename V>
    precs_trx isEqual(K k, V v) && {
        _buffer.setValue(std::forward<K>(k));
        _buffer.openObject();
        _buffer.setKey("old", std::forward<V>(v));
        _buffer.closeObject();
        return std::move(_buffer);
    };

    template<typename K>
    precs_trx isEmpty(K k) && {
        _buffer.setValue(k);
        _buffer.openObject();
        _buffer.setKey("oldEmpty", true);
        _buffer.closeObject();
        return std::move(_buffer);
    };

    T done() && { _buffer.closeObject(); _buffer.closeArray(); return std::move(_buffer); };
private:
    friend write_trx<B, T>;
    friend T;

    precs_trx(buffer &&b) : _buffer(std::move(b)) {}
    buffer _buffer;
};

template<typename B, typename T>
struct write_trx {
    using buffer = buffer_mapper<B>;

    template<typename K, typename V>
    write_trx set(K k, V v) && {
        _buffer.setValue(k);
        _buffer.openObject();
        _buffer.setKey("op", "set");
        _buffer.setKey("new", v);
        _buffer.closeObject();
        return std::move(_buffer);
    };

    template<typename K, typename F>
    write_trx emplace(K k, F f) && {
        _buffer.setValue(k);
        _buffer.openObject();
        f(_buffer.userObject());
        _buffer.closeObject();
        return std::move(_buffer);
    }

    template<typename K>
    write_trx remove(K k) && {
        _buffer.setValue(k);
        _buffer.openObject();
        _buffer.setKey("op", "delete");
        _buffer.closeObject();
        return std::move(_buffer);
    };

    template<typename K>
    write_trx inc(K k, uint64_t delta = 1) && {
        _buffer.setValue(k);
        _buffer.openObject();
        _buffer.setKey("op", "increment");
        _buffer.setKey("delta", delta);
        _buffer.closeObject();
        return std::move(_buffer);
    };


    // TODO generate inquiry ID here
    T done() && { _buffer.closeObject(); _buffer.closeArray(); return std::move(_buffer); }
    precs_trx<B, T> precs() && { _buffer.closeObject(); _buffer.openObject(); return std::move(_buffer); }
    write_trx& operator=(write_trx&&) = default;
private:
    friend T;

    write_trx(buffer &&b) : _buffer(std::move(b)) {}
    buffer _buffer;
};


template<typename B>
struct envelope {
    using buffer = buffer_mapper<B>;


    read_trx<B, envelope> read() && { _buffer.openArray(); return std::move(_buffer); };
    write_trx<B, envelope> write() && { _buffer.openArray(); _buffer.openObject(); return std::move(_buffer); };
    void done() && { _buffer.closeArray(); /* return std::move(_buffer).stealBuffer();*/ };

    /*template<typename... Ts>
    static envelope create(Ts... t) {
        buffer b((t)...);
        envelope env(std::move(b));
        env._buffer.openArray();
        return env;
    }*/
    static envelope create(VPackBuilder &b) {
        buffer buff(b);
        envelope env(b);
        env._buffer.openArray();
        return env;
    }
private:
    friend write_trx<B, envelope>;
    friend read_trx<B, envelope>;
    friend precs_trx<B, envelope>;
    envelope(buffer&& b) : _buffer(std::move(b)) {}
    envelope() {};
    buffer _buffer;
};

namespace detail {
template<typename V>
struct value_adder {
    template<typename B>
    static void add(B* b, V v) { b->add(VPackValue(std::forward<V>(v))); }
};
template<>
struct value_adder<VPackSlice> {
    template<typename B>
    static void add(B* b, VPackSlice v) { b->add(v); }
};
}


template<>
struct buffer_mapper<VPackBuilder> {

    buffer_mapper(VPackBuilder& builder) : _builder(&builder) {};

    template<typename K, typename V>
    void setKey(K k, V v) {
        detail::value_adder<K>::add(_builder, std::forward<K>(k));
        detail::value_adder<V>::add(_builder, std::forward<V>(v));
    }

    template<typename K>
    void setValue(K k) { detail::value_adder<K>::add(_builder, std::forward<K>(k)); }

    void openArray() { _builder->openArray(); }
    void closeArray() { _builder->close(); }
    void openObject() { _builder->openObject(); }
    void closeObject() { _builder->close(); }

    VPackBuilder& userObject() { return *_builder; };

    VPackBuilder *_builder;
};


}
}
