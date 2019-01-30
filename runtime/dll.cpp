#include "dll.hpp"
#include <stdexcept>
#include <string>
#if defined(__linux__) or defined(__APPLE__)
#define __UNIX__
#endif

#ifdef __UNIX__
#include <dlfcn.h>
#endif

namespace ebl {

DLL::DLL(const char* name)
{
#ifdef __UNIX__
    handle_ = dlopen(name, RTLD_LAZY);
    if (not handle_) {
        throw std::runtime_error("failed to load DLL " + std::string(name));
    }
#else
    throw std::runtime_error("DLLs unimplemented for your platform");
#endif
}


void* DLL::sym(const char* name)
{
#ifdef __UNIX__
    return dlsym(handle_, name);
#endif
}


DLL::DLL(DLL&& from) noexcept
{
    this->handle_ = from.handle_;
    from.handle_ = nullptr;
}


DLL& DLL::operator=(DLL&& from) noexcept
{
    this->handle_ = from.handle_;
    from.handle_ = nullptr;
    return *this;
}


DLL::~DLL()
{
#ifdef __UNIX__
    if (handle_) {
        dlclose(handle_);
        handle_ = nullptr;
    }
#endif
}


} // namespace ebl
