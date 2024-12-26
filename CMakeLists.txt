cmake_minimum_required(VERSION 3.24)

#Options
option(TREAT_WARNINGS_AS_ERRORS "Treat warnings as errors" TRUE)
option(BUILD_TESTS "Build unit-tests" TRUE)
option(ENABLE_CLANG_TIDY "Run clang-tidy with the build, makes the build slower" FALSE)
option(BUILD_COVERAGE "Generate Code-Coverage Information " FALSE)

#Enable clang tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#Enable Clang-Tidy
if(ENABLE_CLANG_TIDY)
  set(CMAKE_CXX_CLANG_TIDY clang-tidy)
  if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CMAKE_VS_GLOBAL_EnableClangTidyCodeAnalysis true)
  endif()
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

#Enable solution folders for Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#Parse the conan_package version to the cmake Major.Minor[.Patch[.Tweak]] format
string(REGEX MATCH "([0-9]+)\\.([0-9]+)(\\.([0-9]+))?(\\.([0-9]+))?" VERSION_EXTRACTED "${CONAN_PACKAGE_VERSION}")
if(VERSION_EXTRACTED STREQUAL "")
  set(VERSION_EXTRACTED 0.0)
  message(STATUS "Building development version")
else()
  message(STATUS "Building version ${VERSION_EXTRACTED}")
endif()

project(${CONAN_PACKAGE_NAME} 
  VERSION ${VERSION_EXTRACTED}
  DESCRIPTION ${CONAN_PACKAGE_DESCRIPTION}
  HOMEPAGE_URL ${CONAN_PACKAGE_URL} 
  LANGUAGES CXX)

find_package(stdexec REQUIRED)

#Compiler Warnings
include(cmake/compiler_warnings.cmake)

#Library Target
add_library(${PROJECT_NAME} INTERFACE ${PUBLIC_HEADERS})

#TODO: strict warnings make problems with stdexec  
#set_project_warnings(${PROJECT_NAME} WARNINGS_AS_ERRORS ${TREAT_WARNINGS_AS_ERRORS})

target_sources(${PROJECT_NAME} 
    INTERFACE FILE_SET HEADERS
    BASE_DIRS
      include/
    FILES 
      include/stdexecutils/spawn_stdfuture.hpp
)

target_include_directories(${PROJECT_NAME} INTERFACE 
  $<INSTALL_INTERFACE:include> 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

set_target_properties(${PROJECT_NAME} PROPERTIES 
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED True
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION}
)

target_link_libraries(${PROJECT_NAME} INTERFACE STDEXEC::stdexec)

#Set coverage flags
if(BUILD_COVERAGE)
  include(CodeCoverage)
  set(COVERAGE_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/include/*")
  append_coverage_compiler_flags_to_target(${PROJECT_NAME})
endif()

#Unit tests
if(BUILD_TESTS)
	find_package(GTest REQUIRED)
	enable_testing()
	include(GoogleTest)
	add_subdirectory(tests)
  #Create coverage report
endif()

#Install Package
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}Targets
        FILE_SET HEADERS
)

install(EXPORT ${PROJECT_NAME}Targets
        FILE ${PROJECT_NAME}Targets.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
          "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

#Optional: Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
  add_custom_target(doxygen)

  set(DOXYGEN_STYLESHEET ${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen-awesome.css)
  set(DOXYGEN_INPUT_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/include)
  set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
  set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/docs/Doxyfile)
  configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

  add_custom_target(doxygen_${PROJECT_NAME}
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM 
  )
  set_target_properties(doxygen_${PROJECT_NAME} PROPERTIES 
    FOLDER "Doxygen"
    EXCLUDE_FROM_ALL True
  )
  add_dependencies(doxygen doxygen_${PROJECT_NAME})
endif()

