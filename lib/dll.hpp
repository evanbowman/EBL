#pragma once

namespace lisp {

class DLL {
public:
    DLL(const char* path);
    DLL(const DLL& dll) = delete;
    DLL(DLL&&) noexcept;
    DLL& operator=(DLL&&) noexcept;
    ~DLL();

    void* sym(const char* name);

private:
    void* handle_;
};

} // namespace lisp
