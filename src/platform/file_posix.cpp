#ifndef _WIN32

#include "platform/file.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace openads::platform {

namespace {

util::Error os_error(const char* op) {
    util::Error e;
    e.code     = (errno == ENOENT) ? 5103 : 5000;
    e.sub_code = errno;
    e.message  = op;
    e.message += ": ";
    e.message += std::strerror(errno);
    return e;
}

intptr_t fd_from_native(void* p) {
    return reinterpret_cast<intptr_t>(p);
}
void* native_from_fd(int fd) {
    return reinterpret_cast<void*>(static_cast<intptr_t>(fd));
}

} // namespace

File::File(File&& other) noexcept : native_(other.native_) {
    other.native_ = nullptr;
}

File& File::operator=(File&& other) noexcept {
    if (this != &other) {
        close_();
        native_ = other.native_;
        other.native_ = nullptr;
    }
    return *this;
}

File::~File() { close_(); }

void File::close_() noexcept {
    if (native_ != nullptr) {
        ::close(static_cast<int>(fd_from_native(native_)));
        native_ = nullptr;
    }
}

util::Result<File> File::open(const std::string& path, OpenMode mode) {
    int flags = 0;
    switch (mode) {
        case OpenMode::ReadOnly:     flags = O_RDONLY; break;
        case OpenMode::ReadWrite:    flags = O_RDWR;   break;
        case OpenMode::CreateRW:     flags = O_RDWR | O_CREAT | O_TRUNC; break;
        case OpenMode::OpenExisting: flags = O_RDWR;   break;
    }
    int fd = ::open(path.c_str(), flags, 0644);
    if (fd < 0) return os_error("open");
    return File{native_from_fd(fd)};
}

util::Result<std::size_t> File::read_at(std::uint64_t offset,
                                        void* buf, std::size_t n) {
    int fd = static_cast<int>(fd_from_native(native_));
    ssize_t got = ::pread(fd, buf, n, static_cast<off_t>(offset));
    if (got < 0) return os_error("pread");
    return static_cast<std::size_t>(got);
}

util::Result<std::size_t> File::write_at(std::uint64_t offset,
                                         const void* buf, std::size_t n) {
    int fd = static_cast<int>(fd_from_native(native_));
    ssize_t wrote = ::pwrite(fd, buf, n, static_cast<off_t>(offset));
    if (wrote < 0) return os_error("pwrite");
    return static_cast<std::size_t>(wrote);
}

util::Result<std::uint64_t> File::size() const {
    int fd = static_cast<int>(fd_from_native(native_));
    struct stat st{};
    if (::fstat(fd, &st) != 0) return os_error("fstat");
    return static_cast<std::uint64_t>(st.st_size);
}

util::Result<void> File::sync() {
    int fd = static_cast<int>(fd_from_native(native_));
    if (::fsync(fd) != 0) return os_error("fsync");
    return {};
}

util::Result<void> File::truncate(std::uint64_t size) {
    int fd = static_cast<int>(fd_from_native(native_));
    if (::ftruncate(fd, static_cast<off_t>(size)) != 0)
        return os_error("ftruncate");
    return {};
}

} // namespace openads::platform

#endif // !_WIN32
