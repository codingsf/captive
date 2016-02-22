#include <devices/io/block/file-backed-block-device.h>
#include <captive.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <aio.h>

USE_CONTEXT(BlockDevice);
DECLARE_CHILD_CONTEXT(FileBackedBlockDevice, BlockDevice);

using namespace captive::devices::io::block;

#ifndef USE_POSIX_AIO
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
#endif

FileBackedBlockDevice::FileBackedBlockDevice() 
	: BlockDevice(512),
		_file_descr(-1),
		_file_size(0),
		_block_count(0),
#ifndef USE_POSIX_AIO
		_aio_thread(NULL),
		_terminate(false),
#endif
		_read_only(false)
{
}

FileBackedBlockDevice::~FileBackedBlockDevice()
{
	if (_file_descr > -1) {
		close_file();
	}
}

bool FileBackedBlockDevice::open_file(std::string filename, bool read_only)
{
	if (_file_descr >= 0) return false;
	
	_read_only = read_only;

	_file_descr = open(filename.c_str(), (read_only ? O_RDONLY : O_RDWR) | O_EXCL);
	if (_file_descr < 0) {
		ERROR << CONTEXT(FileBackedBlockDevice) << "Failed to open file";
		return false;
	}

	struct stat st;
	if (fstat(_file_descr, &st)) {
		ERROR << CONTEXT(FileBackedBlockDevice) << "Failed to stat file";
		close(_file_descr);
		_file_descr = -1;
		
		return false;
	}

	DEBUG << CONTEXT(FileBackedBlockDevice) << "File size: " << st.st_size;

	_file_size = st.st_size;

	_block_count = _file_size / block_size();
	if (_file_size % block_size() != 0) {
		_block_count++;
	}
		
#ifndef USE_POSIX_AIO
	bzero((void *)&_aio, sizeof(_aio));
	if (io_setup(128, &_aio)) {
		ERROR << CONTEXT(FileBackedBlockDevice) << "Unable to setup AIO:" << strerror(errno);
		close(_file_descr);
		_file_descr = -1;
		
		return false;
	}
	
	_terminate = false;
	_aio_thread = new std::thread(aio_thread_proc, this);
#endif
	
	return true;
}

void FileBackedBlockDevice::close_file()
{
	if (_file_descr == -1) return;
	
#ifndef USE_POSIX_AIO
	_terminate = true;
	io_destroy(_aio);

	if (_aio_thread->joinable()) _aio_thread->join();	
#endif
	
	close(_file_descr);
	_file_descr = -1;
}

#ifdef USE_POSIX_AIO
void FileBackedBlockDevice::posix_aio_notify(sigval_t sv)
{
	AsyncBlockRequestContext *ctx = (AsyncBlockRequestContext *)sv.sival_ptr;
	
	int err;
	while ((err = aio_error(&ctx->aio_cb)) == EINPROGRESS);
	
	if (err) {
		fprintf(stderr, "ERROR\n");
		exit(0);
	}
	
	uint64_t transferred = aio_return(&ctx->aio_cb);
	bool success = transferred == ctx->aio_cb.aio_nbytes;
	
	if (ctx->cb) {
		ctx->cb(ctx->rq, success);
	}
		
	delete ctx;
}

bool FileBackedBlockDevice::submit_request(AsyncBlockRequest *rq, block_request_cb_t callback)
{
	AsyncBlockRequestContext *ctx = new AsyncBlockRequestContext(rq, callback);
	
	bzero(&ctx->aio_cb, sizeof(ctx->aio_cb));
	ctx->aio_cb.aio_fildes = _file_descr;
	
	ctx->aio_cb.aio_buf = rq->buffer;
	ctx->aio_cb.aio_nbytes = rq->block_count * block_size();
	ctx->aio_cb.aio_offset = rq->block_offset * block_size();
	
	ctx->aio_cb.aio_sigevent.sigev_notify = SIGEV_THREAD;
	ctx->aio_cb.aio_sigevent.sigev_notify_function = posix_aio_notify;
	ctx->aio_cb.aio_sigevent.sigev_value.sival_ptr = ctx;

	if (rq->is_read) {
		return aio_read(&ctx->aio_cb) == 0;
	} else {
		return aio_write(&ctx->aio_cb) == 0;
	}
}
#else
bool FileBackedBlockDevice::submit_request(BlockDeviceRequest *rq, block_request_cb_t callback)
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
	cb.aio_data = (uint64_t)new BlockDeviceRequestContext(rq, callback);
	
	DEBUG << CONTEXT(FileBackedBlockDevice) << "Submitting IO request";
	int rc = io_submit(_aio, 1, cbs);
	if (rc < 0) {
		ERROR << CONTEXT(FileBackedBlockDevice) << "IO submission error";
		return false;
	} else if (rc != 1) {
		ERROR << CONTEXT(FileBackedBlockDevice) << "IO submission rejection";
		return false;
	}
	
	return true;
}

void FileBackedBlockDevice::aio_thread_proc(FileBackedBlockDevice* bdev)
{
	struct io_event events[8];
	while (!bdev->_terminate) {		
		int rc = io_getevents(bdev->_aio, 1, 8, events, NULL);
		if (rc < 0) {
			if (errno == EINTR) continue;
			
			ERROR << CONTEXT(FileBackedBlockDevice) << "IO event error";
			break;
		}
		
		for (int i = 0; i < rc; i++) {
			BlockDeviceRequestContext *ctx = (BlockDeviceRequestContext *)events[i].data;
			bool success = events[i].res == (ctx->rq->block_count * bdev->block_size());

			if (ctx->cb) {
				ctx->cb(ctx->rq, success);
			}
		
			delete ctx;
		}
	}
}
#endif