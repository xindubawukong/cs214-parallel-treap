cmake_minimum_required(VERSION 3.15.0)
project(
  cs214-project2
  VERSION 0.0.1
  DESCRIPTION "cs214-project2"
  HOMEPAGE_URL "https://github.com/xindubawukong/cs214-project2"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-DLOCAL)

set(WarningOptions
# -O3
# -Wall -Wextra -pedantic
# -Wshadow -Wformat=2 -Wfloat-equal -Wcast-qual -Wcast-align
# -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC
# -fsanitize=address
# -fsanitize=undefined
# -fno-sanitize-recover
# -fstack-protector
)
add_compile_options(${WarningOptions})
add_link_options(${WarningOptions})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/parlaylib/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/PAM/include)

add_executable(main main.cpp)
