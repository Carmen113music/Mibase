#pragma once

#include "mibase/ContextMusical.h"
#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <string>
#include <vector>

/** 0.73: canvi de to de la veu en temps real (base de l'Harmonitzador, de l'Octava/Formant i, més endavant, del Tune).

    Com funciona (el mateix principi que el Vocal Editor, però mentre sona):
    - Detecció de l'afinació amb YIN sobre la veu rebaixada a ~12 kHz, cada 5 ms (de 90 a 1000 Hz).
    - Marques de període (un pols de la veu per període) sobre la veu filtrada.
    - TD-PSOLA: grans de dos períodes, posats al ritme del període nou. Així l'afinació canvia però els formants (el
      «color» de la persona) es queden; els formants es poden moure a part reescalant els grans. Els trossos sense to
      (consonants, respiracions) passen tal qual.
    - Té una latència fixa (uns 35 ms a 48 kHz), que el PDC compensa; la veu seca també es retarda perquè quadri.
    Fil d'àudio: no demana memòria (tot es prepara a prepara()). */
namespace mibase::canvito
{
    struct Marca { double pos = 0.0; float periode = 0.0f; bool sonor = false; };

    /** La veu que entra: anell, detecció de l'afinació i marques de període. Compartit per totes les veus noves. */
    class Analitzador
    {
    public:
        void prepara (double fs);
        void buida() noexcept;
        /** Fil d'àudio: una mostra nova (mono). */
        void entra (float x) noexcept;
        long long mostres() const noexcept { return n; }
        double fs() const noexcept { return frq; }
        /** L'afinació d'ara (Hz; 0 = sense to). */
        float f0() const noexcept { return f0Ara; }
        /** La veu (lineal) a la posició «p» (mostres des del principi; interpolada). */
        float mostra (double p) const noexcept;
        /** Mostres de retard que fan servir les veus noves (latència). */
        int latencia() const noexcept { return lat; }
        double hMax() const noexcept { return hmax; }
        double tu() const noexcept { return tSord; }
        // marques (anell)
        long long numMarques() const noexcept { return nMarques; }
        long long primeraMarca() const noexcept { return nMarques > (long long) marques.size() ? nMarques - (long long) marques.size() : 0; }
        const Marca& marca (long long k) const noexcept { return marques[(size_t) (k & (long long) (marques.size() - 1))]; }

    private:
        void detecta() noexcept;
        void marcaNova() noexcept;
        double pic (double de, double a) const noexcept;
        double frq = 48000.0, tSord = 240.0, hmax = 533.0;
        int lat = 1700;
        long long n = 0;
        std::vector<float> anell, filtrat;
        long long mascara = 0;
        float lp1 = 0.0f, lp2 = 0.0f, k900 = 0.1f, kD = 0.5f;   // filtres (marques i delmat)
        // detecció
        int dec = 4, w = 300, tauMax = 133, tauMin = 12, hop = 60;
        double fd = 12000.0;
        float d1 = 0.0f, d2 = 0.0f, acum = 0.0f;
        int comptaDec = 0, comptaHop = 0;
        std::vector<float> dAnell, dif, cmnd, lin;
        long long dn = 0, dMascara = 0;
        float hist[3] {};
        float f0Ara = 0.0f;
        // 0.77: continuïtat (contra els errors d'octava i els talls a mitja nota)
        float fEstable = 0.0f;      // el darrer to bo
        int edat = 1000;            // passos (5 ms) des del darrer to bo
        int passosSalt = 0;         // quants passos seguits demana un salt d'octava
        float fSalt = 0.0f;
        // marques
        std::vector<Marca> marques;
        long long nMarques = 0;
        double ultima = 0.0;
        bool ultimaSonor = false;
    };

    /** 0.79: per a l'afinador: a cada pols amb to, la nota que hi ha (MIDI amb decimals, del període real del pols) i el
        temps des del pols anterior; torna els semitons que s'hi han de moure. */
    struct Afinacio { virtual double desplacament (double nota, double dt) noexcept = 0; virtual ~Afinacio() = default; };

    /** Una veu nova: el mateix que entra, amb una altra afinació i (si es vol) uns altres formants. */
    class Veu
    {
    public:
        void prepara (const Analitzador& a);
        void buida() noexcept;
        /** Fil d'àudio, just després d'Analitzador::entra: genera els grans que ja es poden fer i torna la mostra de sortida
            (retardada «latencia()» mostres). «semitons» i «formant» (semitons) s'apliquen als trossos amb to;
            «guanySord» als que no en tenen. */
        float processa (const Analitzador& a, double semitons, double formant, float guanySord, Afinacio* af = nullptr) noexcept;
        int salts = 0;   // per a les proves: quants cops s'ha hagut de saltar endavant (forat a la sortida)

    private:
        std::vector<float> acc, pes;
        long long mascara = 0;
        double tS = 0.0;
        long long cursor = 0;
    };
}

/** 0.73: «Harmonitzador» del Vocal Studio («intern:harmonitzador»). Fins a 4 veus noves a partir de la veu que canta:
    - Tonalitat: cada veu a uns graus de l'escala de la cançó (una 3a amunt, una 5a…). Afinades encara que la veu no ho sigui.
    - Acords: cada veu a una nota de l'acord que sona a la pista d'Acords (la 1a de l'acord per sobre, la 2a…). Si al
      capçal no hi ha acord, fa servir la tonalitat.
    - Teclat MIDI: les veus són les notes que toques al teclat (com Bon Iver o Imogen Heap). */
namespace mibase::harmonitzador
{
    inline constexpr const char* idPlugin = "intern:harmonitzador";
    inline constexpr const char* nomPlugin = "Harmonitzador (Mibase)";
    constexpr int numVeus = 4;

    enum Mode : int { Tonalitat, Acords, Teclat, numModes };
    enum Param : int { ModeP, Sec, Harmonies, Formant, Humanitza, Lliscament,
                       Interval1, Nivell1 = Interval1 + numVeus, Pan1 = Nivell1 + numVeus, numParams = Pan1 + numVeus };

    struct InfoParam { const char* clau; const char* nom; float mn, mx, def; const char* ajuda; };
    const InfoParam& info (int p);
    const char* nomMode (int m);
    /** El nom d'un interval per a una veu: «3a amunt», «5a avall», «2a nota de l'acord per sobre», «Apagada». */
    std::string textInterval (int mode, int valor);

    /** La nota (MIDI) «passos» notes de la màscara (12 bits, classes de to) amunt o avall de «nota» (que primer s'ajusta a la
        màscara si no hi és). Per a l'escala i per als acords. */
    int mouDinsMascara (int nota, int mascara, int passos) noexcept;
    /** La nota de la màscara més propera (en empat, la de sota). */
    int ajustaAMascara (int nota, int mascara) noexcept;

    class Harmonitzador : public EfecteIntern
    {
    public:
        Harmonitzador();
        ~Harmonitzador() override;
        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;
        int latencia() const noexcept override { return an.latencia(); }

        int numParametres() const override { return numParams; }
        InfoParametre infoParametre (int i) const override;
        void posaParametre (int i, float v01) noexcept override;
        float parametre (int i) const noexcept override;
        std::string textParametre (int i, float v01) const override;
        static float normal (int p, float v) noexcept;
        static float deNormal (int p, float v01) noexcept;

        void posa (int p, float v) noexcept;
        float valor (int p) const noexcept;
        std::string estat() const override;
        void posaEstat (const std::string& s) override;
        void buida() noexcept override { demanaBuidar.store (true); }

        /** Per a la finestra: la nota que canta (MIDI amb decimals, < 0 sense to) i la nota de cada veu (-1 = no sona). */
        float notaDetectada() const noexcept { return detectada.load (std::memory_order_relaxed); }
        int notaVeu (int v) const noexcept { return v >= 0 && v < numVeus ? notesVeus[(size_t) v].load (std::memory_order_relaxed) : -1; }
        /** El context d'on llegeix la tonalitat, els acords i el teclat (per defecte, el global). */
        void posaContext (ContextMusical* c) noexcept { ctx = c; }

    private:
        void registraTeclat (bool vol) noexcept;
        std::array<std::atomic<float>, numParams> par;
        std::atomic<bool> demanaBuidar { false };
        std::atomic<bool> escolta { false };
        ContextMusical* ctx;
        canvito::Analitzador an;
        std::array<canvito::Veu, numVeus> veus;
        std::vector<float> secL, secR;
        long long secMascara = 0, secW = 0;
        std::array<float, numVeus> semi {}, guany {}, deriva {}, desti {};
        std::array<float, numVeus> derivaDe {}, derivaA {}, derivaFase {}, derivaPas {};   // 0.77: deriva lenta
        std::array<std::atomic<int>, numVeus> notesVeus;
        std::atomic<float> detectada { -1.0f };
        int notaLead = -1;
        int pendent = -1;                    // 0.74: nota nova que encara no ha durat prou (40 ms)
        long long pendentMostres = 0;
        int ultimesNotes[numVeus] { -1, -1, -1, -1 };   // 0.74: teclat: les darreres notes (es mantenen 120 ms)
        int nUltimes = 0;
        long long senseNotes = 0;
        float sSec = 1.0f;
        unsigned llavor = 12345u;
        double fs = 48000.0;
    };
}

/** 0.73: «Octava i formant» («intern:octava»): la veu més greu o més aguda (fins a dues octaves) i el seu color
    (formants) a part: una veu més fosca o més prima sense canviar la nota, una octava avall per doblar… */
namespace mibase::octava
{
    inline constexpr const char* idPlugin = "intern:octava";
    inline constexpr const char* nomPlugin = "Octava i formant (Mibase)";
    enum Param : int { To, Fi, Formant, Mescla, numParams };
    struct InfoParam { const char* clau; const char* nom; float mn, mx, def; const char* ajuda; };
    const InfoParam& info (int p);

    class Octava : public EfecteIntern
    {
    public:
        Octava();
        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;
        int latencia() const noexcept override { return an.latencia(); }
        int numParametres() const override { return numParams; }
        InfoParametre infoParametre (int i) const override;
        void posaParametre (int i, float v01) noexcept override;
        float parametre (int i) const noexcept override;
        std::string textParametre (int i, float v01) const override;
        static float normal (int p, float v) noexcept;
        static float deNormal (int p, float v01) noexcept;
        void posa (int p, float v) noexcept;
        float valor (int p) const noexcept;
        std::string estat() const override;
        void posaEstat (const std::string& s) override;
        void buida() noexcept override { demanaBuidar.store (true); }
    private:
        std::array<std::atomic<float>, numParams> par;
        std::atomic<bool> demanaBuidar { false };
        canvito::Analitzador an;
        canvito::Veu veu;
        std::vector<float> secL, secR;
        long long secMascara = 0, secW = 0;
        float sMescla = 0.0f;
        bool primer = true;
    };
}

/** 0.79: «Tune» (estil Auto-Tune, «intern:tune»): porta la veu a la nota més propera de l'escala, en temps real.
    - Escala: la de la cançó (la tonalitat del projecte), una de triada (tònica + escala) o cromàtica (totes les notes).
    - Velocitat: quant triga a arribar a la nota (0 ms = l'efecte robòtic; 20–50 ms = natural).
    - Humanitza: a les notes llargues corregeix més a poc a poc, perquè es conservi el vibrato.
    - Correcció: quina part de la desafinació es corregeix. Mescla: amb la veu original.
    Mateix motor que l'Harmonitzador (uns 36 ms de retard, que el PDC compensa). */
namespace mibase::tune
{
    inline constexpr const char* idPlugin = "intern:tune";
    inline constexpr const char* nomPlugin = "Tune (Mibase)";
    enum ModeEscala : int { Canco, Triada, Cromatica, numModesEscala };
    enum Param : int { ModeP, Tonica, Escala, Velocitat, Humanitza, Correccio, Mescla, numParams };
    struct InfoParam { const char* clau; const char* nom; float mn, mx, def; const char* ajuda; };
    const InfoParam& info (int p);
    constexpr int numEscales = 7;
    const char* nomEscala (int e);
    int mascaraEscala (int e);   // intervals des de la tònica (12 bits)

    class Tune : public EfecteIntern
    {
    public:
        Tune();
        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;
        int latencia() const noexcept override { return an.latencia(); }
        int numParametres() const override { return numParams; }
        InfoParametre infoParametre (int i) const override;
        void posaParametre (int i, float v01) noexcept override;
        float parametre (int i) const noexcept override;
        std::string textParametre (int i, float v01) const override;
        static float normal (int p, float v) noexcept;
        static float deNormal (int p, float v01) noexcept;
        void posa (int p, float v) noexcept;
        float valor (int p) const noexcept;
        std::string estat() const override;
        void posaEstat (const std::string& s) override;
        void buida() noexcept override { demanaBuidar.store (true); }
        void posaContext (ContextMusical* c) noexcept { ctx = c; }
        /** Per a la finestra: la nota que canta (MIDI amb decimals; < 0 sense to), la nota on la porta (-1) i la correcció
            que s'hi aplica ara (cèntims). */
        float notaDetectada() const noexcept { return detectada.load (std::memory_order_relaxed); }
        int notaDesti() const noexcept { return desti.load (std::memory_order_relaxed); }
        float correccioAra() const noexcept { return correccio.load (std::memory_order_relaxed); }
    private:
        std::array<std::atomic<float>, numParams> par;
        std::atomic<bool> demanaBuidar { false };
        ContextMusical* ctx;
        canvito::Analitzador an;
        canvito::Veu veu;
        std::vector<float> secL, secR;
        long long secMascara = 0, secW = 0;
        std::atomic<float> detectada { -1.0f }, correccio { 0.0f };
        std::atomic<int> desti { -1 };
        /** La correcció de cada pols (0.79). */
        struct Calcul : canvito::Afinacio
        {
            double desplacament (double nota, double dt) noexcept override;
            int escala = 0xFFF, notaObj = -1, candidat = -1;   // 0.80: la nota nova ha d'aguantar uns ms abans de canviar
            double vel = 0.025, hum = 0.3, corr = 1.0, temps = 0.0, s = 0.0, ultimaNota = -1.0, suau = -1.0, tempsCand = 0.0;
        } calcul;
        float sMescla = 1.0f;
        double fs = 48000.0;
    };
}
