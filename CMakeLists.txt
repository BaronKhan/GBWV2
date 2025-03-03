cmake_minimum_required(VERSION 3.20)
project(GameBoyEmulator VERSION 0.1.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find WebView2 package
find_package(nuget QUIET)
if(NOT nuget_FOUND)
    message(STATUS "NuGet not found. Will download WebView2 manually.")
    # Download WebView2 NuGet package
    file(DOWNLOAD
        "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.3065.39"
        "${CMAKE_BINARY_DIR}/Microsoft.Web.WebView2.1.0.3065.39.nupkg"
        SHOW_PROGRESS
    )
    # Extract the package
    file(ARCHIVE_EXTRACT INPUT "${CMAKE_BINARY_DIR}/Microsoft.Web.WebView2.1.0.3065.39.nupkg"
        DESTINATION "${CMAKE_BINARY_DIR}/WebView2"
    )
    # Add include path
    include_directories("${CMAKE_BINARY_DIR}/WebView2/build/native/include")
    # Add library path
    link_directories("${CMAKE_BINARY_DIR}/WebView2/build/native/x64")
    
    # Copy WebView2Loader.dll to output directory
    file(COPY "${CMAKE_BINARY_DIR}/WebView2/build/native/x64/WebView2Loader.dll"
         DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug")
endif()

# Download and include nlohmann/json (header-only library)
if(NOT EXISTS "${CMAKE_BINARY_DIR}/json.hpp")
    message(STATUS "Downloading nlohmann/json...")
    file(DOWNLOAD
        "https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp"
        "${CMAKE_BINARY_DIR}/json.hpp"
        SHOW_PROGRESS
    )
endif()
include_directories("${CMAKE_BINARY_DIR}")

# Source files
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
)

# Add executable
add_executable(GameBoyEmulator WIN32 ${SOURCES})

# Copy resources to build directory
add_custom_command(TARGET GameBoyEmulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:GameBoyEmulator>/resources
)

# Link libraries
target_link_libraries(GameBoyEmulator PRIVATE
    "${CMAKE_BINARY_DIR}/WebView2/build/native/x64/WebView2Loader.dll.lib"
)

# Windows specific settings
if(WIN32)
    target_compile_definitions(GameBoyEmulator PRIVATE UNICODE _UNICODE)
endif() 