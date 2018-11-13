/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_PROFILING_MEMORY_INTERNER_H_
#define SRC_PROFILING_MEMORY_INTERNER_H_

#include <stddef.h>
#include <set>

namespace perfetto {
namespace profiling {

using InternID = uintptr_t;

template <typename T>
class Interner {
 private:
  struct Entry {
    Entry(T d, Interner<T>* in) : data(std::move(d)), interner(in) {}
    bool operator<(const Entry& other) const { return data < other.data; }

    const T data;
    size_t ref_count = 0;
    Interner<T>* interner;
  };

 public:
  class Interned {
   public:
    friend class Interner<T>;
    Interned(Entry* entry) : entry_(entry) {}
    Interned(const Interned& other) : entry_(other.entry_) {
      if (entry_ != nullptr)
        entry_->ref_count++;
    }

    Interned(Interned&& other) noexcept : entry_(other.entry_) {
      other.entry_ = nullptr;
    }

    Interned& operator=(Interned&& other) {
      entry_ = other.entry_;
      other.entry_ = nullptr;
      return *this;
    }

    Interned& operator=(Interned& other) noexcept {
      entry_ = other.entry_;
      if (entry_ != nullptr)
        entry_->ref_count++;
      return *this;
    }

    const T& data() const { return entry_->data; }

    InternID id() const { return reinterpret_cast<InternID>(entry_); }

    ~Interned() {
      if (entry_ != nullptr)
        entry_->interner->Return(entry_);
    }

    bool operator<(const Interned& other) const {
      if (entry_ == nullptr || other.entry_ == nullptr)
        return entry_ < other.entry_;
      return *entry_ < *(other.entry_);
    }

    const T* operator->() const { return &entry_->data; }

   private:
    Interner::Entry* entry_;
  };

  Interned Intern(const T& data) {
    auto itr = entries_.emplace(data, this);
    Entry& entry = const_cast<Entry&>(*itr.first);
    entry.ref_count++;
    return Interned(&entry);
  }
  size_t entry_count_for_testing() { return entries_.size(); }

 private:
  void Return(Entry* entry) {
    if (--entry->ref_count == 0)
      entries_.erase(*entry);
  }
  std::set<Entry> entries_;
  static_assert(sizeof(Interned) == sizeof(void*),
                "interned things should be small");
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_INTERNER_H_