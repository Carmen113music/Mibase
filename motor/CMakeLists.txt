add_library(mibase_motor STATIC
    src/ClicDeProva.cpp
    src/SinteSenzill.cpp
    src/MesuraLatencia.cpp
    src/MotorDiagnostic.cpp
    src/ProjecteDisc.cpp
    src/Edicio.cpp
    src/Takes.cpp
    src/Pool.cpp
    src/Elastic.cpp
    src/DeteccioAcords.cpp
    src/Wav.cpp
    src/Gravadora.cpp
    src/MotorEstudi.cpp
    src/Midi.cpp
    src/Harmonia.cpp
    src/Guitarra.cpp
    src/Piano.cpp
    src/PresetsPads.cpp
    src/Bateria.cpp
    src/PatronsBateria.cpp
    src/Veu.cpp
    src/Sampler.cpp
    src/Biblioteca.cpp
    src/GeneralMidi.cpp
    src/Reverb.cpp
    src/ReverbPresets.cpp
    src/Automatitzacio.cpp
    src/Dinamica.cpp
    src/DinamicaPresets.cpp
    src/Delay.cpp
    src/DelayPresets.cpp
    src/Saturacio.cpp
    src/SaturacioPresets.cpp
    src/Drive.cpp
    src/DrivePresets.cpp
    src/SinteBase.cpp
    src/SubBass.cpp
    src/SubBassPresets.cpp
    src/Lead.cpp
    src/LeadPresets.cpp
    src/Atmos.cpp
    src/AtmosPresets.cpp
    src/Neo.cpp
    src/NeoPresets.cpp
    src/Limitador.cpp
    src/LimitadorPresets.cpp
    src/Doubler.cpp
    src/Harmonitzador.cpp
    src/RackDesplega.cpp
    src/Rack.cpp
    src/RackPresets.cpp
    src/RackMaster.cpp
    src/Loudness.cpp
    src/Estereo.cpp
    src/Clipper.cpp
    src/Transitoris.cpp
    src/Convolucio.cpp
    src/Equalitzador.cpp
    src/Espectre.cpp
    src/EqPresets.cpp)

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
