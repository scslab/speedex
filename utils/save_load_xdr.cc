#include "utils/save_load_xdr.h"

namespace speedex
{

constexpr static auto FILE_PERMISSIONS
    = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

unique_fd
preallocate_file(const char* filename, size_t size)
{
#ifdef __APPLE__

    unique_fd fd{ open(filename, O_CREAT | O_RDONLY, FILE_PERMISSIONS) };
    return fd;

#else

    unique_fd fd{ open(
        filename, O_CREAT | O_WRONLY | O_DIRECT, FILE_PERMISSIONS) };

    if (size == 0)
    {
        return fd;
    }
    auto res = fallocate(fd.get(), 0, 0, size);
    if (res)
    {
        threrror("fallocate");
    }
    return fd;

#endif
}

//! make a new directory, does not throw error if dir already exists.
bool
mkdir_safe(const char* dirname)
{
    constexpr static auto mkdir_perms = S_IRWXU | S_IRWXG | S_IRWXO;

    auto res = mkdir(dirname, mkdir_perms);
    if (res == 0)
    {
        return false;
    }

    if (errno == EEXIST)
    {
        return true;
    }
    threrror(std::string("mkdir ") + std::string(dirname));
}

} // namespace speedex