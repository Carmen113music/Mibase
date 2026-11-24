#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

class EstatEstudi;
class ServeiAudio;

/** 0.97: la finestra de la Font d'inspiració (eina a part, com els sintes): una fila d'acords que es fa amb les
    fletxes, sona amb piano o guitarra GM i un patró, i s'envia a la pista d'Acords. «aturaCanco» para la cançó si
    sonava (quan la Font comença a sonar). «enTancar»: la X o Esc. */
std::unique_ptr<juce::DocumentWindow> creaFinestraFont (EstatEstudi& estat, ServeiAudio& servei,
                                                        std::function<void()> aturaCanco, std::function<void()> enTancar);
