cmake_minimum_required(VERSION 3.16)
project(QtFeatureHub VERSION 1.0.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)    # 自动处理 MOC
set(CMAKE_AUTORCC ON)    # 自动处理资源文件
set(CMAKE_AUTOUIC ON)    # 自动处理 UI 文件

# 查找 Qt 包
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)

# 添加子目录
add_subdirectory(src)

# 安装目标
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)