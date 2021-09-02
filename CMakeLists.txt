cmake_minimum_required(VERSION 3.0.0)
project(as_nm_test VERSION 0.1.0)

include(CTest)
enable_testing()

link_directories(./lib)

include_directories("./src")
include_directories("./include")

aux_source_directory("./src" DIR_SRC)

add_executable(as_nm_test ${DIR_SRC})
target_link_libraries(as_nm_test nanomsg)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
