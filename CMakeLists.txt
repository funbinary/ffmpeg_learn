cmake_minimum_required(VERSION 3.16)
project(ffmpeg_learn)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_EXE_LINKER_FLAGS "-static  -static-libgcc")

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(SRC_EXT_PATTERN ".cpp")

include_directories(${CMAKE_SOURCE_DIR}/include)

# 添加ffmpeg库
#find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL AVDEVICE REQUIRED)
#if(FFMPEG_FOUND)
#    #  FFMPEG_INCLUDE_DIRS  - Include directory necessary for using the required components headers.
#    #  FFMPEG_LIBRARIES     - Link these to use the required ffmpeg components.
#    #  FFMPEG_DEFINITIONS   - Compiler switches required for using the required ffmpeg components.
#    message("FFMPEG_INCLUDE_DIRS = ${FFMPEG_INCLUDE_DIRS} ")
#    message("FFMPEG_LIBRARIES = ${FFMPEG_LIBRARIES} ")
#    message("FFMPEG_DEFINITIONS = ${FFMPEG_DEFINITIONS} ")
#    include_directories(${FFMPEG_INCLUDE_DIRS})
#else()
#    message(FATAL_ERROR "FFMPEG not found")
#endif()
#find_package(PkgConfig)
#pkg_check_modules(ffmpeg REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libavdevice)
# 添加ffmpeg库
if (CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    find_package(FFmpeg COMPONENTS AVCODEC AVFORMAT AVUTIL AVDEVICE REQUIRED)
    if (FFMPEG_FOUND)
        #  FFMPEG_INCLUDE_DIRS  - Include directory necessary for using the required components headers.
        #  FFMPEG_LIBRARIES     - Link these to use the required ffmpeg components.
        #  FFMPEG_DEFINITIONS   - Compiler switches required for using the required ffmpeg components.
        message("FFMPEG_INCLUDE_DIRS = ${FFMPEG_INCLUDE_DIRS} ")
        message("FFMPEG_LIBRARIES = ${FFMPEG_LIBRARIES} ")
        message("FFMPEG_DEFINITIONS = ${FFMPEG_DEFINITIONS} ")
        include_directories(${FFMPEG_INCLUDE_DIRS})
        SET(FFMPEG_LIB ${FFMPEG_LIBRARIES})
    else ()
        message(FATAL_ERROR "FFMPEG not found")
    endif ()
elseif (CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    find_package(PkgConfig)
    pkg_check_modules(ffmpeg REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libavdevice)
else ()
    message("current platform: unkonw ")
endif ()

file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/*${SRC_EXT_PATTERN})
source_group("Source Files" FILES ${SRC_FILES})


add_executable(cameraToh264 src/CameraToH264.cpp)
add_executable(flvToRtmp src/flvToRTMP.cpp)
add_executable(cameraToRtmp src/CameraToRtmp.cpp)
#add_executable(${PROJECT_NAME} src/main.cpp)

#set_target_properties(${PROJECT_NAME}  PROPERTIES LINK_SEARCH_START_STATIC 1)
#set_target_properties(${PROJECT_NAME} PROPERTIES LINK_SEARCH_END_STATIC 1)

#target_link_libraries(${PROJECT_NAME}  PRIVATE -static PkgConfig::ffmpeg)

if (CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    #    target_link_libraries(${PROJECT_NAME} PRIVATE -static ${FFMPEG_LIB})
    #    target_link_libraries(cameraToh264 PRIVATE -static ${FFMPEG_LIB})
    #    target_link_libraries(flvToRtmp PRIVATE -static ${FFMPEG_LIB})

elseif (CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    #    target_link_libraries(${PROJECT_NAME} PRIVATE -static PkgConfig::ffmpeg)
    target_link_libraries(cameraToh264 PRIVATE -static PkgConfig::ffmpeg)
    target_link_libraries(flvToRtmp PRIVATE -static PkgConfig::ffmpeg)
    target_link_libraries(cameraToRtmp PRIVATE -static PkgConfig::ffmpeg)
endif ()
