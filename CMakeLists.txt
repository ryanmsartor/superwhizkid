cmake_minimum_required(VERSION 3.13)
project(superwhizkid C)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

add_library(superwhizkid STATIC src/gamate/gamate.c src/gamate/cpu.c)
target_include_directories(superwhizkid PUBLIC src)
target_compile_options(superwhizkid PRIVATE -O3)

# Heavy VRAM access instrumentation is compiled only into diagnostic executables. Keeping its checks
# out of the normal library preserves the LCD hot path used by the libretro core.
add_library(superwhizkid_diagnostics STATIC src/gamate/gamate.c src/gamate/cpu.c)
target_include_directories(superwhizkid_diagnostics PUBLIC src)
target_compile_definitions(superwhizkid_diagnostics PUBLIC GAMATE_VRAM_DIAGNOSTICS)
target_compile_options(superwhizkid_diagnostics PRIVATE -O3)

add_executable(gamatetest tests/gamatetest.c)
target_link_libraries(gamatetest superwhizkid_diagnostics)
target_compile_options(gamatetest PRIVATE -UNDEBUG)
add_test(NAME gamate-unit COMMAND gamatetest)

add_executable(gamate-6502test tests/6502test.c src/gamate/cpu.c)
target_include_directories(gamate-6502test PRIVATE src)

add_executable(gamateboot tests/gamateboot.c)
target_link_libraries(gamateboot superwhizkid_diagnostics)

add_executable(gamatebioscalls tests/gamatebioscalls.c)
target_link_libraries(gamatebioscalls superwhizkid)

add_library(superwhizkid_libretro SHARED libretro/gamate.c)
target_include_directories(superwhizkid_libretro PRIVATE shared/libretro)
target_link_libraries(superwhizkid_libretro superwhizkid)
set_target_properties(superwhizkid_libretro PROPERTIES PREFIX "")
target_link_options(superwhizkid_libretro PRIVATE
	-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/shared/libretro/link.T
	-Wl,--no-undefined)

add_executable(gamatecoretest tests/gamatecoretest.c)
target_include_directories(gamatecoretest PRIVATE shared/libretro)
target_link_libraries(gamatecoretest dl)
target_compile_options(gamatecoretest PRIVATE -UNDEBUG)
add_test(NAME gamate-libretro
	COMMAND gamatecoretest $<TARGET_FILE:superwhizkid_libretro>)

set(LIBRETRO_CORE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/libretro" CACHE PATH
	"Installation directory for libretro core shared libraries")
set(LIBRETRO_INFO_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/libretro/info" CACHE PATH
	"Installation directory for libretro core info files")
install(TARGETS superwhizkid_libretro LIBRARY DESTINATION "${LIBRETRO_CORE_INSTALL_DIR}")
install(FILES libretro/superwhizkid_libretro.info DESTINATION "${LIBRETRO_INFO_INSTALL_DIR}")
