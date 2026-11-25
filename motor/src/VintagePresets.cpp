#include "mibase/Vintage.h"
#include <cmath>
#include <cstdlib>
#include <memory>
#include <sstream>

/** 1.4: els presets de fàbrica del VINTAGE, programats un per un (sense copiar cap preset comercial): cada un recrea la
    manera de fer d'una família d'aparells (quins oscil·ladors, quin filtre, quines envolupants, quin caràcter).
    Llenguatge: «clau=valor» separats per espais, sobre el so inicial (les claus són les de Vintage.cpp).
      Ones (o1w…): 0 serra 1 quadrada 2 triangle 3 sinus 4 pols. Peus (o1o…): -2 32' · -1 16' · 0 8' · 1 4' · 2 2'.
      Filtre (ft): 0 Ladder 1 OTA 2 SEM 3 MS-20 · pendent (fp): 0 12 dB 1 24 dB · mode (fmo): 0 LP 1 BP 2 HP 3 Notch.
      LFO (lw): 0 triangle 1 sinus 2 serra 3 rampa 4 quadrada 5 atzar · font (lf): 0 LFO 1 osc 3 2 soroll.
      Chorus (ch): 1 I 2 II 3 I+II · Reverb (rvt): 0 molla 1 placa 2 cambra 3 sala · Delay (dlt): 2 1/8 3 1/8. 5 1/4 6 1/4.
      Arp: 1 amunt 3 amunt-avall 4 atzar 7 patró · arpr: 1 1/8 3 1/16 · patrons (arpp): 0 octaves 2 berlinesa 3 galop 7 synthwave 10 pulsació.
    El volum («vol») de cada preset està anivellat (finestra de 400 ms més forta d'un acord de 3 notes, o d'una nota en
    els monofònics, a −14 dB). */
namespace mibase::analogic
{
    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c { "Bass", "Lead", "Pad", "Brass", "Strings", "Keys", "Plucks", "Sequences", "FX", "Poly Synth" };
        return c;
    }

    const std::vector<std::string>& families()
    {
        static const std::vector<std::string> f {
            "Moog Bass", "Moog Lead", "Prophet Bass", "Prophet Brass", "Juno Pads", "Juno Strings", "Jupiter Brass", "Jupiter Pads",
            "Oberheim Brass", "Oberheim Strings", "ARP Sequences", "MS-20 Aggressive", "Analog Plucks", "Vintage Keys", "Vintage Leads",
            "Classic Bass", "Classic Strings", "Classic Brass", "Poly Synth", "Unison Synth", "Analog FX" };
        return f;
    }

    const std::vector<std::string>& tipusAtzar()
    {
        static const std::vector<std::string> t { "Baix", "Lead", "Pad", "Metalls", "Cordes", "Tecles", "Pluck", "Seqüència", "FX", "Poli" };
        return t;
    }

    namespace
    {
        struct P { const char* fam; const char* cat; const char* nom; const char* desc; const char* canvis; };
        // envolupants i maneres que es repeteixen
        #define BAIX " mode=1 veus=1 aa=1 ad=500 as=0.7 ar=90 fa=1 fd=320 fs=0.12"
        #define SOLO " mode=1 veus=1 aa=4 ad=700 as=0.85 ar=220"
        #define PLUCK " aa=1 ad=420 as=0 ar=380 fa=1 fd=300 fs=0"
        #define METALL " aa=55 ad=900 as=0.75 ar=380 fa=110 fd=800 fs=0.45"
        #define PAD " aa=900 ad=2200 as=0.85 ar=2400 fa=1300 fd=2600 fs=0.55"
        #define CORDA " aa=380 ad=1500 as=0.9 ar=1300 fa=450 fd=1600 fs=0.7"
        #define MOOG " ft=0 fp=1 ader=0.3 acal=0.45 asat=0.3"
        #define PROPHET " ft=1 fp=1 ader=0.35 aedat=0.2 afil=0.25"
        #define JUNO " ft=1 fp=1 ader=0.04 ains=0.04 afil=0.1 aest=0.35"
        #define JUPITER " ft=1 fp=1 ader=0.2 afil=0.2 aest=0.4"
        #define OBERHEIM " ft=2 ader=0.4 afil=0.35 aest=0.55 acal=0.4"
        #define ARP " ft=1 fp=1 ader=0.3 ains=0.25"
        #define MS20 " ft=3 ader=0.3 asat=0.4"
        #define CS80 " ft=1 fp=0 ader=0.3 aest=0.5 atc=0.5"
        const P taula[] = {
            // ================= Moog Bass =================
            { "Moog Bass", "Bass", "Model 72", "Tres serres a 16' i 32' pel Ladder amb el mesclador saturat: el baix gros i rodó per excel·lència.",
              BAIX MOOG " o1o=-1 o2o=-1 o2f=7 o2n=0.75 o3o=-2 o3w=1 o3n=0.45 cut=320 res=0.22 fenv=0.55 drv=0.4" " fkey=0.4 vol=2" },
            { "Moog Bass", "Bass", "Funk de goma", "Quadrada i serra amb ressonància alta i una caiguda curta del filtre: el «bwow» del funk dels 70.",
              BAIX MOOG " o1w=1 o1o=-1 o2o=-1 o2n=0.6 o2f=-5 cut=260 res=0.55 fenv=0.7 fd=180 fs=0 drv=0.3" " ad=380 as=0.6 vol=4" },
            { "Moog Bass", "Bass", "Pedalers", "Serra a 32' amb una caiguda llarga i el filtre que es tanca lent: els pedals de baix de les bandes de rock progressiu.",
              MOOG " o1o=-2 o2o=-1 o2n=0.5 o2f=4 cut=420 res=0.15 fenv=0.45 fd=1600 fs=0.25 aa=2 ad=2500 as=0.4 ar=900 mode=1 veus=1" " vol=2" },
            { "Moog Bass", "Bass", "Subgreu suau", "Triangle i sinus a 16' i 32', filtre gairebé tancat: un greu net que omple sense molestar.",
              BAIX MOOG " o1w=2 o1o=-1 o2w=3 o2o=-2 o2n=0.8 cut=500 res=0.05 fenv=0.15 drv=0.1" " as=0.9 vol=1" },
            { "Moog Bass", "Bass", "Baix saturat", "Les tres serres empeses al màxim dins el filtre: dur, amb el gruny que talla per sota d'una guitarra.",
              BAIX MOOG " o1o=-1 o2o=-1 o2f=9 o2n=0.8 o3o=0 o3n=0.3 cut=650 res=0.3 fenv=0.45 drv=0.9 asat=0.7" " vol=4.5" },
            { "Moog Bass", "Bass", "Pinçat greu", "Filtre que s'obre i es tanca de cop amb ressonància: el baix «pluck» que fa groove.",
              BAIX MOOG " o1w=1 o1o=-1 o2o=-1 o2n=0.5 cut=200 res=0.42 fenv=0.75 fd=140 fs=0 ad=320 as=0" " vol=7.5" },
            { "Moog Bass", "Bass", "Lliscant", "Legato amb glide: les notes lligades llisquen sense tornar a atacar, com les línies de baix del G-funk.",
              BAIX MOOG " o1w=4 o1pw=0.3 o1o=-1 o2o=-2 o2n=0.5 cut=380 res=0.35 fenv=0.4 glide=110" " mode=1 vol=2" },
            { "Moog Bass", "Bass", "Gruny de l'osc 3", "L'oscil·lador 3 a ritme d'àudio mou el filtre (font «Osc 3»): un baix que grunyi amb harmònics que bateguen.",
              BAIX MOOG " o1o=-1 o2o=-1 o2n=0.6 o2f=5 o3o=-1 o3s=7 o3w=2 lf=1 lcut=0.25 cut=420 res=0.3 fenv=0.4" " vol=2" },

            // ================= Moog Lead =================
            { "Moog Lead", "Lead", "Solista amb glide", "Tres serres (8', 8' i 4') amb portament i vibrato a la roda: el lead de les balades de rock dels 70.",
              SOLO MOOG " o2f=6 o2n=0.7 o3o=1 o3n=0.35 cut=1800 res=0.25 fenv=0.4 fd=900 fs=0.5 glide=120 rto=0.45" " dlm=0.18 dlt=5 rvm=0.15 vol=1" },
            { "Moog Lead", "Lead", "Quadrada solista", "Quadrada i serra a 4' amb el filtre mig obert: un solo amb cos que no fa mal a les orelles.",
              SOLO MOOG " o1w=1 o2o=1 o2n=0.4 cut=2200 res=0.2 fenv=0.3 glide=50 lret=500 lto=0.15" " vol=0" },
            { "Moog Lead", "Lead", "Quintes progressives", "Oscil·lador 2 una quinta amunt: el solo de teclat del rock progressiu, gran i solemne.",
              SOLO MOOG " o2s=7 o2n=0.65 o3o=-1 o3n=0.35 cut=1600 res=0.3 fenv=0.45 fd=1200 fs=0.55 glide=80" " rvm=0.2 rvt=3 vol=2" },
            { "Moog Lead", "Lead", "Xiulet", "Triangle amb el Ladder a punt d'oscil·lar sol: un lead fi i cantant, gairebé una flauta elèctrica.",
              SOLO MOOG " o1w=2 cut=1300 res=0.82 fenv=0.2 fkey=1 glide=70 lret=400 lto=0.12" " dlm=0.15 dlt=3 vol=4" },
            { "Moog Lead", "Lead", "Theremin de teclat", "Sinus amb glide llarg i vibrato que entra a poc a poc: l'aire de les pel·lícules de ciència-ficció antigues.",
              SOLO MOOG " o1w=3 cut=6000 res=0 fenv=0 glide=320 lret=700 lto=0.3 lr=5.5 aa=60 ar=500" " rvm=0.3 rvt=1 vol=0" },
            { "Moog Lead", "Lead", "Funk espacial", "Quadrada estreta amb glide i molta ressonància: els leads que cantaven els grups de funk dels 70.",
              SOLO MOOG " o1w=4 o1pw=0.22 o2w=0 o2n=0.4 o2f=-8 cut=900 res=0.55 fenv=0.55 fd=400 fs=0.3 glide=140" " vol=3.5" },
            { "Moog Lead", "Lead", "Cel obert", "Serra brillant amb delay amb punt i reverb de sala: un lead que vola per sobre de la banda.",
              SOLO MOOG " o2f=-7 o2n=0.55 cut=3500 res=0.15 fenv=0.25 glide=40 lret=300 lto=0.1" " dlm=0.3 dlt=3 dlf=0.45 rvm=0.25 rvt=3 vol=-0.5" },
            { "Moog Lead", "Lead", "Gruny de solo", "Drive al filtre i l'osc 3 desafinat: un solo brut, a la frontera del rock dur.",
              SOLO MOOG " o2n=0.7 o2f=12 o3o=-1 o3n=0.4 cut=2400 res=0.35 fenv=0.3 drv=0.75 asat=0.6 glide=60" " vol=3.5" },

            // ================= Prophet Bass =================
            { "Prophet Bass", "Bass", "Cinc veus de baix", "Serra i pols pel filtre OTA de 24 dB, net i amb cop: el baix polifònic de les produccions dels 80.",
              BAIX PROPHET " o1o=-1 o2w=4 o2pw=0.3 o2o=-1 o2n=0.6 o2f=6 cut=500 res=0.2 fenv=0.5 fd=260 fs=0.15" " vol=6" },
            { "Prophet Bass", "Bass", "Polymod greu", "L'oscil·lador 3 modula la freqüència de l'1 (FM): un baix amb harmònics de campana a l'atac.",
              BAIX PROPHET " o1o=-1 o3o=-1 o3s=12 o3w=2 fm=0.18 o2o=-1 o2n=0.5 cut=600 res=0.25 fenv=0.45 fd=220 fs=0.1" " vol=4" },
            { "Prophet Bass", "Bass", "Sync greu", "Osc 2 sincronitzat amb l'1 i l'envolupant que li puja el to: l'atac metàl·lic i agressiu del sync.",
              BAIX PROPHET " o1o=-1 o1n=0.3 o2o=-1 o2n=0.8 sync=1 env2=14 cut=1200 res=0.15 fenv=0.35 fd=200 fs=0.1" " vol=4.5" },
            { "Prophet Bass", "Bass", "Baix de vellut", "Dues serres lleugerament desafinades amb poc filtre: suau i càlid per a balades.",
              BAIX PROPHET " o1o=-1 o2o=-1 o2f=8 o2n=0.7 cut=380 res=0.08 fenv=0.25 fd=600 fs=0.4 acal=0.5" " vol=4" },
            { "Prophet Bass", "Bass", "Pols de baix", "Pols estret amb PWM lent: un baix nasal que es mou sol.",
              BAIX PROPHET " o1w=4 o1pw=0.18 o1o=-1 lpw=0.3 lr=0.4 o2w=1 o2o=-2 o2n=0.4 cut=700 res=0.2 fenv=0.4" " vol=5" },
            { "Prophet Bass", "Bass", "Cinc a l'uníson", "Mode mono amb 5 veus apilades i desafinades: el baix enorme dels sintes polifònics en uníson.",
              BAIX PROPHET " o1o=-1 o2o=-1 o2n=0.6 uni=5 unid=0.35 cut=550 res=0.25 fenv=0.5 fd=300 fs=0.2" " vol=-5.5" },
            { "Prophet Bass", "Bass", "Pas de new wave", "Quadrada a 16' amb la caiguda justa per a línies de corxeres: el baix sintètic de la new wave.",
              BAIX PROPHET " o1w=1 o1o=-1 o2o=-1 o2n=0.4 o2f=-6 cut=700 res=0.3 fenv=0.45 fd=200 fs=0.05 ad=260 as=0.3 ar=60" " vol=5.5" },

            // ================= Prophet Brass =================
            { "Prophet Brass", "Brass", "Metalls de cinc", "Dues serres desafinades i el filtre que puja amb l'atac: els metalls sintètics polifònics clàssics.",
              METALL PROPHET " o2f=8 o2n=0.8 cut=900 res=0.15 fenv=0.5" " ch=1 rvm=0.15 rvt=1 vol=-2" },
            { "Prophet Brass", "Brass", "Metalls suaus", "Atac lent i filtre mig tancat: metalls de fons que acompanyen sense imposar-se.",
              METALL PROPHET " o2f=-6 o2n=0.7 cut=700 res=0.1 fenv=0.35 aa=180 fa=300" " ch=1 rvm=0.2 rvt=1 vol=-1" },
            { "Prophet Brass", "Brass", "Metalls amb sync", "El sync de l'osc 2 amb l'envolupant: metalls brillants i agressius per a tornades de rock.",
              METALL PROPHET " o2n=0.7 sync=1 env2=7 cut=1500 res=0.15 fenv=0.4" " vol=-0.5" },
            { "Prophet Brass", "Brass", "Cops de metall", "Atac sec i caiguda curta: els acords curts de metall (stabs) de la música dels 80.",
              PROPHET " o2f=9 o2n=0.8 cut=1100 res=0.2 fenv=0.6 aa=8 ad=350 as=0.25 ar=200 fa=10 fd=250 fs=0.2" " vol=1.5" },
            { "Prophet Brass", "Brass", "Metalls de cinema", "Tres oscil·ladors (un a 16') i reverb de sala: fanfàries grans per a bandes sonores.",
              METALL PROPHET " o2f=7 o2n=0.75 o3o=-1 o3n=0.45 cut=1000 res=0.12 fenv=0.45 aa=120" " rvm=0.3 rvt=3 rvs=0.7 vol=-1.5" },
            { "Prophet Brass", "Brass", "Metalls de pols", "Pols ample amb PWM: metalls més buits, com de saxo sintètic.",
              METALL PROPHET " o1w=4 o1pw=0.35 o2w=4 o2pw=0.42 o2n=0.7 o2f=6 lpw=0.25 lr=0.8 cut=1200 res=0.1 fenv=0.4" " vol=-3.5" },
            { "Prophet Brass", "Brass", "Metalls polymod", "Una mica d'FM de l'osc 3 a l'1: l'atac de metall agafa un brillant de llautó.",
              METALL PROPHET " o3o=1 o3w=3 fm=0.12 o2f=8 o2n=0.75 cut=1000 res=0.15 fenv=0.5" " ch=1 vol=-2.5" },

            // ================= Juno Pads =================
            { "Juno Pads", "Pad", "Pad de Juno", "Pols amb PWM i sub a −1 octava pel chorus II: el pad càlid i ample de la música pop dels 80.",
              PAD JUNO " o1w=4 o1pw=0.3 lpw=0.55 lr=0.6 subn=0.5 cut=2200 res=0.12 fenv=0.15 hpf=60" " ch=2 vol=-2" },
            { "Juno Pads", "Pad", "Somni de PWM", "Dos polsos amb PWM a velocitats diferents i chorus I: un llit que respira.",
              PAD JUNO " o1w=4 o1pw=0.25 o2w=4 o2pw=0.4 o2n=0.6 o2f=4 lpw=0.6 lr=0.35 cut=1800 res=0.1 fenv=0.2" " ch=1 rvm=0.25 rvt=1 vol=-1" },
            { "Juno Pads", "Pad", "Cor de sub", "Serra suau i sub de sinus, filtre fosc: un cor greu per sota dels acords.",
              PAD JUNO " o1n=0.6 subn=0.7 subw=1 cut=900 res=0.15 fenv=0.25 hpf=40" " ch=2 rvm=0.2 rvt=3 vol=-2.5" },
            { "Juno Pads", "Pad", "Pad d'aire", "Pols, soroll rosa i HPF alt: un pad que és sobretot aire, per a intros.",
              PAD JUNO " o1w=4 o1pw=0.35 lpw=0.4 lr=0.5 nzn=0.35 nzc=1 cut=3500 res=0.2 fenv=0.1 hpf=350" " ch=2 rvm=0.35 rvt=3 rvs=0.7 vol=1.5" },
            { "Juno Pads", "Pad", "Pad brillant", "Serra i pols amb el filtre obert i chorus I+II: un pad lluminós de synth-pop.",
              PAD JUNO " o2w=4 o2pw=0.3 o2n=0.6 lpw=0.4 lr=0.7 cut=5000 res=0.1 fenv=0.1 hpf=120" " aa=400 ch=3 vol=3" },
            { "Juno Pads", "Pad", "Pad que creix", "El filtre s'obre en quatre segons amb l'envolupant: entrades lentes per a crescendos.",
              PAD JUNO " o1w=4 o1pw=0.3 lpw=0.5 lr=0.5 subn=0.4 cut=300 res=0.2 fenv=0.7 fa=4000 fd=3000 fs=0.8 hpf=60" " ch=2 rvm=0.2 vol=-2" },
            { "Juno Pads", "Pad", "Pad polifònic", "Serra i sub amb un atac més ràpid: acords de pad per tocar amb ritme.",
              JUNO " subn=0.45 cut=2600 res=0.15 fenv=0.25 aa=120 ad=1500 as=0.8 ar=900 hpf=80" " ch=1 vol=0.5" },
            { "Juno Pads", "Pad", "Pad nocturn", "Pols estret i filtre baix amb molta reverb de sala: un pad fosc per a moments íntims.",
              PAD JUNO " o1w=4 o1pw=0.15 lpw=0.25 lr=0.25 cut=800 res=0.2 fenv=0.15 hpf=50" " ch=2 rvm=0.4 rvt=3 rvs=0.8 vol=-0.5" },

            // ================= Juno Strings =================
            { "Juno Strings", "Strings", "Cordes de Juno", "Serra i pols amb chorus II i atac lent: les cordes sintètiques del pop d'abans.",
              CORDA JUNO " o2w=4 o2pw=0.35 o2n=0.5 lpw=0.4 lr=0.6 cut=3200 res=0.05 fenv=0.1 hpf=160" " ch=2 vol=0.5" },
            { "Juno Strings", "Strings", "Conjunt lleuger", "Serra sola, filtre obert i chorus I+II: un ensemble fi que no tapa la veu.",
              CORDA JUNO " cut=4500 res=0.05 fenv=0.05 hpf=250" " ch=3 vol=6" },
            { "Juno Strings", "Strings", "Cordes a l'octava", "Osc 2 a 4': cordes que sonen una octava amunt, com uns violins.",
              CORDA JUNO " o2o=1 o2n=0.55 o2f=3 cut=3800 res=0.05 fenv=0.1 hpf=200" " ch=2 rvm=0.2 rvt=1 vol=-1" },
            { "Juno Strings", "Strings", "Arc", "L'atac del filtre una mica més lent que el del volum: la nota entra com un arc sobre la corda.",
              CORDA JUNO " o2w=4 o2pw=0.3 o2n=0.5 lpw=0.3 lr=0.5 cut=1200 res=0.1 fenv=0.45 fa=700 fd=1200 fs=0.6 hpf=120 lret=900 lto=0.1" " ch=2 vol=-0.5" },
            { "Juno Strings", "Strings", "Cordes desafinades", "Dues serres desafinades: més gruix i més moviment sense chorus.",
              CORDA JUNO " o2f=12 o2n=0.8 cut=3000 res=0.05 fenv=0.1 hpf=150" " ader=0.15 vol=0.5" },
            { "Juno Strings", "Strings", "Cordes de disco", "Atac ràpid i alliberament curt: els gargots de cordes de la música disco.",
              JUNO " o2w=4 o2pw=0.4 o2n=0.5 cut=4000 res=0.1 fenv=0.25 aa=60 ad=800 as=0.85 ar=400 fa=60 hpf=180" " ch=3 vol=3" },
            { "Juno Strings", "Strings", "Cordes de balada", "Atac molt lent, filtre tou i reverb de placa: el coixí de les balades.",
              CORDA JUNO " cut=1800 res=0.05 fenv=0.1 aa=900 ar=2000 hpf=100" " ch=2 rvm=0.3 rvt=1 vol=-0.5" },

            // ================= Jupiter Brass =================
            { "Jupiter Brass", "Brass", "Metalls estel·lars", "Dues serres amb l'OTA i un atac de filtre llampant: els metalls grans dels polifònics de gamma alta.",
              METALL JUPITER " o2f=10 o2n=0.85 cut=1100 res=0.18 fenv=0.55" " rvm=0.15 rvt=1 vol=-1" },
            { "Jupiter Brass", "Brass", "Metalls creuats", "Modulació creuada (FM 3 → 1) suau: un metall amb un fil de brillantor metàl·lica.",
              METALL JUPITER " o3w=3 o3s=12 fm=0.1 o2f=7 o2n=0.75 cut=1200 res=0.15 fenv=0.5" " vol=-1.5" },
            { "Jupiter Brass", "Brass", "Metalls amb sync brillant", "Sync i envolupant al to de l'osc 2: metalls que obren com una trompeta.",
              METALL JUPITER " o2n=0.8 sync=1 env2=5 cut=2000 res=0.1 fenv=0.35" " vol=-1" },
            { "Jupiter Brass", "Brass", "Metalls a l'uníson", "Dues veus per nota i un toc de chorus: un mur de metall.",
              METALL JUPITER " o2f=8 o2n=0.7 uni=2 unid=0.25 cut=1000 res=0.15 fenv=0.5" " ch=1 vol=-5.5" },
            { "Jupiter Brass", "Brass", "Trompes suaus", "Triangle i serra amb poc filtre: com unes trompes, rodones i llunyanes.",
              METALL JUPITER " o1w=2 o2n=0.6 o2f=5 cut=700 res=0.05 fenv=0.4 aa=120 fa=200" " rvm=0.3 rvt=3 vol=0.5" },
            { "Jupiter Brass", "Brass", "Fanfàrria", "Tres oscil·ladors (amb una quinta) i atac ràpid: obertures i fanfàrries.",
              METALL JUPITER " o2f=6 o2n=0.75 o3s=7 o3n=0.4 cut=1400 res=0.15 fenv=0.5 aa=25" " rvm=0.25 rvt=3 vol=-1" },
            { "Jupiter Brass", "Brass", "Metalls de pop", "Atac ràpid, caiguda mitjana i chorus: els riffs de metall dels èxits dels 80.",
              JUPITER " o2f=9 o2n=0.8 cut=1500 res=0.2 fenv=0.45 aa=15 ad=600 as=0.5 fa=20 fd=500 fs=0.35" " ch=1 vol=-2" },

            // ================= Jupiter Pads =================
            { "Jupiter Pads", "Pad", "Pad estel·lar", "Dues serres desafinades amb atac lent i placa: el pad ample i sedós dels polifònics grans.",
              PAD JUPITER " o2f=9 o2n=0.8 cut=1600 res=0.12 fenv=0.25" " ch=1 rvm=0.3 rvt=1 vol=-4.5" },
            { "Jupiter Pads", "Pad", "Pad creuat", "FM suau de l'osc 3 a l'1: un pad amb una brillantor de vidre a dins.",
              PAD JUPITER " o3w=3 o3o=1 fm=0.15 o2w=4 o2pw=0.35 o2n=0.6 cut=2200 res=0.1 fenv=0.2" " ch=1 rvm=0.3 rvt=1 vol=-1.5" },
            { "Jupiter Pads", "Pad", "Pad de vidre", "Sync lleu amb el filtre obert: un pad cristal·lí.",
              PAD JUPITER " o1n=0.4 o2n=0.7 o2s=12 sync=1 cut=4000 res=0.1 fenv=0.1" " rvm=0.35 rvt=1 vol=-2" },
            { "Jupiter Pads", "Pad", "Pad doble", "Serra i quadrada a l'octava i molta variació estèreo: un pad que envolta.",
              PAD JUPITER " o2w=1 o2o=-1 o2n=0.5 cut=1400 res=0.1 fenv=0.25 aest=0.8" " ch=2 vol=-2.5" },
            { "Jupiter Pads", "Pad", "Capvespre", "Filtre fosc que s'obre molt a poc a poc amb un LFO lent: un pad que canvia de color.",
              PAD JUPITER " o2f=7 o2n=0.75 cut=700 res=0.2 fenv=0.2 lcut=0.25 lr=0.12" " rvm=0.3 rvt=3 vol=-1" },
            { "Jupiter Pads", "Pad", "Pad de placa", "Pols i serra amb una cua llarga de reverb de placa: per a finals de cançó.",
              PAD JUPITER " o1w=4 o1pw=0.3 lpw=0.3 lr=0.4 o2n=0.6 o2f=6 cut=2000 res=0.1 fenv=0.15" " rvm=0.45 rvt=1 rvs=0.85 vol=-5.5" },
            { "Jupiter Pads", "Pad", "Pad pulsant", "El volum bombeja a corxeres amb el tempo: un pad rítmic.",
              PAD JUPITER " o2f=8 o2n=0.7 cut=1800 res=0.15 fenv=0.2 aa=200 lw=2 ls=4 lvol=0.7" " ch=1 vol=-0.5" },

            // ================= Oberheim Brass =================
            { "Oberheim Brass", "Brass", "Metalls SEM", "Dues serres molt desafinades pel filtre SEM de 12 dB: metalls oberts, grossos i amb molta presència.",
              METALL OBERHEIM " o2f=14 o2n=0.85 cut=1300 res=0.25 fenv=0.45" " vol=-7.5" },
            { "Oberheim Brass", "Brass", "Acords de salt", "Atac sec, sosteniment alt i un punt de ressonància: els acords de metall que obren cançons de rock.",
              OBERHEIM " o2f=12 o2n=0.85 cut=1800 res=0.3 fenv=0.35 aa=5 ad=500 as=0.8 ar=250 fa=5 fd=400 fs=0.5" " vol=-7.5" },
            { "Oberheim Brass", "Brass", "Metalls amples", "Dues veus per nota i molta variació estèreo: el so de paret de les màquines de quatre veus.",
              METALL OBERHEIM " o2f=10 o2n=0.8 uni=2 unid=0.3 cut=1200 res=0.25 fenv=0.45 aest=0.8" " vol=-10.5" },
            { "Oberheim Brass", "Brass", "Fanfàrria SEM", "Una quinta amb l'osc 3 i reverb de sala: fanfàrries brillants.",
              METALL OBERHEIM " o2f=9 o2n=0.75 o3s=7 o3n=0.45 cut=1500 res=0.2 fenv=0.4 aa=30" " rvm=0.25 rvt=3 vol=-7" },
            { "Oberheim Brass", "Brass", "Metalls en banda", "El SEM en passa-banda: metalls nasals, com amb sordina.",
              METALL OBERHEIM " o2f=10 o2n=0.8 fmo=1 cut=1100 res=0.35 fenv=0.35" " vol=6.5" },
            { "Oberheim Brass", "Brass", "Metalls buits", "Quadrada i pols pel SEM: un metall més buit, com de clarinet gran.",
              METALL OBERHEIM " o1w=1 o2w=4 o2pw=0.3 o2n=0.75 o2f=8 cut=1400 res=0.2 fenv=0.4" " vol=-9" },
            { "Oberheim Brass", "Brass", "Metalls que pugen", "Atac d'un segon i el filtre que s'obre al mateix temps: metalls que entren en crescendo.",
              OBERHEIM " o2f=12 o2n=0.85 cut=500 res=0.2 fenv=0.65 aa=1000 fa=1300 fd=1500 fs=0.6" " ad=1000 as=0.85 ar=600 rvm=0.2 vol=-8.5" },

            // ================= Oberheim Strings =================
            { "Oberheim Strings", "Strings", "Cordes SEM", "Dues serres pel SEM amb atac lent: cordes riques i obertes, amb molta vida.",
              CORDA OBERHEIM " o2f=11 o2n=0.8 cut=2600 res=0.1 fenv=0.15" " rvm=0.2 rvt=1 vol=-7.5" },
            { "Oberheim Strings", "Strings", "Cordes en notch", "El SEM en notch: cordes amb un forat al mig, que sonen com si hi hagués un phaser quiet.",
              CORDA OBERHEIM " o2f=10 o2n=0.8 fmo=3 cut=1400 res=0.3 fenv=0.1 lcut=0.15 lr=0.15" " vol=-7" },
            { "Oberheim Strings", "Strings", "Cordes exuberants", "Tres oscil·ladors desafinats i chorus: cordes enormes per a tornades.",
              CORDA OBERHEIM " o2f=12 o2n=0.75 o3f=-9 o3n=0.6 cut=2800 res=0.1 fenv=0.15" " ch=1 rvm=0.25 rvt=3 vol=-8" },
            { "Oberheim Strings", "Strings", "Cordes de cor", "Pols amb PWM i SEM en passa-banda suau: cordes amb un aire de veus.",
              CORDA OBERHEIM " o1w=4 o1pw=0.3 lpw=0.4 lr=0.5 o2n=0.6 o2f=8 fmo=1 cut=1500 res=0.25 fenv=0.1" " rvm=0.3 rvt=3 vol=3" },
            { "Oberheim Strings", "Strings", "Orquestra analògica", "Serra a 8' i 16' amb vibrato retardat: una secció de cordes sintètica completa.",
              CORDA OBERHEIM " o2o=-1 o2n=0.6 o2f=7 cut=2200 res=0.1 fenv=0.15 lret=1000 lto=0.12 lr=5" " rvm=0.25 rvt=3 vol=-6" },
            { "Oberheim Strings", "Strings", "Cordes de cinema", "Atac molt lent i reverb de sala gran: per sota de les escenes llargues.",
              CORDA OBERHEIM " o2f=9 o2n=0.8 cut=1600 res=0.1 fenv=0.2 aa=1500 ar=3000" " rvm=0.4 rvt=3 rvs=0.85 vol=-8.5" },
            { "Oberheim Strings", "Strings", "Cordes primes", "El SEM en passa-alts: cordes fines que només ocupen els aguts.",
              CORDA OBERHEIM " o2f=10 o2n=0.8 fmo=2 cut=600 res=0.15 fenv=0.1" " ch=1 vol=0.5" },

            // ================= ARP Sequences =================
            { "ARP Sequences", "Sequences", "Seqüència berlinesa", "Patró berlinès a semicorxeres amb el filtre que respira: les seqüències hipnòtiques de l'electrònica dels 70.",
              PLUCK ARP " o1w=1 o2n=0.5 o2f=6 cut=700 res=0.45 fenv=0.5 lcut=0.2 lr=0.08" " ad=250 fd=180 mode=1 veus=1 arp=7 arpp=2 arpr=3 arpg=0.6 dlm=0.25 dlt=3 vol=7.5" },
            { "ARP Sequences", "Sequences", "Mostreig i retenció", "L'LFO «atzar» mou el filtre ressonant a semicorxeres: les bombolles clàssiques del sample & hold.",
              ARP " o2n=0.6 o2f=7 cut=900 res=0.65 fenv=0.1 lw=5 ls=5 lcut=0.55 aa=2 as=1 ar=300" " mode=1 veus=1 rvm=0.2 rvt=0 vol=1.5" },
            { "ARP Sequences", "Sequences", "Pols amb sync", "Seqüència d'octaves amb el sync escombrant: brillant i mecànica.",
              PLUCK ARP " o1n=0.3 o2n=0.8 sync=1 env2=10 cut=1800 res=0.2 fenv=0.4" " ad=200 mode=1 veus=1 arp=7 arpp=0 arpr=3 arpg=0.5 dlm=0.2 dlt=3 vol=9" },
            { "ARP Sequences", "Sequences", "Arpegi d'octaves", "Arpegiador amunt per dues octaves: els arpegis de les bandes sonores dels 80.",
              PLUCK ARP " o1w=4 o1pw=0.3 cut=1500 res=0.3 fenv=0.45" " arp=1 arpo=2 arpr=3 arpg=0.5 dlm=0.25 dlt=3 rvm=0.2 vol=4.5" },
            { "ARP Sequences", "Sequences", "Seqüència d'anell", "Ring de l'osc 1 i el 2 una quinta amunt: seqüències metàl·liques i estranyes.",
              PLUCK ARP " o1n=0.3 o2s=7 o2n=0.2 ring=0.8 cut=2500 res=0.25 fenv=0.3" " mode=1 veus=1 arp=7 arpp=10 arpr=3 dlm=0.2 dlt=2 vol=9.5" },
            { "ARP Sequences", "Sequences", "Pulsació hipnòtica", "Patró de pulsació a semicorxeres amb el filtre que s'obre lent: l'arrencada de mil temes d'electrònica.",
              PLUCK ARP " o1o=-1 o2o=-1 o2n=0.6 cut=400 res=0.5 fenv=0.4 lcut=0.35 lw=1 lr=0.06" " ad=180 fd=150 mode=1 veus=1 arp=7 arpp=10 arpr=3 arpg=0.4 vol=8.5" },
            { "ARP Sequences", "Sequences", "Arpegi de disco", "Arpegi amunt-avall a semicorxeres amb chorus: el pols de la música disco electrònica.",
              PLUCK ARP " o2f=8 o2n=0.6 cut=2200 res=0.2 fenv=0.35" " ad=300 arp=3 arpo=2 arpr=3 arpg=0.45 ch=1 vol=5" },
            { "ARP Sequences", "Sequences", "Galop", "Patró de galop a semicorxeres: ritme de cavall per a electrònica fosca.",
              PLUCK ARP " o1w=1 o1o=-1 o2n=0.5 cut=600 res=0.4 fenv=0.55" " ad=200 mode=1 veus=1 arp=7 arpp=3 arpr=3 arpg=0.5 dlm=0.15 dlt=3 vol=7.5" },

            // ================= MS-20 Aggressive =================
            { "MS-20 Aggressive", "Lead", "Crit", "Filtre MS-20 al límit de l'oscil·lació: un lead que crida i esquinça.",
              SOLO MS20 " o2n=0.6 o2f=10 cut=1600 res=0.85 fenv=0.4 drv=0.5" " glide=40 vol=1.5" },
            { "MS-20 Aggressive", "Bass", "Baix que grunyi", "Serra i quadrada a 16' amb el MS-20 saturat: un baix brut i viu.",
              BAIX MS20 " o1o=-1 o2w=1 o2o=-1 o2n=0.6 cut=500 res=0.6 fenv=0.5 drv=0.6" " vol=7" },
            { "MS-20 Aggressive", "Lead", "Passa-alts esquinçat", "El MS-20 en passa-alts amb ressonància: un so prim i que talla, com una guitarra amb fuzz.",
              SOLO MS20 " o1w=4 o1pw=0.2 o2n=0.6 fmo=2 cut=500 res=0.7 fenv=0.3 drv=0.7" " vol=4" },
            { "MS-20 Aggressive", "FX", "Vent ressonant", "Soroll blanc pel MS-20 en passa-banda amb l'LFO lent: vent i xiulets.",
              MS20 " o1n=0 nzn=1 fmo=1 cut=900 res=0.8 fenv=0 lcut=0.5 lr=0.15 aa=800 as=1 ar=2000" " rvm=0.35 rvt=3 vol=6" },
            { "MS-20 Aggressive", "Bass", "Lladruc", "Caiguda molt curta del filtre ressonant: un baix que lladra a cada nota.",
              BAIX MS20 " o1o=-1 o2o=-1 o2n=0.5 cut=250 res=0.75 fenv=0.75 fd=120 fs=0 drv=0.5" " ad=250 as=0.4 vol=9" },
            { "MS-20 Aggressive", "Sequences", "Seqüència industrial", "Patró tecno amb el MS-20 saturat i delay: electrònica dura i metàl·lica.",
              PLUCK MS20 " o1w=1 o1o=-1 o2n=0.6 o2s=7 cut=800 res=0.6 fenv=0.5 drv=0.8" " ad=180 mode=1 veus=1 arp=7 arpp=12 arpr=3 arpg=0.4 dlm=0.2 dlt=3 vol=9" },
            { "MS-20 Aggressive", "Lead", "Esgarip", "Pols estret, ressonància a dalt i vibrato ràpid: un lead que xiscla.",
              SOLO MS20 " o1w=4 o1pw=0.12 cut=2200 res=0.78 fenv=0.25 lto=0.2 lr=6.5 drv=0.4" " vol=4.5" },
            { "MS-20 Aggressive", "FX", "Udol", "El filtre oscil·la sol i l'LFO el mou amunt i avall: udols i sirenes.",
              MS20 " o1n=0.15 nzn=0.2 cut=700 res=0.98 fenv=0.2 lcut=0.6 lr=0.5 lw=1 aa=300 as=1 ar=1500" " rvm=0.3 rvt=0 vol=3" },

            // ================= Analog Plucks =================
            { "Analog Plucks", "Plucks", "Pluck clàssic", "Serra amb el filtre que s'obre i es tanca de cop: el pluck de sempre, per a arpegis i riffs.",
              PLUCK PROPHET " o2f=6 o2n=0.6 cut=600 res=0.25 fenv=0.65" " dlm=0.2 dlt=3 rvm=0.15 vol=3" },
            { "Analog Plucks", "Plucks", "Pluck de Juno", "Pols amb sub i chorus: plucks rodons i amples.",
              PLUCK JUNO " o1w=4 o1pw=0.3 subn=0.4 cut=900 res=0.2 fenv=0.55" " ch=2 vol=1.5" },
            { "Analog Plucks", "Plucks", "Pluck de Ladder", "El Ladder amb ressonància: un pluck gras i dolç.",
              PLUCK MOOG " o1w=1 o2n=0.5 o2f=-5 cut=400 res=0.45 fenv=0.6 drv=0.3" " vol=1" },
            { "Analog Plucks", "Plucks", "Pluck de vidre", "Sinus i triangle a l'octava amb el filtre obert: plucks clars com de marimba de vidre.",
              PLUCK PROPHET " o1w=3 o2w=2 o2o=1 o2n=0.5 cut=8000 res=0 fenv=0.1" " ad=600 ar=500 rvm=0.25 rvt=1 vol=2" },
            { "Analog Plucks", "Plucks", "Arpa analògica", "Triangle i serra suau amb caiguda llarga: una arpa sintètica.",
              PLUCK PROPHET " o1w=2 o2n=0.35 cut=2500 res=0.1 fenv=0.4 ad=1200 ar=900 fd=500" " rvm=0.3 rvt=1 vol=2" },
            { "Analog Plucks", "Plucks", "Quadrada pinçada", "Quadrada amb el SEM: un pluck buit i simpàtic, com un videojoc antic però càlid.",
              PLUCK OBERHEIM " o1w=1 cut=1500 res=0.2 fenv=0.5" " ad=300 vol=-5" },
            { "Analog Plucks", "Plucks", "Pizzicato", "Serra amb una caiguda molt curta i poc filtre: cordes pinçades sintètiques.",
              PLUCK JUPITER " o2f=8 o2n=0.6 cut=2200 res=0.05 fenv=0.4 ad=180 ar=150 fd=150" " rvm=0.2 rvt=2 vol=5.5" },
            { "Analog Plucks", "Plucks", "Calimba", "Sinus amb FM de l'osc 3 a l'octava i mitja: un pluck de làmines metàl·liques.",
              PLUCK PROPHET " o1w=3 o3w=3 o3o=1 o3s=7 fm=0.25 cut=6000 res=0 fenv=0.1 ad=700 ar=600" " rvm=0.2 rvt=2 vol=1" },

            // ================= Vintage Keys =================
            { "Vintage Keys", "Keys", "Piano analògic", "Serra i triangle amb caiguda de piano i filtre que segueix la força: un piano sintètic dels 70.",
              PROPHET " o1w=2 o2n=0.5 cut=1800 res=0.05 fenv=0.4 fvel=0.6 aa=2 ad=2200 as=0 ar=500 fa=1 fd=1200 fs=0" " vv=0.7 ch=1 vol=-0.5" },
            { "Vintage Keys", "Keys", "Clavi sintètic", "Pols molt estret en passa-banda: el so percussiu i funky d'un clavinet.",
              PROPHET " o1w=4 o1pw=0.16 fmo=2 cut=500 res=0.3 drv=0.5 fenv=0.4 aa=1 ad=500 as=0.2 ar=120 fa=1 fd=250 fs=0" " vv=0.6 phm=0.3 phr=0.6 vol=8.5" },
            { "Vintage Keys", "Keys", "Orgue de peus", "Quadrades a 8', 4' i 16' sense filtre ni envolupant: un orgue de transistors.",
              JUNO " o1w=1 o2w=1 o2o=1 o2n=0.5 o3w=1 o3o=-1 o3n=0.5 cut=6000 res=0 fenv=0 aa=2 ad=100 as=1 ar=60" " ch=3 vol=-2" },
            { "Vintage Keys", "Keys", "Tecles de campana", "Sinus amb FM inharmònica: tecles amb atac de campana, com un piano elèctric de làmines.",
              PROPHET " o1w=3 o3w=3 o3o=1 o3s=2 fm=0.3 o2w=2 o2n=0.4 cut=5000 fenv=0.1 aa=1 ad=2500 as=0.1 ar=700" " ch=1 vol=-3" },
            { "Vintage Keys", "Keys", "Tecles polifòniques", "Serra i pols amb una caiguda mitjana: acords de teclat per tocar amb ritme.",
              JUPITER " o2w=4 o2pw=0.3 o2n=0.6 cut=2000 res=0.15 fenv=0.35 aa=2 ad=1200 as=0.4 ar=400 fa=1 fd=700 fs=0.2" " ch=1 vol=-0.5" },
            { "Vintage Keys", "Keys", "Piano de joguina", "Triangle a 4' amb una mica de deriva: un piano de joguina tendre.",
              PROPHET " o1w=2 o1o=1 o2w=1 o2o=1 o2n=0.2 cut=4000 fenv=0.1 aa=1 ad=700 as=0 ar=300 ader=0.5" " rvm=0.2 rvt=2 vol=1.5" },
            { "Vintage Keys", "Keys", "Wurli sintètic", "Triangle i pols amb saturació i trèmolo: com un piano elèctric de lengüeta.",
              PROPHET " o1w=2 o2w=4 o2pw=0.25 o2n=0.35 cut=1600 fenv=0.3 fvel=0.5 aa=1 ad=1800 as=0.2 ar=400 lvol=0.25 lr=5 asat=0.7" " vv=0.6 vol=1.5" },
            { "Vintage Keys", "Keys", "Tecles d'aftertouch", "La força i l'aftertouch obren el filtre (com al CS-80): tecles molt expressives.",
              CS80 " o2w=4 o2pw=0.35 o2n=0.6 cut=700 res=0.15 fenv=0.3 fvel=0.8 atc=0.8 aa=5 ad=1500 as=0.6 ar=600" " vv=0.8 ch=1 vol=-1.5" },

            // ================= Vintage Leads =================
            { "Vintage Leads", "Lead", "Lead d'Odissea", "Serra i quadrada pel filtre de 24 dB amb ressonància: el lead brillant dels duofònics dels 70.",
              SOLO ARP " o2w=1 o2n=0.6 o2f=-6 cut=2400 res=0.35 fenv=0.3 glide=30" " dlm=0.15 dlt=3 vol=2.5" },
            { "Vintage Leads", "Lead", "Lead de sync", "Sync amb l'envolupant que puja el to de l'osc 2: el solo que talla com un ganivet.",
              SOLO PROPHET " o1n=0.3 o2n=0.85 sync=1 env2=12 fd=600 fs=0.3 cut=3000 res=0.15 fenv=0.2 glide=30" " dlm=0.2 dlt=3 vol=1.5" },
            { "Vintage Leads", "Lead", "Lead de PWM", "Pols amb PWM ràpid: un lead nasal i viu.",
              SOLO JUNO " o1w=4 o1pw=0.25 lpw=0.5 lr=4 cut=2000 res=0.25 fenv=0.25" " ch=1 vol=1" },
            { "Vintage Leads", "Lead", "Lead d'anell", "Ring amb l'osc 2 desafinat: un lead metàl·lic de ciència-ficció.",
              SOLO ARP " o1n=0.4 o2s=5 o2n=0.2 ring=0.7 cut=3000 res=0.2 fenv=0.2" " dlm=0.25 dlt=3 rvm=0.2 vol=3.5" },
            { "Vintage Leads", "Lead", "Serra lliscant", "Serra amb portament llarg i vibrato a la roda: per a melodies lentes.",
              SOLO JUPITER " o2f=8 o2n=0.6 cut=2800 res=0.2 fenv=0.2 glide=250 rto=0.5" " rvm=0.25 rvt=3 vol=4" },
            { "Vintage Leads", "Lead", "Lead de cos", "Pressió (aftertouch) al filtre i vibrato retardat: un lead que respon com un instrument de vent.",
              SOLO CS80 " o2n=0.6 o2f=6 cut=1200 res=0.2 fenv=0.25 atc=0.9 lret=500 lto=0.15" " vol=0.5" },
            { "Vintage Leads", "Lead", "Lead galàctic", "FM de l'osc 3 a ritme d'LFO lent (osc 3 lent): un lead que trontolla com un platet volador.",
              SOLO ARP " o3lo=1 o3w=2 o3o=1 lf=1 lto=0.5 o2w=1 o2n=0.5 cut=2500 res=0.3 fenv=0.2 glide=60" " dlm=0.3 dlt=3 dlf=0.5 vol=4" },

            // ================= Classic Bass =================
            { "Classic Bass", "Bass", "Quadrada sub", "Quadrada a 16' i sub de sinus: un baix rodó que funciona a tot arreu.",
              BAIX JUNO " o1w=1 o1o=-1 subn=0.6 subw=1 cut=600 res=0.1 fenv=0.3" " vol=3.5" },
            { "Classic Bass", "Bass", "Serra de baix", "Una serra a 16' amb el filtre mig obert: el baix sintètic més directe.",
              BAIX PROPHET " o1o=-1 cut=800 res=0.2 fenv=0.4" " vol=5" },
            { "Classic Bass", "Bass", "Baix de Juno", "Pols amb sub a −1 octava i HPF tancat: el baix de les cançons de synth-pop.",
              BAIX JUNO " o1w=4 o1pw=0.35 o1o=-1 subn=0.7 cut=900 res=0.15 fenv=0.35" " vol=4.5" },
            { "Classic Bass", "Bass", "Octaves de disco", "Arpegiador d'octaves a corxeres: la línia de baix saltant de la música disco.",
              BAIX PROPHET " o1o=-1 o2o=-1 o2n=0.5 o2f=5 cut=700 res=0.25 fenv=0.45 ad=250 as=0.3" " arp=7 arpp=0 arpr=1 arpg=0.55 vol=6.5" },
            { "Classic Bass", "Bass", "Baix pinçat", "Caiguda curta i sense sosteniment: un baix per a línies ràpides.",
              BAIX OBERHEIM " o1w=1 o1o=-1 o2o=-1 o2n=0.5 cut=500 res=0.2 fenv=0.6 ad=300 as=0 ar=100 fd=180 fs=0" " vol=1.5" },
            { "Classic Bass", "Bass", "Baix ressonant", "Ressonància alta i el filtre que segueix el teclat: el baix «acid» d'abans de l'acid.",
              BAIX MOOG " o1o=-1 cut=350 res=0.7 fenv=0.55 fkey=0.8" " vol=6.5" },
            { "Classic Bass", "Bass", "Baix rodó", "Triangle i serra pel SEM: un greu suau i mullat.",
              BAIX OBERHEIM " o1w=2 o1o=-1 o2o=-1 o2n=0.4 cut=600 res=0.15 fenv=0.3" " vol=-1.5" },
            { "Classic Bass", "Bass", "Baix fosc", "Serra a 32' amb el filtre tancat i saturació: un subgreu amenaçador per a cinema.",
              BAIX MOOG " o1o=-2 o2o=-1 o2n=0.4 cut=180 res=0.25 fenv=0.25 asat=0.6 drv=0.5" " ar=400 vol=4.5" },

            // ================= Classic Strings =================
            { "Classic Strings", "Strings", "Ensemble", "Serres amb el chorus I+II ràpid: l'ensemble de cordes de les màquines dels 70.",
              CORDA JUNO " o2o=1 o2n=0.5 cut=4500 res=0.05 fenv=0 hpf=200" " ch=3 rvm=0.2 rvt=1 vol=1.5" },
            { "Classic Strings", "Strings", "Cordes expressives", "La força i l'aftertouch obren les cordes (com al CS-80): per tocar amb sentiment.",
              CORDA CS80 " o2f=9 o2n=0.75 cut=900 res=0.1 fenv=0.3 fvel=0.6 atc=0.8 lret=800 lto=0.1" " vv=0.6 rvm=0.3 rvt=3 vol=-3" },
            { "Classic Strings", "Strings", "Cordes lentes", "Atac de dos segons: cordes que apareixen de no-res.",
              CORDA JUPITER " o2f=8 o2n=0.75 cut=2000 res=0.05 fenv=0.1 aa=2000 ar=2500" " rvm=0.35 rvt=3 vol=-1.5" },
            { "Classic Strings", "Strings", "Cordes agudes", "Les dues serres a 4': violins sintètics per a contramelodies.",
              CORDA JUPITER " o1o=1 o2o=1 o2f=7 o2n=0.75 cut=4000 res=0.05 fenv=0.1 lret=700 lto=0.1" " ch=1 vol=-3" },
            { "Classic Strings", "Strings", "Violins de síntesi", "Vibrato que entra tard i un atac de filtre una mica lent: com un violí que s'acosta.",
              CORDA PROPHET " o2f=6 o2n=0.6 cut=1500 res=0.15 fenv=0.35 fa=500 lret=1200 lto=0.18 lr=5.5" " rvm=0.25 rvt=2 vol=-1" },
            { "Classic Strings", "Strings", "Cordes de cambra", "Una mica de passa-alts i reverb de cambra: cordes íntimes.",
              CORDA OBERHEIM " o2f=7 o2n=0.7 cut=2400 res=0.05 fenv=0.1 hpf=300" " rvm=0.3 rvt=2 vol=-4" },
            { "Classic Strings", "Strings", "Cordes amples", "Dues veus per nota i molta variació estèreo: cordes que omplen tot el panorama.",
              CORDA JUPITER " o2f=10 o2n=0.7 uni=2 unid=0.35 cut=2600 res=0.05 fenv=0.1 aest=0.9" " ch=1 vol=-5.5" },

            // ================= Classic Brass =================
            { "Classic Brass", "Brass", "Metalls sintètics", "Dues serres i l'envolupant del filtre amb un punt d'atac: els metalls sintètics de manual.",
              METALL PROPHET " o2f=7 o2n=0.8 cut=1000 res=0.15 fenv=0.5" " vol=-1" },
            { "Classic Brass", "Brass", "Pad de metall", "Atac lent i sosteniment alt: un metall que fa de pad.",
              METALL JUPITER " o2f=8 o2n=0.75 cut=800 res=0.1 fenv=0.35 aa=500 fa=600 ar=1200" " ch=1 rvm=0.25 rvt=1 vol=-1" },
            { "Classic Brass", "Brass", "Secció de metalls", "Tres oscil·ladors desafinats: una secció sencera de metalls.",
              METALL OBERHEIM " o2f=9 o2n=0.75 o3f=-8 o3n=0.6 cut=1200 res=0.15 fenv=0.45" " vol=-8" },
            { "Classic Brass", "Brass", "Metalls expressius", "Aftertouch al filtre i vibrato: metalls que creixen quan prems la tecla.",
              METALL CS80 " o2f=8 o2n=0.75 cut=800 res=0.15 fenv=0.4 atc=0.9 rto=0.3" " rvm=0.2 rvt=3 vol=-3" },
            { "Classic Brass", "Brass", "Metalls de dos polsos", "Pols a dos amples diferents: metalls més nasals, a mig camí d'un saxo.",
              METALL JUNO " o1w=4 o1pw=0.3 o2w=4 o2pw=0.45 o2n=0.75 o2f=7 cut=1300 res=0.15 fenv=0.4" " ch=1 vol=-3.5" },
            { "Classic Brass", "Brass", "Cop brillant", "Atac i caiguda curts amb el filtre molt obert: stabs que brillen.",
              JUPITER " o2f=9 o2n=0.8 cut=2500 res=0.2 fenv=0.4 aa=3 ad=300 as=0.2 ar=180 fa=3 fd=250 fs=0.1" " rvm=0.15 rvt=1 vol=2.5" },
            { "Classic Brass", "Brass", "Fanfàrria clàssica", "Quinta amb l'osc 3 i reverb de sala: obertures solemnes.",
              METALL PROPHET " o2f=6 o2n=0.75 o3s=7 o3n=0.4 o3o=-1 cut=1300 res=0.12 fenv=0.45 aa=40" " rvm=0.3 rvt=3 vol=-0.5" },

            // ================= Poly Synth =================
            { "Poly Synth", "Poly Synth", "Cinc veus", "Dues serres desafinades amb l'OTA: l'acord polifònic de referència, per a qualsevol cançó.",
              PROPHET " o2f=8 o2n=0.75 cut=1800 res=0.15 fenv=0.35 aa=5 ad=1500 as=0.65 ar=500 fa=5 fd=900 fs=0.4" " ch=1 vol=-2.5" },
            { "Poly Synth", "Poly Synth", "Serra polifònica", "Una serra sola amb el filtre obert: brillant i directa.",
              JUPITER " cut=3500 res=0.1 fenv=0.2 aa=3 ad=1000 as=0.7 ar=400" " vol=0" },
            { "Poly Synth", "Poly Synth", "Polifonia desafinada", "Tres serres molt desafinades: un acord ample i una mica vell.",
              OBERHEIM " o2f=14 o2n=0.75 o3f=-12 o3n=0.6 cut=2200 res=0.1 fenv=0.25 aa=5 ad=1200 as=0.7 ar=600 aedat=0.5" " vol=-7.5" },
            { "Poly Synth", "Poly Synth", "Polifonia amb chorus", "Pols amb PWM i chorus II: acords amples sense esforç.",
              JUNO " o1w=4 o1pw=0.3 lpw=0.45 lr=0.6 subn=0.3 cut=2500 res=0.1 fenv=0.25 aa=5 ad=1500 as=0.7 ar=600" " ch=2 vol=-2" },
            { "Poly Synth", "Poly Synth", "Polifonia amb sync", "Sync i envolupant al to: acords brillants amb un atac que mossega.",
              PROPHET " o1n=0.35 o2n=0.75 sync=1 env2=8 fd=400 fs=0.2 cut=3000 res=0.1 fenv=0.25 aa=3 ad=1000 as=0.6 ar=500" " vol=0.5" },
            { "Poly Synth", "Poly Synth", "Quadrades polifòniques", "Quadrades a 8' i 4' pel SEM: un acord buit i net.",
              OBERHEIM " o1w=1 o2w=1 o2o=1 o2n=0.5 cut=2000 res=0.15 fenv=0.25 aa=5 ad=1200 as=0.7 ar=500" " ch=1 vol=-9.5" },
            { "Poly Synth", "Poly Synth", "Stabs", "Atac sec i caiguda molt curta: acords curts per a ritmes.",
              JUPITER " o2f=8 o2n=0.75 cut=1500 res=0.2 fenv=0.55 aa=1 ad=250 as=0 ar=200 fa=1 fd=200 fs=0" " rvm=0.2 rvt=1 vol=4.5" },
            { "Poly Synth", "Poly Synth", "Polifonia buida", "Triangle i pols en passa-banda: acords amb un so de fusta.",
              PROPHET " o1w=2 o2w=4 o2pw=0.2 o2n=0.6 fmo=1 cut=1200 res=0.3 fenv=0.3 aa=3 ad=1200 as=0.5 ar=500" " ch=1 vol=8" },

            // ================= Unison Synth =================
            { "Unison Synth", "Lead", "Lead a l'uníson", "Mode mono amb 6 veus apilades: un lead enorme que ho omple tot.",
              SOLO PROPHET " o2n=0.6 uni=6 unid=0.4 cut=2500 res=0.2 fenv=0.3 glide=50" " dlm=0.15 dlt=3 vol=-7.5" },
            { "Unison Synth", "Bass", "Baix a l'uníson", "Quatre veus apilades a 16': un baix ample i amenaçador.",
              BAIX OBERHEIM " o1o=-1 o2o=-1 o2n=0.5 uni=4 unid=0.3 cut=500 res=0.2 fenv=0.45" " vol=-10.5" },
            { "Unison Synth", "Poly Synth", "Supersaw dels 70", "Tres veus per nota amb dues serres cadascuna: el mur de serres abans de les màquines digitals.",
              JUPITER " o2f=9 o2n=0.75 uni=3 unid=0.45 cut=3500 res=0.1 fenv=0.15 aa=10 ad=1500 as=0.8 ar=600" " ch=1 vol=-7.5" },
            { "Unison Synth", "Poly Synth", "Cop a l'uníson", "Stabs amb dues veus per nota: grossos i amples.",
              PROPHET " o2f=7 o2n=0.7 uni=2 unid=0.35 cut=1800 res=0.2 fenv=0.5 aa=1 ad=300 as=0 ar=200 fa=1 fd=250 fs=0" " rvm=0.2 rvt=1 vol=0" },
            { "Unison Synth", "Lead", "Monstre mono", "Vuit veus a l'uníson pel Ladder saturat: el so més gros que pot fer el VINTAGE.",
              SOLO MOOG " o2n=0.6 o2o=-1 uni=8 unid=0.35 cut=1500 res=0.25 fenv=0.35 drv=0.6 glide=80" " vol=-8.5" },
            { "Unison Synth", "Lead", "Sync a l'uníson", "Quatre veus amb sync i l'envolupant al to: agressiu i gegant.",
              SOLO PROPHET " o1n=0.3 o2n=0.8 sync=1 env2=10 fd=500 fs=0.3 uni=4 unid=0.3 cut=3000 res=0.15 fenv=0.2" " vol=-5.5" },
            { "Unison Synth", "Pad", "Pad a l'uníson", "Dues veus per nota amb atac lent i chorus: un pad gruixut.",
              PAD JUNO " o1w=4 o1pw=0.3 lpw=0.4 lr=0.5 uni=2 unid=0.4 cut=1800 res=0.1 fenv=0.2" " ch=1 rvm=0.3 rvt=3 vol=-8" },
            { "Unison Synth", "Brass", "Metalls a l'uníson amples", "Tres veus per nota pel SEM i molta variació estèreo: metalls de pel·lícula.",
              METALL OBERHEIM " o2f=8 o2n=0.75 uni=3 unid=0.3 cut=1300 res=0.2 fenv=0.45 aest=0.9" " vol=-12" },

            // ================= Analog FX =================
            { "Analog FX", "FX", "Làser", "L'osc 2 sol amb l'envolupant del filtre baixant-li el to de cop: trets de làser.",
              PROPHET " o1n=0 o2n=0.8 o2o=1 env2=24 fa=1 fd=180 fs=0 cut=6000 res=0.2 fenv=0.1 aa=1 ad=300 as=0 ar=100" " dlm=0.25 dlt=2 vol=4.5" },
            { "Analog FX", "FX", "Vent", "Soroll rosa en passa-banda amb l'LFO lent: vent que va i ve.",
              OBERHEIM " o1n=0 nzn=1 nzc=1 fmo=1 cut=800 res=0.6 fenv=0 lcut=0.5 lr=0.1 lw=1 aa=1500 as=1 ar=3000" " rvm=0.3 rvt=3 vol=2.5" },
            { "Analog FX", "FX", "Bombolles", "Mostreig i retenció ràpid al filtre ressonant: bombolles de laboratori.",
              MOOG " o1w=2 cut=1200 res=0.8 fenv=0 lw=5 lr=9 lcut=0.6 aa=5 as=1 ar=500" " mode=1 veus=1 rvm=0.25 rvt=0 vol=2.5" },
            { "Analog FX", "FX", "Sirena", "Vibrato de set semitons a velocitat lenta: una sirena antiga.",
              PROPHET " o1w=4 o1pw=0.3 o2n=0.5 o2f=10 lto=7 lr=0.35 lw=1 cut=3000 res=0.2 fenv=0 aa=200 as=1 ar=800" " mode=1 veus=1 vol=2.5" },
            { "Analog FX", "FX", "Onades", "Soroll blanc pel Ladder amb un LFO molt lent al tall i al volum: el mar.",
              MOOG " o1n=0 nzn=1 cut=1800 res=0.1 fenv=0 lcut=0.6 lvol=0.35 lr=0.08 lw=1 aa=2000 as=1 ar=4000" " rvm=0.35 rvt=3 rvs=0.8 vol=12" },
            { "Analog FX", "FX", "Robot", "Ring entre l'osc 1 i un osc 2 desafinat amb una mica de soroll: veus de robot de pel·lícula.",
              ARP " o1w=1 o1n=0.2 o2s=-5 o2f=20 o2n=0.2 ring=1 nzn=0.1 cut=2500 res=0.3 fenv=0.2 aa=5 as=1 ar=200" " vol=-1.5" },
            { "Analog FX", "FX", "Tro", "Soroll blanc a sota de tot, filtre tancat i molta saturació: un tro llunyà.",
              MOOG " o1n=0 nzn=1 cut=320 res=0.3 fenv=0.5 fa=400 fd=3000 fs=0 aa=300 ad=4000 as=0 ar=3000 asat=0.8 drv=0.8" " rvm=0.4 rvt=3 rvs=0.9 vol=12" },
            { "Analog FX", "FX", "Platet volador", "L'osc 3 lent modula la FM i el to: un platet volador que se'n va.",
              ARP " o1w=3 o3lo=1 o3w=2 o3o=2 fm=0.5 lf=1 lto=3 cut=5000 res=0.2 fenv=0 aa=500 as=1 ar=1500" " mode=1 veus=1 dlm=0.3 dlt=5 dlf=0.6 rvm=0.3 vol=-1" },
        };
        #undef BAIX
        #undef SOLO
        #undef PLUCK
        #undef METALL
        #undef PAD
        #undef CORDA
        #undef MOOG
        #undef PROPHET
        #undef JUNO
        #undef JUPITER
        #undef OBERHEIM
        #undef ARP
        #undef MS20
        #undef CS80

        void aplica (Vintage& s, const char* canvis)
        {
            std::istringstream is (canvis);
            std::string tok;
            while (is >> tok)
            {
                const auto ig = tok.find ('=');
                if (ig == std::string::npos) continue;
                const auto clau = tok.substr (0, ig);
                const float v = std::strtof (tok.c_str() + ig + 1, nullptr);
                for (int p = 0; p < numParams; ++p)
                    if (clau == info (p).clau) { s.posa (p, v); break; }
            }
        }

        struct Atzar
        {
            std::uint32_t s;
            explicit Atzar (unsigned l) : s (l * 2654435761u + 911u) { if (s == 0) s = 1; }
            float u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 16777216.0f; }
            float r (float a, float b) { return a + (b - a) * u(); }
            float lg (float a, float b) { return a * std::pow (b / a, u()); }
            bool p (float prob) { return u() < prob; }
            int i (int a, int b) { return a + (int) (u() * (float) (b - a + 1)) % (b - a + 1); }
        };
    }

    std::string estatInicial() { auto s = std::make_unique<Vintage>(); return s->estatSo(); }

    std::string construeix (const std::string& canvis)
    {
        auto s = std::make_unique<Vintage>();
        aplica (*s, canvis.c_str());
        return s->estatSo();
    }

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> v = []
        {
            std::vector<Preset> r;
            for (const auto& x : taula) r.push_back ({ x.nom, x.cat, std::string (x.fam) + " · " + x.desc, construeix (x.canvis) });
            return r;
        }();
        return v;
    }

    std::string aleatori (unsigned llavor, int tipus)
    {
        Atzar a (llavor);
        if (tipus < 0 || tipus >= (int) tipusAtzar().size()) tipus = a.i (0, (int) tipusAtzar().size() - 1);
        std::ostringstream c;
        // la família: el filtre i el caràcter
        const int ft = a.i (0, 3);
        c << " ft=" << ft << " fp=" << (a.p (0.8f) ? 1 : 0) << " ader=" << a.r (0.05f, 0.45f) << " afil=" << a.r (0.05f, 0.4f)
          << " aest=" << a.r (0.1f, 0.7f) << " acal=" << a.r (0.1f, 0.5f);
        // oscil·ladors
        c << " o1w=" << a.i (0, 4) << " o1pw=" << a.r (0.1f, 0.5f);
        if (a.p (0.75f)) c << " o2w=" << a.i (0, 4) << " o2n=" << a.r (0.4f, 0.85f) << " o2f=" << a.r (-14.0f, 14.0f) << (a.p (0.25f) ? " o2s=7" : "") << (a.p (0.2f) ? " o2o=1" : "");
        if (a.p (0.3f)) c << " o3w=" << a.i (0, 3) << " o3n=" << a.r (0.2f, 0.5f) << " o3o=" << a.i (-1, 0) << " o3f=" << a.r (-10.0f, 10.0f);
        if (a.p (0.15f)) c << " sync=1 env2=" << a.r (4.0f, 16.0f);
        if (a.p (0.12f)) c << " fm=" << a.r (0.05f, 0.25f) << " o3w=3 o3o=" << a.i (0, 1);
        if (a.p (0.2f)) c << " lpw=" << a.r (0.2f, 0.6f) << " lr=" << a.lg (0.2f, 1.5f);
        switch (tipus)
        {
            case 0: c << " mode=1 veus=1 o1o=-1 o2o=-1 aa=1 ad=" << a.lg (200.0f, 900.0f) << " as=" << a.r (0.2f, 0.8f) << " ar=90 fa=1 fd=" << a.lg (120.0f, 600.0f)
                      << " fs=" << a.r (0.0f, 0.3f) << " cut=" << a.lg (200.0f, 900.0f) << " res=" << a.r (0.1f, 0.6f) << " fenv=" << a.r (0.3f, 0.7f) << " drv=" << a.r (0.1f, 0.6f)
                      << (a.p (0.4f) ? " subn=0.5" : ""); break;
            case 1: c << " mode=1 veus=1 aa=4 ad=700 as=0.85 ar=220 glide=" << a.lg (20.0f, 200.0f) << " cut=" << a.lg (1200.0f, 4000.0f) << " res=" << a.r (0.1f, 0.5f)
                      << " fenv=" << a.r (0.1f, 0.4f) << " lret=500 lto=0.12 dlm=" << a.r (0.1f, 0.3f) << " dlt=3"; break;
            case 2: c << " aa=" << a.lg (400.0f, 2000.0f) << " ad=2200 as=0.85 ar=" << a.lg (1200.0f, 3500.0f) << " fa=1200 fd=2500 fs=0.55 cut=" << a.lg (700.0f, 3000.0f)
                      << " res=0.12 fenv=0.25 ch=" << a.i (0, 2) << " rvm=" << a.r (0.2f, 0.4f) << " rvt=" << a.i (1, 3); break;
            case 3: c << " aa=" << a.lg (20.0f, 150.0f) << " ad=900 as=0.75 ar=380 fa=110 fd=800 fs=0.45 cut=" << a.lg (700.0f, 1500.0f) << " res=0.15 fenv=" << a.r (0.4f, 0.6f)
                      << " o1w=0 o2w=0 o2n=0.8"; break;
            case 4: c << " aa=" << a.lg (250.0f, 1500.0f) << " ad=1500 as=0.9 ar=1300 fa=450 fd=1600 fs=0.7 cut=" << a.lg (1500.0f, 4500.0f) << " res=0.05 fenv=0.1 hpf="
                      << a.lg (60.0f, 300.0f) << " ch=" << a.i (1, 3); break;
            case 5: c << " aa=2 ad=" << a.lg (800.0f, 2500.0f) << " as=" << a.r (0.0f, 0.5f) << " ar=500 fa=1 fd=900 fs=0.2 fvel=0.5 cut=" << a.lg (1000.0f, 3000.0f) << " fenv=0.35 ch=1"; break;
            case 6: c << " aa=1 ad=" << a.lg (200.0f, 900.0f) << " as=0 ar=380 fa=1 fd=" << a.lg (150.0f, 500.0f) << " fs=0 cut=" << a.lg (400.0f, 1500.0f) << " res=" << a.r (0.1f, 0.45f)
                      << " fenv=" << a.r (0.4f, 0.7f) << " dlm=0.2 dlt=3"; break;
            case 7: c << " mode=1 veus=1 aa=1 ad=" << a.lg (150.0f, 400.0f) << " as=0 ar=200 fa=1 fd=200 fs=0 cut=" << a.lg (400.0f, 1200.0f) << " res=" << a.r (0.2f, 0.6f)
                      << " fenv=0.5 arp=7 arpp=" << a.i (0, 15) << " arpr=3 arpg=0.5 dlm=0.2 dlt=3"; break;
            case 8: c << " o1n=" << a.r (0.0f, 0.5f) << " nzn=" << a.r (0.3f, 1.0f) << " fmo=" << a.i (0, 1) << " cut=" << a.lg (300.0f, 3000.0f) << " res=" << a.r (0.4f, 0.9f)
                      << " lw=" << a.i (0, 5) << " lr=" << a.lg (0.1f, 8.0f) << " lcut=" << a.r (0.3f, 0.7f) << " aa=" << a.lg (5.0f, 1500.0f) << " as=1 ar=1500 rvm=0.3"; break;
            default: c << " aa=5 ad=" << a.lg (600.0f, 2000.0f) << " as=0.65 ar=500 fa=5 fd=900 fs=0.4 cut=" << a.lg (1200.0f, 4000.0f) << " res=0.15 fenv=0.3 ch=" << a.i (0, 2); break;
        }
        auto s = std::make_unique<Vintage>();
        aplica (*s, c.str().c_str());
        // volum aproximat segons l'energia
        float e = s->valor (O1Niv) + s->valor (O2Niv) + s->valor (O3Niv) + 0.5f * s->valor (SubNiv) + 0.3f * s->valor (NzNiv);
        e *= std::sqrt ((float) std::max (1, (int) s->valor (Unison)));
        const bool poli = (int) s->valor (Mode) == 2;
        s->posa (Volum, std::clamp (-6.0f - 6.0f * std::log2 (std::max (0.5f, e)) - (poli ? 4.0f : 0.0f) - 3.0f * s->valor (FRes), -20.0f, 0.0f));
        return s->estatSo();
    }

    std::string varia (const std::string& estat, unsigned llavor, float quant)
    {
        Atzar a (llavor);
        auto sp = std::make_unique<Vintage>();
        auto& s = *sp;
        s.posaEstat (estat);
        for (int p = 0; p < numParams; ++p)
        {
            const auto& i = info (p);
            if (i.escala == sinte::Enter || p == Volum || p == BendRang || p >= Mode) continue;
            if (std::abs (s.valor (p)) < 1e-6f) continue;   // el que està apagat es queda apagat
            float salt = 0.15f * quant;
            if (p == O1Fi || p == O2Fi || p == O3Fi) salt *= 0.4f;
            s.posa (p, sinte::deNormal (i, sinte::normal (i, s.valor (p)) + (a.u() * 2.0f - 1.0f) * salt));
        }
        return s.estatSo();
    }
}
