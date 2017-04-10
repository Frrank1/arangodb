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

#include "RocksDBEngine/RocksDBExportCursor.h"
#include "RocksDBEngine/RocksDBCollectionExport.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

RocksDBExportCursor::RocksDBExportCursor(TRI_vocbase_t* vocbase, CursorId id,
                                         arangodb::RocksDBCollectionExport* ex,
                                         size_t batchSize, double ttl,
                                         bool hasCount)
    : Cursor(id, batchSize, nullptr, ttl, hasCount),
      _vocbaseGuard(vocbase),
      _ex(ex),
      _size(ex->_vpack.size()) {}

RocksDBExportCursor::~RocksDBExportCursor() { delete _ex; }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the cursor contains more data
////////////////////////////////////////////////////////////////////////////////

bool RocksDBExportCursor::hasNext() {
  if (_ex == nullptr) {
    return false;
  }

  return (_position < _size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next element (not implemented)
////////////////////////////////////////////////////////////////////////////////

VPackSlice RocksDBExportCursor::next() {
  // should not be called directly
  return VPackSlice();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cursor size
////////////////////////////////////////////////////////////////////////////////

size_t RocksDBExportCursor::count() const { return _size; }

static bool IncludeAttribute(
    RocksDBCollectionExport::Restrictions::Type const restrictionType,
    std::unordered_set<std::string> const& fields, std::string const& key) {
  if (restrictionType ==
          RocksDBCollectionExport::Restrictions::RESTRICTION_INCLUDE ||
      restrictionType ==
          RocksDBCollectionExport::Restrictions::RESTRICTION_EXCLUDE) {
    bool const keyContainedInRestrictions = (fields.find(key) != fields.end());
    if ((restrictionType ==
             RocksDBCollectionExport::Restrictions::RESTRICTION_INCLUDE &&
         !keyContainedInRestrictions) ||
        (restrictionType ==
             RocksDBCollectionExport::Restrictions::RESTRICTION_EXCLUDE &&
         keyContainedInRestrictions)) {
      // exclude the field
      return false;
    }
    // include the field
    return true;
  } else {
    // no restrictions
    TRI_ASSERT(restrictionType ==
               RocksDBCollectionExport::Restrictions::RESTRICTION_NONE);
    return true;
  }
  return true;
}

void RocksDBExportCursor::dump(VPackBuilder& builder) {
  auto transactionContext =
      std::make_shared<transaction::StandaloneContext>(_vocbaseGuard.vocbase());

  VPackOptions const* oldOptions = builder.options;

  builder.options = transactionContext->getVPackOptions();

  TRI_ASSERT(_ex != nullptr);
  auto const restrictionType = _ex->_restrictions.type;

  try {
    builder.add("result", VPackValue(VPackValueType::Array));
    size_t const n = batchSize();

    for (size_t i = 0; i < n; ++i) {
      if (!hasNext()) {
        break;
      }

      VPackSlice const slice(_ex->_vpack.at(_position++).slice());
      builder.openObject();
      // Copy over shaped values
      for (auto const& entry : VPackObjectIterator(slice)) {
        std::string key(entry.key.copyString());

        if (!IncludeAttribute(restrictionType, _ex->_restrictions.fields,
                              key)) {
          // Ignore everything that should be excluded or not included
          continue;
        }
        // If we get here we need this entry in the final result
        if (entry.value.isCustom()) {
          builder.add(key,
                      VPackValue(builder.options->customTypeHandler->toString(
                          entry.value, builder.options, slice)));
        } else {
          builder.add(key, entry.value);
        }
      }
      builder.close();
    }
    builder.close();  // close Array

    // builder.add("hasMore", VPackValue(hasNext() ? "true" : "false"));
    // //should not be string
    builder.add("hasMore", VPackValue(hasNext()));

    if (hasNext()) {
      builder.add("id", VPackValue(std::to_string(id())));
    }

    if (hasCount()) {
      builder.add("count", VPackValue(static_cast<uint64_t>(count())));
    }

    if (extra().isObject()) {
      builder.add("extra", extra());
    }

    if (!hasNext()) {
      // mark the cursor as deleted
      delete _ex;
      _ex = nullptr;
      this->deleted();
    }
  } catch (arangodb::basics::Exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "internal error during RocksDBExportCursor::dump");
  }
  builder.options = oldOptions;
}
