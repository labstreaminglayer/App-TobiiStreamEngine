# Distributed under the OSI-approved BSD 3-Clause License.  See 
# https://cmake.org/licensing for details.

#.rst:
# FindTobii
# --------
#
# Find the Tobii "Stream Engine" SDK.
#
# This module accepts the following variable for finding the SDK (in addition to ``CMAKE_PREFIX_PATH``)
#
# ::
#
#     Tobii_ROOT - An additional path to search. Set to the root of the extracted SDK zip.
#
# IMPORTED Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines the :prop_tgt:`IMPORTED` target ``Tobii::Tobii``,
# with both `.lib` and `.dll` files associated, if the Tobii Stream
# Engine SDK has been found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables:
#
# ::
#
#   Tobii_INCLUDE_DIRS - include directories for Tobii Stream SDK
#   Tobii_RUNTIME_LIBRARIES - Runtime libraries (.dll file) needed for execution of an app linked against this SDK, if it differs from Tobii_LIBRARIES.
#   Tobii_RUNTIME_LIBRARY_DIRS - directories to add to runtime library search path.
#   Tobii_LIBRARIES - libraries to link against Tobii Stream SDK
#   Tobii_FOUND - true if Tobii Stream SDK has been found and can be used

# Original Author:
# 2018 Ryan Pavlik for Sensics, Inc.
#
# Copyright Sensics, Inc. 2018.


set(Tobii_ROOT
    "${Tobii_ROOT}"
    CACHE
    PATH
    "Path to search for Tobii Stream SDK: set to the root of the extracted SDK.")

set(_Tobii_extra_paths)
set(_Tobii_hint)
if(Tobii_ROOT)
    list(APPEND _Tobii_extra_paths "${Tobii_ROOT}")
    # In case they were one directory too high
    file(GLOB _Tobii_more "${Tobii_ROOT}/*_stream_engine_*")
    if(_Tobii_more)
        list(APPEND _Tobii_extra_paths ${_Tobii_more})
    endif()
endif()


if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_Tobii_LIB_PATH_SUFFIX lib/tobii)
else()
    set(_Tobii_LIB_PATH_SUFFIX lib/x86)
endif()

find_path(Tobii_INCLUDE_DIR
    tobii/tobii.h
    PATHS ${_Tobii_extra_paths}
	PATH_SUFFIXES include)

if(Tobii_INCLUDE_DIR)
    # parent of include dir is useful root
    get_filename_component(_Tobii_hint "${Tobii_INCLUDE_DIR}" DIRECTORY)
endif()

find_library(Tobii_STREAM_ENGINE_LIBRARY
    tobii_stream_engine
    PATH_SUFFIXES ${_Tobii_LIB_PATH_SUFFIX}
    HINTS ${_Tobii_hint}
    PATHS ${_Tobii_extra_paths})

set(_Tobii_extra_required)
if(WIN32)
    list(APPEND _Tobii_extra_required Tobii_STREAM_ENGINE_RUNTIME_LIBRARY)
endif()
if(WIN32 AND Tobii_STREAM_ENGINE_LIBRARY)
    # directory of link import library is also location for DLL, typically
    get_filename_component(_Tobii_libdir "${Tobii_STREAM_ENGINE_LIBRARY}" DIRECTORY)
    find_file(Tobii_STREAM_ENGINE_RUNTIME_LIBRARY
        tobii_stream_engine.dll
        HINTS ${_Tobii_libdir} ${_Tobii_hint}
        PATH_SUFFIXES ${_Tobii_LIB_PATH_SUFFIX}
        PATHS ${_Tobii_extra_paths})
endif()

mark_as_advanced(Tobii_INCLUDE_DIR Tobii_STREAM_ENGINE_LIBRARY Tobii_STREAM_ENGINE_RUNTIME_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Tobii
                                  REQUIRED_VARS Tobii_INCLUDE_DIR Tobii_STREAM_ENGINE_LIBRARY ${_Tobii_extra_required})
if(Tobii_FOUND)
    set(Tobii_INCLUDE_DIRS "${Tobii_INCLUDE_DIR}")
    set(Tobii_LIBRARIES "${Tobii_STREAM_ENGINE_LIBRARY}")
    if(WIN32)
        set(Tobii_RUNTIME_LIBRARIES "${Tobii_STREAM_ENGINE_RUNTIME_LIBRARY}")
        get_filename_component(Tobii_RUNTIME_LIBRARY_DIRS "${Tobii_STREAM_ENGINE_RUNTIME_LIBRARY}" DIRECTORY)
    else()
        get_filename_component(Tobii_RUNTIME_LIBRARY_DIRS "${Tobii_STREAM_ENGINE_LIBRARY}" DIRECTORY)
    endif()
    if(NOT TARGET Tobii::Tobii)
        add_library(Tobii::Tobii SHARED IMPORTED)
        set_target_properties(Tobii::Tobii PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Tobii_INCLUDE_DIRS}")
        if(WIN32)
            set_target_properties(Tobii::Tobii PROPERTIES
                IMPORTED_IMPLIB "${Tobii_STREAM_ENGINE_LIBRARY}"
                IMPORTED_LOCATION "${Tobii_STREAM_ENGINE_RUNTIME_LIBRARY}")
        else()
            set_target_properties(Tobii::Tobii PROPERTIES
                IMPORTED_LOCATION "${Tobii_STREAM_ENGINE_LIBRARY}")
        endif()
    endif()
    mark_as_advanced(Tobii_ROOT)
endif()