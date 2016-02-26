////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef __ARANGODB_CONSENSUS_STORE__
#define __ARANGODB_CONSENSUS_STORE__

#include <cassert>
#include <memory>
#include <stdexcept>
#include <iterator>
#include <set>
#include <queue>
#include <algorithm>
#include <cstddef>


/**
 * @brief Node
 */
template<class T>
class store_node_ { 
	public:
		store_node_();
		store_node_(const T&);

		store_node_<T> *parent;
	   store_node_<T> *first_child, *last_child;
		store_node_<T> *prev_sibling, *next_sibling;
		T data;
}; 

template<class T>
store_node_<T>::store_node_()
	: parent(0), first_child(0), last_child(0), prev_sibling(0), next_sibling(0)
	{
	}

template<class T>
store_node_<T>::store_node_(const T& val)
	: parent(0), first_child(0), last_child(0), prev_sibling(0), next_sibling(0), data(val)
	{
	}

/**
 * @brief Store
 */
template <class T, class store_node_allocator = std::allocator<store_node_<T> > >
class store {
	protected:
		typedef store_node_<T> store_node;
	public:
		
		typedef T value_type;

		class iterator_base;
		class pre_order_iterator;
		class post_order_iterator;
		class sibling_iterator;
      class leaf_iterator;

		store();                                         // empty constructor
		store(const T&);                                 // constructor setting given element as head
		store(const iterator_base&);
		store(const store<T, store_node_allocator>&);      // copy constructor
		store(store<T, store_node_allocator>&&);           // move constructor
		~store();
		store<T,store_node_allocator>& operator=(const store<T, store_node_allocator>&);   // copy assignment
		store<T,store_node_allocator>& operator=(store<T, store_node_allocator>&&);        // move assignment

		class iterator_base {
			public:
				typedef T                               value_type;
				typedef T*                              pointer;
				typedef T&                              reference;
				typedef size_t                          size_type;
				typedef ptrdiff_t                       difference_type;
				typedef std::bidirectional_iterator_tag iterator_category;

				iterator_base();
				iterator_base(store_node *);

				T&             operator*() const;
				T*             operator->() const;

            /// When called, the next increment/decrement skips children of this node.
				void         skip_children();
				void         skip_children(bool skip);
				/// Number of children of the node pointed to by the iterator.
				unsigned int number_of_children() const;

				sibling_iterator begin() const;
				sibling_iterator end() const;

				store_node *node;
			protected:
				bool skip_current_children_;
		};

		// Depth-first iterator, first accessing the node, then its children.
		class pre_order_iterator : public iterator_base { 
			public:
				pre_order_iterator();
				pre_order_iterator(store_node *);
				pre_order_iterator(const iterator_base&);
				pre_order_iterator(const sibling_iterator&);

				bool    operator==(const pre_order_iterator&) const;
				bool    operator!=(const pre_order_iterator&) const;
				pre_order_iterator&  operator++();
			   pre_order_iterator&  operator--();
				pre_order_iterator   operator++(int);
				pre_order_iterator   operator--(int);
				pre_order_iterator&  operator+=(unsigned int);
				pre_order_iterator&  operator-=(unsigned int);

				pre_order_iterator&  next_skip_children();
		};

		// Depth-first iterator, first accessing the children, then the node itself.
		class post_order_iterator : public iterator_base {
			public:
				post_order_iterator();
				post_order_iterator(store_node *);
				post_order_iterator(const iterator_base&);
				post_order_iterator(const sibling_iterator&);

				bool    operator==(const post_order_iterator&) const;
				bool    operator!=(const post_order_iterator&) const;
				post_order_iterator&  operator++();
			   post_order_iterator&  operator--();
				post_order_iterator   operator++(int);
				post_order_iterator   operator--(int);
				post_order_iterator&  operator+=(unsigned int);
				post_order_iterator&  operator-=(unsigned int);

				// Set iterator to the first child as deep as possible down the store.
				void descend_all();
		};

		// Breadth-first iterator, using a queue
		class breadth_first_queued_iterator : public iterator_base {
			public:
				breadth_first_queued_iterator();
				breadth_first_queued_iterator(store_node *);
				breadth_first_queued_iterator(const iterator_base&);

				bool    operator==(const breadth_first_queued_iterator&) const;
				bool    operator!=(const breadth_first_queued_iterator&) const;
				breadth_first_queued_iterator&  operator++();
				breadth_first_queued_iterator   operator++(int);
				breadth_first_queued_iterator&  operator+=(unsigned int);

			private:
				std::queue<store_node *> traversal_queue;
		};

		// The default iterator types throughout the store class.
		typedef pre_order_iterator            iterator;
		typedef breadth_first_queued_iterator breadth_first_iterator;

		// Iterator which traverses only the nodes at a given depth from the root.
		class fixed_depth_iterator : public iterator_base {
			public:
				fixed_depth_iterator();
				fixed_depth_iterator(store_node *);
				fixed_depth_iterator(const iterator_base&);
				fixed_depth_iterator(const sibling_iterator&);
				fixed_depth_iterator(const fixed_depth_iterator&);

				bool    operator==(const fixed_depth_iterator&) const;
				bool    operator!=(const fixed_depth_iterator&) const;
				fixed_depth_iterator&  operator++();
			   fixed_depth_iterator&  operator--();
				fixed_depth_iterator   operator++(int);
				fixed_depth_iterator   operator--(int);
				fixed_depth_iterator&  operator+=(unsigned int);
				fixed_depth_iterator&  operator-=(unsigned int);

				store_node *top_node;
		};

		// Iterator which traverses only the nodes which are siblings of each other.
		class sibling_iterator : public iterator_base {
			public:
				sibling_iterator();
				sibling_iterator(store_node *);
				sibling_iterator(const sibling_iterator&);
				sibling_iterator(const iterator_base&);

				bool    operator==(const sibling_iterator&) const;
				bool    operator!=(const sibling_iterator&) const;
				sibling_iterator&  operator++();
				sibling_iterator&  operator--();
				sibling_iterator   operator++(int);
				sibling_iterator   operator--(int);
				sibling_iterator&  operator+=(unsigned int);
				sibling_iterator&  operator-=(unsigned int);

				store_node *range_first() const;
				store_node *range_last() const;
				store_node *parent_;
			private:
				void set_parent_();
		};

      // Iterator which traverses only the leaves.
      class leaf_iterator : public iterator_base {
         public:
            leaf_iterator();
            leaf_iterator(store_node *, store_node *top=0);
            leaf_iterator(const sibling_iterator&);
            leaf_iterator(const iterator_base&);

            bool    operator==(const leaf_iterator&) const;
            bool    operator!=(const leaf_iterator&) const;
            leaf_iterator&  operator++();
            leaf_iterator&  operator--();
            leaf_iterator   operator++(int);
            leaf_iterator   operator--(int);
            leaf_iterator&  operator+=(unsigned int);
            leaf_iterator&  operator-=(unsigned int);
			private:
				store_node *top_node;
      };

		inline pre_order_iterator   begin() const;
		inline pre_order_iterator   end() const;
		post_order_iterator  begin_post() const;
		post_order_iterator  end_post() const;
		fixed_depth_iterator begin_fixed(const iterator_base&, unsigned int) const;
		fixed_depth_iterator end_fixed(const iterator_base&, unsigned int) const;
		breadth_first_queued_iterator begin_breadth_first() const;
		breadth_first_queued_iterator end_breadth_first() const;
		sibling_iterator     begin(const iterator_base&) const;
		sibling_iterator     end(const iterator_base&) const;
      leaf_iterator   begin_leaf() const;
      leaf_iterator   end_leaf() const;
      leaf_iterator   begin_leaf(const iterator_base& top) const;
      leaf_iterator   end_leaf(const iterator_base& top) const;

		template<typename	iter> static iter parent(iter);
		template<typename iter> static iter previous_sibling(iter);
		template<typename iter> static iter next_sibling(iter);
		template<typename iter> iter next_at_same_depth(iter) const;

		void     clear();
		template<typename iter> iter erase(iter);
		void     erase_children(const iterator_base&);

		template<typename iter> iter append_child(iter position); 
		template<typename iter> iter prepend_child(iter position); 
		template<typename iter> iter append_child(iter position, const T& x);
		template<typename iter> iter prepend_child(iter position, const T& x);
		template<typename iter> iter append_child(iter position, iter other_position);
		template<typename iter> iter prepend_child(iter position, iter other_position);
		template<typename iter> iter append_children(iter position, sibling_iterator from, sibling_iterator to);
		template<typename iter> iter prepend_children(iter position, sibling_iterator from, sibling_iterator to);

		pre_order_iterator set_head(const T& x);
		template<typename iter> iter insert(iter position, const T& x);
		sibling_iterator insert(sibling_iterator position, const T& x);
		template<typename iter> iter insert_substore(iter position, const iterator_base& substore);
		template<typename iter> iter insert_after(iter position, const T& x);
		template<typename iter> iter insert_substore_after(iter position, const iterator_base& substore);

		template<typename iter> iter replace(iter position, const T& x);
		template<typename iter> iter replace(iter position, const iterator_base& from);
		sibling_iterator replace(sibling_iterator orig_begin, sibling_iterator orig_end, 
										 sibling_iterator new_begin,  sibling_iterator new_end); 

		template<typename iter> iter flatten(iter position);
		template<typename iter> iter reparent(iter position, sibling_iterator begin, sibling_iterator end);
		template<typename iter> iter reparent(iter position, iter from);

		template<typename iter> iter wrap(iter position, const T& x);

		template<typename iter> iter move_after(iter target, iter source);
      template<typename iter> iter move_before(iter target, iter source);
      sibling_iterator move_before(sibling_iterator target, sibling_iterator source);
		template<typename iter> iter move_ontop(iter target, iter source);

		store                         move_out(iterator);
		template<typename iter> iter move_in(iter, store&);
		template<typename iter> iter move_in_below(iter, store&);
		template<typename iter> iter move_in_as_nth_child(iter, size_t, store&);

		void     merge(sibling_iterator, sibling_iterator, sibling_iterator, sibling_iterator, 
							bool duplicate_leaves=false);
		void     sort(sibling_iterator from, sibling_iterator to, bool deep=false);
		template<class StrictWeakOrdering>
		void     sort(sibling_iterator from, sibling_iterator to, StrictWeakOrdering comp, bool deep=false);
		template<typename iter>
		bool     equal(const iter& one, const iter& two, const iter& three) const;
		template<typename iter, class BinaryPredicate>
		bool     equal(const iter& one, const iter& two, const iter& three, BinaryPredicate) const;
		template<typename iter>
		bool     equal_substore(const iter& one, const iter& two) const;
		template<typename iter, class BinaryPredicate>
		bool     equal_substore(const iter& one, const iter& two, BinaryPredicate) const;
		store     substore(sibling_iterator from, sibling_iterator to) const;
		void     substore(store&, sibling_iterator from, sibling_iterator to) const;
		void     swap(sibling_iterator it);
	   void     swap(iterator, iterator);
		
		size_t   size() const;
		size_t   size(const iterator_base&) const;
		bool     empty() const;
		static int depth(const iterator_base&);
		static int depth(const iterator_base&, const iterator_base&);
		int      max_depth() const;
		int      max_depth(const iterator_base&) const;
		static unsigned int number_of_children(const iterator_base&);
		unsigned int number_of_siblings(const iterator_base&) const;
		bool     is_in_substore(const iterator_base& position, const iterator_base& begin, 
									  const iterator_base& end) const;
		bool     is_valid(const iterator_base&) const;
		iterator lowest_common_ancestor(const iterator_base&, const iterator_base &) const;

		unsigned int index(sibling_iterator it) const;
		static sibling_iterator child(const iterator_base& position, unsigned int);
		sibling_iterator sibling(const iterator_base& position, unsigned int);  				
		
		void debug_verify_consistency() const;
		
		class iterator_base_less {
			public:
				bool operator()(const typename store<T, store_node_allocator>::iterator_base& one,
									 const typename store<T, store_node_allocator>::iterator_base& two) const
					{
					return one.node < two.node;
					}
		};
		store_node *head, *feet; 
	private:
		store_node_allocator alloc_;
		void head_initialise_();
		void copy_(const store<T, store_node_allocator>& other);

		template<class StrictWeakOrdering>
		class compare_nodes {
			public:
				compare_nodes(StrictWeakOrdering comp) : comp_(comp) {};
				
				bool operator()(const store_node *a, const store_node *b) 
					{
					return comp_(a->data, b->data);
					}
			private:
				StrictWeakOrdering comp_;
		};
};


template <class T, class store_node_allocator>
store<T, store_node_allocator>::store() 
	{
	head_initialise_();
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::store(const T& x) 
	{
	head_initialise_();
	set_head(x);
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::store(store<T, store_node_allocator>&& x) 
	{
	head_initialise_();
	head->next_sibling=x.head->next_sibling;
	feet->prev_sibling=x.head->prev_sibling;
	x.head->next_sibling->prev_sibling=head;
	x.feet->prev_sibling->next_sibling=feet;
	x.head->next_sibling=x.feet;
	x.feet->prev_sibling=x.head;
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::store(const iterator_base& other)
	{
	head_initialise_();
	set_head((*other));
	replace(begin(), other);
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::~store()
	{
	clear();
	alloc_.destroy(head);
	alloc_.destroy(feet);
	alloc_.deallocate(head,1);
	alloc_.deallocate(feet,1);
	}

template <class T, class store_node_allocator>
void store<T, store_node_allocator>::head_initialise_() 
   { 
   head = alloc_.allocate(1,0); 
	feet = alloc_.allocate(1,0);
	alloc_.construct(head, store_node_<T>());
	alloc_.construct(feet, store_node_<T>());

   head->parent=0;
   head->first_child=0;
   head->last_child=0;
   head->prev_sibling=0; 
   head->next_sibling=feet;

	feet->parent=0;
	feet->first_child=0;
	feet->last_child=0;
	feet->prev_sibling=head;
	feet->next_sibling=0;
   }

template <class T, class store_node_allocator>
store<T,store_node_allocator>& store<T, store_node_allocator>::operator=(const store<T, store_node_allocator>& other)
	{
	if(this != &other)
		copy_(other);
	return *this;
	}

template <class T, class store_node_allocator>
store<T,store_node_allocator>& store<T, store_node_allocator>::operator=(store<T, store_node_allocator>&& x)
	{
	if(this != &x) {
		head->next_sibling=x.head->next_sibling;
		feet->prev_sibling=x.head->prev_sibling;
		x.head->next_sibling->prev_sibling=head;
		x.feet->prev_sibling->next_sibling=feet;
		x.head->next_sibling=x.feet;
		x.feet->prev_sibling=x.head;
		}
	return *this;
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::store(const store<T, store_node_allocator>& other)
	{
	head_initialise_();
	copy_(other);
	}

template <class T, class store_node_allocator>
void store<T, store_node_allocator>::copy_(const store<T, store_node_allocator>& other) 
	{
	clear();
	pre_order_iterator it=other.begin(), to=begin();
	while(it!=other.end()) {
		to=insert(to, (*it));
		it.skip_children();
		++it;
		}
	to=begin();
	it=other.begin();
	while(it!=other.end()) {
		to=replace(to, it);
		to.skip_children();
		it.skip_children();
		++to;
		++it;
		}
	}

template <class T, class store_node_allocator>
void store<T, store_node_allocator>::clear()
	{
	if(head)
		while(head->next_sibling!=feet)
			erase(pre_order_iterator(head->next_sibling));
	}

template<class T, class store_node_allocator> 
void store<T, store_node_allocator>::erase_children(const iterator_base& it)
	{
	if(it.node==0) return;

	store_node *cur=it.node->first_child;
	store_node *prev=0;

	while(cur!=0) {
		prev=cur;
		cur=cur->next_sibling;
		erase_children(pre_order_iterator(prev));
		alloc_.destroy(prev);
		alloc_.deallocate(prev,1);
		}
	it.node->first_child=0;
	it.node->last_child=0;
	}

template<class T, class store_node_allocator> 
template<class iter>
iter store<T, store_node_allocator>::erase(iter it)
	{
	store_node *cur=it.node;
	assert(cur!=head);
	iter ret=it;
	ret.skip_children();
	++ret;
	erase_children(it);
	if(cur->prev_sibling==0) {
		cur->parent->first_child=cur->next_sibling;
		}
	else {
		cur->prev_sibling->next_sibling=cur->next_sibling;
		}
	if(cur->next_sibling==0) {
		cur->parent->last_child=cur->prev_sibling;
		}
	else {
		cur->next_sibling->prev_sibling=cur->prev_sibling;
		}

	alloc_.destroy(cur);
   alloc_.deallocate(cur,1);
	return ret;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::pre_order_iterator store<T, store_node_allocator>::begin() const
	{
	return pre_order_iterator(head->next_sibling);
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::pre_order_iterator store<T, store_node_allocator>::end() const
	{
	return pre_order_iterator(feet);
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::breadth_first_queued_iterator store<T, store_node_allocator>::begin_breadth_first() const
	{
	return breadth_first_queued_iterator(head->next_sibling);
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::breadth_first_queued_iterator store<T, store_node_allocator>::end_breadth_first() const
	{
	return breadth_first_queued_iterator();
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::post_order_iterator store<T, store_node_allocator>::begin_post() const
	{
	store_node *tmp=head->next_sibling;
	if(tmp!=feet) {
		while(tmp->first_child)
			tmp=tmp->first_child;
		}
	return post_order_iterator(tmp);
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::post_order_iterator store<T, store_node_allocator>::end_post() const
	{
	return post_order_iterator(feet);
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::fixed_depth_iterator store<T, store_node_allocator>::begin_fixed(const iterator_base& pos, unsigned int dp) const
	{
	typename store<T, store_node_allocator>::fixed_depth_iterator ret;
	ret.top_node=pos.node;

	store_node *tmp=pos.node;
	unsigned int curdepth=0;
	while(curdepth<dp) { 
		while(tmp->first_child==0) {
			if(tmp->next_sibling==0) {
				
				do {
					if(tmp==ret.top_node)
					   throw std::range_error("store: begin_fixed out of range");
					tmp=tmp->parent;
               if(tmp==0) 
					   throw std::range_error("store: begin_fixed out of range");
               --curdepth;
				   } while(tmp->next_sibling==0);
				}
			tmp=tmp->next_sibling;
			}
		tmp=tmp->first_child;
		++curdepth;
		}

	ret.node=tmp;
	return ret;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::fixed_depth_iterator store<T, store_node_allocator>::end_fixed(const iterator_base& pos, unsigned int dp) const
	{
	assert(1==0); 
	store_node *tmp=pos.node;
	unsigned int curdepth=1;
	while(curdepth<dp) { 
		while(tmp->first_child==0) {
			tmp=tmp->next_sibling;
			if(tmp==0)
				throw std::range_error("store: end_fixed out of range");
			}
		tmp=tmp->first_child;
		++curdepth;
		}
	return tmp;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator store<T, store_node_allocator>::begin(const iterator_base& pos) const
	{
	assert(pos.node!=0);
	if(pos.node->first_child==0) {
		return end(pos);
		}
	return pos.node->first_child;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator store<T, store_node_allocator>::end(const iterator_base& pos) const
	{
	sibling_iterator ret(0);
	ret.parent_=pos.node;
	return ret;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::leaf_iterator store<T, store_node_allocator>::begin_leaf() const
   {
   store_node *tmp=head->next_sibling;
   if(tmp!=feet) {
      while(tmp->first_child)
         tmp=tmp->first_child;
      }
   return leaf_iterator(tmp);
   }

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::leaf_iterator store<T, store_node_allocator>::end_leaf() const
   {
   return leaf_iterator(feet);
   }

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::leaf_iterator store<T, store_node_allocator>::begin_leaf(const iterator_base& top) const
   {
	store_node *tmp=top.node;
	while(tmp->first_child)
		 tmp=tmp->first_child;
   return leaf_iterator(tmp, top.node);
   }

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::leaf_iterator store<T, store_node_allocator>::end_leaf(const iterator_base& top) const
   {
   return leaf_iterator(top.node, top.node);
   }

template <class T, class store_node_allocator>
template <typename iter>
iter store<T, store_node_allocator>::parent(iter position) 
	{
	assert(position.node!=0);
	return iter(position.node->parent);
	}

template <class T, class store_node_allocator>
template <typename iter>
iter store<T, store_node_allocator>::previous_sibling(iter position) 
	{
	assert(position.node!=0);
	iter ret(position);
	ret.node=position.node->prev_sibling;
	return ret;
	}

template <class T, class store_node_allocator>
template <typename iter>
iter store<T, store_node_allocator>::next_sibling(iter position) 
	{
	assert(position.node!=0);
	iter ret(position);
	ret.node=position.node->next_sibling;
	return ret;
	}

template <class T, class store_node_allocator>
template <typename iter>
iter store<T, store_node_allocator>::next_at_same_depth(iter position) const
	{
	// We make use of a temporary fixed_depth iterator to implement this.

	typename store<T, store_node_allocator>::fixed_depth_iterator tmp(position.node);

	++tmp;
	return iter(tmp);

	}

template <class T, class store_node_allocator>
template <typename iter>
iter store<T, store_node_allocator>::append_child(iter position)
 	{
	assert(position.node!=head);
	assert(position.node!=feet);
	assert(position.node);

	store_node *tmp=alloc_.allocate(1,0);
	alloc_.construct(tmp, store_node_<T>());
	tmp->first_child=0;
	tmp->last_child=0;

	tmp->parent=position.node;
	if(position.node->last_child!=0) {
		position.node->last_child->next_sibling=tmp;
		}
	else {
		position.node->first_child=tmp;
		}
	tmp->prev_sibling=position.node->last_child;
	position.node->last_child=tmp;
	tmp->next_sibling=0;
	return tmp;
 	}

template <class T, class store_node_allocator>
template <typename iter>
iter store<T, store_node_allocator>::prepend_child(iter position)
 	{
	assert(position.node!=head);
	assert(position.node!=feet);
	assert(position.node);

	store_node *tmp=alloc_.allocate(1,0);
	alloc_.construct(tmp, store_node_<T>());
	tmp->first_child=0;
	tmp->last_child=0;

	tmp->parent=position.node;
	if(position.node->first_child!=0) {
		position.node->first_child->prev_sibling=tmp;
		}
	else {
		position.node->last_child=tmp;
		}
	tmp->next_sibling=position.node->first_child;
	position.node->prev_child=tmp;
	tmp->prev_sibling=0;
	return tmp;
 	}

template <class T, class store_node_allocator>
template <class iter>
iter store<T, store_node_allocator>::append_child(iter position, const T& x)
	{
	assert(position.node!=head);
	assert(position.node!=feet);
	assert(position.node);

	store_node* tmp = alloc_.allocate(1,0);
	alloc_.construct(tmp, x);
	tmp->first_child=0;
	tmp->last_child=0;

	tmp->parent=position.node;
	if(position.node->last_child!=0) {
		position.node->last_child->next_sibling=tmp;
		}
	else {
		position.node->first_child=tmp;
		}
	tmp->prev_sibling=position.node->last_child;
	position.node->last_child=tmp;
	tmp->next_sibling=0;
	return tmp;
	}

template <class T, class store_node_allocator>
template <class iter>
iter store<T, store_node_allocator>::prepend_child(iter position, const T& x)
	{
	assert(position.node!=head);
	assert(position.node!=feet);
	assert(position.node);

	store_node* tmp = alloc_.allocate(1,0);
	alloc_.construct(tmp, x);
	tmp->first_child=0;
	tmp->last_child=0;

	tmp->parent=position.node;
	if(position.node->first_child!=0) {
		position.node->first_child->prev_sibling=tmp;
		}
	else {
		position.node->last_child=tmp;
		}
	tmp->next_sibling=position.node->first_child;
	position.node->first_child=tmp;
	tmp->prev_sibling=0;
	return tmp;
	}

template <class T, class store_node_allocator>
template <class iter>
iter store<T, store_node_allocator>::append_child(iter position, iter other)
	{
	assert(position.node!=head);
	assert(position.node!=feet);
	assert(position.node);

	sibling_iterator aargh=append_child(position, value_type());
	return replace(aargh, other);
	}

template <class T, class store_node_allocator>
template <class iter>
iter store<T, store_node_allocator>::prepend_child(iter position, iter other)
	{
	assert(position.node!=head);
	assert(position.node!=feet);
	assert(position.node);

	sibling_iterator aargh=prepend_child(position, value_type());
	return replace(aargh, other);
	}

template <class T, class store_node_allocator>
template <class iter>
iter store<T, store_node_allocator>::append_children(iter position, sibling_iterator from, sibling_iterator to)
	{
	assert(position.node!=head);
	assert(position.node!=feet);
	assert(position.node);

	iter ret=from;

	while(from!=to) {
		insert_substore(position.end(), from);
		++from;
		}
	return ret;
	}

template <class T, class store_node_allocator>
template <class iter>
iter store<T, store_node_allocator>::prepend_children(iter position, sibling_iterator from, sibling_iterator to)
	{
	assert(position.node!=head);
	assert(position.node!=feet);
	assert(position.node);

	iter ret=from;

	while(from!=to) {
		insert_substore(position.begin(), from);
		++from;
		}
	return ret;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::pre_order_iterator store<T, store_node_allocator>::set_head(const T& x)
	{
	assert(head->next_sibling==feet);
	return insert(iterator(feet), x);
	}

template <class T, class store_node_allocator>
template <class iter>
iter store<T, store_node_allocator>::insert(iter position, const T& x)
	{
	if(position.node==0) {
		position.node=feet;
		                   
		}
	store_node* tmp = alloc_.allocate(1,0);
	alloc_.construct(tmp, x);

	tmp->first_child=0;
	tmp->last_child=0;

	tmp->parent=position.node->parent;
	tmp->next_sibling=position.node;
	tmp->prev_sibling=position.node->prev_sibling;
	position.node->prev_sibling=tmp;

	if(tmp->prev_sibling==0) {
		if(tmp->parent) 
			tmp->parent->first_child=tmp;
		}
	else
		tmp->prev_sibling->next_sibling=tmp;
	return tmp;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator store<T, store_node_allocator>::insert(sibling_iterator position, const T& x)
	{
	store_node* tmp = alloc_.allocate(1,0);
	alloc_.construct(tmp, x);

	tmp->first_child=0;
	tmp->last_child=0;

	tmp->next_sibling=position.node;
	if(position.node==0) { 
		tmp->parent=position.parent_;
		tmp->prev_sibling=position.range_last();
		tmp->parent->last_child=tmp;
		}
	else {
		tmp->parent=position.node->parent;
		tmp->prev_sibling=position.node->prev_sibling;
		position.node->prev_sibling=tmp;
		}

	if(tmp->prev_sibling==0) {
		if(tmp->parent) 
			tmp->parent->first_child=tmp;
		}
	else
		tmp->prev_sibling->next_sibling=tmp;
	return tmp;
	}

template <class T, class store_node_allocator>
template <class iter>
iter store<T, store_node_allocator>::insert_after(iter position, const T& x)
	{
	store_node* tmp = alloc_.allocate(1,0);
	alloc_.construct(tmp, x);
	tmp->first_child=0;
	tmp->last_child=0;

	tmp->parent=position.node->parent;
	tmp->prev_sibling=position.node;
	tmp->next_sibling=position.node->next_sibling;
	position.node->next_sibling=tmp;

	if(tmp->next_sibling==0) {
		if(tmp->parent) 
			tmp->parent->last_child=tmp;
		}
	else {
		tmp->next_sibling->prev_sibling=tmp;
		}
	return tmp;
	}

template <class T, class store_node_allocator>
template <class iter>
iter store<T, store_node_allocator>::insert_substore(iter position, const iterator_base& substore)
	{
	iter it=insert(position, value_type());
	return replace(it, substore);
	}

template <class T, class store_node_allocator>
template <class iter>
iter store<T, store_node_allocator>::insert_substore_after(iter position, const iterator_base& substore)
	{
	iter it=insert_after(position, value_type());
	return replace(it, substore);
	}


template <class T, class store_node_allocator>
template <class iter>
iter store<T, store_node_allocator>::replace(iter position, const T& x)
	{
	position.node->data=x;
	return position;
	}

template <class T, class store_node_allocator>
template <class iter>
iter store<T, store_node_allocator>::replace(iter position, const iterator_base& from)
	{
	assert(position.node!=head);
	store_node *current_from=from.node;
	store_node *start_from=from.node;
	store_node *current_to  =position.node;

	erase_children(position);	
	store_node* tmp = alloc_.allocate(1,0);
	alloc_.construct(tmp, (*from));
	tmp->first_child=0;
	tmp->last_child=0;
	if(current_to->prev_sibling==0) {
		if(current_to->parent!=0)
			current_to->parent->first_child=tmp;
		}
	else {
		current_to->prev_sibling->next_sibling=tmp;
		}
	tmp->prev_sibling=current_to->prev_sibling;
	if(current_to->next_sibling==0) {
		if(current_to->parent!=0)
			current_to->parent->last_child=tmp;
		}
	else {
		current_to->next_sibling->prev_sibling=tmp;
		}
	tmp->next_sibling=current_to->next_sibling;
	tmp->parent=current_to->parent;
	alloc_.destroy(current_to);
	alloc_.deallocate(current_to,1);
	current_to=tmp;
	
	store_node *last=from.node->next_sibling;

	pre_order_iterator toit=tmp;
	do {
		assert(current_from!=0);
		if(current_from->first_child != 0) {
			current_from=current_from->first_child;
			toit=append_child(toit, current_from->data);
			}
		else {
			while(current_from->next_sibling==0 && current_from!=start_from) {
				current_from=current_from->parent;
				toit=parent(toit);
				assert(current_from!=0);
				}
			current_from=current_from->next_sibling;
			if(current_from!=last) {
				toit=append_child(parent(toit), current_from->data);
				}
			}
		} while(current_from!=last);

	return current_to;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator store<T, store_node_allocator>::replace(
	sibling_iterator orig_begin, 
	sibling_iterator orig_end, 
	sibling_iterator new_begin, 
	sibling_iterator new_end)
	{
	store_node *orig_first=orig_begin.node;
	store_node *new_first=new_begin.node;
	store_node *orig_last=orig_first;
	while((++orig_begin)!=orig_end)
		orig_last=orig_last->next_sibling;
	store_node *new_last=new_first;
	while((++new_begin)!=new_end)
		new_last=new_last->next_sibling;

	bool first=true;
	pre_order_iterator ret;
	while(1==1) {
		pre_order_iterator tt=insert_substore(pre_order_iterator(orig_first), pre_order_iterator(new_first));
		if(first) {
			ret=tt;
			first=false;
			}
		if(new_first==new_last)
			break;
		new_first=new_first->next_sibling;
		}

	bool last=false;
	store_node *next=orig_first;
	while(1==1) {
		if(next==orig_last) 
			last=true;
		next=next->next_sibling;
		erase((pre_order_iterator)orig_first);
		if(last) 
			break;
		orig_first=next;
		}
	return ret;
	}

template <class T, class store_node_allocator>
template <typename iter>
iter store<T, store_node_allocator>::flatten(iter position)
	{
	if(position.node->first_child==0)
		return position;

	store_node *tmp=position.node->first_child;
	while(tmp) {
		tmp->parent=position.node->parent;
		tmp=tmp->next_sibling;
		} 
	if(position.node->next_sibling) {
		position.node->last_child->next_sibling=position.node->next_sibling;
		position.node->next_sibling->prev_sibling=position.node->last_child;
		}
	else {
		position.node->parent->last_child=position.node->last_child;
		}
	position.node->next_sibling=position.node->first_child;
	position.node->next_sibling->prev_sibling=position.node;
	position.node->first_child=0;
	position.node->last_child=0;

	return position;
	}


template <class T, class store_node_allocator>
template <typename iter>
iter store<T, store_node_allocator>::reparent(iter position, sibling_iterator begin, sibling_iterator end)
	{
	store_node *first=begin.node;
	store_node *last=first;

	assert(first!=position.node);
	
	if(begin==end) return begin;
	while((++begin)!=end) {
		last=last->next_sibling;
		}
	if(first->prev_sibling==0) {
		first->parent->first_child=last->next_sibling;
		}
	else {
		first->prev_sibling->next_sibling=last->next_sibling;
		}
	if(last->next_sibling==0) {
		last->parent->last_child=first->prev_sibling;
		}
	else {
		last->next_sibling->prev_sibling=first->prev_sibling;
		}
	if(position.node->first_child==0) {
		position.node->first_child=first;
		position.node->last_child=last;
		first->prev_sibling=0;
		}
	else {
		position.node->last_child->next_sibling=first;
		first->prev_sibling=position.node->last_child;
		position.node->last_child=last;
		}
	last->next_sibling=0;

	store_node *pos=first;
   for(;;) {
		pos->parent=position.node;
		if(pos==last) break;
		pos=pos->next_sibling;
		}

	return first;
	}

template <class T, class store_node_allocator>
template <typename iter> iter store<T, store_node_allocator>::reparent(iter position, iter from)
	{
	if(from.node->first_child==0) return position;
	return reparent(position, from.node->first_child, end(from));
	}

template <class T, class store_node_allocator>
template <typename iter> iter store<T, store_node_allocator>::wrap(iter position, const T& x)
	{
	assert(position.node!=0);
	sibling_iterator fr=position, to=position;
	++to;
	iter ret = insert(position, x);
	reparent(ret, fr, to);
	return ret;
	}

template <class T, class store_node_allocator>
template <typename iter> iter store<T, store_node_allocator>::move_after(iter target, iter source)
   {
   store_node *dst=target.node;
   store_node *src=source.node;
   assert(dst);
   assert(src);

   if(dst==src) return source;
	if(dst->next_sibling)
		if(dst->next_sibling==src) 
			return source;

   if(src->prev_sibling!=0) src->prev_sibling->next_sibling=src->next_sibling;
   else                     src->parent->first_child=src->next_sibling;
   if(src->next_sibling!=0) src->next_sibling->prev_sibling=src->prev_sibling;
   else                     src->parent->last_child=src->prev_sibling;

   if(dst->next_sibling!=0) dst->next_sibling->prev_sibling=src;
   else                     dst->parent->last_child=src;
   src->next_sibling=dst->next_sibling;
   dst->next_sibling=src;
   src->prev_sibling=dst;
   src->parent=dst->parent;
   return src;
   }

template <class T, class store_node_allocator>
template <typename iter> iter store<T, store_node_allocator>::move_before(iter target, iter source)
   {
   store_node *dst=target.node;
   store_node *src=source.node;
   assert(dst);
   assert(src);

   if(dst==src) return source;
	if(dst->prev_sibling)
		if(dst->prev_sibling==src) 
			return source;

   if(src->prev_sibling!=0) src->prev_sibling->next_sibling=src->next_sibling;
   else                     src->parent->first_child=src->next_sibling;
   if(src->next_sibling!=0) src->next_sibling->prev_sibling=src->prev_sibling;
   else                     src->parent->last_child=src->prev_sibling;

   if(dst->prev_sibling!=0) dst->prev_sibling->next_sibling=src;
   else                     dst->parent->first_child=src;
   src->prev_sibling=dst->prev_sibling;
   dst->prev_sibling=src;
   src->next_sibling=dst;
   src->parent=dst->parent;
   return src;
   }

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator store<T, store_node_allocator>::move_before(sibling_iterator target, 
																													  sibling_iterator source)
	{
	store_node *dst=target.node;
	store_node *src=source.node;
	store_node *dst_prev_sibling;
	if(dst==0) { 
		dst_prev_sibling=target.parent_->last_child;
		assert(dst_prev_sibling);
		}
	else dst_prev_sibling=dst->prev_sibling;
	assert(src);

	if(dst==src) return source;
	if(dst_prev_sibling)
		if(dst_prev_sibling==src) 
			return source;

	if(src->prev_sibling!=0) src->prev_sibling->next_sibling=src->next_sibling;
	else                     src->parent->first_child=src->next_sibling;
	if(src->next_sibling!=0) src->next_sibling->prev_sibling=src->prev_sibling;
	else                     src->parent->last_child=src->prev_sibling;

	if(dst_prev_sibling!=0) dst_prev_sibling->next_sibling=src;
	else                    target.parent_->first_child=src;
	src->prev_sibling=dst_prev_sibling;
	if(dst) {
		dst->prev_sibling=src;
		src->parent=dst->parent;
		}
	src->next_sibling=dst;
	return src;
	}

template <class T, class store_node_allocator>
template <typename iter> iter store<T, store_node_allocator>::move_ontop(iter target, iter source)
	{
	store_node *dst=target.node;
	store_node *src=source.node;
	assert(dst);
	assert(src);

	if(dst==src) return source;

	store_node *b_prev_sibling=dst->prev_sibling;
	store_node *b_next_sibling=dst->next_sibling;
	store_node *b_parent=dst->parent;

	erase(target);

	if(src->prev_sibling!=0) src->prev_sibling->next_sibling=src->next_sibling;
	else                     src->parent->first_child=src->next_sibling;
	if(src->next_sibling!=0) src->next_sibling->prev_sibling=src->prev_sibling;
	else                     src->parent->last_child=src->prev_sibling;

	if(b_prev_sibling!=0) b_prev_sibling->next_sibling=src;
	else                  b_parent->first_child=src;
	if(b_next_sibling!=0) b_next_sibling->prev_sibling=src;
	else                  b_parent->last_child=src;
	src->prev_sibling=b_prev_sibling;
	src->next_sibling=b_next_sibling;
	src->parent=b_parent;
	return src;
	}


template <class T, class store_node_allocator>
store<T, store_node_allocator> store<T, store_node_allocator>::move_out(iterator source)
	{
	store ret;

	ret.head->next_sibling = source.node;
	ret.feet->prev_sibling = source.node;
	source.node->parent=0;

	if(source.node->prev_sibling!=0) 
		source.node->prev_sibling->next_sibling = source.node->next_sibling;

	if(source.node->next_sibling!=0) 
		source.node->next_sibling->prev_sibling = source.node->prev_sibling;

	source.node->prev_sibling = ret.head;
	source.node->next_sibling = ret.feet;

	return ret; 
	}

template <class T, class store_node_allocator>
template<typename iter> iter store<T, store_node_allocator>::move_in(iter loc, store& other)
	{
	if(other.head->next_sibling==other.feet) return loc; 

	store_node *other_first_head = other.head->next_sibling;
	store_node *other_last_head  = other.feet->prev_sibling;

	sibling_iterator prev(loc);
	--prev;
	
	prev.node->next_sibling = other_first_head;
	loc.node->prev_sibling  = other_last_head;
	other_first_head->prev_sibling = prev.node;
	other_last_head->next_sibling  = loc.node;

	store_node *walk=other_first_head;
	while(true) {
		walk->parent=loc.node->parent;
		if(walk==other_last_head)
			break;
		walk=walk->next_sibling;
		}

	// Close other store.
	other.head->next_sibling=other.feet;
	other.feet->prev_sibling=other.head;

	return other_first_head;
	}

template <class T, class store_node_allocator>
template<typename iter> iter store<T, store_node_allocator>::move_in_as_nth_child(iter loc, size_t n, store& other)
	{
	if(other.head->next_sibling==other.feet) return loc; // other store is empty

	store_node *other_first_head = other.head->next_sibling;
	store_node *other_last_head  = other.feet->prev_sibling;

	if(n==0) {
		if(loc.node->first_child==0) {
			loc.node->first_child=other_first_head;
			loc.node->last_child=other_last_head;
			other_last_head->next_sibling=0;
			other_first_head->prev_sibling=0;
			} 
		else {
			loc.node->first_child->prev_sibling=other_last_head;
			other_last_head->next_sibling=loc.node->first_child;
			loc.node->first_child=other_first_head;
			other_first_head->prev_sibling=0;
			}
		}
	else {
		--n;
		store_node *walk = loc.node->first_child;
		while(true) {
			if(walk==0)
				throw std::range_error("store: move_in_as_nth_child position "
											  +std::to_string(n+1)
											  +" out of range; only "
											  +std::to_string(number_of_children(loc))
											  +" child nodes present");
			if(n==0) 
				break;
			--n;
			walk = walk->next_sibling;
			}
		if(walk->next_sibling==0) 
			loc.node->last_child=other_last_head;
		else 
			walk->next_sibling->prev_sibling=other_last_head;
		other_last_head->next_sibling=walk->next_sibling;
		walk->next_sibling=other_first_head;
		other_first_head->prev_sibling=walk;
		}

	// Adjust parent pointers.
	store_node *walk=other_first_head;
	while(true) {
		walk->parent=loc.node;
		if(walk==other_last_head)
			break;
		walk=walk->next_sibling;
		}

	// Close other store.
	other.head->next_sibling=other.feet;
	other.feet->prev_sibling=other.head;

	return other_first_head;
	}


template <class T, class store_node_allocator>
void store<T, store_node_allocator>::merge(sibling_iterator to1,   sibling_iterator to2,
														sibling_iterator from1, sibling_iterator from2,
														bool duplicate_leaves)
	{
	sibling_iterator fnd;
	while(from1!=from2) {
		if((fnd=std::find(to1, to2, (*from1))) != to2) { // element found
			if(from1.begin()==from1.end()) { // full depth reached
				if(duplicate_leaves)
					append_child(parent(to1), (*from1));
				}
			else { // descend further
				merge(fnd.begin(), fnd.end(), from1.begin(), from1.end(), duplicate_leaves);
				}
			}
		else { // element missing
			insert_substore(to2, from1);
			}
		++from1;
		}
	}


template <class T, class store_node_allocator>
void store<T, store_node_allocator>::sort(sibling_iterator from, sibling_iterator to, bool deep)
	{
	std::less<T> comp;
	sort(from, to, comp, deep);
	}

template <class T, class store_node_allocator>
template <class StrictWeakOrdering>
void store<T, store_node_allocator>::sort(sibling_iterator from, sibling_iterator to, 
													 StrictWeakOrdering comp, bool deep)
	{
	if(from==to) return;

	std::multiset<store_node *, compare_nodes<StrictWeakOrdering> > nodes(comp);
	sibling_iterator it=from, it2=to;
	while(it != to) {
		nodes.insert(it.node);
		++it;
		}
	// reassemble
	--it2;

	// prev and next are the nodes before and after the sorted range
	store_node *prev=from.node->prev_sibling;
	store_node *next=it2.node->next_sibling;
	typename std::multiset<store_node *, compare_nodes<StrictWeakOrdering> >::iterator nit=nodes.begin(), eit=nodes.end();
	if(prev==0) {
		if((*nit)->parent!=0) // to catch "sorting the head" situations, when there is no parent
			(*nit)->parent->first_child=(*nit);
		}
	else prev->next_sibling=(*nit);

	--eit;
	while(nit!=eit) {
		(*nit)->prev_sibling=prev;
		if(prev)
			prev->next_sibling=(*nit);
		prev=(*nit);
		++nit;
		}
	if(prev)
		prev->next_sibling=(*eit);

	(*eit)->next_sibling=next;
   (*eit)->prev_sibling=prev; // missed in the loop above
	if(next==0) {
		if((*eit)->parent!=0)
			(*eit)->parent->last_child=(*eit);
		}
	else next->prev_sibling=(*eit);

	if(deep) {
		sibling_iterator bcs(*nodes.begin());
		sibling_iterator ecs(*eit);
		++ecs;
		while(bcs!=ecs) {
			sort(begin(bcs), end(bcs), comp, deep);
			++bcs;
			}
		}
	}

template <class T, class store_node_allocator>
template <typename iter>
bool store<T, store_node_allocator>::equal(const iter& one_, const iter& two, const iter& three_) const
	{
	std::equal_to<T> comp;
	return equal(one_, two, three_, comp);
	}

template <class T, class store_node_allocator>
template <typename iter>
bool store<T, store_node_allocator>::equal_substore(const iter& one_, const iter& two_) const
	{
	std::equal_to<T> comp;
	return equal_substore(one_, two_, comp);
	}

template <class T, class store_node_allocator>
template <typename iter, class BinaryPredicate>
bool store<T, store_node_allocator>::equal(const iter& one_, const iter& two, const iter& three_, BinaryPredicate fun) const
	{
	pre_order_iterator one(one_), three(three_);

	while(one!=two && is_valid(three)) {
		if(!fun(*one,*three))
			return false;
		if(one.number_of_children()!=three.number_of_children()) 
			return false;
		++one;
		++three;
		}
	return true;
	}

template <class T, class store_node_allocator>
template <typename iter, class BinaryPredicate>
bool store<T, store_node_allocator>::equal_substore(const iter& one_, const iter& two_, BinaryPredicate fun) const
	{
	pre_order_iterator one(one_), two(two_);

	if(!fun(*one,*two)) return false;
	if(number_of_children(one)!=number_of_children(two)) return false;
	return equal(begin(one),end(one),begin(two),fun);
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator> store<T, store_node_allocator>::substore(sibling_iterator from, sibling_iterator to) const
	{
	assert(from!=to);

	store tmp;
	tmp.set_head(value_type());
	tmp.replace(tmp.begin(), tmp.end(), from, to);
	return tmp;
	}

template <class T, class store_node_allocator>
void store<T, store_node_allocator>::substore(store& tmp, sibling_iterator from, sibling_iterator to) const
	{
	assert(from!=to); // if from==to, the range is empty, hence no store to return.

	tmp.set_head(value_type());
	tmp.replace(tmp.begin(), tmp.end(), from, to);
	}

template <class T, class store_node_allocator>
size_t store<T, store_node_allocator>::size() const
	{
	size_t i=0;
	pre_order_iterator it=begin(), eit=end();
	while(it!=eit) {
		++i;
		++it;
		}
	return i;
	}

template <class T, class store_node_allocator>
size_t store<T, store_node_allocator>::size(const iterator_base& top) const
	{
	size_t i=0;
	pre_order_iterator it=top, eit=top;
	eit.skip_children();
	++eit;
	while(it!=eit) {
		++i;
		++it;
		}
	return i;
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::empty() const
	{
	pre_order_iterator it=begin(), eit=end();
	return (it==eit);
	}

template <class T, class store_node_allocator>
int store<T, store_node_allocator>::depth(const iterator_base& it) 
	{
	store_node* pos=it.node;
	assert(pos!=0);
	int ret=0;
	while(pos->parent!=0) {
		pos=pos->parent;
		++ret;
		}
	return ret;
	}

template <class T, class store_node_allocator>
int store<T, store_node_allocator>::depth(const iterator_base& it, const iterator_base& root) 
	{
	store_node* pos=it.node;
	assert(pos!=0);
	int ret=0;
	while(pos->parent!=0 && pos!=root.node) {
		pos=pos->parent;
		++ret;
		}
	return ret;
	}

template <class T, class store_node_allocator>
int store<T, store_node_allocator>::max_depth() const
	{
	int maxd=-1;
	for(store_node *it = head->next_sibling; it!=feet; it=it->next_sibling)
		maxd=std::max(maxd, max_depth(it));

	return maxd;
	}


template <class T, class store_node_allocator>
int store<T, store_node_allocator>::max_depth(const iterator_base& pos) const
	{
	store_node *tmp=pos.node;

	if(tmp==0 || tmp==head || tmp==feet) return -1;

	int curdepth=0, maxdepth=0;
	while(true) { // try to walk the bottom of the store
		while(tmp->first_child==0) {
			if(tmp==pos.node) return maxdepth;
			if(tmp->next_sibling==0) {
				// try to walk up and then right again
				do {
					tmp=tmp->parent;
               if(tmp==0) return maxdepth;
               --curdepth;
				   } while(tmp->next_sibling==0);
				}
         if(tmp==pos.node) return maxdepth;
			tmp=tmp->next_sibling;
			}
		tmp=tmp->first_child;
		++curdepth;
		maxdepth=std::max(curdepth, maxdepth);
		} 
	}

template <class T, class store_node_allocator>
unsigned int store<T, store_node_allocator>::number_of_children(const iterator_base& it) 
	{
	store_node *pos=it.node->first_child;
	if(pos==0) return 0;
	
	unsigned int ret=1;
//	  while(pos!=it.node->last_child) {
//		  ++ret;
//		  pos=pos->next_sibling;
//		  }
	while((pos=pos->next_sibling))
		++ret;
	return ret;
	}

template <class T, class store_node_allocator>
unsigned int store<T, store_node_allocator>::number_of_siblings(const iterator_base& it) const
	{
	store_node *pos=it.node;
	unsigned int ret=0;
	// count forward
	while(pos->next_sibling && 
			pos->next_sibling!=head &&
			pos->next_sibling!=feet) {
		++ret;
		pos=pos->next_sibling;
		}
	// count backward
	pos=it.node;
	while(pos->prev_sibling && 
			pos->prev_sibling!=head &&
			pos->prev_sibling!=feet) {
		++ret;
		pos=pos->prev_sibling;
		}
	
	return ret;
	}

template <class T, class store_node_allocator>
void store<T, store_node_allocator>::swap(sibling_iterator it)
	{
	store_node *nxt=it.node->next_sibling;
	if(nxt) {
		if(it.node->prev_sibling)
			it.node->prev_sibling->next_sibling=nxt;
		else
			it.node->parent->first_child=nxt;
		nxt->prev_sibling=it.node->prev_sibling;
		store_node *nxtnxt=nxt->next_sibling;
		if(nxtnxt)
			nxtnxt->prev_sibling=it.node;
		else
			it.node->parent->last_child=it.node;
		nxt->next_sibling=it.node;
		it.node->prev_sibling=nxt;
		it.node->next_sibling=nxtnxt;
		}
	}

template <class T, class store_node_allocator>
void store<T, store_node_allocator>::swap(iterator one, iterator two)
	{
	// if one and two are adjacent siblings, use the sibling swap
	if(one.node->next_sibling==two.node) swap(one);
	else if(two.node->next_sibling==one.node) swap(two);
	else {
		store_node *nxt1=one.node->next_sibling;
		store_node *nxt2=two.node->next_sibling;
		store_node *pre1=one.node->prev_sibling;
		store_node *pre2=two.node->prev_sibling;
		store_node *par1=one.node->parent;
		store_node *par2=two.node->parent;

		// reconnect
		one.node->parent=par2;
		one.node->next_sibling=nxt2;
		if(nxt2) nxt2->prev_sibling=one.node;
		else     par2->last_child=one.node;
		one.node->prev_sibling=pre2;
		if(pre2) pre2->next_sibling=one.node;
		else     par2->first_child=one.node;    

		two.node->parent=par1;
		two.node->next_sibling=nxt1;
		if(nxt1) nxt1->prev_sibling=two.node;
		else     par1->last_child=two.node;
		two.node->prev_sibling=pre1;
		if(pre1) pre1->next_sibling=two.node;
		else     par1->first_child=two.node;
		}
	}


template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::is_in_substore(const iterator_base& it, const iterator_base& begin, 
																 const iterator_base& end) const
	{
	// FIXME: this should be optimised.
	pre_order_iterator tmp=begin;
	while(tmp!=end) {
		if(tmp==it) return true;
		++tmp;
		}
	return false;
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::is_valid(const iterator_base& it) const
	{
	if(it.node==0 || it.node==feet || it.node==head) return false;
	else return true;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::iterator store<T, store_node_allocator>::lowest_common_ancestor(
	const iterator_base& one, const iterator_base& two) const
	{
	std::set<iterator, iterator_base_less> parents;

	// Walk up from 'one' storing all parents.
	iterator walk=one;
	do {
		walk=parent(walk);
		parents.insert(walk);
		} while( is_valid(parent(walk)) );

	// Walk up from 'two' until we encounter a node in parents.
	walk=two;
	do {
		walk=parent(walk);
		if(parents.find(walk) != parents.end()) break;
		} while( is_valid(parent(walk)) );

	return walk;
	}

template <class T, class store_node_allocator>
unsigned int store<T, store_node_allocator>::index(sibling_iterator it) const
	{
	unsigned int ind=0;
	if(it.node->parent==0) {
		while(it.node->prev_sibling!=head) {
			it.node=it.node->prev_sibling;
			++ind;
			}
		}
	else {
		while(it.node->prev_sibling!=0) {
			it.node=it.node->prev_sibling;
			++ind;
			}
		}
	return ind;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator store<T, store_node_allocator>::sibling(const iterator_base& it, unsigned int num)
   {
   store_node *tmp;
   if(it.node->parent==0) {
      tmp=head->next_sibling;
      while(num) {
         tmp = tmp->next_sibling;
         --num;
         }
      }
   else {
      tmp=it.node->parent->first_child;
      while(num) {
         assert(tmp!=0);
         tmp = tmp->next_sibling;
         --num;
         }
      }
   return tmp;
   }
 
template <class T, class store_node_allocator>
void store<T, store_node_allocator>::debug_verify_consistency() const
	{
	iterator it=begin();
	while(it!=end()) {
		if(it.node->parent!=0) {
			if(it.node->prev_sibling==0) 
				assert(it.node->parent->first_child==it.node);
			else 
				assert(it.node->prev_sibling->next_sibling==it.node);
			if(it.node->next_sibling==0) 
				assert(it.node->parent->last_child==it.node);
			else
				assert(it.node->next_sibling->prev_sibling==it.node);
			}
		++it;
		}
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator store<T, store_node_allocator>::child(const iterator_base& it, unsigned int num) 
	{
	store_node *tmp=it.node->first_child;
	while(num--) {
		assert(tmp!=0);
		tmp=tmp->next_sibling;
		}
	return tmp;
	}




// Iterator base

template <class T, class store_node_allocator>
store<T, store_node_allocator>::iterator_base::iterator_base()
	: node(0), skip_current_children_(false)
	{
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::iterator_base::iterator_base(store_node *tn)
	: node(tn), skip_current_children_(false)
	{
	}

template <class T, class store_node_allocator>
T& store<T, store_node_allocator>::iterator_base::operator*() const
	{
	return node->data;
	}

template <class T, class store_node_allocator>
T* store<T, store_node_allocator>::iterator_base::operator->() const
	{
	return &(node->data);
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::post_order_iterator::operator!=(const post_order_iterator& other) const
	{
	if(other.node!=this->node) return true;
	else return false;
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::post_order_iterator::operator==(const post_order_iterator& other) const
	{
	if(other.node==this->node) return true;
	else return false;
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::pre_order_iterator::operator!=(const pre_order_iterator& other) const
	{
	if(other.node!=this->node) return true;
	else return false;
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::pre_order_iterator::operator==(const pre_order_iterator& other) const
	{
	if(other.node==this->node) return true;
	else return false;
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::sibling_iterator::operator!=(const sibling_iterator& other) const
	{
	if(other.node!=this->node) return true;
	else return false;
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::sibling_iterator::operator==(const sibling_iterator& other) const
	{
	if(other.node==this->node) return true;
	else return false;
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::leaf_iterator::operator!=(const leaf_iterator& other) const
   {
   if(other.node!=this->node) return true;
   else return false;
   }

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::leaf_iterator::operator==(const leaf_iterator& other) const
   {
   if(other.node==this->node && other.top_node==this->top_node) return true;
   else return false;
   }

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator store<T, store_node_allocator>::iterator_base::begin() const
	{
	if(node->first_child==0) 
		return end();

	sibling_iterator ret(node->first_child);
	ret.parent_=this->node;
	return ret;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator store<T, store_node_allocator>::iterator_base::end() const
	{
	sibling_iterator ret(0);
	ret.parent_=node;
	return ret;
	}

template <class T, class store_node_allocator>
void store<T, store_node_allocator>::iterator_base::skip_children()
	{
	skip_current_children_=true;
	}

template <class T, class store_node_allocator>
void store<T, store_node_allocator>::iterator_base::skip_children(bool skip)
   {
   skip_current_children_=skip;
   }

template <class T, class store_node_allocator>
unsigned int store<T, store_node_allocator>::iterator_base::number_of_children() const
	{
	store_node *pos=node->first_child;
	if(pos==0) return 0;
	
	unsigned int ret=1;
	while(pos!=node->last_child) {
		++ret;
		pos=pos->next_sibling;
		}
	return ret;
	}



// Pre-order iterator

template <class T, class store_node_allocator>
store<T, store_node_allocator>::pre_order_iterator::pre_order_iterator() 
	: iterator_base(0)
	{
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::pre_order_iterator::pre_order_iterator(store_node *tn)
	: iterator_base(tn)
	{
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::pre_order_iterator::pre_order_iterator(const iterator_base &other)
	: iterator_base(other.node)
	{
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::pre_order_iterator::pre_order_iterator(const sibling_iterator& other)
	: iterator_base(other.node)
	{
	if(this->node==0) {
		if(other.range_last()!=0)
			this->node=other.range_last();
		else 
			this->node=other.parent_;
		this->skip_children();
		++(*this);
		}
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::pre_order_iterator& store<T, store_node_allocator>::pre_order_iterator::operator++()
	{
	assert(this->node!=0);
	if(!this->skip_current_children_ && this->node->first_child != 0) {
		this->node=this->node->first_child;
		}
	else {
		this->skip_current_children_=false;
		while(this->node->next_sibling==0) {
			this->node=this->node->parent;
			if(this->node==0)
				return *this;
			}
		this->node=this->node->next_sibling;
		}
	return *this;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::pre_order_iterator& store<T, store_node_allocator>::pre_order_iterator::operator--()
	{
	assert(this->node!=0);
	if(this->node->prev_sibling) {
		this->node=this->node->prev_sibling;
		while(this->node->last_child)
			this->node=this->node->last_child;
		}
	else {
		this->node=this->node->parent;
		if(this->node==0)
			return *this;
		}
	return *this;
}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::pre_order_iterator store<T, store_node_allocator>::pre_order_iterator::operator++(int)
	{
	pre_order_iterator copy = *this;
	++(*this);
	return copy;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::pre_order_iterator& store<T, store_node_allocator>::pre_order_iterator::next_skip_children() 
   {
	(*this).skip_children();
	(*this)++;
	return *this;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::pre_order_iterator store<T, store_node_allocator>::pre_order_iterator::operator--(int)
{
  pre_order_iterator copy = *this;
  --(*this);
  return copy;
}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::pre_order_iterator& store<T, store_node_allocator>::pre_order_iterator::operator+=(unsigned int num)
	{
	while(num>0) {
		++(*this);
		--num;
		}
	return (*this);
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::pre_order_iterator& store<T, store_node_allocator>::pre_order_iterator::operator-=(unsigned int num)
	{
	while(num>0) {
		--(*this);
		--num;
		}
	return (*this);
	}



// Post-order iterator

template <class T, class store_node_allocator>
store<T, store_node_allocator>::post_order_iterator::post_order_iterator() 
	: iterator_base(0)
	{
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::post_order_iterator::post_order_iterator(store_node *tn)
	: iterator_base(tn)
	{
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::post_order_iterator::post_order_iterator(const iterator_base &other)
	: iterator_base(other.node)
	{
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::post_order_iterator::post_order_iterator(const sibling_iterator& other)
	: iterator_base(other.node)
	{
	if(this->node==0) {
		if(other.range_last()!=0)
			this->node=other.range_last();
		else 
			this->node=other.parent_;
		this->skip_children();
		++(*this);
		}
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::post_order_iterator& store<T, store_node_allocator>::post_order_iterator::operator++()
	{
	assert(this->node!=0);
	if(this->node->next_sibling==0) {
		this->node=this->node->parent;
		this->skip_current_children_=false;
		}
	else {
		this->node=this->node->next_sibling;
		if(this->skip_current_children_) {
			this->skip_current_children_=false;
			}
		else {
			while(this->node->first_child)
				this->node=this->node->first_child;
			}
		}
	return *this;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::post_order_iterator& store<T, store_node_allocator>::post_order_iterator::operator--()
	{
	assert(this->node!=0);
	if(this->skip_current_children_ || this->node->last_child==0) {
		this->skip_current_children_=false;
		while(this->node->prev_sibling==0)
			this->node=this->node->parent;
		this->node=this->node->prev_sibling;
		}
	else {
		this->node=this->node->last_child;
		}
	return *this;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::post_order_iterator store<T, store_node_allocator>::post_order_iterator::operator++(int)
	{
	post_order_iterator copy = *this;
	++(*this);
	return copy;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::post_order_iterator store<T, store_node_allocator>::post_order_iterator::operator--(int)
	{
	post_order_iterator copy = *this;
	--(*this);
	return copy;
	}


template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::post_order_iterator& store<T, store_node_allocator>::post_order_iterator::operator+=(unsigned int num)
	{
	while(num>0) {
		++(*this);
		--num;
		}
	return (*this);
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::post_order_iterator& store<T, store_node_allocator>::post_order_iterator::operator-=(unsigned int num)
	{
	while(num>0) {
		--(*this);
		--num;
		}
	return (*this);
	}

template <class T, class store_node_allocator>
void store<T, store_node_allocator>::post_order_iterator::descend_all()
	{
	assert(this->node!=0);
	while(this->node->first_child)
		this->node=this->node->first_child;
	}


// Breadth-first iterator

template <class T, class store_node_allocator>
store<T, store_node_allocator>::breadth_first_queued_iterator::breadth_first_queued_iterator()
	: iterator_base()
	{
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::breadth_first_queued_iterator::breadth_first_queued_iterator(store_node *tn)
	: iterator_base(tn)
	{
	traversal_queue.push(tn);
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::breadth_first_queued_iterator::breadth_first_queued_iterator(const iterator_base& other)
	: iterator_base(other.node)
	{
	traversal_queue.push(other.node);
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::breadth_first_queued_iterator::operator!=(const breadth_first_queued_iterator& other) const
	{
	if(other.node!=this->node) return true;
	else return false;
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::breadth_first_queued_iterator::operator==(const breadth_first_queued_iterator& other) const
	{
	if(other.node==this->node) return true;
	else return false;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::breadth_first_queued_iterator& store<T, store_node_allocator>::breadth_first_queued_iterator::operator++()
	{
	assert(this->node!=0);

	// Add child nodes and pop current node
	sibling_iterator sib=this->begin();
	while(sib!=this->end()) {
		traversal_queue.push(sib.node);
		++sib;
		}
	traversal_queue.pop();
	if(traversal_queue.size()>0)
		this->node=traversal_queue.front();
	else 
		this->node=0;
	return (*this);
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::breadth_first_queued_iterator store<T, store_node_allocator>::breadth_first_queued_iterator::operator++(int)
	{
	breadth_first_queued_iterator copy = *this;
	++(*this);
	return copy;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::breadth_first_queued_iterator& store<T, store_node_allocator>::breadth_first_queued_iterator::operator+=(unsigned int num)
	{
	while(num>0) {
		++(*this);
		--num;
		}
	return (*this);
	}



// Fixed depth iterator

template <class T, class store_node_allocator>
store<T, store_node_allocator>::fixed_depth_iterator::fixed_depth_iterator()
	: iterator_base()
	{
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::fixed_depth_iterator::fixed_depth_iterator(store_node *tn)
	: iterator_base(tn), top_node(0)
	{
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::fixed_depth_iterator::fixed_depth_iterator(const iterator_base& other)
	: iterator_base(other.node), top_node(0)
	{
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::fixed_depth_iterator::fixed_depth_iterator(const sibling_iterator& other)
	: iterator_base(other.node), top_node(0)
	{
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::fixed_depth_iterator::fixed_depth_iterator(const fixed_depth_iterator& other)
	: iterator_base(other.node), top_node(other.top_node)
	{
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::fixed_depth_iterator::operator==(const fixed_depth_iterator& other) const
	{
	if(other.node==this->node && other.top_node==top_node) return true;
	else return false;
	}

template <class T, class store_node_allocator>
bool store<T, store_node_allocator>::fixed_depth_iterator::operator!=(const fixed_depth_iterator& other) const
	{
	if(other.node!=this->node || other.top_node!=top_node) return true;
	else return false;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::fixed_depth_iterator& store<T, store_node_allocator>::fixed_depth_iterator::operator++()
	{
	assert(this->node!=0);

	if(this->node->next_sibling) {
		this->node=this->node->next_sibling;
		}
	else { 
		int relative_depth=0;
	   upper:
		do {
			if(this->node==this->top_node) {
				this->node=0; // FIXME: return a proper fixed_depth end iterator once implemented
				return *this;
				}
			this->node=this->node->parent;
			if(this->node==0) return *this;
			--relative_depth;
			} while(this->node->next_sibling==0);
	   lower:
		this->node=this->node->next_sibling;
		while(this->node->first_child==0) {
			if(this->node->next_sibling==0)
				goto upper;
			this->node=this->node->next_sibling;
			if(this->node==0) return *this;
			}
		while(relative_depth<0 && this->node->first_child!=0) {
			this->node=this->node->first_child;
			++relative_depth;
			}
		if(relative_depth<0) {
			if(this->node->next_sibling==0) goto upper;
			else                          goto lower;
			}
		}
	return *this;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::fixed_depth_iterator& store<T, store_node_allocator>::fixed_depth_iterator::operator--()
	{
	assert(this->node!=0);

	if(this->node->prev_sibling) {
		this->node=this->node->prev_sibling;
		}
	else { 
		int relative_depth=0;
	   upper:
		do {
			if(this->node==this->top_node) {
				this->node=0;
				return *this;
				}
			this->node=this->node->parent;
			if(this->node==0) return *this;
			--relative_depth;
			} while(this->node->prev_sibling==0);
	   lower:
		this->node=this->node->prev_sibling;
		while(this->node->last_child==0) {
			if(this->node->prev_sibling==0)
				goto upper;
			this->node=this->node->prev_sibling;
			if(this->node==0) return *this;
			}
		while(relative_depth<0 && this->node->last_child!=0) {
			this->node=this->node->last_child;
			++relative_depth;
			}
		if(relative_depth<0) {
			if(this->node->prev_sibling==0) goto upper;
			else                            goto lower;
			}
		}
	return *this;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::fixed_depth_iterator store<T, store_node_allocator>::fixed_depth_iterator::operator++(int)
	{
	fixed_depth_iterator copy = *this;
	++(*this);
	return copy;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::fixed_depth_iterator store<T, store_node_allocator>::fixed_depth_iterator::operator--(int)
   {
	fixed_depth_iterator copy = *this;
	--(*this);
	return copy;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::fixed_depth_iterator& store<T, store_node_allocator>::fixed_depth_iterator::operator-=(unsigned int num)
	{
	while(num>0) {
		--(*this);
		--(num);
		}
	return (*this);
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::fixed_depth_iterator& store<T, store_node_allocator>::fixed_depth_iterator::operator+=(unsigned int num)
	{
	while(num>0) {
		++(*this);
		--(num);
		}
	return *this;
	}


// Sibling iterator

template <class T, class store_node_allocator>
store<T, store_node_allocator>::sibling_iterator::sibling_iterator() 
	: iterator_base()
	{
	set_parent_();
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::sibling_iterator::sibling_iterator(store_node *tn)
	: iterator_base(tn)
	{
	set_parent_();
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::sibling_iterator::sibling_iterator(const iterator_base& other)
	: iterator_base(other.node)
	{
	set_parent_();
	}

template <class T, class store_node_allocator>
store<T, store_node_allocator>::sibling_iterator::sibling_iterator(const sibling_iterator& other)
	: iterator_base(other), parent_(other.parent_)
	{
	}

template <class T, class store_node_allocator>
void store<T, store_node_allocator>::sibling_iterator::set_parent_()
	{
	parent_=0;
	if(this->node==0) return;
	if(this->node->parent!=0)
		parent_=this->node->parent;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator& store<T, store_node_allocator>::sibling_iterator::operator++()
	{
	if(this->node)
		this->node=this->node->next_sibling;
	return *this;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator& store<T, store_node_allocator>::sibling_iterator::operator--()
	{
	if(this->node) this->node=this->node->prev_sibling;
	else {
		assert(parent_);
		this->node=parent_->last_child;
		}
	return *this;
}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator store<T, store_node_allocator>::sibling_iterator::operator++(int)
	{
	sibling_iterator copy = *this;
	++(*this);
	return copy;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator store<T, store_node_allocator>::sibling_iterator::operator--(int)
	{
	sibling_iterator copy = *this;
	--(*this);
	return copy;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator& store<T, store_node_allocator>::sibling_iterator::operator+=(unsigned int num)
	{
	while(num>0) {
		++(*this);
		--num;
		}
	return (*this);
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::sibling_iterator& store<T, store_node_allocator>::sibling_iterator::operator-=(unsigned int num)
	{
	while(num>0) {
		--(*this);
		--num;
		}
	return (*this);
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::store_node *store<T, store_node_allocator>::sibling_iterator::range_first() const
	{
	store_node *tmp=parent_->first_child;
	return tmp;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::store_node *store<T, store_node_allocator>::sibling_iterator::range_last() const
	{
	return parent_->last_child;
	}

// Leaf iterator

template <class T, class store_node_allocator>
store<T, store_node_allocator>::leaf_iterator::leaf_iterator() 
   : iterator_base(0), top_node(0)
   {
   }

template <class T, class store_node_allocator>
store<T, store_node_allocator>::leaf_iterator::leaf_iterator(store_node *tn, store_node *top)
   : iterator_base(tn), top_node(top)
   {
   }

template <class T, class store_node_allocator>
store<T, store_node_allocator>::leaf_iterator::leaf_iterator(const iterator_base &other)
   : iterator_base(other.node), top_node(0)
   {
   }

template <class T, class store_node_allocator>
store<T, store_node_allocator>::leaf_iterator::leaf_iterator(const sibling_iterator& other)
   : iterator_base(other.node), top_node(0)
   {
   if(this->node==0) {
      if(other.range_last()!=0)
         this->node=other.range_last();
      else 
         this->node=other.parent_;
      ++(*this);
      }
   }

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::leaf_iterator& store<T, store_node_allocator>::leaf_iterator::operator++()
   {
	assert(this->node!=0);
	if(this->node->first_child!=0) { // current node is no longer leaf (children got added)
		 while(this->node->first_child) 
			  this->node=this->node->first_child;
		 }
	else {
		 while(this->node->next_sibling==0) { 
			  if (this->node->parent==0) return *this;
			  this->node=this->node->parent;
			  if (top_node != 0 && this->node==top_node) return *this;
			  }
		 this->node=this->node->next_sibling;
		 while(this->node->first_child)
			  this->node=this->node->first_child;
		 }
	return *this;
   }

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::leaf_iterator& store<T, store_node_allocator>::leaf_iterator::operator--()
   {
	assert(this->node!=0);
	while (this->node->prev_sibling==0) {
		if (this->node->parent==0) return *this;
		this->node=this->node->parent;
		if (top_node !=0 && this->node==top_node) return *this; 
		}
	this->node=this->node->prev_sibling;
	while(this->node->last_child)
		this->node=this->node->last_child;
	return *this;
	}

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::leaf_iterator store<T, store_node_allocator>::leaf_iterator::operator++(int)
   {
   leaf_iterator copy = *this;
   ++(*this);
   return copy;
   }

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::leaf_iterator store<T, store_node_allocator>::leaf_iterator::operator--(int)
   {
   leaf_iterator copy = *this;
   --(*this);
   return copy;
   }


template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::leaf_iterator& store<T, store_node_allocator>::leaf_iterator::operator+=(unsigned int num)
   {
   while(num>0) {
      ++(*this);
      --num;
      }
   return (*this);
   }

template <class T, class store_node_allocator>
typename store<T, store_node_allocator>::leaf_iterator& store<T, store_node_allocator>::leaf_iterator::operator-=(unsigned int num)
   {
   while(num>0) {
      --(*this);
      --num;
      }
   return (*this);
   }

#endif

