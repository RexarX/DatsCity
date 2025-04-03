# This module helps find dependencies for DatsCity using both system packages and vcpkg

find_package(PkgConfig)

function(find_dependency_with_fallback name out_var_lib out_var_inc)
    # Try using CMake's find_package first (for vcpkg)
    if("${name}" STREQUAL "spdlog")
        find_package(spdlog CONFIG QUIET)
        if(spdlog_FOUND)
            set(${out_var_lib} spdlog::spdlog PARENT_SCOPE)
            if(TARGET spdlog::spdlog)
                get_target_property(include_dirs spdlog::spdlog INTERFACE_INCLUDE_DIRECTORIES)
                if(include_dirs)
                    set(${out_var_inc} "${include_dirs}" PARENT_SCOPE)
                endif()
            endif()
            return()
        endif()
    elseif("${name}" STREQUAL "argparse")
        find_package(argparse CONFIG QUIET)
        if(argparse_FOUND)
            set(${out_var_lib} argparse::argparse PARENT_SCOPE)
            # For header-only libraries, we can't use get_target_property directly
            if(TARGET argparse::argparse)
                get_target_property(include_dirs argparse::argparse INTERFACE_INCLUDE_DIRECTORIES)
                if(include_dirs)
                    set(${out_var_inc} "${include_dirs}" PARENT_SCOPE)
                endif()
            endif()
            return()
        endif()
    elseif("${name}" STREQUAL "glaze")
    find_package(glaze CONFIG QUIET)
    if(glaze_FOUND)
        set(${out_var_lib} glaze::glaze PARENT_SCOPE)
        if(TARGET glaze::glaze)
            get_target_property(include_dirs glaze::glaze INTERFACE_INCLUDE_DIRECTORIES)
            if(include_dirs)
                set(${out_var_inc} "${include_dirs}" PARENT_SCOPE)
            endif()
        endif()
        return()
    endif()
    elseif("${name}" STREQUAL "cpr")
        find_package(cpr CONFIG QUIET)
        if(cpr_FOUND)
            set(${out_var_lib} cpr::cpr PARENT_SCOPE)
            if(TARGET cpr::cpr)
                get_target_property(include_dirs cpr::cpr INTERFACE_INCLUDE_DIRECTORIES)
                if(include_dirs)
                    set(${out_var_inc} "${include_dirs}" PARENT_SCOPE)
                endif()
            endif()
            return()
        endif()
    elseif("${name}" STREQUAL "glm")
        find_package(glm CONFIG QUIET)
        if(glm_FOUND)
            set(${out_var_lib} glm::glm PARENT_SCOPE)
            if(TARGET glm::glm)
                get_target_property(include_dirs glm::glm INTERFACE_INCLUDE_DIRECTORIES)
                if(include_dirs)
                    set(${out_var_inc} "${include_dirs}" PARENT_SCOPE)
                endif()
            endif()
            return()
        endif()
    elseif("${name}" STREQUAL "raylib")
        find_package(raylib CONFIG QUIET)
        if(raylib_FOUND)
            set(${out_var_lib} raylib PARENT_SCOPE)
            if(TARGET raylib)
                get_target_property(include_dirs raylib INTERFACE_INCLUDE_DIRECTORIES)
                if(include_dirs)
                    set(${out_var_inc} "${include_dirs}" PARENT_SCOPE)
                endif()
            endif()
            return()
        endif()
    endif()
    
    # Try pkg-config as fallback for system packages
    pkg_check_modules(${name} QUIET IMPORTED_TARGET ${name})
    if(${${name}_FOUND})
        set(${out_var_lib} PkgConfig::${name} PARENT_SCOPE)
        set(${out_var_inc} "${${name}_INCLUDE_DIRS}" PARENT_SCOPE)
        return()
    endif()
    
    # Try alternative names for pkg-config
    if("${name}" STREQUAL "raylib")
        pkg_check_modules(raylib QUIET IMPORTED_TARGET raylib)
        if(raylib_FOUND)
            set(${out_var_lib} PkgConfig::raylib PARENT_SCOPE)
            set(${out_var_inc} "${raylib_INCLUDE_DIRS}" PARENT_SCOPE)
            return()
        endif()
    endif()
    
    set(${out_var_lib} "" PARENT_SCOPE)
    set(${out_var_inc} "" PARENT_SCOPE)
    message(STATUS "Could not find ${name}")
endfunction()

# Initialize variables to collect all targets and include directories
set(DATSCITY_LIBS "")
set(DATSCITY_INCLUDE_DIRS "")

# Define the list of all dependencies
set(DATSCITY_DEPENDENCIES 
    spdlog 
    argparse 
    glaze
    cpr
    glm
    raylib
)

# Find each dependency separately and make it available as a separate variable
foreach(dep ${DATSCITY_DEPENDENCIES})
    # Convert dependency name to a valid CMake variable name
    string(REPLACE "-" "_" dep_var_name "${dep}")
    string(TOUPPER "${dep_var_name}" dep_var_name_upper)

    find_dependency_with_fallback(${dep} DATSCITY_${dep_var_name_upper}_LIB DATSCITY_${dep_var_name_upper}_INCLUDE)
    
    if(DATSCITY_${dep_var_name_upper}_LIB)
        # Add the individual library target to the list of all targets
        list(APPEND DATSCITY_LIBS ${DATSCITY_${dep_var_name_upper}_LIB})
        
        # Add include directories if found
        if(DATSCITY_${dep_var_name_upper}_INCLUDE)
            list(APPEND DATSCITY_INCLUDE_DIRS ${DATSCITY_${dep_var_name_upper}_INCLUDE})
        endif()
        
        message(STATUS "Found ${dep}: ${DATSCITY_${dep_var_name_upper}_LIB}")
        if(DATSCITY_${dep_var_name_upper}_INCLUDE)
            message(STATUS "  Include dirs: ${DATSCITY_${dep_var_name_upper}_INCLUDE}")
        endif()
    else()
        message(WARNING "Required dependency not found: ${dep}")
    endif()
endforeach()