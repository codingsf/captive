/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   completion.h
 * Author: s0457958
 *
 * Created on 17 August 2015, 13:29
 */

#ifndef COMPLETION_H
#define COMPLETION_H

#include <mutex>
#include <condition_variable>

namespace captive {
	namespace util {
		template<typename TResult>
		class Completion
		{
		public:
			Completion() : _complete(false) { }
			
			TResult wait()
			{
				std::unique_lock<std::mutex> lock(m);
				
				if (_complete) return _result;
				while (!_complete) cv.wait(lock);
				
				return _result;
			}
			
			void complete(TResult result)
			{
				std::unique_lock<std::mutex> lock(m);
				_complete = true;
				_result = result;
				
				cv.notify_all();
			}
			
		private:
			bool _complete;
			TResult _result;
			std::mutex m;
			std::condition_variable cv;
		};
	}
}

#endif /* COMPLETION_H */

