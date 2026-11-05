#pragma once

#include "Dibuix.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <memory>
#include <array>
#include <mibase/Edicio.h>
#include <mibase/MotorEstudi.h>

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
    int pistaTriada = -1;                              // la pista de la columna esquerra
    std::vector<mibase::edicio::RefClip> triats;       // clips triats (poden ser de pistes diferents)

    /** Tros triat amb el requadre en mode «només el que queda dins». */
    struct Rang
    {
        bool actiu = false;
        double t0 = 0.0, t1 = 0.0;
        int pistaDe = 0, pistaFins = 0;
    } rang;

    // Botons de la barra d'eines.
    bool modeRang = false;     // requadre: false = clips sencers; true = només el que queda dins
    bool insereix = false;     // enganxar: false = trepitja; true = aparta el que hi ha (tecla Insert)

    /** Clip MIDI obert al Piano Roll (pista i índex del clip) i si el Piano Roll es veu. */
    mibase::edicio::RefClip clipObert;
    bool pianoRoll = false;

    std::string idPistaObert;   // per seguir la pista si canvia de lloc (s'esborra o es mou una altra)

    void obreClip (mibase::edicio::RefClip r)
    {
        clipObert = r;
        idPistaObert = pista (r.pista) != nullptr ? projecte.pistes[(size_t) r.pista].id : std::string();
    }

    /** El clip obert, si encara existeix i és MIDI. */
    mibase::Clip* clipMidiObert()
    {
        if (auto* t = pista (clipObert.pista); t == nullptr || t->id != idPistaObert)
        {
            clipObert.pista = -1;
            for (int i = 0; i < numPistes(); ++i)
                if (projecte.pistes[(size_t) i].id == idPistaObert) clipObert.pista = i;
        }
        auto* c = clip (clipObert);
        return (c != nullptr && c->midi) ? c : nullptr;
    }

    /** El que s'ha copiat (Ctrl+C). */
    mibase::edicio::Retall portaretalls;

    /** Text d'informació de dalt (el valor del que s'arrossega). */
    juce::String info;

    /** Ones de les preses. */
    std::map<std::string, std::shared_ptr<OnaPresa>> ones;

    /** Eina triada a la caixa d'eines. */
    Eina eina = Eina::Fletxa;

    /** Taula de treball (pistes) o taula de mescles. */
    bool vistaMescles = false;

    // ---- Transport (la pantalla ho copia del motor unes 30 vegades per segon) ----
    int transport = 0;               // 0 aturat, 1 sonant, 2 comptant, 3 gravant
    double cursorGravacio = 0.0;     // on s'ha premut gravar
    /** El capçal s'ha mogut a mà (per fer saltar el que sona). */
    std::function<void (double)> onCapcalMogut;

    /** Mou el capçal. Mentre sona, només ho fa la regla («sempre»): clicar els clips no fa saltar la música. */
    void mouCapcal (double t, bool sempre = false)
    {
        if (transport != 0 && ! sempre) return;
        capcal = juce::jmax (0.0, t);
        if (onCapcalMogut) onCapcalMogut (capcal);
    }

    // ---- Vúmetres (0..1, amb la caiguda suau ja feta) ----
    std::array<float, mibase::maxSlots * 2> nivells {};
    std::array<float, 32> nivellsEntrada {};
    std::array<float, 2> nivellsSortida {};
    juce::StringArray nomsEntrades;   // de la targeta (buit: entrada apagada)

    /** Nivell d'una pista: el que sona i, si té el botó de gravar encès, el que entra per la targeta. */
    float nivell (int pista, int canal) const
    {
        if (pista < 0 || pista >= numPistes()) return 0.0f;
        const auto& t = projecte.pistes[(size_t) pista];
        float v = nivells[(size_t) (mibase::slotDePista (t) * 2 + (canal & 1))];
        if (t.grava)
        {
            const int e = juce::jlimit (0, 31, t.entrada + (t.canals == 2 ? (canal & 1) : 0));
            v = juce::jmax (v, nivellsEntrada[(size_t) e]);
        }
        return v;
    }

    /** «E1» o «E1-2»: d'on grava la pista. */
    static juce::String textEntrada (const mibase::Pista& t)
    {
        return "E" + juce::String (t.entrada + 1) + (t.canals == 2 ? "-" + juce::String (t.entrada + 2) : juce::String());
    }

    /** «C», «E 40», «D 25». */
    static juce::String textPan (double pan)
    {
        const int v = juce::roundToInt (pan * 100.0);
        if (v == 0) return "C";
        return (v < 0 ? "E " : "D ") + juce::String (std::abs (v));
    }

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
        return std::find (triats.begin(), triats.end(), mibase::edicio::RefClip { p, c }) != triats.end();
    }

    mibase::Clip* clip (mibase::edicio::RefClip r)
    {
        auto* t = pista (r.pista);
        if (t == nullptr || r.clip < 0 || r.clip >= (int) t->clips.size()) return nullptr;
        return &t->clips[(size_t) r.clip];
    }

    mibase::Clip* clipUnicTriat() { return triats.size() == 1 ? clip (triats[0]) : nullptr; }

    /** Tria un sol clip (i la seva pista). */
    void triaClip (int p, int c) { triats = { { p, c } }; pistaTriada = p; rang.actiu = false; }
    void capTriat() { triats.clear(); rang.actiu = false; }

    /** Color d'un clip: el seu, si en té; si no, el de la pista. */
    juce::Colour colorClip (int p, const mibase::Clip& c) const
    {
        if (c.rgb != 0) return juce::Colour (c.rgb);
        return (p >= 0 && p < numPistes()) ? colorPista (projecte.pistes[(size_t) p]) : colorPista (0);
    }

    /** Paleta de colors de les pistes (8). */
    static juce::Colour colorPista (int i)
    {
        static const juce::uint32 colors[] = { 0xfff5c445, 0xff59d0d6, 0xffb18cff, 0xff7fd67a, 0xff6fa8ff, 0xffff8fc4, 0xffe8a87c, 0xffc9b8a6 };
        return juce::Colour (colors[(size_t) juce::jlimit (0, 7, i)]);
    }
    static juce::Colour colorPista (const mibase::Pista& t) { return t.rgb != 0 ? juce::Colour (t.rgb) : colorPista (t.color); }

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
        triats.erase (std::remove_if (triats.begin(), triats.end(), [this] (mibase::edicio::RefClip r) { return clip (r) == nullptr; }), triats.end());
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
