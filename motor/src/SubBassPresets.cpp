#include "mibase/SubBass.h"
#include <cmath>
#include <cstdlib>
#include <memory>
#include <sstream>

/** 0.88: els presets de fàbrica del SUB & BASS. Cada un és un so fet amb la síntesi pròpia de Mibase, inspirat en
    famílies de sintetitzadors clàssics (sense copiar cap preset comercial). S'escriuen com a canvis respecte del
    so inicial: «clau=valor» (les claus són les de la taula de paràmetres de SubBass.cpp). */
namespace mibase::subbass
{
    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c {
            "Pure Sub", "Deep Sine", "Clean Synth Bass", "Analog Bass", "Moog Style Bass", "Acid Bass", "303 Style Bass",
            "Reese Bass", "Distorted Bass", "Growl Bass", "FM Bass", "Metallic Bass", "Rubber Bass", "Funk Bass",
            "Pluck Bass", "Sequence Bass", "Indie Rock Bass", "Modern Pop Bass", "Synthwave Bass", "Cinematic Bass", "Estils" };
        return c;
    }

    namespace
    {
        struct P { const char* cat; const char* nom; const char* desc; const char* canvis; };
        const P taula[] = {
            // ---- Pure Sub ----
            { "Pure Sub", "Sub pur", "Un sinus net, sense filtre ni harmònics: només greu. Per sota d'un baix elèctric o d'un kick.",
              "o1w=0 o1n=0.95 subn=0 cut=18000 fenv=0 key=0 res=0 fdrv=0 aa=2 as=1 ar=60 vv=0.2 vol=-4.5" },
            { "Pure Sub", "Sub 808", "Sinus amb caiguda llarga i un cop de to a l'atac, com una caixa de ritmes clàssica.",
              "o1w=0 o1n=0.95 subn=0 cut=18000 fenv=0 res=0 fdrv=0 pe=12 pet=45 as=0 ad=1800 ar=400 clic=0.12 vv=0.3 vol=-3.0" },
            { "Pure Sub", "Sub triangle", "Triangle filtrat: una mica més present que el sinus als altaveus petits.",
              "o1w=1 o1n=0.9 subn=0 cut=900 fenv=0 res=0 key=0.5 fdrv=0 as=1 ar=80 vol=-0.5" },
            { "Pure Sub", "Sub amb pes", "Sinus i sub una octava avall amb un punt de vàlvula perquè se senti als mòbils.",
              "o1w=0 o1n=0.8 subw=0 subn=0.55 cut=18000 fenv=0 res=0 dt=0 dq=0.3 dto=0.4 as=1 ar=90 vol=-9.5" },
            { "Pure Sub", "Sub legato", "Sub pur amb glide entre notes lligades: per a línies de baix que llisquen.",
              "o1w=0 o1n=0.95 subn=0 cut=18000 fenv=0 res=0 fdrv=0 mode=1 glide=90 as=1 ar=70 vol=-3.5" },
            // ---- Deep Sine ----
            { "Deep Sine", "Sinus profund", "Sinus amb sub i una mica de saturació: greu rodó i ple.",
              "o1w=0 subw=0 subn=0.4 cut=18000 fenv=0 res=0 dq=0.18 as=0.9 ar=200 vol=-8.0" },
            { "Deep Sine", "Sinus amb clic", "Sinus amb clic d'atac: es retalla davant del kick.",
              "o1w=0 clic=0.35 subn=0.3 cut=18000 fenv=0 res=0 as=0.85 ad=600 ar=120 vol=-5.5" },
            { "Deep Sine", "Sinus calent", "Sinus passat per vàlvula fosca: harmònics suaus i càlids.",
              "o1w=0 dt=0 dq=0.5 dto=0.3 subn=0.35 cut=18000 fenv=0 res=0 as=0.9 vol=-6.5" },
            { "Deep Sine", "Sinus i quinta", "Sinus amb una quinta molt baixa a sobre: greu amb color.",
              "o1w=0 o2w=0 o2s=7 o2f=0 o2n=0.22 cut=18000 fenv=0 res=0 subn=0.3 as=0.9 vol=-5.5" },
            { "Deep Sine", "Sinus que cau", "Cada nota comença una quinta amunt i cau al to: efecte de caiguda.",
              "o1w=0 pe=7 pet=140 subn=0.4 cut=18000 fenv=0 res=0 as=0.8 ad=900 vol=-5.5" },
            // ---- Clean Synth Bass ----
            { "Clean Synth Bass", "Serra neta", "La serra clàssica amb el filtre a mig obrir: baix de sinte polivalent.",
              "cut=1400 res=0.1 fenv=0.3 fd=300 fs=0.35 subn=0.35 vol=-1.0" },
            { "Clean Synth Bass", "Quadrada neta", "Quadrada amb filtre suau: buida i rodona.",
              "o1w=3 cut=1100 res=0.15 fenv=0.35 subn=0.4 vol=-6.0" },
            { "Clean Synth Bass", "Pols net", "Pols estret: nasal i definit.",
              "o1w=4 o1p=0.3 cut=1600 res=0.12 fenv=0.3 subn=0.4 vol=-5.5" },
            { "Clean Synth Bass", "Net i rodó", "SVF de 24 dB, serra i quadrada una octava avall: net i ple.",
              "flt=1 pend=1 cut=700 res=0.05 fenv=0.25 o2w=3 o2o=-1 o2f=0 o2n=0.4 subn=0.2 vol=-5.5" },
            { "Clean Synth Bass", "Net brillant", "Filtre obert i seguiment de teclat: les notes agudes brillen.",
              "cut=3000 res=0.25 fenv=0.2 key=0.6 subn=0.4 as=0.7 vol=0.0" },
            // ---- Analog Bass ----
            { "Analog Bass", "Analògic clàssic", "Dues serres desafinades amb fase lliure: cada nota una mica diferent.",
              "o2n=0.6 o2f=9 cut=600 res=0.3 fenv=0.55 fd=350 fs=0.25 lliure=1 subn=0.35 vol=-3.0" },
            { "Analog Bass", "Analògic gruixut", "Serra, quadrada una octava avall i sub, amb el filtre saturat.",
              "o2w=3 o2o=-1 o2f=4 o2n=0.5 subn=0.5 cut=500 res=0.25 fenv=0.6 fdrv=0.4 lliure=1 vol=-8.5" },
            { "Analog Bass", "Analògic tou", "Atac lent i filtre tancat: un coixí de greus.",
              "aa=9 cut=380 res=0.1 fenv=0.4 fa=25 fd=600 fs=0.3 lliure=1 o2n=0.4 o2f=6 ar=250 vol=-4.0" },
            { "Analog Bass", "Analògic amb pols", "Pols amb PWM lenta: es mou sol.",
              "o1w=4 o1p=0.35 o2n=0.4 o2f=8 ld=2 lq=0.3 lr=0.6 lret=0 lliure=1 cut=800 fenv=0.4 vol=-4.0" },
            { "Analog Bass", "Analògic àgil", "Envolupants ràpides i molta envolupant de filtre: per a notes curtes.",
              "ad=220 as=0.4 fd=150 fs=0.05 fenv=0.7 cut=350 res=0.35 lliure=1 o2n=0.5 o2f=11 vol=-0.5" },
            // ---- Moog Style Bass ----
            { "Moog Style Bass", "Ladder clàssic", "Tres oscil·ladors (serra, serra, triangle avall) pel ladder de 24 dB saturat.",
              "o2n=0.7 o2f=-8 subw=1 subn=0.5 subf=1 cut=450 res=0.35 fenv=0.6 fd=400 fs=0.3 fdrv=0.5 lliure=1 vol=-7.5" },
            { "Moog Style Bass", "Ladder rodó", "Quadrada per un ladder tancat i saturat: el greu gras de sempre.",
              "o1w=3 cut=300 res=0.2 fenv=0.65 fd=600 fs=0.4 fdrv=0.6 lliure=1 subn=0.4 subf=1 vol=-9.5" },
            { "Moog Style Bass", "Ladder saturat", "El filtre i la vàlvula a fons, com un mono modern empès.",
              "fdrv=0.85 dt=0 dq=0.35 dpos=0 cut=650 res=0.45 fenv=0.5 o2n=0.6 o2f=-6 lliure=1 vol=-9.5" },
            { "Moog Style Bass", "Ladder legato", "Mono legato amb glide: les notes lligades llisquen sense tornar a atacar.",
              "mode=1 glide=85 cut=520 res=0.3 fenv=0.45 o2n=0.6 o2f=7 fdrv=0.4 lliure=1 vol=-7.0" },
            { "Moog Style Bass", "Ladder ressonant", "Ressonància alta i filtre que segueix el teclat: canta.",
              "cut=900 res=0.68 fenv=0.25 key=0.7 fdrv=0.5 o1w=3 lliure=1 subn=0.4 vol=-7.0" },
            // ---- Acid Bass ----
            { "Acid Bass", "Àcid clàssic", "Serra pel filtre de díodes amb molta ressonància i glide: «squelch».",
              "flt=2 cut=350 res=0.75 fenv=0.7 fd=180 fs=0 mode=1 glide=60 vf=0.6 subn=0 vol=2.0" },
            { "Acid Bass", "Àcid quadrat", "Quadrada pel filtre de díodes: més buit i més dur.",
              "o1w=3 flt=2 cut=300 res=0.8 fenv=0.75 fd=220 fs=0 mode=1 glide=60 subn=0 vol=-0.5" },
            { "Acid Bass", "Àcid distorsionat", "L'àcid de sempre passat per un pedal dur.",
              "flt=2 cut=380 res=0.78 fenv=0.7 fd=170 fs=0 mode=1 glide=60 dt=1 dq=0.6 dto=0.55 subn=0 vol=-1.5" },
            { "Acid Bass", "Àcid wobble", "L'LFO obre i tanca el filtre a corxeres.",
              "flt=2 cut=420 res=0.72 fenv=0.45 fd=200 fs=0.1 mode=1 glide=60 ls=4 ld=0 lq=0.3 lw=1 subn=0 vol=2.0" },
            { "Acid Bass", "Àcid xiulant", "Ressonància al límit: el filtre xiula a cada nota.",
              "flt=2 cut=500 res=0.93 fenv=0.5 fd=120 fs=0 mode=1 glide=45 subn=0 vol=2.0" },
            // ---- 303 Style Bass ----
            { "303 Style Bass", "Línia 303", "Serra, díodes i slide: per a seqüències de 16 notes.",
              "flt=2 cut=420 res=0.65 fenv=0.6 fd=150 fs=0 aa=0.5 ad=900 as=0.9 ar=20 mode=1 glide=55 vf=0.7 vv=0.7 subn=0 key=0.2 vol=2.0" },
            { "303 Style Bass", "303 quadrada", "La versió quadrada: més fosca.",
              "o1w=3 flt=2 cut=380 res=0.62 fenv=0.62 fd=170 fs=0 aa=0.5 ad=900 as=0.9 ar=20 mode=1 glide=55 vf=0.7 subn=0 vol=-1.0" },
            { "303 Style Bass", "303 amb accent", "Les notes fortes s'obren molt més: toca amb dinàmica.",
              "flt=2 cut=330 res=0.8 fenv=0.5 fd=140 fs=0 ar=20 mode=1 glide=55 vf=1 vv=0.9 subn=0 vol=3.0" },
            { "303 Style Bass", "303 amb pedal", "Línia àcida amb distorsió després del filtre.",
              "flt=2 cut=400 res=0.7 fenv=0.65 fd=160 fs=0 ar=20 mode=1 glide=55 dt=1 dq=0.75 dto=0.5 vf=0.7 subn=0 vol=-0.5" },
            { "303 Style Bass", "303 slide llarg", "Glide llarg: les notes lligades s'arrosseguen molt.",
              "flt=2 cut=450 res=0.6 fenv=0.55 fd=200 fs=0 ar=20 mode=1 glide=180 vf=0.6 subn=0 vol=2.0" },
            // ---- Reese Bass ----
            { "Reese Bass", "Reese clàssic", "Dues serres desafinades que baten: el baix del drum'n'bass.",
              "o2n=0.85 o2f=18 cut=1200 res=0.15 fenv=0.1 key=0.2 lliure=1 subn=0.5 as=1 ar=150 vol=-7.0" },
            { "Reese Bass", "Reese ample", "Unison de 4 obert a l'estèreo amb el sub al mig.",
              "uni=4 unid=0.35 unia=0.9 o2n=0.8 o2f=12 subn=0.6 width=1.3 cut=1500 fenv=0.1 lliure=1 as=1 vol=-4.0" },
            { "Reese Bass", "Reese fosc", "Filtre tancat que respira amb un LFO molt lent.",
              "o2n=0.85 o2f=15 cut=500 res=0.3 fenv=0.1 ld=0 lq=0.15 lr=0.3 lret=0 lliure=1 subn=0.6 as=1 vol=-6.5" },
            { "Reese Bass", "Reese neurofunk", "SVF de 12 dB i distorsió: aspre i modern.",
              "o2n=0.85 o2f=20 flt=1 pend=0 cut=1800 res=0.3 fenv=0.1 dt=1 dq=0.5 dto=0.6 lliure=1 subn=0.6 as=1 vol=-6.5" },
            { "Reese Bass", "Reese que es mou", "L'LFO obre el filtre a poc a poc: moviment constant.",
              "o2n=0.85 o2f=14 cut=700 res=0.45 fenv=0 ld=0 lq=0.4 lr=0.25 lret=0 lliure=1 subn=0.5 as=1 vol=-5.5" },
            // ---- Distorted Bass ----
            { "Distorted Bass", "Distorsió dura", "Transistor asimètric després del filtre i sub net per sota.",
              "dt=1 dq=0.7 dto=0.55 cut=1500 res=0.2 fenv=0.3 subn=0.6 vol=-4.5" },
            { "Distorted Bass", "Fuzz de baix", "Quadrada per un fuzz trencat.",
              "o1w=3 dt=2 dq=0.75 dto=0.45 cut=900 res=0.15 subn=0.55 vol=-3.0" },
            { "Distorted Bass", "Saturat calent", "Vàlvula abans del filtre: el filtre arrodoneix la distorsió.",
              "dt=0 dq=0.65 dpos=0 cut=700 res=0.3 fenv=0.4 subn=0.4 vol=-1.5" },
            { "Distorted Bass", "Ampli de baix", "Com un ampli de baix empès: barreja de net i saturat.",
              "o2w=3 o2o=-1 o2f=0 o2n=0.4 dt=0 dq=0.5 dto=0.4 dmix=0.6 cut=1300 res=0.1 fenv=0.25 vol=-7.5" },
            { "Distorted Bass", "Estripat", "Fuzz a fons pel filtre MS-20: no té pietat.",
              "dt=2 dq=1 dto=0.75 dmix=0.8 flt=3 cut=1200 res=0.4 fenv=0.3 subn=0.5 vol=-1.5" },
            // ---- Growl Bass ----
            { "Growl Bass", "Growl clàssic", "Ona plegada, FM i un LFO a corxeres sobre el filtre MS-20.",
              "fm=0.25 dt=3 dq=0.55 flt=3 cut=900 res=0.55 ld=0 lq=0.45 ls=4 subn=0.7 fenv=0.1 as=1 vol=-10.0" },
            { "Growl Bass", "Growl plegat", "L'LFO mou la FM: la boca s'obre i es tanca.",
              "fm=0.2 dt=3 dq=0.8 ld=4 lq=0.5 ls=3 cut=2500 res=0.3 fenv=0 subn=0.7 as=1 vol=-10.0" },
            { "Growl Bass", "Growl vocal", "SVF de 12 dB ressonant: sona com una «o» que diu «uà».",
              "flt=1 pend=0 res=0.75 cut=700 ld=0 lq=0.5 ls=5 lw=1 dt=3 dq=0.4 subn=0.6 fenv=0 as=1 vol=-9.5" },
            { "Growl Bass", "Growl lent", "Moviment a blanques: per a drops a mig temps.",
              "fm=0.3 dt=3 dq=0.6 flt=3 cut=800 res=0.5 ld=0 lq=0.5 ls=2 subn=0.7 fenv=0 as=1 vol=-10.5" },
            { "Growl Bass", "Growl agressiu", "Unison, fuzz i FM: dens i enfadat.",
              "uni=3 unid=0.2 dt=2 dq=0.7 fm=0.35 flt=3 cut=1200 res=0.45 ld=0 lq=0.35 ls=4 subn=0.7 as=1 vol=-5.5" },
            // ---- FM Bass ----
            { "FM Bass", "FM rodó", "Dos sinus en FM: rodó amb un punt de metall.",
              "o1w=0 o2w=0 o2f=0 o1n=0 o2n=0.85 fm=0.35 cut=1500 fenv=0.4 fd=300 res=0 subn=0.3 vol=-2.0" },
            { "FM Bass", "FM digital", "Modulador una octava amunt: el baix dels anys 80 digitals.",
              "o1w=0 o1o=1 o2w=0 o2f=0 o1n=0 o2n=0.85 fm=0.45 cut=4000 fenv=0.3 ad=300 as=0.6 res=0 subn=0.3 vol=-3.0" },
            { "FM Bass", "FM elèctric", "Com un baix elèctric de piano FM: clic, atac i caiguda.",
              "o1w=0 o2w=0 o2f=0 o1n=0 o2n=0.9 fm=0.5 cut=2500 fenv=0.4 fd=200 fs=0.2 clic=0.2 ad=500 as=0.4 res=0 subn=0.25 vol=-3.0" },
            { "FM Bass", "FM amb sub", "FM per al color i sub net per al pes.",
              "o1w=0 o2w=0 o2f=0 o1n=0 o2n=0.7 fm=0.4 cut=3000 fenv=0.2 res=0 subn=0.75 vol=-4.5" },
            { "FM Bass", "FM amb punch", "Envolupant de to a l'atac: cada nota pica.",
              "o1w=0 o2w=0 o2f=0 o1n=0 o2n=0.85 fm=0.4 pe=7 pet=25 cut=2200 fenv=0.4 fd=180 res=0 subn=0.4 vol=-2.0" },
            // ---- Metallic Bass ----
            { "Metallic Bass", "Sync metàl·lic", "L'oscil·lador 2 sincronitzat una octava i quarta amunt: tallant.",
              "sync=1 o2o=1 o2s=5 o2n=0.85 o2f=0 o1n=0.2 cut=4000 fenv=0.2 res=0.1 subn=0.5 vol=-3.0" },
            { "Metallic Bass", "Campana greu", "FM amb un modulador a la quinta: com una campana grossa.",
              "fm=0.7 o1w=0 o1s=7 o2w=0 o2f=0 o1n=0 o2n=0.8 ad=1200 as=0.2 ar=600 cut=6000 fenv=0 res=0 subn=0.3 vol=-2.0" },
            { "Metallic Bass", "Metall fred", "FM desafinada: harmònics que no són de la nota (inharmònic).",
              "fm=0.6 o1w=0 o1o=2 o1f=37 o2w=0 o2f=0 o1n=0 o2n=0.8 cut=5000 fenv=0.1 ad=700 as=0.4 res=0 subn=0.4 vol=-3.0" },
            { "Metallic Bass", "Sync ressonant", "Sync i MS-20 ressonant: crida.",
              "sync=1 o2o=1 o2s=0 o2f=0 o2n=0.85 o1n=0 flt=3 res=0.6 cut=1500 fenv=0.4 fd=250 subn=0.5 vol=-4.0" },
            { "Metallic Bass", "Ring", "FM molt forta amb un modulador greu: sona com un modulador d'anell.",
              "fm=0.85 o1w=0 o1o=-1 o1s=5 o2w=0 o2f=0 o1n=0 o2n=0.75 cut=8000 fenv=0 res=0 subn=0.4 vol=-4.0" },
            // ---- Rubber Bass ----
            { "Rubber Bass", "Goma", "Quadrada ressonant amb envolupant curta i un petit salt de to: elàstic.",
              "o1w=3 cut=250 res=0.5 fenv=0.55 fd=120 fs=0.1 pe=3 pet=30 glide=30 mode=1 subn=0.4 vol=-3.5" },
            { "Rubber Bass", "Goma rodona", "SVF ressonant i molt tancat: «boing» rodó.",
              "flt=1 pend=1 res=0.55 cut=200 fenv=0.6 fd=160 fs=0.05 o1w=1 o2w=3 o2o=-1 o2f=0 o2n=0.5 subn=0.3 vol=-8.0" },
            { "Rubber Bass", "Rebot", "La nota baixa ràpidament des de dalt: rebota.",
              "o1w=3 pe=5 pet=60 cut=300 res=0.4 fenv=0.5 fd=180 fs=0.1 subn=0.4 vol=-3.5" },
            { "Rubber Bass", "Goma àcida", "El díode ressonant amb una envolupant que rebota.",
              "flt=2 res=0.6 cut=200 fenv=0.6 fd=200 fs=0.05 pe=2 pet=40 subn=0.3 vol=2.0" },
            { "Rubber Bass", "Bombolla", "Ressonància i LFO a corxeres: fa bombolles.",
              "o1w=3 res=0.6 cut=280 fenv=0.4 fd=150 fs=0.1 ld=0 lq=0.25 ls=4 lw=0 subn=0.4 vol=-3.0" },
            // ---- Funk Bass ----
            { "Funk Bass", "Funk pols", "Pols estret i filtre que salta amb la força de cada nota.",
              "o1w=4 o1p=0.25 cut=400 res=0.45 fenv=0.75 fd=140 fs=0.1 vf=0.6 subn=0.35 vol=-2.0" },
            { "Funk Bass", "Funk ladder", "Quadrada pel ladder: el baix de sinte dels discos de funk.",
              "o1w=3 cut=500 res=0.5 fenv=0.7 fd=160 fs=0.15 fdrv=0.3 lliure=1 subn=0.4 vol=-6.0" },
            { "Funk Bass", "Funk amb glide", "Legato: les notes lligades llisquen com un baix sense trasts.",
              "o1w=3 mode=1 glide=40 cut=450 res=0.45 fenv=0.65 fd=180 fs=0.2 subn=0.4 vol=-3.5" },
            { "Funk Bass", "Funk de dits", "Clic i salt de to: recorda un slap.",
              "clic=0.4 pe=2 pet=15 cut=1200 fenv=0.5 fd=90 fs=0.3 res=0.25 subn=0.4 vv=0.7 vol=0.5" },
            { "Funk Bass", "Sinus G-funk", "Sinus lligat amb glide i vibrato a la roda.",
              "o1w=0 mode=1 glide=60 ld=1 lq=0.12 lr=5 mwl=0.5 cut=18000 fenv=0 res=0 subn=0.3 vol=-5.0" },
            // ---- Pluck Bass ----
            { "Pluck Bass", "Pluck curt", "Nota curta que es tanca de seguida: per a ritmes.",
              "ad=180 as=0 ar=120 fd=90 fs=0 fenv=0.8 cut=250 res=0.35 subn=0.4 vol=5.0" },
            { "Pluck Bass", "Pluck brillant", "Atac obert i caiguda ràpida: pica.",
              "cut=600 fenv=0.7 fd=120 fs=0 res=0.2 ad=260 as=0 ar=150 subn=0.4 vol=3.0" },
            { "Pluck Bass", "Pluck FM", "FM curta: un pluck amb vidre.",
              "o1w=0 o2w=0 o2f=0 o1n=0 o2n=0.85 fm=0.5 ad=250 as=0 ar=140 cut=3000 fenv=0.5 fd=120 fs=0 res=0 subn=0.3 vol=0.0" },
            { "Pluck Bass", "Pluck house", "Quadrada i serra una octava amunt: el pluck de la house.",
              "o1w=3 o2n=0.5 o2s=12 o2f=0 ad=220 as=0 ar=120 fd=110 fs=0 cut=350 res=0.4 fenv=0.6 subn=0.3 vol=1.0" },
            { "Pluck Bass", "Pizzicato sintètic", "Triangle i pols amb caiguda curta: com una corda pinçada.",
              "o1w=1 o2w=4 o2p=0.2 o2n=0.4 o2f=0 ad=300 as=0 ar=200 cut=1500 fenv=0.4 fd=150 fs=0 clic=0.15 subn=0.3 vol=4.5" },
            // ---- Sequence Bass ----
            { "Sequence Bass", "Seqüència curta", "Per a semicorxeres: tancat i ràpid.",
              "ad=120 as=0 ar=60 fd=100 fs=0 cut=500 fenv=0.6 res=0.3 subn=0.4 vol=6.0" },
            { "Sequence Bass", "Seqüència berlinesa", "Serra i quadrada amb ressonància i un LFO lentíssim al tall.",
              "o2w=3 o2n=0.5 o2o=-1 o2f=0 ad=200 as=0.1 ar=80 fd=150 fs=0 res=0.55 cut=300 fenv=0.5 ld=0 lq=0.2 lr=0.08 lret=0 subn=0.3 vol=1.5" },
            { "Sequence Bass", "Seqüència pols", "Pols amb PWM: les seqüències es mouen soles.",
              "o1w=4 o1p=0.2 ad=150 as=0.05 ar=70 fd=120 fs=0 cut=600 fenv=0.5 res=0.3 ld=2 lq=0.3 lr=0.3 lret=0 subn=0.4 vol=3.0" },
            { "Sequence Bass", "Seqüència ràpida", "Molt curta i amb clic: per a tempos alts.",
              "ad=80 as=0 ar=40 fd=70 fs=0 cut=700 fenv=0.5 res=0.25 clic=0.25 subn=0.4 vol=6.0" },
            { "Sequence Bass", "Arpegi de baix", "Poli, curt i ressonant: deixa que els arpegis se solapin.",
              "mode=2 ad=250 as=0 ar=250 fd=150 fs=0 cut=400 fenv=0.55 res=0.45 o2n=0.4 o2f=8 subn=0.3 vol=3.0" },
            // ---- Indie Rock Bass ----
            { "Indie Rock Bass", "Indie gruixut", "Serra i quadrada avall amb una mica de vàlvula: ocupa el lloc del baix elèctric.",
              "o2w=3 o2o=-1 o2f=0 o2n=0.5 dt=0 dq=0.35 dto=0.45 cut=1100 res=0.1 fenv=0.25 fd=500 fs=0.5 subn=0.35 vol=-8.5" },
            { "Indie Rock Bass", "Indie amb púa", "Clic i transistor: com un baix tocat amb púa.",
              "clic=0.3 cut=1800 fenv=0.35 fd=200 fs=0.45 dt=1 dq=0.3 dto=0.6 res=0.1 subn=0.35 vol=-6.0" },
            { "Indie Rock Bass", "Indie fosc", "Tancat i saturat: per a cançons lentes i fosques.",
              "o1w=3 cut=550 res=0.2 fenv=0.3 fd=600 fs=0.5 fdrv=0.5 dt=0 dq=0.3 subn=0.5 vol=-10.5" },
            { "Indie Rock Bass", "Indie fuzz", "Fuzz barrejat amb el net: el greu queda sencer.",
              "dt=2 dq=0.5 dmix=0.55 dto=0.5 cut=1400 res=0.1 fenv=0.25 subn=0.5 vol=-2.5" },
            { "Indie Rock Bass", "Post-punk", "Quadrada brillant i dura: el baix fa la melodia.",
              "o1w=3 cut=2200 res=0.2 fenv=0.2 dq=0.3 dt=1 dto=0.7 ad=600 as=0.6 subn=0.3 key=0.5 vol=-7.5" },
            // ---- Modern Pop Bass ----
            { "Modern Pop Bass", "Pop net", "Sinus i una mica de serra: net, gros i amb definició.",
              "o1w=0 o2n=0.35 o2f=0 cut=900 res=0.05 fenv=0.25 subn=0.5 as=0.9 vol=-3.5" },
            { "Modern Pop Bass", "Pop amb sub", "Serra tancada a sobre d'un sub fort.",
              "cut=500 res=0.1 fenv=0.3 subn=0.8 as=0.85 vol=-5.5" },
            { "Modern Pop Bass", "Pop pluck", "Curt i definit, per a tornades amb molt de ritme.",
              "o1w=3 ad=300 as=0.1 ar=100 fd=140 fs=0 cut=450 fenv=0.55 res=0.25 subn=0.6 vol=-2.5" },
            { "Modern Pop Bass", "Pop que bomba", "L'LFO fa de sidechain: el volum respira a negres.",
              "o2n=0.4 o2f=6 cut=800 res=0.1 fenv=0.2 ld=3 lw=2 ls=3 lq=0.8 lret=1 subn=0.6 as=1 vol=-5.0" },
            { "Modern Pop Bass", "Pop brillant", "Filtre obert i un punt de drive: es sent als altaveus petits.",
              "cut=2200 res=0.15 fenv=0.2 dt=0 dq=0.3 dto=0.75 subn=0.55 as=0.8 vol=-6.0" },
            // ---- Synthwave Bass ----
            { "Synthwave Bass", "Synthwave clàssic", "Dues serres desafinades per l'SVF de 24 dB: anys 80 purs.",
              "o2n=0.7 o2f=-10 flt=1 pend=1 cut=700 res=0.3 fenv=0.45 fd=250 fs=0.3 lliure=1 uni=2 unid=0.2 subn=0.35 vol=-1.0" },
            { "Synthwave Bass", "Octaves synthwave", "La segona serra una octava amunt: per a les línies de corxeres en octaves.",
              "o2o=1 o2f=5 o2n=0.5 flt=1 pend=1 cut=900 res=0.25 fenv=0.4 fd=200 fs=0.2 lliure=1 subn=0.3 vol=-5.0" },
            { "Synthwave Bass", "Synthwave fosc", "Tancat, amb molt de sub: per a la nit.",
              "o2n=0.6 o2f=-8 flt=1 pend=1 cut=380 res=0.35 fenv=0.4 fd=300 fs=0.25 lliure=1 subn=0.7 vol=-8.0" },
            { "Synthwave Bass", "Synthwave pols", "Pols amb PWM: el color dels polifònics clàssics.",
              "o1w=4 o1p=0.35 ld=2 lq=0.2 lr=0.4 lret=0 flt=1 pend=1 cut=800 res=0.2 fenv=0.4 lliure=1 subn=0.4 vol=-8.5" },
            { "Synthwave Bass", "Nit de neó", "Unison ample i glide curt: gran i brillant.",
              "uni=3 unid=0.25 unia=0.8 o2n=0.5 o2f=7 flt=1 pend=1 cut=1100 res=0.3 fenv=0.35 mode=1 glide=35 subn=0.5 vol=-5.5" },
            // ---- Cinematic Bass ----
            { "Cinematic Bass", "Drone", "Atac i alliberament llargs, unison i soroll fosc: sota una escena.",
              "aa=600 ar=3000 as=1 cut=300 res=0.4 fenv=0.1 ld=0 lq=0.25 lr=0.1 lret=0 uni=3 unid=0.3 nz=0.15 nzc=0.7 subn=0.6 vol=1.0" },
            { "Cinematic Bass", "Impacte", "Un cop sec que cau dues octaves: per a talls i trailers.",
              "o1w=0 pe=24 pet=180 ad=2500 as=0 ar=2000 clic=0.5 dt=0 dq=0.35 cut=18000 fenv=0 res=0 subn=0.5 vol=-8.0" },
            { "Cinematic Bass", "Tensió", "Dues serres a un semitò: inquietud que creix.",
              "o2s=1 o2f=0 o2n=0.6 cut=400 res=0.5 ld=0 lq=0.2 lr=0.15 lret=0 aa=300 ar=2000 as=1 fenv=0.2 fa=800 fd=3000 subn=0.5 vol=-1.0" },
            { "Cinematic Bass", "Braam", "Unison, vàlvula i MS-20 obrint-se a poc a poc: el «braam» dels trailers.",
              "uni=5 unid=0.25 dt=0 dq=0.6 cut=500 fenv=0.4 fa=400 fd=2500 fs=0.5 aa=50 ar=1500 as=1 res=0.3 subn=0.7 flt=3 vol=-5.5" },
            { "Cinematic Bass", "Pols de suspens", "Un sinus que tremola a corxeres: compte enrere.",
              "o1w=0 o2w=1 o2n=0.3 o2f=0 ld=3 lq=0.65 ls=4 lw=3 cut=1500 fenv=0 res=0 subn=0.5 as=1 ar=600 vol=-6.0" },
            // ---- Estils ----
            { "Estils", "Indie rock modern · Gruixut", "Per a indie rock d'ara: saturat, mig obert, enganxat al bombo.",
              "o2w=3 o2o=-1 o2f=0 o2n=0.45 dt=0 dq=0.4 dto=0.5 cut=1000 res=0.15 fenv=0.3 fd=400 fs=0.45 clic=0.15 subn=0.4 vol=-8.0" },
            { "Estils", "Indie rock modern · Amb fuzz", "El baix fa de guitarra: fuzz barrejat i greu sencer.",
              "o1w=3 dt=2 dq=0.6 dmix=0.5 dto=0.55 cut=1600 res=0.1 fenv=0.2 subn=0.55 vol=-5.5" },
            { "Estils", "Indie pop · Rodó", "Net, rodó i amable, amb un punt d'aire.",
              "o1w=1 o2w=2 o2n=0.3 o2f=5 flt=1 pend=1 cut=900 res=0.1 fenv=0.3 nz=0.05 nzc=0.5 subn=0.45 vol=-4.5" },
            { "Estils", "Indie pop · Saltador", "Curt i amb rebot, per a tornades alegres.",
              "o1w=3 ad=350 as=0.2 fd=130 fs=0.05 cut=380 res=0.4 fenv=0.55 pe=2 pet=25 subn=0.45 vol=-1.5" },
            { "Estils", "Synth pop · Clàssic", "Serres desafinades i SVF: el baix dels anys 80 que torna.",
              "o2n=0.65 o2f=-9 flt=1 pend=1 cut=650 res=0.3 fenv=0.5 fd=220 fs=0.2 lliure=1 subn=0.35 vol=-7.0" },
            { "Estils", "Synth pop · Corxeres", "Per a línies de corxeres seguides: curt i brillant.",
              "o2o=1 o2f=4 o2n=0.4 ad=260 as=0.15 ar=60 fd=140 fs=0.05 flt=1 pend=1 cut=600 res=0.3 fenv=0.5 subn=0.35 vol=-2.0" },
            { "Estils", "Rock alternatiu · Dur", "Transistor i clic: el baix aguanta les guitarres.",
              "dt=1 dq=0.55 dto=0.5 clic=0.25 cut=1400 res=0.1 fenv=0.25 fd=300 fs=0.5 subn=0.45 vol=-4.5" },
            { "Estils", "Rock alternatiu · Fosc", "Quadrada saturada i tancada, per a estrofes fosques.",
              "o1w=3 dt=0 dq=0.55 dpos=0 cut=600 res=0.25 fenv=0.3 subn=0.55 vol=-5.0" },
            { "Estils", "EDM · Drop", "Reese saturat i ample per a un drop gran.",
              "uni=3 unid=0.3 unia=0.8 o2n=0.7 o2f=12 dt=1 dq=0.45 cut=1600 res=0.25 fenv=0.2 subn=0.7 as=1 vol=-8.0" },
            { "Estils", "EDM · Wobble", "Wobble a corxeres amb FM i plec.",
              "fm=0.2 dt=3 dq=0.5 flt=1 pend=0 cut=600 res=0.55 ld=0 lq=0.55 ls=4 lw=0 subn=0.7 fenv=0 as=1 vol=-10.0" },
            { "Estils", "House · Orgue de baix", "Sinus amb un punt de quadrada i clic: el baix de la house clàssica.",
              "o1w=0 o2w=3 o2n=0.25 o2f=0 o2o=1 clic=0.2 ad=400 as=0.4 cut=1200 fenv=0.3 res=0 subn=0.3 vol=-3.5" },
            { "Estils", "House · Deep", "Pluck fosc i rodó per a deep house.",
              "o1w=1 o2w=3 o2n=0.35 o2f=6 ad=500 as=0.15 fd=200 fs=0.05 cut=300 res=0.3 fenv=0.5 subn=0.5 vol=-0.5" },
            { "Estils", "Techno · Rumble", "Greu que retruny amb el bombo: curt, fosc i amb drive.",
              "o1w=0 o2w=2 o2n=0.3 o2f=0 dt=0 dq=0.5 dto=0.3 ad=300 as=0 ar=200 cut=250 res=0.2 fenv=0.3 fd=150 subn=0.5 vol=-2.0" },
            { "Estils", "Techno · Àcid fosc", "Díodes, quadrada i distorsió per a techno dur.",
              "o1w=3 flt=2 cut=250 res=0.7 fenv=0.65 fd=200 fs=0 mode=1 glide=50 dt=1 dq=0.5 dto=0.4 subn=0.2 vol=-4.0" },
            { "Estils", "Trap · 808 llarg", "808 amb glide: llisca entre notes lligades.",
              "o1w=0 subn=0 cut=18000 fenv=0 res=0 pe=9 pet=40 ad=3000 as=0.25 ar=500 mode=1 glide=110 clic=0.15 dt=0 dq=0.25 dto=0.5 vol=-7.5" },
            { "Estils", "Trap · 808 distorsionat", "808 trencat per sonar a tot arreu.",
              "o1w=0 subn=0 cut=18000 fenv=0 res=0 pe=10 pet=35 ad=2500 as=0.2 ar=400 mode=1 glide=90 clic=0.2 dt=1 dq=0.6 dto=0.6 dmix=0.8 vol=-3.0" },
            { "Estils", "Dark pop · Pes", "Sub molt fort i una serra fosca a sobre.",
              "cut=350 res=0.2 fenv=0.2 subn=0.85 dt=0 dq=0.3 dto=0.3 as=1 vol=-8.5" },
            { "Estils", "Dark pop · Distorsió freda", "Sinus i fuzz barrejats: inquietant però net al greu.",
              "o1w=0 dt=2 dq=0.55 dmix=0.4 dto=0.6 cut=18000 fenv=0 res=0 subn=0.6 as=1 vol=-6.5" },
            { "Estils", "Cinematic · Sub de tràiler", "Un cop i una cua llarguíssima.",
              "o1w=0 pe=12 pet=250 ad=4000 as=0 ar=3000 clic=0.4 cut=18000 fenv=0 res=0 subn=0.6 vol=-6.5" },
            { "Estils", "Cinematic · Pulsació", "Baix que pulsa a semicorxeres amb el tempo.",
              "o1w=2 cut=500 res=0.3 fenv=0.1 ld=3 lw=3 ls=5 lq=0.7 lret=1 subn=0.6 as=1 vol=-2.5" },
            { "Estils", "Pop d'estadi · Coixí", "Textura ampla i càlida per sota de guitarres i pianos grans.",
              "uni=3 unid=0.2 unia=0.7 o2n=0.5 o2f=7 o2o=1 flt=1 pend=1 cut=700 res=0.1 fenv=0.15 aa=25 ar=600 as=1 subn=0.5 width=1.2 vol=-3.5" },
            { "Estils", "Pop d'estadi · Corxeres", "Corxeres en octaves que fan córrer la cançó (inspiració general, sense copiar res).",
              "o2o=1 o2n=0.45 o2f=3 ad=400 as=0.3 ar=90 flt=1 pend=1 cut=850 res=0.15 fenv=0.35 fd=200 fs=0.2 subn=0.45 vol=-3.5" },
            { "Estils", "Retro 80s · Polifònic", "Pols i serra per l'SVF, amb PWM: el baix dels polifònics.",
              "o1w=4 o1p=0.4 o2n=0.5 o2f=-7 ld=2 lq=0.2 lr=0.5 lret=0 flt=1 pend=1 cut=750 res=0.25 fenv=0.4 subn=0.3 vol=-5.5" },
            { "Estils", "Retro 80s · FM", "El baix FM dels anys 80, amb el seu clic.",
              "o1w=0 o2w=0 o2f=0 o1n=0 o2n=0.9 fm=0.48 clic=0.2 ad=600 as=0.35 cut=3000 fenv=0.4 fd=200 res=0 subn=0.3 vol=-3.0" },
            { "Estils", "Massiu · Distorsionat", "Unison, fuzz i MS-20: enorme.",
              "uni=4 unid=0.3 unia=0.8 dt=2 dq=0.8 dto=0.55 flt=3 cut=1300 res=0.35 fenv=0.3 subn=0.8 as=1 vol=-6.0" },
            { "Estils", "Massiu · Plegat", "Ona plegada i reese: amplitud i moviment.",
              "o2n=0.8 o2f=16 dt=3 dq=0.7 cut=1800 res=0.3 fenv=0.15 ld=0 lq=0.25 lr=0.4 lret=0 subn=0.75 as=1 vol=-11.0" },
            { "Estils", "Sub pop · Net", "Sub sinus amb un harmònic perquè se senti en qualsevol altaveu.",
              "o1w=0 o2w=0 o2o=1 o2f=0 o2n=0.15 cut=18000 fenv=0 res=0 subn=0 as=1 ar=90 vol=-5.0" },
            { "Estils", "Sub pop · Amb clic", "Sub per a pop amb clic i caiguda suau.",
              "o1w=0 clic=0.3 ad=900 as=0.6 ar=120 cut=18000 fenv=0 res=0 subn=0.3 dt=0 dq=0.15 vol=-6.5" },
        };

        void aplica (SubBass& s, const char* canvis)
        {
            std::istringstream in (canvis);
            std::string tok;
            while (in >> tok)
            {
                const auto eq = tok.find ('=');
                if (eq == std::string::npos) continue;
                const auto clau = tok.substr (0, eq);
                for (int i = 0; i < numParams; ++i)
                    if (clau == info (i).clau) s.posa (i, (float) std::strtod (tok.c_str() + eq + 1, nullptr));
            }
        }

        struct Atzar
        {
            std::uint32_t s;
            explicit Atzar (unsigned l) : s (l * 2654435761u + 12345u) { if (s == 0) s = 1; }
            float u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 16777216.0f; }
            float r (float a, float b) { return a + (b - a) * u(); }
            float log (float a, float b) { return a * std::pow (b / a, u()); }
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
                auto s = std::make_unique<SubBass>();
                aplica (*s, x.canvis);
                r.push_back ({ x.nom, x.cat, x.desc, s->estatSo() });
            }
            return r;
        }();
        return v;
    }

    std::string estatInicial()
    {
        auto s = std::make_unique<SubBass>();
        return s->estatSo();
    }

    std::string aleatori (unsigned llavor)
    {
        Atzar a (llavor);
        auto sp = std::make_unique<SubBass>();
        auto& s = *sp;
        static const int ones[] = { 2, 2, 2, 3, 3, 4, 1, 0 };
        s.posa (O1Ona, (float) ones[a.i (0, 7)]);
        s.posa (O1Oct, a.p (0.2f) ? -1.0f : 0.0f);
        s.posa (O1Pw, a.r (0.15f, 0.5f));
        s.posa (O1Niv, a.r (0.7f, 0.95f));
        s.posa (Fase, (float) a.i (0, 3) * 90.0f);
        s.posa (FaseLliure, a.p (0.4f) ? 1.0f : 0.0f);
        if (a.p (0.6f))
        {
            static const float semis[] = { 0, 0, 0, 7, 12, -12, 0, 5 };
            s.posa (O2Ona, (float) ones[a.i (0, 7)]);
            s.posa (O2Semi, semis[a.i (0, 7)]);
            s.posa (O2Fi, a.r (3.0f, 22.0f) * (a.p (0.5f) ? 1.0f : -1.0f));
            s.posa (O2Niv, a.r (0.3f, 0.8f));
            s.posa (O2Pw, a.r (0.15f, 0.5f));
        }
        else s.posa (O2Niv, 0.0f);
        s.posa (Sync, a.p (0.08f) ? 1.0f : 0.0f);
        if (s.valor (Sync) > 0.5f) { s.posa (O2Oct, 1.0f); s.posa (O2Niv, 0.8f); }
        s.posa (Fm, a.p (0.15f) ? a.r (0.1f, 0.5f) : 0.0f);
        if (a.p (0.25f)) { s.posa (Unison, (float) a.i (2, 4)); s.posa (UniDetune, a.r (0.1f, 0.4f)); s.posa (UniAmple, a.r (0.3f, 0.9f)); }
        s.posa (Clic, a.p (0.2f) ? a.r (0.1f, 0.4f) : 0.0f);
        s.posa (SubOna, a.p (0.75f) ? 0.0f : 1.0f);
        s.posa (SubNiv, a.p (0.85f) ? a.r (0.3f, 0.75f) : 0.0f);
        s.posa (SubFiltre, a.p (0.2f) ? 1.0f : 0.0f);
        s.posa (SorollNiv, a.p (0.1f) ? a.r (0.03f, 0.15f) : 0.0f);
        s.posa (FModel, (float) a.i (0, 3));
        s.posa (FPendent, a.p (0.7f) ? 1.0f : 0.0f);
        s.posa (FTall, a.log (180.0f, 2200.0f));
        s.posa (FRes, a.p (0.3f) ? a.r (0.45f, 0.8f) : a.r (0.0f, 0.4f));
        s.posa (FEnv, a.r (0.15f, 0.75f));
        s.posa (FKey, a.r (0.1f, 0.6f));
        s.posa (FDrive, a.r (0.0f, 0.55f));
        const bool pluck = a.p (0.3f);
        s.posa (AA, a.log (0.5f, 12.0f));
        s.posa (AD, a.log (pluck ? 120.0f : 200.0f, pluck ? 500.0f : 1500.0f));
        s.posa (AS, pluck ? 0.0f : a.r (0.4f, 1.0f));
        s.posa (AR, a.log (30.0f, 350.0f));
        s.posa (FA, a.log (0.5f, 20.0f));
        s.posa (FD, a.log (80.0f, 700.0f));
        s.posa (FS, a.r (0.0f, 0.45f));
        s.posa (FR, a.log (50.0f, 350.0f));
        if (a.p (0.3f))
        {
            s.posa (LOna, (float) a.i (0, 4));
            s.posa (LDesti, a.p (0.7f) ? 0.0f : 2.0f);
            s.posa (LRitme, a.p (0.5f) ? 0.0f : (float) a.i (3, 5));
            s.posa (LVel, a.log (0.1f, 6.0f));
            s.posa (LQuant, a.r (0.1f, 0.4f));
        }
        else s.posa (LQuant, 0.0f);
        if (a.p (0.4f)) { s.posa (DTipus, (float) a.i (0, 3)); s.posa (DQuant, a.r (0.15f, 0.6f)); s.posa (DTo, a.r (0.35f, 0.8f)); s.posa (DLloc, a.p (0.6f) ? 1.0f : 0.0f); }
        else s.posa (DQuant, 0.0f);
        if (a.p (0.2f)) { s.posa (PEnvQuant, a.r (2.0f, 12.0f)); s.posa (PEnvTemps, a.log (15.0f, 80.0f)); }
        const float m = a.u();
        s.posa (Mode, m < 0.7f ? 0.0f : m < 0.95f ? 1.0f : 2.0f);
        s.posa (Glide, s.valor (Mode) > 0.5f && s.valor (Mode) < 1.5f ? a.r (30.0f, 120.0f) : 0.0f);
        s.posa (Volum, s.valor (DQuant) > 0.4f || s.valor (Unison) > 2.5f ? -6.0f : -3.0f);
        return s.estatSo();
    }

    std::string varia (const std::string& estat, unsigned llavor, float quant)
    {
        Atzar a (llavor);
        auto sp = std::make_unique<SubBass>();
        auto& s = *sp;
        s.posaEstat (estat);
        for (int p = 0; p < numParams; ++p)
        {
            const auto& i = info (p);
            if (i.escala == Enter || p == Volum || p == BendRang || p == Fase) continue;
            if (std::abs (s.valor (p)) < 1e-6f && (p == O2Niv || p == Fm || p == SorollNiv || p == Clic || p == LQuant || p == DQuant || p == PEnvQuant || p == SubNiv || p == Glide))
                continue;   // el que està apagat es queda apagat
            const float x = normal (p, s.valor (p)) + (a.u() * 2.0f - 1.0f) * 0.18f * quant;
            s.posa (p, deNormal (p, x));
        }
        return s.estatSo();
    }
}
