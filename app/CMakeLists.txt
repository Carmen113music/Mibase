juce_add_gui_app(Mibase
    PRODUCT_NAME "Mibase"
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Carmen113music")

target_sources(Mibase PRIVATE
    src/Main.cpp
    src/ServeiAudio.cpp
    src/PantallaDiagnostic.cpp)

target_compile_definitions(Mibase PRIVATE
    MIBASE_VERSIO="${PROJECT_VERSION}"
    JUCE_APPLICATION_NAME_STRING="Mibase"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DIRECTSOUND=0
    JUCE_WASAPI=1)

if(WIN32)
    # ASIO amb el kit oficial de Steinberg que tenim a extern/asiosdk (GPLv3).
    target_compile_definitions(Mibase PRIVATE JUCE_ASIO=1 JUCE_ASIO_USE_EXTERNAL_SDK=1)
    target_include_directories(Mibase PRIVATE ${CMAKE_SOURCE_DIR}/extern/asiosdk/common)
endif()

if(MSVC)
    target_compile_options(Mibase PRIVATE /utf-8)
endif()

target_link_libraries(Mibase
    PRIVATE
        mibase_motor
        juce::juce_audio_devices
        juce::juce_gui_basics
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
