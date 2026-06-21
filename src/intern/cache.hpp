#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace lut::cache {
template <typename T>
class Cache {
  public:
    struct Entry {
        std::mutex mtx;
        std::wstring name;
        T cache;

        explicit Entry(const std::wstring& n) : name(n) {}
    };

    class LockedEntry {
      public:
        explicit LockedEntry(std::shared_ptr<Entry> e) : entry_(std::move(e)), lock_(entry_->mtx) {}

        [[nodiscard]] T* operator->() const noexcept { return &entry_->cache; }
        [[nodiscard]] T& operator*() const noexcept { return entry_->cache; }

      private:
        std::shared_ptr<Entry> entry_;
        std::unique_lock<std::mutex> lock_;
    };

    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;
    Cache(Cache&&) = delete;
    Cache& operator=(Cache&&) = delete;

    Cache() = default;
    ~Cache() = default;

    [[nodiscard]] LockedEntry Fetch(int64_t id, const std::wstring& name) {
        {
            std::shared_lock lock(mtx_);
            if (auto it = id_to_cache_.find(id); it != id_to_cache_.end()) {
                if (auto entry = it->second; entry != nullptr && entry->name == name) {
                    return LockedEntry{std::move(entry)};
                }
            }
        }

        std::unique_lock lock(mtx_);
        if (auto it = id_to_cache_.find(id); it != id_to_cache_.end()) {
            if (auto entry = it->second; entry != nullptr && entry->name == name) {
                return LockedEntry{std::move(entry)};
            }

            Release(id);
        }

        std::shared_ptr<Entry> entry;
        if (auto it = name_to_cache_.find(name); it != name_to_cache_.end()) {
            entry = it->second.lock();
            if (entry == nullptr) {
                name_to_cache_.erase(it);
            }
        }

        if (entry == nullptr) {
            entry = std::make_shared<Entry>(name);
            name_to_cache_[name] = entry;
        }

        return LockedEntry{id_to_cache_[id] = std::move(entry)};
    }

    void Reset(int64_t id) {
        std::unique_lock lock(mtx_);
        Release(id);
    }

    void Reset(const std::wstring& name) {
        std::unique_lock lock(mtx_);
        Release(name);
    }

    void Reset() {
        std::unique_lock lock(mtx_);
        std::unordered_map<std::wstring, std::weak_ptr<Entry>>{}.swap(name_to_cache_);
        std::unordered_map<int64_t, std::shared_ptr<Entry>>{}.swap(id_to_cache_);
    }

  private:
    std::shared_mutex mtx_;
    std::unordered_map<std::wstring, std::weak_ptr<Entry>> name_to_cache_;
    std::unordered_map<int64_t, std::shared_ptr<Entry>> id_to_cache_;

    void Release(int64_t id) {
        if (auto node = id_to_cache_.extract(id)) {
            auto& entry = node.mapped();
            if (entry != nullptr) {
                const auto name = entry->name;
                entry.reset();
                if (auto it = name_to_cache_.find(name); it != name_to_cache_.end() && it->second.expired()) {
                    name_to_cache_.erase(it);
                }
            }
        }
    }

    void Release(const std::wstring& name) {
        if (auto node = name_to_cache_.extract(name)) {
            std::erase_if(id_to_cache_, [&](const auto& e) { return e.second != nullptr && e.second->name == name; });
        }
    }
};
}  // namespace lut::cache
