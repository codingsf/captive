/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   lock-check.h
 * Author: s0457958
 *
 * Created on 31 August 2016, 11:45
 */

#ifndef LOCK_CHECK_H
#define LOCK_CHECK_H

#include <mutex>

namespace captive {
	namespace util {
		class checked_mutex
		{
		public:
			inline void lock() { mtx.lock(); }
			inline void unlock() { mtx.unlock(); }
			
		private:
			std::mutex mtx;
		};
	}
}

#endif /* LOCK_CHECK_H */

