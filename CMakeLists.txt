cmake_minimum_required(VERSION 3.10)

project(sdraw)

add_executable(${PROJECT_NAME} main.c)

set(CMAKE_INSTALL_PREFIX /usr/local)

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 23)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb")

target_link_libraries(${PROJECT_NAME}
    SDL2
)

install(TARGETS ${PROJECT_NAME} DESTINATION)
install(FILES sdraw.desktop DESTINATION /usr/share/applications)
