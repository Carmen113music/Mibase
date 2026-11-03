add_library(mibase_motor STATIC
    src/ClicDeProva.cpp
    src/SinteSenzill.cpp
    src/MesuraLatencia.cpp
    src/MotorDiagnostic.cpp)

target_include_directories(mibase_motor PUBLIC include)
target_compile_features(mibase_motor PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(mibase_motor PRIVATE /W4 /utf-8)
else()
    target_compile_options(mibase_motor PRIVATE -Wall -Wextra)
endif()
