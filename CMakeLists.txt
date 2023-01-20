cmake_minimum_required(VERSION 3.25)

cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

set(PROJECT_NAME HwInfo)
set(CMAKE_CXX_STANDARD 20)
set(SD ${CMAKE_SOURCE_DIR}/src/)
set(ID ${CMAKE_SOURCE_DIR}/inc/)

project (${PROJECT_NAME})

set(INC, ${ID}/robin_hood.h)
set(FILES ${SD}/ut.hpp ${SD}/shared.hpp ${SD}/collect.hpp ${SD}/cpu.hpp)

file(GLOB SOURCE_FILES
        ${FILES}
        ${INC}
)

add_executable(${PROJECT_NAME} test.cpp)