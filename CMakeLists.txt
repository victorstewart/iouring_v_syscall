cmake_minimum_required (VERSION 3.18)

project(iouring_vs_syscall LANGUAGES C CXX)

add_executable(iouring_vs_syscall iouring_vs_syscall.cpp)

include(ExternalProject)

set_directory_properties(PROPERTIES EP_BASE "${CMAKE_BINARY_DIR}/3rdparty")
set(3rdParty_SDIR ${CMAKE_BINARY_DIR}/3rdparty/Source)
set(3rdParty_BDIR ${CMAKE_BINARY_DIR}/3rdparty/Build)

ExternalProject_Add(liburing
	GIT_REPOSITORY https://github.com/axboe/liburing.git
	GIT_TAG master
	BUILD_IN_SOURCE ON
	GIT_SHALLOW ON
	CONFIGURE_COMMAND ${3rdParty_SDIR}/liburing/configure
	BUILD_COMMAND make -j4 
	INSTALL_COMMAND ""
)
add_dependencies(iouring_vs_syscall liburing)

target_compile_options(iouring_vs_syscall PUBLIC -w -mavx2 -flto -Ofast -frename-registers -fno-signed-zeros -fno-trapping-math)

target_include_directories(iouring_vs_syscall PUBLIC ${3rdParty_SDIR}/liburing/src/include)
target_link_libraries(iouring_vs_syscall ${3rdParty_SDIR}/liburing/src/liburing.a)

add_custom_command(TARGET iouring_vs_syscall
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:iouring_vs_syscall> ..)
