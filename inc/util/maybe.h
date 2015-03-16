/*
 * File:   maybe.h
 * Author: spink
 *
 * Created on 16 March 2015, 08:10
 */

#ifndef MAYBE_H
#define	MAYBE_H

namespace captive {
	namespace util {
		template<typename T>
		class maybe
		{
		public:
			maybe() : _has_value(false) { }
			maybe(T v) : _has_value(true), _value(v) { }

			inline bool has_value() const { return _has_value; }

			inline T& value() const { return _value; }

			bool operator==(const T& check) const {
				if (!_has_value) {
					return false;
				} else {
					return _value == check;
				}
			}

		private:
			bool _has_value;
			T _value;
		};
	}
}

#endif	/* MAYBE_H */

