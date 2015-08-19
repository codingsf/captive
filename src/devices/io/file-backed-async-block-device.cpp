#include <devices/io/file-backed-async-block-device.h>
#include <captive.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <aio.h>

USE_CONTEXT(BlockDevice);

using namespace captive::devices::io;

FileBackedAsyncBlockDevice::FileBackedAsyncBlockDevice() : AsyncBlockDevice(512), _file_descr(-1), _file_size(0), _block_count(0), _read_only(false)
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
	_read_only = read_only;

	_file_descr = open(filename.c_str(), read_only ? O_RDONLY : O_RDWR);
	if (_file_descr < 0) {
		ERROR << "Failed to open file";
		return false;
	}

	struct stat st;
	if (fstat(_file_descr, &st)) {
		ERROR << "Failed to stat file";
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

	return true;
}

void FileBackedAsyncBlockDevice::close_file()
{
	close(_file_descr);
	_file_descr = -1;
}

struct AsyncContext
{
	struct aiocb64 aiorq;
	AsyncBlockRequest *rq;
	FileBackedAsyncBlockDevice::block_request_cb_t cb;
};

extern "C" void async_read_complete(sigval_t sigval)
{
	AsyncContext *ctx = (AsyncContext *)sigval.sival_ptr;
	//DEBUG << "Asynchronous read operation has completed";
	
	ctx->rq->request_complete.complete(aio_error64(&ctx->aiorq) == 0);
	if (ctx->cb) {
		ctx->cb(ctx->rq);
	}
	
	delete ctx;
}

bool FileBackedAsyncBlockDevice::submit_request(AsyncBlockRequest *rq, block_request_cb_t cb)
{
	AsyncContext *ctx = new AsyncContext();
	ctx->rq = rq;
	ctx->cb = cb;
	
	bzero(&ctx->aiorq, sizeof(ctx->aiorq));
	ctx->aiorq.aio_fildes = _file_descr;
	ctx->aiorq.aio_offset = rq->offset;
	ctx->aiorq.aio_buf = (void *)rq->buffer;
	ctx->aiorq.aio_nbytes = rq->block_count * block_size();
	ctx->aiorq.aio_reqprio = 0;
	ctx->aiorq.aio_sigevent.sigev_notify = SIGEV_THREAD;
	ctx->aiorq.aio_sigevent.sigev_value.sival_ptr = ctx;
	ctx->aiorq.aio_sigevent._sigev_un._sigev_thread._function = async_read_complete;
	ctx->aiorq.aio_lio_opcode = 0;
	
	if (rq->is_read) {
		//DEBUG << "Enqueing asynchronous block read request";
		
		if (aio_read64(&ctx->aiorq)) {
			//ERROR << "AIO failure: " << strerror(errno);
			return false;
		}
	} else {
		if (aio_write64(&ctx->aiorq)) {
			//ERROR << "AIO failure: " << strerror(errno);
			return false;
		}
	}
	
	//DEBUG << "Asynchronous block request was queued up";
	return true;
}
