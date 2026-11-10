#pragma once

#include "Projecte.h"

#include <filesystem>
#include <functional>
#include <vector>

/** 0.22: Vocal Editor (veu monofònica). Tot fora del fil d'àudio.
    - Anàlisi: pYIN (YIN amb probabilitats + camí de Viterbi) a 16 kHz, un punt cada 5 ms; després, les notes
      (segmentació que no talla pel vibrato ni per la deriva lenta; les consonants i respiracions no són notes).
    - So: TD-PSOLA amb marques de període: canvia l'afinació conservant els formants (la veu no «canvia de persona»),
      i els formants es poden moure a part reescalant els grans. Els trams sense to (consonants) no es toquen. */
namespace mibase::veu
{
    constexpr double pas = 0.005;   // segons entre dos punts de l'anàlisi

    /** El que s'ha mesurat d'una presa: per a cada punt (temps = i · pas), l'afinació (Hz; 0 = sense to),
        la probabilitat que hi hagi to i el nivell (RMS lineal). */
    struct Analisi
    {
        double fs = 48000.0;
        long long mostres = 0;
        std::vector<float> f0, prob, rms;
        int punts() const noexcept { return (int) f0.size(); }
        bool buida() const noexcept { return f0.empty(); }
    };

    double midiDeHz (double hz) noexcept;
    double hzDeMidi (double midi) noexcept;

    /** Analitza un senyal mono. «progres» (0..1) pot tornar false per cancel·lar (llavors torna una anàlisi buida). */
    Analisi analitza (const float* x, long long n, double fs, const std::function<bool (float)>& progres = {});

    bool desaAnalisi (const Analisi& a, const std::filesystem::path& cami);
    bool llegeixAnalisi (const std::filesystem::path& cami, Analisi& a);

    /** Les notes que s'hi han cantat (sense canvis). */
    std::vector<NotaVeu> detectaNotes (const Analisi& a);

    /** Corbes per punt, a partir de l'anàlisi i de les notes (NaN = sense to):
        «original» i «editada» en MIDI amb decimals; «canvi» = semitons que s'han de moure (ja suavitzats),
        «formant» en semitons i «guany» lineal. «nota» = índex de la nota de cada punt (-1: cap). */
    struct Corbes
    {
        std::vector<float> original, editada, canvi, formant, guany;
        std::vector<int> nota;
    };
    Corbes corbes (const Analisi& a, const VeuPresa& v);

    /** Quins trossos (en mostres) han de passar pel PSOLA (on hi ha algun canvi). Inclouen un marge sense canvis. */
    struct Tros { long long de = 0, a = 0; };
    std::vector<Tros> trossosAmbCanvis (const Analisi& a, const Corbes& c);
    /** Trossos que s'han de tornar a fer en passar d'unes corbes a unes altres (només el que canvia; si «abans» és
        buit, tots els que tenen canvis). Cada tros comença i acaba on no hi ha canvis. */
    std::vector<Tros> trossosPerRefer (const Analisi& a, const Corbes& abans, const Corbes& ara);

    /** Refà a «sortida» (mateixos canals i longitud que l'entrada) el tros [de, fins) a partir de l'original: hi copia
        l'original i passa pel PSOLA els trossos amb canvis que hi ha a dins. La resta de «sortida» no es toca. */
    void renderitza (const float* const* entrada, int canals, long long n, double fs, const Analisi& a, const Corbes& c,
                     float* const* sortida, long long de, long long fins);

    // ---- Correcció ----
    enum class Desti { Cromatic = 0, Escala = 1 };
    struct Correccio
    {
        double centre = 1.0;      // 0..1: quant s'acosta el centre a la nota objectiu
        double deriva = 0.0;      // 0..1: quanta deriva es treu
        double vibrato = 1.0;     // 0..2: vibrato que es deixa (1 = el mateix)
        Desti desti = Desti::Cromatic;
        Tonalitat tonalitat {};
    };
    /** La nota objectiu (MIDI enter) per a una afinació. */
    int notaObjectiu (double midi, Desti d, const Tonalitat& t);
    /** Aplica la correcció a les notes indicades (totes si «quines» és buit). */
    void corregeix (VeuPresa& v, const std::vector<int>& quines, const Correccio& c);
    /** 0.23: la tonalitat on van les notes d'una presa (segons el selector del Vocal Editor; «canco» = la de la cançó)
        i si és cromàtica (totes les notes valen). */
    Tonalitat tonalitatDe (const VeuPresa& v, const Tonalitat& canco) noexcept;
    inline bool cromatic (const VeuPresa& v) noexcept { return v.modeTon == 1; }
    /** La nota objectiu (MIDI enter) d'una afinació, dins la tonalitat de la presa. */
    int notaObjectiu (double midi, const VeuPresa& v, const Tonalitat& canco);
    /** La nota de la tonalitat que hi ha «graus» graus amunt (o avall) de la més propera a «midi». */
    int mouGraus (double midi, int graus, const VeuPresa& v, const Tonalitat& canco);
    /** La nota tal com sonarà (centre + desplaçament). */
    inline double centreFinal (const NotaVeu& n) noexcept { return n.centre + n.desplac; }
    bool teCanvis (const VeuPresa& v) noexcept;
    /** Torna les notes a l'original (totes si «quines» és buit). */
    void restaura (VeuPresa& v, const std::vector<int>& quines);
}
