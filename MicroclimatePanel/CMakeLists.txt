cmake_minimum_required(VERSION 3.16)
project(MicroclimatePanel VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Флаг для поддержки больших объектных файлов (решает проблему "too many sections")
add_compile_options(-Wa,-mbig-obj)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Gui PrintSupport)

qt_standard_project_setup()

qt_add_executable(MicroclimatePanel
    main.cpp
    mainwindow.cpp
    mainwindow.h
    qcustomplot.cpp
    qcustomplot.h
)

target_link_libraries(MicroclimatePanel PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Gui
    Qt6::PrintSupport
)