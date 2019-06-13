/*=============================================================================
    Copyright (c) 2010.2017 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_SPIRIT_QUICKBOOK_CLEANUP_HPP)
#define BOOST_SPIRIT_QUICKBOOK_CLEANUP_HPP

namespace quickbook
{
    // This header defines a class which will store pointers and delete what
    // they're pointing to on destruction. Add an object, and you can use
    // pointers and references to it during the cleanup object's lifespan.
    //
    // Example use:
    //
    // struct wonder_struct {
    //     quickbook::cleanup cleanup;
    // };
    //
    // wonder_struct w;
    // thing& t = w.cleanup.add(new thing());
    //
    // Can now use 't' until the wonder_struct is destroyed.
    //
    // Anything added to cleanup is destroyed in reverse order, so it
    // should be okay for an object to depend on something that was previously
    // added.

    namespace detail
    {
        struct cleanup_node;
    }
    struct cleanup
    {
        cleanup() : first_(0) {}
        ~cleanup();
        template <typename T> T& add(T*);

      private:
        detail::cleanup_node* first_;

        cleanup& operator=(cleanup const&);
        cleanup(cleanup const&);
    };

    namespace detail
    {
        template <typename T> void delete_impl(void* ptr)
        {
            delete static_cast<T*>(ptr);
        }

        struct cleanup_node
        {
            void* ptr_;
            void (*del_)(void*);
            cleanup_node* next_;

            cleanup_node() : ptr_(0), del_(0), next_(0) {}
            cleanup_node(void* ptr, void (*del)(void* x))
                : ptr_(ptr), del_(del), next_(0)
            {
            }
            ~cleanup_node()
            {
                if (ptr_) del_(ptr_);
            }

            void move_assign(cleanup_node& n)
            {
                ptr_ = n.ptr_;
                del_ = n.del_;
                n.ptr_ = 0;
                n.del_ = 0;
            }

          private:
            cleanup_node(cleanup_node const&);
            cleanup_node& operator=(cleanup_node const&);
        };
    }

    template <typename T> T& cleanup::add(T* ptr)
    {
        detail::cleanup_node n(ptr, &detail::delete_impl<T>);
        detail::cleanup_node* n2 = new detail::cleanup_node();
        n2->next_ = first_;
        first_ = n2;
        n2->move_assign(n);
        return *ptr;
    }

    inline cleanup::~cleanup()
    {
        while (first_) {
            detail::cleanup_node* to_delete = first_;
            first_ = first_->next_;
            delete to_delete;
        }
    }
}

#endif
