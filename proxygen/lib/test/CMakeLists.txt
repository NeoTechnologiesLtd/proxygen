# Copyright (c) 2019-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.

if(NOT BUILD_TESTING)
    return()
endif()

add_library(testtransport STATIC TestAsyncTransport.cpp)
target_include_directories(
    testtransport PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    ${LIBGMOCK_INCLUDE_DIR}
    ${LIBGTEST_INCLUDE_DIR}
)
target_link_libraries(testtransport PRIVATE proxygen)
add_dependencies(testtransport googletest)

add_library(testmain STATIC TestMain.cpp)
target_include_directories(
    testmain PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    ${LIBGMOCK_INCLUDE_DIR}
    ${LIBGTEST_INCLUDE_DIR}
)
target_link_libraries(testmain PRIVATE Folly::folly)
add_dependencies(testmain googletest)
