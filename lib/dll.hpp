#pragma once

namespace lisp {

class DLL {
public:
    DLL(const char* path);
    DLL(const DLL& dll) = delete;
    DLL(DLL&&) = default;
    DLL& operator=(DLL&&) = default;
    ~DLL();

    void* sym(const char* name);

private:
    void* handle_;
};

} // namespace lisp
