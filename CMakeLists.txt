cmake_minimum_required(VERSION 3.16)
project(skitzojams LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(skitzojams WIN32
    main.cpp
    mainwindow.h
    mainwindow.cpp
    audioengine.h
    audioengine.cpp
    resources.qrc
)

target_link_libraries(skitzojams PRIVATE Qt6::Widgets)

# WASAPI/COM libraries the audio engine needs.
if (WIN32)
    target_link_libraries(skitzojams PRIVATE ole32 uuid)
endif()

# Bundle the required Qt DLLs next to the .exe automatically after each
# build, so the app runs standalone without manually copying DLLs.
if (WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")
    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET skitzojams POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:skitzojams>"
            COMMENT "Running windeployqt to bundle required Qt DLLs next to the .exe"
        )
    else()
        message(WARNING "windeployqt not found — you may need to manually copy Qt DLLs next to the .exe to run it outside Visual Studio's debugger.")
    endif()
endif()
