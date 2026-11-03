#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mibase
{
    /** Graella on s'enganxen els clips. */
    enum class Snap { Compas = 0, Temps = 1, Lliure = 2 };

    /** Una gravació (un fitxer d'Àudios). Tots els temps, en segons. */
    struct Presa
    {
        std::string id;          // «s1», «s2»…
        std::string fitxer;      // relatiu a la carpeta del projecte: «Àudios/Guitarra - presa 01.wav»
        std::string nom;         // el que es mostra (no canvia el nom del fitxer)
        double durada = 0.0;
        double primer = -1.0;    // bandera del primer temps, dins el fitxer (negatiu: no en té)
    };

    /** Un tros d'una presa col·locat a la línia de temps (una «finestra»). No toca mai el fitxer.
        Sense presa (presa buida) és una finestra buida, feta amb el llapis. */
    struct Clip
    {
        std::string presa;
        bool mut = false;        // silenciat amb l'eina X
        double pos = 0.0;        // on cau l'inici del FITXER a la línia de temps
        double inici = 0.0;      // finestra visible dins el fitxer (pot sortir de 0..durada: és silenci)
        double fi = 0.0;
        double fadeIn = 0.0, fadeOut = 0.0;
        double guanyDb = 0.0;    // de -24 a 0
        std::uint32_t rgb = 0;   // color propi (0xAARRGGBB); 0 = el de la pista

        double iniciVisible() const noexcept { return pos + inici; }
        double fiVisible() const noexcept    { return pos + fi; }
        double longitud() const noexcept     { return fi - inici; }
    };

    struct Pista
    {
        std::string id;          // «t1», «t2»…
        std::string nom;
        std::string tipus = "audio";   // audio (més endavant: instrument, midi, bateria, acords, bus)
        int canals = 1;                 // 1 mono, 2 estèreo
        int color = 0;                  // índex de la paleta de colors de pista
        std::uint32_t rgb = 0;          // color triat a mà (0xAARRGGBB); 0 = el de la paleta
        std::vector<Clip> clips;
        bool mut = false, solo = false, grava = false;
        double faderDb = 0.0;    // de -60 (silenci) a +6
        double pan = 0.0;        // de -1 (tot a l'esquerra) a +1 (tot a la dreta)
        int entrada = 0;         // primera entrada de la targeta per gravar (0 = entrada 1); estèreo: aquesta i la següent
    };

    /** Bucle per compassos sencers (índexs interns, des de 0): [de, a). */
    struct Bucle
    {
        bool marcat = false, actiu = false;
        int de = 0, a = 0;
    };

    struct Projecte
    {
        std::string nom;
        double tempo = 120.0;
        int compasNum = 4;
        int compasDen = 4;
        std::string plantilla = "buit";
        std::string creat;       // ISO: 2026-11-03T09:45:00
        std::string modificat;

        std::vector<Presa> preses;
        std::vector<Pista> pistes;
        Bucle bucle;
        Snap snap = Snap::Compas;
        bool clic = true;
        double clicVolum = 1.0;  // 0 a 1,5
        int clicSo = 0;          // 0 digital, 1 fusta, 2 analògic, 3 campana
        double masterDb = 0.0;   // volum de la sortida (de -60 a +6)
        int comptador = 0;       // per fer identificadors únics

        /** El JSON tal com s'ha llegit: els camps que aquesta versió no coneix es conserven en desar. */
        std::string jsonOriginal;
    };
}
