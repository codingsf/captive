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
	ThreadPool::ThreadPoolWorkerInfo *info = (ThreadPool::ThreadPoolWorkerInfo *)o;

	info->owner->thread_proc(info->id);
}

void ThreadPool::start()
{
	terminate = false;

	DEBUG << CONTEXT(ThreadPool) << " Launching " << _max_threads << " worker threads";
	for (uint32_t i = 0; i < _max_threads; i++) {
		ThreadPoolWorkerInfo *info = new ThreadPoolWorkerInfo();
		info->owner = this;
		info->id = i;

		threads.push_back(new std::thread(thread_proc_tramp, info));
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

void ThreadPool::thread_proc(uint32_t id)
{
	std::unique_lock<std::mutex> lock(work_queue_mutex);

	while (!terminate) {
		while (work_queue.size() == 0 && !terminate) {
			DEBUG << CONTEXT(ThreadPool) << " Worker thread " << std::this_thread::get_id() << " idling";
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

		lock.lock();
	}
}

void ThreadPool::process_work(ThreadPoolWork& work)
{
	DEBUG << CONTEXT(ThreadPool) << " Worker thread " << std::this_thread::get_id() << " picking up work";

	if (work.action) {
		uint64_t result = work.action(work.data);
		if (work.completion) {
			work.completion(result, work.data);
		}
	}
}
