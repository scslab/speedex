#include "cleanup.h"

#include <cerrno>
#include <system_error>

namespace speedex {

void
threrror(const char *msg)
{
  throw std::system_error(errno, std::system_category(), msg);
}

void
threrror(const std::string msg)
{
  throw std::system_error(errno, std::system_category(), std::move(msg));
}

}
