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
    mutable std::vector<int> files;   // cau de edicio::filesVisibles
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

    /** Els editors MIDI oberts (Piano Rolls apilats a la zona de baix, fins a 3). Cada un mostra un clip; un editor
        bloquejat es queda amb el seu clip, i el que no ho està obre el clip MIDI on es fa doble clic. */
    struct EditorMidi
    {
        mibase::edicio::RefClip clip;
        std::string idPista;        // per seguir la pista si canvia de lloc (s'esborra o es mou una altra)
        bool bloquejat = false;
        bool pads = false;          // a baix, els pads d'acords (Chord Pad) en lloc de les velocitats
        std::vector<std::string> fantasmes;   // 0.10: pistes (id) de les quals es veuen les notes fantasma (per defecte, cap)
    };
    static constexpr int maxEditors = 3;
    std::vector<EditorMidi> editors;
    int editorActiu = 0;            // el que fan servir el panell d'harmonia i les dreceres
    int filaBateria = 0;            // 0.20: la fila del Drumpad triada (el panell de la bateria l'edita)
    bool pianoRoll = false;         // la zona dels editors es veu

    /** 0.22: Vocal Editor obert a baix, sobre la presa d'un clip d'àudio (les edicions són de la presa). */
    struct EditorVeu
    {
        bool obert = false;
        int pista = -1, clip = -1;
        std::string presa;
        std::vector<int> triades;   // notes triades (índexs)
        int eina = 0;               // EditorVeu::Eina
        bool ona = true;            // l'ona de fons
    };
    EditorVeu veu;
    /** La presa que edita el Vocal Editor (nullptr si no n'hi ha). */
    mibase::Presa* presaVeu()
    {
        if (! veu.obert) return nullptr;
        for (auto& s : projecte.preses) if (s.id == veu.presa) return &s;
        return nullptr;
    }

    /** Vista horitzontal comuna a tots els editors (així tenen les mateixes referències de temps). */
    struct VistaEditors
    {
        double iniciVista = 0.0;    // ticks absoluts
        double pxPerTick = 0.06;
        bool feta = false;          // ja s'ha encaixat un cop
    } vistaEditors;

    /** Acord preparat per posar al Piano Roll amb un clic (del panell d'harmonia). */
    struct Segell
    {
        std::vector<int> notes;
        long long durada = 0;       // ticks
        bool enllaca = false;       // amb moviment mínim des de l'acord d'abans
        juce::String nom;
        bool perArrossegar = false; // es posa arrossegant (s'esborra quan s'amolla, caigui on caigui)
        bool actiu() const noexcept { return ! notes.empty(); }
    } segell;

    void editorMostra (int i, mibase::edicio::RefClip r)
    {
        if (i < 0 || i >= (int) editors.size()) return;
        editors[(size_t) i].clip = r;
        editors[(size_t) i].idPista = pista (r.pista) != nullptr ? projecte.pistes[(size_t) r.pista].id : std::string();
    }

    /** El clip d'un editor, si encara existeix i és MIDI. */
    mibase::Clip* clipEditor (int i)
    {
        if (i < 0 || i >= (int) editors.size()) return nullptr;
        auto& ed = editors[(size_t) i];
        if (auto* t = pista (ed.clip.pista); t == nullptr || t->id != ed.idPista)
        {
            ed.clip.pista = -1;
            for (int k = 0; k < numPistes(); ++k)
                if (projecte.pistes[(size_t) k].id == ed.idPista) ed.clip.pista = k;
        }
        auto* c = clip (ed.clip);
        return (c != nullptr && c->midi) ? c : nullptr;
    }

    /** Quin editor mostra aquest clip (-1: cap). */
    int editorDe (mibase::edicio::RefClip r)
    {
        for (int i = 0; i < (int) editors.size(); ++i)
            if (clipEditor (i) != nullptr && editors[(size_t) i].clip == r) return i;
        return -1;
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
    /** 0.15: taula d'estructura (en lloc de la taula de treball). */
    bool vistaEstructura = false;
    bool foraTreball() const noexcept { return vistaMescles || vistaEstructura; }
    /** 0.16: el Piano Roll té el «focus» (la columna esquerra hi mostra el seu panell); clicant una altra pista, no. */
    bool focusEditor = true;

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
    // Files de la taula de treball (0.11): amb carpetes plegades o amagades, algunes pistes no es veuen (alçada 0)
    // i la primera d'una carpeta plegada és la fila de la carpeta.
    int modeFila (int i) const noexcept
    {
        if (i < 0 || i >= numPistes()) return 1;
        if (files.size() != projecte.pistes.size()) files = mibase::edicio::filesVisibles (projecte);
        return files[(size_t) i];
    }
    int altDe (int i) const noexcept          { return modeFila (i) == 0 ? 0 : altPista; }
    bool filaCarpeta (int i) const noexcept   { return modeFila (i) == 2; }
    // 0.17: la primera fila és la del màster (pistaTriada == master quan és la triada).
    static constexpr int master = -2;
    bool masterTriat() const noexcept         { return pistaTriada == master; }
    int yMaster() const noexcept              { return -desplY; }
    int yPista (int i) const noexcept
    {
        int y = altPista;
        for (int k = 0; k < juce::jmin (i, numPistes()); ++k) y += altDe (k);
        if (i > numPistes()) y += (i - numPistes()) * altPista;
        return y - desplY;
    }
    int pistaA (int y) const noexcept
    {
        const int yy = y + desplY - altPista;
        if (yy < 0) return (int) std::floor ((double) yy / altPista);
        int acc = 0;
        for (int i = 0; i < numPistes(); ++i)
        {
            const int h = altDe (i);
            if (h > 0 && yy < acc + h) return i;
            acc += h;
        }
        return numPistes() + (yy - acc) / altPista;
    }
    /** Alçada de totes les files visibles. */
    int altTotal() const noexcept             { return yPista (numPistes()) + desplY; }
    /** Torna a calcular quines pistes es veuen (després de canviar carpetes). */
    void recalculaFiles() { files = mibase::edicio::filesVisibles (projecte); }

    /** Pistes triades amb Ctrl o Maj (per ids; la pistaTriada n'és la principal). */
    std::vector<std::string> pistesTriades;
    bool pistaEsTriada (int i) const
    {
        if (i == pistaTriada) return true;
        if (i < 0 || i >= numPistes()) return false;
        return std::find (pistesTriades.begin(), pistesTriades.end(), projecte.pistes[(size_t) i].id) != pistesTriades.end();
    }
    /** Índexs de les pistes triades (la principal inclosa), en ordre. */
    std::vector<int> indexsTriades() const
    {
        std::vector<int> r;
        for (int i = 0; i < numPistes(); ++i) if (pistaEsTriada (i)) r.push_back (i);
        return r;
    }
    /** 0.12: s'està triant un clip de la taula de treball per veure'l en fantasma a l'editor d'aquest índex (-1: no). */
    int triantFantasma = -1;
    /** Mostra el panell de carpetes a l'esquerra dels noms de pista. */
    bool panellCarpetes = false;

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
    void avisa() { recalculaFiles(); sendSynchronousChangeMessage(); }

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
        pistesTriades.erase (std::remove_if (pistesTriades.begin(), pistesTriades.end(), [this] (const std::string& id)
            { return std::none_of (projecte.pistes.begin(), projecte.pistes.end(), [&id] (const mibase::Pista& t) { return t.id == id; }); }),
            pistesTriades.end());
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
