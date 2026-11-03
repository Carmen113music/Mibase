#pragma once

#include "Dibuix.h"

#include <map>
#include <mibase/Edicio.h>

/** Tot el que comparteixen les seccions de l'estudi: el projecte, el que hi ha triat i com es mira
    (zoom, desplaçament, alçada de les pistes, capçal). Quan alguna cosa canvia, avisa totes les seccions. */
class EstatEstudi : public juce::ChangeBroadcaster
{
public:
    mibase::Projecte projecte;
    juce::File fitxer, carpeta;
    mibase::edicio::Desfes desfes;

    // Vista.
    double pxPerSegon = 48.0;
    double iniciVista = 0.0;     // segons a la vora esquerra
    int desplY = 0;              // píxels
    int altPista = 76;
    double capcal = 0.0;         // segons

    // Triat.
    int pistaTriada = -1;
    std::vector<int> clipsTriats;   // índexs dins la pista triada

    /** Text d'informació de dalt (el valor del que s'arrossega). */
    juce::String info;

    /** Ones de les preses (pic cada 10 ms). */
    std::map<std::string, std::vector<float>> ones;

    /** Demana desar (el fa la pantalla, una mica després) i, si n'hi ha, anota una línia al diari. */
    std::function<void (const juce::String& evolucio)> onCanviat;

    // ---- Ajudes ----
    float x (double t) const noexcept         { return (float) ((t - iniciVista) * pxPerSegon); }
    double t (float px) const noexcept        { return iniciVista + (double) px / pxPerSegon; }
    int numPistes() const noexcept            { return (int) projecte.pistes.size(); }
    int yPista (int i) const noexcept         { return i * altPista - desplY; }
    int pistaA (int y) const noexcept         { return (int) std::floor ((double) (y + desplY) / altPista); }

    mibase::Pista* pista (int i) { return (i >= 0 && i < numPistes()) ? &projecte.pistes[(size_t) i] : nullptr; }

    bool clipTriat (int p, int c) const
    {
        return p == pistaTriada && std::find (clipsTriats.begin(), clipsTriats.end(), c) != clipsTriats.end();
    }

    mibase::Clip* clipUnicTriat()
    {
        auto* t = pista (pistaTriada);
        if (t == nullptr || clipsTriats.size() != 1 || clipsTriats[0] < 0 || clipsTriats[0] >= (int) t->clips.size())
            return nullptr;
        return &t->clips[(size_t) clipsTriats[0]];
    }

    static juce::Colour colorTipus (const std::string& tipus)
    {
        if (tipus == "guitarra") return tema::guitarra;
        if (tipus == "veu")      return tema::veu;
        return tema::audio;
    }

    static juce::String nomTipus (const std::string& tipus)
    {
        if (tipus == "guitarra") return "Guitarra";
        if (tipus == "veu")      return "Veu";
        return juce::String::fromUTF8 ("Àudio");
    }

    /** Les seccions es tornen a dibuixar. */
    void avisa() { sendSynchronousChangeMessage(); }

    /** Després d'un canvi fet (ja guardat el «com era» a desfes): avisa, desa i anota. */
    void canviat (const juce::String& evolucio = {})
    {
        netejaSeleccio();
        avisa();
        if (onCanviat) onCanviat (evolucio);
    }

    /** Treu de la selecció el que ja no existeix. */
    void netejaSeleccio()
    {
        if (pistaTriada >= numPistes()) pistaTriada = numPistes() - 1;
        auto* t = pista (pistaTriada);
        const int n = t ? (int) t->clips.size() : 0;
        clipsTriats.erase (std::remove_if (clipsTriats.begin(), clipsTriats.end(), [n] (int c) { return c < 0 || c >= n; }), clipsTriats.end());
    }

    // ---- Fader: 0 dB a tres quarts del recorregut; de −60 (silenci) a +6 dB ----
    static float posFader (double db)
    {
        if (db <= -59.95) return 0.0f;
        if (db <= 0.0) return 0.75f * (float) std::pow ((db + 60.0) / 60.0, 2.2);
        return 0.75f + 0.25f * (float) juce::jmin (1.0, db / 6.0);
    }

    static double dbFader (float pos)
    {
        pos = juce::jlimit (0.0f, 1.0f, pos);
        if (pos < 0.004f) return -60.0;
        if (pos <= 0.75f) return 60.0 * std::pow (pos / 0.75, 1.0 / 2.2) - 60.0;
        return (pos - 0.75f) / 0.25f * 6.0;
    }

    static juce::String textDb (double db)
    {
        if (db <= -59.95) return juce::String::fromUTF8 ("-∞ dB");
        return (db > 0.05 ? "+" : "") + juce::String (db, 1).replaceCharacter ('.', ',') + " dB";
    }
};
