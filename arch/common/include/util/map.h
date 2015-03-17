/*
 * File:   map.h
 * Author: spink
 *
 * Created on 16 March 2015, 18:04
 */

#ifndef MAP_H
#define	MAP_H

#include <util/pair.h>
#include <util/allocator.h>
#include <util/list.h>

namespace captive {
	namespace arch {
		namespace util {
			template<typename TKey, typename TValue, typename TAllocator=allocator<pair<TKey, TValue>>>
			class map
			{
			public:
				typedef pair<TKey, TValue> map_pair;
				typedef TKey key_type;
				typedef TValue value_type;

				class iterator
				{
				public:
					iterator(list<map_pair>& elems, bool is_end) : _elems(elems), _is_end(is_end) { }

					void operator ++()
					{

					}

					bool operator !=(const iterator& iter)
					{
						return false;
					}

				private:
					list<map_pair>& _elems;
					bool _is_end;
				};

				class value_iterator : public iterator
				{
				public:
					value_iterator(list<map_pair>& elems, bool is_end=false) : iterator(elems, is_end) { }

					value_type operator*()
					{
					}
				};

				class key_iterator : public iterator
				{
					key_iterator(list<map_pair>& elems) : iterator(elems, false) { }

					key_type operator*()
					{
					}
				};

				map() { }

				bool try_get_value(TKey key, TValue& value)
				{
					map_pair p;
					if (locate(key, p)) {
						value = p.t2;
						return true;
					} else {
						return false;
					}
				}

				void insert(TKey key, TValue value)
				{
					map_pair p;

					if (locate(key, p)) {
						assert(false);
					} else {
						elems.add(map_pair(key, value));
					}
				}

				typename list<map_pair>::iterator begin()
				{
					return elems.begin();
				}

				typename list<map_pair>::iterator end()
				{
					return elems.end();
				}

				value_iterator values_begin()
				{
					return value_iterator(elems, false);
				}

				value_iterator values_end()
				{
					return value_iterator(elems, true);
				}

			private:
				list<map_pair> elems;

				bool locate(TKey key, map_pair& pair)
				{
					for (auto elem : elems) {
						if (elem.t1 == key) {
							pair = elem;
							return true;
						}
					}

					return false;
				}
			};
		}
	}
}

#endif	/* MAP_H */
