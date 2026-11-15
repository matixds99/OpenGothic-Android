# miniz for ZenKit (its zipped VDF support, ZK_ENABLE_ZIPPED_VDF): the copy of OpenGothic in lib/miniz instead of a
# download (FETCHCONTENT_SOURCE_DIR_MINIZ points here). OpenGothic links the same target.
cmake_minimum_required(VERSION 3.12)
project(miniz C)

set(OG_MINIZ_DIR "${CMAKE_CURRENT_LIST_DIR}/../../lib/miniz")
add_library(miniz STATIC "${OG_MINIZ_DIR}/miniz.c" "${OG_MINIZ_DIR}/miniz.h")
target_include_directories(miniz PUBLIC "${OG_MINIZ_DIR}")
target_compile_definitions(miniz PUBLIC MINIZ_NO_STDIO)
set_target_properties(miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)
