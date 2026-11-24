#include "mibase/Rack.h"
#include "mibase/Clipper.h"
#include "mibase/Equalitzador.h"
#include "mibase/Transitoris.h"
#include "mibase/Estereo.h"
#include "mibase/LowEnd.h"
#include "mibase/Multibanda.h"
#include "mibase/Limitador.h"
#include "mibase/Loudness.h"
#include "mibase/Saturacio.h"
#include <algorithm>
#include <cmath>

/** 0.94: els presets de fàbrica del Mastering Studio.

    Cada preset és una cadena real (EQ de màster → de-esser → compressor → saturació → estèreo → limitador) amb
    els seus valors, els 8 macros connectats i un objectiu de loudness orientatiu. Són punts de partida per escoltar
    i ajustar, no receptes universals: el mateix preset sona diferent en cada mescla.

    Els 8 macros del màster (si el preset té el mòdul): Escalfor, Presència, Punch, Amplada, Greus, Densitat, Aire
    i Loudness. Els presets inspirats en artistes arribaran més endavant, després d'investigar-los (amb una fitxa
    que separi el que està documentat del que és interpretació). */
namespace mibase::rack
{
    namespace
    {
        // Bandes de l'EQ de màster: 0 passa-alts (sub), 1 greus (prestatge 100 Hz), 2 cos (campana), 3 fang (campana),
        // 4 presència (campana, estèreo o només Mid), 5 aire (prestatge 12 kHz), 6 (opcional) passa-alts només al Side
        struct ConfM
        {
            const char* cat; const char* nom; const char* desc;
            // EQ
            float hpf = 25, greus = 0, cosF = 180, cos = 0, fangF = 350, fang = 0, presF = 3000, pres = 0, aire = 0, sideHpf = 0;
            bool presMid = false;
            // de-esser
            bool deess = false; float deessF = 7500, deessThr = -30;
            // compressor
            bool comp = true; float thr = -18, ratio = 2, atac = 30, rel = 150, makeup = 1, knee = 6, scHpf = 100, mix = 100;
            // saturació
            bool sat = false; int estil = saturacio::A; float drive = 1.5f, satMix = 35, tone = 0;
            // 0.99: multibanda (3 bandes) i low end focus
            bool mb = false; float mbX1 = 150, mbX2 = 3000, mbThr[3] { -22, -22, -24 }, mbRatio = 2;
            bool le = false; float leSub = 0, lePunch = 0, leTight = 0, leEsc = 0, leFang = 0, leMono = 120;
            // 0.96: transient shaper i clipper
            bool tr = false; float trAtac = 0, trSus = 0; int trFocus = transitoris::Tot;
            bool clip = false; int clipMode = clipper::Soft; float clipDrive = 1.5f, clipCeil = -1.5f;
            // estèreo
            float amp = 100, ampG = 100, ampM = 100, ampA = 100, monoG = 0;
            // limitador
            int limEstil = limitador::Transparent; float limGain = 3, ceil = -1;
            int objectiu = 1;
        };

        struct Enl { int modul, param; float mn, mx; };

        struct Constructor
        {
            Rack r { Tipus::Master };
            int iEq = -1, iDeEss = -1, iComp = -1, iSat = -1, iSt = -1, iLim = -1, iTr = -1, iClip = -1, iMb = -1, iLe = -1;

            float normal (int modul, int p, float v)
            {
                auto* f = r.modul (modul)->fx.get();
                if (dynamic_cast<eq::Equalitzador*> (f)) return (v + eq::guanyMax) / (2.0f * eq::guanyMax);   // només guanys
                if (dynamic_cast<dinamica::Dinamica*> (f)) return dinamica::Dinamica::normal (p, v);
                if (dynamic_cast<saturacio::Saturacio*> (f)) return saturacio::Saturacio::normal (p, v);
                if (dynamic_cast<estereo::Estereo*> (f)) return estereo::Estereo::normal (p, v);
                if (dynamic_cast<limitador::Limitador*> (f)) return limitador::Limitador::normal (p, v);
                if (dynamic_cast<clipper::Clipper*> (f)) return clipper::Clipper::normal (p, v);
                if (dynamic_cast<multibanda::Multibanda*> (f)) return multibanda::Multibanda::normal (p, v);
                if (dynamic_cast<lowend::LowEnd*> (f)) return lowend::LowEnd::normal (p, v);
                if (dynamic_cast<transitoris::Transitoris*> (f)) return transitoris::Transitoris::normal (p, v);
                return 0.5f;
            }
            /** Com al Vocal Studio: el macro arrenca on ja és el primer paràmetre i els altres el segueixen; aquí, a més, el preset
                sona exactament com està escrit (el rang dels altres es desplaça al seu voltant). */
            void macro (int m, std::initializer_list<Enl> es)
            {
                bool primer = true;
                float v = 0.5f;
                for (const auto& e : es)
                {
                    if (e.modul < 0) continue;
                    float a = normal (e.modul, e.param, e.mn), b = normal (e.modul, e.param, e.mx);
                    const float ara = r.modul (e.modul)->fx->parametre (e.param);
                    if (primer)
                    {
                        v = std::abs (b - a) > 1e-6f ? std::clamp ((ara - a) / (b - a), 0.0f, 1.0f) : 0.5f;
                        r.posaMacro (m, v);
                        primer = false;
                    }
                    else
                    {
                        // els altres: el mateix recorregut, desplaçat perquè amb el macro on és quedin on el preset els ha posat
                        const float span = b - a;
                        a = std::clamp (ara - span * v, 0.0f, 1.0f);
                        b = std::clamp (a + span, 0.0f, 1.0f);
                    }
                    r.enllaca ({ m, e.modul, e.param, a, b });
                }
                r.aplicaMacro (m);
            }
        };

        std::string fes (const ConfM& c)
        {
            Constructor k;
            {
                auto q = std::make_shared<eq::Equalitzador>();
                auto banda = [&q] (int n, int tipus, float f, float g, float qq, int canal = eq::Estereo)
                {
                    eq::Banda b; b.usada = true; b.tipus = tipus; b.freq = f; b.guany = g; b.q = qq; b.canal = canal;
                    q->posaBanda (n, b);
                };
                banda (0, eq::HighPass, c.hpf, 0, eq::qDefecte);
                banda (1, eq::LowShelf, 100.0f, c.greus, eq::qDefecte);
                banda (2, eq::Bell, c.cosF, c.cos, 0.9f);
                banda (3, eq::Bell, c.fangF, c.fang, 1.0f);
                banda (4, eq::Bell, c.presF, c.pres, 0.7f, c.presMid ? eq::Mid : eq::Estereo);
                banda (5, eq::HighShelf, 12000.0f, c.aire, eq::qDefecte);
                if (c.sideHpf > 0) banda (6, eq::HighPass, c.sideHpf, 0, eq::qDefecte, eq::Side);
                k.iEq = k.r.afegeix ("eqm", q);
            }
            if (c.mb)   // 0.99
            {
                auto m = std::make_shared<multibanda::Multibanda>();
                using namespace multibanda;
                m->posa (Bandes, 3); m->posa (X1, c.mbX1); m->posa (X2, c.mbX2);
                for (int b = 0; b < 3; ++b) { m->posa (index (b, Llindar), c.mbThr[b]); m->posa (index (b, Ratio), c.mbRatio); }
                k.iMb = k.r.afegeix ("multibanda", m);
            }
            if (c.deess)
            {
                auto d = std::make_shared<dinamica::Dinamica>();
                using namespace dinamica;
                d->posa (ModeP, DeEsser); d->posa (FreqDeEsser, c.deessF); d->posa (Llindar, c.deessThr); d->posa (Ratio, 3); d->posa (Atac, 1); d->posa (Alliberament, 80);
                k.iDeEss = k.r.afegeix ("deessm", d);
            }
            if (c.comp)
            {
                auto d = std::make_shared<dinamica::Dinamica>();
                using namespace dinamica;
                d->posa (ModeP, Compressor); d->posa (Llindar, c.thr); d->posa (Ratio, c.ratio); d->posa (Atac, c.atac); d->posa (Alliberament, c.rel);
                d->posa (Guany, c.makeup); d->posa (Genoll, c.knee); d->posa (FiltreGreus, c.scHpf); d->posa (Detector, 1); d->posa (Mix, c.mix);
                k.iComp = k.r.afegeix ("glue", d);
            }
            if (c.le)   // 0.99
            {
                auto g = std::make_shared<lowend::LowEnd>();
                using namespace lowend;
                g->posa (Sub, c.leSub); g->posa (Punch, c.lePunch); g->posa (Tight, c.leTight); g->posa (Escalfor, c.leEsc); g->posa (Fang, c.leFang); g->posa (Mono, c.leMono);
                k.iLe = k.r.afegeix ("lowend", g);
            }
            if (c.tr)   // 0.96
            {
                auto t = std::make_shared<transitoris::Transitoris>();
                t->posa (transitoris::Atac, c.trAtac); t->posa (transitoris::Sustain, c.trSus); t->posa (transitoris::FocusP, (float) c.trFocus);
                k.iTr = k.r.afegeix ("transitoris", t);
            }
            if (c.sat)
            {
                auto s = std::make_shared<saturacio::Saturacio>();
                using namespace saturacio;
                s->posa (EstilP, (float) c.estil); s->posa (Drive, c.drive); s->posa (Mix, c.satMix); s->posa (Tone, c.tone); s->posa (Auto, 1);
                k.iSat = k.r.afegeix ("colorm", s);
            }
            {
                auto s = std::make_shared<estereo::Estereo>();
                using namespace estereo;
                s->posa (Amplada, c.amp); s->posa (AmpGreus, c.ampG); s->posa (AmpMitjos, c.ampM); s->posa (AmpAguts, c.ampA); s->posa (MonoGreus, c.monoG);
                k.iSt = k.r.afegeix ("estereo", s);
            }
            if (c.clip)   // 0.96
            {
                auto cl = std::make_shared<clipper::Clipper>();
                cl->posa (clipper::ModeP, (float) c.clipMode); cl->posa (clipper::Drive, c.clipDrive); cl->posa (clipper::Ceiling, c.clipCeil);
                k.iClip = k.r.afegeix ("clipper", cl);
            }
            {
                auto l = std::make_shared<limitador::Limitador>();
                using namespace limitador;
                l->posa (EstilP, (float) c.limEstil); l->posa (Gain, c.limGain); l->posa (Ceiling, c.ceil); l->posa (TruePeak, 1); l->posa (AutoRel, 1);
                k.iLim = k.r.afegeix ("limitm", l);
            }
            k.r.posaObjectiu (c.objectiu);

            const int bGreus = eq::indexParam (1, eq::PGuany), bCos = eq::indexParam (2, eq::PGuany);
            const int bPres = eq::indexParam (4, eq::PGuany), bAire = eq::indexParam (5, eq::PGuany);
            // M1 Escalfor: saturació (si n'hi ha) i una mica de cos
            if (k.iSat >= 0) k.macro (0, { { k.iSat, saturacio::Drive, 0.0f, 6.0f }, { k.iEq, bCos, -1.0f, 2.5f } });
            else k.macro (0, { { k.iEq, bCos, -2.0f, 3.0f } });
            // M2 Presència
            k.macro (1, { { k.iEq, bPres, -3.0f, 4.0f } });
            // M3 Punch: l'atac del compressor (més lent = deixa passar més el cop)
            if (k.iComp >= 0) k.macro (2, { { k.iComp, dinamica::Atac, 8.0f, 70.0f }, { k.iTr, transitoris::Atac, -20.0f, 60.0f } });
            else k.macro (2, { { k.iTr, transitoris::Atac, -20.0f, 60.0f } });   // 0.96: el transient shaper també fa punch
            // M4 Amplada: el Side de tot i, una mica més, el dels aguts
            k.macro (3, { { k.iSt, estereo::Amplada, 60.0f, 150.0f }, { k.iSt, estereo::AmpAguts, 90.0f, 130.0f } });
            // M5 Greus
            k.macro (4, { { k.iEq, bGreus, -4.0f, 4.0f }, { k.iLe, lowend::Tight, 40.0f, 0.0f }, { k.iLe, lowend::Sub, -2.0f, 2.5f } });   // 0.99: amb Low End Focus, menys greus = més ajustats
            // M6 Densitat: compressió (amb el seu makeup) i saturació en paral·lel
            if (k.iComp >= 0)
                k.macro (5, { { k.iComp, dinamica::Llindar, -8.0f, -28.0f }, { k.iComp, dinamica::Guany, 0.0f, 3.5f }, { k.iSat, saturacio::Mix, 15.0f, 70.0f },
                              { k.iMb, multibanda::Mix, 30.0f, 100.0f } });
            else k.macro (5, { { k.iMb, multibanda::Mix, 30.0f, 100.0f }, { k.iSat, saturacio::Mix, 15.0f, 70.0f } });   // 0.99
            // M7 Aire: el prestatge alt i, si n'hi ha, el de-esser es posa més atent
            k.macro (6, { { k.iEq, bAire, -3.0f, 5.0f }, { k.iDeEss, dinamica::Llindar, -24.0f, -38.0f } });
            // M8 Loudness: el guany del limitador (el sostre no es mou mai)
            k.macro (7, { { k.iLim, limitador::Gain, 0.0f, 12.0f }, { k.iClip, clipper::Drive, 0.0f, 4.0f } });   // 0.96: amb clipper, se'n reparteixen la feina
            return k.r.estat();
        }

        std::vector<ConfM> taula()
        {
            using namespace saturacio;
            using limitador::Transparent; using limitador::Modern; using limitador::Punch; using limitador::Agressiu;
            std::vector<ConfM> v;
            auto nou = [&v] (const char* cat, const char* nom, const char* desc) -> ConfM& { v.push_back ({}); auto& c = v.back(); c.cat = cat; c.nom = nom; c.desc = desc; return c; };

            // ---- Bàsics ----
            { auto& c = nou ("Bàsics", "Màster net", "El punt de partida: treu el subgreu inútil, cohesió molt suau (1–2 dB) i limitador transparent. Vigila: és neutre a propòsit; mou els macros.");
              c.thr = -16; c.ratio = 1.5f; c.limGain = 3; }
            { auto& c = nou ("Bàsics", "Només limitador", "Per a una mescla que ja sona com vols: només puja el volum sense passar del sostre. Vigila: si el limitador redueix més de 3–4 dB, perds cop.");
              c.comp = false; c.limGain = 4; }
            { auto& c = nou ("Bàsics", "Dinàmica natural", "Gairebé no comprimeix: respecta els cops i les diferències entre estrofa i tornada. Per a cançons que respiren. Vigila: sonarà més fluix que un màster comercial (és normal).");
              c.thr = -14; c.ratio = 1.5f; c.atac = 40; c.rel = 250; c.limGain = 2; c.objectiu = 3; }

            // ---- Objectiu sonor ----
            { auto& c = nou ("Objectiu sonor", "Càlid", "Cinta en paral·lel, una mica de cos i els aguts suaus. Per a mescles una mica fredes o digitals. Vigila: massa escalfor enfosqueix la veu.");
              c.sat = true; c.estil = A; c.drive = 2; c.satMix = 40; c.cos = 1; c.aire = -1; c.limGain = 4; }
            { auto& c = nou ("Objectiu sonor", "Brillant", "Més aire i presència, amb un de-esser que vigila les S i els plats. Per a mescles apagades. Vigila: si ja és brillant, cansa l'oïda.");
              c.aire = 2.5f; c.pres = 1; c.presF = 4000; c.deess = true; c.limGain = 4; }
            { auto& c = nou ("Objectiu sonor", "Punchy", "Atac lent al compressor perquè el cop de la bateria passi, i limitador Punch. Vigila: amb massa Loudness es perd el que guanyes.");
              c.atac = 45; c.rel = 120; c.ratio = 2.5f; c.greus = 1; c.limEstil = Punch; c.limGain = 5; c.tr = true; c.trAtac = 25; }
            { auto& c = nou ("Objectiu sonor", "Ample", "Obre els aguts i els mitjos i deixa els greus al mig (mono per sota de 120 Hz). Vigila: comprova-ho amb el botó Mono de l'Estèreo i la correlació.");
              c.amp = 115; c.ampA = 130; c.monoG = 120; c.limGain = 4; }
            { auto& c = nou ("Objectiu sonor", "Dens", "Compressió més ferma, saturació de consola i limitador modern: un màster compacte i ple. Vigila: la tornada pot quedar al mateix nivell que l'estrofa.");
              c.thr = -22; c.ratio = 2.5f; c.atac = 20; c.rel = 120; c.makeup = 2; c.sat = true; c.estil = E; c.drive = 2; c.satMix = 35;
              c.limEstil = Modern; c.limGain = 6; c.objectiu = 4; c.mb = true; }
            { auto& c = nou ("Objectiu sonor", "Greus controlats", "Per a mescles amb un baix que bombeja o s'escapa: multibanda de 3 bandes que frena els greus, Low End Focus amb Tight i mono per sota de 120 Hz. Vigila: si treus massa greus, perd cos.");
              c.mb = true; c.mbThr[0] = -26; c.mbRatio = 2.5f; c.le = true; c.leTight = 30; c.lePunch = 15; c.leFang = 20; c.ratio = 1.5f; c.limGain = 4; c.objectiu = 4; }
            { auto& c = nou ("Objectiu sonor", "Transparent", "El mínim que es pot fer: sense color, compressió quasi imperceptible. Per a mescles molt acabades. Vigila: no esperis canvis de caràcter.");
              c.thr = -14; c.ratio = 1.3f; c.atac = 40; c.limGain = 2; }
            { auto& c = nou ("Objectiu sonor", "Vintage", "Cinta més present, aguts arrodonits, greus una mica més plens i compressió lenta. Vigila: amb material ja fosc, pot quedar tapat.");
              c.hpf = 30; c.sat = true; c.estil = A; c.drive = 3; c.satMix = 50; c.tone = -1; c.aire = -1.5f; c.greus = 1; c.rel = 300; c.amp = 95; c.limGain = 3; }
            { auto& c = nou ("Objectiu sonor", "Fort", "Per competir en volum: compressió ferma, saturació i limitador agressiu. Vigila: mira el crest i la reducció del limitador; si el bombo s'aixafa, baixa el Loudness.");
              c.thr = -22; c.ratio = 3; c.atac = 15; c.rel = 100; c.makeup = 2; c.sat = true; c.estil = E; c.drive = 2; c.satMix = 30;
              c.clip = true; c.clipMode = clipper::Soft; c.clipDrive = 2; c.clipCeil = -1.5f;
              c.limEstil = Agressiu; c.limGain = 7; c.objectiu = 5; }
            { auto& c = nou ("Objectiu sonor", "Veu al davant", "Presència només al Mid (on és la veu), menys fang i de-esser. Vigila: si la veu està enterrada a la mescla, és millor arreglar-ho a la pista.");
              c.pres = 1.5f; c.presF = 2500; c.presMid = true; c.fang = -1; c.deess = true; c.limGain = 4; }
            { auto& c = nou ("Objectiu sonor", "Guitarres al davant", "Presència cap als 2 kHz, menys fang i mitjos una mica més oberts. Vigila: les guitarres amb massa greus s'arreglen a la mescla, no al màster.");
              c.pres = 1.5f; c.presF = 2000; c.fang = -1.5f; c.fangF = 300; c.ampM = 110; c.limGain = 4; }
            { auto& c = nou ("Objectiu sonor", "Fosc i íntim", "Aguts suaus, una vàlvula molt subtil i poca compressió. Per a cançons tranquil·les i properes. Vigila: als auriculars de mòbil pot perdre definició.");
              c.aire = -2; c.pres = -0.5f; c.ratio = 1.5f; c.thr = -16; c.atac = 40; c.sat = true; c.estil = T; c.drive = 1.5f; c.satMix = 25; c.limGain = 2; c.objectiu = 3; }

            // ---- Gèneres ----
            { auto& c = nou ("Gèneres", "Indie rock", "Mitjos nets, presència i una mica d'aire; cinta lleugera i aguts oberts. Vigila: el fang de les guitarres (250–350 Hz) mana; ajusta'l d'orella.");
              c.fang = -1; c.fangF = 300; c.pres = 1; c.aire = 1; c.sat = true; c.estil = A; c.drive = 1.5f; c.satMix = 30; c.ampA = 115; c.monoG = 100;
              c.limEstil = Modern; c.limGain = 5; c.objectiu = 4; }
            { auto& c = nou ("Gèneres", "Pop rock", "Greus plens però controlats, presència i aire, una mica més ample. Vigila: comprova que la veu no quedi darrere de les guitarres.");
              c.greus = 1; c.fang = -1; c.pres = 1; c.aire = 1.5f; c.thr = -20; c.atac = 25; c.rel = 120; c.makeup = 1.5f; c.amp = 110; c.monoG = 100;
              c.limEstil = Modern; c.limGain = 6; c.objectiu = 4; }
            { auto& c = nou ("Gèneres", "Rock contundent", "Greus i presència, consola, compressió ferma i limitador Punch: gran i agressiu. Vigila: la bateria ha de seguir colpejant; si no, menys Loudness.");
              c.greus = 1.5f; c.fang = -1.5f; c.pres = 1.5f; c.presF = 2500; c.aire = 1; c.thr = -20; c.ratio = 3; c.atac = 20; c.rel = 100;
              c.sat = true; c.estil = E; c.drive = 2.5f; c.satMix = 35; c.ampA = 120; c.monoG = 120; c.tr = true; c.trAtac = 20; c.trFocus = transitoris::Mitjos;
              c.clip = true; c.clipDrive = 1.5f; c.limEstil = Punch; c.limGain = 7; c.objectiu = 5; c.mb = true; c.le = true; c.lePunch = 25; c.leTight = 15; }
            { auto& c = nou ("Gèneres", "Folk i acústic", "Natural: treu una mica de fang, afegeix aire i comprimeix molt poc. Vigila: les guitarres acústiques poden tenir massa greu en primer pla.");
              c.hpf = 30; c.fang = -1; c.fangF = 250; c.aire = 1.5f; c.ratio = 1.5f; c.thr = -16; c.atac = 40; c.rel = 250; c.limGain = 2; c.objectiu = 3; }
            { auto& c = nou ("Gèneres", "Synth pop", "Greus definits i mono, aire, compressió més ràpida i ample als mitjos i aguts. Vigila: els sintes amples als greus poden perdre's en mono.");
              c.greus = 1.5f; c.aire = 2; c.ratio = 2.5f; c.thr = -20; c.atac = 10; c.rel = 80; c.sat = true; c.estil = E; c.drive = 1.5f; c.satMix = 25;
              c.amp = 115; c.ampG = 80; c.monoG = 120; c.clip = true; c.clipMode = clipper::Modern; c.clipDrive = 1.5f; c.limEstil = Modern; c.limGain = 6; c.objectiu = 4;
              c.le = true; c.leSub = 1; c.lePunch = 20; c.leEsc = 25; }
            { auto& c = nou ("Gèneres", "Balada", "Cos i aire, compressió lenta i suau, una mica de cinta. Deixa créixer la cançó. Vigila: no la facis tan forta com un tema ràpid.");
              c.cos = 1; c.aire = 1; c.ratio = 1.5f; c.thr = -18; c.atac = 40; c.rel = 300; c.sat = true; c.estil = A; c.drive = 1.2f; c.satMix = 25; c.amp = 110;
              c.limGain = 3; c.objectiu = 3; }

            return v;
        }
    }

    // ---- 0.95: l'assistent de loudness ----
    int limitadorFinal (const Rack& r)
    {
        for (int i = r.numModuls() - 1; i >= 0; --i)
            if (r.cami (i) == 0 && dynamic_cast<limitador::Limitador*> (r.modul (i)->fx.get()) != nullptr) return i;
        return -1;
    }

    float guanyLimitador (const Rack& r)
    {
        const int i = limitadorFinal (r);
        return i < 0 ? 0.0f : dynamic_cast<limitador::Limitador*> (r.modul (i)->fx.get())->valor (limitador::Gain);
    }

    void posaGuanyLimitador (Rack& r, float db)
    {
        const int i = limitadorFinal (r);
        if (i < 0) return;
        auto* l = dynamic_cast<limitador::Limitador*> (r.modul (i)->fx.get());
        db = std::clamp (db, 0.0f, 24.0f);
        const float n = limitador::Limitador::normal (limitador::Gain, db);
        // si un macro mou el guany, el macro es posa on toca (i, si no hi arriba, se n'estén el rang)
        for (int k = 0; k < (int) r.enllacos().size(); ++k)
        {
            auto e = r.enllacos()[(size_t) k];
            if (e.modul != i || e.param != limitador::Gain) continue;
            if (n > std::max (e.mn, e.mx) || n < std::min (e.mn, e.mx))
            {
                if (e.mx >= e.mn) { e.mx = std::max (e.mx, n); e.mn = std::min (e.mn, n); }
                else { e.mn = std::max (e.mn, n); e.mx = std::min (e.mx, n); }
                r.posaEnllac (k, e);
            }
            const float v = std::abs (e.mx - e.mn) > 1e-6f ? std::clamp ((n - e.mn) / (e.mx - e.mn), 0.0f, 1.0f) : 0.0f;
            r.posaMacro (e.macro, v);   // mou el guany (i els altres enllaços del macro, com si el moguessis a mà)
        }
        l->posa (limitador::Gain, db);
        r.marcaModificat();
    }

    void posaSostre (Rack& r, float dbtp)
    {
        const int i = limitadorFinal (r);
        if (i < 0) return;
        auto* l = dynamic_cast<limitador::Limitador*> (r.modul (i)->fx.get());
        // 0,2 dB de marge: el true peak del limitador és una estimació i el mesurador (x4) a vegades veu una mica més
        l->posa (limitador::Ceiling, std::clamp (dbtp - 0.2f, -12.0f, 0.0f));
        l->posa (limitador::TruePeak, 1);
        r.posaOn (i, true);
    }

    void AssistentLoudness::comenca (Rack& r, float lufs, float tp)
    {
        objectiu = lufs; sostre = tp;
        iteracions = 0; grMax = 0.0f; error = 0.0f; mesurat = loudnessSilenci;
        if (limitadorFinal (r) < 0) { estat = Estat::SenseLimitador; return; }
        posaSostre (r, tp);
        r.reiniciaMesures();
        reiniciVist = false;
        estat = Estat::Escoltant;
    }

    AssistentLoudness::Estat AssistentLoudness::actualitza (Rack& r)
    {
        if (estat != Estat::Escoltant) return estat;
        const int i = limitadorFinal (r);
        const auto* out = r.loudnessSortida();
        if (i < 0 || out == nullptr) { estat = Estat::SenseLimitador; return estat; }
        auto* l = dynamic_cast<limitador::Limitador*> (r.modul (i)->fx.get());
        if (! reiniciVist) { if (out->segons.load() < 1.0) reiniciVist = true; else return estat; }
        grMax = std::max (grMax, l->mesures().reduccio.load());
        const float integ = out->integrated.load();
        if (out->segons.load() < segonsEscolta || integ <= loudness::silenci + 1.0f) return estat;
        mesurat = integ;
        error = objectiu - integ;
        if (std::abs (error) <= 0.5f) { estat = Estat::Fet; return estat; }
        if (error > 0.0f && grMax > grMaxima) { estat = Estat::Massa; return estat; }
        const float ara = guanyLimitador (r);
        if (error > 0.0f && ara >= 23.95f) { estat = Estat::NoArriba; return estat; }
        if (++iteracions > maxVoltes) { estat = Estat::Fet; return estat; }
        // quan el limitador ja treballa, el volum puja menys que el guany: hi anem una mica més de pressa
        const float factor = error > 0.0f && grMax > 2.0f ? 1.3f : 1.0f;
        posaGuanyLimitador (r, ara + error * factor);
        r.reiniciaMesures();
        reiniciVist = false;
        grMax = 0.0f;
        return estat;
    }

    const std::vector<std::string>& categories (Tipus t)
    {
        static const std::vector<std::string> m { "Bàsics", "Objectiu sonor", "Gèneres" };   // 0.95: sense Carmen 113
        return t == Tipus::Master ? m : categories();
    }

    const std::vector<Preset>& presetsFabrica (Tipus t)
    {
        if (t != Tipus::Master) return presetsFabrica();
        static const std::vector<Preset> p = []
        {
            std::vector<Preset> v;
            for (const auto& c : taula()) v.push_back ({ c.nom, c.cat, c.desc, fes (c) });
            return v;
        }();
        return p;
    }

    const std::string& estatInicial (Tipus t)
    {
        if (t != Tipus::Master) return estatInicial();
        static const std::string s = presetsFabrica (Tipus::Master).front().estat;
        return s;
    }
}
