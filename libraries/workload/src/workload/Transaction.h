//
//  Transaction.h
//  libraries/workload/src/workload
//
//  Created by Sam Gateau 2018.03.12
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_workload_Transaction_h
#define hifi_workload_Transaction_h

#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include "Proxy.h"


namespace workload {

// Transaction is the mechanism to make any change to the Space.
// Whenever a new proxy need to be reset,
// or when an proxy changes its position or its size
// or when an proxy is destroyed
// These changes must be expressed through the corresponding command from the Transaction
// The Transaction is then queued on the Space so all the pending transactions can be consolidated and processed at the time
// of updating the space at the Frame boundary.
// 
class Transaction {
    friend class Space;
public:
    using ProxyPayload = Sphere;

    Transaction() {}
    ~Transaction() {}

    // Proxy transactions
    void reset(ProxyID id, const ProxyPayload& sphere);
    void remove(ProxyID id);
    bool hasRemovals() const { return !_removedItems.empty(); }

    void update(ProxyID id, const ProxyPayload& sphere);
    
    void reserve(const std::vector<Transaction>& transactionContainer);
    void merge(const std::vector<Transaction>& transactionContainer);
    void merge(std::vector<Transaction>&& transactionContainer);
    void merge(const Transaction& transaction);
    void merge(Transaction&& transaction);
    void clear();
    
protected:

    using Reset = std::tuple<ProxyID, ProxyPayload>;
    using Remove = ProxyID;
    using Update = std::tuple<ProxyID, ProxyPayload>;

    using Resets = std::vector<Reset>;
    using Removes = std::vector<Remove>;
    using Updates = std::vector<Update>;

    Resets _resetItems;
    Removes _removedItems;
    Updates _updatedItems;
};
typedef std::vector<Transaction> TransactionQueue;

namespace indexed_container {

    using Index = int32_t;
    const Index MAXIMUM_INDEX{ 1 << 30 };
    const Index INVALID_INDEX{ -1 };
    using Indices = std::vector< Index >;

    template <Index MaxNumElements = MAXIMUM_INDEX>
    class Allocator {
    public:
        Allocator() {}
        Indices _freeIndices;
        std::atomic<int32_t> _nextNewIndex{ 0 };
        std::atomic<int32_t> _numFreeIndices{ 0 };

        bool checkIndex(Index index) const { return ((index >= 0) && (index < _nextNewIndex.load())); }
        Index getNumIndices() const { return _nextNewIndex - (Index)_freeIndices.size(); }
        Index getNumFreeIndices() const { return (Index)_freeIndices.size(); }
        Index getNumAllocatedIndices() const { return _nextNewIndex.load(); }

        Index allocateIndex() {
            if (_freeIndices.empty()) {
                Index index = _nextNewIndex;
                if (index >= MaxNumElements) {
                    // abort! we are trying to go overboard with the total number of allocated elements
                    assert(false);
                    // This should never happen because Bricks are allocated along with the cells and there
                    // is already a cap on the cells allocation
                    return INVALID_INDEX;
                }
                _nextNewIndex++;
                return index;
            } else {
                Index index = _freeIndices.back();
                _freeIndices.pop_back();
                return index;
            }
        }

        void freeIndex(Index index) {
            if (checkIndex(index)) {
                _freeIndices.push_back(index);
            }
        }

        void clear() {
            _freeIndices.clear();
            _nextNewIndex = 0;
        }
    };
}

class Collection {
public:

    // This call is thread safe, can be called from anywhere to allocate a new ID
    ProxyID allocateID();

    // Check that the ID is valid and allocated for this space, this a threadsafe call
    bool isAllocatedID(const ProxyID& id) const;

    // THis is the total number of allocated proxies, this a threadsafe call
    Index getNumAllocatedProxies() const { return _numAllocatedProxies.load(); }

    // Enqueue transaction to the space
    void enqueueTransaction(const Transaction& transaction);

    // Enqueue transaction to the space
    void enqueueTransaction(Transaction&& transaction);

    // Enqueue end of frame transactions boundary
    uint32_t enqueueFrame();

    // Process the pending transactions queued
    void processTransactionQueue();

protected:

    // Thread safe elements that can be accessed from anywhere
    std::atomic<unsigned int> _IDAllocator{ 1 }; // first valid itemID will be One
    std::atomic<unsigned int> _numAllocatedItems{ 1 }; // num of allocated items, matching the _items.size()
    std::mutex _transactionQueueMutex;
    TransactionQueue _transactionQueue;


    std::mutex _transactionFramesMutex;
    using TransactionFrames = std::vector<Transaction>;
    TransactionFrames _transactionFrames;
    uint32_t _transactionFrameNumber{ 0 };

    // Process one transaction frame 
    void processTransactionFrame(const Transaction& transaction);
};

} // namespace workload

#endif // hifi_workload_Transaction_h