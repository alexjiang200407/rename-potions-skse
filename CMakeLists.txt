cmake_minimum_required(VERSION 3.21)
set(AE_VERSION 1)

option(BUILD_SKYRIMVR "Build for Skyrim VR" OFF)
option(BUILD_SKYRIMAE "Build for Skyrim AE" OFF)
option(STRIPPED_PDB "Strip private for PDB" OFF)

macro(set_from_environment VARIABLE)
	if (NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif ()
endmacro()

macro(find_commonlib_path)
	if (CommonLibName AND NOT ${CommonLibName} STREQUAL "")
		# Check extern
		message(STATUS ${CommonLibPath})
		find_path(CommonLibPath
		include/REL/Relocation.h
		PATHS extern/${CommonLibName})
		message(STATUS ${CommonLibPath})
		if (${CommonLibPath} STREQUAL "CommonLibPath-NOTFOUND")
			#Check path
			set_from_environment(${CommonLibName}Path)
			set(CommonLibPath ${${CommonLibName}Path})
		endif()
	endif()
endmacro()

set_from_environment(VCPKG_ROOT)
if(BUILD_SKYRIMAE)
	message("Building Skyrim AE")
	add_compile_definitions(SKYRIM_AE)
	add_compile_definitions(SKYRIM_SUPPORT_AE)
	set(CommonLibName "CommonLibSSE")
	set_from_environment(SkyrimAEPath)
	set(SkyrimPath ${SkyrimAEPath})
	set(SkyrimVersion "Skyrim AE")
	set(VERSION ${VERSION}.${AE_VERSION})
elseif(BUILD_SKYRIMVR)
	add_compile_definitions(SKYRIMVR)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
	set(CommonLibName "CommonLibVR")
	set_from_environment(SkyrimVRPath)
	set(SkyrimPath ${SkyrimVRPath})
	set(SkyrimVersion "Skyrim VR")
	set(VERSION ${VERSION}.${VR_VERSION})
else()
	set(CommonLibName "CommonLibSSE")
	set_from_environment(Skyrim64Path)
	set(SkyrimPath ${Skyrim64Path})
	set(SkyrimVersion "Skyrim SSE")
endif()


find_commonlib_path()

message(
	STATUS
	"Building ${NAME} ${VERSION} for ${SkyrimVersion} at ${SkyrimPath} with ${CommonLibName} at ${CommonLibPath}."
)

if (DEFINED VCPKG_ROOT)
	set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
	set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
else ()
	message(
		WARNING
		"Variable VCPKG_ROOT is not set. Continuing without vcpkg."
	)
endif ()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")

project(RenamePotionsSKSE VERSION 1.0.0 LANGUAGES CXX)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
	@ONLY
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/Version.rc
	@ONLY
)

if (DEFINED CommonLibPath AND NOT ${CommonLibPath} STREQUAL "" AND IS_DIRECTORY ${CommonLibPath})
	add_subdirectory(${CommonLibPath} ${CommonLibName})
else ()
	message(
		FATAL_ERROR
		"Variable ${CommonLibName}Path is not set or in extern/. ${CommonLibPath}"
	)
endif()

set_from_environment(SkyrimModsPath)
set(OUTPUT_FOLDER "${SkyrimModsPath}/${PROJECT_NAME}")

message(STATUS "Output path is ${OUTPUT_FOLDER}")


include(cmake/headerlist.cmake)
include(cmake/sourcelist.cmake)

source_group(
	TREE
		${CMAKE_CURRENT_SOURCE_DIR}
	FILES
		${HEADERS}
		${SOURCES}
)

source_group(
	TREE
		${CMAKE_CURRENT_BINARY_DIR}
	FILES
		${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
)

add_library(
    ${PROJECT_NAME} 
    SHARED 
    ${SOURCES}
	${HEADERS}
    ${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	.clang-format
	.editorconfig
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_compile_definitions(
	${PROJECT_NAME}
	PRIVATE
		_UNICODE
)

target_include_directories(
    ${PROJECT_NAME} 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
		${SIMPLEINI_INCLUDE_DIRS}
)

target_link_libraries(
	${PROJECT_NAME}
    PRIVATE
        ${CommonLibName}::${CommonLibName}
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

if(STRIPPED_PDB)
	find_program(PDBCOPY_EXECUTABLE pdbcopy REQUIRED)
	if(PDBCOPY_EXECUTABLE)
		set(STRIPPED_PDB_LOCATION "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.stripped.pdb")
		message(STATUS "pdbcopy found")
		add_custom_command(
			TARGET "${PROJECT_NAME}" POST_BUILD
			COMMAND "${PDBCOPY_EXECUTABLE}"
				"$<TARGET_PDB_FILE:${PROJECT_NAME}>"
				"${STRIPPED_PDB_LOCATION}"
				-p
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${STRIPPED_PDB_LOCATION}" "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
			COMMAND "${CMAKE_COMMAND}" -E remove -f "${STRIPPED_PDB_LOCATION}"
			COMMENT "Stripping private symbols from actual PDB (Release)"
			VERBATIM
		)
	else()
		message(WARNING "pdbcopy not found, cannot strip private symbols from PDB.")
	endif()
endif()


if(DEFINED SkyrimModsPath)
	set(OUTPUT_DIR "${SkyrimModsPath}/${PROJECT_NAME}")
    set(DLL_FOLDER "${OUTPUT_DIR}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
		COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/Skyrim/Data" "${OUTPUT_DIR}"
        VERBATIM
    )
endif()
