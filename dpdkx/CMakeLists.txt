find_package(Boost COMPONENTS program_options unit_test_framework REQUIRED )
find_package(PkgConfig REQUIRED)
find_package(fmt)
pkg_check_modules(LIBDPDK REQUIRED 
#                  IMPORTED_TARGET
                   libdpdk)

add_library(libdpdk_staic INTERFACE)
target_compile_options(libdpdk_staic INTERFACE ${LIBDPDK_STATIC_CFLAGS})
target_link_libraries(libdpdk_staic INTERFACE ${LIBDPDK_STATIC_LDFLAGS})

# Add source to this project's executable.
add_library (dpdkx
              error.cpp
              log.cpp
              loggers.hpp
              loggers.cpp
              config/program_options.cpp
              config/socket.cpp
              config/io.hpp
              config/io.cpp
              udptx_channel.cpp
              rx_channel.cpp
              device.cpp
              jobs.hpp
              jobs.cpp
              mcast.cpp
              mbuf.cpp
              mempool.hpp
              mempool.cpp
              detail/tx_queue.cpp
              detail/tx_queue.hpp
              detail/utils.cpp
              detail/utils.hpp
              utils.cpp
              io.hpp
              io.cpp
			)

#set_source_files_properties(device.cpp PROPERTIES COMPILE_FLAGS /Qpar COMPILE_FLAGS /Qpar-report:2 COMPILE_FLAGS -Rpass-missed=loop-vectorize)

add_library (mx-c::dpdkx ALIAS dpdkx)

target_sources(dpdkx PUBLIC FILE_SET HEADERS
				FILES
                error.hpp
                log.hpp
                utils.hpp
                udptx_channel.hpp
                rx_channel.hpp
                device.hpp
                mbuf.hpp
                config/device.hpp
                config/socket.hpp
                config/program_options.hpp
                proto/igmp.hpp
                netinet_in.hpp
			   )


target_include_directories(dpdkx INTERFACE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
		$<INSTALL_INTERFACE:include/dpdkx>
		)
          

target_link_libraries(dpdkx
                PUBLIC
                libdpdk_staic
                utils::parsers
                utils::flags
                utils::program_options
                Boost::boost
                $<$<BOOL:${fmt_FOUND}>:fmt::fmt>
				PRIVATE 
				Boost::disable_autolinking
			)

target_compile_options(dpdkx PRIVATE
     $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc>
     $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-string-conversion -Wno-unsafe-buffer-usage -Wno-global-constructors -Wno-newline-eof>
#     $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unsafe-buffer-usage -Wno-reserved-macro-identifier>
)

set_property(TARGET dpdkx PROPERTY CXX_STANDARD 23)

add_executable (dpdkx_test
        test.cpp
        )

target_link_libraries(dpdkx_test
		PRIVATE
        mx-c::dpdkx        
		Boost::unit_test_framework
        Boost::disable_autolinking
#        Boost::dynamic_linking
)

set_property(TARGET dpdkx_test PROPERTY CXX_STANDARD 23)
target_compile_options(dpdkx_test PRIVATE
     $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc>
     $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-c++98-compat -Wno-c++98-compat-pedantic>
#     $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unsafe-buffer-usage -Wno-reserved-macro-identifier>
)


include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(dpdkx_VERSION 0.0.1)


