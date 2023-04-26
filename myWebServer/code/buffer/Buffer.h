#pragma once

#include <string>
#include <cstring>  // perror
#include <vector>
#include <atomic>
#include <unistd.h>   // wirte
#include <cassert>
#include <sys/uio.h>

class Buffer {
public:
    Buffer(int buf_size = 1024);
    ~Buffer() = default;

    std::size_t readable_bytes() const;
    std::size_t writable_bytes() const ;
    std::size_t prependable_bytes() const;

    const char* peek() const;
    void retrieve(std::size_t len);
    void retrieve_until(const char* end);
    void retrieve_all();
    std::string retrieve_all_to_str();

    const char* begin_write() const;
    char* begin_write();

    void ensure_writeable(std::size_t len);
    void has_written(std::size_t len);

    void append(const char* str, std::size_t len);
    void append(const std::string& str);
    void append(const void* data, std::size_t len);
    void append(const Buffer& buff);

    ssize_t read_fd(int fd, int* save_errno);
    ssize_t write_fd(int fd, int* save_errno);

private:
    char* begin_ptr();
    const char* begin_ptr() const;
    void make_space(std::size_t len);

    std::vector<char> buffer_;  // 具体装数据的vector
    std::atomic<std::size_t> read_pos_;     // 读的位置
    std::atomic<std::size_t> write_pos_;       // 写的位置， 写是用来将 fd 中读到的数据写到 buffer_ 中，即从 fd 中读取数据到 buffer_ 中
};