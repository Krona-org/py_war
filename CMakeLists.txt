cmake_minimum_required(VERSION 3.16)
project(lab1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Рекурсивный поиск всех cpp и hpp
file(GLOB_RECURSE ALL_SOURCE CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
)
file(GLOB_RECURSE ALL_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.hpp"
)

# Передаем и сорцы, и хэдеры (чтобы IDE видела файлы)
add_executable(lab1 ${ALL_SOURCE} ${ALL_HEADERS})

target_include_directories(lab1 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/source"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/scandocs/data"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/work1_3"
)
