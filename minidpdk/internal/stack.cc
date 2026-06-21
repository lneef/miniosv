#include <minidpdk/internal/stack.hh>

#include <osv/irqlock.hh>
#include <osv/sched.hh>

unsigned int stack::push(void *const *obj_table, unsigned int n) {
    irq_save_lock_type irq_lock;
    WITH_LOCK(irq_lock) {
        if (capacity - head.load(std::memory_order_relaxed) < n) [[unlikely]]
            return 0;
        for (unsigned i = 0; i < n; ++i)
            objs[head + i] = obj_table[i];
        head.fetch_add(n, std::memory_order_relaxed);
    }
    return n;
}

unsigned int stack::pop(void **obj_table, unsigned int n) {
    irq_save_lock_type irq_lock;
    WITH_LOCK(irq_lock) {
        if (head.load(std::memory_order_relaxed) < n) [[unlikely]]
            return 0;
        for (unsigned i = 0; i < n; ++i)
            obj_table[n - i - 1] = objs[head - n + i];
        head.fetch_sub(n, std::memory_order_relaxed);
    }
    return n;
}
