cmake_minimum_required(VERSION 3.16)
project(app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE ALL_SOURCE CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
)

add_executable(app ${ALL_SOURCE})

file(GLOB_RECURSE ALL_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.hpp"
)

set(HEADER_DIRS)
foreach(header ${ALL_HEADERS})
    get_filename_component(dir ${header} PATH)
    list(APPEND HEADER_DIRS ${dir})
endforeach()
list(REMOVE_DUPLICATES HEADER_DIRS)

target_include_directories(app PRIVATE ${HEADER_DIRS})

add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/voina_mir"
        "$<TARGET_FILE_DIR:app>/voina_mir"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/example.txt"
        "$<TARGET_FILE_DIR:app>/example.txt"
    COMMENT "Copying data files to output directory"
)