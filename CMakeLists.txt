cmake_minimum_required(VERSION 3.22)
project(mobilador_id LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_library(MobiladorID SHARED
    src/main.cpp
    third_party/aml/mod/logger.cpp
    third_party/aml/mod/config.cpp)
target_include_directories(MobiladorID PRIVATE third_party/aml)
target_compile_options(MobiladorID PRIVATE -ffunction-sections -fdata-sections)
target_link_libraries(MobiladorID PRIVATE log dl)
target_link_options(MobiladorID PRIVATE -Wl,--gc-sections)
