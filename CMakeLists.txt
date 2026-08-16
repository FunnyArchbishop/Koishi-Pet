cmake_minimum_required(VERSION 3.16)
project(DeskPetQt VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(KoishiPet WIN32
    main.cpp
    petwindow.cpp
    petwindow.h
    sprite_loader.cpp
    sprite_loader.h
    pet_state.h
)

target_include_directories(KoishiPet PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(KoishiPet PRIVATE Qt6::Widgets)

# 复制 assets 目录到构建目录
add_custom_command(TARGET KoishiPet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:KoishiPet>/assets
)
