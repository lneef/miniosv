#pragma once

#include <cstddef>
#include <iterator>

namespace minidpdk {

class intrusive_hook {
public:
  constexpr intrusive_hook() noexcept = default;
  ~intrusive_hook() { unlink(); }

  intrusive_hook(const intrusive_hook &) = delete;
  intrusive_hook &operator=(const intrusive_hook &) = delete;

  void unlink() noexcept {
    if (next_) {
      prev_->next_ = next_;
      next_->prev_ = prev_;
      next_ = prev_ = nullptr;
    }
  }

private:
  template <class T, intrusive_hook T::*Member> friend class intrusive_list;

  intrusive_hook *prev_ = nullptr;
  intrusive_hook *next_ = nullptr;
};

template <class T, intrusive_hook T::*Member> class intrusive_list {
public:
  constexpr intrusive_list() noexcept {
    sentinel_.next_ = &sentinel_;
    sentinel_.prev_ = &sentinel_;
  }

  intrusive_list(const intrusive_list &) = delete;
  intrusive_list &operator=(const intrusive_list &) = delete;

  void push_back(T &value) noexcept {
    intrusive_hook *node = &(value.*Member);
    intrusive_hook *last = sentinel_.prev_;
    node->prev_ = last;
    node->next_ = &sentinel_;
    last->next_ = node;
    sentinel_.prev_ = node;
  }

  class iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using reference = T &;
    using pointer = T *;
    using difference_type = std::ptrdiff_t;

    iterator() = default;
    explicit iterator(intrusive_hook *node) noexcept : node_(node) {}

    reference operator*() const noexcept { return *to_elem(node_); }
    pointer operator->() const noexcept { return to_elem(node_); }

    iterator &operator++() noexcept {
      node_ = node_->next_;
      return *this;
    }
    iterator operator++(int) noexcept {
      iterator tmp = *this;
      ++*this;
      return tmp;
    }

    bool operator==(const iterator &o) const noexcept {
      return node_ == o.node_;
    }
    bool operator!=(const iterator &o) const noexcept {
      return node_ != o.node_;
    }

  private:
    intrusive_hook *node_ = nullptr;
  };

  iterator begin() noexcept { return iterator(sentinel_.next_); }
  iterator end() noexcept { return iterator(&sentinel_); }

private:
  // Recover the enclosing T from a hook pointer via the byte offset of Member.
  static T *to_elem(intrusive_hook *node) noexcept {
    T *const base = reinterpret_cast<T *>(alignof(T));
    const std::size_t off = reinterpret_cast<char *>(&(base->*Member)) -
                            reinterpret_cast<char *>(base);
    return reinterpret_cast<T *>(reinterpret_cast<char *>(node) - off);
  }

  intrusive_hook sentinel_;
};

} // namespace minidpdk
