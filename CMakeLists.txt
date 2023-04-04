cmake_minimum_required(VERSION 3.25)

cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

set(PROJECT_NAME bhwinfo)
set(CMAKE_CXX_FLAGS -pthread)
set(CMAKE_CXX_STANDARD 20)
set(SD ${CMAKE_SOURCE_DIR}/src/)
set(ID ${CMAKE_SOURCE_DIR}/include/)
set(CMAKE_STATIC_LIBRARY_PREFIX "")

project (${PROJECT_NAME})

set(FILES ${SD}/bhwinfo.hpp)

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
add_library(lib${PROJECT_NAME} SHARED ${ID}/cpu.hpp ${ID}/mem.hpp ${SD}/cpu.cpp ${SD}/mem.cpp)

set_target_properties(lib${PROJECT_NAME} PROPERTIES PREFIX "")

add_executable(${PROJECT_NAME} example.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PUBLIC lib${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
        PRIVATE ftxui::screen
        PRIVATE ftxui::dom
        PRIVATE ftxui::component
)