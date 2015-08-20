#include <devices/io/file-backed-async-block-device.h>
#include <captive.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>

inline int io_setup(unsigned nr, aio_context_t *ctxp)
{
	return syscall(__NR_io_setup, nr, ctxp);
}

inline int io_destroy(aio_context_t ctx) 
{
	return syscall(__NR_io_destroy, ctx);
}

inline int io_submit(aio_context_t ctx, long nr,  struct iocb **iocbpp) 
{
	return syscall(__NR_io_submit, ctx, nr, iocbpp);
}

inline int io_getevents(aio_context_t ctx, long min_nr, long max_nr, struct io_event *events, struct timespec *timeout)
{
	return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout);
}

USE_CONTEXT(BlockDevice);
DECLARE_CHILD_CONTEXT(FileBackedAsyncBlockDevice, BlockDevice);

using namespace captive::devices::io;

FileBackedAsyncBlockDevice::FileBackedAsyncBlockDevice() : AsyncBlockDevice(512), _file_descr(-1), _file_size(0), _block_count(0), _read_only(false), _aio_thread(NULL), _terminate(false)
{
}

FileBackedAsyncBlockDevice::~FileBackedAsyncBlockDevice()
{
	if (_file_descr > -1) {
		close_file();
	}
}

bool FileBackedAsyncBlockDevice::open_file(std::string filename, bool read_only)
{
	if (_file_descr >= 0) return false;
	
	_read_only = read_only;

	_file_descr = open(filename.c_str(), (read_only ? O_RDONLY : O_RDWR));
	if (_file_descr < 0) {
		ERROR << CONTEXT(BlockDevice) << "Failed to open file";
		return false;
	}

	struct stat st;
	if (fstat(_file_descr, &st)) {
		ERROR << CONTEXT(BlockDevice) << "Failed to stat file";
		close(_file_descr);
		_file_descr = -1;
		
		return false;
	}

	DEBUG << CONTEXT(BlockDevice) << "File size: " << st.st_size;

	_file_size = st.st_size;

	_block_count = _file_size / block_size();
	if (_file_size % block_size() != 0) {
		_block_count++;
	}
	
	if (io_setup(128, &_aio)) {
		ERROR << CONTEXT(FileBackedAsyncBlockDevice) << "Unable to setup AIO";
		close(_file_descr);
		_file_descr = -1;
		
		return false;
	}
	
	_terminate = false;
	_aio_thread = new std::thread(aio_thread_proc, this);
	
	return true;
}

void FileBackedAsyncBlockDevice::close_file()
{
	if (_file_descr == -1) return;
	
	_terminate = true;
	io_destroy(_aio);

	if (_aio_thread->joinable()) _aio_thread->join();	
	
	close(_file_descr);
	_file_descr = -1;
}

bool FileBackedAsyncBlockDevice::submit_request(AsyncBlockRequest *rq, block_request_cb_t callback)
{
	struct iocb cb;
	struct iocb *cbs[] = { &cb };
	
	bzero(&cb, sizeof(cb));
	cb.aio_fildes = _file_descr;
	
	if (rq->is_read)
		cb.aio_lio_opcode = IOCB_CMD_PREAD;
	else
		cb.aio_lio_opcode = IOCB_CMD_PWRITE;
	
	cb.aio_buf = (uint64_t)rq->buffer;
	cb.aio_offset = rq->block_offset * block_size();
	cb.aio_nbytes = rq->block_count * block_size();
	cb.aio_data = (uint64_t)new AsyncBlockRequestContext(rq, callback);
	
	DEBUG << CONTEXT(FileBackedAsyncBlockDevice) << "Submitting IO request";
	int rc = io_submit(_aio, 1, cbs);
	if (rc < 0) {
		ERROR << CONTEXT(FileBackedAsyncBlockDevice) << "IO submission error";
		return false;
	} else if (rc != 1) {
		ERROR << CONTEXT(FileBackedAsyncBlockDevice) << "IO submission rejection";
		return false;
	}
	
	return true;
}

void FileBackedAsyncBlockDevice::aio_thread_proc(FileBackedAsyncBlockDevice* bdev)
{
	struct io_event events[8];
	while (!bdev->_terminate) {		
		int rc = io_getevents(bdev->_aio, 1, 8, events, NULL);
		if (rc < 0) {
			if (errno == EINTR) continue;
			
			ERROR << CONTEXT(FileBackedAsyncBlockDevice) << "IO event error";
			break;
		}
		
		for (int i = 0; i < rc; i++) {
			AsyncBlockRequestContext *ctx = (AsyncBlockRequestContext *)events[i].data;
			bool success = events[i].res == (ctx->rq->block_count * bdev->block_size());

			if (ctx->cb) {
				ctx->cb(ctx->rq, success);
			}
		
			delete ctx;
		}
	}
}
