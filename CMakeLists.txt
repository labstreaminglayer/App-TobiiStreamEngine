#: # CMakeLists.txt ([source](../appskeleton/CMakeLists.txt))
#: 
#: `CMakeLists.txt` contains the build configuration that can be used to build
#: your App without any IDE or to generate project files for your specific IDE.
#: 
#: ## Required settings for all apps
cmake_minimum_required(VERSION 3.5)

#: This sets the minimum required CMake version.
#: Here, [CMake 3.5](https://cmake.org/cmake/help/latest/release/3.5.html#modules)
#: was chosen because it is the first version to support imported targets
#: (e.g. `boost::thread`). Before setting this to a higher version, please check
#: if it has arrived in [debian stable](https://packages.debian.org/stable/cmake).
project(TobiiStreamEngine
	LANGUAGES CXX
	VERSION 0.1)

#: [project](https://cmake.org/cmake/help/latest/command/project.html) sets the
#: name of the app, the languages used and the version. The version is later on
#: used in the packages CMake creates for your app.

#: ## Finding liblsl
#: 
#: Your app most likely requires liblsl, so CMake has to find it.
#: 
#: The easiest way to find it using the Findliblsl.cmake find-module in the cmake subfolder.
#: The newest version of Findliblsl.cmake should be available in the template app
#: repository [here](https://github.com/labstreaminglayer/AppTemplate_cpp_qt/blob/master/cmake/Findliblsl.cmake).

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

#: First we need to tell cmake to search in the `cmake` subfolder for cmake find-modules.

find_package(liblsl REQUIRED)

#: Then we can simply use find_package. This enables us to later link LSL::lsl
#: You can read more about [find_package](https://cmake.org/cmake/help/latest/command/find_package.html).

#: ## Finding vendor SDKs
#: 
#: More often than not, vendors offer a library file (`.so`, `.dylib`, `.dll`)
#: and a header file (`.h`). CMake can search for these files and create an
#: imported target, so all compilers get the correct settings for finding and linking them.
#:
#: Before you begin writing cmake code for the library, search the internet
#: (especially GitHub) for Find<Device>.cmake or Find<Vendor>.cmake
#: Be sure to look through the resources here: https://github.com/onqtam/awesome-cmake#modules
#: If found, then drop that file in your app repository's cmake folder.
#: Be sure to read that cmake file for further instructions, but typically
#: one would be required to set Vendor_ROOT_DIR to tell the package finder
#: where to look.
#: Then one could use `find_package(Vendor REQUIRED)` to import its variables.
#:
#: Note that some (older) find modules will set the following variables:
#:  Vendor_INCLUDE_DIRS -- Use this with target_include_dirs
#:  Vendor_LIBRARIES    -- Use this with target_link_libraries
#:
#: Whereas newer find modules will provide an imported target:
#: (https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html#imported-targets)
#: Vendor::Device -- Use this with target_link_libraries.

find_package(Tobii REQUIRED)

# Find our custom package. But we set this to QUIET because it will fail.
# You should set yours to REQUIRED

#: ## Using Qt
#:
#: Qt is a popular-enough library that cmake already knows how to find it.
#: 
#: Nevertheless, it is often necessary in Windows to give cmake a hint where to
#: find it by adding the following to your cmake invocation:
#: -DQt5_Dir="C:/Qt/<version>/<platform>/lib/cmake/Qt5"
#: Where <version> is 5.something and <platform> is something like msvc2017_64
#:
#: Then, to import Qt, just call `find_package` with the components your app uses
#: (usually just `Widgets`).

#: Needed for ui_XY.h files
set(CMAKE_INCLUDE_CURRENT_DIR ON)
#: Enable automatic compilation of .cpp->.moc, xy.ui->ui_xy.h and resource files
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)


#: ## Native threads
#:
#: Native `std::thread`s still require a platform thread library. CMake
#: can find and link to it with the `Threads` package (link your executable with
#: `Threads::Threads` afterwards).

find_package(Threads REQUIRED)


#: If everything succeeds, you can link your app with the vendor SDK
#: by linking to the imported target.
#: 
#: ## Creating executables for your app
#: 
#: Your app can have multiple executables. All of them are set up the same way:

add_executable(${PROJECT_NAME} MACOSX_BUNDLE WIN32
	main.cpp
	mainwindow.cpp
	mainwindow.h
	mainwindow.ui
	reader.h
	reader.cpp
)
target_link_libraries(${PROJECT_NAME}
	PRIVATE
	Qt5::Widgets
	Threads::Threads
	LSL::lsl
	Tobii::Tobii
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)
# target_compile_features(${PROJECT_NAME} PRIVATE cxx_auto_type cxx_lambda_init_captures)

#: [add_executable](https://cmake.org/cmake/help/latest/command/add_executable.html)
#: tells CMake to take all files listed (even Qt `.ui` files), compile them and
#: link them together. `MACOSX_BUNDLE` creates a bundle on OS X, `WIN32` tells
#: Windows compilers not to show a command line window when launching the app.
#: 
#: Using `${PROJECT_NAME}` as a placeholder for the executable name  makes it easier
#: to reuse parts of the `CMakeLists.txt` in other projects.
#: 
#: [target_link_libraries](https://cmake.org/cmake/help/latest/command/target_link_libraries.html)
#: tells CMake to add the include paths (and if necessary `#define`s) to the
#: compiler command line and link to the libraries when producing a binary.
#:
#: If you want to use newer C++ features, either set the target standard version via
#: [`set_property(... CXX_STANDARD 11)`](https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html)
#: or explicitly enable features you need with 
#: [`target_compile_features`](https://cmake.org/cmake/help/latest/command/target_link_libraries.html)
#: (see [`CMAKE_CXX_KNOWN_FEATURES`](https://cmake.org/cmake/help/latest/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html#prop_gbl:CMAKE_CXX_KNOWN_FEATURES)
#: for a list.
#: 
#: ## Setting up deployment
#: 
#: You can also let CMake generate a zip / dmg file with everything needed to run
#: your app:

installLSLApp(${PROJECT_NAME})
installLSLAuxFiles(${PROJECT_NAME}
	${PROJECT_NAME}.cfg
)

LSLGenerateCPackConfig()
#: `installLSLApp` creates an install target for the binary target, so that the
#: CMake install target creates a directory for your app and copies the binary
#: and all needed libraries (including Qt) to this folder.
#: 
#: `installLSLAuxFiles` copies additional files needed (e.g. config files) to the
#: distribution directory.
#: 
#: `LSLGenerateCPackConfig` (has to be the last line!) generates a
#: [CPack](https://cmake.org/Wiki/CMake:Packaging_With_CPack) configuration.
#: CPack will then create packages (`.deb` on Linux, `.dmg` on OS X, `.zip` on
#: Windows) that are easy to send someone and install on another computer.
