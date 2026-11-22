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
target_compile_definitions(superwhizkid_diagnostics PUBLIC GAMATE_ACCESS_DIAGNOSTICS)
target_compile_options(superwhizkid_diagnostics PRIVATE -O3)

add_executable(gamatetest tests/gamatetest.c)
target_link_libraries(gamatetest superwhizkid_diagnostics)
target_compile_options(gamatetest PRIVATE -UNDEBUG)
add_test(NAME gamate-unit COMMAND gamatetest)

add_executable(gamate-6502test tests/6502test.c src/gamate/cpu.c)
target_include_directories(gamate-6502test PRIVATE src)

add_executable(gamateboot tests/gamateboot.c)
target_link_libraries(gamateboot superwhizkid_diagnostics)

add_executable(gamatebootpolicytest tests/gamatebootpolicytest.c)
target_link_libraries(gamatebootpolicytest superwhizkid_diagnostics)
target_compile_options(gamatebootpolicytest PRIVATE -UNDEBUG)
add_test(NAME gamate-boot-policy COMMAND gamatebootpolicytest)

add_executable(gamatebioscalls tests/gamatebioscalls.c)
target_link_libraries(gamatebioscalls superwhizkid)

add_executable(gamatebiosprobe tests/gamatebiosprobe.c)
target_link_libraries(gamatebiosprobe superwhizkid_diagnostics)

add_executable(gamatebiosf00c tests/gamatebiosf00c.c)
target_link_libraries(gamatebiosf00c superwhizkid_diagnostics)

add_executable(gamatebiosf012 tests/gamatebiosf012.c)
target_link_libraries(gamatebiosf012 superwhizkid_diagnostics)
add_executable(gamatebiosf027 tests/gamatebiosf027.c)
target_link_libraries(gamatebiosf027 superwhizkid_diagnostics)
add_executable(gamatebiosrowstream tests/gamatebiosrowstream.c)
target_link_libraries(gamatebiosrowstream superwhizkid_diagnostics)

add_executable(gamatebiosf018 tests/gamatebiostilestream.c)
target_link_libraries(gamatebiosf018 superwhizkid_diagnostics)

add_executable(gamatebiosf01e tests/gamatebiostilestream.c)
target_compile_definitions(gamatebiosf01e PRIVATE GAMATE_BIOS_F01E=1)
target_link_libraries(gamatebiosf01e superwhizkid_diagnostics)

add_executable(gamatebiosf030 tests/gamatebiosf030.c)
target_link_libraries(gamatebiosf030 superwhizkid_diagnostics)

add_executable(gamatebiosf033 tests/gamatebiosf033.c)
target_link_libraries(gamatebiosf033 superwhizkid_diagnostics)

option(SUPERWHIZKID_EXPERIMENTAL_BIOS "Build the independent experimental firmware" OFF)
if(SUPERWHIZKID_EXPERIMENTAL_BIOS)
	find_program(SUPERWHIZKID_CA65 ca65)
	find_program(SUPERWHIZKID_LD65 ld65)
	if(NOT SUPERWHIZKID_CA65 OR NOT SUPERWHIZKID_LD65)
		message(FATAL_ERROR "Experimental firmware requires ca65 and ld65 from cc65")
	endif()
	set(EXPERIMENTAL_BIOS "${CMAKE_CURRENT_BINARY_DIR}/experimental-gamate-bios.bin")
	set(EXPERIMENTAL_BIOS_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/experimental-gamate-bios.o")
	add_custom_command(OUTPUT "${EXPERIMENTAL_BIOS}"
		COMMAND "${SUPERWHIZKID_CA65}" -o "${EXPERIMENTAL_BIOS_OBJECT}"
			"${CMAKE_CURRENT_SOURCE_DIR}/firmware/experimental/gamate_bios.s"
		COMMAND "${SUPERWHIZKID_LD65}" -C
			"${CMAKE_CURRENT_SOURCE_DIR}/firmware/experimental/gamate_bios.cfg"
			-o "${EXPERIMENTAL_BIOS}" "${EXPERIMENTAL_BIOS_OBJECT}"
		DEPENDS firmware/experimental/gamate_bios.s firmware/experimental/gamate_bios.cfg
		BYPRODUCTS "${EXPERIMENTAL_BIOS_OBJECT}"
		VERBATIM)
	add_custom_target(gamate-experimental-bios DEPENDS "${EXPERIMENTAL_BIOS}")
	add_executable(gamatebiosprototypetest tests/gamatebiosprototypetest.c)
	target_link_libraries(gamatebiosprototypetest superwhizkid)
	target_compile_options(gamatebiosprototypetest PRIVATE -UNDEBUG)
	add_dependencies(gamatebiosprototypetest gamate-experimental-bios)
	add_test(NAME gamate-bios-prototype
		COMMAND gamatebiosprototypetest "${EXPERIMENTAL_BIOS}")
endif()

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
