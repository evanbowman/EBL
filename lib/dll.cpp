#include "dll.hpp"
#include <stdexcept>
#if defined(__linux__) or defined(__APPLE__)
#define __UNIX__
#endif

#ifdef __UNIX__
#include <dlfcn.h>
#endif

namespace lisp {

DLL::DLL(const char* name)
{
#ifdef __UNIX__
    handle_ = dlopen(name, RTLD_LAZY);
    if (not handle_) {
        throw std::runtime_error("failed to load DLL");
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


DLL::~DLL()
{
#ifdef __UNIX__
    dlclose(handle_);
#endif
}


} // namespace lisp
