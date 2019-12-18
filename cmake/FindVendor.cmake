# Copyright Chadwick Boulay and Tristan Stenner
# Distributed under the MIT License.
# See https://opensource.org/licenses/MIT

#[[
FindVendor
----------

This file is an example of a cmake find-module.
https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html#id4

It attempts to find a fictitious library that does not exist and thus
will always fail.

It would be good to put a link to your device <here>.

Import Targets
^^^^^^^^^^^^^^

This module provides the following imported targets, if found (i.e., never):

``Vendor::DeviceModule``
  The library for Vendor's Device SDK.

Cache Variables
^^^^^^^^^^^^^^^^

The following cache variables may also be set to assist/control the operation of this module:

    ``Vendor_ROOT_DIR``
    The root to search for Vendor's SDK.

The following cache variables are set if a GSL target is not already found:

    ``Vendor_INCLUDE_DIRS``
    The directory to add to your include path to be able to #include <Vendor/DeviceHeader>
]]

# Se up cache variables
set(Vendor_ROOT_DIR
    "${Vendor_ROOT_DIR}"
    CACHE PATH "The root to search for Vendor SDK"
)

# find the vendorsdk.h header in the Vendor_ROOT_DIR/include
find_path(Vendor_INCLUDE_DIR
    name "vendorsdk.h"
    PATHS ${Vendor_ROOT_DIR}
    PATH_SUFFIXES include
)

# find a shared library called e.g. vendorsdk.lib or libvendorsdk.so
# in Vendor_ROOT_DIR/lib
find_library(Vendor_LIBRARY
    name vendorsdk
    PATHS ${Vendor_ROOT_DIR}
    PATH_SUFFIXES lib
)

# Handle the _INCLUDE_DIR and _LIBRARY arguments. If found then Vendor_FOUND will be set to TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vendor REQUIRED_VARS Vendor_INCLUDE_DIR Vendor_LIBRARY)

if(Vendor_FOUND)
    # For backwards-compatibility with older style, set Vendor_INCLUDE_DIRS in case some users expect it.
    set(Vendor_INCLUDE_DIRS ${Vendor_INCLUDE_DIR})

    if(NOT TARGET Vendor::Device)
        # Declare the target
        add_library(Vendor::Device SHARED IMPORTED)
        #Use INTERFACE for header-only libs.
        # https://cmake.org/cmake/help/latest/command/add_library.html#id3

        # Windows needs additional help
        get_filename_component(libext vendorsdk_SDK_LIB EXT)
        if(libext STREQUAL ".lib")
            set_target_properties(Vendor::Device PROPERTIES
                IMPORTED_IMPLIB ${Vendor_LIBRARY})
            string(REPLACE ".lib" ".dll" Vendor_LIBRARY ${Vendor_LIBRARY})
        endif()

        set_target_properties(Vendor::Device PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Vendor_INCLUDE_DIR}"
            IMPORTED_LOCATION ${Vendor_LIBRARY}
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            INTERFACE_COMPILE_FEATURES cxx_std_14
        )

    endif(NOT TARGET Vendor::Device)
endif(Vendor_FOUND)
mark_as_advanced(Vendor_ROOT_DIR Vendor_INCLUDE_DIR Vendor_LIBRARY)
