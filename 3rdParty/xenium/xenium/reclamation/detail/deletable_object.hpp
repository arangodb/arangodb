//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_DETAIL_DELETABLE_OBJECT_HPP
#define XENIUM_DETAIL_DELETABLE_OBJECT_HPP

#include <memory>
#include <type_traits>

namespace xenium { namespace reclamation { namespace detail {

  struct deletable_object
  {
    virtual void delete_self() = 0;
    deletable_object* next = nullptr;
  protected:
     virtual ~deletable_object() = default;
  };

  inline void delete_objects(deletable_object*& list)
  {
    auto cur = list;
    for (deletable_object* next = nullptr; cur != nullptr; cur = next)
    {
      next = cur->next;
      cur->delete_self();
    }
    list = nullptr;
  }

  template <class Derived, class DeleterT, class Base>
  struct deletable_object_with_non_empty_deleter : Base
  {
    using Deleter = DeleterT;
    static_assert(std::is_base_of<deletable_object, Base>::value, "Base must derive from deletable_object");
    virtual void delete_self() override
    {
      Deleter& my_deleter = reinterpret_cast<Deleter&>(deleter_buffer);
      Deleter deleter(std::move(my_deleter));
      my_deleter.~Deleter();

      deleter(static_cast<Derived*>(this));
    }

    void set_deleter(Deleter deleter)
    {
      reinterpret_cast<Deleter&>(deleter_buffer) = std::move(deleter);
    }

  private:
    char deleter_buffer[sizeof(Deleter)];
  };

  template <class Derived, class DeleterT, class Base>
  struct deletable_object_with_empty_deleter : Base
  {
    using Deleter = DeleterT;
    virtual void delete_self() override
    {
      static_assert(std::is_default_constructible<Deleter>::value, "empty deleters must be default constructible");
      Deleter deleter{};
      deleter(static_cast<Derived*>(this));
    }

    void set_deleter(Deleter deleter) {}
  };

  template <class Derived, class Deleter = std::default_delete<Derived>, class Base = deletable_object>
  using deletable_object_impl = std::conditional_t<std::is_empty<Deleter>::value,
    deletable_object_with_empty_deleter<Derived, Deleter, Base>,
    deletable_object_with_non_empty_deleter<Derived, Deleter, Base>
  >;
}}}

#endif
