# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

if(NOT BUILD_TESTS)
  return()
endif()

file(
  GLOB_RECURSE QUIC_API_HEADERS_TOINSTALL
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  *.h
)
list(FILTER QUIC_API_HEADERS_TOINSTALL EXCLUDE REGEX test/)
foreach(header ${QUIC_API_HEADERS_TOINSTALL})
  get_filename_component(header_dir ${header} DIRECTORY)
  install(FILES ${header} DESTINATION include/quic/samples/${header_dir})
endforeach()

add_executable(echo_client echo/client.cpp echo/config_file_parser.cpp echo/config_file_parser.h)

target_compile_options(
  echo_client
  PRIVATE
  ${_QUIC_COMMON_COMPILE_OPTIONS}
)

add_dependencies(echo_client googletest)

target_link_libraries(
  echo_client PUBLIC
  mvfst_test_utils
  ${GFLAGS_LIBRARIES}
  ${LIBGMOCK_LIBRARIES}
  ${LIBGTEST_LIBRARIES}
)

add_executable(echo_server echo/server.cpp echo/config_file_parser.cpp echo/config_file_parser.h)

target_compile_options(
  echo_server
  PRIVATE
  ${_QUIC_COMMON_COMPILE_OPTIONS}
)

add_dependencies(echo_server googletest)

target_link_libraries(
  echo_server PUBLIC
  mvfst_test_utils
  ${GFLAGS_LIBRARIES}
  ${LIBGMOCK_LIBRARIES}
  ${LIBGTEST_LIBRARIES}
)
