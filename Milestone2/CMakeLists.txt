cmake_minimum_required(VERSION 3.10)
project(ComputerArchitecture C)

set(CMAKE_C_STANDARD 99)

# Add source files
add_executable(CASimulator
    main.c
    FileReader.c
)

# If you use any special includes:
# target_include_directories(milestone2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
