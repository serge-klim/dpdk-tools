find_package( Boost )

# Add source to this project's executable.
add_library (sockperf
			 x.hpp
			 sockperf.hpp
			 sockperf.cpp
				)

target_link_libraries(sockperf
				PUBLIC
				Boost::boost
			)

target_include_directories(sockperf INTERFACE ..)

set_property(TARGET sockperf PROPERTY CXX_STANDARD 20)


target_compile_options(sockperf PRIVATE
     $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc>
     $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror -Wno-c++98-compat -Wno-unsafe-buffer-usage -Wno-switch-default>
)