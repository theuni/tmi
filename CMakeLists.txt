cmake_minimum_required(VERSION 3.22)

project ("tmi" LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/module)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(TryAppendCXXFlags)

add_library(warnings_interface INTERFACE)
try_append_cxx_flags("-Wall" TARGET warnings_interface SKIP_LINK)
try_append_cxx_flags("-Wextra" TARGET warnings_interface SKIP_LINK)
try_append_cxx_flags("-pedantic" TARGET warnings_interface SKIP_LINK)
try_append_cxx_flags("-Weverything" TARGET warnings_interface SKIP_LINK)

try_append_cxx_flags("-Wc++98-compat" TARGET warnings_interface SKIP_LINK
  IF_CHECK_PASSED "-Wno-c++98-compat"
)
try_append_cxx_flags("-Wpadded" TARGET warnings_interface SKIP_LINK
  IF_CHECK_PASSED "-Wno-padded"
)
try_append_cxx_flags("-Wunsafe-buffer-usage" TARGET warnings_interface SKIP_LINK
  IF_CHECK_PASSED "-Wno-unsafe-buffer-usage"
)
try_append_cxx_flags("-Wshadow-uncaptured-local" TARGET warnings_interface SKIP_LINK
  IF_CHECK_PASSED "-Wno-shadow-uncaptured-local"
)

add_executable(example example.cpp)
target_link_libraries(example PRIVATE warnings_interface)

