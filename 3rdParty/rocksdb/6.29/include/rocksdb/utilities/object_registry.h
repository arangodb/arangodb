// Copyright (c) 2016-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#ifndef ROCKSDB_LITE

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "rocksdb/status.h"
#include "rocksdb/utilities/regex.h"

namespace ROCKSDB_NAMESPACE {
class Customizable;
class Logger;
class ObjectLibrary;

// Returns a new T when called with a string. Populates the std::unique_ptr
// argument if granting ownership to caller.
template <typename T>
using FactoryFunc =
    std::function<T*(const std::string&, std::unique_ptr<T>*, std::string*)>;

// The signature of the function for loading factories
// into an object library.  This method is expected to register
// factory functions in the supplied ObjectLibrary.
// The ObjectLibrary is the library in which the factories will be loaded.
// The std::string is the argument passed to the loader function.
// The RegistrarFunc should return the number of objects loaded into this
// library
using RegistrarFunc = std::function<int(ObjectLibrary&, const std::string&)>;

template <typename T>
using ConfigureFunc = std::function<Status(T*)>;

class ObjectLibrary {
 private:
  // Base class for an Entry in the Registry.
  class Entry {
   public:
    virtual ~Entry() {}
    virtual bool Matches(const std::string& target) const = 0;
    virtual const char* Name() const = 0;
  };

  // A class that implements an Entry based on Regex.
  //
  // WARNING: some regexes are problematic for std::regex; see
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61582 for example
  //
  // This class is deprecated and will be removed in a future release
  class RegexEntry : public Entry {
   public:
    explicit RegexEntry(const std::string& name) : name_(name) {
      Regex::Parse(name, &regex_).PermitUncheckedError();
    }

    bool Matches(const std::string& target) const override {
      return regex_.Matches(target);
    }
    const char* Name() const override { return name_.c_str(); }

   private:
    std::string name_;
    Regex regex_;  // The pattern for this entry
  };

 public:
  // Class for matching target strings to a pattern.
  // Entries consist of a name that starts the pattern and attributes
  // The following attributes can be added to the entry:
  //   -Suffix: Comparable to name(suffix)
  //   -Separator: Comparable to name(separator).+ or name(separator).*
  //   -Number: Comparable to name(separator).[0-9]+
  //   -AltName: Comparable to (name|alt)
  //   -Optional: Comparable to name(separator)?
  // Multiple separators can be combined and cause multiple matches.
  // For example, Pattern("A").AnotherName("B").AddSeparator("@").AddNumber("#")
  // is roughly equivalent to "(A|B)@.+#.+"
  //
  // Note that though this class does provide some regex-style matching,
  // it is not a full regex parser and has some key differences:
  //   - Separators are matched left-most.  For example, an entry
  //     Name("Hello").AddSeparator(" ").AddSuffix("!") would match
  //     "Hello world!", but not "Hello world!!"
  //   - No backtracking is necessary, enabling reliably efficient matching
  class PatternEntry : public Entry {
   private:
    enum Quantifier {
      kMatchZeroOrMore,  // [suffix].*
      kMatchAtLeastOne,  // [suffix].+
      kMatchExact,       // [suffix]
      kMatchNumeric,     // [suffix][0-9]+
    };

   public:
    // Short-cut for creating an entry that matches to a
    // Customizable::IndividualId
    static PatternEntry AsIndividualId(const std::string& name) {
      PatternEntry entry(name, true);
      entry.AddSeparator("@");
      entry.AddSeparator("#");
      return entry;
    }

    // Creates a new PatternEntry for "name".  If optional is true,
    // Matches will also return true if name==target
    explicit PatternEntry(const std::string& name, bool optional = true)
        : name_(name), optional_(optional), slength_(0) {
      nlength_ = name_.size();
    }

    // Adds a suffix (exact match of separator with no trailing characters) to
    // the separator
    PatternEntry& AddSuffix(const std::string& suffix) {
      separators_.emplace_back(suffix, kMatchExact);
      slength_ += suffix.size();
      return *this;
    }

    // Adds a separator (exact match of separator with trailing characters) to
    // the entry
    // If at_least_one is true, the separator must be followed by at least
    // one character (e.g. separator.+).
    // If at_least_one is false, the separator may be followed by zero or
    // more characters (e.g. separator.*).
    PatternEntry& AddSeparator(const std::string& separator,
                               bool at_least_one = true) {
      slength_ += separator.size();
      if (at_least_one) {
        separators_.emplace_back(separator, kMatchAtLeastOne);
        ++slength_;
      } else {
        separators_.emplace_back(separator, kMatchZeroOrMore);
      }
      return *this;
    }

    // Adds a separator (exact match of separator with trailing numbers) to the
    // entry
    PatternEntry& AddNumber(const std::string& separator) {
      separators_.emplace_back(separator, kMatchNumeric);
      slength_ += separator.size() + 1;
      return *this;
    }

    // Sets another name that this entry will match, similar to (name|alt)
    PatternEntry& AnotherName(const std::string& alt) {
      names_.emplace_back(alt);
      return *this;
    }

    // Sets whether the separators are required -- similar to name(separator)?
    // If optional is true, then name(separator)? would match
    // If optional is false, then the separators must also match
    PatternEntry& SetOptional(bool optional) {
      optional_ = optional;
      return *this;
    }

    // Checks to see if the target matches this entry
    bool Matches(const std::string& target) const override;
    const char* Name() const override { return name_.c_str(); }

   private:
    size_t MatchSeparatorAt(size_t start, Quantifier mode,
                            const std::string& target, size_t tlen,
                            const std::string& pattern) const;

    bool MatchesTarget(const std::string& name, size_t nlen,
                       const std::string& target, size_t ylen) const;
    std::string name_;                // The base name for this entry
    size_t nlength_;                  // The length of name_
    std::vector<std::string> names_;  // Alternative names for this entry
    bool optional_;   // Whether matching of separators is required
    size_t slength_;  // The minimum required length to match the separators
    std::vector<std::pair<std::string, Quantifier>>
        separators_;  // What to match
  };                  // End class Entry

 private:
  // An Entry containing a FactoryFunc for creating new Objects
  template <typename T>
  class FactoryEntry : public Entry {
   public:
    FactoryEntry(Entry* e, FactoryFunc<T> f)
        : entry_(e), factory_(std::move(f)) {}
    bool Matches(const std::string& target) const override {
      return entry_->Matches(target);
    }
    const char* Name() const override { return entry_->Name(); }

    // Creates a new T object.
    T* NewFactoryObject(const std::string& target, std::unique_ptr<T>* guard,
                        std::string* msg) const {
      return factory_(target, guard, msg);
    }
    const FactoryFunc<T>& GetFactory() const { return factory_; }

   private:
    std::unique_ptr<Entry> entry_;  // What to match for this entry
    FactoryFunc<T> factory_;
  };  // End class FactoryEntry
 public:
  explicit ObjectLibrary(const std::string& id) { id_ = id; }

  const std::string& GetID() const { return id_; }

  // Finds the factory function for the input target.
  // @see PatternEntry for the matching rules to target
  // @return If matched, the FactoryFunc for this target, else nullptr
  template <typename T>
  FactoryFunc<T> FindFactory(const std::string& target) const {
    std::unique_lock<std::mutex> lock(mu_);
    auto factories = factories_.find(T::Type());
    if (factories != factories_.end()) {
      for (const auto& e : factories->second) {
        if (e->Matches(target)) {
          const auto* fe =
              static_cast<const ObjectLibrary::FactoryEntry<T>*>(e.get());
          return fe->GetFactory();
        }
      }
    }
    return nullptr;
  }

  // Returns the total number of factories registered for this library.
  // This method returns the sum of all factories registered for all types.
  // @param num_types returns how many unique types are registered.
  size_t GetFactoryCount(size_t* num_types) const;

  void Dump(Logger* logger) const;

  // Registers the factory with the library for the regular expression pattern.
  // If the pattern matches, the factory may be used to create a new object.
  //
  // WARNING: some regexes are problematic for std::regex; see
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61582 for example
  //
  // Deprecated. Will be removed in a major release. Code should use AddFactory
  // instead.
  template <typename T>
  const FactoryFunc<T>& Register(const std::string& pattern,
                                 const FactoryFunc<T>& factory) {
    std::unique_ptr<Entry> entry(
        new FactoryEntry<T>(new RegexEntry(pattern), factory));
    AddFactoryEntry(T::Type(), std::move(entry));
    return factory;
  }

  // Registers the factory with the library for the name.
  // If name==target, the factory may be used to create a new object.
  template <typename T>
  const FactoryFunc<T>& AddFactory(const std::string& name,
                                   const FactoryFunc<T>& func) {
    std::unique_ptr<Entry> entry(
        new FactoryEntry<T>(new PatternEntry(name), func));
    AddFactoryEntry(T::Type(), std::move(entry));
    return func;
  }

  // Registers the factory with the library for the entry.
  // If the entry matches the target, the factory may be used to create a new
  // object.
  // @see PatternEntry for the matching rules.
  template <typename T>
  const FactoryFunc<T>& AddFactory(const PatternEntry& entry,
                                   const FactoryFunc<T>& func) {
    std::unique_ptr<Entry> factory(
        new FactoryEntry<T>(new PatternEntry(entry), func));
    AddFactoryEntry(T::Type(), std::move(factory));
    return func;
  }

  // Invokes the registrar function with the supplied arg for this library.
  int Register(const RegistrarFunc& registrar, const std::string& arg) {
    return registrar(*this, arg);
  }

  // Returns the default ObjectLibrary
  static std::shared_ptr<ObjectLibrary>& Default();

 private:
  void AddFactoryEntry(const char* type, std::unique_ptr<Entry>&& entry) {
    std::unique_lock<std::mutex> lock(mu_);
    auto& factories = factories_[type];
    factories.emplace_back(std::move(entry));
  }

  // Protects the entry map
  mutable std::mutex mu_;
  // ** FactoryFunctions for this loader, organized by type
  std::unordered_map<std::string, std::vector<std::unique_ptr<Entry>>>
      factories_;

  // The name for this library
  std::string id_;
};

// The ObjectRegistry is used to register objects that can be created by a
// name/pattern at run-time where the specific implementation of the object may
// not be known in advance.
class ObjectRegistry {
 public:
  static std::shared_ptr<ObjectRegistry> NewInstance();
  static std::shared_ptr<ObjectRegistry> NewInstance(
      const std::shared_ptr<ObjectRegistry>& parent);
  static std::shared_ptr<ObjectRegistry> Default();
  explicit ObjectRegistry(const std::shared_ptr<ObjectRegistry>& parent)
      : parent_(parent) {}

  std::shared_ptr<ObjectLibrary> AddLibrary(const std::string& id) {
    auto library = std::make_shared<ObjectLibrary>(id);
    AddLibrary(library);
    return library;
  }

  void AddLibrary(const std::shared_ptr<ObjectLibrary>& library) {
    std::unique_lock<std::mutex> lock(library_mutex_);
    libraries_.push_back(library);
  }

  void AddLibrary(const std::string& id, const RegistrarFunc& registrar,
                  const std::string& arg) {
    auto library = AddLibrary(id);
    library->Register(registrar, arg);
  }

  // Creates a new T using the factory function that was registered for this
  // target.  Searches through the libraries to find the first library where
  // there is an entry that matches target (see PatternEntry for the matching
  // rules).
  //
  // If no registered functions match, returns nullptr. If multiple functions
  // match, the factory function used is unspecified.
  //
  // Populates guard with result pointer if caller is granted ownership.
  // Deprecated.  Use NewShared/Static/UniqueObject instead.
  template <typename T>
  T* NewObject(const std::string& target, std::unique_ptr<T>* guard,
               std::string* errmsg) {
    guard->reset();
    auto factory = FindFactory<T>(target);
    if (factory != nullptr) {
      return factory(target, guard, errmsg);
    } else {
      *errmsg = std::string("Could not load ") + T::Type();
      return nullptr;
    }
  }

  // Creates a new unique T using the input factory functions.
  // Returns OK if a new unique T was successfully created
  // Returns NotSupported if the type/target could not be created
  // Returns InvalidArgument if the factory return an unguarded object
  //                      (meaning it cannot be managed by a unique ptr)
  template <typename T>
  Status NewUniqueObject(const std::string& target,
                         std::unique_ptr<T>* result) {
    std::string errmsg;
    T* ptr = NewObject(target, result, &errmsg);
    if (ptr == nullptr) {
      return Status::NotSupported(errmsg, target);
    } else if (*result) {
      return Status::OK();
    } else {
      return Status::InvalidArgument(std::string("Cannot make a unique ") +
                                         T::Type() + " from unguarded one ",
                                     target);
    }
  }

  // Creates a new shared T using the input factory functions.
  // Returns OK if a new shared T was successfully created
  // Returns NotSupported if the type/target could not be created
  // Returns InvalidArgument if the factory return an unguarded object
  //                      (meaning it cannot be managed by a shared ptr)
  template <typename T>
  Status NewSharedObject(const std::string& target,
                         std::shared_ptr<T>* result) {
    std::string errmsg;
    std::unique_ptr<T> guard;
    T* ptr = NewObject(target, &guard, &errmsg);
    if (ptr == nullptr) {
      return Status::NotSupported(errmsg, target);
    } else if (guard) {
      result->reset(guard.release());
      return Status::OK();
    } else {
      return Status::InvalidArgument(std::string("Cannot make a shared ") +
                                         T::Type() + " from unguarded one ",
                                     target);
    }
  }

  // Creates a new static T using the input factory functions.
  // Returns OK if a new static T was successfully created
  // Returns NotSupported if the type/target could not be created
  // Returns InvalidArgument if the factory return a guarded object
  //                      (meaning it is managed by a unique ptr)
  template <typename T>
  Status NewStaticObject(const std::string& target, T** result) {
    std::string errmsg;
    std::unique_ptr<T> guard;
    T* ptr = NewObject(target, &guard, &errmsg);
    if (ptr == nullptr) {
      return Status::NotSupported(errmsg, target);
    } else if (guard.get()) {
      return Status::InvalidArgument(std::string("Cannot make a static ") +
                                         T::Type() + " from a guarded one ",
                                     target);
    } else {
      *result = ptr;
      return Status::OK();
    }
  }

  // Sets the object for the given id/type to be the input object
  // If the registry does not contain this id/type, the object is added and OK
  // is returned. If the registry contains a different object, an error is
  // returned. If the registry contains the input object, OK is returned.
  template <typename T>
  Status SetManagedObject(const std::shared_ptr<T>& object) {
    assert(object != nullptr);
    return SetManagedObject(object->GetId(), object);
  }

  template <typename T>
  Status SetManagedObject(const std::string& id,
                          const std::shared_ptr<T>& object) {
    const auto c = std::static_pointer_cast<Customizable>(object);
    return SetManagedObject(T::Type(), id, c);
  }

  // Returns the object for the given id, if one exists.
  // If the object is not found in the registry, a nullptr is returned
  template <typename T>
  std::shared_ptr<T> GetManagedObject(const std::string& id) const {
    auto c = GetManagedObject(T::Type(), id);
    return std::static_pointer_cast<T>(c);
  }

  // Returns the set of managed objects found in the registry matching
  // the input type and ID.
  // If the input id is not empty, then only objects of that class
  // (IsInstanceOf(id)) will be returned (for example, only return LRUCache
  // objects) If the input id is empty, then all objects of that type (all Cache
  // objects)
  template <typename T>
  Status ListManagedObjects(const std::string& id,
                            std::vector<std::shared_ptr<T>>* results) const {
    std::vector<std::shared_ptr<Customizable>> customizables;
    results->clear();
    Status s = ListManagedObjects(T::Type(), id, &customizables);
    if (s.ok()) {
      for (const auto& c : customizables) {
        results->push_back(std::static_pointer_cast<T>(c));
      }
    }
    return s;
  }

  template <typename T>
  Status ListManagedObjects(std::vector<std::shared_ptr<T>>* results) const {
    return ListManagedObjects("", results);
  }

  // Creates a new ManagedObject in the registry for the id if one does not
  // currently exist.  If an object with that ID already exists, the current
  // object is returned.
  //
  // The ID is the identifier of the object to be returned/created and returned
  // in result
  // If a new object is created (using the object factories), the cfunc
  // parameter will be invoked to configure the new object.
  template <typename T>
  Status GetOrCreateManagedObject(const std::string& id,
                                  std::shared_ptr<T>* result,
                                  const ConfigureFunc<T>& cfunc = nullptr) {
    if (parent_ != nullptr) {
      auto object = parent_->GetManagedObject(T::Type(), id);
      if (object != nullptr) {
        *result = std::static_pointer_cast<T>(object);
        return Status::OK();
      }
    }
    {
      std::unique_lock<std::mutex> lock(objects_mutex_);
      auto key = ToManagedObjectKey(T::Type(), id);
      auto iter = managed_objects_.find(key);
      if (iter != managed_objects_.end()) {
        auto object = iter->second.lock();
        if (object != nullptr) {
          *result = std::static_pointer_cast<T>(object);
          return Status::OK();
        }
      }
      std::shared_ptr<T> object;
      Status s = NewSharedObject(id, &object);
      if (s.ok() && cfunc != nullptr) {
        s = cfunc(object.get());
      }
      if (s.ok()) {
        auto c = std::static_pointer_cast<Customizable>(object);
        if (id != c->Name()) {
          // If the ID is not the base name of the class, add the new
          // object under the input ID
          managed_objects_[key] = c;
        }
        if (id != c->GetId() && c->GetId() != c->Name()) {
          // If the input and current ID do not match, and the
          // current ID is not the base bame, add the new object under
          // its new ID
          key = ToManagedObjectKey(T::Type(), c->GetId());
          managed_objects_[key] = c;
        }
        *result = object;
      }
      return s;
    }
  }

  // Dump the contents of the registry to the logger
  void Dump(Logger* logger) const;

 private:
  explicit ObjectRegistry(const std::shared_ptr<ObjectLibrary>& library) {
    libraries_.push_back(library);
  }
  static std::string ToManagedObjectKey(const std::string& type,
                                        const std::string& id) {
    return type + "://" + id;
  }

  // Returns the Customizable managed object associated with the key (Type/ID).
  // If not found, nullptr is returned.
  std::shared_ptr<Customizable> GetManagedObject(const std::string& type,
                                                 const std::string& id) const;
  Status ListManagedObjects(
      const std::string& type, const std::string& pattern,
      std::vector<std::shared_ptr<Customizable>>* results) const;
  // Sets the managed object associated with the key (Type/ID) to c.
  // If the named managed object does not exist, the object is added and OK is
  // returned If the object exists and is the same as c, OK is returned
  // Otherwise, an error status is returned.
  Status SetManagedObject(const std::string& type, const std::string& id,
                          const std::shared_ptr<Customizable>& c);

  // Searches (from back to front) the libraries looking for the
  // factory that matches this name.
  // Returns the factory if it is found, and nullptr otherwise
  template <typename T>
  const FactoryFunc<T> FindFactory(const std::string& name) const {
    {
      std::unique_lock<std::mutex> lock(library_mutex_);
      for (auto iter = libraries_.crbegin(); iter != libraries_.crend();
           ++iter) {
        const auto factory = iter->get()->FindFactory<T>(name);
        if (factory != nullptr) {
          return factory;
        }
      }
    }
    if (parent_ != nullptr) {
      return parent_->FindFactory<T>(name);
    } else {
      return nullptr;
    }
  }

  // The set of libraries to search for factories for this registry.
  // The libraries are searched in reverse order (back to front) when
  // searching for entries.
  std::vector<std::shared_ptr<ObjectLibrary>> libraries_;
  std::map<std::string, std::weak_ptr<Customizable>> managed_objects_;
  std::shared_ptr<ObjectRegistry> parent_;
  mutable std::mutex objects_mutex_;  // Mutex for managed objects
  mutable std::mutex library_mutex_;  // Mutex for managed libraries
};
}  // namespace ROCKSDB_NAMESPACE
#endif  // ROCKSDB_LITE
