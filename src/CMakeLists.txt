set(PROJECT_NAME "DatsCity")

file(GLOB_RECURSE src
    "*.h" "*.cpp"
)

file(GLOB_RECURSE src__Game
    "Game/*.h" "Game/*.cpp"
)

file(GLOB_RECURSE src__Renderer
    "Renderer/*.h" "Renderer/*.cpp"
)

file(GLOB_RECURSE src__Server
    "Server/*.h" "Server/*.cpp"
)

file(GLOB_RECURSE src__Utils
    "Utils/*.h" "Utils/*.cpp"
)

set(ALL_FILES
    ${src}
    ${src__Game}
    ${src__Renderer}
    ${src__Server}
    ${src__Utils}
)

if(ENABLE_UNITY_BUILD)
    set_source_files_properties(${src} PROPERTIES UNITY_GROUP "src") 
    set_source_files_properties(${src__Game} PROPERTIES UNITY_GROUP "game")
    set_source_files_properties(${src__Renderer} PROPERTIES UNITY_GROUP "renderer")
    set_source_files_properties(${src__Server} PROPERTIES UNITY_GROUP "server")
    set_source_files_properties(${src__Utils} PROPERTIES UNITY_GROUP "utils")

    if(CMAKE_PCH_ENABLED)
        set_source_files_properties("pch.h" PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    endif()
endif()

add_executable(${PROJECT_NAME} ${ALL_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(ENABLE_UNITY_BUILD)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        UNITY_BUILD ON
        UNITY_BUILD_MODE BATCH
        CMAKE_UNITY_BUILD_BATCH_SIZE ${UNITY_BUILD_BATCH_SIZE}
    )
endif()

target_precompile_headers(${PROJECT_NAME} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>"
)

foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} UPOUTPUTCONFIG)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        TARGET_NAME_${UPOUTPUTCONFIG} ${PROJECT_NAME}
        ARCHIVE_OUTPUT_NAME_${UPOUTPUTCONFIG} ${PROJECT_NAME}
        RUNTIME_OUTPUT_DIRECTORY_${UPOUTPUTCONFIG}
            ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${OUTPUTCONFIG}-${PLATFORM}-${ARCHITECTURE}
        LIBRARY_OUTPUT_DIRECTORY_${UPOUTPUTCONFIG}
            ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${OUTPUTCONFIG}-${PLATFORM}-${ARCHITECTURE}
        ARCHIVE_OUTPUT_DIRECTORY_${UPOUTPUTCONFIG}
            ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${OUTPUTCONFIG}-${PLATFORM}-${ARCHITECTURE}
    )
endforeach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:
        "DEBUG_MODE"
    >
    $<$<CONFIG:RelWithDebInfo>:
        "RELEASE_WITH_DEBUG_INFO_MODE"
    >
    $<$<CONFIG:Release>:
        "RELEASE_MODE"
    >
)

if(WIN32)
	target_compile_definitions(${PROJECT_NAME} PRIVATE "PLATFORM_WINDOWS")
elseif(UNIX)
	target_compile_definitions(${PROJECT_NAME} PRIVATE "PLATFORM_LINUX")
    target_compile_options(${PROJECT_NAME} PRIVATE -fPIC)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:RelWithDebInfo>:/O2 /Oi /Gy /GL>
        $<$<CONFIG:Release>:/O2 /Oi /Gy /GL>
        /W3
    >
    $<$<CXX_COMPILER_ID:Clang,GNU>:
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<CONFIG:RelWithDebInfo>:-O3 -flto>
        $<$<CONFIG:Release>:-O3 -flto>
    >
)

if(ENABLE_SIMD_SSE2)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:
            $<$<CONFIG:RelWithDebInfo>:/arch:SSE2>
		    $<$<CONFIG:Release>:/arch:SSE2>
	    >
	    $<$<CXX_COMPILER_ID:Clang,GNU>:
            $<$<CONFIG:RelWithDebInfo>:-msse2>
		    $<$<CONFIG:Release>:-msse2>   
	    >
    )
elseif(ENABLE_SIMD_SSE3)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>: # VC doesn't support SSE3
            $<$<CONFIG:RelWithDebInfo>:/arch:SSE2>
		    $<$<CONFIG:Release>:/arch:SSE2>   
	    >
	    $<$<CXX_COMPILER_ID:Clang,GNU>:
            $<$<CONFIG:RelWithDebInfo>:-msse3>
		    $<$<CONFIG:Release>:-msse3> 
	    >
    )
elseif(ENABLE_SIMD_SSSE3)
	target_compile_options(${PROJECT_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>: # VC doesn't support SSSE3
            $<$<CONFIG:RelWithDebInfo>:/arch:SSSE3>
			$<$<CONFIG:Release>:/arch:SSSE3>
		>
		$<$<CXX_COMPILER_ID:Clang,GNU>:
            $<$<CONFIG:RelWithDebInfo>:-mssse3>
			$<$<CONFIG:Release>:-mssse3>
		>
	)
elseif(ENABLE_SIMD_SSE4_1)
	target_compile_options(${PROJECT_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>: # VC doesn't support SSE4.1
            $<$<CONFIG:RelWithDebInfo>:/arch:SSE4.1>
			$<$<CONFIG:Release>:/arch:SSE4.1>
		>
		$<$<CXX_COMPILER_ID:Clang,GNU>:
            $<$<CONFIG:RelWithDebInfo>:-msse4.1>
			$<$<CONFIG:Release>:-msse4.1>	
		>
	)
elseif(ENABLE_SIMD_SSE4_2)
	target_compile_options(${PROJECT_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>: # VC doesn't support SSE4.2
            $<$<CONFIG:RelWithDebInfo>:/arch:SSE4.2>
			$<$<CONFIG:Release>:/arch:SSE4.2>
		>
		$<$<CXX_COMPILER_ID:Clang,GNU>:
			$<$<CONFIG:Release>:-msse4.2>
			$<$<CONFIG:RelWithDebInfo>:-msse4.2>
		>
	)
elseif(ENABLE_SIMD_AVX)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX>
			$<$<CONFIG:Release>:/arch:AVX>
		>
		$<$<CXX_COMPILER_ID:Clang,GNU>:
            $<$<CONFIG:RelWithDebInfo>:-mavx>
			$<$<CONFIG:Release>:-mavx>
			
		>
    )
elseif(ENABLE_SIMD_AVX2)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:
            $<$<CONFIG:RelWithDebInfo>:/arch:AVX2>
			$<$<CONFIG:Release>:/arch:AVX2>
		>
		$<$<CXX_COMPILER_ID:Clang,GNU>:
            $<$<CONFIG:RelWithDebInfo>:-mavx2>
			$<$<CONFIG:Release>:-mavx2>
		>
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${DATSCITY_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${DATSCITY_LIBS})

if (UNIX)
    pkg_check_modules(tbb IMPORTED_TARGET tbb)
    target_link_libraries(${PROJECT_NAME} PRIVATE tbb)
endif()

add_custom_command(TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory 
        ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${CMAKE_BUILD_TYPE}-${PLATFORM}-${ARCHITECTURE}/Assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../Assets
        ${CMAKE_CURRENT_SOURCE_DIR}/../bin/${CMAKE_BUILD_TYPE}-${PLATFORM}-${ARCHITECTURE}/Assets
    COMMENT "Copying Assets to output directory"
)