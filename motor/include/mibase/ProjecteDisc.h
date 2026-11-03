#pragma once

#include "mibase/Projecte.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/** Els projectes al disc: carpeta, fitxer del projecte, diari d'evolució, còpies i noms de les preses.

    Estructura d'un projecte:
        <carpeta triada>/<Nom>/
            <Nom>.mibase        el projecte (JSON, text)
            Evolució.txt        diari del projecte, el més recent a dalt
            Àudios/             les gravacions: «Pista - presa 01.wav»
            Còpies/             còpies automàtiques del fitxer del projecte (les 10 últimes)

    Tots els textos (noms, camins) són UTF-8. Les hores les dona qui crida
    (així el motor no depèn del rellotge i es pot provar). */
namespace mibase
{
    namespace disc
    {
        inline const char* extensio = ".mibase";
        inline const char* carpetaAudios = "Àudios";
        inline const char* carpetaCopies = "Còpies";
        inline const char* fitxerEvolucio = "Evolució.txt";
        constexpr int maxCopies = 10;

        /** Camí a partir d'un text UTF-8 (a Windows, amb accents, cal fer-ho així). */
        std::filesystem::path cami (const std::string& utf8);
        /** Text UTF-8 d'un camí. */
        std::string text (const std::filesystem::path& p);

        struct Validacio
        {
            bool correcte = false;
            std::string missatge;
        };

        /** Comprova que un nom serveixi per a una carpeta de Windows. */
        Validacio validaNom (const std::string& nom);

        /** Treu els caràcters que Windows no accepta en un nom de fitxer. */
        std::string netejaNom (const std::string& nom);

        struct Resultat
        {
            bool correcte = false;
            std::string error;
            std::filesystem::path fitxer;   // el .mibase
        };

        /** Carpeta del projecte (mare/nom). */
        std::filesystem::path carpetaDe (const std::filesystem::path& carpetaMare, const std::string& nom);

        /** Crea la carpeta, el fitxer, Àudios, Còpies i el diari. No trepitja res que ja existeixi. */
        Resultat creaProjecte (const std::filesystem::path& carpetaMare, const Projecte& projecte,
                               const std::string& araIso, const std::string& araText);

        /** Llegeix un .mibase. */
        std::optional<Projecte> llegeix (const std::filesystem::path& fitxer, std::string* error = nullptr);

        /** Desa el projecte. Si el fitxer ja hi era, abans en guarda una còpia a Còpies (com a molt 10).
            araPerNom: data per al nom de la còpia, que s'ha de poder ordenar («2026-11-03 094512»).
            ferCopia: false per no fer còpia aquesta vegada (l'estudi en fa una de tant en tant, no a cada canvi). */
        Resultat desa (const std::filesystem::path& fitxer, Projecte projecte,
                       const std::string& araIso, const std::string& araPerNom, bool ferCopia = true);

        /** Afegeix una línia al diari, a dalt de tot (sota el títol). */
        bool afegeixEvolucio (const std::filesystem::path& carpetaProjecte, const std::string& quan, const std::string& text);

        struct EntradaEvolucio
        {
            std::string quan, text;
        };

        /** Les entrades del diari, la més recent primer. */
        std::vector<EntradaEvolucio> llegeixEvolucio (const std::filesystem::path& carpetaProjecte, int maxim);

        /** Nom per a la propera presa d'una pista: «Guitarra - presa 01.wav», «… 02», etc. */
        std::string nomPresa (const std::filesystem::path& carpetaAudios, const std::string& nomPista);

        /** Quantes preses (.wav) hi ha a Àudios. */
        int comptaPreses (const std::filesystem::path& carpetaProjecte);

        /** És una carpeta de projecte (té un .mibase a dins)? */
        bool esCarpetaDeProjecte (const std::filesystem::path& carpeta);

        /** El .mibase d'una carpeta de projecte (o buit). */
        std::filesystem::path fitxerDe (const std::filesystem::path& carpetaProjecte);

        /** Quants projectes hi ha just a dins d'aquesta carpeta. */
        int comptaProjectes (const std::filesystem::path& carpeta);
    }
}
