/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   completion.h
 * Author: s0457958
 *
 * Created on 20 August 2015, 11:35
 */

#ifndef COMPLETION_H
#define COMPLETION_H

#include <mutex>
#include <condition_variable>

namespace captive {
	namespace util {
		template<typename retval_t>
		class Completion
		{
		public:
			Completion() : _complete(false) { }
			
			inline void signal(retval_t result)
			{
				std::unique_lock<std::mutex> lock(_m);
				
				if (_complete) return;
				
				_result = result;
				_complete = true;
				
				_cv.notify_all();
			}
			
			inline retval_t wait()
			{
				std::unique_lock<std::mutex> lock(_m);
				
				if (_complete) return _result;
				
				while (!_complete) _cv.wait(lock);
				
				return _result;
			}
			
		private:
			bool _complete;
			retval_t _result;
			std::mutex _m;
			std::condition_variable _cv;
		};
	}
}

#endif /* COMPLETION_H */

