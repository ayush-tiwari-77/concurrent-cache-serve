#pragma once
#include "lru_cache.h"
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <sstream>

// Splits the keyspace across N independent LRUCache shards using
// hash(key) % N. Each shard has its OWN mutex, so requests for keys
// in different shards can proceed truly in parallel instead of
// contending on one global lock. This is the same partitioning idea
// used by real distributed caches/databases to spread load across
// multiple nodes -- here it's simulated across in-process shards.
class ShardedCache {
public:
    ShardedCache(size_t num_shards, size_t capacity_per_shard) {
        for (size_t i = 0; i < num_shards; ++i)
            shards_.push_back(std::make_unique<LRUCache>(capacity_per_shard));
    }

    void put(const std::string& key, const std::string& value) {
        shard_for(key)->put(key, value);
    }

    std::optional<std::string> get(const std::string& key) {
        return shard_for(key)->get(key);
    }

    bool remove(const std::string& key) {
        return shard_for(key)->remove(key);
    }

    std::string stats_string() {
        long hits = 0, misses = 0, evictions = 0, size = 0;
        for (auto& shard : shards_) {
            auto s = shard->stats();
            hits += s.hits; misses += s.misses; evictions += s.evictions;
            size += shard->size();
        }
        std::ostringstream oss;
        oss << "shards=" << shards_.size()
            << " size=" << size
            << " hits=" << hits
            << " misses=" << misses
            << " evictions=" << evictions;
        return oss.str();
    }

    size_t shard_count() const { return shards_.size(); }

private:
    LRUCache* shard_for(const std::string& key) {
        size_t h = std::hash<std::string>{}(key);
        return shards_[h % shards_.size()].get();
    }
    std::vector<std::unique_ptr<LRUCache>> shards_;
};
