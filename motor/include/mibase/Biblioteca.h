#pragma once

#include <filesystem>
#include <string>
#include <vector>

/** 0.27: la biblioteca de samples. Una carpeta al disc amb els fitxers i un índex («biblioteca.json») amb les
    metadades de cada un: nom, categoria, etiquetes, durada, BPM, tonalitat i data. El motor no llegeix àudio:
    la durada i la tonalitat les posa qui afegeix el fitxer. */
namespace mibase::biblioteca
{
    struct Sample
    {
        std::string fitxer;                    // relatiu a la carpeta de la biblioteca («Gol.wav»)
        std::string nom;                       // el que es mostra (per defecte, el nom del fitxer sense extensió)
        std::string categoria = "Altres";      // una de categories()
        std::vector<std::string> etiquetes;
        double durada = 0.0;                   // segons
        double bpm = 0.0;                      // 0: no se sap
        std::string tonalitat;                 // «A3», «C#4»… o buit
        long long data = 0;                    // quan es va afegir (ms des de 1970)
    };

    struct Index
    {
        std::vector<Sample> samples;
        const Sample* busca (const std::string& fitxer) const;
        Sample* busca (const std::string& fitxer);
    };

    /** Bombo, Caixa, Hi-hat, Percussió, Baix, Veu, Sintetitzador, FX, Ambient, Altres. */
    const std::vector<std::string>& categories();
    /** La categoria que suggereix el nom del fitxer («Kick 01.wav» → Bombo; «vocal_gol» → Veu…). */
    std::string categoriaPerNom (const std::string& nomFitxer);
    /** El BPM si el nom el diu («Loop 120bpm», «groove_95_BPM»…); 0 si no. */
    double bpmPerNom (const std::string& nomFitxer);

    inline constexpr const char* nomIndex = "biblioteca.json";
    /** Llegeix l'índex (si no n'hi ha, buit). */
    Index llegeix (const std::filesystem::path& carpeta);
    bool desa (const std::filesystem::path& carpeta, const Index& index);

    /** Els fitxers d'àudio de la carpeta que l'índex no té (per afegir-los), i treu de l'índex els que ja no hi són.
        Torna els camins relatius dels nous. */
    std::vector<std::string> sincronitza (const std::filesystem::path& carpeta, Index& index);
    bool esAudio (const std::filesystem::path& f);

    /** Un nom de fitxer lliure a la carpeta («Gol.wav», «Gol 2.wav»…). */
    std::string nomLliure (const std::filesystem::path& carpeta, const std::string& base, const std::string& extensio);

    enum class Ordre { Nom, Categoria, Durada, Data };
    /** Els índexs dels samples que passen el filtre (text a nom, categoria, etiquetes o tonalitat, sense
        distingir majúscules ni accents; categoria buida = totes), ordenats. */
    std::vector<int> filtra (const Index& index, const std::string& text, const std::string& categoria, Ordre ordre, bool descendent);

    /** Etiquetes a partir d'un text («veu, crit; futbol» → veu, crit, futbol). */
    std::vector<std::string> etiquetesDeText (const std::string& text);
    std::string textDeEtiquetes (const std::vector<std::string>& etiquetes);
}
