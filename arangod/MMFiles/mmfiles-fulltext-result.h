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

#ifndef ARANGOD_MMFILES_MMFILES_FULLTEXT_RESULT_H
#define ARANGOD_MMFILES_MMFILES_FULLTEXT_RESULT_H 1

#include "mmfiles-fulltext-common.h"

// Forward declarations
namespace arangodb {
  struct DocumentIdentifierToken;
}

/// @brief typedef for a fulltext result list
typedef struct TRI_fulltext_result_s {
  uint32_t _numDocuments;
  arangodb::DocumentIdentifierToken* _documents;
} TRI_fulltext_result_t;

/// @brief create a result
TRI_fulltext_result_t* TRI_CreateResultMMFilesFulltextIndex(const uint32_t);

/// @brief destroy a result
void TRI_DestroyResultMMFilesFulltextIndex(TRI_fulltext_result_t*);

/// @brief free a result
void TRI_FreeResultMMFilesFulltextIndex(TRI_fulltext_result_t*);

#endif
