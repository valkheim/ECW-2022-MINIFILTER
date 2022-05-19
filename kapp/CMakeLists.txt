find_package(WDK REQUIRED)

set(target kapp)

file(GLOB_RECURSE sources "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE headers "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

wdk_add_driver(${target} ${sources} ${headers})

target_include_directories(${target}
    PUBLIC  ../klib/include/public
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(${target} WDK::FLTMGR klib)

include(Sign)
sign_without_timestamp(${target})

add_custom_command(
    TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_SOURCE_DIR}/kapp.inf
    $<TARGET_FILE_DIR:${target}>
)