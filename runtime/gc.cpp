#include "gc.hpp"
#include "environment.hpp"
#include "memory.hpp"
#include "persistent.hpp"
#include <deque>
#include <set>

// FIXME: This code could use a good deal of work! On the one hand,
// it's a reasonably performant mark/compact collector in less than
// 200 lines of code, on the other hand, it could be a lot better.

namespace ebl {

static void markFrame(Environment& frame);

static void markValue(ValuePtr val)
{
    if (val->marked()) {
        return;
    }
    val->mark();
    switch (val->typeId()) {
    case typeId<Pair>():
        markValue(val.cast<Pair>()->getCar());
        markValue(val.cast<Pair>()->getCdr());
        break;

    case typeId<Function>():
        markFrame(*val.cast<Function>()->definitionEnvironment());
        markValue(val.cast<Function>()->getDocstring());
        break;

    case typeId<Symbol>():
        markValue(val.cast<Symbol>()->value());
        break;

    case typeId<Box>():
        markValue(val.cast<Box>()->get());
        break;
    }
}

static void markFrame(Environment& frame)
{
    for (auto& val : frame.getVars()) {
        markValue(val);
    }
}

void MarkCompact::mark(Environment& env)
{
    for (auto& frameInfo : env.getContext()->callStack()) {
        markFrame(*frameInfo.env_);
    }
    for (auto& val : env.getContext()->immediates()) {
        markValue(val);
    }
    for (auto& val : env.getContext()->operandStack()) {
        markValue(val);
    }
    auto plist = env.getContext()->getPersistentsList();
    while (plist) {
        markValue(plist->getUntypedVal());
        plist = plist->next();
    }
    env.getBool(true)->mark();
    env.getBool(false)->mark();
    env.getNull()->mark();
}

using BreakList = std::vector<std::pair<Value*, size_t>>;

static void* remapValueAddress(void* val, const BreakList& breaks)
{
    size_t shiftAmount = 0;
    auto iter = breaks.begin();
    while (iter not_eq breaks.end() and iter->first < val) {
        shiftAmount += iter->second;
        ++iter;
    }
    return (uint8_t*)val - shiftAmount;
}

// Frames must be visited exactly once! FIXME: but the var shouldn't be global.
thread_local std::set<Environment*> frameSet;

static void remapFrame(Environment& frame, const BreakList& breaks)
{
    for (auto& val : frame.getVars()) {
        val.UNSAFE_overwrite(remapValueAddress(val.handle(), breaks));
    }
}

static void gatherFrames(Environment& env)
{
    auto current = env.reference();
    while (current) {
        frameSet.insert(current.get());
        current = current->parent();
    }
}

static void remapInternalPointers(Value* val, const BreakList& breaks)
{
    switch (val->typeId()) {
    case typeId<Pair>(): {
        auto p = (Pair*)val;
        auto car = p->getCar();
        auto cdr = p->getCdr();
        car.UNSAFE_overwrite(remapValueAddress(car.handle(), breaks));
        cdr.UNSAFE_overwrite(remapValueAddress(cdr.handle(), breaks));
        p->setCar(car);
        p->setCdr(cdr);
    } break;

    case typeId<Symbol>(): {
        auto s = (Symbol*)val;
        auto val = s->value();
        val.UNSAFE_overwrite(remapValueAddress(val.handle(), breaks));
        s->set(val);
    } break;

    case typeId<Box>(): {
        auto b = (Box*)val;
        auto val = b->get();
        val.UNSAFE_overwrite(remapValueAddress(val.handle(), breaks));
        b->set(val);
    } break;

    case typeId<Function>(): {
        auto f = (Function*)val;
        auto doc = f->getDocstring();
        doc.UNSAFE_overwrite(remapValueAddress(doc.handle(), breaks));
        f->setDocstring(doc);
        gatherFrames(*f->definitionEnvironment());
        break;
    }
    }
}

void MarkCompact::compact(Environment& env, Heap& heap)
{
    BreakList breakList;
    size_t bytesCompacted = 0;
    size_t index = 0;
    bool collapse = false; // collapse consecutive vals
    while (index < heap.size()) {
        auto current = (Value*)(heap.begin() + index);
        const size_t currentSize = typeInfo(current).size_;
        if (current->marked()) {
        PRESERVE:
            current->unmark();
            collapse = false;
            if (bytesCompacted) {
                uint8_t* const dest = ((uint8_t*)current) - bytesCompacted;
                typeInfo(current).relocatePolicy(current, dest);
            }
        } else {
            if (isType<String>(current)) {
                auto s = (String*)current;
                for (size_t i = 0; i < s->length(); ++i) {
                    // If there are any outstanding references to the characters
                    // belonging to the string, we can't collect it yet.
                    if ((*s)[i]->marked())
                        goto PRESERVE;
                }
            }
            if (not collapse) {
                breakList.push_back({current, currentSize});
                collapse = true;
            } else {
                breakList.back().second += currentSize;
            }
            bytesCompacted += currentSize;
        }
        index += currentSize;
    }
    heap.compacted(bytesCompacted);
    index = 0;
    while (index < heap.size()) {
        auto current = (Value*)(heap.begin() + index);
        const size_t currentSize = typeInfo(current).size_;
        remapInternalPointers(current, breakList);
        index += currentSize;
    }
    for (auto& frameInfo : env.getContext()->callStack()) {
        gatherFrames(*frameInfo.env_);
    }
    for (auto& frame : frameSet) {
        remapFrame(*frame, breakList);
    }
    frameSet.clear();
    for (auto& val : env.getContext()->immediates()) {
        auto target = remapValueAddress(val.handle(), breakList);
        val.UNSAFE_overwrite(target);
    }
    for (auto& val : env.getContext()->operandStack()) {
        auto target = remapValueAddress(val.handle(), breakList);
        val.UNSAFE_overwrite(target);
    }
    auto plist = env.getContext()->getPersistentsList();
    while (plist) {
        auto val = plist->getUntypedVal();
        auto target = remapValueAddress(val.handle(), breakList);
        val.UNSAFE_overwrite(target);
        plist->UNSAFE_overwrite(val);
        plist = plist->next();
    }
}


void MarkCompact::run(Environment& env, Heap& heap)
{
    mark(env);
    compact(env, heap);
}

} // namespace ebl
