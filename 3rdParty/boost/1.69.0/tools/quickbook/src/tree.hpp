/*=============================================================================
Copyright (c) 2017 Daniel James

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_QUICKBOOK_TREE_HPP)
#define BOOST_QUICKBOOK_TREE_HPP

#include <utility>

namespace quickbook
{
    namespace detail
    {
        struct tree_node_base;
        template <typename T> struct tree_node;
        struct tree_base;
        template <typename T> struct tree;
        struct tree_builder_base;
        template <typename T> struct tree_builder;

        struct tree_node_base
        {
            friend struct tree_base;
            friend struct tree_builder_base;

          protected:
            tree_node_base* parent_;
            tree_node_base* children_;
            tree_node_base* next_;
            tree_node_base* prev_;

          public:
            tree_node_base() : parent_(), children_(), next_(), prev_() {}

          protected:
            void add_before(tree_base*);
            void add_after(tree_base*);
            void add_first_child(tree_base*);
            void add_last_child(tree_base*);
        };

        template <typename T> struct tree_node : tree_node_base
        {
            T* parent() const { return static_cast<T*>(parent_); }
            T* children() const { return static_cast<T*>(children_); }
            T* next() const { return static_cast<T*>(next_); }
            T* prev() const { return static_cast<T*>(prev_); }

            void add_before(tree<T>&& x) { tree_node_base::add_before(&x); }
            void add_after(tree<T>&& x) { tree_node_base::add_after(&x); }
            void add_first_child(tree<T>&& x)
            {
                tree_node_base::add_first_child(&x);
            }
            void add_last_child(tree<T>&& x)
            {
                tree_node_base::add_last_child(&x);
            }
        };

        template <typename Node> void delete_nodes(Node* n)
        {
            while (n) {
                Node* to_delete = n;
                n = n->next();
                delete_nodes(to_delete->children());
                delete (to_delete);
            }
        }

        struct tree_base
        {
            friend struct tree_node_base;

          private:
            tree_base(tree_base const&);
            tree_base& operator=(tree_base const&);

          protected:
            tree_node_base* root_;

            tree_base();
            explicit tree_base(tree_node_base*);
            tree_base(tree_base&&);
            tree_base& operator=(tree_base&&);
            ~tree_base();

            tree_node_base* extract(tree_node_base*);
        };

        template <typename T> struct tree : tree_base
        {
            tree() {}
            explicit tree(T* r) : tree_base(r) {}
            tree(tree<T>&& x) : tree_base(std::move(x)) {}
            ~tree() { delete_nodes(root()); }
            tree& operator=(tree<T>&& x)
            {
                tree_base::operator=(std::move(x));
                return *this;
            }

            tree extract(T* n)
            {
                return tree(static_cast<T*>(tree_base::extract(n)));
            }
            T* root() const { return static_cast<T*>(root_); }
        };

        struct tree_builder_base : tree_base
        {
          private:
            tree_builder_base(tree_builder_base const&);
            tree_builder_base& operator=(tree_builder_base const&);

          protected:
            tree_node_base* current_;
            tree_node_base* parent_;

            tree_builder_base();
            tree_builder_base(tree_builder_base&&);
            tree_builder_base& operator=(tree_builder_base&& x);
            ~tree_builder_base();

            tree_node_base* extract(tree_node_base*);
            tree_node_base* release();
            void add_element(tree_node_base* n);

          public:
            void start_children();
            void end_children();
        };

        template <typename T> struct tree_builder : tree_builder_base
        {
          public:
            tree_builder() : tree_builder_base() {}
            tree_builder(tree_builder<T>&& x) : tree_builder_base(std::move(x))
            {
            }
            ~tree_builder() { delete_nodes(root()); }
            tree_builder& operator=(tree_builder&& x)
            {
                return tree_builder_base::operator=(std::move(x));
            }

            T* parent() const { return static_cast<T*>(parent_); }
            T* current() const { return static_cast<T*>(current_); }
            T* root() const { return static_cast<T*>(root_); }
            tree<T> extract(T* x)
            {
                return tree<T>(static_cast<T*>(tree_builder_base::extract(x)));
            }
            tree<T> release()
            {
                return tree<T>(static_cast<T*>(tree_builder_base::release()));
            }
            void add_element(T* n) { tree_builder_base::add_element(n); }
        };
    }
}

#endif
