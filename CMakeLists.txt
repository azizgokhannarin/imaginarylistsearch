cmake_minimum_required(VERSION 3.16)

project(ImaginaryListSearch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(imaginary_list_search  main.cpp)

include(GNUInstallDirs)
install(TARGETS imaginary_list_search 
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
