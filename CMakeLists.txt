cmake_minimum_required(VERSION 3.15)
project(ProcesadorSimulado CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(processor
    src/ProcessingElement.cpp
    src/ProcessorSystem.cpp
)

add_executable(proc_sim src/main.cpp)

target_include_directories(processor PUBLIC src)

target_link_libraries(proc_sim PRIVATE processor)
