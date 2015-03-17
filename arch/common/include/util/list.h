/*
 * File:   list.h
 * Author: spink
 *
 * Created on 16 March 2015, 18:39
 */

#ifndef LIST_H
#define	LIST_H

namespace captive {
	namespace arch {
		namespace util {
			template<typename TElem>
			class list
			{
			private:
				struct node
				{
					node *next;
					TElem elem;
				};

			public:
				class iterator
				{
				public:
					iterator(node *st) : cur(st) { }

					TElem operator*()
					{
						return cur->elem;
					}

					bool operator!=(iterator& iter)
					{
						return cur != iter.cur;
					}

					void operator++()
					{
						cur = cur->next;
					}

				private:
					node *cur;
				};

				list() : head(NULL) { }

				inline void add(TElem elem) {
					node **slot = &head;

					while (*slot) {
						slot = &(*slot)->next;
					}

					*slot = new node;
					(*slot)->next = NULL;
					(*slot)->elem = elem;
				}

				inline void remove(iterator& i)
				{
					assert(false);
				}

				inline iterator begin() {
					return iterator(head);
				}

				inline iterator end() {
					return iterator(NULL);
				}

			private:
				node *head;
			};
		}
	}
}

#endif	/* LIST_H */

