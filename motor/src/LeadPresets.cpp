#include "mibase/Lead.h"
#include <cmath>
#include <cstdlib>
#include <memory>
#include <sstream>

/** 0.89: els presets de fàbrica del LEAD, fets amb la síntesi pròpia (inspirats en famílies de sintes clàssics, sense
    copiar cap preset comercial). Cada un és una llista de canvis «clau=valor» respecte del so inicial.
    Ones: 0 sinus, 1 triangle, 2 serra, 3 quadrada, 4 pols, 5 supersaw, 6 wavetable, 7 soroll. Filtre: 0 LP, 1 HP, 2 BP,
    3 Ladder; pendent 0/1/2 = 12/24/36 dB. Destins LFO: 0 to, 1 tall, 2 forma, 3 volum, 4 FM, 5 pan.
    Delay: 0 1/16, 1 1/8T, 2 1/8, 3 1/8., 4 1/4T, 5 1/4, 6 1/4., 7 1/2. Mode: 0 mono, 1 legato, 2 poli. */
namespace mibase::lead
{
    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c {
            "Classic Lead", "Analog Lead", "Moog Lead", "Prophet Lead", "Juno Lead", "ARP Lead", "Oberheim Style", "Synthwave Lead",
            "80s Lead", "Modern Pop Lead", "Indie Lead", "Stadium Lead", "Supersaw Lead", "Bright Lead", "Soft Lead", "Fat Lead",
            "Distorted Lead", "Acid Lead", "FM Lead", "Digital Lead", "Ambient Lead", "Cinematic Lead", "Pluck Lead", "Solo Lead",
            "Sequence Lead", "Expressive Lead", "Experimental Lead" };
        return c;
    }

    namespace
    {
        // vibrato que entra a poc a poc, compartit per molts presets
        #define VIB " l1d=0 l1q=0.09 l1del=550"
        struct P { const char* cat; const char* nom; const char* desc; const char* canvis; };
        const P taula[] = {
            // ---- Classic Lead ----
            { "Classic Lead", "Serra clàssica", "Dues serres lleugerament desafinades i filtre a mig obrir: el lead de sempre.", "cut=2600 res=0.25 fenv=0.35 vol=-2.0" VIB },
            { "Classic Lead", "Quadrada clàssica", "Quadrada amb un punt de vibrato: buida i cantant.", "o1w=3 o2w=3 o2f=5 cut=2200 res=0.2 vol=-6.5" VIB },
            { "Classic Lead", "Lead amb glide", "Legato amb portament: les notes lligades llisquen.", "glide=90 cut=2400 res=0.3 vol=-2.0" VIB },
            { "Classic Lead", "Clàssic tallant", "Filtre obert i ressonant: travessa la mescla.", "cut=4500 res=0.45 fenv=0.25 key=0.7 vol=-2.5" VIB },
            { "Classic Lead", "Clàssic d'octaves", "L'oscil·lador 2 una octava amunt: més presència.", "o2o=1 o2f=3 n2=0.5 cut=3000 res=0.2 vol=-0.5" VIB },
            // ---- Analog Lead ----
            { "Analog Lead", "Analògic viu", "Deriva analògica i fase lliure: cada nota és una mica diferent.", "deriva=0.5 o2f=9 cut=2000 res=0.3 fdrv=0.3 vol=-4.5" VIB },
            { "Analog Lead", "Analògic calent", "Ladder de 24 dB, saturació i tres oscil·ladors.", "ft=3 deriva=0.4 n3=0.4 o3w=2 o3o=0 o3f=-6 cut=1600 res=0.3 fdrv=0.5 fenv=0.45 vol=-0.5" VIB },
            { "Analog Lead", "Analògic pols", "Pols amb PWM lenta de l'LFO 2.", "o1w=4 o2w=4 o2f=6 l2d=2 l2q=0.5 l2r=0.5 cut=2400 deriva=0.3 vol=-5.5" VIB },
            { "Analog Lead", "Analògic vintage", "Desafinat, fosc i inestable: com un sinte vell.", "deriva=0.9 o2f=14 cut=1300 res=0.25 nz=0.04 nzc=0.6 vol=-2.0" VIB },
            { "Analog Lead", "Analògic agut", "Seguiment de teclat al màxim: com més amunt, més brilla.", "key=1 cut=1200 res=0.35 fenv=0.3 deriva=0.3 vol=-2.0" VIB },
            // ---- Moog Lead ----
            { "Moog Lead", "Ladder cantant", "Tres serres pel Ladder saturat amb glide: el lead de solo clàssic.", "ft=3 n3=0.6 o3w=2 o3o=0 o3f=-9 o2f=8 cut=1400 res=0.4 fenv=0.5 fd=700 fs=0.5 fdrv=0.6 glide=70 vol=-2.0" VIB },
            { "Moog Lead", "Ladder rodó", "Quadrada i triangle pel Ladder: gruixut i suau.", "ft=3 o1w=3 o2w=1 o2o=1 cut=1100 res=0.3 fenv=0.4 fdrv=0.5 vol=-3.5" VIB },
            { "Moog Lead", "Ladder xiulant", "Ressonància alta i filtre que segueix el teclat.", "ft=3 cut=1800 res=0.72 key=0.8 fenv=0.2 fdrv=0.4 glide=50 vol=1.5" VIB },
            { "Moog Lead", "Ladder de rock", "Ladder empès amb overdrive: per a solos de rock progressiu.", "ft=3 n3=0.5 o3w=3 o3o=0 cut=2000 res=0.35 fdrv=0.8 dt=1 dq=0.35 dto=0.6 glide=60 vol=-8.0" VIB },
            { "Moog Lead", "Ladder de 12 dB", "Ladder de 2 pols: més obert i més aspre.", "ft=3 fp=0 cut=1700 res=0.45 fenv=0.4 fdrv=0.5 vol=-1.0" VIB },
            // ---- Prophet Lead ----
            { "Prophet Lead", "Polifònic sync", "Sync de l'oscil·lador 2 amb l'envolupant de to: el «yoi» clàssic.", "sync2=1 o2o=1 o2s=7 pe=12 pet=220 n1=0.3 n2=0.8 cut=5000 res=0.15 vol=0.0" VIB },
            { "Prophet Lead", "Polifònic de vidre", "Pols estret i serra, filtre net de 24 dB.", "o1w=4 o1sh=0.4 cut=3200 res=0.3 fenv=0.25 chm=0.25 vol=-2.5" VIB },
            { "Prophet Lead", "Poly mod", "L'oscil·lador 3 modula l'1 (FM): metall suau.", "fm31=0.35 n3=0.0 o3w=1 o3o=1 cut=3000 res=0.2 vol=-2.0" VIB },
            { "Prophet Lead", "Sync amb roda", "Sync, i la roda obre el filtre.", "sync2=1 o2o=1 o2s=3 n1=0.2 n2=0.8 cut=1500 mwc=0.8 mwv=0 res=0.3 vol=1.0" },
            { "Prophet Lead", "Polifònic suau", "Mode poli amb atac curt i reverb: per a melodies a dues veus.", "mode=2 cut=2200 res=0.2 ar=500 rvm=0.2 rvs=0.4 vol=-3.5" VIB },
            // ---- Juno Lead ----
            { "Juno Lead", "Pols amb chorus", "Pols i sub, amb chorus: el so dels polifònics amb un sol oscil·lador.", "o1w=4 o1sh=0.3 n2=0 subn=0.5 chm=0.6 chr=0.6 cut=2600 res=0.25 l2d=2 l2q=0.3 vol=-3.0" VIB },
            { "Juno Lead", "Serra amb chorus", "Serra, chorus intens i delay curt.", "n2=0 chm=0.7 chp=0.7 cut=2800 res=0.2 dlm=0.18 dlt=2 vol=1.0" VIB },
            { "Juno Lead", "Juno de solo", "Pols ressonant, glide i chorus.", "o1w=4 o1sh=0.5 n2=0 cut=1900 res=0.5 glide=60 chm=0.45 vol=-2.0" VIB },
            { "Juno Lead", "Sub i pols", "Pols amb molt de sub: gruixut i net.", "o1w=4 o1sh=0.2 n2=0 subn=0.8 cut=2000 res=0.2 chm=0.4 vol=-3.5" VIB },
            { "Juno Lead", "Chorus brillant", "Filtre obert, chorus i reverb: per a tornades.", "o1w=4 o1sh=0.4 n2=0.4 o2w=2 o2f=0 cut=5000 res=0.15 chm=0.6 rvm=0.2 vol=-1.5" VIB },
            // ---- ARP Lead ----
            { "ARP Lead", "Duo agut", "Dues serres a una quinta: el lead dels anys 70 amb pes.", "o2s=7 o2f=0 n2=0.55 cut=3000 res=0.35 fenv=0.3 vol=-0.5" VIB },
            { "ARP Lead", "Filtre que xiula", "SVF de 12 dB a punt d'autooscil·lar.", "fp=0 cut=1800 res=0.82 key=0.8 fenv=0.25 vol=-3.5" VIB },
            { "ARP Lead", "Mostra i reté", "LFO a l'atzar sobre el tall: el borbolleig clàssic.", "l2w=4 l2d=1 l2q=0.45 l2s=5 cut=1500 res=0.6 fp=0 vol=-2.5" VIB },
            { "ARP Lead", "Quadrada nasal", "Quadrada pel passa-banda: com si sonés per un tub.", "o1w=3 n2=0 ft=2 fp=0 cut=1400 res=0.5 vol=6.0" VIB },
            { "ARP Lead", "Sync de 70", "Sync agressiu amb l'LFO movent el to del 2.", "sync2=1 o2o=1 n1=0.2 n2=0.8 l2d=2 l2q=0 cut=4000 pe=7 pet=400 vol=0.0" VIB },
            // ---- Oberheim Style ----
            { "Oberheim Style", "Gros i brassy", "Dues serres obertes, filtre de 12 dB i atac de metall.", "fp=0 o2f=11 cut=1300 res=0.25 fenv=0.6 fa=40 fd=400 fs=0.4 aa=15 deriva=0.4 vol=-2.0" VIB },
            { "Oberheim Style", "Saltador", "Poli brassy amb chorus per a riffs de teclat.", "mode=2 fp=0 o2f=9 cut=1500 res=0.2 fenv=0.55 fd=300 fs=0.35 chm=0.3 vol=-1.0" },
            { "Oberheim Style", "Pols ample", "Pols desafinats en unison.", "o1w=4 o2w=4 o2f=12 uni=2 unid=0.25 fp=0 cut=2200 res=0.2 deriva=0.4 vol=-9.0" VIB },
            { "Oberheim Style", "Brass lead", "Atac de filtre lent: com un metall que s'obre.", "fp=0 cut=900 res=0.15 fenv=0.7 fa=120 fd=800 fs=0.5 aa=25 o2f=10 vol=-2.0" VIB },
            { "Oberheim Style", "Ample i fosc", "Tres serres i filtre tancat: un mur de so.", "n3=0.5 o3w=2 o3o=0 o3f=-12 fp=0 cut=900 res=0.2 fenv=0.4 deriva=0.5 width=1.2 vol=-3.0" VIB },
            // ---- Synthwave Lead ----
            { "Synthwave Lead", "Neó", "Serres amb chorus i delay amb punt: el lead de la nit.", "o2f=10 cut=2600 res=0.25 chm=0.4 dlm=0.3 dlt=3 dlf=0.45 rvm=0.2 vol=-0.5" VIB },
            { "Synthwave Lead", "Carretera", "Quadrada i serra amb glide i molt d'espai.", "o1w=3 o2f=6 glide=80 cut=2200 dlm=0.35 dlt=3 rvm=0.3 rvs=0.7 vol=-4.0" VIB },
            { "Synthwave Lead", "Solo retro", "Ladder, glide i delay: per a solos lents.", "ft=3 cut=1500 res=0.35 fdrv=0.4 glide=100 dlm=0.3 dlt=6 rvm=0.25 vol=-3.0" VIB },
            { "Synthwave Lead", "Arpegi de neó", "Arpegiador a semicorxeres amb delay: prem un acord.", "mode=2 arp=1 arpr=3 arpo=2 arpg=0.45 ad=250 as=0.2 ar=150 cut=2500 dlm=0.25 dlt=3 vol=0.0" },
            { "Synthwave Lead", "Pols nocturn", "Pols amb PWM, chorus i reverb gran.", "o1w=4 o2w=4 o2f=7 l2d=2 l2q=0.4 chm=0.5 rvm=0.35 rvs=0.8 cut=2000 vol=-4.0" VIB },
            // ---- 80s Lead ----
            { "80s Lead", "Sync dels 80", "Sync amb envolupant de to i chorus.", "sync2=1 o2o=1 o2s=5 pe=10 pet=180 n1=0.2 n2=0.8 chm=0.3 cut=6000 vol=2.5" VIB },
            { "80s Lead", "Digital dels 80", "FM brillant amb una cua curta: com els teclats digitals.", "o1w=0 o2w=0 o2f=0 n1=0 n2=0.8 fm12=0.5 o1o=1 cut=8000 fenv=0.2 ad=900 as=0.5 chm=0.25 vol=-2.0" VIB },
            { "80s Lead", "Lead de pop 80", "Quadrada, pols i chorus: alegre.", "o1w=3 o2w=4 o2o=1 o2f=0 n2=0.4 cut=3500 chm=0.45 rvm=0.15 vol=-4.0" VIB },
            { "80s Lead", "Flauta sintètica", "Triangle amb soroll d'aire.", "o1w=1 n2=0 nz=0.08 nzc=0.2 cut=4000 res=0 aa=30 ar=300 rvm=0.2 vol=2.0" VIB },
            { "80s Lead", "Hook de vuitanta", "Quadrada curta amb delay rítmic: per a hooks.", "o1w=3 o2w=3 o2o=1 n2=0.4 ad=350 as=0.4 cut=3000 dlm=0.3 dlt=2 vol=-4.0" },
            // ---- Modern Pop Lead ----
            { "Modern Pop Lead", "Pop net", "Supersaw estret i filtre obert: definit i modern.", "o1w=5 o1sh=0.3 n2=0 cut=6000 res=0.1 chm=0.15 rvm=0.15 vol=1.5" VIB },
            { "Modern Pop Lead", "Pop pluck-lead", "Atac net i caiguda: es retalla bé a la mescla.", "o1w=5 o1sh=0.35 n2=0 ad=400 as=0.45 fenv=0.4 fd=300 cut=2500 dlm=0.2 dlt=2 vol=4.5" },
            { "Modern Pop Lead", "Pop vocal", "Wavetable vocal: sembla una veu que diu «aa».", "o1w=6 o1t=2 o1sh=0.1 o2w=6 o2t=2 o2sh=0.15 o2f=6 cut=7000 res=0 l2d=2 l2q=0.15 vol=4.0" VIB },
            { "Modern Pop Lead", "Pop sense filtre", "Sinus plegat amb harmònics: brillant però dolç.", "o1w=0 o1sh=0.35 n2=0 cut=12000 res=0 fenv=0 rvm=0.15 vol=1.0" VIB },
            { "Modern Pop Lead", "Pop amb octava", "Serra i sinus una octava amunt: hook de tornada.", "o2w=0 o2o=1 o2f=0 n2=0.5 cut=5000 res=0.1 dlm=0.15 vol=-2.0" VIB },
            // ---- Indie Lead ----
            { "Indie Lead", "Indie desafinat", "Dues serres molt desafinades i deriva: imperfecte a propòsit.", "o2f=22 deriva=0.7 cut=2000 res=0.2 chm=0.3 vol=-2.0" VIB },
            { "Indie Lead", "Indie de joguina", "Quadrada petita i aguda: com un teclat de joguina.", "o1w=3 o1o=1 n2=0 cut=5000 res=0 ad=300 as=0.6 deriva=0.4 vol=-3.5" VIB },
            { "Indie Lead", "Indie amb fuzz", "Fuzz suau i to fosc: guitarra-sinte.", "dt=3 dq=0.4 dto=0.4 cut=3000 res=0.1 rvm=0.15 vol=-3.5" VIB },
            { "Indie Lead", "Indie tremolós", "Trèmolo amb el tempo i reverb.", "o1w=1 o2w=2 n2=0.4 l2d=3 l2q=0.6 l2s=4 l2w=0 cut=3000 rvm=0.3 rvs=0.6 vol=-2.0" },
            { "Indie Lead", "Indie llunyà", "Filtre passa-banda i delay llarg: sona de lluny.", "ft=2 fp=0 cut=1600 res=0.3 dlm=0.35 dlt=6 dlf=0.5 rvm=0.3 vol=5.0" VIB },
            // ---- Stadium Lead ----
            { "Stadium Lead", "Estadi ample", "Supersaw ample amb delay amb punt i reverb gran.", "o1w=5 o1sh=0.5 n2=0 width=1.3 cut=5000 dlm=0.3 dlt=3 dlf=0.4 rvm=0.3 rvs=0.8 vol=-1.0" VIB },
            { "Stadium Lead", "Himne", "Serres en unison i octava: per a melodies de tornada grans.", "uni=3 unid=0.25 o2o=1 n2=0.5 cut=4500 res=0.1 rvm=0.3 rvs=0.75 dlm=0.2 vol=-2.0" VIB },
            { "Stadium Lead", "Corxeres d'estadi", "Arpegiador amb el patró «Repeteix» a corxeres i delay: pols de concert (inspiració general).", "arp=7 arpp=15 arpr=1 arpg=0.7 o1w=5 o1sh=0.4 n2=0 cut=4000 dlm=0.3 dlt=3 rvm=0.3 vol=-0.5" },
            { "Stadium Lead", "Llum de mòbils", "Quadrada suau amb chorus i reverb molt gran.", "o1w=3 o2w=1 o2o=1 n2=0.4 cut=3000 aa=40 ar=900 chm=0.4 rvm=0.4 rvs=0.9 vol=-5.5" VIB },
            { "Stadium Lead", "Crit d'estadi", "Supersaw saturat i obert per sobre de guitarres.", "o1w=5 o1sh=0.6 n2=0 dt=0 dq=0.4 cut=6000 res=0.15 rvm=0.25 width=1.2 vol=-7.0" VIB },
            // ---- Supersaw Lead ----
            { "Supersaw Lead", "Supersaw", "Set serres desafinades: el lead del trance.", "o1w=5 o1sh=0.55 n2=0 cut=7000 res=0.1 rvm=0.2 vol=1.0" VIB },
            { "Supersaw Lead", "Supersaw doble", "Dos supersaws a l'octava.", "o1w=5 o1sh=0.5 o2w=5 o2o=1 o2sh=0.4 n2=0.45 cut=7000 rvm=0.2 width=1.2 vol=-1.0" VIB },
            { "Supersaw Lead", "Supersaw tancat", "Supersaw que s'obre amb l'envolupant.", "o1w=5 o1sh=0.5 n2=0 cut=800 res=0.3 fenv=0.6 fa=200 fd=900 fs=0.5 vol=0.5" VIB },
            { "Supersaw Lead", "Supersaw de festival", "Supersaw, sub i delay: per a drops.", "o1w=5 o1sh=0.7 n2=0 subn=0.4 cut=9000 dlm=0.25 dlt=3 rvm=0.25 width=1.3 vol=0.5" VIB },
            { "Supersaw Lead", "Supersaw estret", "Poc obert: més definit, per a melodies ràpides.", "o1w=5 o1sh=0.15 n2=0 cut=5000 res=0.2 glide=30 vol=0.5" VIB },
            // ---- Bright Lead ----
            { "Bright Lead", "Brillant", "Filtre passa-alts suau: només la part de dalt.", "ft=1 fp=0 cut=300 res=0.1 o2o=1 n2=0.5 vol=-1.0" VIB },
            { "Bright Lead", "Vidre", "Wavetable de campana i sinus a dalt.", "o1w=6 o1t=5 o1sh=0.3 o2w=0 o2o=1 o2f=0 n2=0.4 cut=12000 res=0 ad=1500 as=0.5 vol=5.5" VIB },
            { "Bright Lead", "Tallant", "Pols estret i ressonància: talla com un ganivet.", "o1w=4 o1sh=0.8 o2w=4 o2sh=0.7 cut=5000 res=0.5 key=0.8 vol=-5.5" VIB },
            { "Bright Lead", "Xiulet", "Sinus agut amb portament: com un xiulet.", "o1w=0 o1o=1 n2=0 cut=12000 res=0 glide=60 aa=20 vol=-0.5" VIB },
            { "Bright Lead", "Brillant amb sync", "Sync alt i filtre obert.", "sync2=1 o2o=2 o2s=0 n1=0.2 n2=0.8 cut=9000 vol=0.5" VIB },
            // ---- Soft Lead ----
            { "Soft Lead", "Suau", "Triangle i sinus amb atac lent: per a melodies emotives.", "o1w=1 o2w=0 o2o=1 o2f=0 n2=0.3 cut=3000 res=0 aa=60 ar=700 rvm=0.25 vol=0.5" VIB },
            { "Soft Lead", "Flauta", "Sinus amb aire i vibrato que entra a poc a poc.", "o1w=0 n2=0 nz=0.06 nzc=0.3 aa=50 ar=400 cut=6000 res=0 l1q=0.1 l1del=800 rvm=0.2 vol=0.0" },
            { "Soft Lead", "Coixí cantant", "Serres filtrades i chorus: suau però amb cos.", "cut=900 res=0.1 fenv=0.15 aa=80 ar=900 chm=0.5 rvm=0.3 vol=-2.0" VIB },
            { "Soft Lead", "Veu suau", "Wavetable vocal amb la posició que es mou.", "o1w=6 o1t=2 o1sh=0.7 n2=0 l2d=2 l2q=0.2 l2r=0.3 cut=5000 aa=80 ar=600 rvm=0.25 vol=6.0" VIB },
            { "Soft Lead", "Ocarina", "Triangle buit amb un toc de soroll.", "o1w=1 o1o=1 n2=0 nz=0.04 cut=5000 res=0 aa=25 ar=250 rvm=0.2 vol=0.0" VIB },
            // ---- Fat Lead ----
            { "Fat Lead", "Gras", "Tres oscil·ladors desafinats i sub: el més gruixut.", "n3=0.6 o3w=2 o3o=0 o3f=-14 o2f=14 subn=0.5 cut=2000 res=0.2 fdrv=0.4 vol=-7.0" VIB },
            { "Fat Lead", "Unison gras", "Unison de 5 amb sub.", "uni=5 unid=0.3 subn=0.4 cut=2400 res=0.15 width=1.2 vol=-3.0" VIB },
            { "Fat Lead", "Quadrades grasses", "Tres quadrades a octaves.", "o1w=3 o2w=3 o2o=1 o3w=3 o3o=-1 n3=0.6 cut=2000 res=0.2 vol=-7.5" VIB },
            { "Fat Lead", "Gras saturat", "Ladder saturat i sub: dens.", "ft=3 subn=0.6 fdrv=0.8 dt=0 dq=0.3 cut=1500 res=0.3 vol=-10.5" VIB },
            { "Fat Lead", "Gras d'octaves", "Sub, serra i serra a l'octava: ocupa tot l'espectre.", "o2o=1 n2=0.5 subn=0.6 subo=0 cut=3000 res=0.1 vol=-2.5" VIB },
            // ---- Distorted Lead ----
            { "Distorted Lead", "Distorsió", "Serres per la distorsió: per a rock electrònic.", "dt=2 dq=0.6 dto=0.55 cut=3500 res=0.2 vol=-6.0" VIB },
            { "Distorted Lead", "Fuzz lead", "Quadrada pel fuzz, fosca i grossa.", "o1w=3 dt=3 dq=0.7 dto=0.4 cut=3000 vol=-0.5" VIB },
            { "Distorted Lead", "Guitarra sintètica", "Sync i overdrive: sona com una guitarra elèctrica.", "sync2=1 o2o=1 o2s=0 n1=0.3 n2=0.8 dt=1 dq=0.65 dto=0.5 cut=4000 glide=40 vol=-6.0" VIB },
            { "Distorted Lead", "Crit", "Ladder ressonant saturat i distorsió: agressiu.", "ft=3 res=0.6 fdrv=0.9 dt=2 dq=0.5 cut=2000 key=0.6 vol=-7.0" VIB },
            { "Distorted Lead", "Trencat", "Sinus plegat i fuzz al màxim.", "o1w=0 o1sh=0.8 n2=0 dt=3 dq=0.9 dto=0.6 cut=6000 vol=2.0" VIB },
            // ---- Acid Lead ----
            { "Acid Lead", "Àcid agut", "Serra pel filtre de 12 dB molt ressonant, amb glide.", "fp=0 cut=700 res=0.82 fenv=0.65 fd=200 fs=0 n2=0 glide=60 vf=0.6 vol=0.0" },
            { "Acid Lead", "Àcid seqüenciat", "El patró «Àcid 1» amb lligades i accents: prem una tecla.", "fp=0 cut=600 res=0.8 fenv=0.65 fd=180 fs=0 n2=0 glide=55 arp=7 arpp=4 arpr=3 arpg=0.6 vf=0.6 vol=-1.0" },
            { "Acid Lead", "Àcid quadrat", "Quadrada amb seqüència «Àcid 2» i distorsió.", "o1w=3 fp=0 cut=550 res=0.78 fenv=0.7 fd=200 fs=0 n2=0 glide=55 arp=7 arpp=5 arpr=3 arpg=0.6 dt=1 dq=0.4 vol=-12.5" },
            { "Acid Lead", "Àcid wobble", "L'LFO obre el filtre a corxeres.", "fp=0 cut=800 res=0.75 fenv=0.3 n2=0 l2d=1 l2q=0.4 l2s=4 l2w=1 glide=50 vol=0.0" },
            { "Acid Lead", "Àcid amb delay", "Àcid amb ecos a corxera amb punt.", "fp=0 cut=650 res=0.8 fenv=0.6 fd=180 fs=0 n2=0 glide=55 dlm=0.35 dlt=3 dlf=0.5 vol=-0.5" },
            // ---- FM Lead ----
            { "FM Lead", "FM clàssic", "Dos sinus en FM: lead digital rodó.", "o1w=0 o2w=0 o2f=0 n1=0 n2=0.85 fm12=0.45 cut=12000 res=0 fenv=0 vol=-1.5" VIB },
            { "FM Lead", "FM metàl·lic", "Modulador a l'octava i quinta: campana.", "o1w=0 o1o=1 o1s=7 o2w=0 o2f=0 n1=0 n2=0.85 fm12=0.55 cut=12000 res=0 ad=1500 as=0.4 vol=-1.5" VIB },
            { "FM Lead", "FM amb envolupant", "L'LFO 2 mou la FM: el timbre respira.", "o1w=0 o2w=0 o2f=0 n1=0 n2=0.85 fm12=0.25 l2d=4 l2q=0.5 l2r=0.5 cut=12000 res=0 vol=-2.0" VIB },
            { "FM Lead", "FM saxo", "FM amb atac de filtre: semblant a un vent.", "o1w=0 o2w=2 o2f=0 n1=0 n2=0.8 fm12=0.3 cut=1500 res=0.2 fenv=0.5 fa=40 aa=20 vol=2.5" VIB },
            { "FM Lead", "FM doble", "Les dues FM alhora (3→1→2).", "o1w=0 o2w=0 o3w=0 o3o=2 o2f=0 n1=0 n2=0.85 fm12=0.4 fm31=0.3 cut=10000 res=0 vol=0.0" VIB },
            // ---- Digital Lead ----
            { "Digital Lead", "Wavetable digital", "La taula «Digital» amb la posició que es mou.", "o1w=6 o1t=4 o1sh=0.4 n2=0 l2d=2 l2q=0.35 l2r=0.25 cut=8000 res=0.1 vol=5.5" VIB },
            { "Digital Lead", "Harmònics", "La taula «Harmònics»: l'envolupant no, l'LFO obre els harmònics.", "o1w=6 o1t=3 o1sh=0.3 o2w=6 o2t=3 o2sh=0.5 o2f=8 l2d=2 l2q=0.4 l2s=3 cut=10000 vol=-2.0" VIB },
            { "Digital Lead", "Sync digital", "La taula «Sync»: sync sense oscil·lador 2.", "o1w=6 o1t=6 o1sh=0.5 n2=0 cut=8000 l2d=2 l2q=0.3 vol=2.5" VIB },
            { "Digital Lead", "Bits", "Quadrada aguda, filtre obert i delay de corxera: videojoc.", "o1w=3 o1o=1 n2=0 cut=14000 res=0 ad=200 as=0.6 ar=60 dlm=0.2 dlt=2 vol=-3.0" },
            { "Digital Lead", "Metall digital", "La taula «Metall».", "o1w=6 o1t=7 o1sh=0.6 n2=0 cut=9000 res=0.1 chm=0.2 vol=0.5" VIB },
            // ---- Ambient Lead ----
            { "Ambient Lead", "Ambient", "Atac lent, delay i reverb enorme.", "o1w=1 o2w=2 n2=0.3 cut=2500 aa=300 ar=2500 dlm=0.35 dlt=6 dlf=0.55 rvm=0.5 rvs=0.95 mode=2 vol=-0.5" VIB },
            { "Ambient Lead", "Boira", "Wavetable vocal i soroll: textura llunyana.", "o1w=6 o1t=2 o1sh=0.6 n2=0 nz=0.1 nzc=0.8 cut=3000 aa=500 ar=3000 rvm=0.55 rvs=0.95 l2d=2 l2q=0.3 l2r=0.1 mode=2 vol=6.0" },
            { "Ambient Lead", "Ecos", "Notes curtes que es multipliquen al delay ping-pong.", "o1w=0 n2=0 ad=300 as=0 ar=300 cut=6000 res=0 dlm=0.45 dlt=3 dlf=0.65 rvm=0.35 vol=3.5" },
            { "Ambient Lead", "Cristall", "Campana suau amb reverb gran.", "o1w=6 o1t=5 o1sh=0.2 n2=0 cut=9000 res=0 aa=10 ad=2500 as=0.3 ar=2500 rvm=0.45 rvs=0.9 vol=6.0" },
            { "Ambient Lead", "Aurora", "Supersaw lent amb chorus i reverb.", "o1w=5 o1sh=0.4 n2=0 cut=1800 aa=700 ar=3000 chm=0.5 rvm=0.5 rvs=0.9 mode=2 vol=2.0" VIB },
            // ---- Cinematic Lead ----
            { "Cinematic Lead", "Tema heroic", "Serres i octava amb reverb de sala gran.", "o2o=1 n2=0.5 cut=3500 res=0.15 aa=30 ar=1200 rvm=0.35 rvs=0.85 vol=1.0" VIB },
            { "Cinematic Lead", "Misteri", "Triangle amb FM lenta i delay.", "o1w=1 o2w=0 o2f=0 fm12=0.15 l2d=4 l2q=0.3 l2r=0.1 cut=3000 dlm=0.3 dlt=6 rvm=0.4 vol=6.0" VIB },
            { "Cinematic Lead", "Ciència-ficció", "Sinus amb glide llarg i ring: so d'espai.", "o1w=0 o2w=0 o2s=7 n2=0 ring=0.4 glide=250 cut=8000 rvm=0.4 dlm=0.25 vol=-1.0" VIB },
            { "Cinematic Lead", "Tensió", "Pols amb vibrato ràpid i fosc.", "o1w=4 o2w=4 o2s=1 o2f=0 n2=0.4 cut=1200 res=0.4 l1d=0 l1q=0.12 l1r=8 rvm=0.35 vol=-3.5" },
            { "Cinematic Lead", "Llunyania", "Passa-banda amb reverb: una melodia a l'horitzó.", "ft=2 fp=1 cut=1800 res=0.3 aa=100 ar=1500 rvm=0.5 rvs=0.9 vol=6.0" VIB },
            // ---- Pluck Lead ----
            { "Pluck Lead", "Pluck", "Atac i caiguda curta: per a melodies rítmiques.", "ad=300 as=0 ar=250 fenv=0.6 fd=200 fs=0 cut=1500 res=0.3 mode=2 vol=2.5" },
            { "Pluck Lead", "Pluck amb delay", "Pluck amb delay de corxera amb punt.", "ad=280 as=0 ar=250 fenv=0.55 fd=180 fs=0 cut=1800 res=0.25 dlm=0.35 dlt=3 mode=2 vol=2.5" },
            { "Pluck Lead", "Marimba", "Sinus amb FM curta: percussiu de fusta.", "o1w=0 o2w=0 o2o=2 o2f=0 n1=0.8 n2=0 fm31=0 ad=350 as=0 ar=300 cut=8000 res=0 fenv=0 mode=2 clic=0 vol=1.5" },
            { "Pluck Lead", "Pluck supersaw", "Supersaw curt: el pluck del pop electrònic.", "o1w=5 o1sh=0.45 n2=0 ad=300 as=0 ar=300 fenv=0.6 fd=220 fs=0 cut=1500 rvm=0.25 mode=2 vol=6.0" },
            { "Pluck Lead", "Pizzicato", "Triangle i pols curts: corda pinçada.", "o1w=1 o2w=4 o2sh=0.3 n2=0.4 ad=200 as=0 ar=200 cut=4000 res=0 mode=2 vol=3.5" },
            // ---- Solo Lead ----
            { "Solo Lead", "Solo de rock", "Sync, overdrive i glide: per a solos llargs.", "sync2=1 o2o=1 o2s=0 n1=0.4 n2=0.7 dt=1 dq=0.5 cut=4000 glide=60 dlm=0.2 dlt=3 rvm=0.2 vol=-7.5" VIB },
            { "Solo Lead", "Solo cantant", "Ladder amb glide i vibrato expressiu (roda i aftertouch).", "ft=3 cut=1800 res=0.35 fdrv=0.5 glide=80 mwv=0.7 atv=0.6 dlm=0.2 rvm=0.2 vol=-2.0" VIB },
            { "Solo Lead", "Solo de jazz", "Triangle i quadrada, fosc i rodó.", "o1w=1 o2w=3 o2f=0 n2=0.3 cut=1600 res=0.1 aa=10 rvm=0.15 vol=6.0" VIB },
            { "Solo Lead", "Solo prog", "Tres oscil·ladors, Ladder i glide llarg.", "ft=3 n3=0.5 o3w=3 o3o=1 cut=2000 res=0.35 fdrv=0.5 glide=150 vol=-0.5" VIB },
            { "Solo Lead", "Solo de fusió", "Pols amb chorus i delay: suau i ràpid.", "o1w=4 o1sh=0.3 n2=0 cut=2800 chm=0.35 dlm=0.25 dlt=2 glide=30 vol=-2.5" VIB },
            // ---- Sequence Lead ----
            { "Sequence Lead", "Seqüència berlinesa", "El patró «Berlinesa» a semicorxeres: prem una tecla i toca sol.", "arp=7 arpp=2 arpr=3 arpg=0.5 ad=200 as=0.1 ar=120 fenv=0.5 fd=150 fs=0 cut=1200 res=0.45 dlm=0.2 vol=0.5" },
            { "Sequence Lead", "Trance", "El patró «Trance» amb supersaw i delay.", "arp=7 arpp=6 arpr=3 arpg=0.5 o1w=5 o1sh=0.4 n2=0 ad=180 as=0.1 cut=3000 dlm=0.3 dlt=3 vol=1.5" },
            { "Sequence Lead", "Arpegi amunt", "Prem un acord: l'arpegiador el toca amunt en dues octaves.", "mode=2 arp=1 arpo=2 arpr=3 arpg=0.5 ad=250 as=0.2 cut=2500 dlm=0.2 dlt=2 vol=-0.5" },
            { "Sequence Lead", "Arpegi amunt-avall", "Arpegi amunt i avall a tresets.", "mode=2 arp=3 arpo=2 arpr=4 arpg=0.5 o1w=4 o1sh=0.3 ad=200 as=0.2 cut=3000 chm=0.3 vol=-1.0" },
            { "Sequence Lead", "Hook seqüenciat", "El patró «Hook» amb lligades: una melodia que toca sola.", "arp=7 arpp=14 arpr=3 arpg=0.65 glide=40 cut=2500 res=0.3 dlm=0.2 dlt=3 vol=-1.0" },
            // ---- Expressive Lead ----
            { "Expressive Lead", "Expressiu", "La velocitat obre molt el filtre i l'aftertouch fa vibrato.", "vf=0.9 vv=0.7 atv=0.8 atc=0.4 cut=1200 res=0.3 fenv=0.4 vol=-1.5" VIB },
            { "Expressive Lead", "Respiració", "L'aftertouch obre el filtre: com bufar més fort.", "atc=0.9 atv=0.3 cut=700 res=0.2 fenv=0.1 aa=40 vol=-1.5" VIB },
            { "Expressive Lead", "Violí sintètic", "Serra amb atac lent i vibrato amb retard.", "n2=0.5 o2f=5 cut=3000 res=0.1 aa=90 ar=400 l1q=0.12 l1del=700 l1r=5.8 rvm=0.25 vol=-2.0" },
            { "Expressive Lead", "Portament lliure", "Glide llarg: llisca entre totes les notes lligades.", "glide=250 cut=2500 res=0.25 vol=-2.0" VIB },
            { "Expressive Lead", "Roda de filtre", "La roda de modulació obre el filtre a fons.", "mwc=1 mwv=0 cut=500 res=0.5 fenv=0.1 vol=-2.0" VIB },
            // ---- Experimental Lead ----
            { "Experimental Lead", "Ring", "Modulació en anell: dissonant i metàl·lic.", "o1w=0 o2w=2 o2s=6 o2f=0 n1=0.3 n2=0.2 ring=0.9 cut=6000 vol=-0.5" VIB },
            { "Experimental Lead", "Glitch", "LFO a l'atzar sobre el to amb el tempo.", "l2w=4 l2d=0 l2q=0.3 l2s=5 cut=4000 res=0.3 dlm=0.2 vol=-2.0" },
            { "Experimental Lead", "Robot", "FM i passa-banda: veu de robot.", "o1w=3 o2w=6 o2t=2 o2f=0 fm12=0.3 ft=2 fp=1 cut=1500 res=0.6 l2d=1 l2q=0.3 l2s=5 vol=5.5" },
            { "Experimental Lead", "Planeta", "Sync amb l'LFO al to del 2 i delay.", "sync2=1 o2o=1 n1=0.1 n2=0.8 l2d=2 l2q=0.6 l2r=0.2 cut=6000 dlm=0.35 dlt=4 rvm=0.3 vol=-1.0" },
            { "Experimental Lead", "Soroll filtrat", "Soroll pel passa-banda molt ressonant: xiulet de vent.", "o1w=7 n2=0 ft=2 fp=1 cut=1200 res=0.85 key=1 aa=40 rvm=0.3 vol=3.5" },
        };
        #undef VIB

        void aplica (Lead& s, const char* canvis)
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
                auto s = std::make_unique<Lead>();
                aplica (*s, x.canvis);
                r.push_back ({ x.nom, x.cat, x.desc, s->estatSo() });
            }
            return r;
        }();
        return v;
    }

    std::string estatInicial() { auto s = std::make_unique<Lead>(); return s->estatSo(); }

    std::string aleatori (unsigned llavor)
    {
        Atzar a (llavor);
        auto sp = std::make_unique<Lead>();
        auto& s = *sp;
        static const int ones1[] = { 2, 2, 2, 3, 4, 5, 6, 0, 1 };
        s.posa (O1Ona, (float) ones1[a.i (0, 8)]);
        s.posa (O1Forma, a.r (0.0f, 0.6f));
        s.posa (O1Taula, (float) a.i (0, 7));
        if (a.p (0.65f))
        {
            static const float semis[] = { 0, 0, 0, 7, 12, 0, 5 };
            s.posa (O2Ona, (float) ones1[a.i (0, 8)]);
            s.posa (O2Oct, a.p (0.3f) ? 1.0f : 0.0f);
            s.posa (O2Semi, semis[a.i (0, 6)]);
            s.posa (O2Fi, a.r (3.0f, 18.0f) * (a.p (0.5f) ? 1.0f : -1.0f));
            s.posa (O2Forma, a.r (0.0f, 0.6f));
            s.posa (O2Taula, (float) a.i (0, 7));
            s.posa (Niv2, a.r (0.3f, 0.7f));
        }
        else s.posa (Niv2, 0.0f);
        s.posa (Niv3, a.p (0.25f) ? a.r (0.2f, 0.5f) : 0.0f);
        s.posa (Sync2, a.p (0.12f) ? 1.0f : 0.0f);
        if (s.valor (Sync2) > 0.5f) { s.posa (O2Oct, 1.0f); s.posa (Niv2, 0.8f); s.posa (Niv1, 0.3f); }
        s.posa (Fm12, a.p (0.12f) ? a.r (0.1f, 0.5f) : 0.0f);
        if (a.p (0.2f)) { s.posa (Unison, (float) a.i (2, 4)); s.posa (UniDetune, a.r (0.1f, 0.35f)); }
        s.posa (Deriva, a.p (0.4f) ? a.r (0.1f, 0.6f) : 0.0f);
        s.posa (SubNiv, a.p (0.25f) ? a.r (0.2f, 0.5f) : 0.0f);
        s.posa (FTipus, a.p (0.75f) ? (a.p (0.5f) ? 0.0f : 3.0f) : (float) a.i (1, 2));
        s.posa (FPendent, (float) a.i (0, 2));
        s.posa (FTall, a.lg (800.0f, 6000.0f));
        s.posa (FRes, a.r (0.05f, 0.6f));
        s.posa (FEnv, a.r (0.1f, 0.6f));
        s.posa (AA, a.lg (1.0f, 60.0f));
        s.posa (AS, a.r (0.4f, 1.0f));
        s.posa (AR, a.lg (80.0f, 900.0f));
        s.posa (L1Quant, a.r (0.05f, 0.12f));
        s.posa (L1Retard, a.r (200.0f, 900.0f));
        if (a.p (0.3f)) { s.posa (L2Desti, (float) a.i (1, 2)); s.posa (L2Quant, a.r (0.15f, 0.4f)); }
        if (a.p (0.3f)) { s.posa (DTipus, (float) a.i (0, 3)); s.posa (DQuant, a.r (0.2f, 0.6f)); }
        s.posa (ChMix, a.p (0.3f) ? a.r (0.2f, 0.6f) : 0.0f);
        s.posa (DlMix, a.p (0.4f) ? a.r (0.15f, 0.35f) : 0.0f);
        s.posa (DlNota, (float) a.i (2, 6));
        s.posa (RvMix, a.p (0.5f) ? a.r (0.1f, 0.35f) : 0.0f);
        s.posa (Glide, a.p (0.4f) ? a.r (30.0f, 120.0f) : 0.0f);
        s.posa (Volum, s.valor (DQuant) > 0.4f ? -7.0f : -4.0f);
        return s.estatSo();
    }

    std::string varia (const std::string& estat, unsigned llavor, float quant)
    {
        Atzar a (llavor);
        auto sp = std::make_unique<Lead>();
        auto& s = *sp;
        s.posaEstat (estat);
        for (int p = 0; p < numParams; ++p)
        {
            const auto& i = info (p);
            if (i.escala == sinte::Enter || p == Volum || p == BendRang || p >= ArpMode) continue;
            if (std::abs (s.valor (p)) < 1e-6f) continue;   // el que està apagat es queda apagat
            s.posa (p, sinte::deNormal (i, sinte::normal (i, s.valor (p)) + (a.u() * 2.0f - 1.0f) * 0.18f * quant));
        }
        return s.estatSo();
    }
}

#include "mibase/SubBass.h"
namespace mibase::lead
{
    /** 0.89: el so que substitueix un dels 8 sons del sinte bàsic antic. */
    std::string estatPerSoAntic (const std::string& id, std::string& instrument)
    {
        struct M { const char* id; bool baix; const char* preset; bool poli; };
        static const M m[] = {
            { "piano", false, "Pluck", true }, { "epiano", false, "Marimba", true }, { "orgue", false, "Quadrades grasses", true },
            { "pad", false, "Aurora", true }, { "baix", true, "Analògic clàssic", false }, { "pluck", false, "Pluck", true },
            { "lead", false, "Serra clàssica", false }, { "cordes", false, "Violí sintètic", true },
        };
        for (const auto& x : m)
        {
            if (id != x.id) continue;
            if (x.baix)
            {
                instrument = subbass::idInstrument;
                for (const auto& p : subbass::presetsFabrica()) if (p.nom == x.preset) return p.estat + ";pnom=" + p.nom;
                return subbass::estatInicial();
            }
            instrument = idInstrument;
            auto s = std::make_unique<Lead>();
            for (const auto& p : presetsFabrica()) if (p.nom == x.preset) { s->posaEstat (p.estat); s->nomPreset = p.nom; }
            if (x.poli) s->posa (Mode, 2.0f);
            return s->estat();
        }
        instrument = idInstrument;
        return estatPerSoAntic ("piano", instrument);
    }

    int migraSintesAntics (Projecte& p)
    {
        static const char* ids[] = { "piano", "epiano", "orgue", "pad", "baix", "pluck", "lead", "cordes" };
        int n = 0;
        for (auto& t : p.pistes)
        {
            if (t.tipus != "midi") continue;
            std::string id;
            if (t.instrument.empty()) id = "piano";
            else if (t.instrument.rfind ("intern:", 0) == 0)
                for (const char* x : ids) if (t.instrument == std::string ("intern:") + x) id = x;
            if (id.empty()) continue;
            t.estatInstrument = estatPerSoAntic (id, t.instrument);
            t.nomInstrument.clear();
            ++n;
        }
        return n;
    }
}
