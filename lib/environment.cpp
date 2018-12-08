#include "environment.hpp"
#include "bytecode.hpp"
#include "parser.hpp"
#include "pool.hpp"
#include "vm.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace lisp {

ObjectPtr Environment::getGlobal(const std::string& key)
{
    // FIXME: this is pretty bad...
    ast::Vector<ast::StrVal> patterns = {key};
    auto loc = context_->astRoot_->find(patterns);
    return context_->topLevel_->load(loc);
}

void Environment::setGlobal(const std::string& key,
                            const std::string& nameSpace, ObjectPtr value)
{
    assert("FIXME, setGlobal unimplemented");
    assert(context_->astRoot_);
    // auto def = make_unique<ast::Def>();
    // auto obj = make_unique<ast::UserObject>(value);
    // def->name_ = key;
    // def->value_ = std::move(obj);
    // // FIXME: this is quite inefficient, to push a namespace ast node
    // // for every definition, but the alternatives are hacky at the
    // // moment.
    // auto newNs = make_unique<ast::Namespace>();
    // newNs->name_ = nameSpace;
    // newNs->statements_.push_back(std::move(def));
    // context_->astRoot_->statements_.push_back(std::move(newNs));
    // context_->astRoot_->statements_.back()->init(*context_->topLevel(),
    //                                              *context_->astRoot_);
    // BytecodeBuilder builder;
    // context_->astRoot_->statements_.back()->visit(builder);
    // FIXME:
    // context_->astRoot_->statements_.back()->execute(*this);
}

void Environment::setGlobal(const std::string& key, ObjectPtr value)
{
    assert("FIXME, setGlobal unimplemented");
    // assert(context_->astRoot_);
    // auto def = make_unique<ast::Def>();
    // auto obj = make_unique<ast::UserObject>(value);
    // def->name_ = key;
    // def->value_ = std::move(obj);
    // context_->astRoot_->stxatements_.push_back(std::move(def));
    // context_->astRoot_->statements_.back()->init(*context_->topLevel(),
    //                                              *context_->astRoot_);
    // BytecodeBuilder builder;
    // context_->astRoot_->statements_.back()->visit(builder);
    // FIXME:
    // context_->astRoot_->statements_.back()->execute(*this);
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
        assert(frame not_eq nullptr);
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
    "(defn list (...)\n"
    "  \"[...] -> construct a list from args in ...\"\n"
    "  ...) ;; Well that was easy :)\n"
    "\n"
    "(defn println (...)\n"
    "  \"[...] -> print each arg in ... with print, then write a line break\"\n"
    "  (apply print ...)\n"
    "  (newline))\n"
    "\n"
    "(defn caar (l) (car (car l)))\n"
    "(defn cadr (l) (car (cdr l)))\n"
    "(defn cdar (l) (cdr (car l)))\n"
    "(defn caddr (l) (car (cdr (cdr l))))\n"
    "(defn cadddr (l) (car (cdr (cdr (cdr l)))))\n";

Context::Context(const Configuration& config)
    : heap_(config.heapSize_, 0),
      topLevel_(std::allocate_shared<Environment>(PoolAllocator<Environment>{}, this, nullptr)),
      booleans_{{topLevel_->create<Boolean>(false)},
                {topLevel_->create<Boolean>(true)}},
      nullValue_{topLevel_->create<Null>()}, collector_{new MarkCompact}
{
    initBuiltins(*topLevel_);
    topLevel_->exec("");
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
        BytecodeBuilder builder;
        const size_t lastExecuted = context_->program_.size();
        // Splice and process each statement into the existing environment
        for (auto& st : root->statements_) {
            context_->astRoot_->statements_.push_back(std::move(st));
            context_->astRoot_->statements_.back()->init(*this,
                                                         *context_->astRoot_);
        }
        context_->astRoot_->visit(builder);
        VM vm;
        context_->program_ = builder.result();
        context_->callStack().push_back({0, 0, reference()});
        vm.execute(*this, context_->program_, lastExecuted);
    } else {
        root->init(*this, *root);
        // FIXME:
        // result = root->execute(*this);
        BytecodeBuilder builder;
        context_->astRoot_ = root.release();
        context_->astRoot_->visit(builder);
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
