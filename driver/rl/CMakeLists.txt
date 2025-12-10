set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(spdlog REQUIRED)

if (NOT gRPC_CPP_PLUGIN_EXECUTABLE)
  get_target_property(gRPC_CPP_PLUGIN_EXECUTABLE gRPC::grpc_cpp_plugin LOCATION)
endif()
if (NOT gRPC_CPP_PLUGIN_EXECUTABLE)
  find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
endif()
if (NOT gRPC_CPP_PLUGIN_EXECUTABLE)
  message(FATAL_ERROR "gRPC cpp plugin not found")
endif()

set(RL_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/rl.proto)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(RL_PROTO_SRCS
    ${GENERATED_DIR}/rl.pb.cc
    ${GENERATED_DIR}/rl.grpc.pb.cc
)
set(RL_PROTO_HDRS
    ${GENERATED_DIR}/rl.pb.h
    ${GENERATED_DIR}/rl.grpc.pb.h
)

set(GIT_VERSION_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/git_version.cpp.in)
set(GIT_VERSION_OUTPUT ${GENERATED_DIR}/git_version.cpp)
set(GIT_VERSION_STATE ${GENERATED_DIR}/git-state.txt)
set(GIT_VERSION_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/UpdateGitVersion.cmake)

add_custom_target(update_git_version_rl
  COMMAND ${CMAKE_COMMAND}
    -DGIT_TEMPLATE:PATH=${GIT_VERSION_TEMPLATE}
    -DGIT_OUTPUT:PATH=${GIT_VERSION_OUTPUT}
    -DGIT_STATE_FILE:PATH=${GIT_VERSION_STATE}
    -DSOURCE_DIR:PATH=${CMAKE_SOURCE_DIR}
    -P ${GIT_VERSION_SCRIPT}
  COMMENT "Updating git hash for RLDriver"
  VERBATIM
)

set_source_files_properties(${GIT_VERSION_OUTPUT} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${RL_PROTO_SRCS} ${RL_PROTO_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
      --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
      --cpp_out=${GENERATED_DIR}
      ${RL_PROTO}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
      --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
      --grpc_out=${GENERATED_DIR}
      --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN_EXECUTABLE}
      ${RL_PROTO}
    DEPENDS ${RL_PROTO}
    COMMENT "Generating gRPC sources for RL driver"
)

add_executable(RLDriver main.cpp ${RL_PROTO_SRCS})
target_include_directories(RLDriver PUBLIC
    ${CMAKE_SOURCE_DIR}/runtime
    ${GENERATED_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(RLDriver PRIVATE
  launcher
  z3parser
  z3
  gRPC::grpc++
  protobuf::libprotobuf
  spdlog::spdlog
  rt
)
add_dependencies(RLDriver update_git_version_rl)
target_sources(RLDriver PRIVATE ${GIT_VERSION_OUTPUT})

install(TARGETS RLDriver DESTINATION ${SYMSAN_BIN_DIR})
