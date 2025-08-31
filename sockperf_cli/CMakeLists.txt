if (WIN32)
    find_package(zstd REQUIRED CONFIG)
    add_library(zstd ALIAS zstd::libzstd_static)
    find_package(liblzma REQUIRED CONFIG)
    add_library(lzma ALIAS liblzma::liblzma)
endif (WIN32)

find_package( Boost COMPONENTS program_options filesystem iostreams unit_test_framework log log_setup REQUIRED )

add_executable (sockperf_cli
                main.cpp
                sockperf.cpp
                utils.hpp
                utils.cpp
                loggers.hpp
                loggers.cpp
                stats.cpp
                stats.hpp
                )

target_link_libraries(sockperf_cli
        dpdkx
        fb::fb_utils::program_options
        fb::fb_utils::program_options::log
        fb::fb_utils::nic_addresses
        Boost::boost
        Boost::program_options
        Boost::iostreams
        Boost::log
        Boost::log_setup
)

set_property(TARGET sockperf_cli PROPERTY CXX_STANDARD 23)

target_compile_options(sockperf_cli PRIVATE
     $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc>
     $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-string-conversion -Wno-unsafe-buffer-usage>
#     $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unsafe-buffer-usage -Wno-reserved-macro-identifier>
)

target_link_options(sockperf_cli PRIVATE
    $<$<PLATFORM_ID:Windows>:/DELAYLOAD:mlx5devx.dll>
)

######### functional test ##################
add_executable (ftests
        ftest.cpp
        )

target_link_libraries(ftests
		PRIVATE
        Boost::program_options
		Boost::unit_test_framework
        Boost::filesystem
        Boost::disable_autolinking
)

add_dependencies(ftests sockperf_cli)

set_property(TARGET ftests PROPERTY CXX_STANDARD 23)
target_compile_options(ftests PRIVATE
     $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc>
     $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wno-c++98-compat -Wno-c++98-compat-pedantic>
#     $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unsafe-buffer-usage -Wno-reserved-macro-identifier>
)

