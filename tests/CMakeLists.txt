# Standalone test harness for the gdclip geometry core.
#
#   cmake -S tests -B tests/build
#   cmake --build tests/build
#   ctest --test-dir tests/build --output-on-failure
#
# Only the Godot-free core (gdclip_core.cpp) and Clipper are compiled here, so
# the tests build and run anywhere with a C++ compiler - no Godot required.
cmake_minimum_required(VERSION 3.10)
project(gdclip_tests CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(test_gdclip
	test_gdclip.cpp
	${ROOT}/gdclip_core.cpp
	${ROOT}/clipper/clipper.cpp)
target_include_directories(test_gdclip PRIVATE ${ROOT})

enable_testing()
add_test(NAME gdclip_unit_tests COMMAND test_gdclip)
