////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/JemallocFeature.h"

#include "Basics/FileUtils.h"
#include "Basics/process-utils.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"

#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

char JemallocFeature::_staticPath[PATH_MAX + 1];

JemallocFeature::JemallocFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Jemalloc"), _defaultPath("./") {
  setOptional(false);
  requiresElevatedPrivileges(false);
}

void JemallocFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
#if ARANGODB_MMAP_JEMALLOC
  options->addSection("vm", "Virtual memory");

  options->addOption("--vm.resident-limit", "resident limit in bytes",
                     new UInt64Parameter(&_residentLimit, TRI_PhysicalMemory));

  options->addOption("--vm.path", "path to the directory for vm files",
                     new StringParameter(&_path));
#endif
}

void JemallocFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
#if ARANGODB_MMAP_JEMALLOC
  static uint64_t MIN_LIMIT = 512 * 1024 * 1024;

  if (0 < _residentLimit && _residentLimit < MIN_LIMIT) {
    LOG_TOPIC(WARN, Logger::MEMORY) << "vm.resident-limit of " << _residentLimit
                                    << " is too small, using " << MIN_LIMIT;

    _residentLimit = MIN_LIMIT;
  }

  if (!_path.empty()) {
    FileUtils::makePathAbsolute(_path);
    FileUtils::normalizePath(_path);
    _path += TRI_DIR_SEPARATOR_STR;
  }

  LOG_TOPIC(INFO, Logger::MEMORY)
      << "using jemalloc with vm.resident-limit = " << _residentLimit
      << ", vm.path = '" << _path << "'";
#else
  LOG_TOPIC(INFO, Logger::MEMORY) << "jemalloc has been disabled";
#endif
}

#if ARANGODB_MMAP_JEMALLOC
extern "C" void adb_jemalloc_set_limit(size_t limit, char const* path);
#endif

void JemallocFeature::setDefaultPath(std::string const& path) {
  _defaultPath = path;
  FileUtils::makePathAbsolute(_defaultPath);
  FileUtils::normalizePath(_defaultPath);

  _defaultPath += TRI_DIR_SEPARATOR_STR;
  _defaultPath += "vm";
  _defaultPath += TRI_DIR_SEPARATOR_STR;
}

void JemallocFeature::start() {
#if ARANGODB_MMAP_JEMALLOC
  *_staticPath = '\0';

  if (0 < _residentLimit) {
    if (_path.empty()) {
      strncat(_staticPath, _defaultPath.c_str(), PATH_MAX);
    } else {
      strncat(_staticPath, _path.c_str(), PATH_MAX);
    }

    LOG_TOPIC(DEBUG, Logger::MEMORY) << "using path " << _staticPath;

    if (!FileUtils::isDirectory(_staticPath)) {
      if (!FileUtils::createDirectory(_staticPath, 0700)) {
        LOG_TOPIC(FATAL, Logger::MEMORY)
            << "cannot create directory '" << _staticPath
            << "' for VM files: " << strerror(errno);
        FATAL_ERROR_EXIT();
      }
    }

    adb_jemalloc_set_limit(_residentLimit, _staticPath);
  }
#endif
}
