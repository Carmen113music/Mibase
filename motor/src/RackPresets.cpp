#include "mibase/Rack.h"
#include "mibase/Delay.h"
#include "mibase/Doubler.h"
#include "mibase/Drive.h"
#include "mibase/Equalitzador.h"
#include "mibase/Harmonitzador.h"
#include "mibase/Limitador.h"
#include "mibase/Reverb.h"
#include "mibase/Saturacio.h"
#include <algorithm>
#include <cmath>

/** 0.70: els presets de fàbrica del Vocal Studio. Basats en les cadenes que en Roger va recollir
    (claude/rack-veu-referencies.md), sense les parts que encara no té el rack (afinador, vocoder). 0.73: noms «Artista · Cançó», harmonies i octava:
    són un punt de partida per ajustar d'orella amb la veu de cadascú.

    Tots fan servir els mateixos 8 macros (si el preset té el mòdul): Cos, Presència, Aire, Compressió, Escalfor,
    Doble, Espai i Eco. */
namespace mibase::rack
{
    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c { "Bàsics", "Íntim", "Pop", "Urbà", "Rock i indie", "Harmonies", "Efectes" };
        return c;
    }

    namespace
    {
        // Bandes de l'EQ: 0 passa-alts, 1 cos (campana), 2 presència (campana), 3 aire (prestatge alt), 4 passa-baixos (opcional)
        struct ConfEq { float hpf = 90, cosF = 220, cos = 0, presF = 3500, pres = 0, aire = 0, lpf = 0; };
        struct ConfComp { bool on = false; float thr = -20, ratio = 4, atac = 5, rel = 120, makeup = 3, mix = 100; };
        struct ConfDeEss { bool on = false; float freq = 6500, thr = -28; };
        struct ConfSat { bool on = false; int estil = saturacio::A; float drive = 2, mix = 60, tone = 0; };
        struct ConfDrive { bool on = false; int mode = drive::Crunch; float gain = 3, tone = 5, mix = 40; };
        struct ConfDbl { bool on = false; float quant = 35, cents = 9, retard = 14; };
        struct ConfDelay { bool on = false; bool sync = true; int nota = 8; float ms = 120, fb = 25, mix = 15, lc = 300, hc = 5000; int enc = delay::Estereo; };
        struct ConfRev { bool on = false; int alg = reverb::Plate; float decay = 1.4f, pre = 30, mix = 15, hc = 9000; };
        struct ConfLim { bool on = false; float gain = 0, ceil = -1; };
        struct ConfHarm   // 0.73
        {
            bool on = false; int mode = harmonitzador::Tonalitat;
            int v[4] { 2, 4, 0, 0 }; float niv[4] { -6, -6, -8, -8 }; float pan[4] { -40, 40, -70, 70 };
            float sec = 100, harm = 70, formant = 0, hum = 30, glide = 20;
        };
        struct ConfOct { bool on = false; float to = -12, formant = 0, mescla = 35; };   // 0.73
        struct Conf
        {
            Conf (const char* c, const char* n, const char* d) : cat (c), nom (n), desc (d) {}
            const char* cat; const char* nom; const char* desc;
            ConfEq eq;
            ConfDeEss deessAbans;     // un de-esser al principi (Dua Lipa)
            ConfComp comp, comp2;     // el segon: més lent, «òptic» (o en paral·lel, amb mix)
            ConfDeEss deess;
            ConfSat sat;
            ConfDrive drv;
            ConfDbl dbl;
            ConfDelay dly;
            ConfRev rev;
            ConfLim lim;
            ConfHarm harm;
            ConfOct oct;
            bool satAbans = false;    // la saturació abans de l'EQ (preamp al principi)
        };

        struct Constructor
        {
            Rack r;
            int iEq = -1, iComp = -1, iDeEss = -1, iSat = -1, iDrive = -1, iDbl = -1, iDly = -1, iRev = -1, iHarm = -1, iOct = -1;

            template <class T> T& fx (int i) { return *std::dynamic_pointer_cast<T> (r.modul (i)->fx); }

            int eq (const ConfEq& c)
            {
                auto q = std::make_shared<eq::Equalitzador>();
                auto banda = [&q] (int k, int tipus, float f, float g, float qq)
                {
                    eq::Banda b; b.usada = true; b.tipus = tipus; b.freq = f; b.guany = g; b.q = qq;
                    q->posaBanda (k, b);
                };
                banda (0, eq::HighPass, c.hpf, 0, eq::qDefecte);
                banda (1, eq::Bell, c.cosF, c.cos, 1.0f);
                banda (2, eq::Bell, c.presF, c.pres, 1.2f);
                banda (3, eq::HighShelf, 12000.0f, c.aire, eq::qDefecte);
                if (c.lpf > 0) banda (4, eq::LowPass, c.lpf, 0, eq::qDefecte);
                return r.afegeix ("eq", q);
            }
            int comp (const ConfComp& c)
            {
                auto d = std::make_shared<dinamica::Dinamica>();
                using namespace dinamica;
                d->posa (ModeP, Compressor); d->posa (Llindar, c.thr); d->posa (Ratio, c.ratio); d->posa (Atac, c.atac);
                d->posa (Alliberament, c.rel); d->posa (Guany, c.makeup); d->posa (Mix, c.mix);
                return r.afegeix ("compressor", d);
            }
            int deess (const ConfDeEss& c)
            {
                auto d = std::make_shared<dinamica::Dinamica>();
                using namespace dinamica;
                d->posa (ModeP, DeEsser); d->posa (FreqDeEsser, c.freq); d->posa (Llindar, c.thr); d->posa (Ratio, 4); d->posa (Atac, 1); d->posa (Alliberament, 60);
                return r.afegeix ("deesser", d);
            }
            int sat (const ConfSat& c)
            {
                auto s = std::make_shared<saturacio::Saturacio>();
                s->posa (saturacio::EstilP, (float) c.estil); s->posa (saturacio::Drive, c.drive); s->posa (saturacio::Mix, c.mix); s->posa (saturacio::Tone, c.tone);
                return r.afegeix ("decapitor", s);
            }
            int drv (const ConfDrive& c)
            {
                auto d = std::make_shared<drive::Drive>();
                d->posa (drive::ModeP, (float) c.mode); d->posa (drive::Gain, c.gain); d->posa (drive::Tone, c.tone); d->posa (drive::Mix, c.mix);
                return r.afegeix ("drive", d);
            }
            int dbl (const ConfDbl& c)
            {
                auto d = std::make_shared<doubler::Doubler>();
                d->posa (doubler::Quantitat, c.quant); d->posa (doubler::Desafina, c.cents); d->posa (doubler::Retard, c.retard);
                return r.afegeix ("doubler", d);
            }
            int harm (const ConfHarm& c)
            {
                auto h = std::make_shared<harmonitzador::Harmonitzador>();
                using namespace harmonitzador;
                h->posa (ModeP, (float) c.mode); h->posa (Sec, c.sec); h->posa (Harmonies, c.harm); h->posa (Formant, c.formant);
                h->posa (Humanitza, c.hum); h->posa (Lliscament, c.glide);
                for (int v = 0; v < 4; ++v) { h->posa (Interval1 + v, (float) c.v[v]); h->posa (Nivell1 + v, c.niv[v]); h->posa (Pan1 + v, c.pan[v]); }
                return r.afegeix ("harmonitzador", h);
            }
            int oct (const ConfOct& c)
            {
                auto o = std::make_shared<octava::Octava>();
                o->posa (octava::To, c.to); o->posa (octava::Formant, c.formant); o->posa (octava::Mescla, c.mescla);
                return r.afegeix ("octava", o);
            }
            int dly (const ConfDelay& c)
            {
                auto d = std::make_shared<delay::Delay>();
                using namespace delay;
                d->posa (Sync, c.sync ? 1.0f : 0.0f); d->posa (Nota, (float) c.nota); d->posa (Temps, c.ms); d->posa (Feedback, c.fb);
                d->posa (Mix, c.mix); d->posa (LowCut, c.lc); d->posa (HighCut, c.hc); d->posa (EncaminamentP, (float) c.enc);
                return r.afegeix ("delay", d);
            }
            int rev (const ConfRev& c)
            {
                auto v = std::make_shared<reverb::Reverb>();
                using namespace reverb;
                v->posa (Alg, (float) c.alg); v->posa (Decay, c.decay); v->posa (PreDelay, c.pre); v->posa (Mix, c.mix); v->posa (HighCut, c.hc);
                return r.afegeix ("reverb", v);
            }
            int lim (const ConfLim& c)
            {
                auto l = std::make_shared<limitador::Limitador>();
                l->posa (limitador::Gain, c.gain); l->posa (limitador::Ceiling, c.ceil);
                return r.afegeix ("limitador", l);
            }

            struct E { int modul, param; float mn, mx; };   // en unitats del paràmetre (dB, ms, %)
            float normal (int modul, int p, float v)
            {
                auto* f = r.modul (modul)->fx.get();
                if (dynamic_cast<eq::Equalitzador*> (f)) return (v + eq::guanyMax) / (2.0f * eq::guanyMax);   // només guanys
                if (dynamic_cast<dinamica::Dinamica*> (f)) return dinamica::Dinamica::normal (p, v);
                if (dynamic_cast<saturacio::Saturacio*> (f)) return saturacio::Saturacio::normal (p, v);
                if (dynamic_cast<drive::Drive*> (f)) return drive::Drive::normal (p, v);
                if (dynamic_cast<doubler::Doubler*> (f)) return doubler::Doubler::normal (p, v);
                if (dynamic_cast<harmonitzador::Harmonitzador*> (f)) return harmonitzador::Harmonitzador::normal (p, v);
                if (dynamic_cast<octava::Octava*> (f)) return octava::Octava::normal (p, v);
                if (dynamic_cast<delay::Delay*> (f)) return delay::Delay::normal (p, v);
                if (dynamic_cast<reverb::Reverb*> (f)) return reverb::Reverb::normal (p, v);
                return 0.5f;
            }
            /** El macro arrenca on ja és el primer paràmetre (el preset sona igual) i els altres el segueixen. */
            void macro (int m, std::initializer_list<E> es)
            {
                bool primer = true;
                for (const auto& e : es)
                {
                    if (e.modul < 0) continue;
                    const float a = normal (e.modul, e.param, e.mn), b = normal (e.modul, e.param, e.mx);
                    if (primer)
                    {
                        const float ara = r.modul (e.modul)->fx->parametre (e.param);
                        const float v = std::abs (b - a) > 1e-6f ? std::clamp ((ara - a) / (b - a), 0.0f, 1.0f) : 0.5f;
                        r.posaMacro (m, v);   // encara sense enllaços: no mou res
                        primer = false;
                    }
                    r.enllaca ({ m, e.modul, e.param, a, b });
                }
                r.aplicaMacro (m);
            }
        };

        std::string fes (const Conf& c)
        {
            Constructor k;
            if (c.satAbans && c.sat.on) k.iSat = k.sat (c.sat);
            if (c.deessAbans.on) k.deess (c.deessAbans);
            k.iEq = k.eq (c.eq);
            if (c.comp.on) k.iComp = k.comp (c.comp);
            if (c.comp2.on) k.comp (c.comp2);
            if (c.deess.on) k.iDeEss = k.deess (c.deess);
            if (! c.satAbans && c.sat.on) k.iSat = k.sat (c.sat);
            if (c.drv.on) k.iDrive = k.drv (c.drv);
            if (c.oct.on) k.iOct = k.oct (c.oct);
            if (c.harm.on) k.iHarm = k.harm (c.harm);
            if (c.dbl.on) k.iDbl = k.dbl (c.dbl);
            if (c.dly.on) k.iDly = k.dly (c.dly);
            if (c.rev.on) k.iRev = k.rev (c.rev);
            if (c.lim.on) k.lim (c.lim);

            const int bandaCos = eq::indexParam (1, eq::PGuany), bandaPres = eq::indexParam (2, eq::PGuany), bandaAire = eq::indexParam (3, eq::PGuany);
            k.macro (0, { { k.iEq, bandaCos, -5.0f, 5.0f } });
            k.macro (1, { { k.iEq, bandaPres, -4.0f, 6.0f } });
            k.macro (2, { { k.iEq, bandaAire, -4.0f, 8.0f }, { k.iDeEss, dinamica::Llindar, -22.0f, -36.0f } });
            k.macro (3, { { k.iComp, dinamica::Llindar, -6.0f, -34.0f }, { k.iComp, dinamica::Guany, 0.0f, 9.0f } });
            if (k.iSat >= 0) k.macro (4, { { k.iSat, saturacio::Drive, 0.0f, 7.0f }, { k.iSat, saturacio::Sortida, 0.0f, -4.0f } });
            else if (k.iDrive >= 0) k.macro (4, { { k.iDrive, drive::Gain, 0.0f, 9.0f }, { k.iDrive, drive::Level, 0.0f, -6.0f } });
            // 0.73: el macro 6 és el Doble; si el preset no en té i té harmonies o octava, les mou (i se'n diu així)
            if (k.iDbl >= 0) k.macro (5, { { k.iDbl, doubler::Quantitat, 0.0f, 80.0f } });
            else if (k.iHarm >= 0) { k.macro (5, { { k.iHarm, harmonitzador::Harmonies, 0.0f, 100.0f } }); k.r.posaNomMacro (5, "Harmonies"); }
            else if (k.iOct >= 0) { k.macro (5, { { k.iOct, octava::Mescla, 0.0f, 100.0f } }); k.r.posaNomMacro (5, "Octava"); }
            k.macro (6, { { k.iRev, reverb::Mix, 0.0f, 40.0f }, { k.iRev, reverb::PreDelay, 15.0f, 80.0f } });
            k.macro (7, { { k.iDly, delay::Mix, 0.0f, 45.0f }, { k.iDly, delay::Feedback, 10.0f, 40.0f } });
            return k.r.estat();
        }

        std::vector<Conf> taula()
        {
            std::vector<Conf> v;
            auto afegeix = [&v] (Conf c) { v.push_back (c); };
            using namespace reverb;
            // ---- Bàsics ----
            { Conf c { "Bàsics", "Veu neta", "El punt de partida: neteja, iguala el volum, suavitza les S i hi posa una mica d'espai." };
              c.eq = { 90, 250, -1.5f, 3500, 1.5f, 2.5f }; c.comp = { true, -20, 4, 5, 120, 3 }; c.deess = { true, 6500, -28 };
              c.rev = { true, Plate, 1.3f, 30, 12, 9000 }; afegeix (c); }
            { Conf c { "Bàsics", "Veu de maqueta", "Per escoltar les idees ben polides: més compressió, un eco discret i plate." };
              c.eq = { 100, 280, -2, 3500, 2, 3 }; c.comp = { true, -22, 4, 5, 100, 4 }; c.deess = { true, 6500, -30 };
              c.dly = { true, true, 8, 120, 22, 10, 300, 4500 }; c.rev = { true, Plate, 1.5f, 30, 15, 8000 }; afegeix (c); }
            { Conf c { "Bàsics", "Veu en directe", "Suau i segura per assajar amb la banda: talla greus i controla els crits." };
              c.eq = { 120, 300, -2, 3000, 1.5f, 1 }; c.comp = { true, -18, 3, 8, 150, 2 }; c.deess = { true, 6500, -26 };
              c.rev = { true, Room, 0.8f, 15, 10, 8000 }; c.lim = { true, 0, -3 }; afegeix (c); }
            { Conf c { "Bàsics", "Cors i segones veus", "Més fines i amples que la principal, una mica enrere." };
              c.eq = { 150, 300, -3, 3000, -1, 3 }; c.comp = { true, -24, 5, 10, 150, 4 }; c.deess = { true, 6500, -32 };
              c.dbl = { true, 45, 11, 18 }; c.rev = { true, Hall, 2.2f, 40, 25, 7000 }; afegeix (c); }
            { Conf c { "Bàsics", "Veu parlada", "Per a locucions i vídeos: clara, constant i sense pics." };
              c.eq = { 80, 200, 1, 3000, 2, 1.5f }; c.comp = { true, -20, 3, 10, 200, 4 }; c.deess = { true, 6000, -28 };
              c.lim = { true, 2, -1 }; afegeix (c); }
            // ---- Íntim ----
            { Conf c { "Íntim", "Billie Eilish · When the Party's Over", "A tocar del micro: doble compressió, molt d'aire i una habitació mínima." };
              c.eq = { 85, 250, -3, 3500, 0, 4 }; c.comp = { true, -24, 4, 8, 60, 3 }; c.comp2 = { true, -20, 2.5f, 20, 250, 2 };
              c.deess = { true, 6500, -30 }; c.rev = { true, Room, 0.4f, 10, 4, 9000 }; afegeix (c); }
            { Conf c { "Íntim", "Bon Iver · Holocene", "Veu fina i càlida amb preamp i una sala fosca. Grava 2–3 preses per doblar-la de debò." };
              c.satAbans = true; c.sat = { true, saturacio::E, 4, 50, 0 };
              c.eq = { 90, 300, 2, 3200, -2, 0 }; c.comp = { true, -18, 3, 15, 200, 3 };
              c.dbl = { true, 25, 6, 20 }; c.rev = { true, Chamber, 3.2f, 80, 22, 5000 }; afegeix (c); }
            { Conf c { "Íntim", "Frank Ocean · Ivy", "Cinta saturada, aguts apagats i un racó petit." };
              c.satAbans = true; c.sat = { true, saturacio::A, 3, 70, -3 };
              c.eq = { 80, 1500, 2, 3500, 0, -3 }; c.comp = { true, -20, 3, 15, 200, 3 };
              c.rev = { true, Chamber, 1.1f, 25, 14, 6000 }; afegeix (c); }
            // ---- Pop ----
            { Conf c { "Pop", "Dua Lipa · Don't Start Now", "Polida i brillant, amb doblat subtil, plate i eco a corxeres." };
              c.deessAbans = { true, 6000, -26 };
              c.eq = { 100, 280, -3, 5000, 2, 3 }; c.comp = { true, -22, 4, 3, 80, 3 }; c.comp2 = { true, -18, 2.5f, 20, 250, 2 };
              c.deess = { true, 7000, -30 }; c.dbl = { true, 15, 9, 12 };
              c.dly = { true, true, 5, 120, 20, 10, 400, 5000 }; c.rev = { true, Plate, 1.4f, 30, 14, 9000 }; afegeix (c); }
            { Conf c { "Pop", "Dua Lipa · Houdini", "Consola, una mica de doble i eco ping-pong a negres, filtrat." };
              c.eq = { 95, 250, 0, 2500, 3, 1 }; c.comp = { true, -22, 4, 2, 100, 4 };
              c.deess = { true, 6500, -28 }; c.dbl = { true, 18, 12, 16 };
              c.dly = { true, true, 8, 120, 25, 16, 500, 3500, delay::PingPong }; c.rev = { true, Plate, 1.2f, 25, 10, 8000 }; afegeix (c); }
            { Conf c { "Pop", "Coldplay · Fix You", "Gran i emotiva: preamp britànic, eco a negres i una sala llarga." };
              c.satAbans = true; c.sat = { true, saturacio::E, 3, 40, 0 };
              c.eq = { 80, 200, 2, 3500, 0, 2.5f }; c.comp = { true, -20, 3, 20, 200, 3 }; c.deess = { true, 6500, -28 };
              c.dbl = { true, 12, 8, 14 }; c.dly = { true, true, 8, 120, 25, 15, 300, 4000 };
              c.rev = { true, Hall, 3.5f, 60, 22, 7000 }; afegeix (c); }
            { Conf c { "Pop", "The Weeknd · Blinding Lights", "Harmònics brillants, eco a corxera amb punt i plate vintage." };
              c.satAbans = true; c.sat = { true, saturacio::P, 3, 40, 1 };
              c.eq = { 100, 250, 0, 3000, 2, 4 }; c.comp = { true, -24, 4, 3, 100, 4 }; c.deess = { true, 7000, -30 };
              c.dly = { true, true, 7, 120, 25, 12, 600, 3000 }; c.rev = { true, Plate, 1.8f, 35, 18, 8000 }; afegeix (c); }
            // ---- Urbà ----
            { Conf c { "Urbà", "Billie Eilish · Bad Guy", "Frontal, sense reverb: cinta, compressió dura en paral·lel i slapback." };
              c.eq = { 110, 250, 0, 3500, 2.5f, 1 }; c.sat = { true, saturacio::A, 4, 70, 0 };
              c.comp = { true, -26, 8, 1, 50, 4 }; c.comp2 = { true, -30, 20, 1, 60, 6, 20 };
              c.dly = { true, false, 8, 65, 0, 18, 400, 3000, delay::Mono }; afegeix (c); }
            { Conf c { "Urbà", "Rosalía · Malamente", "Present i viva; puja el macro Eco a l'última paraula de cada frase." };
              c.eq = { 120, 250, 0, 4000, 3, 2 }; c.comp = { true, -26, 8, 2, 80, 4 }; c.deess = { true, 7000, -30 };
              c.sat = { true, saturacio::Suau, 2, 30, 2 };
              c.dly = { true, true, 8, 120, 30, 2, 400, 4000 }; c.rev = { true, Hall, 3.0f, 40, 4, 7000 }; afegeix (c); }
            { Conf c { "Urbà", "Travis Scott · Sicko Mode", "Greu i fosca, amb doble ample i una sala llarga. L'afinador arribarà més endavant." };
              c.eq = { 90, 180, 2, 3500, 0, -3 }; c.comp = { true, -26, 6, 3, 80, 4 }; c.deess = { true, 6000, -28 };
              c.dbl = { true, 20, 14, 10 }; c.rev = { true, Hall, 4.0f, 40, 25, 6000 }; afegeix (c); }
            { Conf c { "Urbà", "James Blake · Retrograde", "Veu seca al davant i una sala enorme i fosca al darrere." };
              c.eq = { 60, 250, 0, 4000, -3, 0 }; c.comp = { true, -22, 3, 15, 200, 4 };
              c.rev = { true, Hall, 4.5f, 90, 28, 2500 }; afegeix (c); }
            // ---- Rock i indie ----
            { Conf c { "Rock i indie", "Indie rock", "Per a la banda: presència, una mica de triode i eco curt, que passi per sobre de les guitarres." };
              c.eq = { 100, 300, -2, 3000, 3, 2 }; c.comp = { true, -22, 4, 5, 100, 4 }; c.deess = { true, 6500, -28 };
              c.sat = { true, saturacio::T, 3, 40, 0 };
              c.dly = { true, true, 5, 120, 15, 10, 400, 4000 }; c.rev = { true, Plate, 1.2f, 25, 12, 8000 }; afegeix (c); }
            { Conf c { "Rock i indie", "Rock amb nervi", "Compressió ferma i una mica de pedal per a les parts fortes." };
              c.eq = { 110, 300, -2, 2500, 3, 1 }; c.comp = { true, -26, 6, 3, 80, 5 }; c.deess = { true, 6500, -28 };
              c.drv = { true, drive::Crunch, 3, 6, 25 }; c.rev = { true, Room, 0.9f, 15, 12, 7000 }; afegeix (c); }
            // ---- Harmonies (0.73) ----
            { Conf c { "Harmonies", "Bon Iver · 715 - CRΣΣKS", "Totes les veus surten del teclat MIDI: toca els acords a l'AKAI mentre cantes. Preamp de vàlvula i una sala fosca. (Sense afinador encara.)" };
              c.satAbans = true; c.sat = { true, saturacio::T, 5, 60, 0 };
              c.eq = { 100, 350, 2, 8000, -3, 0 }; c.comp = { true, -22, 4, 5, 120, 3 };
              c.harm = { true, harmonitzador::Teclat, { 0, 0, 0, 0 }, { 0, 0, -1, -1 }, { -60, -20, 20, 60 }, 0, 100, 0, 10, 0 };
              c.rev = { true, Hall, 2.8f, 45, 20, 4000 }; afegeix (c); }
            { Conf c { "Harmonies", "Imogen Heap · Hide and Seek", "Un cor de la teva veu que toques amb el teclat MIDI, sense la veu original, nets i amb una reverb llarga." };
              c.eq = { 120, 300, -1, 3500, 1, 3 }; c.comp = { true, -20, 3, 10, 150, 2 }; c.deess = { true, 6500, -30 };
              c.harm = { true, harmonitzador::Teclat, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { -50, -15, 15, 50 }, 0, 100, 0, 0, 0 };
              c.rev = { true, Plate, 2.6f, 30, 22, 8000 }; afegeix (c); }
            { Conf c { "Harmonies", "Fleet Foxes · White Winter Hymnal", "Cor de tres veus dins de la tonalitat (3a i 5a amunt i 4a avall) i una sala gran." };
              c.eq = { 110, 300, -2, 3000, 1, 2 }; c.comp = { true, -22, 3, 10, 150, 3 }; c.deess = { true, 6500, -30 };
              c.harm = { true, harmonitzador::Tonalitat, { 2, 4, -3, 0 }, { -6, -8, -9, -8 }, { -50, 50, 0, 70 }, 100, 75, 0, 40, 30 };
              c.rev = { true, Hall, 3.0f, 40, 28, 6000 }; afegeix (c); }
            { Conf c { "Harmonies", "Harmonies automàtiques (3a i 5a)", "Dues veus per sobre dins de la tonalitat de la cançó, als costats i una mica enrere." };
              c.eq = { 100, 280, -2, 3500, 1.5f, 2 }; c.comp = { true, -20, 4, 5, 120, 3 }; c.deess = { true, 6500, -28 };
              c.harm = { true, harmonitzador::Tonalitat, { 2, 4, 0, 0 }, { -8, -10, -8, -8 }, { -45, 45, -70, 70 }, 100, 70, 0, 30, 20 };
              c.rev = { true, Plate, 1.5f, 30, 15, 8000 }; afegeix (c); }
            { Conf c { "Harmonies", "Segona veu amb els acords", "Una veu per sobre que segueix la pista d'Acords (la nota de l'acord més propera per sobre)." };
              c.eq = { 100, 280, -2, 3500, 1.5f, 2 }; c.comp = { true, -20, 4, 5, 120, 3 }; c.deess = { true, 6500, -28 };
              c.harm = { true, harmonitzador::Acords, { 1, 0, 0, 0 }, { -6, -8, -8, -8 }, { -25, 25, -70, 70 }, 100, 75, 0, 30, 25 };
              c.dly = { true, true, 8, 120, 20, 8, 300, 4500 }; c.rev = { true, Plate, 1.4f, 30, 14, 8000 }; afegeix (c); }
            // ---- Efectes ----
            { Conf c { "Efectes", "Octava avall", "La mateixa veu una octava més greu, barrejada a sota: més cos i més fosca." };
              c.eq = { 80, 250, -1, 3500, 1.5f, 1 }; c.comp = { true, -20, 4, 5, 120, 3 };
              c.oct = { true, -12, 1, 35 }; c.rev = { true, Room, 0.9f, 15, 10, 7000 }; afegeix (c); }
            { Conf c { "Efectes", "Prince · If I Was Your Girlfriend", "La veu accelerada (més aguda i més prima), com «Camille»." };
              c.eq = { 150, 300, -1, 3500, 2, 2 }; c.comp = { true, -22, 4, 3, 100, 4 };
              c.oct = { true, 5, 3, 100 }; c.rev = { true, Plate, 1.0f, 20, 10, 8000 }; afegeix (c); }
            { Conf c { "Efectes", "Veu de gegant", "Més greu i amb el color d'una persona molt més gran." };
              c.eq = { 60, 200, 1, 3000, 0, -2 }; c.comp = { true, -22, 4, 5, 120, 3 };
              c.oct = { true, -5, -3, 100 }; c.rev = { true, Hall, 2.0f, 30, 15, 5000 }; afegeix (c); }
            { Conf c { "Efectes", "Megàfon", "Veu de telèfon o de megàfon: només els mitjos i trencada." };
              c.eq = { 500, 1200, 3, 2500, 2, -6, 3000 }; c.comp = { true, -24, 6, 3, 80, 6 };
              c.drv = { true, drive::Crunch, 6, 6, 70 }; afegeix (c); }
            { Conf c { "Efectes", "Veu distorsionada", "Distorsió de pedal i eco ample, per a tornades fortes o ponts estranys." };
              c.eq = { 150, 300, -2, 2500, 3, 0 }; c.comp = { true, -24, 6, 2, 80, 4 };
              c.drv = { true, drive::Distorsio, 6, 5, 60 }; c.dly = { true, true, 7, 120, 35, 20, 400, 4000, delay::PingPong };
              c.rev = { true, Room, 1.0f, 20, 10, 6000 }; afegeix (c); }
            return v;
        }
    }

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> p = []
        {
            std::vector<Preset> v;
            for (const auto& c : taula()) v.push_back ({ c.nom, c.cat, c.desc, fes (c) });
            return v;
        }();
        return p;
    }

    const std::string& estatInicial()
    {
        static const std::string s = presetsFabrica().front().estat;
        return s;
    }
}
