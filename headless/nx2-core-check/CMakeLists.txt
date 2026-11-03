cmake_minimum_required(VERSION 3.24)
project(eglantine_nx2_core_check LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
file(READ "${EDEN_SOURCE}/cpmfile.json" packages)
string(JSON catch_version GET "${packages}" catch2 version)
string(JSON catch_hash GET "${packages}" catch2 hash)
set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(Catch2
    URL "https://github.com/catchorg/Catch2/archive/refs/tags/${catch_version}.tar.gz"
    URL_HASH "SHA512=${catch_hash}")
FetchContent_MakeAvailable(Catch2)
file(GLOB ampere_tests CONFIGURE_DEPENDS "${EDEN_SOURCE}/src/tests/video_core/ampere_*.cpp")
# Shader integration tests require the complete compiler and belong to the upstream test target.
# This target checks header-only lowering, captures, command ordering and memory helpers.
list(FILTER ampere_tests EXCLUDE REGEX "ampere_(bindless_tex|sured|async_copy)\\.cpp$")
message(STATUS "Full-compiler integration tests are outside this header-only host target")
set(core_regression_sources)
foreach(relative IN ITEMS
    "src/tests/core/read_traps.cpp"
    "src/tests/core/nx2_readback_cache.cpp"
    "src/tests/core/nx2_core_readback_capacity.cpp"
    "src/tests/video_core/nx2_gpu_mappings.cpp"
    "src/tests/video_core/nx2_command_ranges.cpp"
    "src/tests/video_core/nx2_command_sync.cpp"
    "src/tests/video_core/nx2_sparse_region.cpp"
    "src/tests/video_core/memory_budget.cpp"
    "src/tests/video_core/command_fetch_order.cpp"
    "src/tests/video_core/async_draw_policy.cpp"
    "src/tests/video_core/multi_range_refresh.cpp"
)
    if(EXISTS "${EDEN_SOURCE}/${relative}")
        list(APPEND core_regression_sources "${EDEN_SOURCE}/${relative}")
    else()
        message(STATUS "This core does not provide ${relative}")
    endif()
endforeach()
add_executable(nx2-core-check ${ampere_tests} ${core_regression_sources})
file(GLOB boost_includes "${DEPENDENCY_SOURCE}/.cache/cpm/boost/*/libs/*/include")
target_include_directories(nx2-core-check PRIVATE "${EDEN_SOURCE}/src" "${FMT_INCLUDE}" ${boost_includes})
target_link_libraries(nx2-core-check PRIVATE Catch2::Catch2WithMain)
