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

bool FileBackedAsyncBlockDevice::submit_request(AsyncBlockRequest& rq, block_request_cb_t cb)
{
	struct aiocb64 aiorq;
	
	aiorq.aio_fildes = _file_descr;
	aiorq.aio_offset = rq.offset;
	aiorq.aio_buf = (void *)rq.buffer;
	aiorq.aio_nbytes = rq.block_count * block_size();
	aiorq.aio_reqprio = 0;
	//aiorq.aio_sigevent.sigev_value = SIGEV_
	aiorq.aio_lio_opcode = 0;
	
	//aio_read64(&aiorq);
	
	return false;
}
