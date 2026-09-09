cmake_minimum_required(VERSION 3.20.0)
project(ImageCRX VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(IS_CLANG_COMPILER 1)
	add_compile_options(
		"-Wno-braced-scalar-init"
        "-fasm-blocks"
	)
	add_link_options("-Wl")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(
        "/source-charset:utf-8"
        "$<$<CONFIG:Release>:/GF>"
        "$<$<CONFIG:Release>:/Oy>"
        "$<$<CONFIG:Release>:/GL>"
        "$<$<CONFIG:Release>:/GS->"
    )
endif()

string(TIMESTAMP PROJECT_BUILD_DATE "%Y%m%d")
add_definitions("-DPROJECT_BUILD_DATE=\"${PROJECT_BUILD_DATE}\"")
add_definitions("-DPROJECT_NAME=\"${PROJECT_NAME}\"")
add_definitions("-DPROJECT_VERSION=\"${PROJECT_VERSION}\"")

add_subdirectory(utils)

add_executable(image_crx
    "src/main.cpp"
    "src/crx/image_crx.cpp"
)

target_include_directories(image_crx PUBLIC
    "src/"
    "src/crx/"
)

find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
target_link_libraries(image_crx PRIVATE 
    ZLIB::ZLIB
    PNG::PNG
    utils
)