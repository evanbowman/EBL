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

static void markObject(ObjectPtr obj)
{
    if (obj->marked()) {
        return;
    }
    obj->mark();
    switch (obj->typeId()) {
    case typeId<Pair>():
        markObject(obj.cast<Pair>()->getCar());
        markObject(obj.cast<Pair>()->getCdr());
        break;

    case typeId<Function>():
        markFrame(*obj.cast<Function>()->definitionEnvironment());
        markObject(obj.cast<Function>()->getDocstring());
        break;

    case typeId<Symbol>():
        markObject(obj.cast<Symbol>()->value());
        break;
    }
}

static void markFrame(Environment& frame)
{
    for (auto& obj : frame.getVars()) {
        markObject(obj);
    }
}

void MarkCompact::mark(Environment& env)
{
    for (auto& frameInfo : env.getContext()->callStack()) {
        markFrame(*frameInfo.env_);
    }
    for (auto& obj : env.getContext()->immediates()) {
        markObject(obj);
    }
    for (auto& obj : env.getContext()->operandStack()) {
        markObject(obj);
    }
    auto plist = env.getContext()->getPersistentsList();
    while (plist) {
        markObject(plist->getUntypedObj());
        plist = plist->next();
    }
    env.getBool(true)->mark();
    env.getBool(false)->mark();
    env.getNull()->mark();
}

using BreakList = std::vector<std::pair<Object*, size_t>>;

static void* remapObjectAddress(void* obj, const BreakList& breaks)
{
    size_t shiftAmount = 0;
    auto iter = breaks.begin();
    while (iter not_eq breaks.end() and iter->first < obj) {
        shiftAmount += iter->second;
        ++iter;
    }
    return (uint8_t*)obj - shiftAmount;
}

// Frames must be visited exactly once! FIXME: but the var shouldn't be global.
thread_local std::set<Environment*> frameSet;

static void remapFrame(Environment& frame, const BreakList& breaks)
{
    for (auto& obj : frame.getVars()) {
        obj.UNSAFE_overwrite(remapObjectAddress(obj.handle(), breaks));
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

static void remapInternalPointers(Object* obj, const BreakList& breaks)
{
    switch (obj->typeId()) {
    case typeId<Pair>(): {
        auto p = (Pair*)obj;
        auto car = p->getCar();
        auto cdr = p->getCdr();
        car.UNSAFE_overwrite(remapObjectAddress(car.handle(), breaks));
        cdr.UNSAFE_overwrite(remapObjectAddress(cdr.handle(), breaks));
        p->setCar(car);
        p->setCdr(cdr);
    } break;

    case typeId<Symbol>(): {
        auto s = (Symbol*)obj;
        auto val = s->value();
        val.UNSAFE_overwrite(remapObjectAddress(val.handle(), breaks));
        s->set(val);
    } break;

    case typeId<Function>(): {
        auto f = (Function*)obj;
        auto doc = f->getDocstring();
        doc.UNSAFE_overwrite(remapObjectAddress(doc.handle(), breaks));
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
    bool collapse = false; // collapse consecutive objs
    while (index < heap.size()) {
        auto current = (Object*)(heap.begin() + index);
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
        auto current = (Object*)(heap.begin() + index);
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
    for (auto& obj : env.getContext()->immediates()) {
        auto target = remapObjectAddress(obj.handle(), breakList);
        obj.UNSAFE_overwrite(target);
    }
    for (auto& obj : env.getContext()->operandStack()) {
        auto target = remapObjectAddress(obj.handle(), breakList);
        obj.UNSAFE_overwrite(target);
    }
    auto plist = env.getContext()->getPersistentsList();
    while (plist) {
        auto obj = plist->getUntypedObj();
        auto target = remapObjectAddress(obj.handle(), breakList);
        obj.UNSAFE_overwrite(target);
        plist->UNSAFE_overwrite(obj);
        plist = plist->next();
    }
}


void MarkCompact::run(Environment& env, Heap& heap)
{
    mark(env);
    compact(env, heap);
}

} // namespace ebl
