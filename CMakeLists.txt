cmake_minimum_required(VERSION 3.16)
project(lab1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE ALL_SOURCE CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
)

add_executable(lab1 ${ALL_SOURCE})

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

target_include_directories(lab1 PRIVATE ${HEADER_DIRS})

add_custom_command(TARGET lab1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/voina_mir"
        "$<TARGET_FILE_DIR:lab1>/voina_mir"
    COMMENT "Copying data files to output directory"
)