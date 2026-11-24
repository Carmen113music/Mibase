#pragma once

#include "MotorEstudi.h"
#include "Projecte.h"

#include <string>
#include <vector>

/** 0.87: treure els acords d'un àudio (per posar-los a la pista d'Acords).
    Es fa un cromagrama (quanta energia té cada nota, de C a B, sumant totes les octaves) per trossos de mig compàs,
    seguint el tempo i el compàs del projecte, i es busca l'acord que s'hi assembla més (majors, menors, 7, m7, maj7, dim).
    El baix ajuda a triar la fonamental. Dos mitjos compassos seguits amb el mateix acord queden en un de sol.
    Va bé amb guitarra, piano o mescles senzilles; amb bateria i veu forta encerta menys (els dubtosos es marquen). */
namespace mibase::acordsAudio
{
    struct AcordTrobat
    {
        double ini = 0.0, fi = 0.0;   // segons de la línia de temps
        std::string nom;              // «Am7», «C»…
        float seguretat = 0.0f;       // 0..1
        bool dubtos = false;          // poc segur: cal revisar-lo
    };

    /** Els acords del clip «c» (tal com sona a la línia de temps) a partir del seu àudio «a» (si el clip té Elastic
        Audio, «a» ha de ser el so estirat i «c» sense punts). Només la part que es veu del clip. */
    std::vector<AcordTrobat> detecta (const Projecte& p, const Clip& c, const AudioPresa& a);
}
