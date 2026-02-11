cmake_minimum_required(VERSION 3.10)
project(CarLinkCLI VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 构建选项
option(BUILD_SHARED_LIBS "Build shared library" OFF)

# 查找 readline 库
find_library(READLINE_LIBRARY readline REQUIRED)
if(NOT READLINE_LIBRARY)
    message(FATAL_ERROR "readline library not found. Please install libreadline-dev (Ubuntu/Debian) or readline-devel (Fedora/RHEL)")
endif()

# 源文件
set(SOURCES
    CLI.cpp
)

# 头文件
set(HEADERS
    CLI.h
)

# 创建库
add_library(carlink_cli ${SOURCES})

# 设置库的公共头文件
target_include_directories(carlink_cli
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# 链接 readline 库
target_link_libraries(carlink_cli PUBLIC ${READLINE_LIBRARY})

# 设置库的版本信息
set_target_properties(carlink_cli PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${HEADERS}"
)

# 编译选项
target_compile_options(carlink_cli PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# 安装规则
include(GNUInstallDirs)

install(TARGETS carlink_cli
    EXPORT CarLinkCLITargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/carlink
)

# 导出配置
install(EXPORT CarLinkCLITargets
    FILE CarLinkCLITargets.cmake
    NAMESPACE CarLink::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CarLinkCLI
)

# 创建配置文件
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/CarLinkCLIConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CarLinkCLIConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CarLinkCLI
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/CarLinkCLIConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/CarLinkCLIConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/CarLinkCLIConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CarLinkCLI
)

# 显示构建信息
message(STATUS "CarLink CLI Library Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build shared library: ${BUILD_SHARED_LIBS}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Readline library: ${READLINE_LIBRARY}")
