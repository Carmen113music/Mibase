add_executable(proves_motor proves_motor.cpp)
target_link_libraries(proves_motor PRIVATE mibase_motor)

if(MSVC)
    target_compile_options(proves_motor PRIVATE /utf-8)
endif()

add_test(NAME proves_motor COMMAND proves_motor)
