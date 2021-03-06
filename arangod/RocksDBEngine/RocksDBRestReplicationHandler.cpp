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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine/RocksDBRestReplicationHandler.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/FollowerInfo.h"
#include "GeneralServer/GeneralServer.h"
#include "Indexes/Index.h"
#include "Logger/Logger.h"
#include "Replication/InitialSyncer.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBReplicationTailing.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Context.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionKeysRepository.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/PhysicalCollection.h"
#include "VocBase/replication-applier.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::rocksutils;

static int restoreDataParser(char const* ptr, char const* pos,
                             std::string const& invalidMsg, bool useRevision,
                             std::string& errorMsg, std::string& key,
                             VPackBuilder& builder, VPackSlice& doc,
                             TRI_replication_operation_e& type);

uint64_t const RocksDBRestReplicationHandler::_defaultChunkSize = 128 * 1024;
uint64_t const RocksDBRestReplicationHandler::_maxChunkSize = 128 * 1024 * 1024;

RocksDBRestReplicationHandler::RocksDBRestReplicationHandler(
    GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response),
      _manager(globalRocksEngine()->replicationManager()) {}

RocksDBRestReplicationHandler::~RocksDBRestReplicationHandler() {}

RestStatus RocksDBRestReplicationHandler::execute() {
  // extract the request type
  auto const type = _request->requestType();
  auto const& suffixes = _request->suffixes();

  size_t const len = suffixes.size();

  if (len >= 1) {
    std::string const& command = suffixes[0];

    if (command == "logger-state") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandLoggerState();
    } else if (command == "logger-follow") {
      if (type != rest::RequestType::GET && type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }
      handleCommandLoggerFollow();
    } else if (command == "determine-open-transactions") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandDetermineOpenTransactions();
    } else if (command == "batch") {
      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandBatch();
      }
    } else if (command == "inventory") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandInventory();
      }
    } else if (command == "keys") {
      if (type != rest::RequestType::GET && type != rest::RequestType::POST &&
          type != rest::RequestType::PUT &&
          type != rest::RequestType::DELETE_REQ) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      if (type == rest::RequestType::POST) {
        handleCommandCreateKeys();
      } else if (type == rest::RequestType::GET) {
        handleCommandGetKeys();
      } else if (type == rest::RequestType::PUT) {
        handleCommandFetchKeys();
      } else if (type == rest::RequestType::DELETE_REQ) {
        handleCommandRemoveKeys();
      }
    } else if (command == "dump") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }

      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandDump();
      }
    } else if (command == "restore-collection") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreCollection();
    } else if (command == "restore-indexes") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreIndexes();
    } else if (command == "restore-data") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      handleCommandRestoreData();
    } else if (command == "sync") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      handleCommandSync();
    } else if (command == "make-slave") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      handleCommandMakeSlave();
    } else if (command == "server-id") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandServerId();
    } else if (command == "applier-config") {
      if (type == rest::RequestType::GET) {
        handleCommandApplierGetConfig();
      } else {
        if (type != rest::RequestType::PUT) {
          goto BAD_CALL;
        }
        handleCommandApplierSetConfig();
      }
    } else if (command == "applier-start") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      handleCommandApplierStart();
    } else if (command == "applier-stop") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      handleCommandApplierStop();
    } else if (command == "applier-state") {
      if (type == rest::RequestType::DELETE_REQ) {
        handleCommandApplierDeleteState();
      } else {
        if (type != rest::RequestType::GET) {
          goto BAD_CALL;
        }
        handleCommandApplierGetState();
      }
    } else if (command == "clusterInventory") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isCoordinator()) {
        generateError(rest::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
      } else {
        handleCommandClusterInventory();
      }
    } else if (command == "addFollower") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        handleCommandAddFollower();
      }
    } else if (command == "removeFollower") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        handleCommandRemoveFollower();
      }
    } else if (command == "holdReadLockCollection") {
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        if (type == rest::RequestType::POST) {
          handleCommandHoldReadLockCollection();
        } else if (type == rest::RequestType::PUT) {
          handleCommandCheckHoldReadLockCollection();
        } else if (type == rest::RequestType::DELETE_REQ) {
          handleCommandCancelHoldReadLockCollection();
        } else if (type == rest::RequestType::GET) {
          handleCommandGetIdForReadLockCollection();
        } else {
          goto BAD_CALL;
        }
      }
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid command");
    }

    return RestStatus::DONE;
  }

BAD_CALL:
  if (len != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "expecting URL /_api/replication/<command>");
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }

  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error if called on a coordinator server
////////////////////////////////////////////////////////////////////////////////

bool RocksDBRestReplicationHandler::isCoordinatorError() {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                  TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "replication API is not supported on a coordinator");
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_return_state
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandLoggerState() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "logger-state API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_delete_batch_replication
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandBatch() {
  // extract the request type
  auto const type = _request->requestType();
  auto const& suffixes = _request->suffixes();
  size_t const len = suffixes.size();

  TRI_ASSERT(len >= 1);

  if (type == rest::RequestType::POST) {
    // create a new blocker
    std::shared_ptr<VPackBuilder> input = _request->toVelocyPackBuilderPtr();

    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    // double expires =
    // VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);

    // TRI_voc_tick_t id;
    // StorageEngine* engine = EngineSelectorFeature::ENGINE;
    // int res = engine->insertCompactionBlocker(_vocbase, expires, id);

    RocksDBReplicationContext* ctx = _manager->createContext();
    RocksDBReplicationContextGuard(_manager, ctx);
    if (ctx == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
    }
    ctx->bind(_vocbase);  // create transaction+snapshot

    VPackBuilder b;
    b.add(VPackValue(VPackValueType::Object));
    b.add("id", VPackValue(std::to_string(ctx->id())));
    b.close();

    generateResult(rest::ResponseCode::OK, b.slice());
    return;
  }

  if (type == rest::RequestType::PUT && len >= 2) {
    // extend an existing blocker
    TRI_voc_tick_t id =
        static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffixes[1]));

    auto input = _request->toVelocyPackBuilderPtr();

    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double expires =
        VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);

    int res = TRI_ERROR_NO_ERROR;
    bool busy;
    RocksDBReplicationContext* ctx = _manager->find(id, busy, expires);
    RocksDBReplicationContextGuard(_manager, ctx);
    if (busy) {
      res = TRI_ERROR_CURSOR_BUSY;
    } else if (ctx == nullptr) {
      res = TRI_ERROR_CURSOR_NOT_FOUND;
    }

    // now extend the blocker
    // StorageEngine* engine = EngineSelectorFeature::ENGINE;
    // res = engine->extendCompactionBlocker(_vocbase, id, expires);

    if (res == TRI_ERROR_NO_ERROR) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  if (type == rest::RequestType::DELETE_REQ && len >= 2) {
    // delete an existing blocker
    TRI_voc_tick_t id =
        static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffixes[1]));

    bool found = _manager->remove(id);
    // StorageEngine* engine = EngineSelectorFeature::ENGINE;
    // int res = engine->removeCompactionBlocker(_vocbase, id);

    if (found) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      int res = TRI_ERROR_CURSOR_NOT_FOUND;
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }

  // we get here if anything above is invalid
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief forward a command in the coordinator case
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleTrampolineCoordinator() {
  bool useVpp = false;
  if (_request->transportType() == Endpoint::TransportType::VPP) {
    useVpp = true;
  }
  if (_request == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid request");
  }

  // First check the DBserver component of the body json:
  ServerID DBserver = _request->value("DBserver");

  if (DBserver.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "need \"DBserver\" parameter");
    return;
  }

  std::string const& dbname = _request->databaseName();

  auto headers = std::make_shared<std::unordered_map<std::string, std::string>>(
      arangodb::getForwardableRequestHeaders(_request.get()));
  std::unordered_map<std::string, std::string> values = _request->values();
  std::string params;

  for (auto const& i : values) {
    if (i.first != "DBserver") {
      if (params.empty()) {
        params.push_back('?');
      } else {
        params.push_back('&');
      }
      params.append(StringUtils::urlEncode(i.first));
      params.push_back('=');
      params.append(StringUtils::urlEncode(i.second));
    }
  }

  // Set a few variables needed for our work:
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    generateError(rest::ResponseCode::BAD, TRI_ERROR_SHUTTING_DOWN,
                  "shutting down server");
    return;
  }

  std::unique_ptr<ClusterCommResult> res;
  if (!useVpp) {
    HttpRequest* httpRequest = dynamic_cast<HttpRequest*>(_request.get());
    if (httpRequest == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid request type");
    }

    // Send a synchronous request to that shard using ClusterComm:
    res = cc->syncRequest("", TRI_NewTickServer(), "server:" + DBserver,
                          _request->requestType(),
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              _request->requestPath() + params,
                          httpRequest->body(), *headers, 300.0);
  } else {
    // do we need to handle multiple payloads here - TODO
    // here we switch vorm vst to http?!
    // i am not allowed to change cluster comm!
    res = cc->syncRequest("", TRI_NewTickServer(), "server:" + DBserver,
                          _request->requestType(),
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              _request->requestPath() + params,
                          _request->payload().toJson(), *headers, 300.0);
  }

  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    generateError(rest::ResponseCode::BAD, TRI_ERROR_CLUSTER_TIMEOUT,
                  "timeout within cluster");
    return;
  }
  if (res->status == CL_COMM_BACKEND_UNAVAILABLE) {
    // there is no result
    generateError(rest::ResponseCode::BAD, TRI_ERROR_CLUSTER_CONNECTION_LOST,
                  "lost connection within cluster");
    return;
  }
  if (res->status == CL_COMM_ERROR) {
    // This could be a broken connection or an Http error:
    TRI_ASSERT(nullptr != res->result && res->result->isComplete());
    // In this case a proper HTTP error was reported by the DBserver,
    // we simply forward the result.
    // We intentionally fall through here.
  }

  bool dummy;
  resetResponse(
      static_cast<rest::ResponseCode>(res->result->getHttpReturnCode()));

  _response->setContentType(
      res->result->getHeaderField(StaticStrings::ContentTypeHeader, dummy));

  if (!useVpp) {
    HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
    if (_response == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid response type");
    }
    httpResponse->body().swap(&(res->result->getBody()));
  } else {
    // TODO copy all payloads
    VPackSlice slice = res->result->getBodyVelocyPack()->slice();
    _response->setPayload(slice, true);  // do we need to generate the body?!
  }

  auto const& resultHeaders = res->result->getHeaderFields();
  for (auto const& it : resultHeaders) {
    _response->setHeader(it.first, it.second);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_returns
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandLoggerFollow() {
  bool useVpp = false;
  if (_request->transportType() == Endpoint::TransportType::VPP) {
    useVpp = true;
  }

  // determine start and end tick
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd = UINT64_MAX;

  bool found;
  std::string const& value1 = _request->value("from", found);

  if (found) {
    tickStart = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value1));
  }

  // determine end tick for dump
  std::string const& value2 = _request->value("to", found);

  if (found) {
    tickEnd = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value2));
  }

  if (found && (tickStart > tickEnd || tickEnd == 0)) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }

  bool includeSystem = true;
  std::string const& value4 = _request->value("includeSystem", found);

  if (found) {
    includeSystem = StringUtils::boolean(value4);
  }

  size_t limit = 10000;  // TODO: determine good default value?
  std::string const& value5 = _request->value("chunkSize", found);

  if (found) {
    limit = static_cast<size_t>(StringUtils::uint64(value5));
  }

  VPackBuilder builder;
  builder.openArray();
  auto result = tailWal(_vocbase, tickStart, limit, includeSystem, builder);
  builder.close();
  auto data = builder.slice();

  if (result.ok()) {
    bool const checkMore =
        (result.maxTick() > 0 && result.maxTick() < latestSequenceNumber());

    // generate the result
    size_t length = 0;
    if (useVpp) {
      length = data.length();
    } else {
      length = data.byteSize();
    }

    if (data.length()) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      resetResponse(rest::ResponseCode::OK);
    }

    // transfer ownership of the buffer contents
    _response->setContentType(rest::ContentType::DUMP);

    // set headers
    _response->setHeaderNC(TRI_REPLICATION_HEADER_CHECKMORE,
                           checkMore ? "true" : "false");
    _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTINCLUDED,
                           StringUtils::itoa(result.maxTick()));
    _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTTICK,
                           StringUtils::itoa(latestSequenceNumber()));
    _response->setHeaderNC(TRI_REPLICATION_HEADER_ACTIVE, "true");
    _response->setHeaderNC(TRI_REPLICATION_HEADER_FROMPRESENT,
                           result.fromTickIncluded() ? "true" : "false");

    if (length > 0) {
      if (useVpp) {
        auto iter = arangodb::velocypack::ArrayIterator(data);
        auto opts = arangodb::velocypack::Options::Defaults;
        for (auto message : iter) {
          _response->addPayload(VPackSlice(message), &opts, true);
        }
      } else {
        HttpResponse* httpResponse =
            dynamic_cast<HttpResponse*>(_response.get());

        if (httpResponse == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "invalid response type");
        }

        if (length > 0) {
          httpResponse->body().appendText(data.toJson());
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the command that determines which transactions were open at
/// a given tick value
/// this is an internal method use by ArangoDB's replication that should not
/// be called by client drivers directly
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandDetermineOpenTransactions() {
  generateError(
      rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_YET_IMPLEMENTED,
      "determine-open-transactions API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_inventory
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandInventory() {
  RocksDBReplicationContext* ctx = nullptr;
  bool found, busy;
  std::string batchId = _request->value("batchId", found);
  if (found) {
    ctx = _manager->find(StringUtils::uint64(batchId), busy);
  }
  if (!found || busy || ctx == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND,
                  "batchId not specified");
    return;
  }
  RocksDBReplicationContextGuard(_manager, ctx);

  TRI_voc_tick_t tick = TRI_CurrentTickServer();

  // include system collections?
  bool includeSystem = true;
  std::string const& value = _request->value("includeSystem", found);
  if (found) {
    includeSystem = StringUtils::boolean(value);
  }

  std::pair<RocksDBReplicationResult, std::shared_ptr<VPackBuilder>> result =
      ctx->getInventory(this->_vocbase, includeSystem);
  if (!result.first.ok()) {
    generateError(rest::ResponseCode::BAD, result.first.errorNumber(),
                  "inventory could not be created");
    return;
  }

  VPackSlice const collections = result.second->slice();
  TRI_ASSERT(collections.isArray());

  VPackBuilder builder;
  builder.openObject();

  // add collections data
  builder.add("collections", collections);

  // "state"
  builder.add("state", VPackValue(VPackValueType::Object));

  // RocksDBLogfileManagerState const s =
  // RocksDBLogfileManager::instance()->state();

  builder.add("running", VPackValue(true));
  builder.add("lastLogTick", VPackValue(std::to_string(ctx->lastTick())));
  builder.add("lastUncommittedLogTick",
              VPackValue(std::to_string(0)));  // s.lastAssignedTick
  builder.add("totalEvents", VPackValue(0));   // s.numEvents + s.numEventsSync
  builder.add("time", VPackValue(utilities::timeString()));
  builder.close();  // state

  std::string const tickString(std::to_string(tick));
  builder.add("tick", VPackValue(tickString));
  builder.close();  // Toplevel

  generateResult(rest::ResponseCode::OK, builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_cluster_inventory
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandClusterInventory() {
  std::string const& dbName = _request->databaseName();
  bool found;
  bool includeSystem = true;

  std::string const& value = _request->value("includeSystem", found);

  if (found) {
    includeSystem = StringUtils::boolean(value);
  }

  ClusterInfo* ci = ClusterInfo::instance();
  std::vector<std::shared_ptr<LogicalCollection>> cols =
      ci->getCollections(dbName);

  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  resultBuilder.add(VPackValue("collections"));
  resultBuilder.openArray();
  for (auto const& c : cols) {
    c->toVelocyPackForClusterInventory(resultBuilder, includeSystem);
  }
  resultBuilder.close();  // collections
  TRI_voc_tick_t tick = TRI_CurrentTickServer();
  auto tickString = std::to_string(tick);
  resultBuilder.add("tick", VPackValue(tickString));
  resultBuilder.add("state", VPackValue("unused"));
  resultBuilder.close();  // base
  generateResult(rest::ResponseCode::OK, resultBuilder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the structure of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandRestoreCollection() {
  std::shared_ptr<VPackBuilder> parsedRequest;

  try {
    parsedRequest = _request->toVelocyPackBuilderPtr();
  } catch (arangodb::velocypack::Exception const& e) {
    std::string errorMsg = "invalid JSON: ";
    errorMsg += e.what();
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  errorMsg);
    return;
  } catch (...) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid JSON");
    return;
  }
  VPackSlice const slice = parsedRequest->slice();

  bool overwrite = false;

  bool found;
  std::string const& value1 = _request->value("overwrite", found);

  if (found) {
    overwrite = StringUtils::boolean(value1);
  }

  bool recycleIds = false;
  std::string const& value2 = _request->value("recycleIds", found);

  if (found) {
    recycleIds = StringUtils::boolean(value2);
  }

  bool force = false;
  std::string const& value3 = _request->value("force", found);

  if (found) {
    force = StringUtils::boolean(value3);
  }

  uint64_t numberOfShards = 0;
  std::string const& value4 = _request->value("numberOfShards", found);

  if (found) {
    numberOfShards = StringUtils::uint64(value4);
  }

  uint64_t replicationFactor = 1;
  std::string const& value5 = _request->value("replicationFactor", found);

  if (found) {
    replicationFactor = StringUtils::uint64(value5);
  }

  std::string errorMsg;
  int res;

  if (ServerState::instance()->isCoordinator()) {
    res = processRestoreCollectionCoordinator(slice, overwrite, recycleIds,
                                              force, numberOfShards, errorMsg,
                                              replicationFactor);
  } else {
    res =
        processRestoreCollection(slice, overwrite, recycleIds, force, errorMsg);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("result", VPackValue(true));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandRestoreIndexes() {
  std::shared_ptr<VPackBuilder> parsedRequest;

  try {
    parsedRequest = _request->toVelocyPackBuilderPtr();
  } catch (...) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid JSON");
    return;
  }
  VPackSlice const slice = parsedRequest->slice();

  bool found;
  bool force = false;
  std::string const& value = _request->value("force", found);

  if (found) {
    force = StringUtils::boolean(value);
  }

  std::string errorMsg;
  int res;
  if (ServerState::instance()->isCoordinator()) {
    res = processRestoreIndexesCoordinator(slice, force, errorMsg);
  } else {
    res = processRestoreIndexes(slice, force, errorMsg);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  VPackBuilder result;
  result.openObject();
  result.add("result", VPackValue(true));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

void RocksDBRestReplicationHandler::handleCommandRestoreData() {
  std::string const& colName = _request->value("collection");

  if (colName.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter, not given");
    return;
  }

  bool recycleIds = false;
  std::string const& value2 = _request->value("recycleIds");

  if (!value2.empty()) {
    recycleIds = StringUtils::boolean(value2);
  }

  bool force = false;
  std::string const& value3 = _request->value("force");

  if (!value3.empty()) {
    force = StringUtils::boolean(value3);
  }

  std::string errorMsg;

  int res = processRestoreData(colName, recycleIds, force, errorMsg);

  if (res != TRI_ERROR_NO_ERROR) {
    if (errorMsg.empty()) {
      generateError(GeneralResponse::responseCode(res), res);
    } else {
      generateError(GeneralResponse::responseCode(res), res,
                    std::string(TRI_errno_string(res)) + ": " + errorMsg);
    }
  } else {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("result", VPackValue(true));
    result.close();
    generateResult(rest::ResponseCode::OK, result.slice());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief produce list of keys for a specific collection
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandCreateKeys() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "create keys API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all key ranges
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandGetKeys() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "keys range API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns date for a key range
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandFetchKeys() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "fetch keys API is not implemented for RocksDB yet");
}

void RocksDBRestReplicationHandler::handleCommandRemoveKeys() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "remove keys API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_dump
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandDump() {
  bool found = false;
  uint64_t contextId = 0;

  // get collection Name
  std::string const& collection = _request->value("collection");
  if (collection.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  // get contextId
  std::string const& contextIdString = _request->value("batchId", found);
  if (found) {
    contextId = StringUtils::uint64(contextIdString);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "replication dump - request misses batchId");
    return;
  }

  // acquire context
  bool isBusy = false;
  RocksDBReplicationContext* context = _manager->find(contextId, isBusy);
  RocksDBReplicationContextGuard(_manager, context);
  if (context == nullptr || isBusy) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "replication dump - unable to acquire context");
    return;
  }

  // print request
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
      << "requested collection dump for collection '" << collection
      << "' using contextId '" << context->id() << "'";

  // TODO needs to generalized || velocypacks needs to support multiple slices
  // per response!
  auto response = dynamic_cast<HttpResponse*>(_response.get());
  StringBuffer dump(TRI_UNKNOWN_MEM_ZONE);

  if (response == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid response type");
  }

  auto result = context->dump(_vocbase, collection, dump, determineChunkSize());

  // generate the result
  if (dump.length() == 0) {
    resetResponse(rest::ResponseCode::NO_CONTENT);
  } else {
    resetResponse(rest::ResponseCode::OK);
  }

  response->setContentType(rest::ContentType::DUMP);
  // set headers
  _response->setHeaderNC(TRI_REPLICATION_HEADER_CHECKMORE,
                         (context->more() ? "true" : "false"));

  _response->setHeaderNC(TRI_REPLICATION_HEADER_LASTINCLUDED,
                         StringUtils::itoa(result.maxTick()));

  // transfer ownership of the buffer contents
  response->body().set(dump.stringBuffer());

  // avoid double freeing
  TRI_StealStringBuffer(dump.stringBuffer());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_makeSlave
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandMakeSlave() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "make-slave API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_synchronize
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandSync() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "sync API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_serverID
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandServerId() {
  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  std::string const serverId = StringUtils::itoa(ServerIdFeature::getId());
  result.add("serverId", VPackValue(serverId));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandApplierGetConfig() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "GET applier-config API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_adjust
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandApplierSetConfig() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "set applier-config API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_start
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandApplierStart() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "applier-start API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_stop
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandApplierStop() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "applier-stop API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_applier_state
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandApplierGetState() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "applier-state get API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete the state of the replication applier
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandApplierDeleteState() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "applier-state delete API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a follower of a shard to the list of followers
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandAddFollower() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "add follower API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a follower of a shard from the list of followers
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandRemoveFollower() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "remove follower API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hold a read lock on a collection to stop writes temporarily
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandHoldReadLockCollection() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "hold read lock API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check the holding of a read lock on a collection
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandCheckHoldReadLockCollection() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "check hold read lock API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel the holding of a read lock on a collection
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::
    handleCommandCancelHoldReadLockCollection() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "cancel hold read lock API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get ID for a read lock job
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandGetIdForReadLockCollection() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "get id for read lock API is not implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the structure of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RocksDBRestReplicationHandler::processRestoreCollection(
    VPackSlice const& collection, bool dropExisting, bool reuseId, bool force,
    std::string& errorMsg) {
  if (!collection.isObject()) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const indexes = collection.get("indexes");

  if (!indexes.isArray()) {
    errorMsg = "collection indexes declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  arangodb::LogicalCollection* col = nullptr;

  if (reuseId) {
    TRI_voc_cid_t const cid =
        arangodb::basics::VelocyPackHelper::extractIdValue(parameters);

    if (cid == 0) {
      errorMsg = "collection id is missing";

      return TRI_ERROR_HTTP_BAD_PARAMETER;
    }

    // first look up the collection by the cid
    col = _vocbase->lookupCollection(cid);
  }

  if (col == nullptr) {
    // not found, try name next
    col = _vocbase->lookupCollection(name);
  }

  // drop an existing collection if it exists
  if (col != nullptr) {
    if (dropExisting) {
      Result res = _vocbase->dropCollection(col, true, -1.0);

      if (res.errorNumber() == TRI_ERROR_FORBIDDEN) {
        // some collections must not be dropped

        // instead, truncate them
        SingleCollectionTransaction trx(
            transaction::StandaloneContext::Create(_vocbase), col->cid(),
            AccessMode::Type::WRITE);
        trx.addHint(
            transaction::Hints::Hint::RECOVERY);  // to turn off waitForSync!

        res = trx.begin();
        if (!res.ok()) {
          return res.errorNumber();
        }

        OperationOptions options;
        OperationResult opRes = trx.truncate(name, options);

        res = trx.finish(opRes.code);

        return res.errorNumber();
      }

      if (!res.ok()) {
        errorMsg =
            "unable to drop collection '" + name + "': " + res.errorMessage();
        res.reset(res.errorNumber(), errorMsg);
        return res.errorNumber();
      }
    } else {
      Result res = TRI_ERROR_ARANGO_DUPLICATE_NAME;

      errorMsg =
          "unable to create collection '" + name + "': " + res.errorMessage();
      res.reset(res.errorNumber(), errorMsg);

      return res.errorNumber();
    }
  }

  // now re-create the collection
  int res = createCollection(parameters, &col, reuseId);

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg =
        "unable to create collection: " + std::string(TRI_errno_string(res));

    return res;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the structure of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

int RocksDBRestReplicationHandler::processRestoreCollectionCoordinator(
    VPackSlice const& collection, bool dropExisting, bool reuseId, bool force,
    uint64_t numberOfShards, std::string& errorMsg,
    uint64_t replicationFactor) {
  if (!collection.isObject()) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  std::string dbName = _vocbase->name();

  ClusterInfo* ci = ClusterInfo::instance();

  try {
    // in a cluster, we only look up by name:
    std::shared_ptr<LogicalCollection> col = ci->getCollection(dbName, name);

    // drop an existing collection if it exists
    if (dropExisting) {
      int res = ci->dropCollectionCoordinator(dbName, col->cid_as_string(),
                                              errorMsg, 0.0);
      if (res == TRI_ERROR_FORBIDDEN) {
        // some collections must not be dropped
        res = truncateCollectionOnCoordinator(dbName, name);
        if (res != TRI_ERROR_NO_ERROR) {
          errorMsg =
              "unable to truncate collection (dropping is forbidden): " + name;
          return res;
        }
      }

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "unable to drop collection '" + name + "': " +
                   std::string(TRI_errno_string(res));

        return res;
      }
    } else {
      int res = TRI_ERROR_ARANGO_DUPLICATE_NAME;

      errorMsg = "unable to create collection '" + name + "': " +
                 std::string(TRI_errno_string(res));

      return res;
    }
  } catch (...) {
  }

  // now re-create the collection

  // Build up new information that we need to merge with the given one
  VPackBuilder toMerge;
  toMerge.openObject();

  // We always need a new id
  TRI_voc_tick_t newIdTick = ci->uniqid(1);
  std::string newId = StringUtils::itoa(newIdTick);
  toMerge.add("id", VPackValue(newId));

  // Number of shards. Will be overwritten if not existent
  VPackSlice const numberOfShardsSlice = parameters.get("numberOfShards");
  if (!numberOfShardsSlice.isInteger()) {
    // The information does not contain numberOfShards. Overwrite it.
    VPackSlice const shards = parameters.get("shards");
    if (shards.isObject()) {
      numberOfShards = static_cast<uint64_t>(shards.length());
    } else {
      // "shards" not specified
      // now check if numberOfShards property was given
      if (numberOfShards == 0) {
        // We take one shard if no value was given
        numberOfShards = 1;
      }
    }
    TRI_ASSERT(numberOfShards > 0);
    toMerge.add("numberOfShards", VPackValue(numberOfShards));
  }

  // Replication Factor. Will be overwritten if not existent
  VPackSlice const replFactorSlice = parameters.get("replicationFactor");
  if (!replFactorSlice.isInteger()) {
    if (replicationFactor == 0) {
      replicationFactor = 1;
    }
    TRI_ASSERT(replicationFactor > 0);
    toMerge.add("replicationFactor", VPackValue(replicationFactor));
  }

  // always use current version number when restoring a collection,
  // because the collection is effectively NEW
  toMerge.add("version", VPackValue(LogicalCollection::VERSION_31));
  toMerge.close();  // TopLevel

  VPackSlice const type = parameters.get("type");
  TRI_col_type_e collectionType;
  if (type.isNumber()) {
    collectionType = static_cast<TRI_col_type_e>(type.getNumericValue<int>());
  } else {
    errorMsg = "collection type not given or wrong";
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const sliceToMerge = toMerge.slice();
  VPackBuilder mergedBuilder =
      VPackCollection::merge(parameters, sliceToMerge, false);
  VPackSlice const merged = mergedBuilder.slice();

  try {
    auto col = ClusterMethods::createCollectionOnCoordinator(collectionType,
                                                             _vocbase, merged);
    TRI_ASSERT(col != nullptr);
  } catch (basics::Exception const& e) {
    // Error, report it.
    errorMsg = e.message();
    return e.code();
  }
  // All other errors are thrown to the outside.
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection, based on the VelocyPack provided TODO: MOVE
////////////////////////////////////////////////////////////////////////////////

int RocksDBRestReplicationHandler::createCollection(
    VPackSlice slice, arangodb::LogicalCollection** dst, bool reuseId) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  if (!slice.isObject()) {
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  std::string const name =
      arangodb::basics::VelocyPackHelper::getStringValue(slice, "name", "");

  if (name.empty()) {
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  TRI_voc_cid_t cid = 0;

  if (reuseId) {
    cid = arangodb::basics::VelocyPackHelper::extractIdValue(slice);

    if (cid == 0) {
      return TRI_ERROR_HTTP_BAD_PARAMETER;
    }
  }

  TRI_col_type_e const type = static_cast<TRI_col_type_e>(
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          slice, "type", (int)TRI_COL_TYPE_DOCUMENT));

  arangodb::LogicalCollection* col = nullptr;

  if (cid > 0) {
    col = _vocbase->lookupCollection(cid);
  }

  if (col != nullptr && col->type() == type) {
    // collection already exists. TODO: compare attributes
    return TRI_ERROR_NO_ERROR;
  }

  // always use current version number when restoring a collection,
  // because the collection is effectively NEW
  VPackBuilder patch;
  patch.openObject();
  patch.add("version", VPackValue(LogicalCollection::VERSION_31));
  patch.close();

  VPackBuilder builder = VPackCollection::merge(slice, patch.slice(), false);
  slice = builder.slice();

  col = _vocbase->createCollection(slice);

  if (col == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  TRI_ASSERT(col != nullptr);

  /* Temporary ASSERTS to prove correctness of new constructor */
  TRI_ASSERT(col->isSystem() == (name[0] == '_'));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_voc_cid_t planId = 0;
  VPackSlice const planIdSlice = slice.get("planId");
  if (planIdSlice.isNumber()) {
    planId =
        static_cast<TRI_voc_cid_t>(planIdSlice.getNumericValue<uint64_t>());
  } else if (planIdSlice.isString()) {
    std::string tmp = planIdSlice.copyString();
    planId = static_cast<TRI_voc_cid_t>(StringUtils::uint64(tmp));
  } else if (planIdSlice.isNone()) {
    // There is no plan ID it has to be equal to collection id
    planId = col->cid();
  }

  TRI_ASSERT(col->planId() == planId);
#endif

  if (dst != nullptr) {
    *dst = col;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RocksDBRestReplicationHandler::processRestoreIndexes(
    VPackSlice const& collection, bool force, std::string& errorMsg) {
  if (!collection.isObject()) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const indexes = collection.get("indexes");

  if (!indexes.isArray()) {
    errorMsg = "collection indexes declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackValueLength const n = indexes.length();

  if (n == 0) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_ERROR_NO_ERROR;

  READ_LOCKER(readLocker, _vocbase->_inventoryLock);

  // look up the collection
  try {
    CollectionGuard guard(_vocbase, name.c_str());

    LogicalCollection* collection = guard.collection();

    SingleCollectionTransaction trx(
        transaction::StandaloneContext::Create(_vocbase), collection->cid(),
        AccessMode::Type::WRITE);

    Result res = trx.begin();

    if (!res.ok()) {
      errorMsg = "unable to start transaction: " + res.errorMessage();
      res.reset(res.errorNumber(), errorMsg);
      THROW_ARANGO_EXCEPTION(res);
    }

    auto physical = collection->getPhysical();
    TRI_ASSERT(physical != nullptr);
    for (VPackSlice const& idxDef : VPackArrayIterator(indexes)) {
      std::shared_ptr<arangodb::Index> idx;

      // {"id":"229907440927234","type":"hash","unique":false,"fields":["x","Y"]}

      res = physical->restoreIndex(&trx, idxDef, idx);

      if (res.errorNumber() == TRI_ERROR_NOT_IMPLEMENTED) {
        continue;
      }

      if (res.fail()) {
        errorMsg = "could not create index: " + res.errorMessage();
        res.reset(res.errorNumber(), errorMsg);
        break;
      }
      TRI_ASSERT(idx != nullptr);
    }

    if (res.fail()) {
      return res.errorNumber();
    }
    res = trx.commit();

  } catch (arangodb::basics::Exception const& ex) {
    // fix error handling
    errorMsg =
        "could not create index: " + std::string(TRI_errno_string(ex.code()));
  } catch (...) {
    errorMsg = "could not create index: unknown error";
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

int RocksDBRestReplicationHandler::processRestoreIndexesCoordinator(
    VPackSlice const& collection, bool force, std::string& errorMsg) {
  if (!collection.isObject()) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }
  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice indexes = collection.get("indexes");

  if (!indexes.isArray()) {
    errorMsg = "collection indexes declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  size_t const n = static_cast<size_t>(indexes.length());

  if (n == 0) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  std::string name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  std::string dbName = _vocbase->name();

  // in a cluster, we only look up by name:
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<LogicalCollection> col;
  try {
    col = ci->getCollection(dbName, name);
  } catch (...) {
    errorMsg = "could not find collection '" + name + "'";
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  TRI_ASSERT(col != nullptr);

  int res = TRI_ERROR_NO_ERROR;
  for (VPackSlice const& idxDef : VPackArrayIterator(indexes)) {
    VPackSlice type = idxDef.get("type");
    if (type.isString() &&
        (type.copyString() == "primary" || type.copyString() == "edge")) {
      // must ignore these types of indexes during restore
      continue;
    }

    VPackBuilder tmp;
    res = ci->ensureIndexCoordinator(dbName, col->cid_as_string(), idxDef, true,
                                     arangodb::Index::Compare, tmp, errorMsg,
                                     3600.0);
    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg =
          "could not create index: " + std::string(TRI_errno_string(res));
      break;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RocksDBRestReplicationHandler::processRestoreDataBatch(
    transaction::Methods& trx, std::string const& collectionName,
    bool useRevision, bool force, std::string& errorMsg) {
  std::string const invalidMsg =
      "received invalid JSON data for collection " + collectionName;

  VPackBuilder builder;

  HttpRequest* httpRequest = dynamic_cast<HttpRequest*>(_request.get());
  if (httpRequest == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid request type");
  }

  std::string const& bodyStr = httpRequest->body();
  char const* ptr = bodyStr.c_str();
  char const* end = ptr + bodyStr.size();

  VPackBuilder allMarkers;
  VPackValueLength currentPos = 0;
  std::unordered_map<std::string, VPackValueLength> latest;

  // First parse and collect all markers, we assemble everything in one
  // large builder holding an array. We keep for each key the latest
  // entry.

  {
    VPackArrayBuilder guard(&allMarkers);
    std::string key;
    while (ptr < end) {
      char const* pos = strchr(ptr, '\n');

      if (pos == nullptr) {
        pos = end;
      } else {
        *((char*)pos) = '\0';
      }

      if (pos - ptr > 1) {
        // found something
        key.clear();
        VPackSlice doc;
        TRI_replication_operation_e type = REPLICATION_INVALID;

        int res = restoreDataParser(ptr, pos, invalidMsg, useRevision, errorMsg,
                                    key, builder, doc, type);
        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }

        // Put into array of all parsed markers:
        allMarkers.add(builder.slice());
        auto it = latest.find(key);
        if (it != latest.end()) {
          // Already found, overwrite:
          it->second = currentPos;
        } else {
          latest.emplace(std::make_pair(key, currentPos));
        }
        ++currentPos;
      }

      ptr = pos + 1;
    }
  }

  // First remove all keys of which the last marker we saw was a deletion
  // marker:
  VPackSlice allMarkersSlice = allMarkers.slice();
  VPackBuilder oldBuilder;
  {
    VPackArrayBuilder guard(&oldBuilder);

    for (auto const& p : latest) {
      VPackSlice const marker = allMarkersSlice.at(p.second);
      VPackSlice const typeSlice = marker.get("type");
      TRI_replication_operation_e type = REPLICATION_INVALID;
      if (typeSlice.isNumber()) {
        int typeInt = typeSlice.getNumericValue<int>();
        if (typeInt == 2301) {  // pre-3.0 type for edges
          type = REPLICATION_MARKER_DOCUMENT;
        } else {
          type = static_cast<TRI_replication_operation_e>(typeInt);
        }
      }
      if (type == REPLICATION_MARKER_REMOVE) {
        oldBuilder.add(VPackValue(p.first));  // Add _key
      } else if (type != REPLICATION_MARKER_DOCUMENT) {
        errorMsg = "unexpected marker type " + StringUtils::itoa(type);
        return TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
      }
    }
  }

  // Note that we ignore individual errors here, as long as the main
  // operation did not fail. In particular, we intentionally ignore
  // individual "DOCUMENT NOT FOUND" errors, because they can happen!
  try {
    OperationOptions options;
    options.silent = true;
    options.ignoreRevs = true;
    options.isRestore = true;
    options.waitForSync = false;
    OperationResult opRes =
        trx.remove(collectionName, oldBuilder.slice(), options);
    if (!opRes.successful()) {
      return opRes.code;
    }
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  // Now try to insert all keys for which the last marker was a document
  // marker, note that these could still be replace markers!
  builder.clear();
  {
    VPackArrayBuilder guard(&builder);

    for (auto const& p : latest) {
      VPackSlice const marker = allMarkersSlice.at(p.second);
      VPackSlice const typeSlice = marker.get("type");
      TRI_replication_operation_e type = REPLICATION_INVALID;
      if (typeSlice.isNumber()) {
        int typeInt = typeSlice.getNumericValue<int>();
        if (typeInt == 2301) {  // pre-3.0 type for edges
          type = REPLICATION_MARKER_DOCUMENT;
        } else {
          type = static_cast<TRI_replication_operation_e>(typeInt);
        }
      }
      if (type == REPLICATION_MARKER_DOCUMENT) {
        VPackSlice const doc = marker.get("data");
        TRI_ASSERT(doc.isObject());
        builder.add(doc);
      }
    }
  }

  VPackSlice requestSlice = builder.slice();
  OperationResult opRes;
  try {
    OperationOptions options;
    options.silent = false;
    options.ignoreRevs = true;
    options.isRestore = true;
    options.waitForSync = false;
    opRes = trx.insert(collectionName, requestSlice, options);
    if (!opRes.successful()) {
      return opRes.code;
    }
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  // Now go through the individual results and check each error, if it was
  // TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED, then we have to call
  // replace on the document:
  VPackSlice resultSlice = opRes.slice();
  VPackBuilder replBuilder;  // documents for replace operation
  {
    VPackArrayBuilder guard(&oldBuilder);
    VPackArrayBuilder guard2(&replBuilder);
    VPackArrayIterator itRequest(requestSlice);
    VPackArrayIterator itResult(resultSlice);

    while (itRequest.valid()) {
      VPackSlice result = *itResult;
      VPackSlice error = result.get("error");
      if (error.isTrue()) {
        error = result.get("errorNum");
        if (error.isNumber()) {
          int code = error.getNumericValue<int>();
          if (code == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
            replBuilder.add(*itRequest);
          } else {
            return code;
          }
        } else {
          return TRI_ERROR_INTERNAL;
        }
      }
      itRequest.next();
      itResult.next();
    }
  }
  try {
    OperationOptions options;
    options.silent = true;
    options.ignoreRevs = true;
    options.isRestore = true;
    options.waitForSync = false;
    opRes = trx.replace(collectionName, replBuilder.slice(), options);
    if (!opRes.successful()) {
      return opRes.code;
    }
  } catch (arangodb::basics::Exception const& ex) {
    return ex.code();
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RocksDBRestReplicationHandler::processRestoreData(
    std::string const& colName, bool useRevision, bool force,
    std::string& errorMsg) {
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::Create(_vocbase), colName,
      AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::RECOVERY);  // to turn off waitForSync!

  Result res = trx.begin();

  if (!res.ok()) {
    errorMsg = "unable to start transaction: " + res.errorMessage();
    res.reset(res.errorNumber(), errorMsg);
    return res.errorNumber();
  }

  int resCode =
      processRestoreDataBatch(trx, colName, useRevision, force, errorMsg);
  res.reset(resCode, errorMsg);
  res = trx.finish(res);

  return res.errorNumber();
}

int restoreDataParser(char const* ptr, char const* pos,
                      std::string const& invalidMsg, bool useRevision,
                      std::string& errorMsg, std::string& key,
                      VPackBuilder& builder, VPackSlice& doc,
                      TRI_replication_operation_e& type) {
  builder.clear();

  try {
    VPackParser parser(builder);
    parser.parse(ptr, static_cast<size_t>(pos - ptr));
  } catch (VPackException const&) {
    // Could not parse the given string
    errorMsg = invalidMsg;

    return TRI_ERROR_HTTP_CORRUPTED_JSON;
  } catch (std::exception const&) {
    // Could not even build the string
    errorMsg = invalidMsg;

    return TRI_ERROR_HTTP_CORRUPTED_JSON;
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  VPackSlice const slice = builder.slice();

  if (!slice.isObject()) {
    errorMsg = invalidMsg;

    return TRI_ERROR_HTTP_CORRUPTED_JSON;
  }

  type = REPLICATION_INVALID;

  for (auto const& pair : VPackObjectIterator(slice, true)) {
    if (!pair.key.isString()) {
      errorMsg = invalidMsg;

      return TRI_ERROR_HTTP_CORRUPTED_JSON;
    }

    std::string const attributeName = pair.key.copyString();

    if (attributeName == "type") {
      if (pair.value.isNumber()) {
        int v = pair.value.getNumericValue<int>();
        if (v == 2301) {
          // pre-3.0 type for edges
          type = REPLICATION_MARKER_DOCUMENT;
        } else {
          type = static_cast<TRI_replication_operation_e>(v);
        }
      }
    }

    else if (attributeName == "data") {
      if (pair.value.isObject()) {
        doc = pair.value;

        if (doc.hasKey(StaticStrings::KeyString)) {
          key = doc.get(StaticStrings::KeyString).copyString();
        }
      }
    }

    else if (attributeName == "key") {
      if (key.empty()) {
        key = pair.value.copyString();
      }
    }
  }

  if (type == REPLICATION_MARKER_DOCUMENT && !doc.isObject()) {
    errorMsg = "got document marker without contents";
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (key.empty()) {
    errorMsg = invalidMsg;

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the chunk size
////////////////////////////////////////////////////////////////////////////////

uint64_t RocksDBRestReplicationHandler::determineChunkSize() const {
  // determine chunk size
  uint64_t chunkSize = _defaultChunkSize;

  bool found;
  std::string const& value = _request->value("chunkSize", found);

  if (found) {
    // query parameter "chunkSize" was specified
    chunkSize = StringUtils::uint64(value);

    // don't allow overly big allocations
    if (chunkSize > _maxChunkSize) {
      chunkSize = _maxChunkSize;
    }
  }

  return chunkSize;
}
