/** \file */

#ifndef _CUCKOOHASH_MAP_HH
#define _CUCKOOHASH_MAP_HH

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "cuckoohash_config.hh"
#include "cuckoohash_util.hh"
#include "lazy_array.hh"
#include "default_hasher.hh"

//! cuckoohash_map is the hash table class.
template < class Key,
           class T,
           class Hash = DefaultHasher<Key>,
           class Pred = std::equal_to<Key>,
           class Alloc = std::allocator<std::pair<const Key, T>>,
           size_t SLOT_PER_BUCKET = DEFAULT_SLOT_PER_BUCKET
           >
class cuckoohash_map {
public:
    //! key_type is the type of keys.
    typedef Key                     key_type;
    //! value_type is the type of key-value pairs.
    typedef std::pair<const Key, T> value_type;
    //! mapped_type is the type of values.
    typedef T                       mapped_type;
    //! hasher is the type of the hash function.
    typedef Hash                    hasher;
    //! key_equal is the type of the equality predicate.
    typedef Pred                    key_equal;
    //! allocator_type is the type of the allocator
    typedef Alloc                   allocator_type;

    //! slot_per_bucket is the number of items each bucket in the table can hold
    static const size_t slot_per_bucket = SLOT_PER_BUCKET;

    //! For any update operations, the callable passed in must be convertible to
    //! the following type
    typedef std::function<void(mapped_type&)> updater_type;

    //! Class returned by operator[] which wraps an entry in the hash table.
    //! Note that this reference type behave somewhat differently from an STL
    //! map reference. Most importantly, running this operator will not insert a
    //! default key-value pair into the map if the given key is not already in
    //! the map.
    class reference {
        // Note that this implementation here is not exactly STL compliant. To
        // maintain performance and avoid hitting the hash table too many times,
        // The reference object is *lazy*. In other words,
        //
        //  - operator[] does not actually perform an insert. It returns a
        //    reference object pointing to the requested key.
        //  - On table[i] = val // reference::operator=(mapped_type)
        //    an update / insert is called
        //  - On table[i] = table[j] // reference::operator=(const reference&)
        //    an update / insert is called with the value of table[j]
        //  - On val = table[i] // operator mapped_type()
        //    a find is called
        //  - On table[i] (i.e. no operation performed)
        //    the destructor is called immediately (reference::~reference())
        //    and nothing happens.
    public:
        //! Delete the default constructor, which should never be used
        reference() = delete;

        //! Casting to \p mapped_type runs a find for the stored key. If the
        //! find fails, it will thrown an exception.
        operator mapped_type() const {
            return owner_.find(key_);
        }

        //! The assignment operator will first try to update the value at the
        //! reference's key. If the key isn't in the table, it will insert the
        //! key with \p val.
        reference& operator=(const mapped_type& val) {
            owner_.upsert(
                key_, [&val](mapped_type& v) { v = val; }, val);
            return *this;
        }

        //! The copy assignment operator doesn't actually copy the passed-in
        //! reference. Instead, it has the same behavior as operator=(const
        //! mapped_type& val).
        reference& operator=(const reference& ref) {
            *this = (mapped_type) ref;
            return *this;
        }

    private:
        // private constructor which initializes the owner and key
        reference(
            cuckoohash_map<Key, T, Hash, Pred, Alloc, slot_per_bucket>& owner,
            const key_type& key) : owner_(owner), key_(key) {}

        // reference to the hash map instance
        cuckoohash_map<Key, T, Hash, Pred, Alloc, slot_per_bucket>& owner_;
        // the referenced key
        const key_type& key_;

        // cuckoohash_map needs to call the private constructor
        friend class cuckoohash_map<Key, T, Hash, Pred, Alloc, slot_per_bucket>;
    };

    typedef const mapped_type const_reference;

    typedef char partial_t;

private:
    // Constants used internally

    // true if the key is small and simple, which means using partial keys for
    // lookup would probably slow us down
    static const bool is_simple =
        std::is_pod<key_type>::value && sizeof(key_type) <= 8;

    // We enable certain methods only if the mapped_type is copy-assignable
    static const bool value_copy_assignable = std::is_copy_assignable<
        mapped_type>::value;

    // number of locks in the locks array
    static const size_t kNumLocks = 1 << 16;

    // number of cores on the machine
    static size_t kNumCores() {
        static size_t cores = std::thread::hardware_concurrency();
        return cores;
    }

    // A fast, lightweight spinlock
    LIBCUCKOO_SQUELCH_PADDING_WARNING
    class LIBCUCKOO_ALIGNAS(64) spinlock {
        std::atomic_flag lock_;
    public:
        spinlock() {
            lock_.clear();
        }

        inline void lock() {
            while (lock_.test_and_set(std::memory_order_acquire));
        }

        inline void unlock() {
            lock_.clear(std::memory_order_release);
        }

        inline bool try_lock() {
            return !lock_.test_and_set(std::memory_order_acquire);
        }

    };

    typedef enum {
        ok,
        failure,
        failure_key_not_found,
        failure_key_duplicated,
        failure_table_full,
        failure_under_expansion,
    } cuckoo_status;

    // The Bucket type holds slot_per_bucket partial keys, key-value pairs, and
    // a occupied bitset, which indicates whether the slot at the given bit
    // index is in the table or not. It uses aligned_storage arrays to store the
    // keys and values to allow constructing and destroying key-value pairs in
    // place. Internally, the values are stored without the const qualifier in
    // the key, to enable modifying bucket memory.
    typedef std::pair<Key, T> storage_value_type;
    class Bucket {
    private:
        std::array<partial_t, slot_per_bucket> partials_;
        std::bitset<slot_per_bucket> occupied_;
        std::array<typename std::aligned_storage<
                       sizeof(storage_value_type),
                       alignof(storage_value_type)>::type,
                   slot_per_bucket> kvpairs_;

    public:
        const partial_t& partial(size_t ind) const {
            return partials_[ind];
        }

        partial_t& partial(size_t ind) {
            return partials_[ind];
        }

        const value_type& kvpair(size_t ind) const {
            return *static_cast<const value_type*>(
                static_cast<const void*>(&kvpairs_[ind]));
        }

        value_type& kvpair(size_t ind) {
            return *static_cast<value_type*>(
                static_cast<void*>(&kvpairs_[ind]));
        }

        storage_value_type& storage_kvpair(size_t ind) {
            return *static_cast<storage_value_type*>(
                static_cast<void*>(&kvpairs_[ind]));
        }

        bool occupied(size_t ind) const {
            return occupied_[ind];
        }

        const key_type& key(size_t ind) const {
            return kvpair(ind).first;
        }

        const mapped_type& val(size_t ind) const {
            return kvpair(ind).second;
        }

        mapped_type& val(size_t ind) {
            return kvpair(ind).second;
        }

        template <typename K, typename... Args>
        void setKV(size_t ind, K&& k, Args&&... args) {
            static allocator_type pair_allocator;
            occupied_[ind] = true;
            pair_allocator.construct(
                &storage_kvpair(ind),
                std::piecewise_construct,
                std::forward_as_tuple(std::forward<K>(k)),
                std::forward_as_tuple(std::forward<Args>(args)...));
        }

        void eraseKV(size_t ind) {
            occupied_[ind] = false;
            (&kvpair(ind))->~value_type();
        }

        void clear() {
            for (size_t i = 0; i < slot_per_bucket; ++i) {
                if (occupied(i)) {
                    eraseKV(i);
                }
            }
        }

        // Moves the item in b1[slot1] into b2[slot2] without copying
        static void move_to_bucket(
            Bucket& b1, size_t slot1,
            Bucket& b2, size_t slot2) {
            assert(b1.occupied(slot1));
            assert(!b2.occupied(slot2));
            storage_value_type& tomove = b1.storage_kvpair(slot1);
            b2.setKV(slot2, std::move(tomove.first), std::move(tomove.second));
            b2.partial(slot2) = b1.partial(slot1);
            b1.occupied_.reset(slot1);
            b2.occupied_.set(slot2);
        }
    };

    // The type of the buckets container
    typedef std::vector<
        Bucket, typename allocator_type::template rebind<Bucket>::other>
    buckets_t;

    // The type of the locks container
    static_assert(LOCK_ARRAY_GRANULARITY >= 0 && LOCK_ARRAY_GRANULARITY <= 16,
                  "LOCK_ARRAY_GRANULARITY constant must be between 0 and 16,"
                  " inclusive");
    typedef lazy_array<
        16 - LOCK_ARRAY_GRANULARITY, LOCK_ARRAY_GRANULARITY,
        spinlock,
        typename allocator_type::template rebind<spinlock>::other> locks_t;

    // The type of the expansion lock
    typedef std::mutex expansion_lock_t;

    // cacheint is a cache-aligned atomic integer type.
    LIBCUCKOO_SQUELCH_PADDING_WARNING
    struct LIBCUCKOO_ALIGNAS(64) cacheint {
        std::atomic<size_t> num;
        cacheint(): num(0) {}
        cacheint(size_t x): num(x) {}
        cacheint(const cacheint& x): num(x.num.load()) {}
        cacheint(cacheint&& x): num(x.num.load()) {}
        cacheint& operator=(const cacheint& x) {
            num = x.num.load();
            return *this;
        }
        cacheint& operator=(const cacheint&& x) {
            num = x.num.load();
            return *this;
        }
    };

    // Helper methods to read and write hashpower_ with the correct memory
    // barriers
    size_t get_hashpower() const {
        return hashpower_.load(std::memory_order_acquire);
    }

    void set_hashpower(size_t val) {
        hashpower_.store(val, std::memory_order_release);
    }

    // get_counterid returns the counterid for the current thread.
    static inline int get_counterid() {
        // counterid stores the per-thread counter index of each thread. Each
        // counter value corresponds to a core on the machine.
        static LIBCUCKOO_THREAD_LOCAL int counterid = -1;

        if (counterid < 0) {
            counterid = rand() % kNumCores();
        }
        return counterid;
    }

    // reserve_calc takes in a parameter specifying a certain number of slots
    // for a table and returns the smallest hashpower that will hold n elements.
    static size_t reserve_calc(const size_t n) {
        size_t buckets = (n + slot_per_bucket - 1) / slot_per_bucket;
        size_t blog2;
        if (buckets <= 1) {
            blog2 = 1;
        } else {
            blog2 = 0;
            for (size_t bcounter = buckets; bcounter > 1; bcounter >>= 1) {
                ++blog2;
            }
            if (hashsize(blog2) < buckets) {
                ++blog2;
            }
        }
        assert(n <= hashsize(blog2) * slot_per_bucket);
        return blog2;
    }

public:
    /**
     * Creates a new cuckohash_map instance
     *
     * @param n the number of elements to reserve space for initially
     * @param mlf the minimum load factor required that the
     * table allows for automatic expansion.
     * @param mhp the maximum hashpower that the table can take on (pass in 0
     * for no limit)
     * @throw std::invalid_argument if the given minimum load factor is invalid,
     * or if the initial space exceeds the maximum hashpower
     */
    cuckoohash_map(size_t n = DEFAULT_SIZE,
                   double mlf = DEFAULT_MINIMUM_LOAD_FACTOR,
                   size_t mhp = NO_MAXIMUM_HASHPOWER,
                   const hasher& hf = hasher(),
                   const key_equal eql = key_equal())
        : hash_fn(hf), eq_fn(eql) {
        minimum_load_factor(mlf);
        maximum_hashpower(mhp);
        size_t hp = reserve_calc(n);
        if (mhp != NO_MAXIMUM_HASHPOWER && hp > mhp) {
            throw std::invalid_argument(
                "hashpower for initial size " + std::to_string(hp) +
                " is greater than the maximum hashpower");
        }
        set_hashpower(hp);
        buckets_.resize(hashsize(hp));
        locks_.allocate(std::min(locks_t::size(), hashsize(hp)));
        num_inserts_.resize(kNumCores(), 0);
        num_deletes_.resize(kNumCores(), 0);
    }

    ~cuckoohash_map() {
        cuckoo_clear();
    }

    //! clear removes all the elements in the hash table, calling their
    //! destructors.
    void clear() noexcept {
        auto unlocker = snapshot_and_lock_all();
        cuckoo_clear();
    }

    //! size returns the number of items currently in the hash table. Since it
    //! doesn't lock the table, elements can be inserted during the computation,
    //! so the result may not necessarily be exact.
    size_t size() const noexcept {
        return cuckoo_size();
    }

    //! empty returns true if the table is empty.
    bool empty() const noexcept {
        return size() == 0;
    }

    //! hashpower returns the hashpower of the table, which is
    //! log<SUB>2</SUB>(the number of buckets).
    size_t hashpower() const noexcept {
        return get_hashpower();
    }

    //! bucket_count returns the number of buckets in the table.
    size_t bucket_count() const noexcept {
        return hashsize(get_hashpower());
    }

    //! load_factor returns the ratio of the number of items in the table to the
    //! total number of available slots in the table.
    double load_factor() const noexcept {
        return cuckoo_loadfactor(get_hashpower());
    }

    /**
     * Sets the minimum load factor allowed for automatic expansions. If an
     * expansion is needed when the load factor of the table is lower than this
     * threshold, the libcuckoo_load_factor_too_low exception is thrown.
     *
     * @param mlf the load factor to set the minimum to
     * @throw std::invalid_argument if the given load factor is less than 0.0
     * or greater than 1.0
     */
    void minimum_load_factor(const double mlf) {
        if (mlf < 0.0) {
            throw std::invalid_argument(
                "load factor " + std::to_string(mlf) + " cannot be "
                " less than 0");
        } else if (mlf > 1.0) {
            throw std::invalid_argument(
                "load factor " + std::to_string(mlf) + " cannot be "
                " greater than 1");
        }
        minimum_load_factor_.store(mlf, std::memory_order_release);
    }

    /**
     * @return the minimum load factor of the table
     */
    double minimum_load_factor() noexcept {
        return minimum_load_factor_.load(std::memory_order_acquire);
    }

    /**
     * Sets the maximum hashpower the table can be. If set to \ref
     * NO_MAXIMUM_HASHPOWER, there will be no limit on the hashpower.
     *
     * @param mhp the hashpower to set the maximum to
     */
    void maximum_hashpower(size_t mhp) noexcept {
        maximum_hashpower_.store(mhp, std::memory_order_release);
    }

    /**
     * @return the maximum hashpower of the table
     */
    size_t maximum_hashpower() noexcept {
        return maximum_hashpower_.load(std::memory_order_acquire);
    }

    //! find searches through the table for \p key, and stores the associated
    //! value it finds in \p val. must be copy assignable.
    template <typename K>
    bool find(const K& key, mapped_type& val) const {
        size_t hv = hashed_key(key);
        auto b = snapshot_and_lock_two(hv);
        const cuckoo_status st = cuckoo_find(key, val, hv, b.i[0], b.i[1]);
        return (st == ok);
    }

    //! This version of find does the same thing as the two-argument version,
    //! except it returns the value it finds, throwing an \p std::out_of_range
    //! exception if the key isn't in the table.
    template <typename K>
    mapped_type find(const K& key) const {
        mapped_type val;
        bool done = find(key, val);
        if (done) {
            return val;
        } else {
            throw std::out_of_range("key not found in table");
        }
    }

    //! contains searches through the table for \p key, and returns true if it
    //! finds it in the table, and false otherwise.
    template <typename K>
    bool contains(const K& key) const {
        size_t hv = hashed_key(key);
        auto b = snapshot_and_lock_two(hv);
        const bool result = cuckoo_contains(key, hv, b.i[0], b.i[1]);
        return result;
    }

    /**
     * Puts the given key-value pair into the table. If the key cannot be placed
     * in the table, it may be automatically expanded to fit more items.
     *
     * @param key the key to insert into the table
     * @param val the value to insert
     * @return true if the insertion succeeded, false if there was a duplicate
     * key
     * @throw libcuckoo_load_factor_too_low if the load factor is below the
     * minimum_load_factor threshold, if expansion is required
     * @throw libcuckoo_maximum_hashpower_exceeded if expansion is required
     * beyond the maximum hash power, if one was set
     */
    template <typename K, typename... Args>
    bool insert(K&& key, Args&&... val) {
        return cuckoo_insert_loop(hashed_key(key), std::forward<K>(key),
                                  std::forward<Args>(val)...);
    }

    //! erase removes \p key and it's associated value from the table, calling
    //! their destructors. If \p key is not there, it returns false, otherwise
    //! it returns true.
    template <typename K>
    bool erase(const K& key) {
        size_t hv = hashed_key(key);
        auto b = snapshot_and_lock_two(hv);
        const cuckoo_status st = cuckoo_delete(key, hv, b.i[0], b.i[1]);
        return (st == ok);
    }

    //! update changes the value associated with \p key to \p val. If \p key is
    //! not there, it returns false, otherwise it returns true.
    template <typename K, typename V>
    bool update(const K& key, V&& val) {
        size_t hv = hashed_key(key);
        auto b = snapshot_and_lock_two(hv);
        const cuckoo_status st = cuckoo_update(hv, b.i[0], b.i[1],
                                               key, std::forward<V>(val));
        return (st == ok);
    }

    //! update_fn changes the value associated with \p key with the function \p
    //! fn. \p fn will be passed one argument of type \p mapped_type& and can
    //! modify the argument as desired, returning nothing. If \p key is not
    //! there, it returns false, otherwise it returns true.
    template <typename K,typename Updater>
    typename std::enable_if<
        std::is_convertible<Updater, updater_type>::value,
        bool>::type update_fn(const K& key, Updater fn) {
        size_t hv = hashed_key(key);
        auto b = snapshot_and_lock_two(hv);
        const cuckoo_status st = cuckoo_update_fn(key, fn, hv, b.i[0], b.i[1]);
        return (st == ok);
    }

    //! upsert is a combination of update_fn and insert. It first tries updating
    //! the value associated with \p key using \p fn. If \p key is not in the
    //! table, then it runs an insert with \p key and \p val. It will always
    //! succeed, since if the update fails and the insert finds the key already
    //! inserted, it can retry the update.
    template <typename Updater, typename K, typename... Args>
    typename std::enable_if<
        std::is_convertible<Updater, updater_type>::value,
        void>::type upsert(K&& key, Updater fn, Args&&... val) {
        size_t hv = hashed_key(key);
        cuckoo_status st;
        do {
            auto b = snapshot_and_lock_two(hv);
            size_t hp = get_hashpower();
            st = cuckoo_update_fn(key, fn, hv, b.i[0], b.i[1]);
            if (st == ok) {
                break;
            }

            // We run an insert, since the update failed. Since we already have
            // the locks, we don't run cuckoo_insert_loop immediately, to avoid
            // releasing and re-grabbing the locks. Recall, that the locks will
            // be released at the end of this call to cuckoo_insert.
            st = cuckoo_insert(hv, std::move(b), std::forward<K>(key),
                               std::forward<Args>(val)...);
            if (st == failure_table_full) {
                cuckoo_fast_double(hp);
                // Retry until the insert doesn't fail due to expansion.
                if (cuckoo_insert_loop(hv, std::forward<K>(key),
                                       std::forward<Args>(val)...)) {
                    break;
                }
                // The only valid reason for failure is a duplicate key. In this
                // case, we retry the entire upsert operation.
            }
        } while (st != ok);
    }

    /**
     * Resizes the table to the given hashpower. If this hashpower is not larger
     * than the current hashpower, then it decreases the hashpower to the
     * maximum of the specified value and the smallest hashpower that can hold
     * all the elements currently in the table.
     *
     * @param n the hashpower to set for the table
     * @return true if the table changed size, false otherwise
     * @throw libcuckoo_maximum_hashpower_exceeded if the specified hashpower is
     * greater than the maximum, if one was set
     */
    bool rehash(size_t n) {
        size_t hp = get_hashpower();
        if (n == hp) {
            return false;
        }
        return cuckoo_expand_simple(n, n > hp) == ok;
    }

    /**
     * Reserve enough space in the table for the given number of elements. If
     * the table can already hold that many elements, the function will shrink
     * the table to the smallest hashpower that can hold the maximum of the
     * specified amount and the current table size.
     *
     * @param n the number of elements to reserve space for
     * @return true if the size of the table changed, false otherwise
     * @throw libcuckoo_maximum_hashpower_exceeded if the specified hashpower is
     * greater than the maximum, if one was set
     */
    bool reserve(size_t n) {
        size_t hp = get_hashpower();
        size_t new_hp = reserve_calc(n);
        if (new_hp == hp) {
            return false;
        }
        return cuckoo_expand_simple(new_hp, new_hp > hp) == ok;
    }

    //! hash_function returns the hash function object used by the table.
    hasher hash_function() const noexcept {
        return hash_fn;
    }

    //! key_eq returns the equality predicate object used by the table.
    key_equal key_eq() const noexcept {
        return eq_fn;
    }

    //! Returns a \ref reference to the mapped value stored at the given key.
    //! Note that the reference behaves somewhat differently from an STL map
    //! reference (see the \ref reference documentation for details).
    reference operator[](const key_type& key) {
        return (reference(*this, key));
    }

    //! Returns a \ref const_reference to the mapped value stored at the given
    //! key. This is equivalent to running the overloaded \ref find function
    //! with no value parameter.
    const_reference operator[](const key_type& key) const {
        return find(key);
    }

private:

    template <size_t N>
    struct BucketContainer {
        static_assert(N >= 1 && N <= 3, "BucketContainer should only be used"
                      " for between 1 and 3 locks");
        const cuckoohash_map* map;
        std::array<size_t, N> i;

        BucketContainer() : map(nullptr), i() {}

        template <typename... Args>
        BucketContainer(const cuckoohash_map* _map, Args&&... inds)
            : map(_map), i{{inds...}} {}

        BucketContainer(const cuckoohash_map* _map, std::array<size_t, N> _i)
            : map(_map), i(_i) {}

        BucketContainer(const BucketContainer&) = delete;
        BucketContainer& operator=(const BucketContainer&) = delete;

        // Moving will not invalidate the bucket bucket indices
        BucketContainer(BucketContainer&& bp) {
            *this = std::move(bp);
        }

        BucketContainer& operator=(BucketContainer&& bp) {
            map = bp.map;
            i = bp.i;
            bp.map = nullptr;
            return *this;
        }

        void release() {
            this->~BucketContainer();
            map = nullptr;
        }

        bool is_active() const {
            return map != nullptr;
        }

        ~BucketContainer() {
            if (map) {
                unlock(i);
            }
        }

    private:
        // unlocks the given bucket index.
        void unlock(std::array<size_t, 1> inds) const {
            map->locks_[lock_ind(inds[0])].unlock();
        }

        // unlocks both of the given bucket indexes, or only one if they are
        // equal. Order doesn't matter here.
        void unlock(std::array<size_t, 2> inds) const {
            const size_t l0 = lock_ind(inds[0]);
            const size_t l1 = lock_ind(inds[1]);
            map->locks_[l0].unlock();
            if (l0 != l1) {
                map->locks_[l1].unlock();
            }
        }

        // unlocks the three given buckets
        void unlock(std::array<size_t, 3> inds) const {
            const size_t l0 = lock_ind(inds[0]);
            const size_t l1 = lock_ind(inds[1]);
            const size_t l2 = lock_ind(inds[2]);
            map->locks_[l0].unlock();
            if (l1 != l0) {
                map->locks_[l1].unlock();
            }
            if (l2 != l0 && l2 != l1) {
                map->locks_[l2].unlock();
            }
        }
    };

    typedef BucketContainer<1> OneBucket;
    typedef BucketContainer<2> TwoBuckets;
    typedef BucketContainer<3> ThreeBuckets;

    // This exception is thrown whenever we try to lock a bucket, but the
    // hashpower is not what was expected
    class hashpower_changed {};

    // After taking a lock on the table for the given bucket, this function will
    // check the hashpower to make sure it is the same as what it was before the
    // lock was taken. If it isn't unlock the bucket and throw a
    // hashpower_changed exception.
    inline void check_hashpower(const size_t hp, const size_t lock) const {
        if (get_hashpower() != hp) {
            locks_[lock].unlock();
            LIBCUCKOO_DBG("%s", "hashpower changed\n");
            throw hashpower_changed();
        }
    }

    // locks the given bucket index.
    //
    // throws hashpower_changed if it changed after taking the lock.
    inline OneBucket lock_one(const size_t hp, const size_t i) const {
        const size_t l = lock_ind(i);
        locks_[l].lock();
        check_hashpower(hp, l);
        return OneBucket{this, i};
    }

    // locks the two bucket indexes, always locking the earlier index first to
    // avoid deadlock. If the two indexes are the same, it just locks one.
    //
    // throws hashpower_changed if it changed after taking the lock.
    TwoBuckets lock_two(const size_t hp, const size_t i1,
                        const size_t i2) const {
        size_t l1 = lock_ind(i1);
        size_t l2 = lock_ind(i2);
        if (l2 < l1) {
            std::swap(l1, l2);
        }
        locks_[l1].lock();
        check_hashpower(hp, l1);
        if (l2 != l1) {
            locks_[l2].lock();
        }
        return TwoBuckets{this, i1, i2};
    }

    // lock_two_one locks the three bucket indexes in numerical order, returning
    // the containers as a two (i1 and i2) and a one (i3). The one will not be
    // active if i3 shares a lock index with i1 or i2.
    //
    // throws hashpower_changed if it changed after taking the lock.
    std::pair<TwoBuckets, OneBucket>
    lock_three(const size_t hp, const size_t i1,
               const size_t i2, const size_t i3) const {
        std::array<size_t, 3> l{{
                lock_ind(i1), lock_ind(i2), lock_ind(i3)}};
        std::sort(l.begin(), l.end());
        locks_[l[0]].lock();
        check_hashpower(hp, l[0]);
        if (l[1] != l[0]) {
            locks_[l[1]].lock();
        }
        if (l[2] != l[1]) {
            locks_[l[2]].lock();
        }
        return std::make_pair(
            TwoBuckets{this, i1, i2},
            OneBucket{
                (lock_ind(i3) == lock_ind(i1) ||
                 lock_ind(i3) == lock_ind(i2)) ?
                    nullptr : this, i3});
    }

    // snapshot_and_lock_two loads locks the buckets associated with the given
    // hash value, making sure the hashpower doesn't change before the locks are
    // taken. Thus it ensures that the buckets and locks corresponding to the
    // hash value will stay correct as long as the locks are held. It returns
    // the bucket indices associated with the hash value and the current
    // hashpower.
    TwoBuckets
    snapshot_and_lock_two(const size_t hv) const noexcept {
        while (true) {
            // Store the current hashpower we're using to compute the buckets
            size_t hp = get_hashpower();
            size_t i1 = index_hash(hp, hv);
            size_t i2 = alt_index(hp, partial_key(hv), i1);
            try {
                return lock_two(hp, i1, i2);
            } catch (hashpower_changed&) {
                // The hashpower changed while taking the locks. Try again.
                continue;
            }
        }
    }

    // A resource manager which releases all the locks upon destruction. It can
    // only be moved, not copied.
    class AllUnlocker {
    private:
        // If nullptr, do nothing
        locks_t* locks_;
    public:
        AllUnlocker(locks_t* locks): locks_(locks) {}

        AllUnlocker(const AllUnlocker&) = delete;
        AllUnlocker(AllUnlocker&& au) : locks_(au.locks_) {
            au.locks_ = nullptr;
        }

        AllUnlocker& operator=(const AllUnlocker&) = delete;
        AllUnlocker& operator=(AllUnlocker&& au) {
            locks_ = au.locks_;
            au.locks_ = nullptr;
        }

        void deactivate() {
            locks_ = nullptr;
        }

        void release() {
            if (locks_) {
                for (size_t i = 0; i < locks_->allocated_size(); ++i) {
                    (*locks_)[i].unlock();
                }
                deactivate();
            }
        }

        ~AllUnlocker() {
            release();
        }
    };

    // snapshot_and_lock_all takes all the locks, and returns a deleter object,
    // that releases the locks upon destruction. Note that after taking all the
    // locks, it is okay to change the buckets_ vector and the hashpower_, since
    // no other threads should be accessing the buckets.
    AllUnlocker snapshot_and_lock_all() const noexcept {
        for (size_t i = 0; i < locks_.allocated_size(); ++i) {
            locks_[i].lock();
        }
        return AllUnlocker(&locks_);
    }

    // lock_ind converts an index into buckets to an index into locks.
    static inline size_t lock_ind(const size_t bucket_ind) {
        return bucket_ind & (kNumLocks - 1);
    }

    // hashsize returns the number of buckets corresponding to a given
    // hashpower.
    static inline size_t hashsize(const size_t hp) {
        return size_t(1) << hp;
    }

    // hashmask returns the bitmask for the buckets array corresponding to a
    // given hashpower.
    static inline size_t hashmask(const size_t hp) {
        return hashsize(hp) - 1;
    }

    // hashed_key hashes the given key.
    template <typename K>
    inline size_t hashed_key(const K& key) const {
        return hash_function()(key);
    }

    // index_hash returns the first possible bucket that the given hashed key
    // could be.
    static inline size_t index_hash(const size_t hp, const size_t hv) {
        return hv & hashmask(hp);
    }

    // alt_index returns the other possible bucket that the given hashed key
    // could be. It takes the first possible bucket as a parameter. Note that
    // this function will return the first possible bucket if index is the
    // second possible bucket, so alt_index(ti, partial, alt_index(ti, partial,
    // index_hash(ti, hv))) == index_hash(ti, hv).
    static inline size_t alt_index(const size_t hp, const partial_t partial,
                                   const size_t index) {
        // ensure tag is nonzero for the multiply.
        const partial_t nonzero_tag = (partial >> 1 << 1) + 1;
        // 0xc6a4a7935bd1e995 is the hash constant from 64-bit MurmurHash2
        const size_t hash_of_tag =
            static_cast<size_t>(nonzero_tag * 0xc6a4a7935bd1e995);
        return (index ^ hash_of_tag) & hashmask(hp);
    }

    // partial_key returns a partial_t representing the upper sizeof(partial_t)
    // bytes of the hashed key. This is used for partial-key cuckoohashing, and
    // for finding the alternate bucket of that a key hashes to.
    static inline partial_t partial_key(const size_t hv) {
        return (partial_t)(hv >> ((sizeof(size_t)-sizeof(partial_t)) * 8));
    }

    // A constexpr version of pow that we can use for static_asserts
    static constexpr size_t const_pow(size_t a, size_t b) {
        return (b == 0) ? 1 : a * const_pow(a, b - 1);
    }

    // The maximum number of items in a BFS path.
    static const uint8_t MAX_BFS_PATH_LEN = 5;

    // CuckooRecord holds one position in a cuckoo path. Since cuckoopath
    // elements only define a sequence of alternate hashings for different hash
    // values, we only need to keep track of the hash values being moved, rather
    // than the keys themselves.
    typedef struct {
        size_t bucket;
        size_t slot;
        size_t hv;
    } CuckooRecord;

    typedef std::array<CuckooRecord, MAX_BFS_PATH_LEN> CuckooRecords;

    // b_slot holds the information for a BFS path through the table
    #pragma pack(push,1)
    struct b_slot {
        // The bucket of the last item in the path
        size_t bucket;
        // a compressed representation of the slots for each of the buckets in
        // the path. pathcode is sort of like a base-slot_per_bucket number, and
        // we need to hold at most MAX_BFS_PATH_LEN slots. Thus we need the
        // maximum pathcode to be at least slot_per_bucket^(MAX_BFS_PATH_LEN)
        size_t pathcode;
        static_assert(const_pow(slot_per_bucket, MAX_BFS_PATH_LEN) <
                      std::numeric_limits<decltype(pathcode)>::max(),
                      "pathcode may not be large enough to encode a cuckoo"
                      " path");
        // The 0-indexed position in the cuckoo path this slot occupies. It must
        // be less than MAX_BFS_PATH_LEN, and also able to hold negative values.
        int_fast8_t depth;
        static_assert(MAX_BFS_PATH_LEN - 1 <=
                      std::numeric_limits<decltype(depth)>::max(),
                      "The depth type must able to hold a value of"
                      " MAX_BFS_PATH_LEN - 1");
        static_assert(-1 >= std::numeric_limits<decltype(depth)>::min(),
                      "The depth type must be able to hold a value of -1");
        b_slot() {}
        b_slot(const size_t b, const size_t p, const decltype(depth) d)
            : bucket(b), pathcode(p), depth(d) {
            assert(d < MAX_BFS_PATH_LEN);
        }
    };
    #pragma pack(pop)

    // b_queue is the queue used to store b_slots for BFS cuckoo hashing.
    #pragma pack(push,1)
    class b_queue {
        // The maximum size of the BFS queue. Note that unless it's less than
        // SLOT_PER_BUCKET^MAX_BFS_PATH_LEN, it won't really mean anything.
        static const size_t MAX_CUCKOO_COUNT = 512;
        static_assert((MAX_CUCKOO_COUNT & (MAX_CUCKOO_COUNT - 1)) == 0,
                      "MAX_CUCKOO_COUNT should be a power of 2");
        // A circular array of b_slots
        b_slot slots[MAX_CUCKOO_COUNT];
        // The index of the head of the queue in the array
        size_t first;
        // One past the index of the last item of the queue in the array.
        size_t last;

        // returns the index in the queue after ind, wrapping around if
        // necessary.
        size_t increment(size_t ind) {
            return (ind + 1) & (MAX_CUCKOO_COUNT - 1);
        }

    public:
        b_queue() : first(0), last(0) {}

        void enqueue(b_slot x) {
            assert(!full());
            slots[last] = x;
            last = increment(last);
        }

        b_slot dequeue() {
            assert(!empty());
            b_slot& x = slots[first];
            first = increment(first);
            return x;
        }

        bool empty() {
            return first == last;
        }

        bool full() {
            return increment(last) == first;
        }
    };
    #pragma pack(pop)

    // slot_search searches for a cuckoo path using breadth-first search. It
    // starts with the i1 and i2 buckets, and, until it finds a bucket with an
    // empty slot, adds each slot of the bucket in the b_slot. If the queue runs
    // out of space, it fails.
    //
    // throws hashpower_changed if it changed during the search
    b_slot slot_search(const size_t hp, const size_t i1,
                       const size_t i2) {
        b_queue q;
        // The initial pathcode informs cuckoopath_search which bucket the path
        // starts on
        q.enqueue(b_slot(i1, 0, 0));
        q.enqueue(b_slot(i2, 1, 0));
        while (!q.full() && !q.empty()) {
            b_slot x = q.dequeue();
            // Picks a (sort-of) random slot to start from
            size_t starting_slot = x.pathcode % slot_per_bucket;
            for (size_t i = 0; i < slot_per_bucket && !q.full();
                 ++i) {
                size_t slot = (starting_slot + i) % slot_per_bucket;
                OneBucket ob = lock_one(hp, x.bucket);
                Bucket& b = buckets_[x.bucket];
                if (!b.occupied(slot)) {
                    // We can terminate the search here
                    x.pathcode = x.pathcode * slot_per_bucket + slot;
                    return x;
                }

                // If x has less than the maximum number of path components,
                // create a new b_slot item, that represents the bucket we would
                // have come from if we kicked out the item at this slot.
                const partial_t partial = b.partial(slot);
                if (x.depth < MAX_BFS_PATH_LEN - 1) {
                    b_slot y(alt_index(hp, partial, x.bucket),
                             x.pathcode * slot_per_bucket + slot, x.depth+1);
                    q.enqueue(y);
                }
            }
        }
        // We didn't find a short-enough cuckoo path, so the queue ran out of
        // space. Return a failure value.
        return b_slot(0, 0, -1);
    }

    // cuckoopath_search finds a cuckoo path from one of the starting buckets to
    // an empty slot in another bucket. It returns the depth of the discovered
    // cuckoo path on success, and -1 on failure. Since it doesn't take locks on
    // the buckets it searches, the data can change between this function and
    // cuckoopath_move. Thus cuckoopath_move checks that the data matches the
    // cuckoo path before changing it.
    //
    // throws hashpower_changed if it changed during the search
    int cuckoopath_search(const size_t hp,
                          CuckooRecords& cuckoo_path,
                          const size_t i1, const size_t i2) {
        b_slot x = slot_search(hp, i1, i2);
        if (x.depth == -1) {
            return -1;
        }
        // Fill in the cuckoo path slots from the end to the beginning
        for (int i = x.depth; i >= 0; i--) {
            cuckoo_path[i].slot = x.pathcode % slot_per_bucket;
            x.pathcode /= slot_per_bucket;
        }
        // Fill in the cuckoo_path buckets and keys from the beginning to the
        // end, using the final pathcode to figure out which bucket the path
        // starts on. Since data could have been modified between slot_search
        // and the computation of the cuckoo path, this could be an invalid
        // cuckoo_path.
        CuckooRecord& first = cuckoo_path[0];
        if (x.pathcode == 0) {
            first.bucket = i1;
        } else {
            assert(x.pathcode == 1);
            first.bucket = i2;
        }
        {
            OneBucket ob = lock_one(hp, first.bucket);
            Bucket& b = buckets_[first.bucket];
            if (!b.occupied(first.slot)) {
                // We can terminate here
                return 0;
            }
            first.hv = hashed_key(b.key(first.slot));
        }
        for (int i = 1; i <= x.depth; ++i) {
            CuckooRecord& curr = cuckoo_path[i];
            CuckooRecord& prev = cuckoo_path[i-1];
            assert(prev.bucket == index_hash(hp, prev.hv) ||
                   prev.bucket == alt_index(hp, partial_key(prev.hv),
                                            index_hash(hp, prev.hv)));
            // We get the bucket that this slot is on by computing the alternate
            // index of the previous bucket
            curr.bucket = alt_index(hp, partial_key(prev.hv), prev.bucket);
            OneBucket ob = lock_one(hp, curr.bucket);
            Bucket& b = buckets_[curr.bucket];
            if (!b.occupied(curr.slot)) {
                // We can terminate here
                return i;
            }
            curr.hv = hashed_key(b.key(curr.slot));
        }
        return x.depth;
    }

    // cuckoopath_move moves keys along the given cuckoo path in order to make
    // an empty slot in one of the buckets in cuckoo_insert. Before the start of
    // this function, the two insert-locked buckets were unlocked in run_cuckoo.
    // At the end of the function, if the function returns true (success), then
    // the both insert-locked buckets remain locked. If the function is
    // unsuccessful, then both insert-locked buckets will be unlocked.
    //
    // throws hashpower_changed if it changed during the move
    bool cuckoopath_move(const size_t hp, CuckooRecords& cuckoo_path,
                         size_t depth, TwoBuckets& b) {
        assert(!b.is_active());
        if (depth == 0) {
            // There is a chance that depth == 0, when try_add_to_bucket sees
            // both buckets as full and cuckoopath_search finds one empty. In
            // this case, we lock both buckets. If the slot that
            // cuckoopath_search found empty isn't empty anymore, we unlock them
            // and return false. Otherwise, the bucket is empty and insertable,
            // so we hold the locks and return true.
            const size_t bucket = cuckoo_path[0].bucket;
            assert(bucket == b.i[0] || bucket == b.i[1]);
            b = lock_two(hp, b.i[0], b.i[1]);
            if (!buckets_[bucket].occupied(cuckoo_path[0].slot)) {
                return true;
            } else {
                b.release();
                return false;
            }
        }

        while (depth > 0) {
            CuckooRecord& from = cuckoo_path[depth-1];
            CuckooRecord& to   = cuckoo_path[depth];
            Bucket& fb = buckets_[from.bucket];
            const size_t fs = from.slot;
            Bucket& tb = buckets_[to.bucket];
            const size_t ts = to.slot;
            TwoBuckets twob;
            OneBucket extrab;
            if (depth == 1) {
                // Even though we are only swapping out of one of the original
                // buckets, we have to lock both of them along with the slot we
                // are swapping to, since at the end of this function, they both
                // must be locked. We store tb inside the extrab container so it
                // is unlocked at the end of the loop.
                std::tie(twob, extrab) = lock_three(hp, b.i[0], b.i[1],
                                                    to.bucket);
            } else {
                twob = lock_two(hp, from.bucket, to.bucket);
            }

            // We plan to kick out fs, but let's check if it is still there;
            // there's a small chance we've gotten scooped by a later cuckoo. If
            // that happened, just... try again. Also the slot we are filling in
            // may have already been filled in by another thread, or the slot we
            // are moving from may be empty, both of which invalidate the swap.
            // We only need to check that the hash value is the same, because,
            // even if the keys are different and have the same hash value, then
            // the cuckoopath is still valid.
            if (hashed_key(fb.key(fs)) != from.hv || tb.occupied(ts) ||
                !fb.occupied(fs)) {
                return false;
            }

            Bucket::move_to_bucket(fb, fs, tb, ts);
            if (depth == 1) {
                // Hold onto the locks contained in twob
                b = std::move(twob);
            }
            depth--;
        }
        return true;
    }

    // run_cuckoo performs cuckoo hashing on the table in an attempt to free up
    // a slot on either of the insert buckets, which are assumed to be locked
    // before the start. On success, the bucket and slot that was freed up is
    // stored in insert_bucket and insert_slot. In order to perform the search
    // and the swaps, it has to release the locks, which can lead to certain
    // concurrency issues, the details of which are explained in the function.
    // If run_cuckoo returns ok (success), then the bucket container will be
    // active, otherwise it will not.
    cuckoo_status run_cuckoo(TwoBuckets& b, size_t &insert_bucket,
                             size_t &insert_slot) {
        // We must unlock the buckets here, so that cuckoopath_search and
        // cuckoopath_move can lock buckets as desired without deadlock.
        // cuckoopath_move has to move something out of one of the original
        // buckets as its last operation, and it will lock both buckets and
        // leave them locked after finishing. This way, we know that if
        // cuckoopath_move succeeds, then the buckets needed for insertion are
        // still locked. If cuckoopath_move fails, the buckets are unlocked and
        // we try again. This unlocking does present two problems. The first is
        // that another insert on the same key runs and, finding that the key
        // isn't in the table, inserts the key into the table. Then we insert
        // the key into the table, causing a duplication. To check for this, we
        // search the buckets for the key we are trying to insert before doing
        // so (this is done in cuckoo_insert, and requires that both buckets are
        // locked). Another problem is that an expansion runs and changes the
        // hashpower, meaning the buckets may not be valid anymore. In this
        // case, the cuckoopath functions will have thrown a hashpower_changed
        // exception, which we catch and handle here.
        size_t hp = get_hashpower();
        assert(b.is_active());
        b.release();
        CuckooRecords cuckoo_path;
        bool done = false;
        try {
            while (!done) {
                int depth = cuckoopath_search(hp, cuckoo_path, b.i[0], b.i[1]);
                if (depth < 0) {
                    break;
                }

                if (cuckoopath_move(hp, cuckoo_path, depth, b)) {
                    insert_bucket = cuckoo_path[0].bucket;
                    insert_slot = cuckoo_path[0].slot;
                    assert(insert_bucket == b.i[0] || insert_bucket == b.i[1]);
                    assert(!locks_[lock_ind(b.i[0])].try_lock());
                    assert(!locks_[lock_ind(b.i[1])].try_lock());
                    assert(!buckets_[insert_bucket].occupied(insert_slot));
                    done = true;
                    break;
                }
            }
        } catch (hashpower_changed&) {
            // The hashpower changed while we were trying to cuckoo, which means
            // we want to retry. b.i[0] and b.i[1] should not be locked in this
            // case.
            return failure_under_expansion;
        }
        return done ? ok : failure;
    }

    // try_read_from_bucket will search the bucket for the given key and store
    // the associated value if it finds it.
    template <typename K>
    bool try_read_from_bucket(const partial_t partial, const K &key,
                              mapped_type &val, const Bucket& b) const {
        // Silence a warning from MSVC about partial being unused if is_simple.
        (void)partial;
        for (size_t i = 0; i < slot_per_bucket; ++i) {
            if (!b.occupied(i)) {
                continue;
            }
            if (!is_simple && partial != b.partial(i)) {
                continue;
            }
            if (key_eq()(b.key(i), key)) {
                val = b.val(i);
                return true;
            }
        }
        return false;
    }

    // check_in_bucket will search the bucket for the given key and return true
    // if the key is in the bucket, and false if it isn't.
    template <typename K>
    bool check_in_bucket(const partial_t partial, const K &key,
                         const Bucket& b) const {
        // Silence a warning from MSVC about partial being unused if is_simple.
        (void)partial;
        for (size_t i = 0; i < slot_per_bucket; ++i) {
            if (!b.occupied(i)) {
                continue;
            }
            if (!is_simple && partial != b.partial(i)) {
                continue;
            }
            if (key_eq()(b.key(i), key)) {
                return true;
            }
        }
        return false;
    }

    // add_to_bucket will insert the given key-value pair into the slot. The key
    // and value will be move-constructed into the table, so they are not valid
    // for use afterwards.
    template <typename K, typename... Args>
    void add_to_bucket(const partial_t partial, Bucket& b,
                       const size_t slot, K&& key, Args&&... val) {
        assert(!b.occupied(slot));
        b.partial(slot) = partial;
        b.setKV(slot, std::forward<K>(key), std::forward<Args>(val)...);
        num_inserts_[get_counterid()].num.fetch_add(
            1, std::memory_order_relaxed);
    }

    // try_find_insert_bucket will search the bucket and store the index of an
    // empty slot if it finds one, or -1 if it doesn't. Regardless, it will
    // search the entire bucket and return false if it finds the key already in
    // the table (duplicate key error) and true otherwise.
    template <typename K>
    bool try_find_insert_bucket(const partial_t partial, const K &key,
                                const Bucket& b, int& slot) const {
        // Silence a warning from MSVC about partial being unused if is_simple.
        (void)partial;
        slot = -1;
        bool found_empty = false;
        for (int i = 0; i < static_cast<int>(slot_per_bucket); ++i) {
            if (b.occupied(i)) {
                if (!is_simple && partial != b.partial(i)) {
                    continue;
                }
                if (key_eq()(b.key(i), key)) {
                    return false;
                }
            } else {
                if (!found_empty) {
                    found_empty = true;
                    slot = i;
                }
            }
        }
        return true;
    }

    // try_del_from_bucket will search the bucket for the given key, and set the
    // slot of the key to empty if it finds it.
    template <typename K>
    bool try_del_from_bucket(const partial_t partial,
                             const K &key, Bucket& b) {
        for (size_t i = 0; i < slot_per_bucket; ++i) {
            if (!b.occupied(i)) {
                continue;
            }
            if (!is_simple && b.partial(i) != partial) {
                continue;
            }
            if (key_eq()(b.key(i), key)) {
                b.eraseKV(i);
                num_deletes_[get_counterid()].num.fetch_add(
                    1, std::memory_order_relaxed);
                return true;
            }
        }
        return false;
    }

    // try_update_bucket will search the bucket for the given key and change its
    // associated value if it finds it.
    template <typename K, typename V>
    bool try_update_bucket(const partial_t partial, Bucket& b,
                           const K &key, V&& val) {
        for (size_t i = 0; i < slot_per_bucket; ++i) {
            if (!b.occupied(i)) {
                continue;
            }
            if (!is_simple && b.partial(i) != partial) {
                continue;
            }
            if (key_eq()(b.key(i), key)) {
                b.val(i) = std::forward<V>(val);
                return true;
            }
        }
        return false;
    }

    // try_update_bucket_fn will search the bucket for the given key and change
    // its associated value with the given function if it finds it.
    template <typename K, typename Updater>
    bool try_update_bucket_fn(const partial_t partial, const K &key,
                              Updater fn, Bucket& b) {
        // Silence a warning from MSVC about partial being unused if is_simple.
        (void)partial;
        for (size_t i = 0; i < slot_per_bucket; ++i) {
            if (!b.occupied(i)) {
                continue;
            }
            if (!is_simple && b.partial(i) != partial) {
                continue;
            }
            if (key_eq()(b.key(i), key)) {
                fn(b.val(i));
                return true;
            }
        }
        return false;
    }

    // cuckoo_find searches the table for the given key and value, storing the
    // value in the val if it finds the key. It expects the locks to be taken
    // and released outside the function.
    template <typename K>
    cuckoo_status cuckoo_find(const K &key, mapped_type& val,
                              const size_t hv, const size_t i1,
                              const size_t i2) const {
        const partial_t partial = partial_key(hv);
        if (try_read_from_bucket(partial, key, val, buckets_[i1])) {
            return ok;
        }
        if (try_read_from_bucket(partial, key, val, buckets_[i2])) {
            return ok;
        }
        return failure_key_not_found;
    }

    // cuckoo_contains searches the table for the given key, returning true if
    // it's in the table and false otherwise. It expects the locks to be taken
    // and released outside the function.
    template <typename K>
    bool cuckoo_contains(const K& key, const size_t hv, const size_t i1,
                         const size_t i2) const {
        const partial_t partial = partial_key(hv);
        if (check_in_bucket(partial, key, buckets_[i1])) {
            return true;
        }
        if (check_in_bucket(partial, key, buckets_[i2])) {
            return true;
        }
        return false;
    }

    // cuckoo_insert tries to insert the given key-value pair into an empty slot
    // in either of the buckets, performing cuckoo hashing if necessary. It
    // expects the locks to be taken outside the function, but they are released
    // here, since different scenarios require different handling of the locks.
    // Before inserting, it checks that the key isn't already in the table.
    // cuckoo hashing presents multiple concurrency issues, which are explained
    // in the function. If the insert fails, the key and value won't be
    // move-constructed, so they can be retried.
    template <typename K, typename... Args>
    cuckoo_status cuckoo_insert(const size_t hv, TwoBuckets b,
                                K&& key, Args&&... val) {
        int res1, res2;
        const partial_t partial = partial_key(hv);
        Bucket& b0 = buckets_[b.i[0]];
        if (!try_find_insert_bucket(partial, key, b0, res1)) {
            return failure_key_duplicated;
        }
        Bucket& b1 = buckets_[b.i[1]];
        if (!try_find_insert_bucket(partial, key, b1, res2)) {
            return failure_key_duplicated;
        }
        if (res1 != -1) {
            add_to_bucket(partial, b0, res1, std::forward<K>(key),
                          std::forward<Args>(val)...);
            return ok;
        }
        if (res2 != -1) {
            add_to_bucket(partial, b1, res2, std::forward<K>(key),
                          std::forward<Args>(val)...);
            return ok;
        }

        // we are unlucky, so let's perform cuckoo hashing
        size_t insert_bucket = 0;
        size_t insert_slot = 0;
        cuckoo_status st = run_cuckoo(b, insert_bucket, insert_slot);
        if (st == failure_under_expansion) {
            // The run_cuckoo operation operated on an old version of the table,
            // so we have to try again. We signal to the calling insert method
            // to try again by returning failure_under_expansion.
            return failure_under_expansion;
        } else if (st == ok) {
            assert(!locks_[lock_ind(b.i[0])].try_lock());
            assert(!locks_[lock_ind(b.i[1])].try_lock());
            assert(!buckets_[insert_bucket].occupied(insert_slot));
            assert(insert_bucket == index_hash(get_hashpower(), hv) ||
                   insert_bucket == alt_index(get_hashpower(), partial,
                                              index_hash(get_hashpower(), hv)));
            // Since we unlocked the buckets during run_cuckoo, another insert
            // could have inserted the same key into either b.i[0] or b.i[1], so
            // we check for that before doing the insert.
            if (cuckoo_contains(key, hv, b.i[0], b.i[1])) {
                return failure_key_duplicated;
            }
            add_to_bucket(partial, buckets_[insert_bucket], insert_slot,
                          std::forward<K>(key), std::forward<Args>(val)...);
            return ok;
        }
        assert(st == failure);
        LIBCUCKOO_DBG("hash table is full (hashpower = %zu, hash_items = %zu,"
                      "load factor = %.2f), need to increase hashpower\n",
                      get_hashpower(), cuckoo_size(),
                      cuckoo_loadfactor(get_hashpower()));
        return failure_table_full;
    }

    /**
     * We run cuckoo_insert in a loop until it succeeds in insert and upsert, so
     * we pulled out the loop to avoid duplicating logic
     *
     * @param key the key to insert
     * @param val the value to insert
     * @param hv the hash value of the key
     * @return true if the insert succeeded, false if there was a duplicate key
     * @throw libcuckoo_load_factor_too_low if expansion is necessary, but the
     * load factor of the table is below the threshold
     */
    template <typename K, typename... Args>
    bool cuckoo_insert_loop(size_t hv, K&& key, Args&&... val) {
        cuckoo_status st;
        do {
            auto b = snapshot_and_lock_two(hv);
            size_t hp = get_hashpower();
            st = cuckoo_insert(hv, std::move(b), std::forward<K>(key),
                               std::forward<Args>(val)...);
            if (st == failure_key_duplicated) {
                return false;
            } else if (st == failure_table_full) {
                if (cuckoo_loadfactor(hp) < minimum_load_factor()) {
                    throw libcuckoo_load_factor_too_low(minimum_load_factor());
                }
                // Expand the table and try again
                cuckoo_fast_double(hp);
            }
        } while (st != ok);
        return true;
    }

    // cuckoo_delete searches the table for the given key and sets the slot with
    // that key to empty if it finds it. It expects the locks to be taken and
    // released outside the function.
    template <class K>
    cuckoo_status cuckoo_delete(const K &key, const size_t hv,
                                const size_t i1, const size_t i2) {
        const partial_t partial = partial_key(hv);
        if (try_del_from_bucket(partial, key, buckets_[i1])) {
            return ok;
        }
        if (try_del_from_bucket(partial, key, buckets_[i2])) {
            return ok;
        }
        return failure_key_not_found;
    }

    // cuckoo_update searches the table for the given key and updates its value
    // if it finds it. It expects the locks to be taken and released outside the
    // function.
    template <typename K, typename V>
    cuckoo_status cuckoo_update(const size_t hv, const size_t i1,
                                const size_t i2, const K &key, V&& val) {
        const partial_t partial = partial_key(hv);
        if (try_update_bucket(partial, buckets_[i1], key,
                              std::forward<V>(val))) {
            return ok;
        }
        if (try_update_bucket(partial, buckets_[i2], key,
                              std::forward<V>(val))) {
            return ok;
        }
        return failure_key_not_found;
    }

    // cuckoo_update_fn searches the table for the given key and runs the given
    // function on its value if it finds it, assigning the result of the
    // function to the value. It expects the locks to be taken and released
    // outside the function.
    template <typename K, typename Updater>
    cuckoo_status cuckoo_update_fn(const K &key, Updater fn,
                                   const size_t hv, const size_t i1,
                                   const size_t i2) {
        const partial_t partial = partial_key(hv);
        if (try_update_bucket_fn(partial, key, fn, buckets_[i1])) {
            return ok;
        }
        if (try_update_bucket_fn(partial, key, fn, buckets_[i2])) {
            return ok;
        }
        return failure_key_not_found;
    }

    // cuckoo_clear empties the table, calling the destructors of all the
    // elements it removes from the table. It assumes the locks are taken as
    // necessary.
    cuckoo_status cuckoo_clear() noexcept {
        for (Bucket& b : buckets_) {
            b.clear();
        }
        for (size_t i = 0; i < num_inserts_.size(); ++i) {
            num_inserts_[i].num.store(0);
            num_deletes_[i].num.store(0);
        }
        return ok;
    }

    // cuckoo_size returns the number of elements in the given table.
    size_t cuckoo_size() const noexcept {
        size_t inserts = 0;
        size_t deletes = 0;
        for (size_t i = 0; i < num_inserts_.size(); ++i) {
            inserts += num_inserts_[i].num.load();
            deletes += num_deletes_[i].num.load();
        }
        return inserts-deletes;
    }

    // cuckoo_loadfactor returns the load factor of the given table.
    double cuckoo_loadfactor(const size_t hp) const noexcept {
        return (static_cast<double>(cuckoo_size()) / slot_per_bucket /
                hashsize(hp));
    }

    void move_buckets(size_t current_hp, size_t new_hp,
                      size_t start_lock_ind, size_t end_lock_ind) {
        for (; start_lock_ind < end_lock_ind; ++start_lock_ind) {
            for (size_t bucket_i = start_lock_ind;
                 bucket_i < hashsize(current_hp);
                 bucket_i += locks_t::size()) {
                // By doubling the table size, the index_hash and alt_index of
                // each key got one bit added to the top, at position
                // current_hp, which means anything we have to move will either
                // be at the same bucket position, or exactly
                // hashsize(current_hp) later than the current bucket
                Bucket& old_bucket = buckets_[bucket_i];
                const size_t new_bucket_i = bucket_i + hashsize(current_hp);
                Bucket& new_bucket = buckets_[new_bucket_i];
                size_t new_bucket_slot = 0;

                // Move each item from the old bucket that needs moving into the
                // new bucket
                for (size_t slot = 0; slot < slot_per_bucket; ++slot) {
                    if (!old_bucket.occupied(slot)) {
                        continue;
                    }
                    const size_t hv = hashed_key(old_bucket.key(slot));
                    const size_t old_ihash = index_hash(current_hp, hv);
                    const size_t old_ahash = alt_index(
                        current_hp, old_bucket.partial(slot), old_ihash);
                    const size_t new_ihash = index_hash(new_hp, hv);
                    const size_t new_ahash = alt_index(
                        new_hp, old_bucket.partial(slot), new_ihash);
                    if ((bucket_i == old_ihash && new_ihash == new_bucket_i) ||
                        (bucket_i == old_ahash && new_ahash == new_bucket_i)) {
                        // We're moving the key from the old bucket to the new
                        // one
                        Bucket::move_to_bucket(
                            old_bucket, slot, new_bucket, new_bucket_slot++);
                    } else {
                        // Check that we don't want to move the new key
                        assert(
                            (bucket_i == old_ihash && new_ihash == old_ihash) ||
                            (bucket_i == old_ahash && new_ahash == old_ahash));
                    }
                }
            }
            // Now we can unlock the lock, because all the buckets corresponding
            // to it have been unlocked
            locks_[start_lock_ind].unlock();
        }
    }

    // cuckoo_fast_double will double the size of the table by taking advantage
    // of the properties of index_hash and alt_index. If the key's move
    // constructor is not noexcept, we use cuckoo_expand_simple, since that
    // provides a strong exception guarantee.
    cuckoo_status cuckoo_fast_double(size_t current_hp) {
        if (!std::is_nothrow_move_constructible<storage_value_type>::value) {
            LIBCUCKOO_DBG("%s", "cannot run cuckoo_fast_double because kv-pair "
                          "is not nothrow move constructible");
            return cuckoo_expand_simple(current_hp + 1, true);
        }
        size_t new_hp = current_hp + 1;
        size_t mhp = maximum_hashpower();
        if (mhp != NO_MAXIMUM_HASHPOWER && new_hp > mhp) {
            throw libcuckoo_maximum_hashpower_exceeded(new_hp);
        }

        std::lock_guard<expansion_lock_t> l(expansion_lock_);
        if (get_hashpower() != current_hp) {
            // Most likely another expansion ran before this one could grab the
            // locks
            LIBCUCKOO_DBG("%s", "another expansion is on-going\n");
            return failure_under_expansion;
        }

        locks_.allocate(std::min(locks_t::size(), hashsize(new_hp)));
        auto unlocker = snapshot_and_lock_all();
        buckets_.resize(buckets_.size() * 2);
        set_hashpower(new_hp);

        // We gradually unlock the new table, by processing each of the buckets
        // corresponding to each lock we took. For each slot in an old bucket,
        // we either leave it in the old bucket, or move it to the corresponding
        // new bucket. After we're done with the bucket, we release the lock on
        // it and the new bucket, letting other threads using the new map
        // gradually. We only unlock the locks being used by the old table,
        // because unlocking new locks would enable operations on the table
        // before we want them.
        const size_t locks_to_move = std::min(locks_t::size(),
                                              hashsize(current_hp));
        parallel_exec(0, locks_to_move, kNumCores(),
                      [this, current_hp, new_hp]
                      (size_t start, size_t end, std::exception_ptr& eptr) {
                          try {
                              move_buckets(current_hp, new_hp, start, end);
                          } catch (...) {
                              eptr = std::current_exception();
                          }
                      });
        parallel_exec(locks_to_move, locks_.allocated_size(), kNumCores(),
                      [this](size_t i, size_t end, std::exception_ptr&) {
                          for (; i < end; ++i) {
                              locks_[i].unlock();
                          }
                      });
        // Since we've unlocked the buckets ourselves, we don't need the
        // unlocker to do it for us.
        unlocker.deactivate();
        return ok;
    }

    // cuckoo_expand_simple will resize the table to at least the given
    // new_hashpower. If is_expansion is true, new_hashpower must be greater
    // than the current size of the table. If it's false, then new_hashpower
    // must be less. When we're shrinking the table, if the current table
    // contains more elements than can be held by new_hashpower, the resulting
    // hashpower will be greater than new_hashpower. It needs to take all the
    // bucket locks, since no other operations can change the table during
    // expansion. Throws libcuckoo_maximum_hashpower_exceeded if we're expanding
    // beyond the maximum hashpower, and we have an actual limit.
    cuckoo_status cuckoo_expand_simple(size_t new_hp,
                                       bool is_expansion) {
        size_t mhp = maximum_hashpower();
        if (mhp != NO_MAXIMUM_HASHPOWER && new_hp > mhp) {
            throw libcuckoo_maximum_hashpower_exceeded(new_hp);
        }
        auto unlocker = snapshot_and_lock_all();
        const size_t hp = get_hashpower();
        if ((is_expansion && new_hp <= hp) ||
            (!is_expansion && new_hp >= hp)) {
            // Most likely another expansion ran before this one could grab the
            // locks
            LIBCUCKOO_DBG("%s", "another expansion is on-going\n");
            return failure_under_expansion;
        }

        // Creates a new hash table with hashpower new_hp and adds all
        // the elements from the old buckets
        cuckoohash_map<Key, T, Hash, Pred, Alloc, slot_per_bucket> new_map(
            hashsize(new_hp) * slot_per_bucket);
        parallel_exec(
            0, hashsize(hp), kNumCores(),
            [this, &new_map]
            (size_t i, size_t end, std::exception_ptr& eptr) {
                try {
                    for (; i < end; ++i) {
                        for (size_t j = 0; j < slot_per_bucket; ++j) {
                            if (buckets_[i].occupied(j)) {
                                storage_value_type& kvpair = (
                                    buckets_[i].storage_kvpair(j));
                                new_map.insert(std::move(kvpair.first),
                                               std::move(kvpair.second));
                            }
                        }
                    }
                } catch (...) {
                    eptr = std::current_exception();
                }
            });

        // Swap the current buckets vector with new_map's and set the hashpower.
        // This is okay, because we have all the locks, so nobody else should be
        // reading from the buckets array. Then the old buckets array will be
        // deleted when new_map is deleted. All the locks should be released by
        // the unlocker as well.
        std::swap(buckets_, new_map.buckets_);
        set_hashpower(new_map.hashpower_);
        return ok;
    }

public:
    //! A locked_table is an ownership wrapper around a \ref cuckoohash_map
    //! table instance. When given a table instance, it takes all the locks on
    //! the table, blocking all outside operations on the table. Because the
    //! locked_table has unique ownership of the table, it can provide a set of
    //! operations on the table that aren't possible in a concurrent context.
    //! Right now, this includes the ability to construct STL-compatible
    //! iterators on the table. When the locked_table is destroyed (or the \ref
    //! release method is called), it will release all locks on the table. This
    //! will invalidate all existing iterators.
    class locked_table {
        // A manager for all the locks we took on the table.
        AllUnlocker unlocker_;
        // A reference to the buckets owned by the table
        std::reference_wrapper<buckets_t> buckets_;
        // A boolean shared to all iterators, indicating whether the
        // locked_table has ownership of the hashtable or not.
        std::shared_ptr<bool> has_table_lock_;

        // The constructor locks the entire table, retrying until
        // snapshot_and_lock_all succeeds. We keep this constructor private (but
        // expose it to the cuckoohash_map class), since we don't want users
        // calling it.
        locked_table(cuckoohash_map<Key, T, Hash, Pred, Alloc,
                     SLOT_PER_BUCKET>& hm)
            : unlocker_(std::move(hm.snapshot_and_lock_all())),
              buckets_(hm.buckets_),
              has_table_lock_(new bool(true)) {}

    public:
        locked_table(locked_table&& lt)
            : unlocker_(std::move(lt.unlocker_)),
              buckets_(std::move(lt.buckets_)),
              has_table_lock_(std::move(lt.has_table_lock_)) {}

        locked_table& operator=(locked_table&& lt) {
            release();
            unlocker_ = std::move(lt.unlocker_);
            buckets_ = std::move(lt.buckets_);
            has_table_lock_ = std::move(lt.has_table_lock_);
            return *this;
        }

        //! Returns true if the locked table still has ownership of the
        //! hashtable, false otherwise.
        bool has_table_lock() const noexcept {
            return has_table_lock_ && *has_table_lock_;
        }

        //! release unlocks the table, thereby freeing it up for other
        //! operations, but also invalidating all iterators and future
        //! operations with this table. It is idempotent.
        void release() noexcept {
            if (has_table_lock()) {
                unlocker_.release();
                *has_table_lock_ = false;
            }
        }

        ~locked_table() {
            release();
        }

    private:
        //! A templated iterator whose implementation works for both const and
        //! non_const iterators. It is an STL-style BidirectionalIterator that
        //! can be used to iterate over a locked table.
        template <bool IS_CONST>
        class templated_iterator :
            public std::iterator<std::bidirectional_iterator_tag, value_type> {

            typedef typename std::conditional<
                IS_CONST, const buckets_t, buckets_t>::type
            maybe_const_buckets_t;

            // The buckets locked and owned by the locked table being iterated
            // over.
            std::reference_wrapper<maybe_const_buckets_t> buckets_;

            // The shared boolean indicating whether the iterator points to a
            // still-locked table or not. It should never be nullptr.
            std::shared_ptr<bool> has_table_lock_;

            // The bucket index of the item being pointed to. For implementation
            // convenience, we let it take on negative values.
            intmax_t index_;
            // The slot in the bucket of the item being pointed to. For
            // implementation convenience, we let it take on negative values.
            intmax_t slot_;

        public:
            //! Return true if the iterators are from the same locked table and
            //! location, false otherwise. This will return false if either of
            //! the iterators has lost ownership of its table.
            template <bool OTHER_CONST>
            bool operator==(const templated_iterator<OTHER_CONST>&
                            it) const noexcept {
                return (*has_table_lock_ && *it.has_table_lock_
                        && &buckets_.get() == &it.buckets_.get()
                        && index_ == it.index_ && slot_ == it.slot_);
            }

            //! Equivalent to !operator==(it)
            template <bool OTHER_CONST>
            bool operator!=(const templated_iterator<OTHER_CONST>&
                            it) const noexcept {
                return !(operator==(it));
            }

            //! Return the key-value pair pointed to by the iterator. Behavior
            //! is undefined if the iterator is at the end.
            const value_type& operator*() const {
                check_iterator();
                return buckets_.get()[index_].kvpair(slot_);
            }

            //! Returns a mutable reference to the current key-value pair
            //! pointed to by the iterator. Behavior is undefined if the
            //! iterator is at the end.
            ENABLE_IF(, !IS_CONST, value_type&) operator*() {
                check_iterator();
                return buckets_.get()[static_cast<size_t>(index_)].
                    kvpair(static_cast<size_t>(slot_));
            }

            //! Return a pointer to the immutable key-value pair pointed to by
            //! the iterator. Behavior is undefined if the iterator is at the
            //! end.
            const value_type* operator->() const {
                check_iterator();
                return &buckets_.get()[index_].kvpair(slot_);
            }

            //! Returns a mutable pointer to the current key-value pair pointed
            //! to by the iterator. Behavior is undefined if the iterator is at
            //! the end.
            ENABLE_IF(, !IS_CONST, value_type*) operator->() {
                check_iterator();
                return &buckets_.get()[index_].kvpair(slot_);
            }


            //! Advance the iterator to the next item in the table, or to the
            //! end of the table. Returns the iterator at its new position.
            //! Behavior is undefined if the iterator is at the end.
            templated_iterator& operator++() {
                // Move forward until we get to a slot that is occupied, or we
                // get to the end
                check_iterator();
                for (; static_cast<size_t>(index_) < buckets_.get().size();
                     ++index_) {
                    while (static_cast<size_t>(++slot_) < SLOT_PER_BUCKET) {
                        if (buckets_.get()[static_cast<size_t>(index_)].
                            occupied(static_cast<size_t>(slot_))) {
                            return *this;
                        }
                    }
                    slot_ = -1;
                }
                // We're at the end, so set index_ and slot_ to the end position
                std::tie(index_, slot_) = end_pos(buckets_.get());
                return *this;
            }

            //! Advance the iterator to the next item in the table, or to the
            //! end of the table. Returns the iterator at its old position.
            //! Behavior is undefined if the iterator is at the end.
            templated_iterator operator++(int) {
                templated_iterator old(*this);
                ++(*this);
                return old;
            }

            //! Move the iterator back to the previous item in the table.
            //! Returns the iterator at its new position. Behavior is undefined
            //! if the iterator is at the beginning.
            templated_iterator& operator--() {
                // Move backward until we get to the beginning. If we try to
                // move before that, we stop.
                check_iterator();
                for (; index_ >= 0; --index_) {
                    while (--slot_ >= 0) {
                        if (buckets_.get()[static_cast<size_t>(index_)]
                            .occupied(static_cast<size_t>(slot_))) {
                            return *this;
                        }
                    }
                    slot_ = SLOT_PER_BUCKET;
                }
                // Either we iterated before begin(), which means we're in
                // undefined territory, or we iterated from the end of the table
                // back, which means the table is empty. Either way, setting the
                // index_ and slot_ to end_pos() is okay.
                std::tie(index_, slot_) = end_pos(buckets_.get());
                return *this;
            }

            //! Move the iterator back to the previous item in the table.
            //! Returns the iterator at its old position. Behavior is undefined
            //! if the iterator is at the beginning.
            templated_iterator operator--(int) {
                templated_iterator old(*this);
                --(*this);
                return old;
            }

        private:
            static const std::pair<intmax_t, intmax_t> end_pos(
                const buckets_t& buckets) {
                // When index_ == buckets.size() and slot_ == 0, we're at the
                // end of the table. When index_ and slot_ point to the data
                // with the lowest bucket and slot, we're at the beginning of
                // the table. If there is nothing in the table, index_ ==
                // buckets.size() and slot_ == 0 also means we're at the
                // beginning of the table (so begin() == end()).
                return {buckets.size(), 0};
            }

            // The private constructor is used by locked_table to create
            // iterators from scratch. If the given index_-slot_ pair is at the
            // end of the table, or that spot is occupied, stay. Otherwise, step
            // forward to the next data item, or to the end of the table.
            templated_iterator(
                maybe_const_buckets_t& buckets,
                std::shared_ptr<bool> has_table_lock, size_t index, size_t slot)
                : buckets_(buckets), has_table_lock_(has_table_lock),
                  index_(static_cast<intmax_t>(index)),
                  slot_(static_cast<intmax_t>(slot)) {
                if (std::make_pair(index_, slot_) != end_pos(buckets) &&
                    !buckets[static_cast<size_t>(index_)]
                    .occupied(static_cast<size_t>(slot_))) {
                    operator++();
                }
            }

            // Throws an exception if the iterator has been invalidated because
            // the locked_table lost ownership of the table info.
            void check_iterator() const {
                if (!(*has_table_lock_)) {
                    throw std::runtime_error("Iterator has been invalidated");
                }
            }

            friend class cuckoohash_map<Key, T, Hash, Pred,
                                        Alloc, SLOT_PER_BUCKET>;
        };

    public:
        typedef templated_iterator<true> const_iterator;
        typedef templated_iterator<false> iterator;

        //! begin returns an iterator to the beginning of the table
        iterator begin() {
            check_table();
            return iterator(buckets_.get(), has_table_lock_, 0, 0);
        }

        //! begin returns a const_iterator to the beginning of the table
        const_iterator begin() const {
            check_table();
            return const_iterator(buckets_.get(), has_table_lock_, 0, 0);
        }

        //! cbegin returns a const_iterator to the beginning of the table
        const_iterator cbegin() const {
            return begin();
        }

        //! end returns an iterator to the end of the table
        iterator end() {
            check_table();
            const auto end_pos = const_iterator::end_pos(buckets_.get());
            return iterator(buckets_.get(), has_table_lock_,
                            static_cast<size_t>(end_pos.first),
                            static_cast<size_t>(end_pos.second));
        }

        //! end returns a const_iterator to the end of the table
        const_iterator end() const {
            check_table();
            const auto end_pos = const_iterator::end_pos(buckets_.get());
            return const_iterator(buckets_.get(), has_table_lock_,
                                  static_cast<size_t>(end_pos.first),
                                  static_cast<size_t>(end_pos.second));
        }

        //! cend returns a const_iterator to the end of the table
        const_iterator cend() const {
            return end();
        }

    private:
        // Throws an exception if the locked_table has been invalidated because
        // it lost ownership of the table info.
        void check_table() const {
            if (!has_table_lock()) {
                throw std::runtime_error(
                    "locked_table lost ownership of table");
            }
        }

        friend class cuckoohash_map<Key, T, Hash, Pred, Alloc, SLOT_PER_BUCKET>;
    };

    //! lock_table construct a \ref locked_table object that owns all the locks
    //! in the table. This can be used to iterate through the table.
    locked_table lock_table() {
        return locked_table(*this);
    }

    // This class is a friend for unit testing
    friend class UnitTestInternalAccess;

    // Member variables
private:
    // 2**hashpower is the number of buckets. This cannot be changed unless all
    // the locks are taken on the table. Since it is still read and written by
    // multiple threads not necessarily synchronized by a lock, we keep it
    // atomic
    std::atomic<size_t> hashpower_;

    // vector of buckets. The size or memory location of the buckets cannot be
    // changed unless al the locks are taken on the table. Thus, it is only safe
    // to access the buckets_ vector when you have at least one lock held.
    buckets_t buckets_;

    // array of locks. marked mutable, so that const methods can take locks.
    // Even though it's a vector, it should not ever change in size after the
    // initial allocation.
    mutable locks_t locks_;

    // a lock to synchronize expansions
    expansion_lock_t expansion_lock_;

    // per-core counters for the number of inserts and deletes
    std::vector<
        cacheint, typename allocator_type::template rebind<cacheint>::other>
    num_inserts_, num_deletes_;

    // stores the minimum load factor allowed for automatic expansions. Whenever
    // an automatic expansion is triggered (during an insertion where cuckoo
    // hashing fails, for example), we check the load factor against this
    // double, and throw an exception if it's lower than this value. It can be
    // used to signal when the hash function is bad or the input adversarial.
    std::atomic<double> minimum_load_factor_;

    // stores the maximum hashpower allowed for any expansions. If set to
    // NO_MAXIMUM_HASHPOWER, this limit will be disregarded.
    std::atomic<size_t> maximum_hashpower_;

    // The hash function
    hasher hash_fn;

    // The equality function
    key_equal eq_fn;
};

#endif // _CUCKOOHASH_MAP_HH
