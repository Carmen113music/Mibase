#pragma once

#include "mibase/Projecte.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

/** 0.56: automatització de paràmetres (volum, panorama, qualsevol paràmetre d'un insert o d'un instrument).
    Les corbes es desen a cada pista (Pista::autos) en compassos interns i valors normalitzats 0..1.
    Aquí hi ha el càlcul de les corbes (el mateix per a la pantalla i per al so) i l'edició. Sense JUCE. */
namespace mibase::automatitzacio
{
    // ---- Objectius ----
    inline constexpr const char* volum = "volum";
    inline constexpr const char* pan = "pan";
    std::string objectiuInsert (const std::string& idInsert, const std::string& idParam);
    std::string objectiuInstrument (const std::string& idParam);
    /** Separa un objectiu: tipus 0 volum, 1 pan, 2 insert, 3 instrument (-1 si no s'entén). */
    int llegeixObjectiu (const std::string& objectiu, std::string& idInsert, std::string& idParam);

    // ---- Valors ----
    /** Volum: 0..1 → guany lineal (0,8 = 0 dB; 1 = +5,8 dB; 0 = silenci). Corba cúbica, com un fader. */
    double guanyDeVolum (double v) noexcept;
    double volumDeDb (double db) noexcept;
    double dbDeVolum (double v) noexcept;
    /** Panorama: 0..1 → -1..+1. */
    inline double panDe (double v) noexcept { return v * 2.0 - 1.0; }
    /** Text per al volum i el panorama («-3,5 dB», «L 40», «C», «R 12»). */
    std::string textVolum (double v);
    std::string textPan (double v);

    // ---- Corbes ----
    /** Com avança un tram (0..1 → 0..1) segons la forma i la tensió del punt on comença. */
    double avanc (double x, int forma, double tensio) noexcept;
    /** Valor de la corba en un compàs (abans del primer punt, el primer; després del darrer, el darrer).
        Sense punts: -1. */
    double valorA (const std::vector<PuntAuto>& punts, double compas) noexcept;

    /** 0.57: la bandera «lliure» d'una línia (per id): quan és true, el motor no la llegeix (s'hi està gravant i mana el
        control). La comparteixen la interfície i el pla; la mateixa línia té sempre el mateix objecte. */
    std::shared_ptr<std::atomic<bool>> bandera (const std::string& idLinia);
    /** Noms dels modes de gravació. */
    const char* nomGrava (int mode);

    /** Una corba ja en segons, per al fil d'àudio (només llegeix: sense memòria ni bloquejos). */
    struct Corba
    {
        struct Punt { double t = 0.0; float v = 0.0f; unsigned char forma = 0; float tensio = 0.0f; };
        std::vector<Punt> p;
        bool buida() const noexcept { return p.empty(); }
        float a (double segons) const noexcept;
    };
    /** La corba d'una línia en segons (amb el mapa de tempo del projecte). */
    Corba corbaEnSegons (const Projecte& pr, const Automatitzacio& a);

    // ---- Edició (fil de la interfície) ----
    Automatitzacio& afegeix (Projecte& p, Pista& t, const std::string& objectiu, const std::string& nom);
    Automatitzacio* busca (Pista& t, const std::string& objectiu);
    /** Afegeix un punt en ordre. Torna'n l'índex. Si ja n'hi ha un al mateix compàs, el substitueix. */
    int posaPunt (Automatitzacio& a, const PuntAuto& pt);
    void esborra (Automatitzacio& a, std::vector<int> indexs);
    /** Mou els punts triats (compassos i valor); no deixa que passin per sobre dels veïns no triats. */
    void mou (Automatitzacio& a, const std::vector<int>& indexs, double dCompas, double dValor);
    /** Enganxa els punts triats a la graella (en compassos: 0,25 = una negra en 4/4). */
    void quantitza (Automatitzacio& a, const std::vector<int>& indexs, double graella);
    /** Copia el tros [de, fins): els punts (relatius a «de») amb un punt a cada vora perquè la corba quedi igual. */
    std::vector<PuntAuto> copia (const Automatitzacio& a, double de, double fins);
    /** Enganxa un tros copiat a «on» (trepitja el que hi havia en aquell espai). */
    void enganxa (Automatitzacio& a, const std::vector<PuntAuto>& tros, double on);
    /** Duplica [de, fins) just després. */
    void duplica (Automatitzacio& a, double de, double fins);
    /** Treu els punts de [de, fins] (amb vores perquè la corba de fora no canviï) i hi posa els nous (dibuix a mà). */
    void substitueix (Automatitzacio& a, double de, double fins, const std::vector<PuntAuto>& nous);
    /** Treu punts que no canvien la corba (dibuix a mà): tolerància en valor normalitzat. */
    std::vector<PuntAuto> simplifica (const std::vector<PuntAuto>& v, double tolerancia = 0.004);
    /** Indexs dels punts dins el rectangle (compassos i valors). */
    std::vector<int> dins (const Automatitzacio& a, double c0, double c1, double v0, double v1);
    /** Desplaça endavant tots els punts a partir de «des» (quan s'insereix espai a la cançó). */
    void desplaca (Pista& t, double des, double compassos);
}
