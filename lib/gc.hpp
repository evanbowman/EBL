#pragma once

namespace lisp {

class Environment;
class Heap;

class GC {
public:
    virtual void run(Environment& env, Heap& heap) = 0;
    virtual ~GC()
    {
    }
};

class MarkCompact : public GC {
public:
    void run(Environment& env, Heap& heap) override;
    void mark(Environment& env);
    void compact(Environment& env, Heap& heap);
};

} // namespace lisp
