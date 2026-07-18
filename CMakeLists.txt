cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME STM32_Environmental_Monitor)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# 自动识别用户源文件 (.c)
# CONFIGURE_DEPENDS: 每次构建前检查文件列表是否变化,
#                     若新增/删除了 .c 文件会自动触发重新 configure,
#                     避免"新加文件却没编译进去"的坑
file(GLOB_RECURSE USER_SOURCES
    CONFIGURE_DEPENDS
        Drivers/Bus/I2C/*.c
        Drivers/BSP/OLED/*.c
        Drivers/BSP/SHT30/*.c
        Drivers/BSP/LED/*.c
        Drivers/BSP/KEY/*.c
)

# 将上面自动识别到的 .c 源文件加入编译目标
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${USER_SOURCES}
)

# 添加自定义头文件搜索路径 (.h)
# 每新建一个驱动目录,需要在这里手动补一行,
# 否则对应目录下的 .h 文件 #include 时会找不到
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    Core/Inc
        Drivers/Bus/I2C
        Drivers/BSP/OLED
        Drivers/BSP/SHT30
        Drivers/BSP/LED
        Drivers/BSP/KEY
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
        STM32_Drivers

    # Add user defined libraries
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
    -u _printf_float        # ← 添加这行，开启 sprintf 浮点支持
)
