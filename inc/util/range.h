/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   range.h
 * Author: s0457958
 *
 * Created on 24 June 2016, 16:55
 */

#ifndef RANGE_H
#define RANGE_H

namespace captive
{
	namespace util
	{
		template< typename T >
		struct range
		{
		public:
			typedef T value_type;

			range( T const & center ) : min_( center ), max_( center ) { }
			range( T const & min, T const & max ) : min_( min ), max_( max ) { }

			inline T min() const { return min_; }
			inline T max() const { return max_; }
			
		private:
			T min_;
			T max_;
		};

		template <typename T>
		struct left_of_range : public std::binary_function< range<T>, range<T>, bool >
		{
			inline bool operator()( range<T> const & lhs, range<T> const & rhs ) const
			{
				return lhs.min() < rhs.min()
					&& lhs.max() <= rhs.min();
			}
		};
	}
}


#endif /* RANGE_H */
