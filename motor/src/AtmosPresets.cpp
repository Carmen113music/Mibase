#include "mibase/Atmos.h"
#include <cmath>
#include <cstdlib>
#include <memory>
#include <sstream>

/** 0.90: els presets de fàbrica de l'ATMOS, programats un per un (inspirats en famílies de sintes i en estils, sense copiar
    cap preset comercial). Cada un és una llista de canvis «clau=valor» respecte del so inicial (vegeu Atmos.cpp).
    Ones: 0 sinus, 1 triangle, 2 serra, 3 quadrada, 4 pols, 5 soroll, 6 wavetable, 7 granular.
    Taules: 0 Bàsica, 1 PWM, 2 Vocal, 3 Harmònics, 4 Digital, 5 Campana, 6 Sync, 7 Metall, 8 Cor, 9 Cordes, 10 Vidre,
    11 Orgue, 12 Aire, 13 Fosc, 14 Octaves, 15 Formant.
    Fonts de la matriu: 1–3 LFO, 4 env filtre, 5 evolució, 6 velocitat, 7 nota, 8 roda, 9 aftertouch, 10 atzar, 11 env volum.
    Destins: 0 to, 1 tall, 2 ressonància, 3 mescla, 4–7 forma 1–4, 8 formes, 9 FM, 10 detune, 11 pan, 12 volum, 13–16 nivells,
    17 grànuls, 18 textura. LFO: 0 sinus, 1 triangle, 2 serra, 3 quadrada, 4 atzar, 5 atzar suau; tempo 3 = 1/4, 4 = 1/8, 5 = 1/16.
    Reverb: 0 hall, 1 plate, 2 ambient, 3 shimmer, 4 room, 5 chamber. Delay: 2 1/8, 3 1/8., 5 1/4, 6 1/4., 7 1/2.
    Evolució: 0 puja, 1 baixa, 2 puja i baixa, 3 cicle. Textura: 0 blanc, 1 vent, 2 cruixit, 3 respiració.
    El volum («vol») de cada preset està anivellat (mesurat amb un acord de 4 notes). */
namespace mibase::atmos
{
    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c {
            "Warm Pads", "Juno Pads", "Prophet Pads", "Oberheim Pads", "Analog Strings", "Digital Strings", "Soft Clouds",
            "Ambient Dreams", "Dark Atmospheres", "Cinematic Pads", "Shimmer Pads", "Choir Pads", "Slow Motion", "Evolving Textures",
            "Frozen Pads", "Vintage Pads", "Space Pads", "Wide Pads", "Rhythmic Pads", "Indie Pads", "Emotional Pads", "Massive Pads",
            "Noise Textures", "Drone Pads", "Pulsating Pads", "Granular Pads", "Dream Pop", "Post Rock", "Synth Pop",
            "Alternative Rock", "Soundtrack", "Intro Textures", "Crescendos" };
        return c;
    }

    const std::vector<std::string>& tipusPad()
    {
        static const std::vector<std::string> t { "Analog Warm", "Vintage", "Digital", "Cinematic", "Ambient", "Choir-like", "String-like",
                                                  "Evolving", "Dark", "Bright", "Frozen", "Shimmer", "Rhythmic", "Pulsating", "Granular", "Drone" };
        return t;
    }

    namespace
    {
        // Matriu buida (els presets que la programen del tot)
        #define NOMOD " m1q=0 m2q=0 m3q=0 m4q=0.4 m4f=8 m4d=1"
        struct P { const char* cat; const char* nom; const char* desc; const char* canvis; };
        const P taula[] = {
            // ---- Warm Pads ----
            { "Warm Pads", "Calor de capvespre", "Dues serres amples i un sinus una octava avall pel filtre suau: el pad càlid de sempre, per a qualsevol balada.",
              "uni=3 unid=0.28 o4n=0.35 mix=0.5 cut=1800 res=0.12 fenv=0.25 aa=900 ar=2800 chm=0.4 rvm=0.3 der=0.25" NOMOD " m1f=1 m1d=1 m1q=0.08 l1r=0.18 vol=-10.2" },
            { "Warm Pads", "Mantell de llana", "Triangles i serra molt filtrada, atac lent: un llit tou que no molesta mai la veu.",
              "o1w=1 o2w=2 o2n=0.35 o4n=0.4 uni=4 unid=0.22 cut=1100 res=0.05 fenv=0.15 aa=1400 ad=3000 as=0.9 ar=3500 chm=0.5 chp=0.6 rvm=0.32 rvs=0.55" NOMOD " m1f=5 m1d=1 m1q=0.25 evt=10 vol=-8.0" },
            { "Warm Pads", "Analògic de casa", "Pols amb PWM lenta i deriva: viu, imperfecte, com un sinte vell ben cuidat.",
              "o1w=4 o2w=4 o1sh=0.3 o2sh=0.6 o2f=9 uni=2 unid=0.2 cut=2200 res=0.18 fenv=0.2 der=0.5 derv=0.4 aa=700 ar=2400 chm=0.3 rvm=0.28" NOMOD " m1f=1 m1d=4 m1q=0.35 m2f=2 m2d=5 m2q=0.35 l1r=0.35 l2r=0.22 l2ph=1 vol=-9.7" },
            { "Warm Pads", "Ambre", "Taula «Fosc» i serra amb saturació al filtre: rodó, dens i daurat.",
              "o1w=6 o1t=13 o1sh=0.5 o2w=2 o2n=0.4 o2f=6 uni=3 cut=2000 res=0.2 fdrv=0.35 fenv=0.2 aa=1000 ar=3000 mwm=0.4 chm=0.25 rvm=0.3" NOMOD " m1f=5 m1d=4 m1q=0.4 evt=12 vol=-13.0" },
            { "Warm Pads", "Pell de vellut", "Sinus i triangle amb unison ample: suau com una mà, perfecte sota una guitarra acústica.",
              "o1w=0 o2w=1 o2o=1 o2n=0.35 o4w=0 o4n=0.25 uni=5 unid=0.35 unia=0.9 cut=5000 res=0 aa=1200 ar=3000 chm=0.35 rvm=0.35 rvs=0.65" NOMOD " m1f=1 m1d=12 m1q=0.06 l1r=0.3 vol=-9.5" },

            // ---- Juno Pads ----
            { "Juno Pads", "Polisintètic 106", "Un sol oscil·lador de pols amb sub i el chorus d'ensemble ben obert: el pad dels 80 per excel·lència.",
              "o1w=4 o1sh=0.4 o2n=0 o4w=3 o4o=-1 o4n=0.35 mix=0.5 uni=1 cut=2600 res=0.22 fenv=0.25 aa=350 ar=1500 chm=0.85 chr=0.55 chp=0.7 rvm=0.18 der=0.15" NOMOD " m1f=1 m1d=4 m1q=0.3 l1r=0.6 vol=-9.0" },
            { "Juno Pads", "Ensemble de corda", "Serra i chorus a fons, amb filtre que obre a poc a poc: les cordes de plàstic més boniques.",
              "o2n=0 uni=1 cut=1600 res=0.1 fenv=0.4 fa=1500 fd=3000 fs=0.7 aa=500 ar=1800 chm=0.9 chr=0.7 chp=0.8 rvm=0.22" NOMOD " vol=-3.8" },
            { "Juno Pads", "Pols de seda", "PWM ràpida i chorus lent: brillant, ple de moviment intern.",
              "o1w=4 o1sh=0.2 o2n=0 uni=1 cut=3800 res=0.15 aa=250 ar=1300 chm=0.7 chr=0.35 rvm=0.2" NOMOD " m1f=1 m1d=4 m1q=0.6 l1r=1.6 vol=-7.2" },
            { "Juno Pads", "Juno fosc", "Pols estret pel filtre tancat amb molt sub: per a versos íntims i synthwave lent.",
              "o1w=4 o1sh=0.6 o2n=0 o4w=3 o4o=-1 o4n=0.5 mix=0.5 uni=1 cut=900 res=0.3 fenv=0.3 aa=400 ar=1600 chm=0.75 rvm=0.25 dlm=0.12 dlt=6" NOMOD " m1f=1 m1d=4 m1q=0.3 l1r=0.4 vol=-9.6" },
            { "Juno Pads", "Arc de Sant Martí", "Pols i serra amb chorus a dos ritmes i delay: dolç i nostàlgic.",
              "o1w=4 o1sh=0.35 o2w=2 o2n=0.35 o2f=4 uni=1 cut=3000 res=0.18 aa=300 ar=2000 chm=0.8 chp=0.65 phm=0.2 phr=0.08 rvm=0.25 dlm=0.18 dlt=3" NOMOD " m1f=1 m1d=4 m1q=0.35 l1r=0.5 vol=-5.8" },

            // ---- Prophet Pads ----
            { "Prophet Pads", "Cinc veus", "Serra i pols desafinades, filtre de 24 dB amb envolupant: el pad polifònic americà, ple i amb cos.",
              "o2w=4 o2sh=0.3 o2f=8 uni=2 unid=0.18 fp=1 cut=1500 res=0.25 fenv=0.45 fa=600 fd=2500 fs=0.45 aa=500 ar=2200 der=0.35 chm=0.2 rvm=0.25" NOMOD " vol=-10.5" },
            { "Prophet Pads", "Poly mod", "L'oscil·lador 2 modula l'1 amb FM lenta: brillantor metàl·lica que respira.",
              "o2w=1 o2o=1 o2n=0.2 fm21=0.25 uni=2 cut=2600 res=0.2 fenv=0.25 aa=600 ar=2500 rvm=0.3 chm=0.2" NOMOD " m1f=2 m1d=9 m1q=0.25 l2r=0.12 l2w=1 vol=-7.8" },
            { "Prophet Pads", "Sync suau", "Sync simulat amb la taula «Sync» que es mou amb l'evolució: un pad que es va encenent.",
              "o1w=6 o1t=6 o1sh=0.1 o2n=0.3 o2f=5 uni=2 cut=3000 res=0.15 aa=700 ar=2500 evt=9 rvm=0.3 chm=0.2" NOMOD " m1f=5 m1d=4 m1q=0.55 vol=-7.3" },
            { "Prophet Pads", "Metall vell", "Dues quadrades amb filtre ressonant i deriva gran: un analògic de 1980 ben calent.",
              "o1w=3 o2w=3 o2f=11 fp=1 cut=1300 res=0.35 fdrv=0.3 fenv=0.35 der=0.6 aa=400 ar=1800 rvm=0.25 chm=0.15" NOMOD " m1f=1 m1d=1 m1q=0.1 l1r=0.2 vol=-17.7" },
            { "Prophet Pads", "Polifònic obert", "Filtre obert i atac mitjà amb delay: per a progressions de pop d'estadi.",
              "o2w=2 o2f=-7 uni=3 unid=0.25 cut=6000 res=0.1 aa=300 ar=1800 dlm=0.2 dlt=3 rvm=0.3 width=1.3" NOMOD " vol=-9.2" },

            // ---- Oberheim Pads ----
            { "Oberheim Pads", "Mur de serres", "Quatre serres desafinades pel filtre de 12 dB: gros, brillant i amb pit.",
              "o3w=2 o3t=0 o3f=-11 o3n=0.5 o4w=2 o4o=-1 o4n=0.45 mix=0.5 uni=2 unid=0.25 cut=2400 res=0.15 fenv=0.35 aa=250 ar=1600 der=0.45 chm=0.2 rvm=0.25" NOMOD " vol=-11.6" },
            { "Oberheim Pads", "Brass pad", "Atac de metall que s'obre i es queda: per a acords que han de manar.",
              "o2f=12 uni=2 cut=900 res=0.15 fenv=0.65 fa=350 fd=1500 fs=0.5 aa=180 ar=1500 der=0.4 chm=0.15 rvm=0.22" NOMOD " vol=-9.9" },
            { "Oberheim Pads", "Pols bessons", "Dos pols amb PWM a velocitats diferents i molta obertura estèreo.",
              "o1w=4 o2w=4 o1p=-0.5 o2p=0.5 o2f=7 uni=1 cut=2800 res=0.12 aa=400 ar=2000 der=0.35 chm=0.25 rvm=0.3" NOMOD " m1f=1 m1d=4 m1q=0.45 m2f=2 m2d=5 m2q=0.45 l1r=0.3 l2r=0.41 l2w=0 vol=-11.4" },
            { "Oberheim Pads", "Pit de bronze", "Serres i quadrada amb saturació i calidesa: dens, per a rock dels 80.",
              "o3w=3 o3n=0.35 o3o=-1 uni=2 cut=1800 res=0.2 fdrv=0.5 mwm=0.35 fenv=0.3 aa=300 ar=1700 rvm=0.2" NOMOD " vol=-13.8" },
            { "Oberheim Pads", "Cortina", "Atac molt lent i filtre que respira amb l'LFO 2: s'obre com un teló.",
              "uni=3 unid=0.3 cut=1400 res=0.18 aa=2500 ar=3500 fa=3000 fenv=0.4 rvm=0.35 chm=0.3" NOMOD " m1f=2 m1d=1 m1q=0.25 l2r=0.09 vol=-8.2" },

            // ---- Analog Strings ----
            { "Analog Strings", "Cordes de cinta", "Serres amb chorus d'ensemble i atac de cordes: el string machine de la cançó d'autor dels 70.",
              "o2f=5 uni=3 unid=0.2 cut=3200 res=0.08 fenv=0.1 aa=450 ar=1500 chm=0.95 chr=0.9 chp=0.6 rvm=0.3 mbr=0.2" NOMOD " m1f=1 m1d=0 m1q=0.004 l1r=5.5 l1in=600 vol=-9.0" },
            { "Analog Strings", "Violes analògiques", "Taula de cordes amb vibrato que entra a poc a poc: més fosques i càlides.",
              "o1w=6 o1t=9 o1sh=0.35 o2n=0.3 uni=3 cut=2400 res=0.1 aa=600 ar=1800 chm=0.6 rvm=0.3" NOMOD " m1f=1 m1d=0 m1q=0.005 l1r=5 l1in=900 m2f=5 m2d=4 m2q=0.3 evt=6 vol=-5.0" },
            { "Analog Strings", "Secció de cambra", "Cordes amb molts unisons i reverb de cambra: per a balades i arranjaments.",
              "o1w=6 o1t=9 o1sh=0.55 o2w=2 o2n=0.3 uni=5 unid=0.2 unia=0.85 cut=4200 res=0.05 aa=700 ar=2000 rvt=5 rvm=0.35 chm=0.4" NOMOD " m1f=1 m1d=0 m1q=0.004 l1r=5.2 l1in=700 vol=-4.6" },
            { "Analog Strings", "Cordes pizzicato", "Atac curt i caiguda: cordes punxades per a riffs suaus.",
              "o1w=6 o1t=9 o1sh=0.6 o2n=0.3 uni=2 cut=2500 res=0.12 fenv=0.5 fa=2 fd=250 fs=0 aa=3 ad=450 as=0 ar=500 chm=0.5 rvm=0.3" NOMOD " vol=4.4" },
            { "Analog Strings", "Cordes llunyanes", "Molt filtrades i molt lluny: un fons de cordes que només s'intueix.",
              "o1w=6 o1t=9 o1sh=0.2 uni=4 cut=1200 res=0.05 aa=1500 ar=3500 chm=0.7 rvm=0.5 rvs=0.8 mdp=0.4 mbr=-0.3" NOMOD " vol=-8.1" },

            // ---- Digital Strings ----
            { "Digital Strings", "Cordes de vidre", "Taula de cordes i de vidre amb FM suau: les cordes digitals dels 90, nítides.",
              "o1w=6 o1t=9 o1sh=0.8 o2w=6 o2t=10 o2sh=0.5 o2o=1 o2n=0.25 fm21=0.12 uni=3 cut=7000 res=0.05 aa=300 ar=1600 rvm=0.3 dlm=0.12" NOMOD " vol=-5.4" },
            { "Digital Strings", "Ensemble FM", "Dues parelles FM (2→1 i 4→3) amb mescla que es mou: cordes brillants que canvien de color.",
              "o1w=0 o2w=0 o2o=1 o2n=0.15 fm21=0.35 o3w=0 o3n=0.5 o4w=0 o4o=1 o4s=7 o4n=0.1 fm43=0.3 uni=3 cut=8000 aa=400 ar=1800 rvm=0.3 chm=0.4" NOMOD " m1f=2 m1d=3 m1q=0.35 l2r=0.1 vol=-8.0" },
            { "Digital Strings", "Cordes planes", "Taula harmònica fixa i neta, sense deriva: per a pop modern i electrònica.",
              "o1w=6 o1t=3 o1sh=0.5 o2n=0 der=0 lliure=0 uni=4 unid=0.18 cut=9000 aa=200 ar=1200 chm=0.2 rvm=0.25" NOMOD " vol=-3.5" },
            { "Digital Strings", "Seda digital", "Formant que es passeja pels harmònics: cordes amb veu.",
              "o1w=6 o1t=15 o1sh=0.4 o2w=6 o2t=9 o2sh=0.6 o2n=0.4 uni=3 cut=6000 aa=500 ar=2000 rvm=0.3 chm=0.3" NOMOD " m1f=2 m1d=4 m1q=0.35 l2r=0.15 l2w=5 vol=-3.5" },
            { "Digital Strings", "Cordes de cristall", "Octaves i vidre amb delay: agudes i brillants, per sobre de tot.",
              "o1w=6 o1t=14 o1sh=0.5 o2w=6 o2t=10 o2o=1 o2n=0.3 uni=3 cut=10000 aa=350 ar=2200 dlm=0.25 dlt=3 rvm=0.35 mbr=0.3" NOMOD " vol=-7.6" },

            // ---- Soft Clouds ----
            { "Soft Clouds", "Núvol de cotó", "Sinus i triangles amb molts unisons i reverb gran: suau, sense cap aresta.",
              "o1w=0 o2w=1 o2o=1 o2n=0.3 uni=5 unid=0.4 unia=1 cut=4000 res=0 aa=1800 ar=4000 rvm=0.5 rvs=0.75 chm=0.5" NOMOD " m1f=1 m1d=12 m1q=0.08 l1r=0.15 vol=-7.6" },
            { "Soft Clouds", "Boira matinal", "Taula «Aire» amb textura de respiració: un buf amb nota.",
              "o1w=6 o1t=12 o1sh=0.4 o2w=0 o2n=0.4 uni=3 cut=5000 aa=2000 ar=4000 nz=0.25 nzt=3 nzc=0.6 rvm=0.45 rvs=0.7" NOMOD " m1f=1 m1d=4 m1q=0.3 l1r=0.07 l1w=5 vol=-4.8" },
            { "Soft Clouds", "Plomes", "Granular suau de la taula «Cor»: un núvol de petites veus.",
              "o1w=7 o1t=8 o1sh=0.35 o1n=0.9 o2n=0 grd=30 grm=180 grs=0.4 grt=0.15 cut=6000 aa=1500 ar=3500 rvm=0.45 chm=0.3" NOMOD " vol=-6.4" },
            { "Soft Clouds", "Llit de núvols", "Un acord tou amb atac i alliberament molt llargs: per a fons de vídeo i meditació.",
              "o1w=1 o2w=0 o2o=1 o2n=0.4 o4w=0 o4o=-1 o4n=0.3 uni=4 unid=0.3 cut=3000 aa=3000 ar=6000 rvm=0.55 rvs=0.85 chm=0.45" NOMOD " vol=-8.3" },
            { "Soft Clouds", "Cel blanc", "Octaves amb shimmer suau: lluminós, alt i tranquil.",
              "o1w=6 o1t=14 o1sh=0.3 o2w=0 o2n=0.3 uni=3 cut=8000 aa=1500 ar=4000 rvt=3 rvm=0.4 rvsh=0.25 rvs=0.7" NOMOD " vol=-7.6" },

            // ---- Ambient Dreams ----
            { "Ambient Dreams", "Somni lent", "Wavetable vocal que es mou molt a poc a poc, reverb ambient i delay llarg: per a escoltar amb els ulls tancats.",
              "o1w=6 o1t=2 o1sh=0.2 o2w=6 o2t=8 o2sh=0.5 o2n=0.4 uni=3 cut=4500 aa=2000 ar=5000 rvt=2 rvm=0.5 rvs=0.8 dlm=0.25 dlt=6 dlf=0.55" NOMOD " m1f=1 m1d=4 m1q=0.5 l1r=0.04 l1w=5 m2f=2 m2d=5 m2q=0.4 l2r=0.06 l2w=5 l2ph=1 vol=-4.3" },
            { "Ambient Dreams", "Aigua quieta", "Sinus amb FM molt lleu i delay ping-pong: gotes que s'apaguen.",
              "o1w=0 o2w=0 o2o=2 o2n=0.08 fm21=0.2 uni=2 cut=7000 aa=900 ar=4500 dlm=0.35 dlt=3 dlf=0.6 rvt=2 rvm=0.45" NOMOD " m1f=5 m1d=9 m1q=-0.15 evt=6 vol=-8.6" },
            { "Ambient Dreams", "Horitzó", "Dues parelles que es creuen amb l'evolució (cicle): el timbre va i ve durant minuts.",
              "o1w=6 o1t=12 o2w=0 o2n=0.4 o3w=6 o3t=9 o3sh=0.6 o3n=0.6 o4w=1 o4o=1 o4n=0.25 mix=0.2 uni=3 aa=1800 ar=4500 evt=14 evf=3 rvt=2 rvm=0.45 rvs=0.75" NOMOD " m1f=5 m1d=3 m1q=0.6 vol=-8.5" },
            { "Ambient Dreams", "Llum de lluna", "Taula de vidre fosca amb textura de vent: fred i bonic.",
              "o1w=6 o1t=10 o1sh=0.3 o2w=6 o2t=13 o2n=0.5 uni=3 cut=3500 aa=2000 ar=5000 nz=0.2 nzt=1 nzm=0.3 rvt=2 rvm=0.5 rvs=0.8" NOMOD " m1f=1 m1d=4 m1q=0.3 l1r=0.05 l1w=5 vol=-6.9" },
            { "Ambient Dreams", "Rellotge aturat", "Sinus i campana amb molta reverb i delay de corxera amb punt: hipnòtic.",
              "o1w=0 o2w=6 o2t=5 o2o=1 o2n=0.25 uni=2 cut=6000 aa=600 ar=4000 dlm=0.35 dlt=3 dlf=0.65 rvt=2 rvm=0.45" NOMOD " m1f=10 m1d=4 m1q=0.4 vol=-8.3" },

            // ---- Dark Atmospheres ----
            { "Dark Atmospheres", "Soterrani", "Taula «Fosc» una octava avall, filtre tancat i vent greu: inquietant i profund.",
              "o1w=6 o1t=13 o1o=-1 o1sh=0.4 o2w=2 o2o=-1 o2n=0.25 o2f=-9 uni=3 cut=600 res=0.25 aa=2500 ar=5000 nz=0.25 nzt=1 nzc=0.85 nzm=0.4 rvm=0.4 rvs=0.75 mbr=-0.5" NOMOD " m1f=1 m1d=1 m1q=0.2 l1r=0.05 l1w=5 vol=-6.3" },
            { "Dark Atmospheres", "Ombres", "Metall FM greu que es mou a l'atzar: per a tensió de cinema i intros fosques.",
              "o1w=6 o1t=7 o1sh=0.2 o1o=-1 o2w=0 o2o=-1 o2s=1 o2n=0.15 fm21=0.3 uni=2 cut=1200 res=0.3 aa=3000 ar=5000 rvm=0.45 rvs=0.8" NOMOD " m1f=2 m1d=9 m1q=0.3 l2w=5 l2r=0.08 m2f=1 m2d=4 m2q=0.3 l1w=5 l1r=0.05 vol=-7.6" },
            { "Dark Atmospheres", "Cova", "Granular fosc amb grànuls llargs i reverb de sala: humit i ple d'eco.",
              "o1w=7 o1t=13 o1sh=0.5 o1n=0.9 o1o=-1 o2n=0 grd=12 grm=350 grs=0.6 grt=0.3 cut=1500 aa=2500 ar=5000 rvt=0 rvm=0.55 rvs=0.85 dlm=0.2 dlt=7" NOMOD " vol=-4.9" },
            { "Dark Atmospheres", "Tempesta llunyana", "Drone dissonant amb soroll de vent fort: amenaça que s'apropa.",
              "o1w=2 o1o=-1 o2w=2 o2o=-1 o2s=1 o2n=0.4 uni=4 unid=0.5 cut=800 res=0.35 aa=3000 ar=5000 nz=0.45 nzt=1 nzm=0.7 rvm=0.45" NOMOD " m1f=1 m1d=1 m1q=0.35 l1r=0.03 l1w=5 vol=-7.7" },
            { "Dark Atmospheres", "Fred", "Vidre greu i passa-banda: buit, metàl·lic i hostil.",
              "o1w=6 o1t=10 o1sh=0.7 o1o=-1 o2w=6 o2t=7 o2n=0.4 o2f=-25 ft=2 cut=900 res=0.45 aa=2000 ar=4000 rvm=0.5 rvs=0.8" NOMOD " m1f=1 m1d=1 m1q=0.4 l1r=0.04 l1w=5 vol=2.6" },

            // ---- Cinematic Pads ----
            { "Cinematic Pads", "Pantalla gran", "Quatre oscil·ladors (cordes, cor, octava i sub) amb filtre que s'obre en 10 segons: l'emoció del tràiler.",
              "o1w=6 o1t=9 o1sh=0.4 o2w=6 o2t=8 o2sh=0.4 o2n=0.5 o3w=2 o3o=1 o3n=0.3 o4w=0 o4o=-1 o4n=0.45 mix=0.5 uni=3 cut=900 fenv=0.6 fa=9000 fd=8000 fs=0.9 aa=1500 ar=4000 rvt=0 rvm=0.45 rvs=0.8 width=1.4" NOMOD " vol=-9.3" },
            { "Cinematic Pads", "Epopeia", "Serres massives i octava amunt amb delay de negra: gran, heroic.",
              "o3w=2 o3o=1 o3n=0.4 o4w=2 o4o=-1 o4n=0.4 uni=4 unid=0.3 cut=2500 fenv=0.3 aa=800 ar=3000 dlm=0.18 dlt=5 rvm=0.4 rvs=0.75 width=1.4" NOMOD " m1f=5 m1d=1 m1q=0.4 evt=8 vol=-10.8" },
            { "Cinematic Pads", "Suspens", "Clúster fosc que es tanca a poc a poc (evolució que baixa): tensió de thriller.",
              "o1w=6 o1t=7 o1sh=0.3 o2s=1 o2n=0.4 o4w=0 o4o=-1 o4n=0.4 uni=3 cut=3000 res=0.25 aa=2500 ar=4000 evt=12 evf=1 rvm=0.45" NOMOD " m1f=5 m1d=1 m1q=0.6 m2f=1 m2d=12 m2q=0.15 l1r=4 l1w=0 vol=-10.3" },
            { "Cinematic Pads", "Alba", "Pad que neix fosc i acaba brillant amb shimmer: per a finals de pel·lícula.",
              "o1w=6 o1t=3 o1sh=0.1 o2w=6 o2t=14 o2n=0.4 uni=3 cut=1200 aa=2500 ar=5000 evt=15 rvt=3 rvm=0.4 rvsh=0.3" NOMOD " m1f=5 m1d=1 m1q=0.8 m2f=5 m2d=8 m2q=0.6 vol=-6.6" },
            { "Cinematic Pads", "Paisatge", "Cordes, aire i vent suau amb amplada gran: la càmera que s'enlaira.",
              "o1w=6 o1t=9 o1sh=0.5 o2w=6 o2t=12 o2n=0.4 uni=4 cut=4500 aa=1800 ar=4500 nz=0.12 nzt=1 rvm=0.45 rvs=0.75 width=1.6" NOMOD " m1f=1 m1d=4 m1q=0.3 l1r=0.05 l1w=5 vol=-7.6" },

            // ---- Shimmer Pads ----
            { "Shimmer Pads", "Catedral de llum", "Reverb shimmer a fons sobre un pad de sinus i octaves: cada nota s'enfila cap al cel.",
              "o1w=0 o2w=6 o2t=14 o2n=0.4 uni=3 cut=6000 aa=1200 ar=5000 rvt=3 rvm=0.55 rvsh=0.6 rvs=0.85" NOMOD " vol=-10.6" },
            { "Shimmer Pads", "Àngels", "Cor wavetable amb shimmer i chorus: etèri, per a ponts emocionals.",
              "o1w=6 o1t=8 o1sh=0.3 o2w=6 o2t=8 o2sh=0.6 o2n=0.5 uni=3 cut=5000 aa=1500 ar=4500 chm=0.4 rvt=3 rvm=0.5 rvsh=0.5" NOMOD " m1f=2 m1d=4 m1q=0.3 l2r=0.06 vol=-6.6" },
            { "Shimmer Pads", "Cristalls", "Vidre i campana amb shimmer i delay: espurnes que pugen.",
              "o1w=6 o1t=10 o1sh=0.5 o2w=6 o2t=5 o2o=1 o2n=0.3 uni=2 cut=9000 aa=300 ar=4000 dlm=0.25 dlt=3 rvt=3 rvm=0.5 rvsh=0.55" NOMOD " vol=-3.6" },
            { "Shimmer Pads", "Post-rock shimmer", "Serres filtrades i shimmer llarg: la guitarra amb efectes, en pad.",
              "uni=3 unid=0.25 cut=2500 aa=800 ar=4000 dlm=0.3 dlt=6 dlf=0.55 rvt=3 rvm=0.55 rvsh=0.45 rvs=0.9" NOMOD " vol=-9.4" },
            { "Shimmer Pads", "Halo", "Granular d'octaves amb shimmer: un núvol que brilla.",
              "o1w=7 o1t=14 o1sh=0.4 o1n=0.9 o2w=0 o2n=0.3 grd=25 grm=200 grs=0.5 gro=1 cut=8000 aa=1500 ar=4500 rvt=3 rvm=0.5 rvsh=0.4" NOMOD " vol=-8.5" },

            // ---- Choir Pads ----
            { "Choir Pads", "Cor de l'església", "Wavetable de cor amb vocals que canvien lentament: «aah» que es torna «ooh».",
              "o1w=6 o1t=8 o1sh=0.1 o2w=6 o2t=8 o2sh=0.15 o2f=9 o2n=0.6 uni=4 unid=0.22 cut=5000 aa=800 ar=2500 rvt=0 rvm=0.4 chm=0.35" NOMOD " m1f=5 m1d=8 m1q=0.6 evt=6 evf=2 m2f=1 m2d=0 m2q=0.004 l1r=5 l1in=900 vol=-6.3" },
            { "Choir Pads", "Veus de vidre", "Taula vocal i vidre: un cor digital, net i fred.",
              "o1w=6 o1t=2 o1sh=0.3 o2w=6 o2t=10 o2n=0.35 uni=3 cut=7000 aa=600 ar=2500 rvm=0.4 chm=0.3" NOMOD " m1f=2 m1d=4 m1q=0.4 l2r=0.1 vol=-6.0" },
            { "Choir Pads", "Monjos", "Cor greu i fosc, formant baix i reverb de sala gran.",
              "o1w=6 o1t=8 o1sh=0.85 o1o=-1 o2w=6 o2t=15 o2sh=0.15 o2n=0.4 uni=4 cut=2200 aa=1200 ar=3500 rvm=0.5 rvs=0.85" NOMOD " m1f=1 m1d=0 m1q=0.003 l1r=4.5 l1in=1500 vol=-5.8" },
            { "Choir Pads", "Cor d'infants", "Cor agut, formant obert i respiració suau.",
              "o1w=6 o1t=8 o1sh=0.05 o1o=1 o2w=6 o2t=2 o2sh=0.2 o2n=0.35 uni=3 cut=6500 aa=700 ar=2500 nz=0.08 nzt=3 rvm=0.45" NOMOD " m1f=1 m1d=0 m1q=0.004 l1r=5.5 l1in=600 vol=-4.0" },
            { "Choir Pads", "Fantasmes", "Grànuls de cor escampats amb shimmer baix: veus que no hi són.",
              "o1w=7 o1t=8 o1sh=0.3 o1n=0.9 o2w=6 o2t=8 o2n=0.25 grd=18 grm=250 grs=0.7 grt=0.25 cut=4000 aa=2000 ar=5000 rvt=2 rvm=0.55 rvsh=0.2" NOMOD " vol=-7.3" },

            // ---- Slow Motion ----
            { "Slow Motion", "Càmera lenta", "Atac de 4 segons i LFO lentíssims: tot passa a poc a poc.",
              "uni=3 cut=2200 aa=4000 ar=6000 fa=5000 fenv=0.3 rvm=0.45 rvs=0.75" NOMOD " m1f=1 m1d=1 m1q=0.2 l1r=0.03 m2f=2 m2d=12 m2q=0.15 l2r=0.05 vol=-9.1" },
            { "Slow Motion", "Respiració", "El volum puja i baixa com una respiració (LFO lent).",
              "o1w=6 o1t=12 o2w=0 o2n=0.4 uni=3 cut=4000 aa=1500 ar=3500 rvm=0.4" NOMOD " m1f=1 m1d=12 m1q=0.35 l1r=0.12 m2f=1 m2d=1 m2q=0.2 vol=-8.4" },
            { "Slow Motion", "Marea", "Filtre i pan que van i vénen com les onades, cada nota al seu ritme.",
              "o2f=9 uni=3 cut=1600 res=0.2 aa=1200 ar=4000 rvm=0.4 dlm=0.15" NOMOD " m1f=1 m1d=1 m1q=0.4 l1r=0.08 l1ph=1 m2f=2 m2d=11 m2q=0.6 l2r=0.06 l2ph=1 vol=-8.7" },
            { "Slow Motion", "Gel que es fon", "Evolució de 20 s que passa de fosc a brillant i de B a A.",
              "o1w=6 o1t=10 o2n=0.4 o3w=6 o3t=13 o3n=0.6 mix=0.8 uni=3 aa=2500 ar=5000 evt=20 rvm=0.45" NOMOD " m1f=5 m1d=3 m1q=-0.7 m2f=5 m2d=1 m2q=0.5 vol=-7.3" },
            { "Slow Motion", "Somnolència", "Desafinació que va i ve molt lentament: com quan t'adorms.",
              "o2f=0 uni=4 unid=0.15 cut=2500 aa=2500 ar=5000 der=0.5 derv=0.1 rvm=0.45" NOMOD " m1f=1 m1d=10 m1q=0.4 l1r=0.04 vol=-8.5" },

            // ---- Evolving Textures ----
            { "Evolving Textures", "Metamorfosi", "Quatre taules diferents que es van creuant (cicle de 16 s) amb l'LFO 3 per veu: no sona mai igual.",
              "o1w=6 o1t=9 o2w=6 o2t=15 o2n=0.5 o3w=6 o3t=3 o3n=0.5 o4w=6 o4t=8 o4n=0.4 uni=2 aa=1500 ar=4000 evt=16 evf=3 rvm=0.4" NOMOD " m1f=5 m1d=3 m1q=0.9 m2f=3 m2d=8 m2q=0.5 l3r=0.07 l3ph=1 l3w=5 vol=-9.6" },
            { "Evolving Textures", "Caleidoscopi", "Les formes de les wavetables es mouen a l'atzar suau, cada veu pel seu compte.",
              "o1w=6 o1t=4 o2w=6 o2t=4 o2o=1 o2n=0.4 uni=2 cut=6000 aa=1000 ar=3500 rvm=0.4 dlm=0.2" NOMOD " m1f=1 m1d=4 m1q=0.6 l1w=5 l1r=0.2 l1ph=1 m2f=2 m2d=5 m2q=0.6 l2w=5 l2r=0.15 l2ph=1 vol=-4.3" },
            { "Evolving Textures", "Bosc viu", "Formant que camina, vent i grànuls: un ambient orgànic.",
              "o1w=6 o1t=15 o2w=7 o2t=12 o2n=0.5 grd=15 grm=200 cut=5000 aa=1500 ar=4000 nz=0.15 nzt=1 rvm=0.45" NOMOD " m1f=1 m1d=4 m1q=0.6 l1r=0.06 l1w=5 vol=-4.9" },
            { "Evolving Textures", "Engranatges", "FM que s'obre i es tanca amb l'evolució (puja i baixa): mecànic però suau.",
              "o1w=0 o2w=0 o2s=7 o2o=1 o2n=0.1 fm21=0.1 uni=2 cut=7000 aa=800 ar=3000 evt=8 evf=2 rvm=0.35" NOMOD " m1f=5 m1d=9 m1q=0.6 vol=-9.5" },
            { "Evolving Textures", "Marees de color", "Mescla A/B amb un LFO triangular lent: dos pads que es relleven.",
              "o1w=6 o1t=8 o2w=6 o2t=8 o2sh=0.7 o2n=0.5 o3w=6 o3t=11 o3n=0.6 o4w=6 o4t=14 o4n=0.3 uni=3 aa=1200 ar=3500 rvm=0.4" NOMOD " m1f=1 m1d=3 m1q=0.6 l1w=1 l1r=0.05 vol=-7.0" },

            // ---- Frozen Pads ----
            { "Frozen Pads", "Glaç etern", "Reverb de cua llarguíssima i so de vidre fred. Prova el FREEZE (efectes): el so es queda congelat.",
              "o1w=6 o1t=10 o1sh=0.4 o2w=0 o2o=1 o2n=0.3 uni=3 cut=7000 aa=600 ar=6000 rvt=0 rvm=0.6 rvs=1 rvto=0.7" NOMOD " vol=-7.2" },
            { "Frozen Pads", "Llac gelat", "Sinus i aire amb delay llarg: quiet i infinit.",
              "o1w=0 o2w=6 o2t=12 o2n=0.4 uni=2 cut=6000 aa=900 ar=6000 dlm=0.3 dlt=7 dlf=0.7 rvt=2 rvm=0.55 rvs=0.95" NOMOD " vol=-8.7" },
            { "Frozen Pads", "Estalactites", "Campana granular glaçada: gotes cristal·lines dins d'una reverb enorme.",
              "o1w=7 o1t=5 o1sh=0.5 o1n=1 o2w=0 o2o=1 o2n=0.2 grd=18 grm=300 grs=0.4 gro=2 cut=9000 aa=300 ar=6000 rvm=0.6 rvs=1" NOMOD " vol=0.2" },
            { "Frozen Pads", "Hivern", "Cordes fredes, vent i cua molt llarga: paisatge nevat.",
              "o1w=6 o1t=9 o1sh=0.7 o2n=0.3 uni=4 cut=5000 aa=1500 ar=6000 nz=0.15 nzt=1 nzc=0.3 rvm=0.55 rvs=0.95 rvto=0.8" NOMOD " vol=-5.8" },
            { "Frozen Pads", "Temps aturat", "Evolució molt lenta i reverb congelable: per sostenir un acord tota una secció.",
              "o1w=6 o1t=14 o2w=6 o2t=8 o2n=0.5 uni=3 aa=2000 ar=6000 evt=30 rvt=0 rvm=0.55 rvs=1" NOMOD " m1f=5 m1d=8 m1q=0.6 vol=-9.4" },

            // ---- Vintage Pads ----
            { "Vintage Pads", "Mellotró", "Taula de cor i cordes amb una mica de cinta (deriva i soroll fosc): l'àlbum dels 70.",
              "o1w=6 o1t=9 o1sh=0.4 o2w=6 o2t=8 o2n=0.4 uni=1 cut=3500 aa=150 ar=900 der=0.5 derv=0.6 nz=0.05 nzc=0.8 chm=0.2 rvm=0.25 mbr=-0.2" NOMOD " m1f=1 m1d=0 m1q=0.006 l1r=0.6 l1w=5 vol=-5.3" },
            { "Vintage Pads", "Solina", "Serres amb ensemble i alliberament curt: el string machine de la disco i el rock simfònic.",
              "o2n=0 uni=2 unid=0.1 cut=4000 aa=200 ar=1200 chm=1 chr=1.2 chp=0.4 rvm=0.2" NOMOD " vol=-5.5" },
            { "Vintage Pads", "Vinil", "Pad càlid amb cruixit de disc: per a lo-fi i intros nostàlgiques.",
              "o1w=1 o2w=2 o2n=0.35 uni=3 cut=1800 aa=600 ar=2500 nz=0.35 nzt=2 nzm=0.4 der=0.4 rvm=0.25 mbr=-0.3 mwm=0.3" NOMOD " vol=-11.3" },
            { "Vintage Pads", "Orgue de saló", "Taula d'orgue amb chorus i Leslie suau (auto-pan): vell i entranyable.",
              "o1w=6 o1t=11 o1sh=0.5 o2n=0 uni=1 cut=5000 aa=40 ar=400 chm=0.4 chr=0.8 apq=0.35 apr=5.5 rvm=0.2" NOMOD " vol=-3.7" },
            { "Vintage Pads", "VHS", "Desafinat, fosc i inestable: com una cinta de vídeo vella.",
              "o2f=14 uni=2 cut=1500 der=0.9 derv=0.7 aa=500 ar=2000 nz=0.1 nzc=0.9 chm=0.5 rvm=0.25 mbr=-0.4" NOMOD " m1f=1 m1d=0 m1q=0.01 l1r=0.4 l1w=5 vol=-7.0" },

            // ---- Space Pads ----
            { "Space Pads", "Òrbita", "Auto-pan lent, phaser i delay ping-pong: el so dona voltes al teu cap.",
              "o1w=6 o1t=4 o1sh=0.4 o2n=0.4 uni=3 cut=5000 aa=1200 ar=4000 phm=0.4 phr=0.07 apq=0.6 apr=0.12 dlm=0.25 dlt=3 rvm=0.45 rvs=0.8" NOMOD " vol=-1.5" },
            { "Space Pads", "Nebulosa", "Granular de taules digitals amb dispersió gran: pols d'estrelles.",
              "o1w=7 o1t=4 o1sh=0.5 o1n=0.9 o2w=7 o2t=14 o2n=0.5 grd=35 grm=150 grs=0.8 grt=0.4 cut=7000 aa=1500 ar=4500 rvm=0.5 rvs=0.85" NOMOD " vol=-6.0" },
            { "Space Pads", "Estació espacial", "Metall FM amb phaser i vent: ciència-ficció dels 80.",
              "o1w=6 o1t=7 o1sh=0.4 o2w=0 o2o=1 o2s=7 o2n=0.1 fm21=0.2 uni=2 cut=4000 res=0.3 aa=1000 ar=3500 phm=0.5 nz=0.12 nzt=1 rvm=0.4" NOMOD " m1f=1 m1d=1 m1q=0.3 l1r=0.1 vol=-6.6" },
            { "Space Pads", "Gravetat zero", "Sinus que flota amb deriva gran i reverb ambient enorme.",
              "o1w=0 o2w=1 o2o=1 o2n=0.3 uni=4 cut=6000 aa=2000 ar=5000 der=0.7 derv=0.2 rvt=2 rvm=0.55 rvs=0.95" NOMOD " vol=-7.4" },
            { "Space Pads", "Senyal llunyà", "Passa-banda que escaneja i delay de tresets: una transmissió perduda.",
              "o1w=2 o2w=4 o2n=0.4 uni=2 ft=2 cut=1500 res=0.5 aa=800 ar=3500 dlm=0.3 dlt=4 dlf=0.6 rvm=0.4" NOMOD " m1f=1 m1d=1 m1q=0.6 l1r=0.15 l1w=1 vol=-3.7" },

            // ---- Wide Pads ----
            { "Wide Pads", "Panoràmic", "Unisons a fons, oscil·ladors als extrems i amplada al màxim: ompli tot l'estèreo.",
              "o1p=-0.8 o2p=0.8 uni=5 unid=0.3 unia=1 cut=5000 aa=700 ar=3000 chm=0.5 rvm=0.35 width=1.9" NOMOD " vol=-12.5" },
            { "Wide Pads", "Gran angular", "Dues parelles a costats oposats i chorus d'ensemble.",
              "o1p=-1 o2p=-0.5 o3w=2 o3p=1 o3n=0.6 o4w=4 o4p=0.5 o4n=0.4 uni=2 cut=4000 aa=600 ar=2500 chm=0.6 rvm=0.3 width=1.7" NOMOD " vol=-12.0" },
            { "Wide Pads", "Estadi", "Serres massives i amples per a la tornada gran: molt de cos.",
              "uni=5 unid=0.35 unia=1 o4w=2 o4o=-1 o4n=0.4 cut=6000 aa=300 ar=2000 rvm=0.3 width=1.8 mwm=0.2" NOMOD " vol=-13.6" },
            { "Wide Pads", "Dos mons", "Cada veu amb el seu pan a l'atzar: els acords s'escampen.",
              "o1w=6 o1t=9 o1sh=0.6 o2w=1 o2o=1 o2n=0.4 o4w=0 o4o=-1 o4n=0.3 uni=3 cut=4500 aa=800 ar=3000 rvm=0.35 width=1.6" NOMOD " m1f=10 m1d=11 m1q=0.8 vol=-9.4" },
            { "Wide Pads", "Cortina estèreo", "Phaser a dos canals i auto-pan lent sobre un pad de pols.",
              "o1w=4 o2w=4 o2f=10 uni=3 cut=3500 aa=700 ar=3000 phm=0.5 phr=0.1 apq=0.4 apr=0.08 rvm=0.35 width=1.6" NOMOD " vol=-10.1" },

            // ---- Rhythmic Pads ----
            { "Rhythmic Pads", "Trance gate", "L'arpegiador en mode acord repeteix el pad a setzenes: el pad tallat de la música de ball.",
              "uni=3 cut=4000 aa=5 ad=200 as=0.6 ar=150 arp=6 arpr=3 arpg=0.55 rvm=0.25 dlm=0.2 dlt=3" NOMOD " vol=-8.1" },
            { "Rhythmic Pads", "Corxeres de pols", "Volum amb LFO quadrat a corxeres: un pad que fa de ritme.",
              "o1w=4 o2w=4 o2f=7 uni=2 cut=3000 aa=200 ar=1500 rvm=0.25 chm=0.3" NOMOD " m1f=1 m1d=12 m1q=-0.8 l1w=3 l1s=4 vol=-17.1" },
            { "Rhythmic Pads", "Filtre sincopat", "El filtre s'obre a tresets de corxera: groove suau per a pop electrònic.",
              "uni=3 cut=900 res=0.35 aa=300 ar=1500 dlm=0.2 dlt=3 rvm=0.25" NOMOD " m1f=1 m1d=1 m1q=0.6 l1w=2 l1s=8 vol=-9.0" },
            { "Rhythmic Pads", "Acords amb swing", "Arpegiador d'acord amb swing i gate curt: per a funk lent i R&B.",
              "o1w=6 o1t=11 o2n=0.3 uni=2 cut=5000 aa=3 ad=300 as=0.4 ar=250 arp=6 arpr=1 arpg=0.4 arpsw=0.35 rvm=0.25" NOMOD " vol=-6.5" },
            { "Rhythmic Pads", "Tren nocturn", "Pan i filtre a setzenes amb delay: moviment constant per a synthwave.",
              "o1w=2 o2w=4 o2n=0.4 uni=2 cut=2200 res=0.3 aa=300 ar=1500 dlm=0.25 dlt=3 rvm=0.3" NOMOD " m1f=1 m1d=1 m1q=0.4 l1w=2 l1s=5 m2f=2 m2d=11 m2q=0.5 l2w=3 l2s=4 vol=-9.2" },

            // ---- Indie Pads ----
            { "Indie Pads", "Garatge amb amor", "Pad de pols saturat, una mica brut i molt càlid: sota guitarres netes d'indie rock.",
              "o1w=4 o1sh=0.4 o2w=2 o2n=0.4 o2f=8 uni=2 cut=2000 res=0.15 fdrv=0.5 mwm=0.4 aa=400 ar=2000 der=0.45 chm=0.4 rvt=4 rvm=0.3" NOMOD " vol=-11.8" },
            { "Indie Pads", "Habitació", "Petit i proper (reverb de room): un pad que no ocupa massa, per a versos.",
              "o1w=1 o2w=2 o2n=0.35 uni=2 cut=2400 aa=300 ar=1500 rvt=4 rvm=0.25 rvs=0.3 mdp=-0.3" NOMOD " vol=-6.1" },
            { "Indie Pads", "Postal", "Taula vocal i chorus amb delay de negra amb punt: indie pop melancòlic.",
              "o1w=6 o1t=2 o1sh=0.3 o2w=4 o2n=0.35 uni=3 cut=3500 aa=500 ar=2500 chm=0.5 dlm=0.22 dlt=6 rvm=0.3" NOMOD " m1f=1 m1d=4 m1q=0.3 l1r=0.1 vol=-6.0" },
            { "Indie Pads", "Fuzz suau", "Un pad amb saturació i calidesa a fons: per a ponts que pugen.",
              "uni=3 cut=1600 fdrv=0.8 mwm=0.6 aa=500 ar=2200 rvm=0.3" NOMOD " m1f=5 m1d=1 m1q=0.35 evt=6 vol=-14.4" },
            { "Indie Pads", "Teulada", "Orgue i cordes junts, amb una mica de vinil: indie folk.",
              "o1w=6 o1t=11 o1sh=0.6 o2w=6 o2t=9 o2n=0.5 uni=2 cut=4000 aa=200 ar=1500 nz=0.12 nzt=2 chm=0.3 rvm=0.3" NOMOD " vol=-7.7" },

            // ---- Emotional Pads ----
            { "Emotional Pads", "Carta", "Cordes suaus que s'obren amb la força de la nota: toca fluix i fort i escolta com respon.",
              "o1w=6 o1t=9 o1sh=0.3 o2w=1 o2o=1 o2n=0.3 uni=3 cut=1500 fvel=0.8 vv=0.7 aa=900 ar=3000 rvm=0.4" NOMOD " m1f=6 m1d=4 m1q=0.5 vol=-5.2" },
            { "Emotional Pads", "Nostàlgia", "Serres amb deriva i vibrato que entra a poc a poc: pop emocional, tornades que ploren.",
              "uni=3 unid=0.25 cut=2800 aa=600 ar=2800 der=0.35 chm=0.35 rvm=0.4 dlm=0.15 dlt=6" NOMOD " m1f=1 m1d=0 m1q=0.005 l1r=5 l1in=1500 vol=-10.1" },
            { "Emotional Pads", "Abraçada", "Cor i cordes amb sub: ple i acollidor.",
              "o1w=6 o1t=8 o1sh=0.4 o2w=6 o2t=9 o2n=0.5 o4w=0 o4o=-1 o4n=0.4 mix=0.5 uni=3 cut=4000 aa=1200 ar=3500 rvm=0.4" NOMOD " m1f=5 m1d=8 m1q=0.3 vol=-8.9" },
            { "Emotional Pads", "Llàgrima", "Sinus i campana suau amb shimmer molt lleu: per a piano i veu.",
              "o1w=0 o2w=6 o2t=5 o2o=1 o2n=0.2 uni=2 cut=6000 aa=800 ar=3500 rvt=3 rvm=0.4 rvsh=0.15" NOMOD " vol=-7.3" },
            { "Emotional Pads", "Esperança", "L'evolució obre el filtre i afegeix octaves al llarg de 12 segons: puja l'emoció.",
              "o1w=2 o2w=6 o2t=14 o2n=0.4 uni=3 cut=1000 aa=1000 ar=3500 evt=12 rvm=0.4" NOMOD " m1f=5 m1d=1 m1q=0.7 m2f=5 m2d=14 m2q=0.6 vol=-8.6" },

            // ---- Massive Pads ----
            { "Massive Pads", "Muntanya", "Quatre serres, cinc unisons i saturació: el pad més gros de la casa.",
              "o3w=2 o3o=1 o3n=0.5 o4w=2 o4o=-1 o4n=0.5 uni=5 unid=0.35 cut=3500 fdrv=0.4 mwm=0.3 aa=300 ar=2500 rvm=0.3 width=1.6" NOMOD " vol=-15.2" },
            { "Massive Pads", "Tsunami", "Atac lent, filtre que s'obre del tot i reverb gran: arrasa.",
              "o3w=2 o3o=1 o3n=0.4 o4w=4 o4o=-1 o4n=0.5 uni=5 cut=500 fenv=0.8 fa=4000 fd=6000 fs=0.9 aa=1500 ar=3500 rvm=0.4 rvs=0.8 width=1.5" NOMOD " vol=-12.0" },
            { "Massive Pads", "Formigó", "Serra i quadrada greus amb saturació: dur i compacte per a rock.",
              "o2w=3 o2o=-1 o2n=0.5 o4w=2 o4o=-1 o4n=0.4 uni=3 cut=2200 fdrv=0.7 mwm=0.5 aa=150 ar=1500 rvt=4 rvm=0.25" NOMOD " vol=-14.4" },
            { "Massive Pads", "Supernova", "Wavetables digitals amb FM i shimmer: gros i brillant.",
              "o1w=6 o1t=4 o1sh=0.6 o2w=0 o2o=1 o2n=0.1 fm21=0.25 o3w=2 o3n=0.5 uni=4 cut=6000 aa=500 ar=3000 rvt=3 rvm=0.4 rvsh=0.3 width=1.6" NOMOD " vol=-6.2" },
            { "Massive Pads", "Catedral", "Orgue gran amb cordes i reverb de sala enorme.",
              "o1w=6 o1t=11 o1sh=0.9 o2w=6 o2t=9 o2n=0.5 o4w=0 o4o=-1 o4n=0.4 uni=3 cut=5000 aa=600 ar=4000 rvm=0.5 rvs=0.95" NOMOD " vol=-8.0" },

            // ---- Noise Textures ----
            { "Noise Textures", "Vent a la muntanya", "Només vent que bufa i es mou, amb un sinus molt fluix a la nota: textura per a intros.",
              "o1w=0 o1n=0.15 o2n=0 nz=1 nzt=1 nzm=0.6 nzc=0.4 uni=1 cut=9000 aa=1500 ar=4000 rvm=0.35" NOMOD " vol=-6.3" },
            { "Noise Textures", "Platja", "Soroll blanc que va i ve com les onades.",
              "o1n=0 o2n=0 o3w=5 o3sh=0.4 o3n=0.6 nz=0.4 nzc=0.3 cut=7000 aa=2000 ar=4000 rvm=0.35" NOMOD " m1f=1 m1d=12 m1q=0.6 l1r=0.12 l1w=1 m2f=1 m2d=1 m2q=0.5 vol=-6.7" },
            { "Noise Textures", "Ràdio", "Soroll pel passa-banda ressonant que segueix la nota: xiulets i interferències.",
              "o1n=0 o2w=6 o2t=6 o2n=0.2 o3w=5 o3n=0.8 ft=2 cut=1200 res=0.7 key=1 aa=500 ar=2500 rvm=0.3" NOMOD " m1f=1 m1d=1 m1q=0.3 l1r=0.3 l1w=4 vol=1.7" },
            { "Noise Textures", "Disc antic", "Cruixit de vinil amb un pad molt fosc a sota.",
              "o1w=1 o1n=0.4 o2n=0 nz=0.6 nzt=2 nzm=0.6 cut=1200 aa=800 ar=2500 rvt=4 rvm=0.2" NOMOD " vol=0.2" },
            { "Noise Textures", "Respir", "Respiració amb nota (textura que puja i baixa): proper, humà.",
              "o1w=6 o1t=12 o1n=0.4 o2n=0 nz=0.7 nzt=3 nzm=0.4 nzc=0.5 cut=6000 aa=1000 ar=3000 rvm=0.35" NOMOD " vol=-3.9" },

            // ---- Drone Pads ----
            { "Drone Pads", "Bordó", "Mono, quinta i octava greus amb deriva: el so que sosté tota una cançó.",
              "veus=1 o2s=7 o2f=0 o4w=0 o4o=-1 o4n=0.5 uni=3 cut=1500 aa=2000 ar=5000 der=0.4 rvm=0.4" NOMOD " m1f=1 m1d=1 m1q=0.25 l1r=0.04 l1w=5 vol=-6.1" },
            { "Drone Pads", "Tibetà", "Formant que es passeja lentament per sobre d'una nota fixa: hipnòtic.",
              "veus=1 o1w=6 o1t=15 o2w=0 o2o=-1 o2n=0.5 uni=2 cut=5000 aa=2000 ar=5000 rvm=0.4" NOMOD " m1f=1 m1d=4 m1q=0.7 l1r=0.05 l1w=1 vol=-3.8" },
            { "Drone Pads", "Motor", "Drone metàl·lic greu amb FM i soroll: industrial.",
              "o1o=-1 o2w=0 o2o=-1 o2s=7 o2n=0.15 fm21=0.4 uni=2 cut=1800 res=0.3 aa=1500 ar=4000 nz=0.2 nzt=1 nzc=0.8 rvm=0.35" NOMOD " m1f=1 m1d=9 m1q=0.2 l1r=0.1 l1w=5 vol=-6.0" },
            { "Drone Pads", "Mantra", "Cor greu i octaves que es mouen amb l'evolució en cicle.",
              "o1w=6 o1t=8 o1o=-1 o2w=6 o2t=14 o2n=0.4 uni=3 cut=4000 aa=2500 ar=5000 evt=10 evf=3 rvm=0.45" NOMOD " m1f=5 m1d=8 m1q=0.6 vol=-7.2" },
            { "Drone Pads", "Pedal de guitarra", "Una sola nota amb shimmer i delay llarg: com un EBow amb efectes.",
              "veus=1 o1w=2 o2w=1 o2o=1 o2n=0.3 uni=2 cut=3000 aa=1500 ar=5000 dlm=0.3 dlt=7 dlf=0.6 rvt=3 rvm=0.5 rvsh=0.35" NOMOD " vol=-1.7" },

            // ---- Pulsating Pads ----
            { "Pulsating Pads", "Batec", "El volum batega a negres: un pad amb pols, per a construccions.",
              "uni=3 cut=3500 aa=300 ar=2000 rvm=0.3" NOMOD " m1f=1 m1d=12 m1q=-0.5 l1w=0 l1s=3 vol=-10.3" },
            { "Pulsating Pads", "Ones de calor", "Trèmolo suau i filtre que respira amb el tempo.",
              "o1w=6 o1t=13 o2n=0.4 uni=3 cut=2500 aa=500 ar=2500 rvm=0.3 chm=0.3" NOMOD " m1f=1 m1d=12 m1q=-0.4 l1s=4 m2f=2 m2d=1 m2q=0.35 l2s=2 vol=-9.3" },
            { "Pulsating Pads", "Radar", "PWM i pan a corxeres: un pad que escaneja.",
              "o1w=4 o2w=4 o2f=6 uni=2 cut=3200 aa=400 ar=2000 rvm=0.3" NOMOD " m1f=1 m1d=4 m1q=0.6 l1w=1 l1s=4 m2f=2 m2d=11 m2q=0.6 l2s=3 vol=-11.9" },
            { "Pulsating Pads", "Cor que batega", "Cor wavetable amb trèmolo de setzenes i delay.",
              "o1w=6 o1t=8 o2w=6 o2t=8 o2sh=0.5 o2n=0.5 uni=3 cut=5000 aa=400 ar=2500 dlm=0.2 dlt=3 rvm=0.35" NOMOD " m1f=1 m1d=12 m1q=-0.6 l1w=1 l1s=5 vol=-5.9" },
            { "Pulsating Pads", "Respiració ràpida", "Filtre ressonant que s'obre a tresets: tensió que avança.",
              "uni=3 cut=1200 res=0.45 aa=400 ar=2000 rvm=0.3" NOMOD " m1f=1 m1d=1 m1q=0.5 l1w=0 l1s=8 vol=-9.9" },

            // ---- Granular Pads ----
            { "Granular Pads", "Núvol de sorra", "Molts grànuls curts de taula harmònica: una textura que brilla com sorra al sol.",
              "o1w=7 o1t=3 o1sh=0.5 o1n=1 o2n=0 grd=60 grm=40 grs=0.6 grt=0.3 cut=8000 aa=800 ar=3000 rvm=0.4" NOMOD " vol=-7.3" },
            { "Granular Pads", "Mosaic", "Grànuls de vidre i d'octaves amb salts de quinta: dibuix sonor que canvia sempre.",
              "o1w=7 o1t=10 o1sh=0.4 o1n=0.9 o2w=7 o2t=14 o2n=0.5 grd=20 grm=150 grs=0.5 gro=3 cut=8000 aa=800 ar=3500 rvm=0.45 dlm=0.2 dlt=3" NOMOD " vol=-8.2" },
            { "Granular Pads", "Polsim", "Granular d'«Aire» amb grànuls llargs i suaus: gairebé un pad normal, però viu.",
              "o1w=7 o1t=12 o1sh=0.5 o1n=1 o2w=0 o2n=0.25 grd=12 grm=400 grs=0.3 cut=6000 aa=1500 ar=4000 rvm=0.4" NOMOD " vol=-7.9" },
            { "Granular Pads", "Escàner", "La posició dels grànuls recorre la taula de formants amb l'LFO: una veu que parla.",
              "o1w=7 o1t=15 o1n=1 o2n=0 grd=30 grm=120 grs=0.15 cut=6000 aa=800 ar=3000 rvm=0.4" NOMOD " m1f=1 m1d=4 m1q=0.8 l1r=0.15 l1w=1 vol=-4.9" },
            { "Granular Pads", "Eixam", "Densitat que puja amb l'evolució: de pocs grànuls a un eixam.",
              "o1w=7 o1t=4 o1sh=0.3 o1n=1 o2w=7 o2t=8 o2n=0.5 grd=6 grm=100 grs=0.7 grt=0.5 cut=7000 aa=1000 ar=3500 evt=10 rvm=0.45" NOMOD " m1f=5 m1d=17 m1q=1 vol=-0.4" },

            // ---- Dream Pop ----
            { "Dream Pop", "Cocteau", "Chorus a fons, delay llarg i reverb enorme sobre serres suaus: el so dels somnis.",
              "o2f=6 uni=3 cut=3000 aa=600 ar=3500 chm=0.8 chp=0.7 dlm=0.3 dlt=6 dlf=0.55 rvm=0.5 rvs=0.85" NOMOD " vol=-7.5" },
            { "Dream Pop", "Platja de nit", "Pols amb chorus i vibrato lent, reverb shimmer suau: Beach House a casa.",
              "o1w=4 o1sh=0.35 o2w=4 o2f=8 o2n=0.5 uni=2 cut=2500 aa=500 ar=3000 chm=0.6 rvt=3 rvm=0.45 rvsh=0.2" NOMOD " m1f=1 m1d=0 m1q=0.008 l1r=4.5 l1in=800 vol=-8.4" },
            { "Dream Pop", "Mareig", "Pitch que oneja una mica (com una cinta) i molta reverb.",
              "o1w=6 o1t=12 o1sh=0.6 o2w=4 o2sh=0.5 o2n=0.5 uni=3 cut=2000 aa=700 ar=3500 chm=0.5 rvm=0.5" NOMOD " m1f=1 m1d=0 m1q=0.012 l1r=0.5 l1w=5 vol=-7.0" },
            { "Dream Pop", "Rosa i blau", "Cor i cordes amb phaser lent: color pastel.",
              "o1w=6 o1t=8 o2w=6 o2t=9 o2n=0.5 uni=3 cut=4000 aa=800 ar=3500 phm=0.35 phr=0.06 chm=0.4 rvm=0.45" NOMOD " vol=-4.2" },
            { "Dream Pop", "Vels", "Granular de cor molt suau sota un pad de sinus: transparent.",
              "o1w=0 o2w=7 o2t=8 o2n=0.6 grd=20 grm=300 grs=0.5 uni=3 cut=5000 aa=1200 ar=4000 chm=0.4 rvm=0.5" NOMOD " vol=-8.8" },

            // ---- Post Rock ----
            { "Post Rock", "Crescendo de guitarres", "Serres saturades que s'obren amb l'evolució i shimmer: el moment que tot explota.",
              "uni=4 cut=900 fdrv=0.6 mwm=0.4 aa=1500 ar=4000 evt=16 rvt=3 rvm=0.45 rvsh=0.35 dlm=0.25 dlt=6" NOMOD " m1f=5 m1d=1 m1q=0.8 vol=-13.8" },
            { "Post Rock", "Tremolo picking", "Volum a setzenes amb delay: com una guitarra en trèmolo.",
              "o1w=2 o2w=3 o2n=0.4 uni=2 cut=3500 fdrv=0.3 aa=200 ar=2500 dlm=0.25 dlt=3 rvm=0.45" NOMOD " m1f=1 m1d=12 m1q=-0.9 l1w=0 l1s=5 vol=-15.4" },
            { "Post Rock", "Cel obert", "Octaves i shimmer amb amplada gran: després de l'explosió.",
              "o1w=6 o1t=14 o2w=2 o2n=0.4 uni=4 cut=6000 aa=800 ar=4500 rvt=3 rvm=0.55 rvsh=0.5 rvs=0.9 width=1.7" NOMOD " vol=-10.9" },
            { "Post Rock", "Reverb infinita", "Pad fosc dins una reverb llarguíssima (prova el FREEZE al final d'una secció).",
              "o1w=6 o1t=13 o2n=0.4 uni=3 cut=2500 aa=1000 ar=5000 rvm=0.6 rvs=1" NOMOD " vol=-9.6" },
            { "Post Rock", "Ebow", "Mono, atac lent i glide: una nota infinita que canta.",
              "veus=1 glide=200 o2w=1 o2o=1 o2n=0.3 uni=2 cut=2500 fdrv=0.3 aa=900 ar=2500 dlm=0.3 dlt=6 rvm=0.45" NOMOD " m1f=1 m1d=0 m1q=0.005 l1r=5 l1in=1500 vol=-4.6" },

            // ---- Synth Pop ----
            { "Synth Pop", "Anys 80", "Pols brillant amb chorus i delay: el pad de les cançons de ràdio de 1984.",
              "o1w=4 o1sh=0.3 o2w=2 o2n=0.4 o2f=5 uni=2 cut=4500 res=0.15 aa=50 ar=1200 chm=0.6 dlm=0.18 dlt=3 rvt=1 rvm=0.25" NOMOD " vol=-9.4" },
            { "Synth Pop", "Synthwave", "Serres amples, sub i delay de negra amb punt: neons i carretera.",
              "o4w=0 o4o=-1 o4n=0.4 uni=4 unid=0.3 cut=3000 res=0.2 fenv=0.2 aa=100 ar=1800 chm=0.5 dlm=0.22 dlt=6 rvt=1 rvm=0.3 width=1.5" NOMOD " vol=-10.7" },
            { "Synth Pop", "Pop modern", "Pad net, digital i ample, sense deriva: per a pop actual.",
              "o1w=6 o1t=3 o1sh=0.6 o2w=2 o2n=0.4 der=0 uni=4 cut=8000 aa=150 ar=1200 rvt=1 rvm=0.25 width=1.5" NOMOD " vol=-6.8" },
            { "Synth Pop", "Discoteca", "Pad tallat amb l'arpegiador d'acord a corxeres.",
              "o1w=4 o2w=2 o2n=0.4 uni=3 cut=4000 aa=5 ad=250 as=0.5 ar=200 arp=6 arpr=1 arpg=0.6 dlm=0.15 dlt=3 rvm=0.25" NOMOD " vol=-9.2" },
            { "Synth Pop", "Vidre de color", "Campana i serra junts: el pad brillant de la balada synth.",
              "o1w=6 o1t=5 o1sh=0.4 o2w=2 o2n=0.4 uni=3 cut=5000 aa=60 ar=2000 chm=0.4 rvt=1 rvm=0.3" NOMOD " vol=-1.6" },

            // ---- Alternative Rock ----
            { "Alternative Rock", "Teclat de directe", "Pad saturat i curt, per sobre de guitarres distorsionades.",
              "uni=2 cut=2500 fdrv=0.6 mwm=0.4 aa=60 ar=900 rvt=4 rvm=0.2" NOMOD " vol=-12.5" },
            { "Alternative Rock", "Murmuri", "Pad fosc i brut sota el vers: omple sense molestar.",
              "o1w=3 o2w=2 o2n=0.4 uni=2 cut=1100 fdrv=0.5 aa=400 ar=2000 der=0.4 rvm=0.25 mbr=-0.3" NOMOD " vol=-16.1" },
            { "Alternative Rock", "Orgue distorsionat", "Orgue amb saturació i Leslie (auto-pan ràpid).",
              "o1w=6 o1t=11 o1sh=0.8 o2n=0 uni=1 cut=4000 fdrv=0.7 mwm=0.5 aa=20 ar=400 apq=0.4 apr=6 rvt=4 rvm=0.2" NOMOD " vol=-10.4" },
            { "Alternative Rock", "Mur de so", "Serres massives saturades amb reverb gran: el final de la cançó.",
              "o3w=2 o3o=1 o3n=0.5 o4w=3 o4o=-1 o4n=0.4 uni=5 cut=3000 fdrv=0.6 mwm=0.4 aa=300 ar=2500 rvm=0.4 width=1.6" NOMOD " vol=-16.8" },
            { "Alternative Rock", "Fona", "Pad que s'obre de cop amb la força de la nota: per a accents.",
              "uni=3 cut=800 fvel=1 vv=0.8 fenv=0.5 fa=50 fd=1500 fs=0.4 aa=50 ar=1500 fdrv=0.3 rvm=0.3" NOMOD " m1f=6 m1d=1 m1q=0.4 vol=-11.6" },

            // ---- Soundtrack ----
            { "Soundtrack", "Títols de crèdit", "Cordes i cor amb reverb de sala gran: final de pel·lícula.",
              "o1w=6 o1t=9 o1sh=0.5 o2w=6 o2t=8 o2n=0.5 o4w=0 o4o=-1 o4n=0.35 uni=4 cut=4500 aa=1500 ar=4000 rvm=0.5 rvs=0.9" NOMOD " m1f=1 m1d=0 m1q=0.004 l1r=5 l1in=1200 vol=-7.9" },
            { "Soundtrack", "Investigació", "Pad fosc amb campana greu i tensió que creix: thriller, documental.",
              "o1w=6 o1t=13 o2w=6 o2t=5 o2o=-1 o2n=0.3 uni=3 cut=1800 aa=2000 ar=4000 evt=12 rvm=0.4" NOMOD " m1f=5 m1d=2 m1q=0.4 m2f=5 m2d=1 m2q=0.4 vol=-7.8" },
            { "Soundtrack", "Record", "Sinus i vidre amb delay: flashback, un record llunyà.",
              "o1w=0 o2w=6 o2t=10 o2o=1 o2n=0.3 uni=2 cut=6000 aa=900 ar=4000 dlm=0.3 dlt=6 dlf=0.55 rvt=2 rvm=0.45" NOMOD " vol=-9.5" },
            { "Soundtrack", "Desert", "Formant i vent amb quinta: paisatge sec i immens.",
              "o1w=6 o1t=15 o1sh=0.3 o2s=7 o2n=0.35 uni=3 cut=3500 aa=1800 ar=4500 nz=0.2 nzt=1 rvm=0.45" NOMOD " m1f=1 m1d=4 m1q=0.4 l1r=0.05 vol=-3.6" },
            { "Soundtrack", "Profunditats", "Drone greu, cruixit i reverb immensa: sota l'aigua.",
              "o1w=6 o1t=13 o1o=-1 o2w=0 o2o=-1 o2n=0.5 uni=3 cut=700 aa=3000 ar=5000 nz=0.25 nzt=2 rvm=0.55 rvs=0.95 mbr=-0.4" NOMOD " m1f=1 m1d=1 m1q=0.3 l1r=0.04 l1w=5 vol=-7.3" },

            // ---- Intro Textures ----
            { "Intro Textures", "Primer compàs", "Neix del no-res en 6 segons, amb vent i grànuls: per començar una cançó.",
              "o1w=6 o1t=8 o2w=7 o2t=12 o2n=0.5 grd=15 uni=3 cut=1500 aa=6000 ar=4000 evt=8 nz=0.2 nzt=1 rvm=0.45" NOMOD " m1f=5 m1d=1 m1q=0.6 vol=-3.9" },
            { "Intro Textures", "Ràdio que s'encén", "Passa-banda que s'obre a LP amb l'evolució: com si la cançó vingués de lluny.",
              "uni=3 ft=2 cut=1000 res=0.4 aa=1500 ar=3000 evt=6 nz=0.15 nzt=2 rvm=0.35" NOMOD " m1f=5 m1d=1 m1q=0.6 m2f=5 m2d=2 m2q=-0.4 vol=3.2" },
            { "Intro Textures", "Despertar", "Pad que va de fosc a brillant mentre puja el volum: ideal per a intros de concert.",
              "o1w=6 o1t=3 o1sh=0.1 o2n=0.4 uni=3 cut=800 aa=4000 ar=4000 evt=10 rvm=0.4" NOMOD " m1f=5 m1d=8 m1q=0.8 m2f=5 m2d=1 m2q=0.6 vol=-7.4" },
            { "Intro Textures", "Batec llunyà", "Batec a negres molt filtrat i amb reverb: l'expectativa abans de la bateria.",
              "o4w=0 o4o=-1 o4n=0.5 uni=3 cut=600 aa=800 ar=2500 rvm=0.5" NOMOD " m1f=1 m1d=12 m1q=-0.6 l1w=0 l1s=3 vol=-11.6" },
            { "Intro Textures", "Boira de sala", "Respiració i cor suau: el teló encara tancat.",
              "o1w=6 o1t=8 o1sh=0.4 o2n=0 nz=0.3 nzt=3 uni=3 cut=4000 aa=2500 ar=4000 rvm=0.45" NOMOD " vol=-5.5" },

            // ---- Crescendos ----
            { "Crescendos", "Construcció", "Filtre que s'obre en 16 segons i octaves que entren: aguanta un acord durant la construcció.",
              "o3w=2 o3o=1 o3n=0.5 uni=4 cut=400 aa=1000 ar=3000 evt=16 rvm=0.35" NOMOD " m1f=5 m1d=1 m1q=1 m2f=5 m2d=15 m2q=0.8 vol=-9.4" },
            { "Crescendos", "Riser", "El to puja una mica i el soroll creix: la pujada abans de la tornada.",
              "uni=4 cut=1000 aa=500 ar=1500 evt=8 nz=0.3 rvm=0.35" NOMOD " m1f=5 m1d=1 m1q=0.8 m2f=5 m2d=18 m2q=1 m3f=5 m3d=0 m3q=0.04 vol=-10.0" },
            { "Crescendos", "Onada", "Evolució que puja i baixa (8 + 8 s): crescendo i decrescendo amb un sol acord.",
              "o1w=6 o1t=9 o2n=0.4 uni=4 cut=800 aa=1500 ar=3000 evt=8 evf=2 rvm=0.4" NOMOD " m1f=5 m1d=1 m1q=0.8 m2f=5 m2d=12 m2q=0.5 vol=-8.0" },
            { "Crescendos", "Tensió", "Trèmolo que s'accelera amb la roda i filtre amb l'evolució.",
              "uni=3 cut=1000 aa=800 ar=2500 evt=10 rvm=0.35" NOMOD " m1f=1 m1d=12 m1q=-0.5 l1s=4 m2f=5 m2d=1 m2q=0.7 vol=-10.2" },
            { "Crescendos", "Explosió", "Atac lent, saturació i reverb gegant: el clímax.",
              "o3w=2 o3o=1 o3n=0.4 o4w=2 o4o=-1 o4n=0.4 uni=5 cut=600 fenv=0.7 fa=3000 fd=5000 fs=0.9 fdrv=0.5 mwm=0.3 aa=2000 ar=4000 rvm=0.45 rvs=0.85 width=1.6" NOMOD " vol=-15.0" },
        };
        #undef NOMOD

        void aplica (Atmos& s, const char* canvis)
        {
            std::istringstream is (canvis);
            std::string tros;
            while (is >> tros)
            {
                const auto ig = tros.find ('=');
                if (ig == std::string::npos) continue;
                const auto clau = tros.substr (0, ig);
                const float v = std::strtof (tros.c_str() + ig + 1, nullptr);
                for (int p = 0; p < numParams; ++p)
                    if (clau == info (p).clau) { s.posa (p, v); break; }
            }
        }

        struct Atzar
        {
            std::uint32_t s;
            explicit Atzar (unsigned l) : s (l * 2654435761u + 777u) { if (s == 0) s = 1; }
            float u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 16777216.0f; }
            float r (float a, float b) { return a + (b - a) * u(); }
            float lg (float a, float b) { return a * std::pow (b / a, u()); }
            bool p (float prob) { return u() < prob; }
            int i (int a, int b) { return a + (int) (u() * (float) (b - a + 1)) % (b - a + 1); }
        };
    }

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> v = []
        {
            std::vector<Preset> r;
            for (const auto& x : taula)
            {
                auto s = std::make_unique<Atmos>();
                aplica (*s, x.canvis);
                r.push_back ({ x.nom, x.cat, x.desc, s->estatSo() });
            }
            return r;
        }();
        return v;
    }

    std::string estatInicial() { auto s = std::make_unique<Atmos>(); return s->estatSo(); }

    std::string aleatori (unsigned llavor, int tipus)
    {
        Atzar a (llavor);
        if (tipus < 0 || tipus >= (int) tipusPad().size()) tipus = a.i (0, (int) tipusPad().size() - 1);
        auto sp = std::make_unique<Atmos>();
        auto& s = *sp;
        // la base comuna: dues parelles consonants, unison moderat, envolupants de pad, efectes dosificats
        static const float intervals[] = { 0, 0, 0, 7, 12, 12, -12, 5 };
        auto osc = [&] (int o, int ona, int taula, float niv)
        {
            const int b = o * 8;
            s.posa (O1Ona + b, (float) ona);
            s.posa (O1Taula + b, (float) taula);
            s.posa (O1Forma + b, a.r (0.0f, 0.8f));
            s.posa (O1Niv + b, niv);
            s.posa (O1Fi + b, a.r (-12.0f, 12.0f));
            s.posa (O1Pan + b, a.r (-0.4f, 0.4f));
        };
        for (int o = 0; o < 4; ++o) s.posa (O1Niv + o * 8, 0.0f);
        s.posa (Unison, (float) a.i (2, 4));
        s.posa (UniDetune, a.r (0.15f, 0.4f));
        s.posa (AA, a.lg (400.0f, 2500.0f));
        s.posa (AR, a.lg (1500.0f, 5000.0f));
        s.posa (AS, a.r (0.7f, 1.0f));
        s.posa (FTall, a.lg (1500.0f, 6000.0f));
        s.posa (FRes, a.r (0.0f, 0.25f));
        s.posa (Deriva, a.r (0.1f, 0.4f));
        s.posa (RvMix, a.r (0.25f, 0.45f));
        s.posa (RvMida, a.r (0.5f, 0.85f));
        s.posa (ChMix, a.p (0.5f) ? a.r (0.2f, 0.6f) : 0.0f);
        s.posa (DlMix, a.p (0.4f) ? a.r (0.1f, 0.3f) : 0.0f);
        s.posa (DlNota, a.p (0.5f) ? 6.0f : 3.0f);
        for (int k = 0; k < 8; ++k) { s.posa (M1Quant + k * 3, 0.0f); s.posa (M1Font + k * 3, 0.0f); }
        s.posa (M4Font, (float) FRoda); s.posa (M4Desti, (float) DTall); s.posa (M4Quant, 0.4f);
        auto via = [&] (int k, int f, int d, float q) { s.posa (M1Font + k * 3, (float) f); s.posa (M1Desti + k * 3, (float) d); s.posa (M1Quant + k * 3, q); };
        // un moviment lent sempre (els pads han de viure)
        s.posa (L1Vel, a.lg (0.04f, 0.4f));
        s.posa (L1Ona, a.p (0.5f) ? 5.0f : 0.0f);
        via (0, FLfo1, a.p (0.5f) ? DTall : DFormes, a.r (0.1f, 0.35f));
        switch (tipus)
        {
            case 0:   // Analog Warm
                osc (0, Serra, 0, 0.7f); osc (1, a.p (0.5f) ? Pols : Serra, 0, 0.5f); osc (3, Sinus, 0, 0.3f);
                s.posa (O4Oct, -1.0f); s.posa (FTall, a.lg (1200.0f, 2800.0f)); s.posa (FDrive, a.r (0.1f, 0.4f)); s.posa (Deriva, a.r (0.3f, 0.6f));
                s.posa (MCalidesa, a.r (0.1f, 0.4f));
                break;
            case 1:   // Vintage
                osc (0, a.p (0.5f) ? Wavetable : Serra, 9, 0.7f); osc (1, Pols, 0, 0.4f);
                s.posa (ChMix, a.r (0.6f, 1.0f)); s.posa (Deriva, a.r (0.4f, 0.8f)); s.posa (NzNiv, a.p (0.5f) ? a.r (0.05f, 0.25f) : 0.0f);
                s.posa (NzTipus, 2.0f); s.posa (MBrillantor, a.r (-0.4f, 0.0f)); s.posa (AA, a.lg (100.0f, 600.0f));
                break;
            case 2:   // Digital
                osc (0, Wavetable, a.p (0.5f) ? 4 : 3, 0.7f); osc (1, Sinus, 0, 0.2f);
                s.posa (O2Oct, 1.0f); s.posa (Fm21, a.r (0.1f, 0.35f)); s.posa (Deriva, 0.0f); s.posa (FTall, a.lg (5000.0f, 10000.0f));
                via (1, FLfo2, DForma1, a.r (0.2f, 0.5f)); s.posa (L2Vel, a.lg (0.05f, 0.3f));
                break;
            case 3:   // Cinematic
                osc (0, Wavetable, 9, 0.6f); osc (1, Wavetable, 8, 0.5f); osc (2, Serra, 0, 0.35f); osc (3, Sinus, 0, 0.4f);
                s.posa (O3Oct, 1.0f); s.posa (O4Oct, -1.0f); s.posa (FTall, a.lg (700.0f, 1500.0f)); s.posa (FEnv, a.r (0.4f, 0.7f));
                s.posa (FA, a.lg (4000.0f, 10000.0f)); s.posa (FS, 0.9f); s.posa (RvMix, a.r (0.4f, 0.5f)); s.posa (RvMida, a.r (0.75f, 0.95f)); s.posa (Ample, 1.5f);
                break;
            case 4:   // Ambient
                osc (0, Wavetable, a.p (0.5f) ? 12 : 2, 0.6f); osc (1, Sinus, 0, 0.4f);
                s.posa (RvTipus, 2.0f); s.posa (RvMix, a.r (0.45f, 0.6f)); s.posa (RvMida, a.r (0.8f, 1.0f)); s.posa (DlMix, a.r (0.2f, 0.35f));
                s.posa (AA, a.lg (1500.0f, 4000.0f)); s.posa (L1Vel, a.lg (0.02f, 0.08f));
                via (1, FLfo2, DForma1, a.r (0.3f, 0.6f)); s.posa (L2Ona, 5.0f); s.posa (L2Vel, a.lg (0.03f, 0.1f)); s.posa (L2Fase, 1.0f);
                break;
            case 5:   // Choir-like
                osc (0, Wavetable, 8, 0.6f); osc (1, Wavetable, a.p (0.5f) ? 8 : 2, 0.5f);
                s.posa (O1Forma, a.r (0.0f, 0.3f)); s.posa (O2Forma, a.r (0.3f, 0.7f));
                via (1, FEvolucio, DFormes, a.r (0.3f, 0.6f)); s.posa (EvTemps, a.lg (4.0f, 10.0f)); s.posa (EvForma, 2.0f);
                via (2, FLfo3, DTo, 0.004f); s.posa (L3Vel, 5.0f); s.posa (L3Entrada, 900.0f);
                break;
            case 6:   // String-like
                osc (0, Wavetable, 9, 0.7f); osc (1, Serra, 0, 0.35f);
                s.posa (ChMix, a.r (0.5f, 0.9f)); s.posa (AA, a.lg (300.0f, 900.0f)); s.posa (Unison, (float) a.i (3, 5));
                via (2, FLfo3, DTo, 0.005f); s.posa (L3Vel, 5.2f); s.posa (L3Entrada, a.r (500.0f, 1200.0f));
                break;
            case 7:   // Evolving
                osc (0, Wavetable, a.i (8, 15), 0.6f); osc (1, Wavetable, a.i (0, 7), 0.45f); osc (2, Wavetable, a.i (8, 15), 0.6f); osc (3, Wavetable, a.i (0, 15), 0.35f);
                s.posa (Mescla, a.r (0.1f, 0.4f)); s.posa (EvTemps, a.lg (8.0f, 20.0f)); s.posa (EvForma, 3.0f);
                via (1, FEvolucio, DMescla, a.r (0.6f, 0.9f)); via (2, FLfo3, DFormes, a.r (0.3f, 0.6f)); s.posa (L3Fase, 1.0f); s.posa (L3Ona, 5.0f); s.posa (L3Vel, a.lg (0.05f, 0.2f));
                break;
            case 8:   // Dark
                osc (0, Wavetable, 13, 0.7f); osc (1, Serra, 0, 0.3f); s.posa (O1Oct, -1.0f); s.posa (O2Oct, -1.0f);
                s.posa (FTall, a.lg (400.0f, 1200.0f)); s.posa (MBrillantor, a.r (-0.6f, -0.2f)); s.posa (NzNiv, a.r (0.1f, 0.3f)); s.posa (NzTipus, 1.0f); s.posa (NzColor, 0.85f);
                s.posa (AA, a.lg (1500.0f, 3500.0f));
                break;
            case 9:   // Bright
                osc (0, Wavetable, a.p (0.5f) ? 14 : 3, 0.6f); osc (1, Serra, 0, 0.4f); s.posa (O2Oct, 1.0f);
                s.posa (FTall, a.lg (6000.0f, 12000.0f)); s.posa (MBrillantor, a.r (0.2f, 0.5f)); s.posa (ChMix, a.r (0.3f, 0.6f));
                break;
            case 10:  // Frozen
                osc (0, Wavetable, 10, 0.6f); osc (1, Sinus, 0, 0.3f); s.posa (O2Oct, 1.0f);
                s.posa (RvMida, a.r (0.95f, 1.0f)); s.posa (RvMix, a.r (0.5f, 0.6f)); s.posa (RvTo, a.r (0.6f, 0.9f)); s.posa (AR, a.lg (4000.0f, 6000.0f));
                break;
            case 11:  // Shimmer
                osc (0, Sinus, 0, 0.6f); osc (1, Wavetable, 14, 0.4f);
                s.posa (RvTipus, 3.0f); s.posa (RvShimmer, a.r (0.35f, 0.6f)); s.posa (RvMix, a.r (0.45f, 0.6f)); s.posa (RvMida, a.r (0.75f, 0.95f));
                break;
            case 12:  // Rhythmic
                osc (0, Serra, 0, 0.7f); osc (1, a.p (0.5f) ? Pols : Wavetable, 11, 0.4f);
                s.posa (ArpMode, 6.0f); s.posa (ArpRitme, a.p (0.5f) ? 3.0f : 1.0f); s.posa (ArpGate, a.r (0.4f, 0.65f));
                s.posa (AA, 5.0f); s.posa (AD, 250.0f); s.posa (AS, 0.5f); s.posa (AR, 200.0f); s.posa (DlNota, 3.0f);
                break;
            case 13:  // Pulsating
                osc (0, Serra, 0, 0.7f); osc (1, Wavetable, a.i (8, 9), 0.5f);
                via (1, FLfo2, DVolum, -a.r (0.4f, 0.8f)); s.posa (L2Ritme, (float) a.i (3, 5)); s.posa (L2Ona, a.p (0.5f) ? 0.0f : 1.0f);
                break;
            case 14:  // Granular
                osc (0, Granular, a.i (0, 15), 1.0f); osc (1, a.p (0.5f) ? Sinus : Granular, a.i (8, 15), 0.4f);
                s.posa (GrDens, a.lg (8.0f, 50.0f)); s.posa (GrMida, a.lg (60.0f, 350.0f)); s.posa (GrSpray, a.r (0.2f, 0.7f)); s.posa (GrTo, a.r (0.05f, 0.4f));
                s.posa (GrOct, a.p (0.3f) ? (float) a.i (1, 3) : 0.0f);
                break;
            default:  // Drone
                s.posa (Veus, 1.0f);
                osc (0, Serra, 0, 0.6f); osc (1, a.p (0.5f) ? Wavetable : Serra, 15, 0.4f); osc (3, Sinus, 0, 0.5f);
                s.posa (O2Semi, a.p (0.6f) ? 7.0f : 12.0f); s.posa (O4Oct, -1.0f); s.posa (AA, a.lg (1500.0f, 3000.0f)); s.posa (L1Vel, a.lg (0.02f, 0.06f));
                break;
        }
        // intervals consonants per als oscil·ladors de la parella B (i de vegades la 2)
        if (a.p (0.3f)) s.posa (O2Semi, intervals[a.i (0, 7)] == -12.0f ? 0.0f : intervals[a.i (0, 7)]);
        s.posa (Mescla, s.valor (O3Niv) > 0.0f || s.valor (O4Niv) > 0.0f ? s.valor (Mescla) : 0.5f);
        // volum anivellat aproximat: més oscil·ladors i més unison = més fort
        float energia = 0.0f;
        for (int o = 0; o < 4; ++o) energia += s.valor (O1Niv + o * 8);
        s.posa (Volum, std::clamp (-6.0f - 6.0f * std::log2 (std::max (0.5f, energia)) - (s.valor (MCalidesa) > 0.2f ? 1.5f : 0.0f), -18.0f, 0.0f));
        return s.estatSo();
    }

    std::string varia (const std::string& estat, unsigned llavor, float quant)
    {
        Atzar a (llavor);
        auto sp = std::make_unique<Atmos>();
        auto& s = *sp;
        s.posaEstat (estat);
        for (int p = 0; p < numParams; ++p)
        {
            const auto& i = info (p);
            if (i.escala == sinte::Enter || p == Volum || p == BendRang || p >= ArpMode || p == Vista) continue;
            if (p >= O1Niv && p <= O4Pan && ((p - O1Ona) % 8 == 6) && s.valor (p) < 1e-6f) continue;
            if (std::abs (s.valor (p)) < 1e-6f) continue;   // el que està apagat es queda apagat
            if (p >= MAtac && p <= MProfunditat) continue;   // els macros són de l'usuari
            float salt = 0.16f * quant;
            if (p == O1Fi || p == O2Fi || p == O3Fi || p == O4Fi) salt *= 0.5f;   // afinació: poc
            s.posa (p, sinte::deNormal (i, sinte::normal (i, s.valor (p)) + (a.u() * 2.0f - 1.0f) * salt));
        }
        return s.estatSo();
    }
}
