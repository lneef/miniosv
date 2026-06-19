#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <osv/mmu.hh>
#include <osv/mutex.h>
#include <osv/pagealloc.hh>

namespace minidpdk {

struct alignas(64) page_header {
  page_header *next;
  page_header *prev;
  uintptr_t iova;
  size_t used;

  static void list_remove(page_header *s) {
    s->prev->next = s->next;
    s->next->prev = s->prev;
  }

  page_header() : next(nullptr), prev(nullptr), used(0) {}
};

static_assert(sizeof(page_header) % 64 == 0, "");
class page_store {
public:
  static constexpr size_t page_size = mmu::huge_page_size;
  static constexpr size_t max_chunk = page_size - sizeof(page_header);

  struct page_list {
    page_header head, tail;
    page_list() : head(), tail() {
      head.next = &tail;
      tail.prev = &head;
    }

    void list_push(page_header *s) {
      s->next = head.next;
      s->prev = &head;
      head.next->prev = s;
      head.next = s;
    }

    bool empty() const { return head.next == &tail; }

    page_header *front() { return head.next; }
  };

  struct region {
    const page_header *hdr;  
    uint8_t *data;     
  };

  page_store() : regions() {}
 
  region alloc(size_t size) {
    SCOPE_LOCK(lock);
    assert(size <= max_chunk);
    if (cur == nullptr || page_size - cur->used < size)
      cur = new_page();
    auto *data = reinterpret_cast<uint8_t *>(cur) + cur->used;
    cur->used += size;
    return {cur, data};
  }

  ~page_store() {
    auto *s = regions.head.next;
    while (s != &regions.tail) {
      auto *next = s->next;
      memory::free_huge_page(s, page_size);
      s = next;
    }
  }

  static std::shared_ptr<page_store> instance() {
    static std::shared_ptr<page_store> inst = std::make_shared<page_store>();
    return inst;
  }

private:
  page_header *new_page() {
    auto *mem = memory::alloc_huge_page(page_size);
    assert(mem != nullptr);
    auto *s = new (mem) page_header();
    s->iova = mmu::virt_to_phys(s);
    s->used = sizeof(page_header);
    regions.list_push(s);
    return s;
  }

  page_list regions;
  page_header *cur = nullptr;
  mutex lock;
};

} // namespace minidpdk
