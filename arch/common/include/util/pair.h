/*
 * File:   pair.h
 * Author: spink
 *
 * Created on 16 March 2015, 18:04
 */

#ifndef PAIR_H
#define	PAIR_H

namespace captive {
	namespace arch {
		namespace util {
			template<class T1, class T2>
			struct pair
			{
				typedef T1 t1_type;
				typedef T2 t2_type;

				t1_type t1;
				t2_type t2;

				constexpr pair() : t1(), t2() { }
				constexpr pair(const t1_type& _t1, const t2_type& _t2) : t1(_t1), t2(_t2) { }

				constexpr pair(const pair&) = default;
				constexpr pair(pair&&) = default;

				inline pair& operator=(const pair& p) {
					t1 = p.t1;
					t2 = p.t2;
					return *this;
				}
			};
		}
	}
}

#endif	/* PAIR_H */

