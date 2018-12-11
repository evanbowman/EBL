#pragma once

#include "utility.hpp"
#include <functional>
#include <limits>
#include <map>
#include <new>
#include <string>
#include <tuple>
#include <vector>

#include "common.hpp"
#include "dll.hpp"
#include "extlib/smallVector.hpp"
#include "gc.hpp"
#include "memory.hpp"
#include "types.hpp"
#include "vm.hpp"


namespace lisp {

class Context;

class Environment;
using EnvPtr = std::shared_ptr<Environment>;

class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment(Context* context, EnvPtr parent)
        : context_(context), parent_(parent)
    {
    }
    Environment(const Environment&) = delete;

    template <typename T, typename... Args> Heap::Ptr<T> create(Args&&... args);

    // Load/store a variable in the root environment.
    ObjectPtr getGlobal(const std::string& key);
    void setGlobal(const std::string& key, ObjectPtr value);
    void setGlobal(const std::string& key, const std::string& nameSpace,
                   ObjectPtr value);

    // Compile and execute lisp code
    ObjectPtr exec(const std::string& code);

    ObjectPtr getNull();
    ObjectPtr getBool(bool trueOrFalse);

    // For storing and loading from an environment's stack. Only meant to be
    // called by the runtime or the vm.
    void push(ObjectPtr value);
    void store(VarLoc loc, ObjectPtr value);
    ObjectPtr load(VarLoc loc);

    void openDLL(const std::string& name);

    EnvPtr derive();
    EnvPtr parent();
    EnvPtr reference();

    Context* getContext();

    using Variables = Ogre::SmallVector<ObjectPtr, 6>;
    Variables& getVars()
    {
        return vars_;
    }

private:
    Environment& getFrame(VarLoc loc);

    Context* context_;
    EnvPtr parent_;
    Variables vars_;
};

template <typename T> struct ConstructImpl {
    template <typename... Args>
    static void construct(T* mem, Environment& env, Args&&... args)
    {
        new (mem) T{std::forward<Args>(args)...};
    }
};

template <> struct ConstructImpl<Function> {
    template <typename... Args>
    static void construct(Function* mem, Environment& env, Args&&... args)
    {
        new (mem) Function{env, std::forward<Args>(args)...};
    }
};

std::vector<std::string> getBuiltinList();

namespace ast {
struct TopLevel;
}

class Context {
public:
    struct Configuration {
        size_t heapSize_;
    };

    Context(const Configuration& config = defaultConfig());
    Context(const Context&) = delete;
    ~Context();

    std::vector<ObjectPtr>& immediates()
    {
        return immediates_;
    }

    std::vector<ObjectPtr>& operandStack()
    {
        return operandStack_;
    }

    // For storing and loading intern'd immediates
    ObjectPtr loadI(ImmediateId immediate);
    template <typename T> ImmediateId storeI(const typename T::Input& val)
    {
        const auto ret = immediates_.size();
        for (size_t i = 0; i < immediates_.size(); ++i) {
            if (isType<T>(immediates_[i])) {
                if (immediates_[i].cast<T>()->value() == val) {
                    return i;
                }
            }
        }
        immediates_.push_back(topLevel_->create<T>(val));
        return ret;
    }

    Environment& topLevel()
    {
        return *topLevel_;
    }

    friend class Environment;

    using CallStack = std::vector<StackFrame>;
    CallStack& callStack()
    {
        return callStack_;
    }

    void runGC(Environment& env)
    {
        collector_->run(env, heap_);
    }

    const Bytecode& getProgram() const
    {
        return program_;
    }

private:
    template <typename T, typename... Args>
    Heap::Ptr<T> create(Environment& env, Args&&... args)
    {
        auto allocObj = [&] {
            return heap_.alloc<typeInfoTable.get<T>().size_>().template cast<T>();
        };
        auto mem = alloc<T>(env, allocObj);
        ConstructImpl<T>::construct(mem.get(), env,
                                    std::forward<Args>(args)...);
        return mem;
    }

    template <typename T, typename F>
    Heap::Ptr<T> alloc(Environment& env, F&& allocImpl)
    {
        try {
            return allocImpl();
        } catch (const Heap::OOM& oom) {
            runGC(env);
            return allocImpl();
        }
    }

    static const Configuration& defaultConfig();

    Heap heap_;
    EnvPtr topLevel_;
    Heap::Ptr<Boolean> booleans_[2];
    Heap::Ptr<Null> nullValue_;
    std::vector<ObjectPtr> immediates_;
    std::vector<ObjectPtr> operandStack_;
    std::vector<DLL> dlls_;
    ast::TopLevel* astRoot_ = nullptr;
    Bytecode program_;
    std::unique_ptr<GC> collector_;
    CallStack callStack_;
};

template <typename T, typename... Args>
Heap::Ptr<T> Environment::create(Args&&... args)
{
    return context_->create<T>(*this, std::forward<Args>(args)...);
}

} // namespace lisp
