cmake_minimum_required(VERSION 3.13)
project(symsan-if-instrument LANGUAGES CXX)

if(NOT DEFINED LLVM_DIR)
  find_package(LLVM 14 REQUIRED CONFIG)
endif()

if(NOT Clang_DIR AND DEFINED LLVM_DIR)
  get_filename_component(_llvm_lib_dir "${LLVM_DIR}/.." ABSOLUTE)
  if(EXISTS "${_llvm_lib_dir}/clang")
    set(Clang_DIR "${_llvm_lib_dir}/clang")
  endif()
endif()
find_package(Clang REQUIRED CONFIG)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure the plugin library is never unloaded (avoids LLVM dlclose issues).
if(NOT APPLE)
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-znodelete")
else()
  set(CMAKE_MODULE_LINKER_FLAGS
      "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-flat_namespace -Wl,-undefined,suppress")
endif()

add_library(IfInstrument MODULE IfInstrument.cpp)
target_include_directories(IfInstrument PRIVATE ${LLVM_INCLUDE_DIRS}
                                             ${CLANG_INCLUDE_DIRS})
target_compile_definitions(IfInstrument PRIVATE ${LLVM_DEFINITIONS})
if(TARGET clang-cpp)
  target_link_libraries(IfInstrument PRIVATE clang-cpp)
else()
  target_link_libraries(IfInstrument PRIVATE clangAST clangBasic clangFrontend
                                             clangLex clangParse clangSema
                                             clangSerialization)
endif()

if(DEFINED LLVM_ENABLE_RTTI AND NOT LLVM_ENABLE_RTTI)
  target_compile_options(IfInstrument PRIVATE -fno-rtti)
endif()

install(TARGETS IfInstrument DESTINATION ${SYMSAN_LIB_DIR})
