#include "gc.hpp"
#include "environment.hpp"
#include "memory.hpp"
#include <iostream>
#include <deque>

namespace lisp {

void markFrame(Environment& frame);

void markObject(ObjectPtr obj)
{
    if (obj->marked()) {
        return;
    }
    obj->mark();
    switch (obj->typeId()) {
    case typeInfo.typeId<Pair>():
        markObject(obj.cast<Pair>()->getCar());
        markObject(obj.cast<Pair>()->getCdr());
        break;

    case typeInfo.typeId<Function>():
        markFrame(*obj.cast<Function>()->definitionEnvironment());
        break;
    }
}

void markFrame(Environment& frame)
{
    for (auto& obj : frame.getVars()) {
        markObject(obj);
    }
}

void MarkCompact::mark(Environment& env)
{
    for (auto& frame : env.getContext()->callStack()) {
        markFrame(*frame);
    }
    for (auto& obj : env.getContext()->immediates()) {
        markObject(obj);
    }
    env.getBool(true)->mark();
    env.getBool(false)->mark();
    env.getNull()->mark();
}

void MarkCompact::compact(Environment& env, Heap& heap)
{
    uint8_t* mem = heap.begin();
    size_t index = 0;
    size_t collectCount = 0;
    while (index < heap.size()) {
        auto current = (Object*)(mem + index);
        if (current->marked()) {
            std::cout << "live object "
                      << typeInfo[current->typeId()].name_
                      << " @ " << current
                      << std::endl;
        } else {
            ++collectCount;
        }
        index += typeInfo[current->typeId()].size_;
    }
    std::cout << "collected: " << collectCount << std::endl;
}


void MarkCompact::run(Environment& env, Heap& heap)
{
    mark(env);
    compact(env, heap);
}

} // namespace lisp
