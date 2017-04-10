////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Daniel H. Larkin
/// @author Jan Steemann
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_COMMON_H
#define ARANGO_ROCKSDB_ROCKSDB_COMMON_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBValue.h"

#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>

namespace rocksdb {class TransactionDB;
  class DB;
  struct ReadOptions;
  class Comparator;
}

namespace arangodb {
class TransactionState;
class RocksDBTransactionState;
class RocksDBKeyBounds;
class RocksDBEngine;
namespace transaction {
class Methods;
}
namespace rocksutils {

enum StatusHint { none, document, collection, view, index, database };

arangodb::Result convertStatus(rocksdb::Status const&,
                               StatusHint hint = StatusHint::none);

uint64_t uint64FromPersistent(char const* p);
void uint64ToPersistent(char* p, uint64_t value);
void uint64ToPersistent(std::string& out, uint64_t value);
RocksDBTransactionState* toRocksTransactionState(transaction::Methods* trx);
rocksdb::TransactionDB* globalRocksDB();
RocksDBEngine* globalRocksEngine();
arangodb::Result globalRocksDBPut(
    rocksdb::Slice const& key, rocksdb::Slice const& value,
    rocksdb::WriteOptions const& = rocksdb::WriteOptions{});

arangodb::Result globalRocksDBRemove(
    rocksdb::Slice const& key,
    rocksdb::WriteOptions const& = rocksdb::WriteOptions{});

/// Iterator over all keys in range and count them
std::size_t countKeyRange(rocksdb::DB*, rocksdb::ReadOptions const&,
                          RocksDBKeyBounds const&);

/// @brief helper method to remove large ranges of data
/// Should mainly be used to implement the drop() call
Result removeLargeRange(rocksdb::TransactionDB* db, RocksDBKeyBounds const& bounds);

std::vector<std::pair<RocksDBKey,RocksDBValue>> collectionKVPairs(TRI_voc_tick_t databaseId);
std::vector<std::pair<RocksDBKey,RocksDBValue>> indexKVPairs(TRI_voc_tick_t databaseId, TRI_voc_cid_t cid);
std::vector<std::pair<RocksDBKey,RocksDBValue>> viewKVPairs(TRI_voc_tick_t databaseId);

// optional switch to std::function to reduce amount of includes and to avoid template
// this helper is not meant for transactional usage!
template<typename T> //T is a invokeable that takes a rocksdb::Iterator*
void iterateBounds(RocksDBKeyBounds const& bounds, T callback, rocksdb::ReadOptions const& options = rocksdb::ReadOptions{}){
  auto cmp = globalRocksEngine()->cmp();
  std::unique_ptr<rocksdb::Iterator> it(globalRocksDB()->NewIterator(options));
  for (it->Seek(bounds.start()); it->Valid() && cmp->Compare(it->key(), bounds.end()) < 0; it->Next()) {
    callback(it.get());
  }
}

}  // namespace rocksutils
}  // namespace arangodb

#endif
