/*
 *  Copyright (c) 2019-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include <boost/variant.hpp>
#include <deque>
#include <folly/Optional.h>
#include <folly/Range.h>
#include <folly/io/Cursor.h>

#include <proxygen/lib/http/HTTP3ErrorCode.h>
#include <proxygen/lib/http/codec/SettingsId.h>
#include <quic/codec/QuicInteger.h>
#include <quic/codec/Types.h>

namespace proxygen { namespace hq {

//////// Constants ////////
// Frame headers have a variable length between 2 and 16 Bytes
const size_t kMaxFrameHeaderSize = 16;
// Index for the maximum GREASE ID allowed on the wire
const uint64_t kMaxGreaseIdIndex = 0x210842108421083;

// Unframed body DATA frame length.
const size_t kUnframedDataFrameLen = 0;

// PushID mask
// to make sure push id and stream id spaces are disjoint
const uint64_t kPushIdMask = ((uint64_t)1) << 63;

//////// Types ////////

using PushId = uint64_t;

// Internally the push IDs have a high bit set
// to prevent a collision with a stream id.
bool isInternalPushId(PushId pushId);

// Externally the push IDs do not have the high bit
// set.
bool isExternalPushId(PushId pushId);

using ParseResult = folly::Optional<HTTP3::ErrorCode>;
using WriteResult = folly::Expected<size_t, quic::TransportErrorCode>;

enum class FrameType : uint64_t {
  DATA = 0x00,
  HEADERS = 0x01,
  PRIORITY = 0x02,
  CANCEL_PUSH = 0x03,
  SETTINGS = 0x04,
  PUSH_PROMISE = 0x05,
  // 0x06 reserved
  GOAWAY = 0x07,
  // 0x08 reserved
  // 0x09 reserved
  MAX_PUSH_ID = 0x0D,
};

struct FrameHeader {
  FrameType type;
  uint64_t length;
};

enum PriorityElementType {
  REQUEST_STREAM = 0x00,
  PUSH_STREAM = 0x01,
  PLACEHOLDER = 0x02,
  TREE_ROOT = 0x03,
};

// The first byte in a Priority Frame has multiple fields
const uint8_t PRIORITIZED_TYPE_POS = 6;
const uint8_t DEPENDENCY_TYPE_POS = 4;
const uint8_t PRIORITY_EMPTY_POS = 1;
const uint8_t PRIORITY_EXCLUSIVE_MASK = 0x01;

struct PriorityUpdate {
  PriorityUpdate(PriorityElementType pt,
                 PriorityElementType dt,
                 bool ex,
                 uint64_t pe,
                 uint64_t ed,
                 uint8_t wt)
      : prioritizedType(pt),
        dependencyType(dt),
        prioritizedElementId(pe),
        elementDependencyId(ed),
        weight(wt),
        exclusive(ex) {
  }

  PriorityUpdate()
      : prioritizedType(PriorityElementType::REQUEST_STREAM),
        dependencyType(PriorityElementType::TREE_ROOT),
        prioritizedElementId(0),
        elementDependencyId(0),
        weight(0),
        exclusive(false) {
  }

  PriorityElementType prioritizedType;
  PriorityElementType dependencyType;
  // the prioritized element ID can be a stream ID, a push ID or a placeholder
  // ID, based on prioritized Type
  uint64_t prioritizedElementId;
  // the element dependency ID can be a stream ID, a push ID or a placeholder
  // ID, based on dependency Type
  uint64_t elementDependencyId;
  uint8_t weight;
  bool exclusive;
};

enum class SettingId : uint64_t {
  HEADER_TABLE_SIZE = 0x01,
  MAX_HEADER_LIST_SIZE = 0x06,
  QPACK_BLOCKED_STREAMS = 0x07,
  NUM_PLACEHOLDERS = 0x09,
};

using SettingValue = uint64_t;
using SettingPair = std::pair<SettingId, SettingValue>;

//////// Functions ////////
folly::Optional<uint64_t> getGreaseId(uint64_t n);
bool isGreaseId(uint64_t id);
bool frameAffectsCompression(FrameType type);

//// Parsing ////

/**
 * This function parses the section of the DATA frame after the common
 * frame header and returns the body data in outBuf.
 * It pulls header.length bytes from the cursor, so it is the
 * caller's responsibility to ensure there is enough data available.
 *
 * @param cursor  The cursor to pull data from.
 * @param header  The frame header for the frame being parsed.
 * @param outBuf  The buf to fill with body data.
 * @return folly::none for successful parse or the quic application error code.
 */
ParseResult parseData(folly::io::Cursor& cursor,
                      const FrameHeader& header,
                      std::unique_ptr<folly::IOBuf>& outBuf) noexcept;

/**
 * This function parses the section of the HEADERS frame after the common
 * frame header and returns the header data in outBuf.
 * It pulls header.length bytes from the cursor, so it is the
 * caller's responsibility to ensure there is enough data available.
 *
 * @param cursor The cursor to pull data from.
 * @param header The frame header for the frame being parsed.
 * @param outBuf The buf to fill with header data.
 * @return folly::none for successful parse or the quic application error code.
 */
ParseResult parseHeaders(folly::io::Cursor& cursor,
                         const FrameHeader& header,
                         std::unique_ptr<folly::IOBuf>& outBuf) noexcept;

/**
 * This function parses the section of the PRIORITY frame after the common
 * frame header. It pulls header.length bytes from the cursor, so it is the
 * caller's responsibility to ensure there is enough data available.
 * It fetches priority information both from the frame payload and from the
 * frame header.
 *
 * @param cursor The cursor to pull data from.
 * @param header The frame header for the frame being parsed.
 * @param outPriority On success, filled with the priority information
 *                    from this frame.
 * @return folly::none for successful parse or the quic application error code.
 */
ParseResult parsePriority(folly::io::Cursor& cursor,
                          const FrameHeader& header,
                          PriorityUpdate& outPriority) noexcept;

/**
 * This function parses the section of the CANCEL_PUSH frame after the common
 * frame header. It pulls header.length bytes from the cursor, so it is the
 * caller's responsibility to ensure there is enough data available.
 *
 * @param cursor The cursor to pull data from.
 * @param header The frame header for the frame being parsed.
 * @param outPushId On success, filled with the push ID to cancel
 * @return folly::none for successful parse or the quic application error code.
 */
ParseResult parseCancelPush(folly::io::Cursor& cursor,
                            const FrameHeader& header,
                            PushId& outPushId) noexcept;

/**
 * This function parses the section of the SETTINGS frame after the
 * common frame header. It pulls header.length bytes from the cursor, so
 * it is the caller's responsibility to ensure there is enough data
 * available.
 *
 * @param cursor The cursor to pull data from.
 * @param header The frame header for the frame being parsed.
 * @param settings The settings received in this frame.
 * @return folly::none for successful parse or the quic application error code.
 */
ParseResult parseSettings(folly::io::Cursor& cursor,
                          const FrameHeader& header,
                          std::deque<SettingPair>& settings) noexcept;

/**
 * This function parses the section of the PUSH_PROMISE frame after the
 * common frame header. It pulls header.length bytes from the cursor, so
 * it is the caller's responsibility to ensure there is enough data
 * available.
 *
 * @param cursor The cursor to pull data from.
 * @param header The frame header for the frame being parsed.
 * @param outPushId the Push ID of the server push request.
 * @param outBuf The buffer to fill with header data.
 * @return folly::none for successful parse or the quic application error code.
 */
ParseResult parsePushPromise(folly::io::Cursor& cursor,
                             const FrameHeader& header,
                             PushId& outPushId,
                             std::unique_ptr<folly::IOBuf>& outBuf) noexcept;

/**
 * This function parses the section of the GOAWAY frame after the common
 * frame header.  It pulls header.length bytes from the cursor, so
 * it is the caller's responsibility to ensure there is enough data
 * available.
 *
 * @param cursor The cursor to pull data from.
 * @param header The frame header for the frame being parsed.
 * @param outStreamID The last stream ID accepted by the remote.
 * @return folly::none for successful parse or the quic application error code.
 */
ParseResult parseGoaway(folly::io::Cursor& cursor,
                        const FrameHeader& header,
                        quic::StreamId& outStreamId) noexcept;

/**
 * This function parses the section of the MAX_PUSH_ID frame after the common
 * frame header.  It pulls header.length bytes from the cursor, so
 * it is the caller's responsibility to ensure there is enough data
 * available.
 *
 * @param cursor The cursor to pull data from.
 * @param header The frame header for the frame being parsed.
 * @param outPushID the maximum value for a Push ID.
 * @return folly::none for successful parse or the quic application error code.
 */
ParseResult parseMaxPushId(folly::io::Cursor& cursor,
                           const FrameHeader& header,
                           PushId& outPushId) noexcept;

//// Egress ////

/**
 * Generate just the common frame header. Returns the total frame header length
 *
 * @param queue   Queue to write to.
 * @param type    Header type.
 * @param length  Length of the payload for the header.
 */
WriteResult writeFrameHeader(folly::IOBufQueue& queue,
                             FrameType type,
                             uint64_t length) noexcept;

/**
 * Generate an entire DATA frame, including the common frame header.
 *
 * @param writeBuf The output queue to write to. It may grow or add
 *                 underlying buffers inside this function.
 * @param data The body data to write out, cannot be nullptr
 * @return The number of bytes written to writeBuf if successful, a quic error
 * otherwise
 */
WriteResult writeData(folly::IOBufQueue& writeBuf,
                      std::unique_ptr<folly::IOBuf> data) noexcept;

/**
 * Write unframed bytes into the buffer.
 *
 * @param writeBuf The output queue to write to. It may grow or add
 *                 underlying buffers inside this function.
 * @param data The body data to write out, cannot be nullptr
 * @return The number of bytes written to writeBuf if successful, a quic error
 * otherwise
 */
WriteResult writeUnframedBytes(folly::IOBufQueue& writeBuf,
                               std::unique_ptr<folly::IOBuf> data) noexcept;

/**
 * Generate an entire HEADER frame, including the common frame header.
 *
 * @param writeBuf The output queue to write to. It may grow or add
 *                 underlying buffers inside this function.
 * @param data The body data to write out, cannot be nullptr
 * @return The number of bytes written to writeBuf if successful, a quic error
 * otherwise
 */
WriteResult writeHeaders(folly::IOBufQueue& writeBuf,
                         std::unique_ptr<folly::IOBuf> data) noexcept;

/**
 * Generate an entire PRIORITY frame, including the common frame header.
 *
 * @param writeBuf The output queue to write to. It may grow or add
 *                 underlying buffers inside this function.
 * @param priority The priority depedency information to update the stream with.
 * @return The number of bytes written to writeBuf if successful, a quic error
 * otherwise
 */
WriteResult writePriority(folly::IOBufQueue& writeBuf,
                          PriorityUpdate priority) noexcept;

/**
 * Generate an entire CANCEL_PUSH frame, including the common frame
 * header.
 *
 * @param writeBuf The output queue to write to. It may grow or add
 *                 underlying buffers inside this function.
 * @param pushId The identifier of the  the server push that is being cancelled.
 * @return The number of bytes written to writeBuf if successful, a quic error
 * otherwise
 */
WriteResult writeCancelPush(folly::IOBufQueue& writeBuf,
                            PushId pushId) noexcept;

/**
 * Generate an entire SETTINGS frame, including the common frame
 * header.
 *
 * @param writeBuf The output queue to write to. It may grow or add
 *                 underlying buffers inside this function.
 * @param settings The settings to send
 * @return The number of bytes written to writeBuf if successful, a quic error
 * otherwise
 */
WriteResult writeSettings(folly::IOBufQueue& writeBuf,
                          const std::deque<SettingPair>& settings);

/**
 * Generate an entire PUSH_PROMISE frame, including the common frame header.
 *
 * @param writeBuf The output queue to write to. It may grow or add
 *                 underlying buffers inside this function.
 * @param pushId the identifier of the server push request
 * @param data The body data to write out, cannot be nullptr
 * @return The number of bytes written to writeBuf if successful, a quic error
 * otherwise
 */
WriteResult writePushPromise(folly::IOBufQueue& writeBuf,
                             PushId pushId,
                             std::unique_ptr<folly::IOBuf> data) noexcept;

/**
 * Generate an entire GOAWAY frame, including the common frame
 * header.
 *
 * @param writeBuf The output queue to write to. It may grow or add
 *                 underlying buffers inside this function.
 * @param lastStreamId The identifier of the last stream accepted.
 * @return The number of bytes written to writeBuf if successful, a quic error
 * otherwise
 */
WriteResult writeGoaway(folly::IOBufQueue& writeBuf,
                        quic::StreamId lastStreamId) noexcept;

/**
 * Generate an entire MAX_PUSH_ID frame, including the common frame
 * header.
 *
 * @param writeBuf The output queue to write to. It may grow or add
 *                 underlying buffers inside this function.
 * @param maxPushId The identifier of the maximum value for a Push ID that the
 * server can use.
 * @return The number of bytes written to writeBuf if successful, a quic error
 * otherwise
 */
WriteResult writeMaxPushId(folly::IOBufQueue& writeBuf,
                           PushId maxPushId) noexcept;

}} // namespace proxygen::hq
