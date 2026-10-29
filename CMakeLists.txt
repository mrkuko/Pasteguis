cmake_minimum_required(VERSION 3.16)
project(Pasteguise LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Link the MSVC runtime statically so the exe runs without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Platform-independent core: timing model, option parsing, UTF-8.
add_library(pasteguise_core STATIC
  src/timing.cpp
  src/options.cpp
)
target_include_directories(pasteguise_core PUBLIC src)

if(MSVC)
  target_compile_options(pasteguise_core PUBLIC /W4 /utf-8)
else()
  target_compile_options(pasteguise_core PUBLIC -Wall -Wextra)
endif()

# Platform backend: clipboard read + key injection.
if(WIN32)
  set(PG_PLATFORM_SRC src/platform/win.cpp)
elseif(APPLE)
  set(PG_PLATFORM_SRC src/platform/mac.cpp)
else()
  find_package(X11 REQUIRED)
  if(NOT X11_Xtest_FOUND)
    message(FATAL_ERROR "XTest extension not found (install libxtst-dev / libXtst-devel)")
  endif()
  set(PG_PLATFORM_SRC src/platform/linux_x11.cpp)
endif()

add_executable(pasteguise src/main.cpp ${PG_PLATFORM_SRC})
target_link_libraries(pasteguise PRIVATE pasteguise_core)

if(WIN32)
  target_compile_definitions(pasteguise PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  target_link_libraries(pasteguise PRIVATE user32 shell32)
elseif(APPLE)
  target_link_libraries(pasteguise PRIVATE "-framework ApplicationServices" "-framework CoreFoundation")
else()
  target_include_directories(pasteguise PRIVATE ${X11_INCLUDE_DIR} ${X11_Xtest_INCLUDE_PATH})
  target_link_libraries(pasteguise PRIVATE ${X11_LIBRARIES} ${X11_Xtest_LIB})
endif()

enable_testing()
add_executable(pasteguise_tests tests/core_tests.cpp)
target_link_libraries(pasteguise_tests PRIVATE pasteguise_core)
add_test(NAME core COMMAND pasteguise_tests)
