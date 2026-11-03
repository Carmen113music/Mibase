#pragma once

#include "Dibuix.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <memory>
#include <mibase/Edicio.h>

/** L'ona d'una presa: mínims i màxims per blocs de 256 mostres (per dibuixar-la de lluny)
    i el fitxer obert per llegir les mostres de debò quan s'hi fa molt de zoom. Tot en mono (mitjana dels canals). */
struct OnaPresa
{
    static constexpr int bloc = 256;
    double sr = 48000.0;
    juce::int64 longitud = 0;
    int canals = 1;
    std::vector<float> mins, maxs;
    std::unique_ptr<juce::AudioFormatReader> lector;

    /** Mostres [ini, ini + n) en mono. Retorna nullptr si no es poden llegir. */
    const float* mostres (juce::int64 ini, int n)
    {
        if (lector == nullptr || n <= 0) return nullptr;
        if (ini >= memIni && ini + n <= memIni + memLen)
            return memoria.data() + (ini - memIni);

        const juce::int64 marge = juce::jmax (4096, n / 2);
        memIni = juce::jmax ((juce::int64) 0, ini - marge);
        memLen = (int) juce::jmin (longitud - memIni, (juce::int64) n + 2 * marge);
        if (memLen <= 0) return nullptr;

        juce::AudioBuffer<float> b ((int) juce::jmax (1u, lector->numChannels), memLen);
        lector->read (&b, 0, memLen, memIni, true, true);
        memoria.assign ((size_t) memLen, 0.0f);
        const float factor = 1.0f / (float) b.getNumChannels();
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            const float* d = b.getReadPointer (ch);
            for (int i = 0; i < memLen; ++i) memoria[(size_t) i] += d[i] * factor;
        }
        if (ini + n > memIni + memLen) return nullptr;
        return memoria.data() + (ini - memIni);
    }

private:
    std::vector<float> memoria;
    juce::int64 memIni = -1;
    int memLen = 0;
};

/** Eines de la caixa d'eines de l'estudi. */
enum class Eina { Fletxa = 0, Llapis, Goma, Tisores, Cola, Silenci };

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

    /** Ones de les preses. */
    std::map<std::string, std::shared_ptr<OnaPresa>> ones;

    /** Eina triada a la caixa d'eines. */
    Eina eina = Eina::Fletxa;

    /** Taula de treball (pistes) o taula de mescles. */
    bool vistaMescles = false;

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

    /** Paleta de colors de les pistes (8). */
    static juce::Colour colorPista (int i)
    {
        static const juce::uint32 colors[] = { 0xfff5c445, 0xff59d0d6, 0xffb18cff, 0xff7fd67a, 0xff6fa8ff, 0xffff8fc4, 0xffe8a87c, 0xffc9b8a6 };
        return juce::Colour (colors[(size_t) juce::jlimit (0, 7, i)]);
    }
    static juce::Colour colorPista (const mibase::Pista& t) { return colorPista (t.color); }

    static juce::String nomColor (int i)
    {
        static const char* noms[] = { "Groc", "Turquesa", "Lila", "Verd", "Blau", "Rosa", "Préssec", "Sorra" };
        return juce::String::fromUTF8 (noms[juce::jlimit (0, 7, i)]);
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
