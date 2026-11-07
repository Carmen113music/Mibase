cmake_minimum_required(VERSION 3.22)

# La versió de Mibase. Puja-la a cada lliurament; no la baixis mai.
project(Mibase VERSION 0.18.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# A Windows, el programa porta les biblioteques de C++ a dins:
# així Mibase.exe funciona sense instal·lar res més.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(MIBASE_APP "Compila el programa amb finestres (necessita JUCE)" ON)

# El motor: sense interfície ni Windows, es prova sol.
add_subdirectory(motor)

enable_testing()
add_subdirectory(proves)

if(MIBASE_APP)
    include(FetchContent)
    # Versió fixa de JUCE: la compilació no canvia sola.
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(app)
endif()
