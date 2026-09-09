# Protect against multiple inclusions
if(DEFINED VCPKG_WRAPPER_INCLUDED)
    return()
endif()

set(VCPKG_WRAPPER_INCLUDED TRUE)

# 1. Detect if CMake is currently in try_compile mode
get_property(IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE)

set(SYSTEM_VCPKG_CMAKE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

# 2. Check if VCPKG_ROOT exists and points to a valid vcpkg.cmake
if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "${SYSTEM_VCPKG_CMAKE}")
    message(STATUS "[vcpkg-wrapper] Using system vcpkg: ${SYSTEM_VCPKG_CMAKE}")
    include("${SYSTEM_VCPKG_CMAKE}")
else()
    # 3. Determine actual root dir (Fix path issues during try_compile)
    if(DEFINED VCPKG_LOCAL_ROOT)
        set(SHARED_VCPKG_DIR "${VCPKG_LOCAL_ROOT}")
    else()
        # Use CMAKE_CURRENT_LIST_DIR (where vcpkg.cmake lives) instead of CMAKE_SOURCE_DIR
        set(SHARED_VCPKG_DIR "${CMAKE_CURRENT_LIST_DIR}/.build/vcpkg")
    endif()

    set(SHARED_VCPKG_CMAKE "${SHARED_VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake")

    # 4. Only execute download and bootstrap if NOT in try_compile phase
    if(NOT IN_TRY_COMPILE AND NOT EXISTS "${SHARED_VCPKG_CMAKE}")
        message(STATUS "[vcpkg-wrapper] Local/System vcpkg not found. Downloading vcpkg archive...")

        set(VCPKG_ZIP "${CMAKE_BINARY_DIR}/vcpkg-master.zip")
        set(VCPKG_TEMP_DIR "${CMAKE_BINARY_DIR}/vcpkg_temp")

        # Ensure parent target directory exists before file(RENAME)
        file(MAKE_DIRECTORY "${SHARED_VCPKG_DIR}")
        file(REMOVE_RECURSE "${SHARED_VCPKG_DIR}")

        # Download vcpkg source zip archive
        file(DOWNLOAD
            "https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip"
            "${VCPKG_ZIP}"
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
        )

        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)

        if(NOT STATUS_CODE EQUAL 0)
            list(GET DOWNLOAD_STATUS 1 STATUS_MSG)
            file(REMOVE "${VCPKG_ZIP}")
            message(FATAL_ERROR "[vcpkg-wrapper] Failed to download vcpkg archive: ${STATUS_MSG}")
        endif()

        # Extract archive
        message(STATUS "[vcpkg-wrapper] Extracting vcpkg archive...")
        file(ARCHIVE_EXTRACT
            INPUT "${VCPKG_ZIP}"
            DESTINATION "${VCPKG_TEMP_DIR}"
        )

        # Ensure target folder's parent directory exists
        get_filename_component(SHARED_VCPKG_PARENT_DIR "${SHARED_VCPKG_DIR}" DIRECTORY)
        file(MAKE_DIRECTORY "${SHARED_VCPKG_PARENT_DIR}")

        # Move extracted files to target shared directory and cleanup temporary files
        file(RENAME "${VCPKG_TEMP_DIR}/vcpkg-master" "${SHARED_VCPKG_DIR}")
        file(REMOVE "${VCPKG_ZIP}")
        file(REMOVE_RECURSE "${VCPKG_TEMP_DIR}")

        # Bootstrap vcpkg executable
        if(CMAKE_HOST_WIN32)
            set(VCPKG_EXEC "${SHARED_VCPKG_DIR}/vcpkg.exe")
            set(BOOTSTRAP_SCRIPT "${SHARED_VCPKG_DIR}/bootstrap-vcpkg.bat")
        else()
            set(VCPKG_EXEC "${SHARED_VCPKG_DIR}/vcpkg")
            set(BOOTSTRAP_SCRIPT "${SHARED_VCPKG_DIR}/bootstrap-vcpkg.sh")
        endif()

        if(NOT EXISTS "${VCPKG_EXEC}")
            message(STATUS "[vcpkg-wrapper] Bootstrapping vcpkg executable...")
            execute_process(
                COMMAND "${BOOTSTRAP_SCRIPT}" -disableMetrics
                WORKING_DIRECTORY "${SHARED_VCPKG_DIR}"
                RESULT_VARIABLE BOOTSTRAP_RESULT
            )

            if(NOT BOOTSTRAP_RESULT EQUAL 0)
                message(FATAL_ERROR "[vcpkg-wrapper] Failed to bootstrap vcpkg executable!")
            endif()
        endif()
    endif()

    # 5. Include toolchain if it exists
    if(EXISTS "${SHARED_VCPKG_CMAKE}")
        if(NOT IN_TRY_COMPILE)
            message(STATUS "[vcpkg-wrapper] Using shared local vcpkg: ${SHARED_VCPKG_CMAKE}")
        endif()

        include("${SHARED_VCPKG_CMAKE}")
    endif()
endif()