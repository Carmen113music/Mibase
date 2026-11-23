#pragma once

#include "MotorEstudi.h"
#include "Projecte.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

/** 0.86: Elastic Audio. Canviar el temps d'un clip d'àudio sense canviar-ne el to.

    Cada clip pot tenir uns punts d'elasticitat (Clip::warp): «aquest moment del fitxer sona en aquest moment del clip».
    Entre dos punts, l'àudio s'estira o s'encongeix de manera uniforme. Sense punts, el clip és l'original.
    Tres maneres de fer-ho servir, amb el mateix motor:
      A) Estirar des de les vores: tot el clip s'allarga o s'escurça (tots els punts s'escalen).
      B) Un marcador al mig: els dos trossos es reparteixen el temps i la durada total no canvia.
      C) Tants marcadors com calgui (també als cops, detectats sols), i quantitzar-los a la graella.

    Mai es toca el fitxer: el so estirat es calcula a part (fora del fil d'àudio) i es guarda a la memòria.
    Algorisme: WSOLA (finestres que se superposen i es busquen on encaixen millor), el mateix del sampler però amb un
    mapa de temps variable. Per no esmorteir els atacs, els modes General i Ritme tallen als cops: cada cop comença
    exactament on toca, amb l'atac intacte, i només s'estira el que hi ha entre dos cops. */
namespace mibase::elastic
{
    enum Mode { General = 0, Ritme = 1, Veu = 2 };
    const char* nomMode (int mode);

    constexpr double ratioMin = 0.25;       // un tros pot anar fins a 4 vegades més ràpid…
    constexpr double ratioMax = 4.0;        // …o 4 vegades més lent
    constexpr double trosMinim = 0.010;     // cap tros (entre dos punts) no pot fer menys de 10 ms
    constexpr double distanciaMinima = 0.020;   // dos marcadors no poden quedar més a prop (temps del clip)

    bool actiu (const Clip& c) noexcept;

    /** Del temps del fitxer al temps del clip, i al revés (en segons). Fora dels punts, a velocitat normal. */
    double aClip (const std::vector<PuntWarp>& w, double font) noexcept;
    double aFont (const std::vector<PuntWarp>& w, double desti) noexcept;
    inline double aClip (const Clip& c, double font) noexcept { return aClip (c.warp, font); }
    inline double aFont (const Clip& c, double desti) noexcept { return aFont (c.warp, desti); }

    /** Quant dura tot el fitxer dins el clip (estirat). */
    double duradaClip (const Clip& c, double duradaFitxer) noexcept;

    /** El ritme d'un tros: durada al clip / durada al fitxer (1 = original; 2 = el doble de lent). */
    double ratioTros (const std::vector<PuntWarp>& w, size_t i) noexcept;

    /** Prepara el clip per a l'Elastic Audio (si encara no ho estava): punts al començament i al final del fitxer. */
    void activa (Clip& c, double duradaFitxer);

    /** Mode A: arrossega una vora del clip (temps absolut de la línia de temps). L'altra vora no es mou.
        Torna el factor aplicat (1 = res). Respecta els límits de velocitat de tots els trossos. */
    double estiraVora (Clip& c, double duradaFitxer, bool dreta, double vora);

    /** Els marcadors que es veuen i es poden moure (índexs de c.warp): els que cauen dins la finestra del clip,
        sense comptar els que són a les vores. */
    std::vector<int> marcadors (const Clip& c);
    /** És a la vora del clip (fix: no es mou arrossegant-lo, sinó estirant la vora). */
    bool esVora (const Clip& c, size_t i) noexcept;

    /** Posa un marcador a «t» (temps absolut) sense canviar res del so. Torna l'índex (o -1 si n'hi ha un massa a prop).
        Si és el primer, també fixa les vores de la finestra (marcadors 1 i 3 del mode B). */
    int afegeixMarcador (Clip& c, double duradaFitxer, double t);
    /** Mou el marcador «i» a «t» (temps absolut). El que hi ha al costat s'estira o s'encongeix; els veïns no es mouen.
        Torna on ha quedat de debò (dins els límits). */
    double mouMarcador (Clip& c, int i, double t);
    bool treuMarcador (Clip& c, int i);
    /** Treu tots els marcadors de dins però conserva l'estirament de tot el clip (mode A). */
    void treuMarcadors (Clip& c, double duradaFitxer);
    /** Torna a l'àudio original: sense punts, el mateix començament a la línia de temps i el contingut que es veia. */
    void restaura (Clip& c);

    /** Cops (atacs) d'un àudio, en segons del fitxer. «sensibilitat» de 0 (només els forts) a 1 (tots). */
    std::vector<double> cops (const AudioPresa& a, double sensibilitat);
    /** Mode C: un marcador a cada cop que cau dins la finestra del clip. Torna quants n'ha posat. */
    int marcadorsAlsCops (Clip& c, double duradaFitxer, const std::vector<double>& copsFitxer);
    /** Porta els marcadors a la línia de la graella més propera (amb «força» 0..1). Torna quants s'han mogut. */
    int quantitza (Clip& c, const Projecte& p, double forca = 1.0);

    /** Clau del so estirat (canvia si canvien els punts, l'algorisme o la presa; no si es mou o es retalla el clip).
        «corregida»: es parteix del so del Vocal Editor. */
    std::string clau (const Clip& c, bool corregida);

    /** El so estirat (fora del fil d'àudio). La mostra 0 és el temps 0 del clip. «progres» pot tornar false per parar
        (llavors torna nullptr). */
    std::shared_ptr<AudioPresa> renderitza (const AudioPresa& original, const std::vector<PuntWarp>& w, int mode,
                                            const std::function<bool (float)>& progres = {});
}
