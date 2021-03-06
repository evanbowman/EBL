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
#include "ebl.hpp"

namespace ebl {

ValuePtr Environment::getGlobal(const std::string& key)
{
    auto loc = context_->astRoot_->find(key).varLoc_;
    return context_->topLevel_->load(loc);
}

static ast::Ptr<ast::Def> makeUoDef(Context* ctx, const std::string& key,
                                    ValuePtr value)
{
    const auto id = ctx->immediates().size();
    ctx->immediates().push_back(value);
    auto def = make_unique<ast::Def>();
    auto val = make_unique<ast::UserValue>(id);
    def->name_ = key;
    def->value_ = std::move(val);
    return def;
}

void Environment::setGlobal(const std::string& key,
                            const std::string& nameSpace, ValuePtr value)
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
    std::copy(newCode.begin(), newCode.end(),
              std::back_inserter(context_->program_));
    context_->callStack().push_back({0, 0, context_->topLevel().reference()});
    VM::execute(*context_->topLevel_, context_->program_, lastExecuted);
    context_->callStack().pop_back();
}

void Environment::setGlobal(const std::string& key, ValuePtr value)
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
    std::copy(newCode.begin(), newCode.end(),
              std::back_inserter(context_->program_));
    context_->callStack().push_back({0, 0, context_->topLevel().reference()});
    VM::execute(*context_->topLevel_, context_->program_, lastExecuted);
    context_->callStack().pop_back();
}

void Environment::push(ValuePtr value)
{
    vars_.push_back(value);
}

void Environment::clear()
{
    vars_.clear();
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

ValuePtr Environment::load(VarLoc loc)
{
    return getFrame(loc).vars_[loc.offset_];
}

void Environment::store(VarLoc loc, ValuePtr value)
{
    getFrame(loc).vars_[loc.offset_] = value;
}

ValuePtr Environment::getNull()
{
    return context_->nullValue_;
}

ValuePtr Environment::getBool(bool trueOrFalse)
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

#include "onloads.hpp"

Context::Context(const Configuration& config)
    : heap_(config.heapSize_, 0),
      topLevel_(std::allocate_shared<Environment>(PoolAllocator<Environment>{},
                                                  this, nullptr)),
      booleans_{{topLevel_->create<Boolean>(false)},
                {topLevel_->create<Boolean>(true)}},
      nullValue_{topLevel_->create<Null>()}, collector_{new MarkCompact},
      persistentsList_(nullptr)
{
    topLevel_->exec("");
    initBuiltins(*topLevel_);
    callStack_.push_back({0, 0, topLevel_});
    topLevel_->exec(onloads);
}

Context::~Context()
{
}

void Context::writeToFile(const std::string& fname)
{
    std::ofstream bc(fname, std::ofstream::binary);

    // bc << "@Section:Immediates\n";
    for (auto& val : immediates_) {
        bc << val->typeId();
        print(*topLevel_, val, bc, false);
        bc << "\n";
    }

    bc << "@Section:Program\n";
    std::cout << immediates_.size() << std::endl;
    bc.write((const char*)program_.data(), program_.size());
}

void Context::loadFromFile(const std::string& fname)
{
    operandStack_.clear();
    callStack_.clear();
    topLevel_->clear();


    std::ifstream bc(fname, std::ifstream::binary);
    std::string line;

    size_t count = 0;
    while (std::getline(bc, line)) {
        if (line == "@Section:Program") {
            break;
        }

        // FIXME: First N immediates are builtin functions...
        if (++count < 126) {
            continue;
        }

        TypeId id(line[0]);

        switch (id) {
        case typeId<Integer>(): {
            std::string value = line.substr(1);
            storeI<Integer>(*this, std::stoi(value));
        } break;

        case typeId<String>(): {
            std::string value = line.substr(1);
            storeI<String>(*this, value);
        } break;

        default:
            std::cout << "garbled" << std::endl;
        }
    }

    program_ = Bytecode(std::istreambuf_iterator<char>(bc),
                        std::istreambuf_iterator<char>());

    callStack_.push_back({0, 0, topLevel_});

    size_t ip = 0;
    while (ip < program_.size()) {
        ip = VM::execute(*topLevel_, program_, ip) + 1;
    }
}

Context* Environment::getContext()
{
    return context_;
}

ValuePtr Environment::exec(const std::string& code)
{
    auto root = ebl::parse(code);
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
            std::copy(newCode.begin(), newCode.end(),
                      std::back_inserter(context_->program_));
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


} // namespace ebl
