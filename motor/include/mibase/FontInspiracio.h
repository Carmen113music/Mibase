#pragma once
// 0.97: Font d'inspiració. Una eina de composició ràpida: una fila d'acords que es tria amb les fletxes, que suggereix
// acords segons la tonalitat i els veïns, que sona amb piano o guitarra General MIDI i un patró, i que s'envia a la
// pista d'Acords. Aquí hi ha tot el que no és interfície (es prova sol): la idea, els suggeriments, les notes i el
// reproductor que sona pel fil d'àudio sense passar pel transport de la cançó.
#include "mibase/Instrument.h"
#include "mibase/Projecte.h"
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mibase::font
{
    /** Un acord de la fila. Els acords van sempre enganxats l'un darrere l'altre: la posició surt de les durades
        dels d'abans. Durada en temps del compàs (negres en 4/4; mig temps com a mínim). Acord buit = silenci. */
    struct AcordIdea
    {
        std::string acord;          // «G», «Bm», «D7»; buit = silenci
        double temps = 4.0;
        bool operator== (const AcordIdea&) const = default;
    };

    enum class Instr { Piano = 0, Guitarra = 1 };

    /** Tot el que es desa amb el projecte. */
    struct Idea
    {
        std::vector<AcordIdea> acords;
        Instr instrument = Instr::Piano;
        int patroPiano = 0, patroGuitarra = 0;   // índex a opcionsPatrons()
        bool bucle = true;
        bool tePropia = false;                   // la tonalitat triada aquí (si no, la del projecte)
        Tonalitat tonalitat;
        bool operator== (const Idea&) const = default;
    };
    std::string textIdea (const Idea& i);     // JSON
    Idea llegeixIdea (const std::string& json);

    constexpr double tempsMinim = 0.5;
    double inici (const Idea& i, int index);  // en temps
    double durada (const Idea& i);

    // ---- Edició (pura) ----
    /** Afegeix un acord al final. */
    void afegeix (Idea& i, const std::string& acord, double temps);
    /** Treu un acord: els de després s'avancen. */
    void treu (Idea& i, int index);
    /** Mou la frontera amb l'acord anterior «delta» temps (negatiu: entra abans). L'anterior s'allarga o s'escurça; el
        final dels dos no es mou. Cap dels dos baixa de mig temps. El primer acord no es pot moure. */
    bool mouInici (Idea& i, int index, double delta);
    /** Allarga o escurça l'acord (els de després es desplacen). Mínim mig temps, màxim 8 compassos. */
    bool canviaDurada (Idea& i, int index, double delta, int tempsCompas);

    // ---- Suggeriments ----
    enum class Nivell { Habitual = 0, Color = 1, Atrevit = 2 };
    struct Suggeriment
    {
        std::string acord;   // buit = silenci
        Nivell nivell = Nivell::Habitual;
        double puntuacio = 0.0;
    };
    /** Els acords que es poden provar al lloc «index» (pot ser == mida: el que vindria després de l'últim). Els sis
        primers són els més habituals de la tonalitat, ordenats segons l'acord d'abans, el de després, la posició dins la
        frase i els que ja hi ha; després venen setenes, sus, dominants secundàries, acords manllevats i el silenci.
        Sense repetits. */
    std::vector<Suggeriment> suggeriments (const Tonalitat& t, const std::vector<AcordIdea>& fila, int index);
    /** L'acord més probable per posar a «index» (el primer de la llista que no és igual que l'anterior). */
    std::string mesProbable (const Tonalitat& t, const std::vector<AcordIdea>& fila, int index);
    bool esMenor (const Tonalitat& t);

    // ---- So ----
    struct OpcioPatro { const char* id; const char* nom; };
    /** Els quatre patrons de cada instrument (dels que ja hi ha a Mibase). */
    const std::array<OpcioPatro, 4>& opcionsPatrons (Instr i);
    constexpr int programaPiano = 0;      // GM 1: Acoustic Grand Piano
    constexpr int programaGuitarra = 24;  // GM 25: Acoustic Guitar (nylon)
    long long ticksTemps (int den);       // 960 * 4 / den
    /** Les notes de la fila (ticks des de l'inici de la fila). El piano tria la inversió més propera a l'acord anterior.
        «nomes» >= 0: només les d'aquest acord (amb el voicing que li toca dins la fila), des de zero. */
    std::vector<Nota> notes (const Idea& idea, int num, int den, double tempo, int nomes = -1);

    /** El que sona: les notes, la llargada (ticks), si torna a començar i el tempo. */
    struct Programa
    {
        std::vector<Nota> notes;   // per ordre d'inici
        long long llargada = 0;
        bool bucle = false;
        double tempo = 120.0;
    };
    std::shared_ptr<const Programa> programa (std::vector<Nota> n, long long llargada, bool bucle, double tempo);

    /** Sona pel fil d'àudio, a part del transport: es pot canviar el programa mentre sona (continua des d'on era, sense
        tallar les notes que ja sonen) i l'instrument. Fils: posaInstrument, toca, atura, prepara: interfície (o mentre
        l'àudio està aturat); processa: àudio (no demana memòria ni s'espera). */
    class Reproductor
    {
    public:
        void prepara (double fs, int maxBloc);
        /** L'instrument que sona (ja creat; aquí es prepara). nullptr = res. */
        void posaInstrument (std::shared_ptr<Instrument> i);
        /** Fa sonar un programa. desDelPrincipi: comença a «desDe» (ticks) i apaga el que sonava; si no, continua des
            d'on és. */
        void toca (std::shared_ptr<const Programa> p, bool desDelPrincipi, double desDe = 0.0);
        void atura();
        /** Suma el so a L i R (poden ser nullptr). */
        void processa (float* L, float* R, int n) noexcept;

        bool sonant() const noexcept { return actiu.load (std::memory_order_relaxed); }
        bool enBucle() const noexcept { return bucleAra.load (std::memory_order_relaxed); }
        /** On és (ticks dins el programa) i quin programa (generació) està sonant. */
        double posicio() const noexcept { return posAra.load (std::memory_order_relaxed); }
        float guany = 0.8f;

    private:
        // interfície
        std::mutex mutex;   // mai al fil d'àudio
        double fs = 48000.0;
        int maxBloc = 2048;
        std::array<std::shared_ptr<const Programa>, 8> programesVius;
        std::array<std::shared_ptr<Instrument>, 4> instrumentsVius;
        int iProg = 0, iInstr = 0;
        // compartit
        std::atomic<const Programa*> progNou { nullptr };
        std::atomic<int> genProg { 0 };
        std::atomic<bool> reinicia { false }, parar { false };
        std::atomic<double> desDeNou { 0.0 };
        std::atomic<Instrument*> instrNou { nullptr };
        std::atomic<bool> actiu { false }, bucleAra { false };
        std::atomic<double> posAra { 0.0 };
        std::atomic<double> fsAudio { 48000.0 };
        // fil d'àudio
        const Programa* prog = nullptr;
        Instrument* instr = nullptr;
        int genVista = 0;
        double pos = 0.0;
        struct Sonant { int to = -1; double queden = 0.0; };   // mostres que li queden
        std::array<Sonant, 96> sonen {};
        std::array<EventInstrument, 512> events {};
        std::vector<float> tmpL, tmpR;
        bool fentSo = false;
        void apagaTotes (int& numEv, int offset) noexcept;
    };
}
