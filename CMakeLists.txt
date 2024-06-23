cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (dpdk-tools)

add_subdirectory (dpdkx)
add_subdirectory (sockperf_cli)
add_subdirectory (sockperf)


add_subdirectory(${toolz_DIR}/utils ${CMAKE_CURRENT_BINARY_DIR}/utils)
add_subdirectory(${toolz_DIR}/utils/program_options ${CMAKE_CURRENT_BINARY_DIR}/utils/program_options)
add_subdirectory(${toolz_DIR}/utils/flags ${CMAKE_CURRENT_BINARY_DIR}/utils/flags)
add_subdirectory(${toolz_DIR}/parsers ${CMAKE_CURRENT_BINARY_DIR}/parsers)

