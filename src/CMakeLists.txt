# 收集源文件
set(APP_SOURCES
    main.cpp
)

set(WINDOW_SOURCES
)

set(WINDOW_HEADERS
)

set(WINDOW_UI_FILES
)

set(DIALOG_SOURCES
    dialogs/mydialog.cpp
)

set(DIALOG_HEADERS
    dialogs/mydialog.h
)

set(DIALOG_UI_FILES
    dialogs/mydialog.ui
)

set(CUSTOMIZE_SOURCES
    customize/framelesswindow.cpp
    customize/framelesshelper.cpp
    customize/customtitlebar.cpp
)

set(CUSTOMIZE_HEADERS
    customize/framelesswindow.h
    customize/framelesshelper.h
    customize/customtitlebar.h
)

set(WIDGET_SOURCES
    widgets/codewindow.cpp
    widgets/mainwindow.cpp
    widgets/openglwindow.cpp
    widgets/testcontrols.cpp
    widgets/pdfviewer.cpp
)

set(WIDGET_HEADERS
    widgets/codewindow.h
    widgets/mainwindow.h
    widgets/openglwindow.h
    widgets/testcontrols.h
    widgets/pdfviewer.h
)

set(WIDGET_UI_FILES
    widgets/mainwindow.ui
    widgets/testcontrols.ui
    widgets/pdfviewer.ui
)

set(DATABASE_SOURCES
    database/databasemanager.cpp
)

set(DATABASE_HEADERS
    database/databasemanager.h
)

set(RESOURCES
    ../resources/resources.qrc
)

# 创建可执行文件
add_executable(${PROJECT_NAME} 
    ${APP_SOURCES}
    ${WINDOW_SOURCES}
    ${WINDOW_HEADERS}
    ${WINDOW_UI_FILES}
    ${DIALOG_SOURCES}
    ${DIALOG_HEADERS}
    ${DIALOG_UI_FILES}
    ${CUSTOMIZE_SOURCES}
    ${CUSTOMIZE_HEADERS}
    ${WIDGET_SOURCES}
    ${WIDGET_HEADERS}
    ${DATABASE_SOURCES}
    ${DATABASE_HEADERS}
    ${RESOURCES}
)

# 确保启用 Qt PrintSupport / Xml / Concurrent 模块（Poppler 相关逻辑会用到 QtConcurrent）
find_package(Qt5 COMPONENTS PrintSupport Xml Concurrent REQUIRED)

# 链接 Qt 库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Sql
    Qt5::PrintSupport
    Qt5::Xml
    Qt5::Concurrent
)

# Poppler C++ library (MSYS2 MinGW64)
set(POPPLER_ROOT "C:/msys64/mingw64" CACHE PATH "Poppler install root")

find_path(POPPLER_INCLUDE_DIR
    NAMES poppler/cpp/poppler-document.h
    PATHS "${POPPLER_ROOT}/include"
    NO_DEFAULT_PATH
)

find_library(POPPLER_CPP_LIBRARY
    NAMES poppler-cpp libpoppler-cpp.dll.a
    PATHS "${POPPLER_ROOT}/lib"
    NO_DEFAULT_PATH
)

if(NOT POPPLER_INCLUDE_DIR OR NOT POPPLER_CPP_LIBRARY)
    message(FATAL_ERROR "Poppler C++ library not found under ${POPPLER_ROOT}. Expected headers in ${POPPLER_ROOT}/include and library in ${POPPLER_ROOT}/lib.")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${POPPLER_INCLUDE_DIR}"
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    "${POPPLER_CPP_LIBRARY}"
)

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/customize
    ${CMAKE_CURRENT_SOURCE_DIR}/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/database
)

# Windows 控制台设置
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()