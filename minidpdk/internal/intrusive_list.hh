#pragma once

#include <boost/intrusive/list.hpp>

namespace minidpdk {

namespace bi = boost::intrusive;
using intrusive_hook = bi::list_member_hook<bi::link_mode<bi::auto_unlink>>;

template <class T, intrusive_hook T::*Member>
using intrusive_list =
    bi::list<T, bi::member_hook<T, intrusive_hook, Member>,
             bi::constant_time_size<false>>;

} // namespace minidpdk
