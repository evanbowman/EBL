#include "environment.hpp"
#include "bytecode.hpp"
#include "parser.hpp"
#include "pool.hpp"
#include "vm.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <cassert>

namespace lisp {

ObjectPtr Environment::getGlobal(const std::string& key)
{
    auto loc = context_->astRoot_->find(key).varLoc_;
    return context_->topLevel_->load(loc);
}

static ast::Ptr<ast::Def> makeUoDef(Context* ctx, const std::string& key, ObjectPtr value)
{
    const auto id = ctx->immediates().size();
    ctx->immediates().push_back(value);
    auto def = make_unique<ast::Def>();
    auto obj = make_unique<ast::UserObject>(id);
    def->name_ = key;
    def->value_ = std::move(obj);
    return def;
}

void Environment::setGlobal(const std::string& key,
                            const std::string& nameSpace, ObjectPtr value)
{
    assert(context_->astRoot_);
    auto newNs = make_unique<ast::Namespace>();
    newNs->name_ = nameSpace;
    newNs->statements_.push_back(makeUoDef(context_, key, value));
    context_->astRoot_->statements_.push_back(std::move(newNs));
    context_->astRoot_->statements_.back()->init(context_->topLevel(),
                                                 *context_->astRoot_);
    BytecodeBuilder builder;
    const size_t lastExecuted = context_->program_.size();
    context_->astRoot_->statements_.back()->visit(builder);

    auto newCode = builder.result();
    std::copy(newCode.begin(), newCode.end(), std::back_inserter(context_->program_));
    context_->callStack().push_back({0, 0, context_->topLevel().reference()});
    VM::execute(*context_->topLevel_, context_->program_, lastExecuted);
    context_->callStack().pop_back();

}

void Environment::setGlobal(const std::string& key, ObjectPtr value)
{
    assert(context_->astRoot_);
    context_->astRoot_->statements_.push_back(makeUoDef(context_, key, value));
    context_->astRoot_->statements_.back()->init(context_->topLevel(),
                                                 *context_->astRoot_);
    BytecodeBuilder builder;
    const size_t lastExecuted = context_->program_.size();
    context_->astRoot_->statements_.back()->visit(builder);
    builder.unusedExpr();
    auto newCode = builder.result();
    std::copy(newCode.begin(), newCode.end(), std::back_inserter(context_->program_));
    context_->callStack().push_back({0, 0, context_->topLevel().reference()});
    VM::execute(*context_->topLevel_, context_->program_, lastExecuted);
    context_->callStack().pop_back();
}

void Environment::push(ObjectPtr value)
{
    vars_.push_back(value);
}

Environment& Environment::getFrame(VarLoc loc)
{
    // The compiler will have already validated variable offsets, so there's
    // no need to check out of bounds access.
    auto frame = reference();
    while (loc.frameDist_ > 0) {
        frame = frame->parent_;
        loc.frameDist_--;
    }
    return *frame;
}

ObjectPtr Environment::load(VarLoc loc)
{
    return getFrame(loc).vars_[loc.offset_];
}

void Environment::store(VarLoc loc, ObjectPtr value)
{
    getFrame(loc).vars_[loc.offset_] = value;
}

ObjectPtr Environment::getNull()
{
    return context_->nullValue_;
}

ObjectPtr Environment::getBool(bool trueOrFalse)
{
    return context_->booleans_[trueOrFalse];
}

EnvPtr Environment::derive()
{
    PoolAllocator<Environment> alloc;
    return std::allocate_shared<Environment>(alloc, context_, reference());
}

EnvPtr Environment::parent()
{
    return parent_;
}

EnvPtr Environment::reference()
{
    return shared_from_this();
}

void initBuiltins(Environment& env);


const Context::Configuration& Context::defaultConfig()
{
    static const Configuration defaults{
        10000000 // Ten megabyte heap
    };
    return defaults;
}

const char* utilities =
    "\n"
    "(namespace std\n"
    "  (defn some (pred lat)\n"
    "    \"[pred list] -> first list element that satisfies pred, otherwise false\"\n"
    "    (if (null? lat)\n"
    "        false\n"
    "        (if (pred (car lat))\n"
    "            (car lat)\n"
    "            (recur pred (cdr lat))))))\n"
    "\n"
    "(def require\n"
    "     ((lambda ()\n"
    "        (def-mut required-set null)\n"
    "        (lambda (file-name)\n"
    "          (let ((found (std::some (lambda (n)\n"
    "                                    (equal? n file-name)) required-set)))\n"
    "            (if found\n"
    "                null\n"
    "                (begin\n"
    "                  (load file-name)\n"
    "                  (set required-set (cons file-name required-set)))))))))\n"
    "";

Context::Context(const Configuration& config)
    : heap_(config.heapSize_, 0),
      topLevel_(std::allocate_shared<Environment>(PoolAllocator<Environment>{}, this, nullptr)),
      booleans_{{topLevel_->create<Boolean>(false)},
                {topLevel_->create<Boolean>(true)}},
      nullValue_{topLevel_->create<Null>()}, collector_{new MarkCompact}
{
    initBuiltins(*topLevel_);
    callStack_.push_back({0, 0, topLevel_});
    topLevel_->exec(utilities);
}

Context::~Context()
{
    if (astRoot_) {
        std::ofstream out("save.lisp");
        out << std::fixed << std::setprecision(15);
        // astRoot_->store(out); FIXME
    }
    // For debugging
    std::ofstream bc("bc", std::ofstream::binary);
    bc.write((const char*)program_.data(), program_.size());
 }

ObjectPtr Context::loadI(ImmediateId immediate)
{
    return immediates_[immediate];
}

Context* Environment::getContext()
{
    return context_;
}

ObjectPtr Environment::exec(const std::string& code)
{
    auto root = lisp::parse(code);
    auto result = getNull();
    if (context_->astRoot_) {
        for (auto& st : root->statements_) {
            BytecodeBuilder builder;
            const size_t lastExecuted = context_->program_.size();
            // Splice and process each statement into the existing environment
            context_->astRoot_->statements_.push_back(std::move(st));
            context_->astRoot_->statements_.back()->init(*context_->topLevel_,
                                                         *context_->astRoot_);
            context_->astRoot_->statements_.back()->visit(builder);
            auto newCode = builder.result();
            std::copy(newCode.begin(), newCode.end(), std::back_inserter(context_->program_));
            context_->callStack().push_back({0, 0, context_->topLevel_});
            VM::execute(*context_->topLevel_, context_->program_, lastExecuted);
            context_->callStack().pop_back();
            result = context_->operandStack().back();
            context_->operandStack().pop_back();
        }
    } else {
        root->init(*this, *root);
        BytecodeBuilder builder;
        context_->astRoot_ = root.release();
        context_->astRoot_->visit(builder);
        context_->program_ = builder.result();
        VM::execute(*context_->topLevel_, context_->program_, 0);
    }
    return result;
}


void Environment::openDLL(const std::string& name)
{
    DLL dll(name.c_str());
    auto sym = (void (*)(Environment&))dll.sym("__dllMain");
    if (sym) {
        sym(*this);
        context_->dlls_.push_back(std::move(dll));
    } else {
        throw std::runtime_error("symbol __dllMain lookup failed");
    }
}


} // namespace lisp
