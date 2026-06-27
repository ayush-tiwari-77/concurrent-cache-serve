#pragma once
#include <unordered_map>
#include <list>
#include <mutex>
#include <string>
#include <optional>
#include <utility>

// Thread-safe LRU (Least Recently Used) cache.
// Backing data structure: doubly linked list (O(1) move-to-front)
//                        + hash map (O(1) key lookup -> list iterator)
// Synchronization: single mutex guarding all operations on this shard.
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    void put(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            order_.splice(order_.begin(), order_, it->second);
            return;
        }
        if (order_.size() >= capacity_) {
            auto& last = order_.back();
            map_.erase(last.first);
            order_.pop_back();
            evictions_++;
        }
        order_.emplace_front(key, value);
        map_[key] = order_.begin();
    }

    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it == map_.end()) {
            misses_++;
            return std::nullopt;
        }
        hits_++;
        order_.splice(order_.begin(), order_, it->second);
        return it->second->second;
    }

    bool remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        order_.erase(it->second);
        map_.erase(it);
        return true;
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return order_.size();
    }

    struct Stats { long hits; long misses; long evictions; };
    Stats stats() {
        std::lock_guard<std::mutex> lock(mutex_);
        return {hits_, misses_, evictions_};
    }

private:
    using KV = std::pair<std::string, std::string>;
    size_t capacity_;
    std::list<KV> order_;                                   // front = most recently used
    std::unordered_map<std::string, std::list<KV>::iterator> map_;
    std::mutex mutex_;
    long hits_ = 0, misses_ = 0, evictions_ = 0;
};
