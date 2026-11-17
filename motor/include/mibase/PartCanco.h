#pragma once
// 0.51: les variacions per parts de la cançó dels patrons de guitarra i de piano (com les de la bateria).
namespace mibase
{
    enum class PartCanco { Original = 0, Estrofa, PreTornada, Tornada, Pont };
    constexpr int numPartsCanco = 5;
    /** «Original», «Estrofa»… */
    const char* nomPart (PartCanco v);
    /** El que va darrere de «~» a l'id de la variació («estrofa», «pretornada»…); buit per a l'original. */
    const char* sufixPart (PartCanco v);
    /** Què fa la variació (per a la finestra de patrons). */
    const char* ajudaPart (PartCanco v, bool piano);
}
