cmake_minimum_required(VERSION 3.24)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (dpdk-tools VERSION 0.0.3 )

add_subdirectory (dpdkx)
add_subdirectory (sockperf_cli)
add_subdirectory (sockperf)


include(FetchContent)
FetchContent_Declare(
  fb_utils
  GIT_REPOSITORY https://github.com/serge-klim/fb_utils.git
  GIT_TAG        7adf7f9b4522778456905b6f3d4ab0f15332a1fc
)

FetchContent_MakeAvailable(fb_utils)

