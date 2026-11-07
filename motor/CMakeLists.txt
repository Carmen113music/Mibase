add_library(mibase_motor STATIC
    src/ClicDeProva.cpp
    src/SinteSenzill.cpp
    src/MesuraLatencia.cpp
    src/MotorDiagnostic.cpp
    src/ProjecteDisc.cpp
    src/Edicio.cpp
    src/Wav.cpp
    src/Gravadora.cpp
    src/MotorEstudi.cpp
    src/Midi.cpp
    src/SinteIntern.cpp
    src/Harmonia.cpp
    src/PresetsPads.cpp
    src/Bateria.cpp
    src/Veu.cpp)

target_include_directories(mibase_motor PUBLIC include)
find_package(Threads REQUIRED)
target_link_libraries(mibase_motor PUBLIC Threads::Threads)
# JSON (nlohmann, MIT): només per dins del motor.
target_include_directories(mibase_motor PRIVATE ${CMAKE_SOURCE_DIR}/extern)
target_compile_features(mibase_motor PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(mibase_motor PRIVATE /W4 /utf-8)
else()
    target_compile_options(mibase_motor PRIVATE -Wall -Wextra)
endif()
