cmake_minimum_required(VERSION 3.16)

project(trsync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(TQT REQUIRED tqt-mt)

find_program(TMOC_EXECUTABLE tmoc)
if(NOT TMOC_EXECUTABLE)
    message(FATAL_ERROR "tmoc not found (TQt3 moc)")
endif()

set(MOC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/moc)
file(MAKE_DIRECTORY ${MOC_OUTPUT_DIR})

set(MOC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mainwindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rsyncprocess.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rsyncdialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rsyncdialog_new.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tqtmultiprogressdialog.h
)

set(MOC_SOURCES)
foreach(hdr ${MOC_HEADERS})
    get_filename_component(hdr_name ${hdr} NAME_WE)
    set(moc_cpp ${MOC_OUTPUT_DIR}/moc_${hdr_name}.cpp)
    add_custom_command(
        OUTPUT ${moc_cpp}
        COMMAND ${TMOC_EXECUTABLE} ${hdr} -o ${moc_cpp}
        DEPENDS ${hdr}
        VERBATIM
    )
    list(APPEND MOC_SOURCES ${moc_cpp})
endforeach()

add_executable(trsync
    src/main.cpp
    src/mainwindow.cpp
    src/inifile.cpp
    src/rsyncprocess.cpp
    src/rsyncdialog.cpp
    src/rsyncdialog_new.cpp
    src/rsyncstats.cpp
    src/tqtmultiprogressdialog.cpp
    ${MOC_SOURCES}
)

target_include_directories(trsync PRIVATE
    ${TQT_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${MOC_OUTPUT_DIR}
)

target_compile_options(trsync PRIVATE
    ${TQT_CFLAGS_OTHER}
    -Wall
    -Wextra
    -Wshadow
    -Os
    -flto=auto
    -fstrict-aliasing
    -ffunction-sections
    -fdata-sections
    -fno-ident
    -fno-plt
    -fomit-frame-pointer
    -fno-stack-protector
    -fno-math-errno
    -fvisibility=hidden
    -fno-threadsafe-statics
    -fno-exceptions
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
)

target_link_options(trsync PRIVATE
    -Wl,-O1
    -Wl,--gc-sections
    -Wl,--as-needed
    -s
)

target_link_libraries(trsync PRIVATE
    ${TQT_LIBRARIES}
)
