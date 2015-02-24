#include <devices/io/file-backed-block-device.h>
#include <captive.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

using namespace captive::devices::io;

FileBackedBlockDevice::FileBackedBlockDevice() : BlockDevice(512), _file_descr(-1), _file_data(NULL), _file_size(0), _block_count(0), _use_mmap(false), _read_only(false)
{

}

FileBackedBlockDevice::~FileBackedBlockDevice()
{
	if (_file_data != NULL) {
		close_file();
	}
}

bool FileBackedBlockDevice::open_file(std::string filename, bool read_only)
{
	_read_only = read_only;

	int fd = open(filename.c_str(), read_only ? O_RDONLY : O_RDWR);
	if (fd < 0) {
		ERROR << "Failed to open file";
		return false;
	}

	struct stat st;
	if (fstat(fd, &st)) {
		ERROR << "Failed to stat file";
		close(fd);
		return false;
	}

	DEBUG << "File size: " << st.st_size;

	_file_size = st.st_size;

	if (_use_mmap) {
		_file_data = mmap(NULL, _file_size, PROT_READ | (_read_only ? 0 : PROT_WRITE), MAP_PRIVATE, fd, 0);
		close(fd);

		if (_file_data == MAP_FAILED) {
			ERROR << "Failed to mmap file";
			return false;
		}
	} else {
		_file_data = NULL;
		_file_descr = fd;
	}

	_block_count = _file_size / block_size();
	if (_file_size % block_size() != 0) {
		_block_count++;
	}

	DEBUG << "File opened and mapped to: " << _file_data << ", blocks=" << std::dec << _block_count;
	return true;
}

void FileBackedBlockDevice::close_file()
{
	if (_use_mmap) {
		munmap(_file_data, _file_size);
		_file_data = NULL;
	} else {
		close(_file_descr);
	}
}

bool FileBackedBlockDevice::read_block(uint64_t block_idx, uint8_t* buffer)
{
	return read_blocks(block_idx, 1, buffer);
}

bool FileBackedBlockDevice::read_blocks(uint64_t block_idx, uint32_t count, uint8_t* buffer)
{
	assert(count < _block_count);

	if (block_idx >= _block_count) {
		WARNING << "Attempted to read blocks past end of device";
		return false;
	}

	if (block_idx > _block_count - count) {
		count = (uint32_t)(_block_count - block_idx);
	}

	// DEBUG << "Reading " << count << " blocks " << block_idx;

	if (_use_mmap) {
		void *data_ptr = get_data_ptr(block_idx);
		memcpy(buffer, data_ptr, block_size() * count);
	} else {
		size_t offset = block_idx * block_size();
		size_t size = block_size() * count;

		ssize_t actually_read;
		do {
			lseek64(_file_descr, offset, SEEK_SET);
			actually_read = read(_file_descr, buffer, size);
		} while (actually_read != (ssize_t)size);
	}

	return true;
}

bool FileBackedBlockDevice::write_block(uint64_t block_idx, const uint8_t* buffer)
{
	return write_blocks(block_idx, 1, buffer);
}

bool FileBackedBlockDevice::write_blocks(uint64_t block_idx, uint32_t count, const uint8_t* buffer)
{
	assert(!_read_only);
	assert(count < _block_count);

	if (block_idx >= _block_count) {
		WARNING << "Attempted to write blocks past end of device";
		return false;
	}

	if (block_idx > _block_count - count) {
		count = _block_count - block_idx;
	}

	// DEBUG << "Writing " << count << " blocks " << block_idx;

	if (_use_mmap) {
		void *data_ptr = get_data_ptr(block_idx);
		memcpy(data_ptr, buffer, block_size() * count);
	} else {
		size_t offset = block_idx * block_size();
		size_t size = block_size() * count;

		ssize_t actually_written;
		do {
			lseek64(_file_descr, offset, SEEK_SET);
			actually_written = write(_file_descr, buffer, size);
		} while (actually_written != (ssize_t)size);
	}

	return true;
}
