cmake_minimum_required(VERSION 3.10)

project(cpp-shell CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(shell
    src/main.cpp
    src/process.cpp
    src/tokenizer.cpp
    src/parser.cpp
    src/history.cpp
)

if(BUILD_TESTING)
    add_executable(shell_tests
        tests/shell_tests.cpp
        src/tokenizer.cpp
        src/parser.cpp
        src/process.cpp
    )
    target_include_directories(shell_tests PRIVATE src)
    add_test(NAME shell_tests COMMAND shell_tests)
endif()
