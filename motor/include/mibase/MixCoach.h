#pragma once

#include "mibase/MixAssistant.h"
#include <string>
#include <vector>

/** 1.27: el MIX COACH (fase 4 del Mix Assistant): conversa en català amb el que ha mesurat l'anàlisi.

    No és una IA: entén de quina pista (pel nom o per l'instrument: «la veu», «les guitarres»…) i de quin tema
    («no sobresurt», «tancada», «embolicada», «greus», «com ho arreglo», «quines alternatives»…) li parles, i respon
    només amb les dades reals de l'anàlisi (mai fa veure que ha escoltat el que no ha analitzat). Si no ho entén, ho diu
    i proposa preguntes. Recorda de què parlàveu («i el baix?», «i com ho arreglo?»).
    En el futur s'hi podria endollar una IA de fora (opcional) per a les preguntes que no entengui. */
namespace mibase::mix::coach
{
    struct Context
    {
        bool analitzat = false;
        std::vector<PistaMescla> pistes;          // les analitzades (amb l'anàlisi i l'instrument)
        std::vector<Incidencia> incidencies;      // les de les pistes i les de la mescla
        const AnalisiPista* mescla = nullptr;     // la mescla sencera (pot ser nul)
        std::vector<CanviBalanc> balanc;
        std::vector<Evolucio> seguiment;
        bool creacio = false;                     // mode Creació
    };

    /** El que recorda de la conversa (de la sessió). */
    struct Conversa
    {
        std::vector<std::string> pistes;         // de quines pistes parlàvem
        int tema = -1;
        std::vector<std::string> incidencies;    // les que s'han explicat a la darrera resposta
    };

    struct Enllac { std::string text, pista, incidencia; };

    struct Resposta
    {
        std::string text;                           // paràgrafs separats per «\n\n»
        std::vector<Enllac> enllacos;               // «Vés a la pista» / «Prepara-la»…
        std::vector<std::string> suggeriments;      // preguntes per continuar
        std::string accio;                          // «analitza» si cal analitzar
        bool entesa = true;
    };

    Resposta respon (const std::string& pregunta, const Context& ctx, Conversa& conversa);

    /** Les preguntes de mostra (per començar). */
    std::vector<std::string> preguntesInicials();
}
