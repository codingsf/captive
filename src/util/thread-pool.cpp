#include <util/thread-pool.h>
#include <captive.h>

DECLARE_CONTEXT(ThreadPool);

using namespace captive::util;

ThreadPool::ThreadPool(uint32_t min_threads, uint32_t max_threads) : _min_threads(min_threads), _max_threads(max_threads)
{

}

ThreadPool::~ThreadPool()
{
	stop();
}

void thread_proc_tramp(void *o)
{
	((ThreadPool *)o)->thread_proc();
}

void ThreadPool::start()
{
	terminate = false;

	for (uint32_t i = 0; i < _max_threads; i++) {
		threads.push_back(new std::thread(thread_proc_tramp, this));
	}
}

void ThreadPool::stop()
{
	if (threads.size() > 0) {
		terminate = true;
		work_queue_cond.notify_all();

		for (auto thread : threads) {
			if (thread->joinable()) {
				thread->join();
			}

			delete thread;
		}

		threads.clear();
	}
}

void ThreadPool::queue_work(action_t action, completion_t completion, void* data)
{
	ThreadPoolWork work;
	work.action = action;
	work.completion = completion;
	work.data = data;

	work_queue_mutex.lock();
	work_queue.push(work);
	work_queue_cond.notify_one();
	work_queue_mutex.unlock();
}

void ThreadPool::thread_proc()
{
	std::unique_lock<std::mutex> lock(work_queue_mutex);
	lock.unlock();

	while (!terminate) {
		lock.lock();

		DEBUG << "Waiting for work";
		while (work_queue.size() == 0 && !terminate) {
			work_queue_cond.wait(lock);
		}

		if (terminate) {
			lock.unlock();
			break;
		}

		ThreadPoolWork work = work_queue.front();
		work_queue.pop();

		lock.unlock();

		process_work(work);
	}
}

void ThreadPool::process_work(ThreadPoolWork& work)
{
	DEBUG << "Doing Work";

	if (work.action) {
		uint64_t result = work.action(work.data);
		if (work.completion) {
			work.completion(result, work.data);
		}
	}
}
