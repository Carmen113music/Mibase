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
        static const std::vector<std::string> c { "Bàsics", "Íntim", "Pop", "Estadi", "Urbà", "Rock i indie", "Harmonies", "Efectes" };
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
        /** 0.76: un retorn en paral·lel amb els seus mòduls (en aquest ordre: EQ, compressor, saturació, drive, octava,
            harmonitzador, doubler, delay, reverb). Els efectes d'un retorn sonen al 100 % (només el seu so). */
        struct ConfRetorn
        {
            const char* nom = "";
            float envia = 0, retorn = -12, pan = 0, duck = 0;
            bool eqOn = false; ConfEq eq;
            ConfComp comp; ConfSat sat; ConfDrive drv; ConfOct oct; ConfHarm harm; ConfDbl dbl; ConfDelay dly; ConfRev rev;
        };
        struct Conf
        {
            Conf (const char* c, const char* n, const char* d) : cat (c), nom (n), desc (d) {}
            std::vector<ConfRetorn> rets;   // 0.76
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
            int camiActual = 0;                                    // 0.76: on van els mòduls que s'afegeixen
            int retRev = -1, retDly = -1, retDbl = -1;             // 0.76: quin retorn és la reverb, el delay, els dobles
            static constexpr int rackP = -1000;                    // 0.76: «mòdul» d'un E que és un paràmetre del rack

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
                return r.afegeix ("eq", q, -1, camiActual);
            }
            int comp (const ConfComp& c)
            {
                auto d = std::make_shared<dinamica::Dinamica>();
                using namespace dinamica;
                d->posa (ModeP, Compressor); d->posa (Llindar, c.thr); d->posa (Ratio, c.ratio); d->posa (Atac, c.atac);
                d->posa (Alliberament, c.rel); d->posa (Guany, c.makeup); d->posa (Mix, c.mix);
                return r.afegeix ("compressor", d, -1, camiActual);
            }
            int deess (const ConfDeEss& c)
            {
                auto d = std::make_shared<dinamica::Dinamica>();
                using namespace dinamica;
                d->posa (ModeP, DeEsser); d->posa (FreqDeEsser, c.freq); d->posa (Llindar, c.thr); d->posa (Ratio, 4); d->posa (Atac, 1); d->posa (Alliberament, 60);
                return r.afegeix ("deesser", d, -1, camiActual);
            }
            int sat (const ConfSat& c)
            {
                auto s = std::make_shared<saturacio::Saturacio>();
                s->posa (saturacio::EstilP, (float) c.estil); s->posa (saturacio::Drive, c.drive); s->posa (saturacio::Mix, c.mix); s->posa (saturacio::Tone, c.tone);
                return r.afegeix ("decapitor", s, -1, camiActual);
            }
            int drv (const ConfDrive& c)
            {
                auto d = std::make_shared<drive::Drive>();
                d->posa (drive::ModeP, (float) c.mode); d->posa (drive::Gain, c.gain); d->posa (drive::Tone, c.tone); d->posa (drive::Mix, c.mix);
                return r.afegeix ("drive", d, -1, camiActual);
            }
            int dbl (const ConfDbl& c)
            {
                auto d = std::make_shared<doubler::Doubler>();
                d->posa (doubler::Quantitat, c.quant); d->posa (doubler::Desafina, c.cents); d->posa (doubler::Retard, c.retard);
                if (camiActual > 0) d->posa (doubler::Veu, 0);   // 0.76: en un retorn, només les còpies
                return r.afegeix ("doubler", d, -1, camiActual);
            }
            int harm (const ConfHarm& c)
            {
                auto h = std::make_shared<harmonitzador::Harmonitzador>();
                using namespace harmonitzador;
                h->posa (ModeP, (float) c.mode); h->posa (Sec, c.sec); h->posa (Harmonies, c.harm); h->posa (Formant, c.formant);
                h->posa (Humanitza, c.hum); h->posa (Lliscament, c.glide);
                for (int v = 0; v < 4; ++v) { h->posa (Interval1 + v, (float) c.v[v]); h->posa (Nivell1 + v, c.niv[v]); h->posa (Pan1 + v, c.pan[v]); }
                return r.afegeix ("harmonitzador", h, -1, camiActual);
            }
            int oct (const ConfOct& c)
            {
                auto o = std::make_shared<octava::Octava>();
                o->posa (octava::To, c.to); o->posa (octava::Formant, c.formant); o->posa (octava::Mescla, c.mescla);
                return r.afegeix ("octava", o, -1, camiActual);
            }
            int dly (const ConfDelay& c)
            {
                auto d = std::make_shared<delay::Delay>();
                using namespace delay;
                d->posa (Sync, c.sync ? 1.0f : 0.0f); d->posa (Nota, (float) c.nota); d->posa (Temps, c.ms); d->posa (Feedback, c.fb);
                d->posa (Mix, c.mix); d->posa (LowCut, c.lc); d->posa (HighCut, c.hc); d->posa (EncaminamentP, (float) c.enc);
                return r.afegeix ("delay", d, -1, camiActual);
            }
            int rev (const ConfRev& c)
            {
                auto v = std::make_shared<reverb::Reverb>();
                using namespace reverb;
                v->posa (Alg, (float) c.alg); v->posa (Decay, c.decay); v->posa (PreDelay, c.pre); v->posa (Mix, c.mix); v->posa (HighCut, c.hc);
                return r.afegeix ("reverb", v, -1, camiActual);
            }
            int lim (const ConfLim& c)
            {
                auto l = std::make_shared<limitador::Limitador>();
                l->posa (limitador::Gain, c.gain); l->posa (limitador::Ceiling, c.ceil);
                return r.afegeix ("limitador", l, -1, camiActual);
            }

            struct E { int modul, param; float mn, mx; };   // en unitats del paràmetre (dB, ms, %)
            float normal (int modul, int p, float v)
            {
                if (modul == rackP) { float mn, mx, def; rangParam (p, mn, mx, def); return std::clamp ((v - mn) / (mx - mn), 0.0f, 1.0f); }
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
                    if (e.modul < 0 && e.modul != rackP) continue;
                    const float a = normal (e.modul, e.param, e.mn), b = normal (e.modul, e.param, e.mx);
                    if (primer)
                    {
                        const float ara = e.modul == rackP ? r.parametre (e.param) : r.modul (e.modul)->fx->parametre (e.param);
                        const float v = std::abs (b - a) > 1e-6f ? std::clamp ((ara - a) / (b - a), 0.0f, 1.0f) : 0.5f;
                        r.posaMacro (m, v);   // encara sense enllaços: no mou res
                        primer = false;
                    }
                    r.enllaca ({ m, e.modul == rackP ? -1 : e.modul, e.param, a, b });
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
            // 0.76: retorns
            for (int q = 0; q < (int) c.rets.size() && q < maxRetorns; ++q)
            {
                const auto& t = c.rets[(size_t) q];
                k.camiActual = q + 1;
                k.r.activaRetorn (q, true, t.nom);
                k.r.posaValor (Envia1 + q, t.envia); k.r.posaValor (Retorn1 + q, t.retorn);
                k.r.posaValor (PanRetorn1 + q, t.pan); k.r.posaValor (Ducking1 + q, t.duck);
                if (t.eqOn) k.eq (t.eq);
                if (t.comp.on) k.comp (t.comp);
                if (t.sat.on) { auto s = t.sat; s.mix = std::max (s.mix, 100.0f); k.sat (s); }
                if (t.drv.on) { auto d = t.drv; d.mix = 100; k.drv (d); }
                if (t.oct.on) { auto o = t.oct; o.mescla = 100; k.oct (o); }
                if (t.harm.on) { auto h = t.harm; h.sec = 0; k.harm (h); }
                if (t.dbl.on) { auto d = t.dbl; d.quant = std::max (d.quant, 100.0f); k.dbl (d); if (k.retDbl < 0) k.retDbl = q; }
                if (t.dly.on) { auto d = t.dly; d.mix = 100; k.dly (d); if (k.retDly < 0) k.retDly = q; }
                if (t.rev.on) { auto v = t.rev; v.mix = 100; k.rev (v); if (k.retRev < 0) k.retRev = q; }
            }
            k.camiActual = 0;

            const int bandaCos = eq::indexParam (1, eq::PGuany), bandaPres = eq::indexParam (2, eq::PGuany), bandaAire = eq::indexParam (3, eq::PGuany);
            k.macro (0, { { k.iEq, bandaCos, -5.0f, 5.0f } });
            k.macro (1, { { k.iEq, bandaPres, -4.0f, 6.0f } });
            k.macro (2, { { k.iEq, bandaAire, -4.0f, 8.0f }, { k.iDeEss, dinamica::Llindar, -22.0f, -36.0f } });
            k.macro (3, { { k.iComp, dinamica::Llindar, -6.0f, -34.0f }, { k.iComp, dinamica::Guany, 0.0f, 9.0f } });
            if (k.iSat >= 0) k.macro (4, { { k.iSat, saturacio::Drive, 0.0f, 7.0f }, { k.iSat, saturacio::Sortida, 0.0f, -4.0f } });
            else if (k.iDrive >= 0) k.macro (4, { { k.iDrive, drive::Gain, 0.0f, 9.0f }, { k.iDrive, drive::Level, 0.0f, -6.0f } });
            // 0.73: el macro 6 és el Doble; si el preset no en té i té harmonies o octava, les mou (i se'n diu així)
            if (k.retDbl >= 0) k.macro (5, { { Constructor::rackP, Retorn1 + k.retDbl, -40.0f, 0.0f } });   // 0.76
            else if (k.iDbl >= 0) k.macro (5, { { k.iDbl, doubler::Quantitat, 0.0f, 100.0f } });
            else if (k.iHarm >= 0) { k.macro (5, { { k.iHarm, harmonitzador::Harmonies, 0.0f, 100.0f } }); k.r.posaNomMacro (5, "Harmonies"); }
            else if (k.iOct >= 0) { k.macro (5, { { k.iOct, octava::Mescla, 0.0f, 100.0f } }); k.r.posaNomMacro (5, "Octava"); }
            if (k.retRev >= 0) k.macro (6, { { Constructor::rackP, Retorn1 + k.retRev, -40.0f, 0.0f } });   // 0.76: el volum del retorn
            else k.macro (6, { { k.iRev, reverb::Mix, 0.0f, 40.0f }, { k.iRev, reverb::PreDelay, 15.0f, 80.0f } });
            if (k.retDly >= 0) k.macro (7, { { Constructor::rackP, Retorn1 + k.retDly, -40.0f, 0.0f } });
            else k.macro (7, { { k.iDly, delay::Mix, 0.0f, 45.0f }, { k.iDly, delay::Feedback, 10.0f, 40.0f } });
            return k.r.estat();
        }

        std::vector<Conf> taula()
        {
            std::vector<Conf> v;
            // 0.76: si ja n'hi ha un amb el mateix nom, el nou (de la guia) el substitueix al mateix lloc
            auto afegeix = [&v] (Conf c)
            {
                for (auto& x : v) if (std::string (x.nom) == c.nom) { x = c; return; }
                v.push_back (c);
            };
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
              c.dbl = { true, 75, 12, 24 }; c.rev = { true, Hall, 2.2f, 40, 25, 7000 }; afegeix (c); }
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
              c.dbl = { true, 55, 8, 26 }; c.rev = { true, Chamber, 3.2f, 80, 22, 5000 }; afegeix (c); }
            { Conf c { "Íntim", "Frank Ocean · Ivy", "Cinta saturada, aguts apagats i un racó petit." };
              c.satAbans = true; c.sat = { true, saturacio::A, 3, 70, -3 };
              c.eq = { 80, 1500, 2, 3500, 0, -3 }; c.comp = { true, -20, 3, 15, 200, 3 };
              c.rev = { true, Chamber, 1.1f, 25, 14, 6000 }; afegeix (c); }
            // ---- Pop ----
            { Conf c { "Pop", "Dua Lipa · Don't Start Now", "Polida i brillant, amb doblat subtil, plate i eco a corxeres." };
              c.deessAbans = { true, 6000, -26 };
              c.eq = { 100, 280, -3, 5000, 2, 3 }; c.comp = { true, -22, 4, 3, 80, 3 }; c.comp2 = { true, -18, 2.5f, 20, 250, 2 };
              c.deess = { true, 7000, -30 }; c.dbl = { true, 40, 10, 20 };
              c.dly = { true, true, 5, 120, 20, 10, 400, 5000 }; c.rev = { true, Plate, 1.4f, 30, 14, 9000 }; afegeix (c); }
            { Conf c { "Pop", "Dua Lipa · Houdini", "Consola, una mica de doble i eco ping-pong a negres, filtrat." };
              c.eq = { 95, 250, 0, 2500, 3, 1 }; c.comp = { true, -22, 4, 2, 100, 4 };
              c.deess = { true, 6500, -28 }; c.dbl = { true, 45, 12, 22 };
              c.dly = { true, true, 8, 120, 25, 16, 500, 3500, delay::PingPong }; c.rev = { true, Plate, 1.2f, 25, 10, 8000 }; afegeix (c); }
            { Conf c { "Pop", "Coldplay · Fix You", "Gran i emotiva: preamp britànic, eco a negres i una sala llarga." };
              c.satAbans = true; c.sat = { true, saturacio::E, 3, 40, 0 };
              c.eq = { 80, 200, 2, 3500, 0, 2.5f }; c.comp = { true, -20, 3, 20, 200, 3 }; c.deess = { true, 6500, -28 };
              c.dbl = { true, 35, 9, 20 }; c.dly = { true, true, 8, 120, 25, 15, 300, 4000 };
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
              c.dbl = { true, 50, 14, 24 }; c.rev = { true, Hall, 4.0f, 40, 25, 6000 }; afegeix (c); }
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
            // ==== 0.76: les 30 cadenes de la «Guia de cadenes vocals modernes» (d'en Roger) ====
            // Cadena principal = INSERT; retorns = busos SEND / paral·lel i doblatge / harmonies.
            // Encara no hi ha Tune, vocoder, chorus/phaser ni compressors amb caràcter: es fa amb el més semblant.
            {
                auto fet = [] (float gr, float ratio = 4.0f) { return ConfComp { true, -14.0f - 1.5f * gr, ratio, 2, 60, gr * 0.6f, 100 }; };
                auto opt = [] (float gr) { return ConfComp { true, -14.0f - 1.5f * gr, 3, 12, 280, gr * 0.6f, 100 }; };
                auto vmu = [] (float gr) { return ConfComp { true, -14.0f - 1.6f * gr, 2.5f, 18, 320, gr * 0.6f, 100 }; };
                auto vca = [] (float gr, float ratio = 4.0f) { return ConfComp { true, -14.0f - 1.5f * gr, ratio, 5, 120, gr * 0.6f, 100 }; };
                auto filtre = [] (ConfRetorn& t, float hp, float lp) { t.eqOn = true; t.eq = { hp, 300, 0, 3000, 0, 0, lp }; };
                auto rv = [&filtre] (const char* nom, int alg, float decay, float pre, float retorn, float hp = 150, float lp = 0)
                {
                    ConfRetorn t; t.nom = nom; t.retorn = retorn; t.rev = { true, alg, decay, pre, 100, lp > 0 ? lp : 9000 };
                    if (hp > 0 || lp > 0) filtre (t, hp > 0 ? hp : 20, lp);
                    return t;
                };
                auto dlNota = [&filtre] (const char* nom, int nota, float fb, float retorn, float hp, float lp, float duck = 0, int enc = delay::Estereo)
                {
                    ConfRetorn t; t.nom = nom; t.retorn = retorn; t.duck = duck;
                    t.dly = { true, true, nota, 120, fb, 100, hp, lp, enc };
                    return t;
                };
                auto slap = [] (const char* nom, float ms, float retorn, float lp = 5000, int enc = delay::Mono, float hp = 200)
                {
                    ConfRetorn t; t.nom = nom; t.retorn = retorn; t.dly = { true, false, 8, ms, 0, 100, hp, lp, enc };
                    return t;
                };
                auto dobles = [&filtre] (const char* nom, float retorn, float cents, float retard, float hp = 150, float lp = 0)
                {
                    ConfRetorn t; t.nom = nom; t.retorn = retorn; t.dbl = { true, 100, cents, retard };
                    t.comp = { true, -26, 4, 3, 80, 4, 100 };
                    filtre (t, hp, lp);
                    return t;
                };
                auto harmonies = [&filtre] (const char* nom, float retorn, int mode, std::initializer_list<int> iv, std::initializer_list<float> pans, float hp = 150, float lp = 0)
                {
                    ConfRetorn t; t.nom = nom; t.retorn = retorn;
                    t.harm.on = true; t.harm.mode = mode; t.harm.harm = 100; t.harm.hum = 40; t.harm.glide = 25;
                    int k = 0; for (int x : iv) if (k < 4) t.harm.v[k++] = x;
                    for (; k < 4; ++k) t.harm.v[k] = 0;
                    k = 0; for (float x : pans) if (k < 4) t.harm.pan[k++] = x;
                    for (int q = 0; q < 4; ++q) t.harm.niv[q] = -4;
                    t.comp = { true, -26, 4, 3, 80, 4, 100 };
                    filtre (t, hp, lp);
                    return t;
                };
                auto octava = [] (const char* nom, float to, float retorn, float formant = 0, float pan = 0)
                {
                    ConfRetorn t; t.nom = nom; t.retorn = retorn; t.pan = pan; t.oct = { true, to, formant, 100 };
                    return t;
                };
                auto paral = [] (float retorn)
                {
                    ConfRetorn t; t.nom = "Comp. paral·lela"; t.retorn = retorn; t.comp = { true, -40, 20, 0.5f, 50, 14, 100 };
                    return t;
                };

                // ---- 1. Intimitat ASMR, minimalisme i textura seca ----
                { Conf c { "Íntim", "Billie Eilish · When the Party's Over", "Molt a prop i gairebé seca: compressió lenta, aire, una sala curtíssima, un micro-desafinat i un cor de veus fluixes als costats." };
                  c.eq = { 80, 250, -2, 2500, -1.5f, 2 }; c.comp = vmu (3.5f); c.deess = { true, 6500, -30 };
                  c.rets = { rv ("Sala", Room, 0.35f, 10, -28), dobles ("Micro-desafinat", -20, 6, 12, 150, 6000),
                             harmonies ("Cor", -16, harmonitzador::Tonalitat, { 2, 4, 6, -3 }, { -60, 60, -100, 100 }) };
                  afegeix (c); }
                { Conf c { "Íntim", "Billie Eilish · Bad Guy", "Seca i a primer pla, amb una mica de cinta. Un slap mono per als finals de frase i dobles xiuxiuejats als costats." };
                  c.eq = { 90, 400, -3, 3500, 0, 1 }; c.comp = vmu (3); c.deess = { true, 6557, -30 };
                  c.sat = { true, saturacio::A, 1.5f, 10, 0 };
                  c.rets = { slap ("Slap 95 ms", 95, -24), dobles ("Dobles xiuxiuejats", -16, 8, 18, 200, 0) };
                  afegeix (c); }
                { Conf c { "Íntim", "Frank Ocean · White Ferrari", "Càlida i suau: òptic, cinta, una plate fosca, un chorus molt subtil i harmonies del teclat MIDI amb el formant pujat. (Sense Tune encara.)" };
                  c.eq = { 100, 180, 1.5f, 3500, 0, 1 }; c.comp = opt (4); c.sat = { true, saturacio::A, 1.5f, 30, 0 };
                  c.rets = { rv ("Plate fosca", Plate, 1.6f, 25, -18, 150, 4500), dobles ("Chorus", -18, 6, 15),
                             harmonies ("Harmonies (teclat)", -9, harmonitzador::Teclat, { 0 }, { -50, -15, 15, 50 }) };
                  c.rets.back().harm.formant = 1;
                  afegeix (c); }
                { Conf c { "Íntim", "Olivia Rodrigo · Vampire", "Dos compressors (ràpid i suau) i vàlvula. Chamber, delay a negres amb ducking, dobles a l'uníson i una octava greu al mig." };
                  c.eq = { 85, 320, -2, 3500, 1, 1.5f }; c.comp = fet (5); c.comp2 = opt (3); c.sat = { true, saturacio::T, 2.5f, 45, 0 };
                  c.rets = { rv ("Chamber", Chamber, 1.4f, 35, -18), dlNota ("Delay 1/4", 8, 25, -20, 300, 3000, 8),
                             dobles ("Dobles", -12, 10, 20), octava ("Octava greu", -12, -14) };
                  afegeix (c); }
                { Conf c { "Íntim", "Olivia Rodrigo · Drivers License", "Cos de pit i suau, amb brillantor a dalt. Una hall càlida, un slap de cinta i un cor de 3es amb saturació." };
                  c.eq = { 75, 250, 0, 3500, 0, 2.5f }; c.comp = opt (3.5f); c.deess = { true, 7000, -28 };
                  auto cor = harmonies ("Cor (3es)", -12, harmonitzador::Tonalitat, { 2, 2, 0 }, { -50, 50 });
                  cor.harm.hum = 70; cor.sat = { true, saturacio::A, 2, 100, 0 };
                  c.rets = { rv ("Hall càlida", Hall, 2.1f, 45, -16, 250), slap ("Slap de cinta", 110, -20), cor };
                  c.rets[1].dly.enc = delay::Mono;
                  afegeix (c); }

                // ---- 2. Harmonització sintètica, vocoders i processament experimental ----
                { Conf c { "Harmonies", "Bon Iver · 715 - CRΣΣKS", "Totes les veus surten del teclat MIDI (toca els acords a l'AKAI mentre cantes), comprimides fort i gairebé seques, amb una sala curta que les enganxa. (Sense Tune encara.)" };
                  c.eq = { 110, 300, 0, 1500, 2, 0 }; c.comp = fet (6, 8);
                  c.harm = { true, harmonitzador::Teclat, { 0, 0, 0, 0 }, { 0, 0, -1, -1 }, { -60, -20, 20, 60 }, 0, 100, 0, 10, 0 };
                  c.rets = { rv ("Sala curta", Room, 0.4f, 10, -22) };
                  afegeix (c); }
                { Conf c { "Harmonies", "Bon Iver · Woods", "Òptic i cinta, una plate mitjana i un cor de veus dins de la tonalitat que s'obren cap als costats. (Sense Tune encara.)" };
                  c.eq = { 90, 500, -2, 3500, 0, 1 }; c.comp = opt (5); c.sat = { true, saturacio::A, 2.5f, 25, 0 };
                  c.rets = { rv ("Plate", Plate, 1.8f, 30, -16, 150, 7000),
                             harmonies ("Cor", -10, harmonitzador::Tonalitat, { 2, 4, -3, 7 }, { -30, 30, -80, 80 }) };
                  afegeix (c); }
                { Conf c { "Harmonies", "Rosalía · Saoko", "Seca i al mig, amb presència i un tallador de pics. Ecos curts una octava amunt, una sala metàl·lica i respostes amb el formant canviat a un costat. (Sense Tune encara.)" };
                  c.eq = { 95, 300, 0, 3500, 3, 1 }; c.comp = { true, -20, 4, 5, 80, 3, 100 }; c.lim = { true, 2, -1 };
                  auto oct = octava ("Ecos d'octava", 12, -20); oct.dly = { true, true, 2, 120, 20, 100, 300, 6000, delay::PingPong };
                  c.rets = { oct, rv ("Sala metàl·lica", Room, 0.6f, 5, -24, 300, 0), octava ("Respostes", 0, -18, -4, -100) };
                  afegeix (c); }
                { Conf c { "Harmonies", "Imogen Heap · Hide and Seek", "Un bloc d'acords de la teva veu tocat amb el teclat MIDI, sense la veu original, i una hall espaiosa ben oberta. (El vocoder de debò arribarà més endavant.)" };
                  c.eq = { 120, 300, -1, 4000, 3, 2 }; c.comp = { true, -24, 4, 5, 120, 4, 100 }; c.deess = { true, 6500, -30 };
                  c.harm = { true, harmonitzador::Teclat, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { -50, -15, 15, 50 }, 0, 100, 0, 0, 0 };
                  c.rets = { rv ("Hall", Hall, 2.5f, 40, -14) };
                  afegeix (c); }
                { Conf c { "Harmonies", "James Blake · Retrograde", "Pes i calidesa: Vari-Mu i saturació suau. Una plate fosca, un delay de negra amb puntet filtrat i una octava avall a sota." };
                  c.eq = { 70, 220, 2, 3500, 0, 0 }; c.comp = vmu (4); c.sat = { true, saturacio::T, 1.5f, 35, 0 };
                  c.rets = { rv ("Plate fosca", Plate, 2.2f, 30, -16, 150, 3500), dlNota ("Delay 1/4·", 10, 30, -20, 400, 2000),
                             octava ("Octava avall", -12, -14) };
                  afegeix (c); }

                // ---- 3. Pop modern brillant, nu-disco i mainstream ----
                { Conf c { "Pop", "Dua Lipa · Don't Start Now", "Brillant i al davant: ràpid + suau, de-esser i aire. Compressió paral·lela aixafada, plate brillant, delay de corxera amb molt ducking i dobles oberts." };
                  c.eq = { 100, 350, -3, 5000, 1.5f, 3 }; c.comp = fet (5); c.comp2 = opt (3); c.deess = { true, 6500, -28 };
                  c.rets = { paral (-18), rv ("Plate brillant", Plate, 1.3f, 40, -16, 200), dlNota ("Delay 1/8", 5, 20, -18, 300, 4500, 12),
                             dobles ("Dobles", -12, 10, 20, 180) };
                  afegeix (c); }
                { Conf c { "Pop", "Dua Lipa · Levitating", "Presència i aire, compressió doble. Un doubler estèreo, un slap de 85 ms per al groove i cors en 3es i 5es." };
                  c.eq = { 105, 300, 0, 3000, 2.5f, 3 }; c.comp = vca (4); c.comp2 = opt (3);
                  c.rets = { dobles ("Doubler estèreo", -10, 9, 15), slap ("Slap 85 ms", 85, -18),
                             harmonies ("Cors", -14, harmonitzador::Tonalitat, { 2, 4 }, { -60, 60 }) };
                  afegeix (c); }
                { Conf c { "Pop", "The Weeknd · Blinding Lights", "Anys 80: presència, compressió ràpida i cinta. Plate llarga, delay de negres amb ducking i un doble a l'uníson molt comprimit. (Sense Tune encara.)" };
                  c.eq = { 95, 300, 0, 4500, 2, 1.5f }; c.comp = fet (5); c.sat = { true, saturacio::A, 2, 40, 0 };
                  auto d = dobles ("Doble", -10, 8, 18); d.comp = { true, -32, 8, 2, 60, 8, 100 };
                  c.rets = { rv ("Plate 80s", Plate, 2.2f, 55, -16, 300), dlNota ("Delay 1/4", 8, 30, -18, 300, 5000, 8), d };
                  afegeix (c); }
                { Conf c { "Pop", "Sia · Chandelier", "Per als crits: compressió forta, òptic i un punt de crunch. Hall gran, delay de blanca, quatre dobles i una octava greu." };
                  c.eq = { 90, 300, 0, 1200, -3, 1.5f }; c.comp = fet (8, 8); c.comp2 = opt (3); c.sat = { true, saturacio::P, 1.5f, 20, 2 };
                  c.rets = { rv ("Hall gran", Hall, 2.6f, 60, -14), dlNota ("Delay 1/2", 11, 30, -20, 300, 4000, 6),
                             dobles ("Dobles", -8, 14, 26), octava ("Octava greu", -12, -16) };
                  afegeix (c); }
                { Conf c { "Pop", "Sabrina Carpenter · Espresso", "Molt d'aire, compressió ràpida i doble de-esser. Una plate curta i un micro-desafinat que obre el timbre." };
                  c.eq = { 110, 300, 0, 3500, 1, 3.5f }; c.comp = fet (4); c.deessAbans = { true, 9000, -32 }; c.deess = { true, 6000, -28 };
                  c.sat = { true, saturacio::A, 1.5f, 25, 1 };
                  c.rets = { rv ("Plate curta", Plate, 0.9f, 25, -16), dobles ("Micro-desafinat", -14, 8, 12) };
                  c.rets[1].dbl.cents = 8;
                  afegeix (c); }

                // ---- 4. Rock d'estadi, pop èpic i himnes ----
                { Conf c { "Estadi", "Coldplay · Fix You", "Himne: òptic i VCA per als crescendos, cinta. Hall d'estadi, delay de negres filtrat i un uníson de grup (molt variat)." };
                  c.eq = { 85, 300, 0, 2200, 2, 1 }; c.comp = opt (4); c.comp2 = vca (3, 3); c.sat = { true, saturacio::A, 1.5f, 30, 0 };
                  auto grup = dobles ("Grup", -12, 15, 30, 200); grup.dbl.cents = 15;
                  c.rets = { rv ("Hall d'estadi", Hall, 2.8f, 55, -14, 250), dlNota ("Delay 1/4", 8, 25, -18, 400, 3500), grup };
                  afegeix (c); }
                { Conf c { "Estadi", "Imagine Dragons · Believer", "Agressiva: compressió de 8:1 i Decapitor fort. Slap distorsionat, sala metàl·lica, crits doblats i una octava greu a sota." };
                  c.eq = { 100, 300, 0, 3500, 3, 1 }; c.comp = fet (7, 8); c.sat = { true, saturacio::P, 5, 50, 1 };
                  auto s = slap ("Slap distorsionat", 90, -16); s.drv = { true, drive::Distorsio, 5, 5, 100 };
                  auto sala = rv ("Sala", Room, 0.8f, 10, -20); sala.comp = { true, -34, 10, 1, 60, 10, 100 };
                  c.rets = { s, sala, dobles ("Crits doblats", -10, 12, 18), octava ("Octava greu", -12, -14) };
                  afegeix (c); }
                { Conf c { "Estadi", "Harry Styles · As It Was", "Vintage: ràpid, cinta i de-esser. Una molla curta, un slap de cinta estèreo i un doble quasi constant a -6 dB." };
                  c.eq = { 95, 400, -2, 3500, 0, 1 }; c.comp = fet (4); c.sat = { true, saturacio::A, 2, 40, 0 }; c.deess = { true, 7000, -28 };
                  auto d = dobles ("Doble", -6, 9, 20); d.pan = 0;
                  c.rets = { rv ("Molla", Spring, 1.1f, 20, -18), slap ("Slap de cinta", 105, -20, 5000, delay::Estereo), d };
                  afegeix (c); }
                { Conf c { "Estadi", "Adele · Rolling in the Deep", "La veu sola al mig: òptic i un 8:1 només per als pics. Chamber, un slap molt subtil i un cor gospel de 3es i 5es amb la seva pròpia reverb llarga." };
                  c.eq = { 75, 280, -2, 3500, 0.5f, 1 }; c.comp = opt (5); c.comp2 = { true, -8, 8, 1, 50, 0, 100 };
                  auto gospel = harmonies ("Cor gospel", -12, harmonitzador::Tonalitat, { 2, 4 }, { -60, 60 });
                  gospel.rev = { true, Hall, 2.5f, 40, 30, 7000 };
                  c.rets = { rv ("Chamber", Chamber, 1.9f, 45, -16), slap ("Slap 75 ms", 75, -26), gospel };
                  afegeix (c); }
                { Conf c { "Estadi", "Muse · Supermassive Black Hole", "Falset tallant: 8:1 i un overdrive subtil. Slap ben present al mig, sala curta, dobles i una 5a per sobre." };
                  c.eq = { 120, 300, 0, 2500, 3, 1 }; c.comp = fet (8, 8); c.drv = { true, drive::Overdrive, 2, 5, 25 };
                  c.rets = { slap ("Slap 115 ms", 115, -12), rv ("Sala curta", Room, 0.7f, 5, -22),
                             dobles ("Dobles", -10, 8, 15), harmonies ("5a amunt", -12, harmonitzador::Tonalitat, { 4 }, { 0 }) };
                  afegeix (c); }

                // ---- 5. Indie-rock, synth-pop brillant i psicodèlia moderna ----
                { Conf c { "Rock i indie", "Two Door Cinema Club · What You Know", "Molt neta per deixar lloc a les guitarres, compressió ràpida i de-esser fort. Slap estèreo, plate brillant, dobles ajustats i un cor una octava amunt." };
                  c.eq = { 115, 300, 0, 4000, 3, 1.5f }; c.comp = fet (6); c.deess = { true, 6500, -34 };
                  c.rets = { slap ("Slap 80 ms", 80, -18, 5000, delay::Estereo), rv ("Plate brillant", Plate, 1.2f, 30, -18, 350),
                             dobles ("Dobles", -10, 8, 14), octava ("Octava amunt", 12, -16) };
                  afegeix (c); }
                { Conf c { "Rock i indie", "Tame Impala · The Less I Know the Better", "Vintage: aguts tallats, VCA i cinta. Molla, delay de cinta de corxera i el doble «Lennon» amb l'octava aguda, molt comprimit. (Sense phaser encara.)" };
                  c.eq = { 100, 300, 0, 3500, 0, 0, 13000 }; c.comp = vca (6); c.sat = { true, saturacio::A, 3, 50, 0 };
                  auto dly = dlNota ("Delay de cinta 1/8", 5, 30, -18, 300, 4000);
                  auto lennon = dobles ("Doble i octava", -10, 10, 18); lennon.oct = { true, 12, 0, 50 }; lennon.comp = { true, -32, 8, 2, 60, 8, 100 };
                  c.rets = { rv ("Molla", Spring, 1.5f, 15, -18), dly, lennon };
                  afegeix (c); }
                { Conf c { "Rock i indie", "The 1975 · Somebody Else", "Aire i compressió doble. Plate llarga, un chorus en paral·lel i harmonies de 3es i 6es filtrades amb la seva reverb. (Sense Tune encara.)" };
                  c.eq = { 90, 300, 0, 3500, 0, 2.5f }; c.comp = fet (4); c.comp2 = opt (3);
                  auto h = harmonies ("Harmonies (3es i 6es)", -12, harmonitzador::Tonalitat, { 2, 5 }, { -50, 50 }, 250, 8000);
                  h.rev = { true, Plate, 2.4f, 50, 30, 8000 };
                  c.rets = { rv ("Plate 80s", Plate, 2.4f, 50, -16), dobles ("Chorus", -14, 8, 10), h };
                  afegeix (c); }
                { Conf c { "Rock i indie", "Phoenix · 1901", "La veu al davant i seca: presència, compressió ràpida i consola. Sala curta, slap mono present i dobles amb els aguts tallats." };
                  c.eq = { 110, 300, 0, 3000, 3, 1 }; c.comp = fet (5); c.sat = { true, saturacio::N, 2.5f, 45, 0 };
                  c.rets = { rv ("Sala curta", Room, 0.5f, 5, -22), slap ("Slap 95 ms", 95, -14), dobles ("Dobles", -10, 6, 12, 150, 6000) };
                  afegeix (c); }
                { Conf c { "Rock i indie", "Arctic Monkeys · Do I Wanna Know?", "Crua: cos i presència, compressió lenta i saturació. Slap de cinta fosc, molla mono i cors aguts en falset als costats." };
                  c.eq = { 80, 200, 1.5f, 2500, 2, 0 }; c.comp = vmu (5); c.sat = { true, saturacio::T, 3, 50, 0 };
                  auto cors = harmonies ("Cors en falset", -12, harmonitzador::Tonalitat, { 7, 4 }, { -70, 70 }); cors.harm.formant = 2;
                  c.rets = { slap ("Slap de cinta", 120, -16, 3000), rv ("Molla", Spring, 1.3f, 10, -18), cors };
                  afegeix (c); }

                // ---- 6. R&B modern, trap melòdic i pop alternatiu ----
                { Conf c { "Urbà", "SZA · Kill Bill", "Aire i compressió doble amb de-esser. Plate càlida, delay de negres fosc amb molt ducking i un acord de quatre veus al fons." };
                  c.eq = { 90, 300, 0, 3500, 0, 3 }; c.comp = fet (4); c.comp2 = opt (3); c.deess = { true, 6500, -28 };
                  c.rets = { rv ("Plate càlida", Plate, 1.5f, 35, -16), dlNota ("Delay 1/4", 8, 30, -18, 300, 2500, 14),
                             harmonies ("Harmonies", -14, harmonitzador::Tonalitat, { 2, 4, -3, 5 }, { -80, 80, -50, 50 }, 150, 7000) };
                  afegeix (c); }
                { Conf c { "Urbà", "Post Malone · Circles", "Presència i compressió doble. Hall, delay de corxera amb puntet amb filtre de telèfon i un doble gruixut gairebé tan fort com la veu. (Sense Tune encara.)" };
                  c.eq = { 100, 300, 0, 3000, 2.5f, 1 }; c.comp = fet (5); c.comp2 = vca (3);
                  auto tel = dlNota ("Delay telèfon", 7, 25, -18, 600, 3000); filtre (tel, 600, 3000);
                  c.rets = { rv ("Hall", Hall, 2.0f, 45, -16), tel, dobles ("Doble gruixut", -3, 10, 14) };
                  afegeix (c); }
                { Conf c { "Urbà", "Kendrick Lamar · HUMBLE.", "Rap sec i agressiu: presència forta, 8:1 i tallador de pics. Només un delay de negres baix per als finals de compàs (puja'l amb el macro Eco o automatitza'l)." };
                  c.eq = { 110, 300, 0, 2800, 3.5f, 1 }; c.comp = fet (7, 8); c.lim = { true, 3, -1 };
                  c.rets = { dlNota ("Throws 1/4", 8, 30, -26, 300, 4000), dobles ("Reforç", -16, 6, 10) };
                  c.rets[1].sat = { true, saturacio::P, 3, 100, 0 };
                  afegeix (c); }
                { Conf c { "Urbà", "Lorde · Royals", "Cos i aire, òptic i de-esser. Una chamber fosca i un cor greu (octava baixa, 3a i 5a) molt comprimit, que fa de base." };
                  c.eq = { 80, 180, 1.5f, 3500, 0, 2 }; c.comp = opt (4); c.deess = { true, 6000, -28 };
                  auto cor = harmonies ("Cor greu", -10, harmonitzador::Tonalitat, { -7, 2, 4 }, { 0, -60, 60 }); cor.comp = { true, -34, 8, 2, 80, 8, 100 };
                  c.rets = { rv ("Chamber fosca", Chamber, 1.2f, 25, -18, 150, 4000), cor };
                  afegeix (c); }
                { Conf c { "Urbà", "Travis Scott · Sicko Mode", "Trap: presència, 8:1 i saturació. Hall gegant, delay de negres que cau de to i ad-libs filtrats a la reverb. (Sense Tune encara.)" };
                  c.eq = { 100, 300, 0, 4000, 3, 1 }; c.comp = fet (6, 8); c.sat = { true, saturacio::A, 3, 45, 0 };
                  auto cau = dlNota ("Delay que cau", 8, 45, -16, 300, 4000); cau.oct = { true, -5, 0, 100 };
                  auto ad = rv ("Ad-libs", Hall, 3.2f, 60, -20, 500, 3000);
                  c.rets = { rv ("Hall gegant", Hall, 3.2f, 60, -14), cau, ad };
                  afegeix (c); }
            }
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
