/*
 *  Copyright (c) 2015-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include <memory>
#include <vector>

#include <folly/SocketAddress.h>

namespace proxygen {

using LoadType = uint32_t;

struct ServerLoadInfo {
  double cpuUser{-1.};
  double cpuSys{-1.};
  double cpuIdle{-1.};
  LoadType queueLen{0};
};

enum class HealthCheckSource {
  INTERNAL = 0,
  EXTERNAL = 1,
};

enum ServerDownInfo {
  NONE = 0,

  PASSIVE_HEALTHCHECK_FAIL = 1,
  HEALTHCHECK_TIMEOUT = 2,
  HEALTHCHECK_BODY_MISMATCH = 3,
  HEALTHCHECK_NON200_STATUS = 4,
  HEALTHCHECK_MESSAGE_ERROR = 5,
  HEALTHCHECK_WRITE_ERROR = 6,
  HEALTHCHECK_UPGRADE_ERROR = 7,
  HEALTHCHECK_EOF = 8,
  HEALTHCHECK_CONNECT_ERROR = 9,
  FEEDBACK_LOOP_HIGH_LOAD = 10,

  HEALTHCHECK_UNKNOWN_ERROR = 99,
};

const std::string serverDownInfoStr(ServerDownInfo info);

/*
 * ServerHealthCheckerCallback is the interface for receiving health check
 * responses. The caller may be from a different thread.
 */
class ServerHealthCheckerCallback {
 public:
  // Additional info received from a successful healthcheck (e.g. HTTP headers)
  using ExtraInfo = std::vector<std::pair<std::string, std::string>>;

  virtual void processHealthCheckFailure(
      ServerDownInfo reason,
      const std::string& extraReasonStr = std::string(),
      HealthCheckSource source = HealthCheckSource::INTERNAL) = 0;

  virtual void processHealthCheckSuccess(
      LoadType load,
      const ServerLoadInfo* serverLoadInfo = nullptr,
      const ExtraInfo* extraInfo = nullptr,
      HealthCheckSource source = HealthCheckSource::INTERNAL) = 0;

  virtual ~ServerHealthCheckerCallback() {
  }
};

} // namespace proxygen
