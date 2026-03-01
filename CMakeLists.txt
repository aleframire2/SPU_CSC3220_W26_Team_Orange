cmake_minimum_required(VERSION 3.16)
project(NarrativeLink CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SQLite amalgamation (compiled as C)
add_library(sqlite3 STATIC src/lib/sqlite3.c)
target_include_directories(sqlite3 PUBLIC src/include)

# All app sources
add_executable(narrativelink
    src/main.cpp
    src/database.cpp
    src/chain_engine.cpp
    src/narrative.cpp
    src/flesch.cpp
    src/archive.cpp
    src/analytics.cpp
    src/wordpool.cpp
    src/export.cpp
)

target_include_directories(narrativelink PRIVATE src/include src)
target_link_libraries(narrativelink PRIVATE sqlite3)

if(WIN32)
    target_link_libraries(narrativelink PRIVATE ws2_32)
endif()
