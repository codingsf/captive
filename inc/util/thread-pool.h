/*
 * File:   thread-pool.h
 * Author: spink
 *
 * Created on 24 March 2015, 10:25
 */

#ifndef THREAD_POOL_H
#define	THREAD_POOL_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <list>

extern void thread_proc_tramp(void *);

namespace captive {
	namespace util {
		typedef uint64_t (*action_t)(void *data);
		typedef void (*completion_t)(uint64_t result, void *data);

		class ThreadPool
		{
			friend void ::thread_proc_tramp(void *);
		public:
			explicit ThreadPool(std::string name_pfx, uint32_t min_threads = 1, uint32_t max_threads = 4);
			~ThreadPool();

			void queue_work(action_t action, completion_t completion, void *data);

			void start();
			void stop();

		private:
			struct ThreadPoolWorkerInfo
			{
				ThreadPool *owner;
				uint32_t id;
				std::string name;
			};

			struct ThreadPoolWork
			{
				void *data;

				action_t action;
				completion_t completion;
			};

			std::string _name_pfx;
			uint32_t _min_threads, _max_threads;

			void thread_proc(uint32_t id);
			void process_work(ThreadPoolWork& work);

			std::list<std::thread *> threads;

			std::mutex work_queue_mutex;
			std::condition_variable work_queue_cond;
			std::queue<ThreadPoolWork> work_queue;

			volatile bool terminate;
			
			void start_new_thread();
			void stop_idle_thread();
		};
	}
}

#endif	/* THREAD_POOL_H */

