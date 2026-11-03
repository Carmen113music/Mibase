#pragma once

#include "mibase/Projecte.h"

#include <optional>
#include <string>
#include <vector>

/** Operacions d'edició de l'estudi (sense interfície: es proven soles).
    Línia de temps en segons. Compassos interns des de 0; el número que es veu és intern - 1
    (−1, 0, 1, 2…: el compàs 0 és on comença la cançó, com a Riffs). */
namespace mibase::edicio
{
    constexpr double guanyMinDb = -24.0;
    constexpr double longitudMinima = 0.02;        // un clip no pot ser més curt
    constexpr double allargaMaxim = 600.0;         // es pot allargar 10 minuts de silenci

    double segonsPerTemps (const Projecte& p);
    double segonsPerCompas (const Projecte& p);

    /** Número de compàs que es veu a partir de l'índex intern. */
    inline int numCompas (int intern) { return intern - 1; }

    /** Temps enganxat a la graella. */
    double enganxa (const Projecte& p, double t, Snap snap);

    /** Text de posició: «3.2» (compàs que es veu . temps), o «3.2 +45 ms» en mode Lliure. */
    std::string textPosicio (const Projecte& p, double t, bool ambMs = false);

    Presa* presa (Projecte& p, const std::string& id);
    const Presa* presa (const Projecte& p, const std::string& id);

    /** La bandera de la presa cau dins la part visible del clip? */
    bool banderaVisible (const Projecte& p, const Clip& c);

    /** El punt del clip que s'enganxa a la graella: la bandera, o l'inici visible. */
    double puntSnap (const Projecte& p, const Clip& c);

    // ---- Pistes i preses ----
    std::string idNou (Projecte& p, char prefix);
    Pista& afegeixPista (Projecte& p, const std::string& nom, const std::string& tipus);
    Presa& afegeixPresa (Projecte& p, const std::string& fitxer, const std::string& nom, double durada);
    /** Clip sencer d'una presa a la pista, amb l'inici visible a «on». */
    Clip& afegeixClip (Projecte& p, int pista, const std::string& idPresa, double on);
    /** Una gravació nova a la pista. El fitxer comença a «iniciFitxer» (amb el compte d'entrada inclòs);
        «cursor» és on es va prémer gravar: hi va la bandera i l'inici de la finestra. «latencia» és el retard
        d'anada i tornada de la targeta: el que es grava arriba aquest temps tard i per això es mou enrere.
        Retorna l'índex del clip nou, o -1 si no ha quedat res després del cursor (no s'hi afegeix res). */
    int afegeixGravacio (Projecte& p, int pista, const std::string& fitxer, const std::string& nom, double durada,
                         double iniciFitxer, double cursor, double latencia);
    void esborraPista (Projecte& p, int pista);
    /** Color de pista menys fet servir (la paleta té «colorsPista» colors). */
    constexpr int colorsPista = 8;
    int colorLliure (const Projecte& p);
    void mouPista (Projecte& p, int de, int a);
    /** Només una pista pot tenir el botó de gravar encès. */
    void triaGravacio (Projecte& p, int pista);
    std::string nomPistaLliure (const Projecte& p, const std::string& base);

    // ---- Clips ----
    /** Mou un clip perquè el seu punt de snap caigui a «desti» (ja enganxat). */
    void mouClipA (Projecte& p, Clip& c, double desti);

    /** Mode Lliure amb la bandera visible: la bandera queda quieta a «banderaAbs» i l'àudio llisca.
        Retorna false (i no fa res) si la bandera quedaria fora del clip. */
    bool mouAmbBanderaFixa (Projecte& p, Clip& c, double posNova, double banderaAbs);

    void retallaInici (Clip& c, double iniciAbs);
    void retallaFi (Clip& c, double fiAbs);
    void fadeIn (Clip& c, double fiFadeAbs);
    void fadeOut (Clip& c, double iniciFadeAbs);
    void guany (Clip& c, double db);

    /** Duplica els clips (índexs de la mateixa pista) i posa les còpies just a continuació.
        Retorna els índexs de les còpies. */
    std::vector<int> duplica (Projecte& p, int pista, std::vector<int> clips);

    /** Treu clips (les preses i els fitxers no es toquen). */
    void treuClips (Projecte& p, int pista, std::vector<int> clips);

    /** Passa clips a una altra pista sense moure'ls en el temps. Retorna els nous índexs. */
    std::vector<int> passaAPista (Projecte& p, int de, std::vector<int> clips, int a);

    // ---- Selecció de diverses pistes, copiar i enganxar ----
    struct RefClip
    {
        int pista = -1, clip = -1;
        bool operator== (const RefClip&) const = default;
    };

    /** Tros copiat: cada clip amb la seva pista i la posició relativa al començament del tros. */
    struct Retall
    {
        double durada = 0.0;
        std::vector<std::pair<int, Clip>> clips;
        bool buit() const noexcept { return clips.empty(); }
    };

    /** Copia clips sencers. El tros comença on comença el primer. */
    Retall copiaClips (const Projecte& p, const std::vector<RefClip>& triats);
    /** Copia només el que queda dins [t0, t1) de les pistes [pistaDe, pistaFins] (els clips es retallen). */
    Retall copiaRang (const Projecte& p, int pistaDe, int pistaFins, double t0, double t1);
    /** Deixa buit [t0, t1) a la pista: treu, retalla o parteix els clips que hi toquen. */
    void buidaRang (Projecte& p, int pista, double t0, double t1);
    /** Aparta tot el que hi ha a partir de «t», a totes les pistes, «durada» segons (els clips que hi passen es parteixen). */
    void insereixEspai (Projecte& p, double t, double durada);
    /** Enganxa el tros amb el començament a «on», a les mateixes pistes. «insereix»: aparta el que hi ha;
        si no, trepitja (el que hi havia sota els clips nous es treu). Retorna els clips nous. */
    std::vector<RefClip> enganxaRetall (Projecte& p, const Retall& r, double on, bool insereix);

    std::vector<RefClip> duplicaTriats (Projecte& p, const std::vector<RefClip>& triats);
    void treuTriats (Projecte& p, const std::vector<RefClip>& triats);
    /** Mou els clips «d» pistes amunt o avall (sense moure'ls en el temps; si no hi caben, tan lluny com es pugui). */
    std::vector<RefClip> mouDePista (Projecte& p, const std::vector<RefClip>& triats, int d);

    /** Llapis: finestra buida a la pista, de «de» a «a». Retorna l'índex del clip nou. */
    int creaFinestra (Projecte& p, int pista, double de, double a);

    /** Tisores: talla el clip a «t» (absolut). Retorna false si «t» no cau prou dins el clip. */
    bool talla (Projecte& p, int pista, int clip, double t);

    struct ResultatCola
    {
        bool fet = false;            // s'han ajuntat (clip = el que queda)
        bool calConsolidar = false;  // no es poden ajuntar sense fer un fitxer nou
        int clip = -1;
    };

    /** Cola: ajunta dos clips d'una pista en una sola finestra, si es pot fer sense crear un fitxer nou
        (trossos contigus del mateix àudio al seu lloc, o finestres buides). Si no, diu que cal consolidar. */
    ResultatCola enganxaClips (Projecte& p, int pista, int a, int b);

    /** Posa (o treu, amb un valor negatiu) la bandera de la presa d'un clip, en temps absolut. */
    void posaBandera (Projecte& p, const Clip& c, double abs);

    // ---- Bucle ----
    int compasA (const Projecte& p, double t);
    void marcaBucle (Projecte& p, int compasA, int compasB);   // inclou tots dos
    double iniciBucle (const Projecte& p);
    double fiBucle (const Projecte& p);

    /** Final del projecte (l'últim clip), en segons. */
    double final (const Projecte& p);

    // ---- Desfés ----
    class Desfes
    {
    public:
        static constexpr int maxim = 200;

        /** Guarda com era el projecte abans d'un canvi. */
        void abans (const Projecte& p);
        bool potDesfer() const noexcept { return ! enrere.empty(); }
        bool potRefer() const noexcept  { return ! endavant.empty(); }
        /** Torna a l'estat anterior (retorna false si no n'hi ha). */
        bool desfes (Projecte& p);
        bool refes (Projecte& p);
        void buida() { enrere.clear(); endavant.clear(); }

    private:
        std::vector<Projecte> enrere, endavant;
    };
}
