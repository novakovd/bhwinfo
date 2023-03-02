cmake_minimum_required(VERSION 3.25)

cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

set(PROJECT_NAME HwInfo)
set(CMAKE_CXX_FLAGS -pthread)
set(CMAKE_CXX_STANDARD 20)
set(SD ${CMAKE_SOURCE_DIR}/src/)
set(ID ${CMAKE_SOURCE_DIR}/include/)

project (${PROJECT_NAME})

set(FILES ${SD}/libhwinfo.hpp)

file(GLOB SOURCE_FILES
        ${FILES}
        ${INC}
)

# --- Fetch FTXUI --------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)
FetchContent_Declare(ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
        GIT_TAG v3.0.0
        )

FetchContent_GetProperties(ftxui)
if(NOT ftxui_POPULATED)
    FetchContent_Populate(ftxui)
    add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# ------------------------------------------------------------------------------

add_library(${PROJECT_NAME}_LIB SHARED ${ID}/cpu.hpp ${ID}/mem.hpp ${SD}/cpu.cpp ${SD}/mem.cpp)

add_executable(${PROJECT_NAME} test.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_NAME}_LIB)

target_link_libraries(${PROJECT_NAME}
        PRIVATE ftxui::screen
        PRIVATE ftxui::dom
        PRIVATE ftxui::component
)