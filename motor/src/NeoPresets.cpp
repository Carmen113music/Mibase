#include "mibase/Neo.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>

/** 0.93: els presets de fàbrica del NEO, programats un per un (sense copiar cap preset comercial).
    Cada preset és una llista de canvis respecte del so inicial, en un llenguatge curt:
      «clau=valor»         un paràmetre (les claus són les de Neo.cpp)
      «font>destí=q»       una via de la matriu; «font>destí=q@via» l'escala amb una macro o una altra font
      «M3=cut:0.5,es:0.3»  els dos destins propis d'una macro (si no n'hi ha, hi van els de per defecte)
    Fonts: l1 l2 l3 l4 ev ef em vel nota roda at rnd pas o3 · Vies: car mov bri agr amp esp evo mor roda at vel em l1
    Destins: to to1 to2 to3 p1 p2 p3 morph forma warp n1 n2 n3 cut res voc pan vol ud ua fold drv gd gm gs rs nz cr sh ch dl rv es
    Motors (o1m…): 0 wavetable, 1 analògic, 2 harmònic, 3 FM, 4 granular, 5 soroll.
    Taules: 0 Bàsica 1 PWM 2 Vocal 3 Harmònics 4 Digital 5 Campana 6 Sync 7 Metall 8 Cor 9 Cordes 10 Vidre 11 Orgue 12 Aire
            13 Fosc 14 Octaves 15 Formant.  Warp: 1 corba 2 sync 3 mirall 4 PM 5 FM 6 bits.
    Filtre: 0 LP 1 HP 2 BP 3 Notch 4 Ladder 5 Formant 6 Comb+ 7 Comb−.  Ressonadors: 0 harmònic 1 campana 2 metall 3 fusta.
    LFO tempo: 0 lliure 2 1/2 3 1/4 4 1/8 5 1/16 8 1/8T.  Passos/arp ritme: 0 1/4 1 1/8 2 1/8T 3 1/16 4 1/16T 5 1/32.
    Delay: 2 1/8 3 1/8. 5 1/4 6 1/4.  Reverb: 0 plate 1 hall 2 ambient 3 shimmer.
    Les vies dels LFO, dels passos i de l'atzar sense «@» passen per la macro MOVIMENT (al mig, tal com està programat).
    El volum de cada preset està anivellat (mesurat amb un acord de tres notes, o una nota en els monofònics). */
namespace mibase::neo
{
    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c {
            "Future Pop", "Modern Indie", "Hyperpop", "Cinematic Hybrid", "Modern Bass", "Modern Lead", "Modern Pluck",
            "Digital Keys", "Organic", "Glitch", "Texture", "Motion", "Hybrid", "Experimental", "Atmospheric", "Pulses",
            "Rhythmic", "Vocal-like synth", "Metallic", "Morphing", "Sound Design" };
        return c;
    }

    const std::vector<std::string>& tipusAtzar()
    {
        static const std::vector<std::string> t { "Baix", "Lead", "Pluck", "Tecles", "Textura", "Pulsació", "Metàl·lic", "Vocal",
                                                  "Riser", "Impacte", "Glitch", "Morph" };
        return t;
    }

    namespace
    {
        struct P { const char* cat; const char* nom; const char* desc; const char* canvis; };
        // Envolupants que es repeteixen
        #define PLUCK " aa=1 ad=320 as=0 ar=320 fa=1 fd=260 fs=0 fenv=0.55"
        #define KEYS " aa=2 ad=1400 as=0.35 ar=500 fa=1 fd=700 fs=0.25 fenv=0.35"
        #define PAD " aa=900 ad=2500 as=0.85 ar=2500"
        #define BAIX " mode=1 veus=1 o3o=-1"
        const P taula[] = {
            // ================= Future Pop =================
            { "Future Pop", "Acord de vidre", "Wavetable de vidre amb unison obert i un sub suau: els acords brillants del pop electrònic d'ara.",
              "o1t=10 o1p=0.35 uni=5 unid=0.32 unia=0.8 subn=0.25 cut=7000 res=0.1 fenv=0.25 fd=900 fs=0.5" KEYS " chm=0.25 rvm=0.22 rvs=0.55 l1r=0.25 l1>p1=0.12 vol=-6.0" },
            { "Future Pop", "Supersaw tou", "Supersaw de 7 veus pel ladder, sense punxa: un mur d'acords que deixa espai a la veu.",
              "o1m=1 o1f=0.33 uni=7 unim=1 unid=0.38 unia=0.9 unib=0.7 ft=4 cut=3200 res=0.15 fenv=0.25" PAD " aa=40 ar=900 chm=0.2 rvm=0.25 vol=-3.0" },
            { "Future Pop", "Acord que respira", "Serra i taula Cor amb la tallada bombejant a negres: l'efecte d'«sidechain» sense compressor.",
              "o1m=1 o1f=0.33 o2m=0 o2t=8 o2n=0.5 uni=4 unid=0.3 cut=5000 res=0.15" PAD " aa=30 l2w=2 l2s=3 l2ret=0 l2>vol=-0.7 rvm=0.2 chm=0.2 vol=-9.0" },
            { "Future Pop", "Campanetes de festival", "FM de relació 3,5 amb cua de delay amb punt: plucks de campaneta per a tornades eufòriques.",
              "o1m=3 o1p=0.35 o1f=0.55 o2m=0 o2t=5 o2n=0.35 o2o=1" PLUCK " ad=600 cut=9000 fenv=0.3 dlm=0.28 dlt=3 rvm=0.2 em>p1=0.35 ea=1 ed=250 es=0 vol=-9.5" },
            { "Future Pop", "Vocal tallada", "Taula Vocal amb el filtre formant mogut pels passos: frases vocals sintètiques sense micròfon.",
              "o1t=2 o1p=0.4 uni=3 unid=0.2 ft=5 cut=1000 res=0.5 voc=0.2 pr=1 pll=0.15 pas>voc=0.5 pas>p1=0.2 aa=5 ad=400 as=0.7 ar=250 rvm=0.2 dlm=0.15 dlt=2 vol=-6.5" },
            { "Future Pop", "Cristall i sub", "Taula Octaves molt neta, sub de sinus i reverb plate: el piano sintètic de les cançons tristes de festival.",
              "o1t=14 o1p=0.2 o2m=2 o2p=0.5 o2f=0.4 o2n=0.35 o2o=1 subn=0.3 cut=8000" KEYS " rvm=0.3 rvs=0.6 vol=-10.5" },
            { "Future Pop", "Lead de tornada", "Dues serres amb glide en legato i vibrato que entra a poc a poc: la melodia de sintetitzador que s'enganxa.",
              "o1m=1 o1f=0.33 o2m=1 o2f=0.5 o2n=0.6 o2fi=9 mode=1 veus=1 glide=90 uni=2 unid=0.15 cut=4200 res=0.25 fenv=0.3 l4in=600 l4>to=0.012 dlm=0.2 dlt=3 rvm=0.18 vol=-1.0" },
            { "Future Pop", "Ona de neó", "PWM ampla amb chorus i filtre que s'obre amb la velocitat: acords brillants de synth-pop actual.",
              "o1m=1 o1f=0.66 o1p=0.2 l1r=0.6 l1>p1=0.35 uni=2 unid=0.2 cut=2800 res=0.2 vel>cut=0.25" KEYS " chm=0.55 rvm=0.2 vol=-11.0" },

            // ================= Modern Indie =================
            { "Modern Indie", "Cassette gastada", "Triangle i serra amb deriva de cinta (LFO atzar suau al to) i crush lleu: so de maqueta de dormitori.",
              "o1m=1 o1f=0.15 o2m=1 o2f=0.4 o2n=0.4 o2fi=-7 cut=2600 res=0.1 l3w=5 l3r=0.7 l3>to=0.012 crm=0.35 crb=11 crr=2" KEYS " ar=700 chm=0.3 rvm=0.18 tilt=-0.2 vol=-4.0" },
            { "Modern Indie", "Orgue de garatge", "Taula Orgue amb drive suau i Leslie lent (chorus): per a tornades de guitarres netes.",
              "o1t=11 o1p=0.35 o2m=0 o2t=11 o2p=0.7 o2o=1 o2n=0.3 dq=0.25 cut=5000 res=0.05 aa=5 ad=200 as=1 ar=150 chm=0.6 chr=0.9 rvm=0.15 vol=-16.5" },
            { "Modern Indie", "Pols de bedroom pop", "Pols estret i dolç amb filtre ladder rodó: les línies d'arpegi de l'indie electrònic.",
              "o1m=1 o1f=0.8 ft=4 cut=1800 res=0.3 fenv=0.4" PLUCK " ad=450 ar=450 dlm=0.25 dlt=3 rvm=0.2 chm=0.25 vol=-5.0" },
            { "Modern Indie", "Pad de sala d'assaig", "Dues serres lentes amb molta deriva i reverb hall: el llit càlid sota una banda de sis.",
              "o1m=1 o1f=0.33 o2m=1 o2f=0.35 o2n=0.6 o2fi=11 uni=3 unid=0.25 cut=1600 res=0.1 fenv=0.2" PAD " l3w=5 l3r=0.3 l3>to=0.008 l1r=0.12 l1>cut=0.12 rvm=0.35 rvt=1 rvs=0.65 chm=0.3 vol=-7.0" },
            { "Modern Indie", "Melodia de joguina", "Sinus FM petit, com un teclat de joguina; vibrato ràpid i crush: per a interludis tendres.",
              "o1m=3 o1p=0.15 o1f=0.27 mode=1 veus=1 glide=40 cut=7000 aa=3 ad=800 as=0.6 ar=200 l4r=6 l4>to=0.006 crm=0.4 crb=9 crr=3 rvm=0.15 vol=1.5" },
            { "Modern Indie", "Baix de corda gruixut", "Serra amb sub pel ladder, sense brillantor: un baix que funciona sota guitarres distorsionades.",
              "o1m=1 o1f=0.33 o3m=1 o3f=0.66 o3n=0.5" BAIX " ft=4 cut=700 res=0.15 fenv=0.35 fd=300 fs=0.2 aa=2 ad=500 as=0.8 ar=120 dq=0.2 vol=-5.5" },
            { "Modern Indie", "Acords d'aigua", "Harmònic additiu amb parells i trèmolo suau: acords rodons per a cançons lentes.",
              "o1m=2 o1p=0.7 o1f=0.4 o2m=2 o2p=0.2 o2f=0.3 o2o=1 o2n=0.25 cut=6000" KEYS " ad=2500 as=0.5 ar=1200 l2w=0 l2r=4.5 l2>vol=-0.15 chm=0.3 rvm=0.3 vol=-3.5" },
            { "Modern Indie", "Fuzz de sintetitzador", "Quadrada amb distorsió fuzz i filtre mig: un lead que sona com un pedal de guitarra.",
              "o1m=1 o1f=0.66 mode=1 veus=1 glide=30 dt=2 dq=0.55 cut=2400 res=0.2 fenv=0.2 aa=2 ad=500 as=0.8 ar=150 rvm=0.12 dlm=0.12 dlt=5 vol=-4.0" },

            // ================= Hyperpop =================
            { "Hyperpop", "Sucre cremat", "Supersaw saturat amb crush i octava amunt: excessiu i brillant a propòsit.",
              "o1m=1 o1f=0.33 uni=7 unim=1 unid=0.5 unia=1 o2m=0 o2t=4 o2o=1 o2n=0.4 dq=0.35 crm=0.3 crb=8 cut=9000 res=0.1" KEYS " as=0.6 rvm=0.15 exc=0.4 vol=-19.0" },
            { "Hyperpop", "Campana digital trencada", "FM de relació inharmònica amb bits a la fase: plucks metàl·lics i estridents.",
              "o1m=3 o1p=0.5 o1f=0.91 o1w=6 o1wq=0.5 o2m=0 o2t=7 o2n=0.3" PLUCK " ad=500 em>p1=0.5 ea=1 ed=180 es=0 dlm=0.2 dlt=2 rvm=0.15 vol=-10.5" },
            { "Hyperpop", "Lead que salta octaves", "Lead de quadrada amb glide exagerat i passos que salten octava: melodies infantils i caòtiques.",
              "o1m=1 o1f=0.66 o2m=1 o2f=0.33 o2o=1 o2n=0.4 mode=1 veus=1 glide=60 pr=1 p1=0 p2=0 p3=1 p4=0 p5=0.58 p6=0 p7=1 p8=-1 pn=8 pas>to=1@mov cut=6000 res=0.15 dq=0.3 dlm=0.15 dlt=3 vol=-15.0" },
            { "Hyperpop", "Baix de goma", "Sinus amb plec d'ona i pitch que cau ràpid: un 808 elàstic i distorsionat.",
              "o1m=2 o1p=0 o1f=0 o1n=0.9" BAIX " o1o=-1 fold=0.35 dt=1 dq=0.4 em>to=0.6 ea=0.5 ed=90 es=0 aa=1 ad=1500 as=0.6 ar=200 cut=6000 glide=40 vol=-13.5" },
            { "Hyperpop", "Veu de dibuix animat", "Formant amb vocal que es mou ràpid i to pujat: un cor sintètic que sembla parlar en un idioma inventat.",
              "o1m=1 o1f=0.4 uni=3 unid=0.25 ft=5 cut=1400 res=0.6 l2w=4 l2s=5 l2>voc=0.6 aa=3 ad=300 as=0.8 ar=200 chm=0.3 rvm=0.15 vol=-4.5" },
            { "Hyperpop", "Píxels de colors", "Taula Digital amb bits i delay ràpid: arpegis de videojoc saturats.",
              "o1t=4 o1p=0.5 o1w=6 o1wq=0.6 arp=1 arpr=3 arpo=2 arpg=0.6" PLUCK " ad=350 as=0.2 cut=9000 crm=0.35 crb=7 dlm=0.2 dlt=2 vol=5.0" },
            { "Hyperpop", "Glaç estripat", "Vidre amb shifter i unison extrem: acords freds que brillen massa.",
              "o1t=10 o1p=0.6 uni=6 unid=0.6 unia=1 shm=0.25 shh=40 cut=10000" KEYS " rvm=0.2 exc=0.5 vol=-4.0" },
            { "Hyperpop", "Sirena de discoteca", "Serra amb LFO al to en triangle: la sirena de festa, ràpida o lenta amb MOVIMENT.",
              "o1m=1 o1f=0.4 uni=3 unid=0.3 mode=1 veus=1 l2w=1 l2r=1.5 l2>to=0.6 cut=5000 dq=0.25 aa=10 as=1 ar=300 rvm=0.2 dlm=0.15 vol=-4.5" },

            // ================= Cinematic Hybrid =================
            { "Cinematic Hybrid", "Pols de tràiler", "Ostinat d'arpegi de serra amb el filtre que s'obre amb EVOLUCIÓ: tensió de pel·lícula d'acció.",
              "o1m=1 o1f=0.33 o3m=1 o3f=0.66 o3n=0.35 arp=1 arpr=3 arpo=1 arpg=0.5 cut=900 res=0.3 fenv=0.35" PLUCK " ad=200 em>cut=0.5@evo ea=6000 ed=1 es=1 mac7=0.5 dlm=0.15 dlt=3 rvm=0.25 rvt=1 vol=0.5" },
            { "Cinematic Hybrid", "Llautó del futur", "Serres amb filtre que obre a l'atac i ressonadors: com una secció de metalls sintètica.",
              "o1m=1 o1f=0.33 o2m=1 o2f=0.33 o2fi=12 o2n=0.6 uni=3 unid=0.2 cut=900 res=0.15 fenv=0.55 fa=150 fd=900 fs=0.4 aa=60 ad=800 as=0.8 ar=600 rsm=0.15 rst=0 rvm=0.3 rvt=1 vol=-9.5" },
            { "Cinematic Hybrid", "Cor d'acer", "Cor sintètic (taula Cor) amb ressonadors metàl·lics: veus que semblen d'una altra època.",
              "o1t=8 o1p=0.3 o2t=7 o2m=0 o2p=0.3 o2n=0.3 uni=4 unid=0.25 rsm=0.35 rst=2 rsd=0.6" PAD " rvm=0.4 rvt=1 rvs=0.75 l1r=0.1 l1>p1=0.2 vol=-6.0" },
            { "Cinematic Hybrid", "Impacte de sub", "Sinus que cau dues octaves amb soroll i reverb gran: el cop de la càmera lenta.",
              "o1m=2 o1p=0 o1f=0 o2m=5 o2f=0.3 o2n=0.35 mode=0 veus=1 em>to=1 em>to1=0.08 ea=0.5 ed=500 es=0 aa=0.5 ad=2500 as=0 ar=2500 dq=0.3 cut=3000 fenv=0.4 fd=400 fs=0 rvm=0.4 rvt=1 rvs=0.8 vol=-10.0" },
            { "Cinematic Hybrid", "Pulsació de cordes", "Taula Cordes tallada pels passos a semicorxeres: les cordes rítmiques de les bandes sonores modernes.",
              "o1t=9 o1p=0.4 uni=4 unid=0.25 o2m=1 o2f=0.33 o2n=0.35 cut=3000 pr=3 p1=1 p2=0.2 p3=0.6 p4=0.2 p5=1 p6=0.2 p7=0.6 p8=0.4 pn=8 pas>vol=0.6 pas>cut=0.2" PAD " aa=30 rvm=0.3 rvt=1 vol=-8.5" },
            { "Cinematic Hybrid", "Riser d'una tornada", "Soroll i serra que pugen una octava i s'obren durant vuit segons: la pujada abans del cop.",
              "o1m=1 o1f=0.33 uni=5 unid=0.35 o2m=5 o2f=0.6 o2n=0.4 ea=8000 ed=1 es=1 em>to=1 em>cut=0.6 em>ud=0.4 em>sh=0.15 cut=600 res=0.25 aa=200 ar=1500 shm=0.2 rvm=0.35 rvt=1 vol=-4.5" },
            { "Cinematic Hybrid", "Drone d'expectació", "Wavetable fosca amb ring i LFO molt lent al morph: un fons que no s'acaba mai de resoldre.",
              "o1t=13 o1p=0.3 o2m=0 o2t=7 o2p=0.4 o2s=7 o2n=0.4 o3o=-1 o3n=0.4 ring=0.2 uni=3" PAD " aa=3000 ar=5000 l3r=0.05 l3>morph=0.4 rvm=0.45 rvt=2 rvs=0.85 vol=-10.0" },
            { "Cinematic Hybrid", "Martell i corda", "Tecla híbrida: ressonador de fusta i serra pel filtre, entre un piano preparat i un sinte.",
              "o1m=1 o1f=0.4 o2m=5 o2f=0.2 o2n=0.25 rsm=0.6 rst=3 rsd=0.55" KEYS " cut=2500 fenv=0.45 rvm=0.25 rvt=1 vol=-6.0" },

            // ================= Modern Bass =================
            { "Modern Bass", "Sub net", "Sinus pur amb una mica de plec quan hi poses AGRESSIVITAT: el sub que mai falla.",
              "o1m=2 o1p=0 o1f=0 o1n=0.95" BAIX " o1o=-1 cut=1500 res=0 aa=2 ad=500 as=1 ar=120 M4=fold:0.5,drv:0.4 vol=-6.5" },
            { "Modern Bass", "Reese que gira", "Dues serres desafinades amb el filtre que es mou lent: el baix gruixut del drum and bass.",
              "o1m=1 o1f=0.33 o2m=1 o2f=0.33 o2fi=25 o2n=0.8 uni=2 unid=0.2" BAIX " o1o=-1 o2o=-1 ft=4 cut=900 res=0.2 l1r=0.3 l1>cut=0.2 l3w=5 l3r=0.15 l3>to2=0.01 dq=0.3 aa=3 as=1 ar=150 vol=-5.5" },
            { "Modern Bass", "Growl de taula", "Taula Digital amb posició moguda per un LFO a corxeres i formant: un baix que «parla».",
              "o1t=4 o1p=0.3 uni=2 unid=0.15" BAIX " o1o=-1 o3m=2 o3p=0 o3f=0 o3n=0.5 o3o=-1 ft=5 cut=1200 res=0.5 l2s=4 l2w=1 l2>p1=0.5 l2>voc=0.5 dq=0.4 dt=1 aa=2 as=1 ar=120 vol=-10.0" },
            { "Modern Bass", "Pluja de mitjos", "Baix FM amb envolupant curta a l'índex: atac de goma i cos rodó.",
              "o1m=3 o1p=0.2 o1f=0.09" BAIX " o1o=-1 o3m=2 o3p=0 o3f=0 o3n=0.45 em>p1=0.45 ea=1 ed=220 es=0 cut=5000 aa=1 ad=600 as=0.7 ar=120 dq=0.15 vol=-11.0" },
            { "Modern Bass", "Àcid modern", "Serra pel ladder amb molta ressonància i passos al tall: àcid, però net i gros.",
              "o1m=1 o1f=0.33" BAIX " o1o=-1 ft=4 cut=500 res=0.7 fenv=0.5 fd=200 fs=0 glide=70 arp=7 arpp=4 arpr=3 arpg=0.55 dq=0.35 aa=1 ad=300 as=0.6 ar=100 dlm=0.12 dlt=3 vol=-0.5" },
            { "Modern Bass", "Baix de pop elàstic", "Quadrada i sub amb un clic de filtre: el baix rodó que sosté una tornada.",
              "o1m=1 o1f=0.66 o1p=0.3 o3m=2 o3p=0 o3f=0 o3n=0.6" BAIX " o1o=-1 cut=600 res=0.15 fenv=0.45 fd=180 fs=0.1 aa=1 ad=400 as=0.7 ar=100 vol=-4.0" },
            { "Modern Bass", "808 amb cua", "Sinus amb caiguda llarga i pitch que baixa: el baix-bombo de trap i pop urbà.",
              "o1m=2 o1p=0 o1f=0.1 o1n=0.95" BAIX " o1o=-1 em>to=0.5 ea=0.5 ed=60 es=0 aa=1 ad=2500 as=0 ar=400 dq=0.25 dt=0 glide=50 cut=5000 vol=-14.5" },
            { "Modern Bass", "Baix de vidre fosc", "Harmònic senar amb plec que s'obre amb la velocitat: dur quan toques fort, rodó quan acaricies.",
              "o1m=2 o1p=0 o1f=0.5" BAIX " o1o=-1 vel>fold=0.5 vel>cut=0.3 cut=2000 aa=1 ad=700 as=0.6 ar=150 vol=-6.5" },

            // ================= Modern Lead =================
            { "Modern Lead", "Lead tallant", "Taula Sync moguda per l'envolupant de mod: l'atac crida i després s'assenta.",
              "o1t=6 o1p=0.6 o2m=1 o2f=0.33 o2n=0.4 o2fi=-8 mode=1 veus=1 glide=50 em>p1=-0.5 ea=1 ed=400 es=0.3 cut=6000 res=0.2 l4in=500 l4>to=0.012 dlm=0.18 dlt=3 rvm=0.15 vol=-1.5" },
            { "Modern Lead", "Lead de sync clàssic", "Serra amb deformació Sync que l'envolupant escombra: el lead agressiu de sempre, refet.",
              "o1m=1 o1f=0.33 o1w=2 o1wq=0.3 em>warp=0.5 ea=1 ed=600 es=0.2 mode=1 veus=1 glide=40 cut=7000 dq=0.2 rvm=0.12 dlm=0.15 dlt=3 vol=-8.0" },
            { "Modern Lead", "Flauta de vidre", "Harmònic gairebé sinus amb soroll d'alè: un lead suau i expressiu, amb vibrato a l'aftertouch.",
              "o1m=2 o1p=0.1 o1f=0.15 o2m=5 o2f=0.7 o2n=0.12 mode=1 veus=1 glide=60 at>to=0.015 l4in=400 l4>to=0.008 aa=40 as=0.9 ar=250 rvm=0.25 dlm=0.15 vol=-1.5" },
            { "Modern Lead", "Lead de supersaw", "Supersaw amb glide i una mica de drive: per a melodies grans de tornada.",
              "o1m=1 o1f=0.33 uni=6 unim=1 unid=0.35 unia=0.7 mode=1 veus=1 glide=70 cut=5500 res=0.15 dq=0.2 aa=3 as=0.9 ar=250 dlm=0.2 dlt=3 rvm=0.2 vol=-6.5" },
            { "Modern Lead", "Lead que plora", "Formant amb vocal que va d'A a U amb l'envolupant: un lead que sembla cantar.",
              "o1m=1 o1f=0.4 o2m=1 o2f=0.66 o2n=0.4 mode=1 veus=1 glide=80 ft=5 cut=1100 res=0.55 em>voc=0.8 ea=200 ed=900 es=0.2 aa=10 as=0.9 ar=250 l4in=500 l4>to=0.01 rvm=0.2 vol=-7.0" },
            { "Modern Lead", "Lead quadrat mossegat", "Pols estret amb PWM i filtre ressonant: punxant, per sobre de qualsevol mescla.",
              "o1m=1 o1f=0.8 l1r=3.5 l1>p1=0.25 mode=1 veus=1 glide=30 cut=2800 res=0.4 fenv=0.35 fd=300 fs=0.4 dq=0.3 dt=1 rvm=0.12 dlm=0.18 dlt=5 vol=-14.0" },
            { "Modern Lead", "Lead FM de cristall", "Dos operadors amb relació 2 i índex amb la velocitat: brillant, nítid i expressiu.",
              "o1m=3 o1p=0.25 o1f=0.27 vel>p1=0.3 mode=1 veus=1 glide=35 cut=9000 aa=2 ad=700 as=0.7 ar=200 l4in=400 l4>to=0.008 dlm=0.2 dlt=3 rvm=0.18 vol=-1.0" },
            { "Modern Lead", "Lead de goma calenta", "Triangle amb plec i drive suau: arrodonit però present, ideal per a línies de baix agut.",
              "o1m=1 o1f=0.1 fold=0.3 mode=1 veus=1 glide=50 cut=4000 dq=0.25 aa=3 as=0.85 ar=200 rvm=0.15 vol=-10.0" },

            // ================= Modern Pluck =================
            { "Modern Pluck", "Pluck de marimba digital", "FM amb relació 4 i índex que cau ràpid: fusta digital per a ritmes tropicals.",
              "o1m=3 o1p=0.3 o1f=0.64 em>p1=0.4 ea=1 ed=80 es=0" PLUCK " ad=350 ar=350 cut=8000 rvm=0.18 dlm=0.12 dlt=3 vol=-9.0" },
            { "Modern Pluck", "Pluck de seda", "Serra i taula Cordes amb filtre ràpid i delay amb punt: el pluck suau de tota la vida, polit.",
              "o1m=1 o1f=0.33 o2t=9 o2m=0 o2n=0.4 uni=3 unid=0.2 cut=700 res=0.2" PLUCK " fenv=0.7 dlm=0.25 dlt=3 rvm=0.22 vol=-9.0" },
            { "Modern Pluck", "Gota d'aigua", "Sinus amb pitch que cau i filtre BP: gotes de so per a arpegis lleugers.",
              "o1m=2 o1p=0 o1f=0.2 em>to=0.25 ea=0.5 ed=40 es=0 cut=3000" PLUCK " ad=250 rvm=0.3 dlm=0.2 dlt=2 vol=-8.0" },
            { "Modern Pluck", "Pluck de vidre trencat", "Taula Vidre amb Comb ressonant: un pluck amb cua metàl·lica i afinada.",
              "o1t=10 o1p=0.5 ft=6 cut=1500 res=0.6 key=1" PLUCK " fenv=0.2 rvm=0.2 vol=1.5" },
            { "Modern Pluck", "Kalimba sintètica", "Harmònic senar amb ressonador de fusta: una kalimba que no ho és del tot.",
              "o1m=2 o1p=0.1 o1f=0.35 rsm=0.45 rst=3 rsd=0.4" PLUCK " ad=600 ar=500 cut=6000 rvm=0.2 vol=-17.5" },
            { "Modern Pluck", "Pluck de pols curt", "Pols estret amb envolupant molt curta: staccato net per a seqüències ràpides.",
              "o1m=1 o1f=0.85 cut=1200 res=0.3" PLUCK " ad=150 ar=150 fd=120 fenv=0.6 dlm=0.18 dlt=2 rvm=0.12 vol=-13.5" },
            { "Modern Pluck", "Pluck amb cua estèreo", "Unison obert i delay ping-pong: cada nota s'escampa pels dos costats.",
              "o1m=0 o1t=3 o1p=0.4 uni=4 unid=0.25 unia=1 cut=1600 res=0.2" PLUCK " dlm=0.35 dlt=3 dlf=0.45 rvm=0.2 vol=-7.5" },
            { "Modern Pluck", "Pluck granular", "Grànuls curts de la taula Campana: un pluck amb gra i brillantor de sorra.",
              "o1m=4 o1t=5 o1p=0.4 gd=80 gm=30 gs=0.4 o2m=2 o2p=0 o2f=0.3 o2n=0.4" PLUCK " ad=500 cut=7000 rvm=0.25 vol=0.5" },

            // ================= Digital Keys =================
            { "Digital Keys", "Piano elèctric FM", "FM amb relació 1 i l'índex que puja amb la velocitat: les tecles brillants dels 80, més netes.",
              "o1m=3 o1p=0.25 o1f=0.09 vel>p1=0.4 em>p1=0.25 ea=1 ed=600 es=0 o2m=3 o2p=0.1 o2f=0.91 o2n=0.15" KEYS " ad=2500 as=0.2 ar=600 cut=9000 chm=0.3 rvm=0.15 vol=-9.0" },
            { "Digital Keys", "Tecles de vidre", "Taula Vidre i harmònic: un piano de cristall per a baladas electròniques.",
              "o1t=10 o1p=0.3 o2m=2 o2p=0.6 o2f=0.45 o2n=0.35" KEYS " cut=7000 rvm=0.25 rvs=0.6 vol=-3.5" },
            { "Digital Keys", "Clavinet digital", "Pols amb filtre BP i envolupant curta: tecles funk punxants.",
              "o1m=1 o1f=0.8 ft=2 cut=1500 res=0.35 fenv=0.4 aa=1 ad=500 as=0.3 ar=100 fd=200 fs=0.2 vel>cut=0.3 dq=0.15 vol=5.5" },
            { "Digital Keys", "Orgue digital", "Taula Orgue amb chorus ràpid: un orgue clar per a gospel electrònic.",
              "o1t=11 o1p=0.6 o2t=11 o2m=0 o2p=0.2 o2o=-1 o2n=0.4 aa=3 ad=100 as=1 ar=100 chm=0.5 chr=4 chp=0.3 rvm=0.15 vol=-10.0" },
            { "Digital Keys", "Rhodes d'una altra galàxia", "Harmònic amb trèmolo estèreo i tilt: un piano elèctric que flota.",
              "o1m=2 o1p=0.4 o1f=0.3 em>forma=0.3 ea=1 ed=200 es=0" KEYS " ad=3000 as=0.3 l2r=5 l2>pan=0.5 cut=5000 chm=0.25 rvm=0.2 tilt=-0.1 vol=-3.0" },
            { "Digital Keys", "Piano de joguina", "FM de relació 3,5 amb ressonador de campana: un piano petitó i encantador.",
              "o1m=3 o1p=0.2 o1f=0.55 rsm=0.3 rst=1 rsd=0.4" KEYS " ad=1200 as=0 ar=500 cut=9000 rvm=0.2 vol=-13.0" },
            { "Digital Keys", "Acords de wavetable", "Taula Harmònics amb el morph a la velocitat: com més fort, més brillant.",
              "o1t=3 o1p=0.1 vel>morph=0.5 uni=2 unid=0.15" KEYS " cut=8000 chm=0.3 rvm=0.2 vol=-4.0" },
            { "Digital Keys", "Tecles d'aire", "Tecla suau amb soroll d'alè i filtre: dolça, una mica antiga.",
              "o1m=2 o1p=0.5 o1f=0.25 nz=0.12 nzc=0.7 cut=3500" KEYS " ad=2000 as=0.4 ar=800 rvm=0.25 vol=-7.0" },

            // ================= Organic =================
            { "Organic", "Fusta tocada", "Soroll curt pels ressonadors de fusta: un instrument de percussió afinat que sembla real.",
              "o1m=5 o1f=0.4 o1n=0 em>n1=1 ea=0.5 ed=12 es=0 rsm=1 rst=3 rsd=0.5 aa=0.5 ad=900 as=0 ar=600 cut=8000 rvm=0.15 vol=6.0" },
            { "Organic", "Corda pinçada", "Comb + excitat per un cop de soroll: una corda de nylon sintètica.",
              "o1m=5 o1f=0.5 o1n=0 em>n1=1 ea=0.5 ed=10 es=0 ft=6 cut=440 key=1 res=0.9 aa=0.5 ad=2500 as=0 ar=800 rvm=0.18 vol=6.0" },
            { "Organic", "Flauta de bambú", "Sinus amb soroll filtrat i vibrato lent: un instrument de vent sense nom.",
              "o1m=2 o1p=0 o1f=0.1 o2m=5 o2f=0.75 o2n=0.2 mode=1 veus=1 glide=40 aa=60 as=0.9 ar=200 l4in=600 l4r=4.8 l4>to=0.01 l3w=5 l3r=3 l3>n2=0.3 rvm=0.3 vol=-2.0" },
            { "Organic", "Arpa de pell", "Harmònic amb ressonador harmònic i atac de soroll: entre arpa i kora.",
              "o1m=2 o1p=0.4 o1f=0.3 o2m=5 o2f=0.5 o2n=0 em>n2=0.5 ea=1 ed=30 es=0 rsm=0.35 rst=0" PLUCK " ad=900 ar=700 rvm=0.2 vol=-15.0" },
            { "Organic", "Cor de respiració", "Taula Aire i Cor amb soroll i formant: veus que respiren, sense paraules.",
              "o1t=12 o1p=0.4 o2t=8 o2m=0 o2n=0.5 nz=0.15 ft=5 cut=1000 res=0.3 voc=0.3 l3w=5 l3r=0.2 l3>voc=0.3" PAD " rvm=0.35 rvt=1 vol=-5.5" },
            { "Organic", "Gotes de bosc", "Grànuls disperses de la taula Campana amb ressonadors: pluja sobre fulles.",
              "o1m=4 o1t=5 o1p=0.5 gd=30 gm=90 gs=0.8 rsm=0.4 rst=0 rsd=0.6" PAD " aa=300 rvm=0.4 rvt=2 vol=2.0" },
            { "Organic", "Violí sintètic", "Serra amb vibrato que entra i formant suau: un arquet electrònic per a melodies lentes.",
              "o1m=1 o1f=0.33 mode=1 veus=1 glide=80 ft=5 cut=1500 res=0.2 voc=0.15 aa=150 as=0.9 ar=300 l4in=500 l4r=5.5 l4>to=0.012 rvm=0.3 rvt=1 vol=-2.0" },
            { "Organic", "Tambor de fang", "Sinus amb pitch que cau i ressonador de fusta: percussió tonal i càlida.",
              "o1m=2 o1p=0 o1f=0.1 em>to=0.5 ea=0.5 ed=40 es=0 rsm=0.3 rst=3 o2m=5 o2n=0.2 aa=0.5 ad=400 as=0 ar=300 rvm=0.15 vol=-18.5" },

            // ================= Glitch =================
            { "Glitch", "Error de memòria", "Taula Digital amb passos aleatoris a la posició i al to: el sintetitzador que s'encalla.",
              "o1t=4 o1p=0.5 pr=3 pll=0 p1=0.8 p2=-0.6 p3=0.1 p4=1 p5=-1 p6=0.3 p7=-0.2 p8=0.9 pn=8 pas>p1=0.6 pas>to=0.25 crm=0.3 crb=6 aa=1 as=1 ar=50 vol=-2.0" },
            { "Glitch", "Disquet ratllat", "Bits a la fase i reducció de mostreig que es mou: so de màquina vella amb pressa.",
              "o1m=1 o1f=0.5 o1w=6 o1wq=0.5 l2w=4 l2s=5 l2>warp=0.5 crm=0.6 crb=6 crr=6 l2>cr=0.3 cut=6000 aa=1 as=1 ar=80 vol=-0.5" },
            { "Glitch", "Tartamudeig", "El volum el tallen els passos a fuses: un acord que tartamudeja.",
              "o1t=3 o1p=0.4 uni=3 unid=0.2 pr=5 p1=1 p2=-1 p3=1 p4=1 p5=-1 p6=-1 p7=1 p8=-1 pn=8 pas>vol=0.95@mov mac2=0.5" PAD " aa=2 rvm=0.15 vol=-18.0" },
            { "Glitch", "Mòdem enfadat", "FM amb índex aleatori a cada nota i shifter: xiulets i grinyols digitals.",
              "o1m=3 o1p=0.4 o1f=0.82 rnd>p1=0.5@mov rnd>to1=0.25 shm=0.4 shh=150 aa=1 ad=300 as=0.4 ar=100 cut=9000 dlm=0.2 dlt=2 vol=-7.5" },
            { "Glitch", "Senyal perdut", "Soroll i sinus amb volum a l'atzar a semicorxeres: un senyal de ràdio que va i ve.",
              "o1m=2 o1p=0 o1f=0.2 o2m=5 o2f=0.4 o2n=0.4 l2w=4 l2s=5 l2>vol=-0.8 l2>n2=0.5 cut=3500 res=0.2 aa=2 as=1 ar=150 crm=0.25 crb=8 vol=-16.0" },
            { "Glitch", "Grànuls trencats", "Granular de grànuls molt curts i dispersos: pols digital per a transicions.",
              "o1m=4 o1t=4 o1p=0.5 gd=150 gm=10 gs=1 crm=0.3 crb=7 crr=4" PAD " aa=10 ar=300 dlm=0.2 dlt=2 vol=0.0" },
            { "Glitch", "Bucle d'error", "Arpegi a fuses amb crush i delay curt: ràfegues de notes digitals.",
              "o1t=6 o1p=0.6 arp=4 arpr=5 arpo=3 arpg=0.3" PLUCK " ad=80 crm=0.4 crb=7 crr=3 dlm=0.25 dlt=0 dlf=0.5 vol=3.0" },
            { "Glitch", "Clic i xiulet", "Pluck de pols curtíssim amb pitch que salta: clics musicals per a ritmes.",
              "o1m=1 o1f=0.9 em>to=1 ea=0.5 ed=8 es=0 aa=0.5 ad=40 as=0 ar=40 cut=9000 rvm=0.1 dlm=0.2 dlt=1 vol=-10.0" },

            // ================= Texture =================
            { "Texture", "Arena digital", "Núvol granular dens de la taula Aire: una textura que s'escampa.",
              "o1m=4 o1t=12 o1p=0.5 gd=60 gm=120 gs=0.7 l3w=5 l3r=0.15 l3>p1=0.3" PAD " aa=1500 ar=3000 rvm=0.4 rvt=2 vol=-2.0" },
            { "Texture", "Cinta que crepita", "Soroll amb cruixit i sinus molt lent: fons de vinil per sota de qualsevol cosa.",
              "o1m=5 o1p=0.6 o1f=0.7 o1n=0.5 o2m=2 o2p=0 o2f=0.2 o2n=0.3" PAD " cut=3000 crm=0.2 crb=10 rvm=0.3 vol=-0.0" },
            { "Texture", "Vent de metall", "Soroll pel filtre comb amb el tall a la deriva: vent que passa per tubs metàl·lics.",
              "o1m=5 o1f=0.5 o1n=1 ft=6 cut=300 res=0.6 key=1 l3w=5 l3r=0.2 l3>cut=0.5" PAD " aa=2000 ar=3000 rvm=0.35 rvt=2 vol=1.0" },
            { "Texture", "Gel que cruix", "Granular de vidre amb shifter: cristalls que es trenquen lentament.",
              "o1m=4 o1t=10 o1p=0.6 gd=25 gm=60 gs=0.9 shm=0.3 shh=8" PAD " aa=1000 rvm=0.45 rvt=3 rvs=0.7 vol=-1.5" },
            { "Texture", "Superfície d'aigua", "Harmònic amb pan i to que es mouen amb atzar suau: aigua en moviment.",
              "o1m=2 o1p=0.5 o1f=0.5 uni=4 unid=0.2 unia=1 l3w=5 l3r=1.2 l3>pan=0.6 l1r=0.3 l1>forma=0.3" PAD " cut=2500 rvm=0.4 vol=-6.0" },
            { "Texture", "Teixit de cordes", "Taula Cordes amb unison d'octaves i morph lent: un tapís sonor.",
              "o1t=9 o1p=0.2 uni=4 unim=2 unid=0.2 unia=1 l1r=0.07 l1>p1=0.5" PAD " aa=1500 ar=3000 cut=4000 chm=0.3 rvm=0.4 vol=-5.0" },
            { "Texture", "Fum espès", "Soroll fosc pel formant amb vocals lentes: com una boca que respira fum.",
              "o1m=5 o1f=0.2 o1n=0.7 ft=5 cut=800 res=0.6 l1r=0.1 l1>voc=0.5" PAD " aa=1200 ar=3000 rvm=0.4 rvt=2 vol=-3.5" },
            { "Texture", "Microsons", "Grànuls minúsculs a moltes octaves: puntets de so que brillen.",
              "o1m=4 o1t=14 o1p=0.5 gd=40 gm=15 gs=0.8 uni=1" PAD " aa=500 ar=2500 cut=9000 rvm=0.45 rvt=3 dlm=0.25 dlt=2 vol=-2.0" },

            // ================= Motion =================
            { "Motion", "Escombrat infinit", "Morph de la taula Sync amb LFO triangle lent: un acord que es mou sol.",
              "o1t=6 o1p=0.2 o2t=3 o2m=0 o2n=0.4 o2o=1 uni=3 unid=0.25 l1w=1 l1s=2 l1>p1=0.6 l1>p2=-0.4" PAD " aa=300 rvm=0.25 vol=-7.0" },
            { "Motion", "Respiració de filtre", "Filtre ladder que s'obre i es tanca a ritme de rodona: moviment lent i musical.",
              "o1m=1 o1f=0.33 uni=4 unid=0.3 ft=4 cut=700 res=0.35 l1s=1 l1w=0 l1>cut=0.4" PAD " aa=200 rvm=0.25 vol=-3.0" },
            { "Motion", "Panorama giratori", "Unison amb el pan mogut per dos LFO desfasats: el so gira al teu voltant.",
              "o1t=8 o1p=0.4 uni=5 unid=0.3 unia=0.5 l1r=0.3 l1>pan=0.8 l2w=0 l2r=0.21 l2>ua=0.4" PAD " rvm=0.3 vol=-5.5" },
            { "Motion", "Passos de colors", "Seqüenciador de 16 passos a la posició de la wavetable: el timbre canvia a cada semicorxera.",
              "o1t=4 o1p=0.3 o2t=10 o2m=0 o2n=0.4 pr=3 pll=0.3 pas>p1=0.6 pas>p2=0.4 pas>cut=0.2 cut=5000" PAD " aa=5 rvm=0.2 dlm=0.15 dlt=3 vol=-5.0" },
            { "Motion", "Trèmolo d'ona", "Trèmolo a corxeres amb triangle i formes que es mouen: pulsació orgànica.",
              "o1m=2 o1p=0.5 o1f=0.4 l2s=4 l2w=1 l2>vol=-0.6 l1r=0.2 l1>forma=0.4" PAD " aa=100 rvm=0.25 vol=-7.5" },
            { "Motion", "Vocal que camina", "Formant amb passos a la vocal: un cor robòtic que pronuncia síl·labes.",
              "o1m=1 o1f=0.4 uni=3 ft=5 cut=1200 res=0.55 pr=1 pll=0.25 p1=0 p2=0.5 p3=1 p4=0.25 p5=-0.5 p6=0.75 p7=-1 p8=0.5 pn=8 pas>voc=0.5" PAD " aa=50 rvm=0.2 vol=-6.0" },
            { "Motion", "Ones creuades", "Dos LFO a velocitats diferents a dues wavetables: un moviment que no es repeteix mai igual.",
              "o1t=13 o1p=0.3 o2t=9 o2m=0 o2n=0.6 o2s=7 l1r=0.13 l1>p1=0.5 l3w=0 l3r=0.31 l3>p2=0.5 l3>n2=0.3" PAD " aa=600 rvm=0.35 vol=-9.5" },
            { "Motion", "Morph amb el tempo", "Morph a ritme de blanques: el so canvia de cara amb el compàs.",
              "o1t=2 o1p=0 o2t=14 o2m=0 o2n=0.4 l1s=2 l1w=1 l1>morph=0.7" PAD " aa=100 rvm=0.25 dlm=0.15 dlt=5 vol=-5.0" },

            // ================= Hybrid =================
            { "Hybrid", "Guitarra de silici", "Comb + excitat per un pols curt amb distorsió: una guitarra elèctrica que no ho és.",
              "o1m=5 o1f=0.6 o1n=0 em>n1=1 ea=0.5 ed=15 es=0 o2m=1 o2f=0.33 o2n=0.15 ft=6 cut=440 key=1 res=0.9 aa=0.5 ad=2500 as=0.3 ar=400 dq=0.4 dt=0 rvm=0.15 vol=-8.0" },
            { "Hybrid", "Piano i sinte", "Ressonadors de fusta sobre serra filtrada: mig piano, mig polisintètic.",
              "o1m=1 o1f=0.35 o2m=5 o2f=0.4 o2n=0 em>n2=0.6 ea=1 ed=20 es=0 rsm=0.45 rst=3" KEYS " cut=3000 chm=0.2 rvm=0.22 vol=-12.5" },
            { "Hybrid", "Violoncel elèctric", "Serra i formant amb soroll d'arquet: corda greu amb un costat elèctric.",
              "o1m=1 o1f=0.33 o2m=5 o2f=0.6 o2n=0.08 mode=1 veus=1 glide=70 ft=5 cut=900 res=0.25 voc=0.6 aa=120 as=0.9 ar=400 l4in=700 l4r=5 l4>to=0.01 rvm=0.3 rvt=1 vol=-2.0" },
            { "Hybrid", "Campana de vidre i sub", "FM de campana sobre un sub: un so per a intros que barreja calidesa i fred.",
              "o1m=3 o1p=0.35 o1f=0.73 o3m=2 o3p=0 o3f=0 o3n=0.4 o3o=-1" KEYS " ad=3000 as=0 ar=2500 rvm=0.3 rvt=1 vol=-11.0" },
            { "Hybrid", "Marimba elèctrica", "Harmònic senar amb ressonador de fusta i drive: marimba endollada a un amplificador.",
              "o1m=2 o1p=0 o1f=0.4 rsm=0.5 rst=3 rsd=0.3 dq=0.3" PLUCK " ad=450 cut=6000 rvm=0.15 vol=-24.0" },
            { "Hybrid", "Veu i serra", "Taula Vocal sumada a una serra amb el formant: entre un cor i un lead.",
              "o1t=2 o1p=0.5 o2m=1 o2f=0.33 o2n=0.5 ft=5 cut=1100 res=0.4 voc=0.4 l1r=0.3 l1>voc=0.3" PAD " aa=80 rvm=0.3 vol=-8.5" },
            { "Hybrid", "Arpa de neó", "Pluck harmònic amb ressonador harmònic i delay: una arpa feta de llum.",
              "o1m=2 o1p=0.5 o1f=0.4 rsm=0.35 rst=0" PLUCK " ad=900 ar=900 cut=5000 dlm=0.2 dlt=3 rvm=0.25 vol=-12.5" },
            { "Hybrid", "Kora i circuit", "Corda comb amb FM: el timbre d'una kora passat per un sinte modular.",
              "o1m=3 o1p=0.2 o1f=0.27 em>p1=0.4 ea=0.5 ed=40 es=0 ft=6 cut=440 key=1 res=0.75" PLUCK " ad=1200 ar=900 rvm=0.2 vol=-0.5" },

            // ================= Experimental =================
            { "Experimental", "Anell invers", "Ring de dos oscil·ladors desafinats i shifter: tons que no pertanyen a cap escala.",
              "o1m=0 o1t=3 o1p=0.4 o2m=2 o2p=0.5 o2f=0.5 o2n=0 o2s=5 o2fi=30 ring=1 shm=0.35 shh=-60" PAD " aa=200 rvm=0.3 vol=-3.0" },
            { "Experimental", "Àudio que modula", "L'oscil·lador 3 modula el to de l'1 a ritme d'àudio: FM salvatge amb MOVIMENT.",
              "o1m=1 o1f=0.2 o3m=2 o3p=0 o3f=0 o3n=0 o3o=0 o3s=7 o3>to1=0.4@mov cut=7000 aa=3 as=0.9 ar=300 rvm=0.2 vol=-3.5" },
            { "Experimental", "Mirall de fase", "Deformació Mirall a fons amb plec: harmònics que es reflecteixen.",
              "o1t=3 o1p=0.3 o1w=3 o1wq=0.6 l1r=0.2 l1>warp=0.3 fold=0.4 cut=6000" PAD " aa=200 rvm=0.3 vol=-8.5" },
            { "Experimental", "Desplaçament Doppler", "El shifter oscil·la: el so s'acosta i s'allunya com un tren.",
              "o1t=8 o1p=0.4 uni=3 shm=0.6 shh=0 l1r=0.15 l1>sh=0.15" PAD " rvm=0.3 vol=-3.0" },
            { "Experimental", "Bits que respiren", "Crush amb la resolució moguda per un LFO lent: de net a brut i tornar.",
              "o1m=1 o1f=0.33 uni=3 unid=0.25 crm=0.7 crb=10 crr=1 l1r=0.12 l1>cr=0.3 l1>cut=0.2" PAD " rvm=0.25 vol=-6.5" },
            { "Experimental", "Pell de tambor estrany", "Soroll pels ressonadors de metall amb to aleatori a cada nota: percussió alienígena.",
              "o1m=5 o1f=0.5 o1n=0 em>n1=1 ea=0.5 ed=15 es=0 rsm=1 rst=2 rsd=0.6 rnd>to=0.3 aa=0.5 ad=1200 as=0 ar=900 rvm=0.2 vol=6.0" },
            { "Experimental", "Ona plegada a l'atzar", "Plec i forma moguts per l'atzar per nota: cada nota té un caràcter diferent.",
              "o1m=1 o1f=0.2 rnd>fold=0.6 rnd>forma=0.4 rnd>pan=0.6 cut=5000" KEYS " rvm=0.2 vol=-9.0" },
            { "Experimental", "Comb invers que canta", "Comb − amb el tall seguint l'LFO: un xiulet fantasmagòric dins el so.",
              "o1m=1 o1f=0.33 uni=2 ft=7 cut=600 res=0.8 l1r=0.25 l1>cut=0.35" PAD " aa=200 rvm=0.3 vol=-3.5" },

            // ================= Atmospheric =================
            { "Atmospheric", "Boira de matinada", "Taula Aire amb granular i reverb ambient: un fons humit i tranquil.",
              "o1t=12 o1p=0.3 o2m=4 o2t=12 o2n=0.5 gd=20 gm=200 gs=0.5 l1r=0.05 l1>p1=0.3" PAD " aa=3000 ar=5000 cut=3000 rvm=0.5 rvt=2 rvs=0.9 vol=-1.5" },
            { "Atmospheric", "Catedral de vidre", "Vidre i shimmer gran: llum que cau des de molt amunt.",
              "o1t=10 o1p=0.4 uni=4 unid=0.2 o2m=2 o2p=0.5 o2f=0.6 o2n=0.25 o2o=1" PAD " aa=2000 ar=5000 rvm=0.5 rvt=3 rvs=0.9 dlm=0.2 dlt=6 vol=-6.0" },
            { "Atmospheric", "Nit al desert", "Sinus i soroll fosc amb un LFO lent al tall: espai obert i fred.",
              "o1m=2 o1p=0.2 o1f=0.1 o2m=5 o2f=0.2 o2n=0.25 cut=1200 l1r=0.04 l1>cut=0.3" PAD " aa=2500 ar=5000 rvm=0.5 rvt=1 rvs=0.9 vol=-10.5" },
            { "Atmospheric", "Aurora", "Taules Octaves i Cor amb morph lent: colors que es fonen al cel.",
              "o1t=14 o1p=0.2 o2t=8 o2m=0 o2n=0.5 o2s=7 uni=3 unid=0.25 l3w=5 l3r=0.06 l3>morph=0.5" PAD " aa=2500 ar=5000 chm=0.3 rvm=0.45 rvt=3 vol=-9.0" },
            { "Atmospheric", "Fons de l'oceà", "Wavetable fosca amb formant i soroll de corrent: pressió i profunditat.",
              "o1t=13 o1p=0.4 o1o=-1 nz=0.15 nzc=0.85 ft=5 cut=600 res=0.4 l1r=0.05 l1>voc=0.4" PAD " aa=2500 ar=4000 rvm=0.45 rvt=1 vol=-1.5" },
            { "Atmospheric", "Estels", "Grànuls aguts dispersos i delay: punts de llum sobre un sinus.",
              "o1m=4 o1t=5 o1p=0.6 o1o=1 gd=8 gm=50 gs=1 o2m=2 o2p=0 o2f=0 o2n=0.35" PAD " aa=1000 rvm=0.5 rvt=3 dlm=0.3 dlt=6 dlf=0.5 vol=-8.0" },
            { "Atmospheric", "Hivern", "Harmònic amb unison d'octaves i reverb llarga: fred i serè.",
              "o1m=2 o1p=0.3 o1f=0.4 uni=3 unim=2 unid=0.15" PAD " aa=2000 ar=5000 cut=5000 rvm=0.5 rvs=0.85 rvt=1 vol=-6.0" },
            { "Atmospheric", "Capvespre elèctric", "Serres lentes i shimmer: el pad per al final de la cançó.",
              "o1m=1 o1f=0.33 uni=5 unid=0.3 cut=1500 res=0.15 l1r=0.08 l1>cut=0.2" PAD " aa=2000 ar=5000 rvm=0.5 rvt=3 rvs=0.8 chm=0.3 vol=-5.5" },

            // ================= Pulses =================
            { "Pulses", "Pols de corxera", "Acord tallat a corxeres amb el filtre que respira: el batec d'una tornada.",
              "o1m=1 o1f=0.33 uni=3 unid=0.25 cut=3000 res=0.2 l2s=4 l2w=2 l2ret=0 l2>vol=-0.85 l2>cut=-0.2" PAD " aa=5 rvm=0.15 vol=-9.0" },
            { "Pulses", "Cor batent", "Volum en pulsació de negres amb sinus: un cor que batega sota la cançó.",
              "o1m=2 o1p=0.4 o1f=0.4 l2s=3 l2w=0 l2>vol=-0.8" PAD " aa=30 rvm=0.2 vol=-8.0" },
            { "Pulses", "Trens de passos", "Passos de semicorxera al volum i al tall: pulsació de seqüenciador analògic.",
              "o1m=1 o1f=0.5 o2m=1 o2f=0.66 o2n=0.5 o2o=-1 cut=1200 res=0.3 pr=3 p1=1 p2=0.3 p3=0.6 p4=0.2 p5=1 p6=0.4 p7=0.7 p8=0 pn=8 pas>vol=0.7 pas>cut=0.3" PAD " aa=5 rvm=0.15 dlm=0.12 dlt=3 vol=-19.0" },
            { "Pulses", "Pulsació de vidre", "Vidre trèmolo a tresets: brillant i rítmic.",
              "o1t=10 o1p=0.4 uni=2 l2s=8 l2w=3 l2>vol=-0.7" PAD " aa=5 rvm=0.2 vol=-8.0" },
            { "Pulses", "Bomba de sub", "Sub que s'infla a negres: per omplir el greu en temes lents.",
              "o1m=2 o1p=0 o1f=0 o1o=-1 l2s=3 l2w=2 l2ret=0 l2>vol=-0.9" BAIX " aa=5 as=1 ar=200 vol=-9.0" },
            { "Pulses", "Pols de formant", "Vocal que obre a corxeres: una pulsació que gairebé diu «ua-ua».",
              "o1m=1 o1f=0.4 uni=3 ft=5 cut=1100 res=0.5 l2s=4 l2w=1 l2>voc=0.6 l2>vol=-0.4" PAD " aa=10 rvm=0.2 vol=-9.0" },
            { "Pulses", "Rellotge de llum", "Pluck repetit per l'arpegiador en acord a semicorxeres: un pols de llum per a tornades.",
              "o1t=3 o1p=0.4 arp=6 arpr=3 arpg=0.4" PLUCK " ad=180 cut=2500 res=0.25 dlm=0.15 dlt=3 rvm=0.2 vol=-4.0" },
            { "Pulses", "Ones que bateguen", "Morph i volum amb LFO a negres: pulsació que canvia de timbre a cada cop.",
              "o1t=6 o1p=0.2 l2s=3 l2w=2 l2>vol=-0.7 l2>p1=0.4" PAD " aa=10 rvm=0.2 vol=-8.5" },

            // ================= Rhythmic =================
            { "Rhythmic", "Seqüència berlinesa", "Pluck de serra amb el patró berlinès de l'arpegiador: el motor de les cançons electròniques.",
              "o1m=1 o1f=0.33 arp=7 arpp=2 arpr=3 arpg=0.5 cut=1000 res=0.35" PLUCK " ad=220 fenv=0.5 dlm=0.2 dlt=3 rvm=0.15 vol=0.0" },
            { "Rhythmic", "Funk de FM", "Pluck FM amb el patró funk: ritme sincopat per a groove.",
              "o1m=3 o1p=0.3 o1f=0.27 em>p1=0.4 ea=1 ed=100 es=0 arp=7 arpp=11 arpr=3 arpg=0.45" PLUCK " ad=250 cut=7000 vol=-0.5" },
            { "Rhythmic", "Trance de colors", "Supersaw amb el patró trance: la seqüència eufòrica de les pistes de ball.",
              "o1m=1 o1f=0.33 uni=5 unim=1 unid=0.3 arp=7 arpp=6 arpr=3 arpg=0.5 cut=2500 res=0.2" PLUCK " ad=200 dlm=0.25 dlt=3 rvm=0.2 vol=-1.5" },
            { "Rhythmic", "Galop", "Patró galop amb pols i delay: per a ritmes que corren.",
              "o1m=1 o1f=0.66 arp=7 arpp=3 arpr=3 arpg=0.5 cut=1800 res=0.3" PLUCK " ad=200 dlm=0.15 dlt=3 vol=-4.0" },
            { "Rhythmic", "Techno de goma", "Baix rítmic amb el patró techno i el filtre que es mou amb MOVIMENT.",
              "o1m=1 o1f=0.5" BAIX " o1o=-1 o3m=2 o3p=0 o3f=0 o3n=0.5 ft=4 cut=700 res=0.35 arp=7 arpp=12 arpr=3 arpg=0.55 l1s=1 l1>cut=0.3 aa=1 ad=250 as=0.5 ar=80 fenv=0.4 fd=150 fs=0 vol=6.0" },
            { "Rhythmic", "Synthwave en marxa", "Patró synthwave amb pols i chorus: la carretera de nit.",
              "o1m=1 o1f=0.7 arp=7 arpp=7 arpr=3 arpg=0.55 cut=1500 res=0.25" PLUCK " ad=250 chm=0.4 dlm=0.2 dlt=3 rvm=0.2 vol=-3.5" },
            { "Rhythmic", "Arpegi de vidre amunt-avall", "Arpegiador amunt-avall de dues octaves amb taula Vidre.",
              "o1t=10 o1p=0.5 arp=3 arpr=3 arpo=2 arpg=0.5" PLUCK " ad=280 cut=4000 dlm=0.2 dlt=3 rvm=0.2 vol=2.0" },
            { "Rhythmic", "Ganxo repetit", "Patró «Hook» amb lead quadrat: una frase curta que s'enganxa.",
              "o1m=1 o1f=0.66 o2m=1 o2f=0.33 o2o=1 o2n=0.3 arp=7 arpp=14 arpr=3 arpg=0.6 cut=3000" PLUCK " ad=300 dlm=0.2 dlt=3 rvm=0.15 vol=-4.0" },

            // ================= Vocal-like synth =================
            { "Vocal-like synth", "Aaaa", "Formant fix a la vocal A amb serra: un cor sintètic obert.",
              "o1m=1 o1f=0.35 uni=4 unid=0.25 ft=5 cut=1000 res=0.55 voc=0" PAD " aa=200 chm=0.3 rvm=0.3 vol=-9.5" },
            { "Vocal-like synth", "Ooo de robot", "Formant a la vocal O amb vibrato: veu de robot dolç.",
              "o1m=1 o1f=0.5 mode=1 veus=1 glide=60 ft=5 cut=1000 res=0.6 voc=0.75 l4in=400 l4>to=0.01 aa=40 as=0.9 ar=250 rvm=0.2 vol=1.5" },
            { "Vocal-like synth", "Talk box", "Vocal moguda per la roda o per l'envolupant: el sinte que parla com amb un tub.",
              "o1m=1 o1f=0.4 o2m=1 o2f=0.66 o2n=0.4 mode=1 veus=1 glide=40 ft=5 cut=1100 res=0.55 roda>voc=0.8 em>voc=0.5 ea=60 ed=300 es=0.2 aa=5 as=0.9 ar=150 dq=0.2 vol=-10.0" },
            { "Vocal-like synth", "Cor de gel", "Taula Cor amb granular i shimmer: un cor congelat.",
              "o1t=8 o1p=0.4 o2m=4 o2t=8 o2n=0.4 gd=30 gm=150 gs=0.4" PAD " aa=1500 rvm=0.45 rvt=3 vol=-5.5" },
            { "Vocal-like synth", "Murmuri", "Taula Formant amb soroll d'alè i LFO lent a la posició: veus que xiuxiuegen.",
              "o1t=15 o1p=0.3 nz=0.25 nzc=0.6 l1r=0.2 l1>p1=0.4 cut=2500" PAD " aa=800 rvm=0.35 vol=0.5" },
            { "Vocal-like synth", "Wah-wah vocal", "Formant que va d'U a A amb cada nota: un «uaa» en cada acord.",
              "o1m=1 o1f=0.4 uni=2 ft=5 cut=1000 res=0.5 voc=1 em>voc=-1 ea=1 ed=500 es=0" KEYS " rvm=0.2 vol=-7.5" },
            { "Vocal-like synth", "Veu d'òrgan", "Taula Vocal amb harmònic de suport: el cor dels teclats de 1985.",
              "o1t=2 o1p=0.6 o2m=2 o2p=0.6 o2f=0.4 o2n=0.35" PAD " aa=300 chm=0.4 rvm=0.3 vol=-2.5" },
            { "Vocal-like synth", "Síl·labes a l'atzar", "Cada nota tria una vocal diferent: un cor que improvisa síl·labes.",
              "o1m=1 o1f=0.45 uni=3 ft=5 cut=1100 res=0.55 voc=0.5 rnd>voc=0.5@mov" KEYS " as=0.6 rvm=0.25 vol=-2.0" },

            // ================= Metallic =================
            { "Metallic", "Campana de bronze", "FM inharmònic amb ressonadors de campana: una campana gran que dura.",
              "o1m=3 o1p=0.4 o1f=0.55 rsm=0.4 rst=1 rsd=0.8 aa=0.5 ad=5000 as=0 ar=4000 cut=9000 rvm=0.3 rvt=1 vol=-14.5" },
            { "Metallic", "Placa d'acer", "Soroll pels ressonadors de metall: una placa colpejada.",
              "o1m=5 o1f=0.6 o1n=0 em>n1=1 ea=0.5 ed=15 es=0 rsm=1 rst=2 rsd=0.9 aa=0.5 ad=3000 as=0 ar=2500 rvm=0.25 vol=6.0" },
            { "Metallic", "Tub de laboratori", "Comb + amb clics de pols: so de tub metàl·lic ressonant.",
              "o1m=5 o1f=0.6 o1n=0 em>n1=1 ea=0.5 ed=12 es=0 ft=6 cut=440 key=1 res=0.85 aa=0.5 ad=1500 as=0 ar=1000 rvm=0.2 vol=6.0" },
            { "Metallic", "Gong digital", "Taula Metall amb shifter que deriva: un gong que no s'acaba d'aturar.",
              "o1t=7 o1p=0.5 shm=0.3 shh=5 l3w=5 l3r=0.3 l3>sh=0.03 aa=1 ad=6000 as=0 ar=5000 rvm=0.35 rvt=1 rvs=0.8 vol=-5.0" },
            { "Metallic", "Martell de fàbrica", "FM dur amb índex molt alt i drive: cops metàl·lics industrials.",
              "o1m=3 o1p=0.8 o1f=0.82 em>p1=0.3 ea=0.5 ed=50 es=0 dq=0.4 dt=1 aa=0.5 ad=400 as=0 ar=200 cut=9000 rvm=0.2 vol=-16.5" },
            { "Metallic", "Cadena que gira", "Ring entre taula Metall i FM amb pan mogut: so de metall en moviment.",
              "o1t=7 o1p=0.4 o2m=3 o2p=0.3 o2f=0.82 o2n=0 ring=1 l1r=0.4 l1>pan=0.7" PAD " aa=300 rvm=0.3 vol=-8.5" },
            { "Metallic", "Gamelan sintètic", "Harmònic amb ressonadors de campana i parells: un gamelan de laboratori.",
              "o1m=2 o1p=0.8 o1f=0.5 rsm=0.5 rst=1 rsd=0.6" PLUCK " ad=1500 ar=1500 cut=7000 rvm=0.25 vol=-15.5" },
            { "Metallic", "Filferro vibrant", "Comb − amb serra: un filferro elèctric que vibra.",
              "o1m=1 o1f=0.33 ft=7 cut=440 key=1 res=0.8" KEYS " rvm=0.2 vol=-0.0" },

            // ================= Morphing =================
            { "Morphing", "Morph d'Harmònics a Fosc", "Dues taules fan el morph amb la macro MORPH: de brillant a fosc.",
              "o1t=3 o1p=0 o2t=13 o2m=0 o2p=0 o2n=0.5 mac8=0.3" PAD " aa=200 rvm=0.3 vol=-7.5" },
            { "Morphing", "Lenta transformació", "L'envolupant de mod fa el morph durant cinc segons: cada nota llarga canvia.",
              "o1t=6 o1p=0 o2t=10 o2m=0 o2p=0 o2n=0.4 em>morph=0.8@evo mac7=0.5 ea=5000 ed=1 es=1" PAD " aa=300 rvm=0.3 vol=-7.0" },
            { "Morphing", "Morph vocal", "La taula Vocal travessada lentament amb formant: de murmuri a crit.",
              "o1t=2 o1p=0 l1r=0.08 l1>p1=0.8 ft=5 cut=1100 res=0.4 l1>voc=0.6" PAD " aa=300 rvm=0.3 vol=-7.0" },
            { "Morphing", "Metall a cordes", "Taula Metall i taula Cordes creuades amb LFO: el so passa d'una matèria a l'altra.",
              "o1t=7 o1p=0.3 o2t=9 o2m=0 o2p=0.3 l1r=0.1 l1>n1=-0.8 l1>n2=0.8 o2n=0.5" PAD " aa=300 rvm=0.3 vol=-7.5" },
            { "Morphing", "Velocitat que transforma", "La força de la nota decideix el timbre: suau és rodó, fort és digital.",
              "o1t=4 o1p=0 vel>morph=0.9 vel>cut=0.3" KEYS " rvm=0.2 vol=-1.5" },
            { "Morphing", "Morph del PWM", "La taula PWM escombrada per la roda: de quadrada a pols fi.",
              "o1t=1 o1p=0.2 roda>p1=0.8 uni=3 unid=0.2" PAD " aa=20 rvm=0.2 chm=0.3 vol=-7.5" },
            { "Morphing", "Tres mons", "Tres oscil·ladors amb tres motors diferents i el morph que els canvia de pes.",
              "o1t=13 o1p=0 o2m=3 o2p=0.2 o2f=0.27 o2n=0.4 o3m=2 o3p=0.5 o3f=0.5 o3o=0 o3n=0.4 l1r=0.07 l1>n1=0.5 l3w=0 l3r=0.11 l3>n2=-0.5 l1>n3=-0.5" PAD " aa=300 rvm=0.35 vol=-8.0" },
            { "Morphing", "Morph amb passos", "Els passos salten per la taula Octaves a corxeres: un arpegi de timbres.",
              "o1t=14 o1p=0.5 pr=1 pll=0.2 pas>p1=0.5" PAD " aa=10 rvm=0.2 dlm=0.15 dlt=3 vol=-5.0" },

            // ================= Sound Design =================
            { "Sound Design", "Pujada de vuit segons", "Riser complet: to, tall, unison i reverb pugen alhora. Mantén la nota.",
              "o1m=1 o1f=0.33 uni=6 unim=1 unid=0.3 o2m=5 o2f=0.7 o2n=0.35 ea=8000 ed=1 es=1 em>to=1 em>cut=0.7 em>ud=0.4 em>rv=0.4 cut=500 res=0.2 aa=500 ar=2000 rvm=0.1 rvt=1 vol=-6.0" },
            { "Sound Design", "Caiguda al buit", "El to cau una octava i el filtre es tanca: el contrari d'un riser.",
              "o1m=1 o1f=0.33 uni=4 unid=0.3 o2m=5 o2f=0.5 o2n=0.25 ea=1 ed=3000 es=0 em>to=1 em>cut=0.6 cut=800 aa=1 ad=3000 as=0 ar=2000 rvm=0.35 rvt=1 dlm=0.2 vol=-10.0" },
            { "Sound Design", "Cop tonal", "Impacte afinat: sinus que cau, soroll i ressonadors. Toca'l a la tònica.",
              "o1m=2 o1p=0 o1f=0.1 o2m=5 o2f=0.4 o2n=0 em>n2=0.8 em>to=0.8 ea=0.5 ed=70 es=0 rsm=0.3 rst=2 dq=0.4 aa=0.5 ad=1500 as=0 ar=1500 rvm=0.3 rvt=1 vol=-24.0" },
            { "Sound Design", "Whoosh", "Soroll pel passabanda que escombra en dos segons: el pas de càmera.",
              "o1m=5 o1f=0.5 o1n=1 ft=2 fp=0 cut=300 res=0.3 key=0 ea=1500 ed=800 es=0 em>cut=0.8 aa=1200 ad=900 as=0 ar=600 rvm=0.3 vol=2.0" },
            { "Sound Design", "Alarma de nau", "Quadrada amb to en triangle a negres: senyal d'alerta.",
              "o1m=1 o1f=0.66 mode=1 veus=1 l2s=3 l2w=1 l2>to=0.5 cut=3000 aa=5 as=1 ar=200 rvm=0.2 dlm=0.15 dlt=5 vol=-6.0" },
            { "Sound Design", "Porta que s'obre", "Comb amb el tall que puja i soroll: un mecanisme que s'obre.",
              "o1m=5 o1f=0.5 o1n=1 ft=6 cut=80 res=0.7 key=0 ea=2000 ed=1 es=1 em>cut=0.7 aa=200 as=1 ar=500 rvm=0.3 vol=-0.5" },
            { "Sound Design", "Transmissió", "FM amb índex a l'atzar i crush: missatge d'una nau que arriba trencat.",
              "o1m=3 o1p=0.3 o1f=0.64 l2w=4 l2s=5 l2>p1=0.5 l2>to1=0.2 crm=0.5 crb=6 crr=5 ft=2 cut=1800 res=0.3" PAD " aa=10 rvm=0.2 vol=-0.5" },
            { "Sound Design", "Explosió llunyana", "Soroll greu amb sub que cau i reverb llarga: un tro lluny.",
              "o1m=5 o1f=0.15 o1n=0.6 o2m=2 o2p=0 o2f=0 o2o=-1 o2n=0.6 em>to=0.8 ea=0.5 ed=200 es=0 cut=900 fenv=0.4 fd=600 fs=0 aa=0.5 ad=3000 as=0 ar=3000 dq=0.3 rvm=0.45 rvt=1 rvs=0.8 vol=-18.5" },
        };
        #undef PLUCK
        #undef KEYS
        #undef PAD
        #undef BAIX

        int busca (const char* const* llista, int n, const std::string& s)
        {
            for (int i = 0; i < n; ++i) if (s == llista[i]) return i;
            return -1;
        }
        const char* const nomsFonts[] = { "", "l1", "l2", "l3", "l4", "ev", "ef", "em", "vel", "nota", "roda", "at", "rnd", "pas", "o3" };
        const char* const nomsVies[] = { "", "car", "mov", "bri", "agr", "amp", "esp", "evo", "mor", "roda", "at", "vel", "em", "l1" };
        const char* const nomsDestins[] = { "", "to", "to1", "to2", "to3", "p1", "p2", "p3", "morph", "forma", "warp", "n1", "n2", "n3",
                                            "cut", "res", "voc", "pan", "vol", "ud", "ua", "fold", "drv", "gd", "gm", "gs", "rs", "nz",
                                            "cr", "sh", "ch", "dl", "rv", "es" };
        // les rutes per defecte de les macros (si el preset no en dona)
        struct Ruta { int d1; float q1; int d2; float q2; };
        const Ruta rutesDefecte[numMacros] = {
            { DForma, 0.45f, DWarp, 0.5f },       // Caràcter
            { DUniDet, 0.3f, DChorus, 0.3f },     // Moviment (i escala les vies dels LFO i els passos)
            { DTall, 0.45f, DEspectre, 0.5f },    // Brillantor
            { DDrive, 0.6f, DFold, 0.35f },       // Agressivitat
            { DUniAmple, 0.6f, DChorus, 0.5f },   // Amplada
            { DReverb, 0.5f, DDelay, 0.35f },     // Espai
            { DCap, 0.0f, DCap, 0.0f },           // Evolució (va per vies de la matriu)
            { DMorph, 1.0f, DTall, 0.12f },       // Morph
        };

        void aplica (Neo& s, const char* canvis)
        {
            std::istringstream is (canvis);
            std::string tros;
            int slot = 0;
            bool macroDona[numMacros] {};
            bool evolucio = false;
            while (is >> tros)
            {
                const auto ig = tros.find ('=');
                if (ig == std::string::npos) continue;
                const auto clau = tros.substr (0, ig);
                const auto resta = tros.substr (ig + 1);
                const auto fl = clau.find ('>');
                if (fl != std::string::npos)
                {
                    const int f = busca (nomsFonts, 15, clau.substr (0, fl)), d = busca (nomsDestins, numDestins, clau.substr (fl + 1));
                    if (f <= 0 || d <= 0 || slot >= numSlots) continue;
                    const auto ar = resta.find ('@');
                    const float q = std::strtof (resta.c_str(), nullptr);
                    int via = ar == std::string::npos ? 0 : busca (nomsVies, numVies, resta.substr (ar + 1));
                    if (via < 0) via = 0;
                    if (via == 0 && ((f >= FLfo1 && f <= FLfo4) || f == FPassos || f == FAtzar)) via = VMac2;
                    if (via == VMac7) evolucio = true;
                    const int b = M1Font + slot * 4;
                    s.posa (b, (float) f); s.posa (b + 1, (float) via); s.posa (b + 2, (float) d); s.posa (b + 3, q);
                    ++slot;
                    continue;
                }
                if (clau.size() == 2 && clau[0] == 'M' && clau[1] >= '1' && clau[1] <= '8')
                {
                    const int k = clau[1] - '1';
                    macroDona[k] = true;
                    std::istringstream rs (resta);
                    std::string r;
                    int n = 0;
                    for (int j = 0; j < 2; ++j) { s.posa (Mc1D1 + k * 4 + j * 2, 0.0f); s.posa (Mc1Q1 + k * 4 + j * 2, 0.0f); }
                    while (std::getline (rs, r, ',') && n < 2)
                    {
                        const auto dp = r.find (':');
                        if (dp == std::string::npos) continue;
                        const int d = busca (nomsDestins, numDestins, r.substr (0, dp));
                        if (d <= 0) continue;
                        s.posa (Mc1D1 + k * 4 + n * 2, (float) d);
                        s.posa (Mc1Q1 + k * 4 + n * 2, std::strtof (r.c_str() + dp + 1, nullptr));
                        ++n;
                    }
                    continue;
                }
                const float v = std::strtof (resta.c_str(), nullptr);
                for (int p = 0; p < numParams; ++p)
                    if (clau == info (p).clau) { s.posa (p, v); break; }
            }
            for (int k = 0; k < numMacros; ++k)
            {
                if (macroDona[k]) continue;
                const auto& r = rutesDefecte[k];
                s.posa (Mc1D1 + k * 4, (float) r.d1); s.posa (Mc1Q1 + k * 4, r.q1);
                s.posa (Mc1D2 + k * 4, (float) r.d2); s.posa (Mc1Q2 + k * 4, r.q2);
            }
            // Evolució: si el preset no la fa servir enlloc, l'envolupant de mod mou el morph i el tall a través seu
            if (! evolucio && slot < numSlots)
            {
                const int b = M1Font + slot * 4;
                s.posa (b, (float) FEnvMod); s.posa (b + 1, (float) VMac7); s.posa (b + 2, (float) DMorph); s.posa (b + 3, 0.5f);
                ++slot;
                if (slot < numSlots)
                {
                    const int c = M1Font + slot * 4;
                    s.posa (c, (float) FEnvMod); s.posa (c + 1, (float) VMac7); s.posa (c + 2, (float) DTall); s.posa (c + 3, 0.25f);
                }
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

    std::string estatInicial() { auto s = std::make_unique<Neo>(); s->posa (Mac2, 0.5f); aplica (*s, ""); return s->estatSo(); }

    std::string construeix (const std::string& canvis)
    {
        auto s = std::make_unique<Neo>();
        s->posa (Mac2, 0.5f);
        aplica (*s, canvis.c_str());
        return s->estatSo();
    }

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> v = []
        {
            std::vector<Preset> r;
            for (const auto& x : taula) r.push_back ({ x.nom, x.cat, x.desc, construeix (x.canvis) });
            return r;
        }();
        return v;
    }

    std::string aleatori (unsigned llavor, int tipus)
    {
        Atzar a (llavor);
        if (tipus < 0 || tipus >= (int) tipusAtzar().size()) tipus = a.i (0, (int) tipusAtzar().size() - 1);
        std::ostringstream c;
        auto osc = [&] (int o, int motor, float niv)
        {
            c << " o" << o << "m=" << motor << " o" << o << "t=" << a.i (0, 15) << " o" << o << "p=" << a.r (0.0f, 0.7f)
              << " o" << o << "n=" << niv;
            if (motor == MAnalogic) c << " o" << o << "f=" << (a.p (0.5f) ? 0.33f : a.r (0.0f, 0.9f));
            else if (motor == MFm) c << " o" << o << "f=" << (float) a.i (0, 11) / 11.0f << " o" << o << "p=" << a.r (0.1f, 0.5f);
            else c << " o" << o << "f=" << a.r (0.3f, 0.7f);
        };
        auto motorAtzar = [&] { static const int m[] = { MWavetable, MWavetable, MAnalogic, MHarmonic, MFm }; return m[a.i (0, 4)]; };
        osc (1, motorAtzar(), 0.8f);
        if (a.p (0.6f)) { osc (2, motorAtzar(), a.r (0.25f, 0.6f)); if (a.p (0.4f)) c << " o2s=" << (a.p (0.5f) ? 7 : 12); else c << " o2fi=" << a.r (-15.0f, 15.0f); }
        const char* moviment[] = { " l1>p1=", " l1>cut=", " l1>forma=", " l1>voc=" };
        switch (tipus)
        {
            case 0:   // Baix
                c << " mode=1 veus=1 o1o=-1 o2o=-1 o3m=2 o3p=0 o3f=0 o3n=" << a.r (0.3f, 0.6f) << " cut=" << a.lg (300.0f, 1500.0f)
                  << " res=" << a.r (0.1f, 0.5f) << " fenv=" << a.r (0.2f, 0.6f) << " fd=" << a.lg (120.0f, 500.0f) << " fs=0.2 aa=1 ad=500 as=0.8 ar=120"
                  << " dq=" << a.r (0.0f, 0.4f) << (a.p (0.4f) ? " ft=4" : "") << (a.p (0.4f) ? " l2s=4 l2>cut=0.25" : "");
                break;
            case 1:   // Lead
                c << " mode=1 veus=1 glide=" << a.lg (20.0f, 120.0f) << " cut=" << a.lg (2000.0f, 8000.0f) << " res=" << a.r (0.1f, 0.4f)
                  << " aa=3 as=0.9 ar=200 l4in=500 l4>to=0.01 dlm=" << a.r (0.1f, 0.25f) << " dlt=3 rvm=0.15" << (a.p (0.4f) ? " uni=3 unid=0.25" : "");
                break;
            case 2:   // Pluck
                c << " aa=1 ad=" << a.lg (150.0f, 600.0f) << " as=0 ar=300 fa=1 fd=" << a.lg (120.0f, 400.0f) << " fs=0 fenv=" << a.r (0.4f, 0.7f)
                  << " cut=" << a.lg (600.0f, 2500.0f) << " res=" << a.r (0.1f, 0.4f) << " dlm=" << a.r (0.1f, 0.3f) << " dlt=3 rvm=0.2"
                  << (a.p (0.3f) ? " rsm=0.35 rst=" : "") << (a.p (0.3f) ? std::to_string (a.i (0, 3)) : "");
                break;
            case 3:   // Tecles
                c << " aa=2 ad=" << a.lg (800.0f, 2500.0f) << " as=0.3 ar=500 fa=1 fd=700 fs=0.25 fenv=0.35 cut=" << a.lg (2500.0f, 8000.0f)
                  << " vel>cut=0.25 chm=" << a.r (0.0f, 0.4f) << " rvm=0.2";
                break;
            case 4:   // Textura
                c << " o2m=4 o2t=" << a.i (0, 15) << " o2n=0.6 gd=" << a.lg (10.0f, 80.0f) << " gm=" << a.lg (30.0f, 300.0f) << " gs=" << a.r (0.3f, 1.0f)
                  << " aa=" << a.lg (800.0f, 3000.0f) << " as=0.9 ar=3000 l3w=5 l3r=" << a.lg (0.05f, 0.4f) << " l3>p2=0.4 rvm=" << a.r (0.35f, 0.5f) << " rvt=2";
                break;
            case 5:   // Pulsació
                c << " uni=3 unid=0.25 aa=5 as=0.9 ar=600 l2s=" << a.i (3, 5) << " l2w=" << (a.p (0.5f) ? 2 : 3) << " l2>vol=-0.8 cut="
                  << a.lg (1500.0f, 5000.0f) << " rvm=0.2";
                break;
            case 6:   // Metàl·lic
                c << " o1m=3 o1f=" << (float) a.i (6, 11) / 11.0f << " rsm=" << a.r (0.3f, 0.7f) << " rst=" << a.i (1, 2) << " rsd=0.7 aa=0.5 ad="
                  << a.lg (800.0f, 4000.0f) << " as=0 ar=2500 cut=9000 rvm=0.3" << (a.p (0.4f) ? " shm=0.25 shh=7" : "");
                break;
            case 7:   // Vocal
                c << " ft=5 cut=" << a.lg (800.0f, 1500.0f) << " res=" << a.r (0.4f, 0.65f) << " voc=" << a.r (0.0f, 1.0f) << " uni=3 unid=0.2 aa="
                  << a.lg (40.0f, 600.0f) << " as=0.9 ar=800" << moviment[3] << a.r (0.2f, 0.5f) << " l1r=" << a.lg (0.1f, 1.0f) << " rvm=0.3";
                break;
            case 8:   // Riser
                c << " uni=5 unid=0.3 o2m=5 o2n=0.35 ea=" << a.lg (3000.0f, 8000.0f) << " ed=1 es=1 em>to=" << (a.p (0.5f) ? 1.0f : 0.58f)
                  << " em>cut=0.6 em>ud=0.3 cut=500 aa=300 as=1 ar=1500 rvm=0.3 rvt=1";
                break;
            case 9:   // Impacte
                c << " o1m=2 o1p=0 o1f=0.1 o2m=5 o2n=0.4 em>to=" << a.r (0.5f, 1.0f) << " ea=0.5 ed=" << a.lg (40.0f, 200.0f)
                  << " es=0 aa=0.5 ad=" << a.lg (800.0f, 3000.0f) << " as=0 ar=2000 dq=0.35 rvm=0.4 rvt=1";
                break;
            case 10:  // Glitch
                c << " pr=" << a.i (3, 5) << " pll=0 pas>p1=" << a.r (0.3f, 0.7f) << " pas>to=" << (a.p (0.5f) ? 0.25f : 0.0f) << " crm=" << a.r (0.2f, 0.6f)
                  << " crb=" << a.r (5.0f, 9.0f) << " crr=" << a.lg (1.0f, 6.0f) << " aa=1 as=1 ar=60";
                for (int k = 1; k <= 8; ++k) c << " p" << k << "=" << a.r (-1.0f, 1.0f);
                break;
            default:  // Morph
                c << " o2m=0 o2n=0.5 aa=" << a.lg (100.0f, 1500.0f) << " as=0.9 ar=2000 l1r=" << a.lg (0.05f, 0.3f) << " l1>morph=" << a.r (0.4f, 0.8f)
                  << " rvm=0.3";
                break;
        }
        if (tipus != 1 && tipus != 0 && a.p (0.4f)) c << moviment[a.i (0, 3)] << a.r (0.1f, 0.35f) << " l1r=" << a.lg (0.08f, 1.5f);
        auto s = std::make_unique<Neo>();
        s->posa (Mac2, 0.5f);
        aplica (*s, c.str().c_str());
        // volum aproximat segons l'energia
        float e = 0.0f;
        for (int o = 0; o < 3; ++o) e += s->valor (O1Niv + o * 11);
        e *= 1.0f + 0.15f * (s->valor (UniVeus) - 1.0f) / 7.0f;
        s->posa (Volum, std::clamp (-7.0f - 6.0f * std::log2 (std::max (0.5f, e)) - 3.0f * s->valor (DQuant), -18.0f, 0.0f));
        return s->estatSo();
    }

    std::string varia (const std::string& estat, unsigned llavor, float quant)
    {
        Atzar a (llavor);
        auto sp = std::make_unique<Neo>();
        auto& s = *sp;
        s.posaEstat (estat);
        for (int p = 0; p < numParams; ++p)
        {
            const auto& i = info (p);
            if (i.escala == sinte::Enter || p == Volum || p == BendRang || p >= Mode) continue;
            if (p >= Mac1 && p <= Mc8Q2) continue;   // les macros són de l'usuari
            if (p >= M1Font && p <= M12Quant && s.valor (p - (p - M1Font) % 4) < 0.5f) continue;   // vies buides
            if (std::abs (s.valor (p)) < 1e-6f) continue;   // el que està apagat es queda apagat
            if ((p >= O1Niv && p <= O3Niv && (p - O1Niv) % 11 == 0) && s.valor (p) < 1e-6f) continue;
            float salt = 0.15f * quant;
            if (p == O1Fi || p == O2Fi || p == O3Fi) salt *= 0.4f;
            if (p >= Pas1 && p <= Pas16) salt *= 2.0f;
            s.posa (p, sinte::deNormal (i, sinte::normal (i, s.valor (p)) + (a.u() * 2.0f - 1.0f) * salt));
        }
        return s.estatSo();
    }
}
