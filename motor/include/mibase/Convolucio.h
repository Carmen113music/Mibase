#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/** 0.56: convolució amb respostes impulsionals (IR) per a la Reverb de Mibase.

    FFT particionada en dues etapes:
    - els primers 4096 punts de la IR, en particions de 64 (latència de 64 mostres, 1,3 ms a 48 kHz);
    - la resta, en particions de 2048, amb la feina repartida entre els blocs (el consum és pla, sense pics).
    Tot es calcula al mateix fil d'àudio (també exportant: el resultat és sempre el mateix).

    Fils: la IR i les opcions es posen des de la interfície; un fil de fons prepara el «motor» nou (sense tocar el
    fil d'àudio) i el fil d'àudio el canvia amb una fosa, sense parar el so. mostra() no demana memòria ni s'espera. */
namespace mibase::conv
{
    /** Una IR tal com és al fitxer: 1 canal (mono), 2 (estèreo) o 4 (true stereo: LL, LR, RL, RR). */
    struct IR
    {
        std::vector<std::vector<float>> c;
        double fs = 48000.0;
        std::string nom;          // el nom del fitxer, sense extensió
        std::string referencia;   // com es desa al projecte (veure referencia())
        long long frames() const noexcept { return c.empty() ? 0 : (long long) c[0].size(); }
        int canals() const noexcept { return (int) c.size(); }
        double durada() const noexcept { return fs > 0.0 ? (double) frames() / fs : 0.0; }
    };

    constexpr double duradaMaxima = 20.0;   // segons (la resta es talla)

    /** Llegeix un WAV (16/24 bits o float). 3 canals: es fan servir els 2 primers; més de 4: els 4 primers. */
    std::shared_ptr<IR> carrega (const std::filesystem::path& cami, std::string& error);

    struct Opcions
    {
        double longitud = 1.0;    // 0,05..1: quina part de la IR es fa servir (amb una baixada suau al final)
        double damping = 0.0;     // 0..1: els aguts s'apaguen més de pressa com més avança la cua
        bool normalitza = true;   // tota IR sona amb un volum semblant (no és el volum de sortida)
        bool reversa = false;
        bool operator== (const Opcions&) const = default;
    };

    /** La IR transformada (longitud, damping, reversa, normalització) i passada a la freqüència «fs».
        Torna un vector per camí: 1 (mono), 2 (L→L, R→R) o 4 (LL, LR, RL, RR). */
    std::vector<std::vector<float>> transforma (const IR& ir, const Opcions& o, double fs);

    // ---- Biblioteca ----
    /** Carpeta de la biblioteca d'IR (la posa el programa: Documents\Mibase\IR). */
    void posaArrel (const std::filesystem::path& carpeta);
    std::filesystem::path arrel();
    /** Com es desa un fitxer al projecte: «bib:Halls/Sala gran.wav» si és dins la biblioteca (així el projecte funciona
        en un altre ordinador amb la mateixa biblioteca), o el camí sencer si és fora. */
    std::string referencia (const std::filesystem::path& cami);
    std::filesystem::path resol (const std::string& referencia);
    struct EntradaBiblioteca { std::string categoria, nom; std::filesystem::path cami; };
    /** Els WAV de la biblioteca (la categoria és la primera carpeta). Ordenats per categoria i nom. */
    std::vector<EntradaBiblioteca> llistaBiblioteca();
    /** Les categories de la biblioteca de demostració, per ordre. */
    const std::vector<std::string>& categoriesDemo();
    /** Crea (si no hi són) les IR de demostració dins «carpeta»/Demo… Són sons generats per Mibase (sense drets de
        ningú). Torna quantes n'ha creat. */
    int creaBibliotecaDemo (const std::filesystem::path& carpeta, double fs = 48000.0);

    class Motor;   // (Convolucio.cpp)

    class Convolucio
    {
    public:
        static constexpr int latencia = 64;

        Convolucio();
        ~Convolucio();
        Convolucio (const Convolucio&) = delete;
        Convolucio& operator= (const Convolucio&) = delete;

        // ---- Fil de la interfície ----
        void posaIR (std::shared_ptr<const IR> ir);
        std::shared_ptr<const IR> ir() const;
        /** Fora del fil d'àudio, abans de sonar: prepara el motor ara mateix (sense fosa). */
        void prepara (double fs);
        /** Espera que el fil de fons acabi la feina pendent (per a les proves i per exportar). */
        void espera();

        // ---- Qualsevol fil (sense bloquejos) ----
        void posaOpcions (const Opcions& o) noexcept;
        Opcions opcions() const noexcept;
        /** Mostres de la IR que sona (0: cap). */
        long long longitudActiva() const noexcept { return longActiva.load (std::memory_order_relaxed); }

        // ---- Fil d'àudio ----
        /** Una mostra estèreo d'entrada i la de sortida (64 mostres de retard). */
        void mostra (float iL, float iR, float& oL, float& oR) noexcept;
        /** Oblida la cua (al pròxim bloc). */
        void buida() noexcept { demanaBuidar.store (true, std::memory_order_release); }
        
        /** Fil de fons: si cal, fa el motor nou. Torna true si n'ha fet un. (El crida el treballador.) */
        bool feina (bool forcar = false);

    private:
        void canvia() noexcept;
        friend class Treballador;

        mutable std::mutex mtx;                  // ir, fs (interfície ↔ fil de fons)
        std::shared_ptr<const IR> irActual;
        double fs = 0.0;
        std::atomic<unsigned> versio { 1 };       // canvia amb la IR o les opcions
        std::atomic<unsigned> versioFeta { 0 };
        std::atomic<float> oLong { 1.0f }, oDamp { 0.0f };
        std::atomic<bool> oNorm { true }, oRev { false };
        std::atomic<long long> longActiva { 0 };

        // Fil d'àudio
        Motor* actiu = nullptr;
        Motor* vell = nullptr;                    // el que s'apaga durant la fosa
        float fosa = 1.0f;                        // 0 → 1
        int compta = 0;
        std::atomic<Motor*> pendent { nullptr };  // fil de fons → fil d'àudio
        std::atomic<Motor*> perEsborrar { nullptr };   // fil d'àudio → fil de fons
        std::atomic<bool> demanaBuidar { false };
    };
}
