#ifndef FD_MANAGER_H
#define FD_MANAGER_H

namespace captive {
	namespace util {
		class FDManager
		{
		public:
			void start();
			void stop();
			
		private:
			int _stop_event;
		};
	}
}

#endif /* FD_MANAGER_H */

