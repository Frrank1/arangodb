////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOCBASE_PHYSICAL_COLLECTION_H
#define ARANGOD_VOCBASE_PHYSICAL_COLLECTION_H 1

#include "Basics/Common.h"
#include "VocBase/DatafileStatisticsContainer.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

struct TRI_df_marker_t;

namespace arangodb {
namespace transaction {
class Methods;
}

struct DocumentIdentifierToken;
class Index;
class LogicalCollection;
class ManagedDocumentResult;
struct OperationOptions;

class PhysicalCollection {
 protected:
  explicit PhysicalCollection(LogicalCollection* collection, VPackSlice const& info) : _logicalCollection(collection) {}

 public:
  virtual ~PhysicalCollection() = default;
  
  //path to logical collection
  virtual std::string const& path() const = 0;
  virtual void setPath(std::string const&) = 0; // should be set during collection creation
                                                // creation happens atm in engine->createCollection
  virtual int updateProperties(VPackSlice const& slice, bool doSync) = 0;
  
  virtual PhysicalCollection* clone(LogicalCollection*, PhysicalCollection*) = 0;

  virtual TRI_voc_rid_t revision() const = 0;
  
  virtual int64_t initialCount() const = 0;

  virtual void updateCount(int64_t) = 0;

  virtual size_t journalSize() const = 0;
  
  virtual void figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>&) = 0;
  void figures(std::shared_ptr<arangodb::velocypack::Builder>& builder);
  
  virtual int close() = 0;
  
  /// @brief rotate the active journal - will do nothing if there is no journal
  /// REVIEW - MOVE INTO MMFILES?? - used in v8-collection
  virtual int rotateActiveJournal() = 0;

  /// REVIEW - MOVE INTO MMFILES?? - used in replication-dump
  virtual bool applyForTickRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
                                 std::function<bool(TRI_voc_tick_t foundTick, TRI_df_marker_t const* marker)> const& callback) = 0;

  /// @brief report extra memory used by indexes etc.
  virtual size_t memory() const = 0;
    
  /// @brief opens an existing collection
  virtual void open(bool ignoreErrors) = 0;

  /// @brief iterate all markers of a collection on load
  virtual int iterateMarkersOnLoad(transaction::Methods* trx) = 0;
  
  virtual uint8_t const* lookupRevisionVPack(TRI_voc_rid_t revisionId) const = 0;
  virtual uint8_t const* lookupRevisionVPackConditional(TRI_voc_rid_t revisionId, TRI_voc_tick_t maxTick, bool excludeWal) const = 0;

  virtual bool isFullyCollected() const = 0;
  virtual bool doCompact() const = 0;

  ////////////////////////////////////
  // -- SECTION Indexes --
  ///////////////////////////////////

  virtual int saveIndex(transaction::Methods* trx,
                        std::shared_ptr<arangodb::Index> idx) = 0;

  /// @brief Restores an index from VelocyPack.
  virtual int restoreIndex(transaction::Methods*, velocypack::Slice const&,
                           std::shared_ptr<Index>&) = 0;

  virtual bool dropIndex(TRI_idx_iid_t iid, bool writeMarker) = 0;

  ////////////////////////////////////
  // -- SECTION DML Operations --
  ///////////////////////////////////

  virtual void truncate(transaction::Methods* trx, OperationOptions& options) = 0;

  virtual int read(transaction::Methods*, arangodb::velocypack::Slice const key,
                   ManagedDocumentResult& result, bool) = 0;

  virtual bool readDocument(transaction::Methods* trx,
                            DocumentIdentifierToken const& token,
                            ManagedDocumentResult& result) = 0;

  virtual bool readDocumentConditional(transaction::Methods* trx,
                                       DocumentIdentifierToken const& token,
                                       TRI_voc_tick_t maxTick,
                                       ManagedDocumentResult& result) = 0;

  virtual int insert(arangodb::transaction::Methods* trx,
                     arangodb::velocypack::Slice const newSlice,
                     arangodb::ManagedDocumentResult& result,
                     OperationOptions& options,
                     TRI_voc_tick_t& resultMarkerTick, bool lock) = 0;

  virtual int update(arangodb::transaction::Methods* trx,
                     VPackSlice const newSlice, ManagedDocumentResult& result,
                     OperationOptions& options,
                     TRI_voc_tick_t& resultMarkerTick, bool lock,
                     TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                     TRI_voc_rid_t const& revisionId,
                     arangodb::velocypack::Slice const key) = 0;

  virtual int replace(transaction::Methods* trx,
                      arangodb::velocypack::Slice const newSlice,
                      ManagedDocumentResult& result, OperationOptions& options,
                      TRI_voc_tick_t& resultMarkerTick, bool lock,
                      TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                      TRI_voc_rid_t const revisionId,
                      arangodb::velocypack::Slice const fromSlice,
                      arangodb::velocypack::Slice const toSlice) = 0;

  virtual int remove(arangodb::transaction::Methods* trx,
                     arangodb::velocypack::Slice const slice,
                     arangodb::ManagedDocumentResult& previous,
                     OperationOptions& options,
                     TRI_voc_tick_t& resultMarkerTick, bool lock,
                     TRI_voc_rid_t const& revisionId, TRI_voc_rid_t& prevRev) = 0;

 protected:

  // SECTION: Document pre commit preperation

  /// @brief new object for insert, value must have _key set correctly.
  int newObjectForInsert(transaction::Methods* trx,
                         velocypack::Slice const& value,
                         velocypack::Slice const& fromSlice,
                         velocypack::Slice const& toSlice,
                         bool isEdgeCollection,
                         velocypack::Builder& builder,
                         bool isRestore) const;


   /// @brief new object for remove, must have _key set
  void newObjectForRemove(transaction::Methods* trx,
                          velocypack::Slice const& oldValue,
                          std::string const& rev, velocypack::Builder& builder) const;

  /// @brief merge two objects for update
  void mergeObjectsForUpdate(transaction::Methods* trx,
                             velocypack::Slice const& oldValue,
                             velocypack::Slice const& newValue,
                             bool isEdgeCollection, std::string const& rev,
                             bool mergeObjects, bool keepNull,
                             velocypack::Builder& builder) const;

  /// @brief new object for replace
  void newObjectForReplace(transaction::Methods* trx,
                           velocypack::Slice const& oldValue,
                           velocypack::Slice const& newValue,
                           velocypack::Slice const& fromSlice,
                           velocypack::Slice const& toSlice,
                           bool isEdgeCollection, std::string const& rev,
                           velocypack::Builder& builder) const;

  int checkRevision(transaction::Methods* trx, TRI_voc_rid_t expected,
                    TRI_voc_rid_t found) const;

 protected:
  LogicalCollection* _logicalCollection;
};

} // namespace arangodb

#endif
