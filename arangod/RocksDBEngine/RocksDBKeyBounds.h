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
/// @author Jan Steemann
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_KEY_BOUNDS_H
#define ARANGO_ROCKSDB_ROCKSDB_KEY_BOUNDS_H 1

#include "Basics/Common.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "VocBase/vocbase.h"

#include <rocksdb/slice.h>

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

class RocksDBKeyBounds {
 public:
  RocksDBKeyBounds() = delete;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for list of all databases
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds Databases();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all collections belonging to a specified database
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds DatabaseCollections(TRI_voc_tick_t databaseId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all collections belonging to a specified database
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds DatabaseIndexes(TRI_voc_tick_t databaseId,
                                          TRI_voc_cid_t cid);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all indexes belonging to a specified collection
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds CollectionIndexes(TRI_voc_tick_t databaseId,
                                            TRI_voc_cid_t collectionId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all documents belonging to a specified collection
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds CollectionDocuments(uint64_t collectionId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all index-entries- belonging to a specified primary index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds PrimaryIndex(uint64_t indexId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all index-entries belonging to a specified edge index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds EdgeIndex(uint64_t indexId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all index-entries belonging to a specified edge index related
  /// to the specified vertex
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds EdgeIndexVertex(uint64_t indexId,
                                          std::string const& vertexId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all index-entries belonging to a specified non-unique index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds IndexEntries(uint64_t indexId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all entries belonging to a specified unique index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds UniqueIndex(uint64_t indexId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all index-entries within a value range belonging to a
  /// specified non-unique index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds IndexRange(uint64_t indexId, VPackSlice const& left,
                                     VPackSlice const& right);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all documents within a value range belonging to a
  /// specified unique index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds UniqueIndexRange(uint64_t indexId,
                                           VPackSlice const& left,
                                           VPackSlice const& right);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all views belonging to a specified database
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds DatabaseViews(TRI_voc_tick_t databaseId);
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all counter values
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds CounterValues();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the left bound slice.
  ///
  /// Forward iterators may use it->Seek(bound.start()) and reverse iterators
  /// may check that the current key is greater than this value.
  //////////////////////////////////////////////////////////////////////////////
  rocksdb::Slice const start() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the right bound slice.
  ///
  /// Reverse iterators may use it->SeekForPrev(bound.end()) and forward
  /// iterators may check that the current key is less than this value.
  //////////////////////////////////////////////////////////////////////////////
  rocksdb::Slice const end() const;

 private:
  RocksDBKeyBounds(RocksDBEntryType type);
  RocksDBKeyBounds(RocksDBEntryType type, uint64_t first);
  RocksDBKeyBounds(RocksDBEntryType type, uint64_t first, uint64_t second);
  RocksDBKeyBounds(RocksDBEntryType type, uint64_t first,
                   std::string const& second);
  RocksDBKeyBounds(RocksDBEntryType type, uint64_t first,
                   VPackSlice const& second, VPackSlice const& third);

  void nextPrefix(std::string& s);

 private:
  static const char _stringSeparator;
  RocksDBEntryType _type;
  std::string _startBuffer;
  std::string _endBuffer;
};

}  // namespace arangodb

#endif
