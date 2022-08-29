#pragma once

/*! \file save_load_xdr.h

Utilities for saving and loading xdr objects to disk 
*/

#include <fcntl.h>
#include <sys/stat.h>

#include <xdrpp/marshal.h>

#include "utils/cleanup.h"

#include "xdr/database_commitments.h"

namespace speedex {

/*! Load an xdr object from disk, dynamically allocating buffer for deserialization
  Can be somewhat slower than  load_xdr_from_file_fast(), but easier to use.
*/
template<typename xdr_type>
int __attribute__((warn_unused_result)) 
load_xdr_from_file(xdr_type& output, const char* filename)  {
	FILE* f = std::fopen(filename, "r");

	if (f == nullptr) {
		return -1;
	}

	std::vector<char> contents;
	const int BUF_SIZE = 65536;
	char buf[BUF_SIZE];

	int count = -1;
	while (count != 0) {
		count = std::fread(buf, sizeof(char), BUF_SIZE, f);
		if (count > 0) {
			contents.insert(contents.end(), buf, buf+count);
		}
	}

	xdr::xdr_from_opaque(contents, output);
	std::fclose(f);
	return 0;
}

/*! Load an xdr object from disk, 
    reading the disk into \a buffer and deserializing from this buffer.
*/
template<typename xdr_type>
int __attribute__((warn_unused_result)) 
load_xdr_from_file_fast(
	xdr_type& output, 
	const char* filename, 
	unsigned char* buffer, 
	size_t BUF_SIZE)
{

	#ifdef __APPLE__

		return load_xdr_from_file(output, filename);

	#else

	void* buf_head = static_cast<void*>(buffer);
	size_t aligned_buf_size = BUF_SIZE;
	unsigned char* aligned_buf = reinterpret_cast<unsigned char*>(std::align(512, sizeof(unsigned char), buf_head, aligned_buf_size));

	unique_fd fd{open(filename, O_DIRECT | O_RDONLY)};

	if (!fd) {
		return -1;
	}
	
	aligned_buf_size -= aligned_buf_size % 512;

	int bytes_read = read(fd.get(), aligned_buf, aligned_buf_size);

	if (bytes_read == -1) {
		threrror("read error");
	}

	if (static_cast<size_t>(bytes_read) >= aligned_buf_size) {
		throw std::runtime_error("buffer wasn't big enough");
	}
 	
 	xdr::xdr_get g(aligned_buf, aligned_buf + bytes_read);
 	xdr::xdr_argpack_archive(g, output);
 	g.done();
 	return 0;
 	
 	#endif
}

namespace {

static inline void 
flush_buffer(unique_fd& fd, unsigned char* buffer, size_t bytes_to_write) {
	std::size_t idx = 0;

	while (idx < bytes_to_write) {
		int written = write(fd.get(), buffer + idx, bytes_to_write - idx);
		if (written < 0) {
			threrror("write error");
		}
		idx += written;
	}
}

constexpr static auto FILE_PERMISSIONS = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
}
/*! Preallocate a file for later usage
*/
unique_fd static inline
preallocate_file(const char* filename, size_t size = 0) {

	#ifdef __APPLE__

	unique_fd fd{open(filename, O_CREAT | O_RDONLY, FILE_PERMISSIONS)};
	return fd;

	#else

	unique_fd fd{open(filename, O_CREAT | O_WRONLY | O_DIRECT, FILE_PERMISSIONS)};

	if (size == 0) {
		return fd;
	}
	auto res = fallocate(fd.get(), 0, 0, size);
	if (res) {
		threrror("fallocate");
	}
	return fd;

	#endif
}

/*! Save xdr object to disk.
    Slower than save_xdr_to_file_fast() because it 
    of serialization buffer allocation, but easier
    to use.
*/
template<typename xdr_type>
int __attribute__((warn_unused_result))
save_xdr_to_file(const xdr_type& value, const char* filename) {

	FILE* f = std::fopen(filename, "w");

	if (f == nullptr) {
		return -1;
	}

	auto buf = xdr::xdr_to_opaque(value);


	std::fwrite(buf.data(), sizeof(buf.data()[0]), buf.size(), f);
	std::fflush(f);

	fsync(fileno(f));

	std::fclose(f);
	return 0;
}


/*! Save an xdr object to disk
*/
template<typename xdr_list_type, unsigned int BUF_SIZE = 65535>
void save_xdr_to_file_fast(const xdr_list_type& value, const char* filename, const size_t prealloc_size = 64000000)  {
	unsigned char buffer[BUF_SIZE];
	auto fd = preallocate_file(filename, prealloc_size);
	save_xdr_to_file_fast<xdr_list_type>(value, fd, buffer, BUF_SIZE);
}

/*! Save an xdr object to disk, using preallocated file and
    an existing serialization buffer.
*/
template<typename xdr_list_type>
void save_xdr_to_file_fast(const xdr_list_type& value, unique_fd& fd, unsigned char* buffer, const unsigned int BUF_SIZE) {

	size_t list_size = value.size();

	unsigned int buf_idx = 0;

	void* buf_head = static_cast<void*>(buffer);

	size_t aligned_buf_size = BUF_SIZE;

	unsigned char* aligned_buf = reinterpret_cast<unsigned char*>(std::align(512, sizeof(unsigned char), buf_head, aligned_buf_size));

	uint32_t* aligned_buf_cast = reinterpret_cast<uint32_t*>(aligned_buf);

	aligned_buf_size -= (aligned_buf_size % 4);
	aligned_buf_size -= 512; // ensure space for last few bits


	std::size_t list_idx = 0;

	xdr::xdr_put p (aligned_buf, aligned_buf + aligned_buf_size);

	p.put32(aligned_buf_cast, xdr::size32(list_size));
	p.p_ ++; // needed because we're not using the typical p.operator() iface
	buf_idx += 4;

	size_t total_written_bytes = 0;

	while (list_idx < list_size) {
		
		size_t next_sz = xdr::xdr_argpack_size(value[list_idx]);

		if (aligned_buf_size - buf_idx < next_sz) {

			size_t write_amount = buf_idx - (buf_idx % 512);

			flush_buffer(fd, aligned_buf, write_amount);
			total_written_bytes += write_amount;
			memcpy(aligned_buf, aligned_buf + write_amount, buf_idx % 512);
			buf_idx %= 512;

			p.p_ = reinterpret_cast<uint32_t*>(aligned_buf + buf_idx);// reset
		}
		p(value[list_idx]);

		buf_idx += next_sz;

		list_idx ++;
	}


	size_t write_amount = buf_idx - (buf_idx % 512) + 512;
	flush_buffer(fd, (aligned_buf), write_amount);

	total_written_bytes += buf_idx;
	auto res = ftruncate(fd.get(), total_written_bytes);

	if (res) {
		threrror("ftruncate");
	}

	fsync(fd.get());
}

/*! fast method for saving account block.
    TODO determine whether it's actually faster than save_xdr_to_file_fast.
    Specialized for account blocks - automatically filters out everything except
    the actual transactions.  This leads to fewer bytes going to disk.
*/
[[maybe_unused]]
static void 
save_account_block_fast(
	const AccountModificationBlock& value, 
	unique_fd& fd, 
	unsigned char* buffer, 
	const unsigned int BUF_SIZE)
{

	unsigned int buf_idx = 0;

	void* buf_head = static_cast<void*>(buffer);

	size_t aligned_buf_size = BUF_SIZE;

	unsigned char* aligned_buf = reinterpret_cast<unsigned char*>(std::align(512, sizeof(unsigned char), buf_head, aligned_buf_size));

	uint32_t* aligned_buf_cast = reinterpret_cast<uint32_t*>(aligned_buf);

	aligned_buf_size -= (aligned_buf_size % 4);
	aligned_buf_size -= 512; // ensure space for last few bits

	size_t first_odirect_block_sz = 1024;
	uint8_t first_odirect_block[1024];
	bool first_set = false;

	void* first_head = static_cast<void*>(first_odirect_block);

	uint8_t* aligned_first = reinterpret_cast<uint8_t*>(
		std::align(512, sizeof(uint8_t), first_head, first_odirect_block_sz));
	uint32_t* aligned_first_reinterpreted = reinterpret_cast<uint32_t*>(aligned_first);


	size_t list_idx = 0;

	xdr::xdr_put p (aligned_buf, aligned_buf + aligned_buf_size);

	p.put32(aligned_buf_cast, xdr::size32(0)); //space for now
	p.p_ ++; // have to use if not using p.operator() mechanism, it seems
	buf_idx += 4;

	size_t total_written_bytes = 0;

	const xdr::xvector<SignedTransaction, MAX_SEQ_NUMS_PER_BLOCK>* tx_buffer = nullptr;
	size_t buffer_idx = 0;
	size_t num_written = 0;

	while (list_idx < value.size()) {

		while (tx_buffer == nullptr) {
			if (value[list_idx].new_transactions_self.size() != 0) {
				tx_buffer = &(value[list_idx].new_transactions_self);
				buffer_idx = 0;
			}
			list_idx ++;
			if (list_idx >= value.size()) {
				break;
			}
		}

		if (tx_buffer == nullptr) {
			break;
		}

		auto& next_tx_to_write = (*tx_buffer)[buffer_idx];

		size_t next_sz = xdr::xdr_argpack_size(next_tx_to_write);

		if (aligned_buf_size - buf_idx < next_sz) {

			size_t write_amount = buf_idx - (buf_idx % 512);

			if (!first_set) {
				first_set = true;
				memcpy(aligned_first, aligned_buf, 512);
			}

			flush_buffer(fd, aligned_buf, write_amount);
			total_written_bytes += write_amount;
			memcpy(aligned_buf, aligned_buf + write_amount, buf_idx % 512);
			buf_idx %= 512;

			p.p_ = reinterpret_cast<uint32_t*>(aligned_buf + buf_idx); // reset xdr_put obj
		}

		p(next_tx_to_write);
		num_written ++;
		buffer_idx++;
		if (buffer_idx >= tx_buffer -> size()) {
			tx_buffer = nullptr;
		}

		buf_idx += next_sz;
	}

	if (!first_set) {
		first_set = true;
		memcpy(aligned_first, aligned_buf, 512);
	}

	size_t write_amount = buf_idx - (buf_idx % 512) + 512;
	flush_buffer(fd, (aligned_buf), write_amount);

	total_written_bytes += buf_idx;



	auto res2 = lseek(fd.get(), 0, SEEK_SET);
	if (res2 != 0) {
		threrror("lseek");
	}

	p.put32(aligned_first_reinterpreted, xdr::size32(num_written));
	flush_buffer(fd, aligned_first, 512);

	auto res = ftruncate(fd.get(), total_written_bytes);

	if (res) {
		threrror("ftruncate");
	}
	
	fsync(fd.get());
}

//! make a new directory, does not throw error if dir already exists.
[[maybe_unused]]
static bool 
mkdir_safe(const char* dirname) {
	constexpr static auto mkdir_perms = S_IRWXU | S_IRWXG | S_IRWXO;

	auto res = mkdir(dirname, mkdir_perms);
	if (res == 0) {
		return false;
	}

	if (errno == EEXIST) {
		return true;
	}
	threrror(std::string("mkdir ") + std::string(dirname));
}

}