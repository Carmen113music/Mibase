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

    /** 0.28: mapa de tempo. Les parts amb tempo o compàs propi el canvien de cop a l'inici i el tornen al de la cançó al
        final. Cada tram comença en un compàs intern, un moment (segons) i un tick (960 per negra). */
    struct TramTempo { double compas = 0.0, segons = 0.0, tick = 0.0, tempo = 120.0; int num = 4, den = 4;
                       double segonsPerCompas() const { return 60.0 / tempo * num; }
                       double segonsPerTemps() const { return 60.0 / tempo; } };
    bool teCanvisTempo (const Projecte& p);
    std::vector<TramTempo> mapaTempo (const Projecte& p);
    double segonsDeCompas (const Projecte& p, double compas);
    double compasDeSegons (const Projecte& p, double segons);
    TramTempo tramA (const Projecte& p, double segons);
    double segonsPerCompasA (const Projecte& p, double segons);
    double segonsPerTempsA (const Projecte& p, double segons);

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
    /** 0.34: posa els grups i retorns al final de la llista (sense canviar l'ordre entre ells). */
    void busosABaix (Projecte& p);
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

    // ---- Parts de la cançó (0.13) ----
    /** Crea una part (de i a en compassos interns, en qualsevol ordre) i en torna l'id. */
    std::string creaPart (Projecte& p, const std::string& nom, double de, double a, std::uint32_t rgb);
    Part* part (Projecte& p, const std::string& id);
    void esborraPart (Projecte& p, const std::string& id);
    /** La part que hi ha en aquest temps (segons), o -1. Si se'n sobreposen, la de més tard a la llista. */
    int partA (const Projecte& p, double t);

    // ---- Estructures (0.15) ----
    /** L'estructura que sona (nullptr: lineal, o si no té cap peça). */
    const Estructura* estructuraActiva (const Projecte& p);
    Estructura* estructura (Projecte& p, const std::string& id);
    std::string creaEstructura (Projecte& p, const std::string& nom);

    // ---- Carpetes de pistes (0.11) ----
    Carpeta* carpeta (Projecte& p, const std::string& id);
    /** Crea una carpeta (amb les pistes donades, si n'hi ha) i en torna l'id. */
    std::string creaCarpeta (Projecte& p, const std::string& nom, const std::vector<int>& pistes = {});
    /** Posa una pista a una carpeta (id buit: fora de qualsevol carpeta). Les pistes d'una carpeta queden seguides:
        la pista es mou just després de l'última de la carpeta. Torna on ha anat a parar la pista. */
    int posaACarpeta (Projecte& p, int pista, const std::string& id);
    /** Esborra la carpeta; les pistes queden fora de cap carpeta (no s'esborren). */
    void esborraCarpeta (Projecte& p, const std::string& id);
    std::vector<int> pistesDeCarpeta (const Projecte& p, const std::string& id);
    /** Com es veu cada pista a la taula de treball: 0 = amagada, 1 = normal, 2 = fila de la carpeta plegada
        (la primera pista de la carpeta; la resta, amagades). */
    std::vector<int> filesVisibles (const Projecte& p);
    // ---- 0.33: busos i enviaments ----
    /** Bus nou al final de la llista (de grup, o retorn d'efectes si «fx»). Sempre estèreo. */
    Pista& afegeixBus (Projecte& p, const std::string& nom, bool fx);
    /** Índex de la pista amb aquest id (-1 si no hi és). */
    int pistaPerId (const Projecte& p, const std::string& id) noexcept;
    /** Es pot connectar «origen» (sortida o enviament) a «desti»? «desti» ha de ser un bus i no pot tancar un cercle. */
    bool potConnectar (const Projecte& p, const std::string& origen, const std::string& desti);
    /** Sortida d'una pista: id d'un bus, o buit per a la sortida. Si no es pot (cercle), no canvia res i torna false. */
    bool posaSortida (Projecte& p, int pista, const std::string& bus);
    /** Enviament nou (post-fader, 0 dB, al mig). Torna'n l'índex, o -1 si no es pot. */
    int afegeixEnviament (Projecte& p, int pista, const std::string& bus);
    /** Canvia la destinació d'un enviament (false si no es pot). */
    bool destiEnviament (Projecte& p, int pista, int quin, const std::string& bus);
    void treuEnviament (Projecte& p, int pista, int quin);
    /** Per a cada pista: 0 = callada, 1 = només sonen els seus enviaments (alimenta un retorn en solo),
        2 = sona. Té en compte els muts i els solos: una pista en solo manté els busos per on passa i els
        retorns on envia; un bus en solo manté el que l'alimenta. */
    std::vector<int> estatSolo (const Projecte& p);
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
    /** Crossfade (0.14) al tram [t0, t1) d'una pista d'àudio on el clip de dalt se sobreposa al de sota. Si el de dalt
        hi entra (el seu començament), el de sota fa fade out i el de dalt fade in; si en surt (el seu final), al revès.
        El clip de sota es parteix si cal (no destructiu). Torna false si no hi ha dos clips sobreposats al tram. */
    bool crossfade (Projecte& p, int pista, double t0, double t1);

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
