#include "mibase/Rack.h"
#include "mibase/Delay.h"
#include "mibase/Doubler.h"
#include "mibase/Drive.h"
#include "mibase/Equalitzador.h"
#include "mibase/Clipper.h"
#include "mibase/Estereo.h"
#include "mibase/LowEnd.h"
#include "mibase/Multibanda.h"
#include "mibase/Transitoris.h"
#include "mibase/Loudness.h"
#include "mibase/Harmonitzador.h"
#include "mibase/Limitador.h"
#include "mibase/Reverb.h"
#include "mibase/Saturacio.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <nlohmann/json.hpp>

namespace mibase::rack
{
    namespace
    {
        inline float dbALin (float db) { return std::pow (10.0f, db / 20.0f); }
        constexpr float maxDb = 24.0f;
    }

    const std::vector<TipusModul>& tipus()
    {
        static const std::vector<TipusModul> t {
            { "eq", eq::idPlugin, "EQ Pro", "Neteja i dona color: treu greus, retalla el fang, afegeix aire." },
            { "compressor", dinamica::idPlugin, "Compressor", "Iguala el volum de la veu perquè s'entengui sempre." },
            { "deesser", dinamica::idPlugin, "De-esser", "Suavitza les S, X i Z que punxen." },
            { "porta", dinamica::idPlugin, "Porta", "Talla el soroll de fons quan no cantes." },
            { "decapitor", saturacio::idPlugin, "Decapitor", "Saturació analògica: escalfor, cos o brutícia." },
            { "drive", drive::idPlugin, "Drive", "Distorsió de pedal: per a veus trencades o d'efecte." },
            { "doubler", doubler::idPlugin, "Doubler", "Dues còpies desafinades als costats: sembla una veu doblada." },
            { "harmonitzador", harmonitzador::idPlugin, "Harmonitzador", "Fins a 4 veus noves: per tonalitat, amb els acords de la cançó o amb el teclat MIDI." },
            { "tune", tune::idPlugin, "Tune", "Afina la veu a la nota de l'escala: natural o robòtic (0.79)." },
            { "octava", octava::idPlugin, "Octava i formant", "La veu més greu o més aguda, o més fosca o més prima." },
            { "delay", delay::idPlugin, "Delay", "Ecos al tempo de la cançó." },
            { "reverb", reverb::idPlugin, "Reverb", "L'espai: habitació, plate, sala…" },
            { "limitador", limitador::idPlugin, "Limitador", "Al final de tot: cap pic passa del sostre (5 ms de retard)." },
        };
        return t;
    }

    const std::vector<TipusModul>& tipus (Tipus q)
    {
        // 0.94: els mòduls del Mastering Studio (mateixos efectes, amb un punt de partida de màster)
        static const std::vector<TipusModul> m {
            { "eqm", eq::idPlugin, "EQ de màster", "Correcció i to: greus, fang, presència i aire, amb Mid/Side i bandes dinàmiques." },
            { "glue", dinamica::idPlugin, "Compressor de màster", "Cohesió (glue): 1–3 dB de reducció, atac lent perquè passi el cop." },
            { "multibanda", multibanda::idPlugin, "Multibanda", "Comprimeix 2, 3 o 4 bandes per separat: greus que s'escapen, mitjos tapats, aguts que punxen (0.99)." },
            { "deessm", dinamica::idPlugin, "De-esser de màster", "Calma les S i els plats que punxen al mix sencer (6–9 kHz)." },
            { "lowend", lowend::idPlugin, "Low End Focus", "Els greus a part: sub, cop, cua, escalfor, fang i mono (0.99)." },
            { "transitoris", transitoris::idPlugin, "Transient Shaper", "Més o menys cop i cua: recupera l'impacte de la bateria o suavitza el que punxa (0.96)." },
            { "colorm", saturacio::idPlugin, "Saturació (color)", "Cinta, consola o vàlvula: escalfor i densitat, suau i en paral·lel." },
            { "estereo", estereo::idPlugin, "Estèreo", "Amplada per bandes, mono als greus, Mid/Side i correlació." },
            { "clipper", clipper::idPlugin, "Clipper", "Just abans del limitador: retalla els pics curts perquè el limitador no aixafi el cop (0.96)." },
            { "limitm", limitador::idPlugin, "Limitador", "L'últim: puja el volum sense que cap pic passi del sostre (true peak)." },
            { "loudness", loudness::idPlugin, "Loudness Meter", "Només mesura: LUFS, true peak, LRA, crest i correlació." },
        };
        return q == Tipus::Master ? m : tipus();
    }

    const TipusModul* busca (const std::string& clau)
    {
        for (const auto& t : tipus()) if (clau == t.clau) return &t;
        for (const auto& t : tipus (Tipus::Master)) if (clau == t.clau) return &t;
        return nullptr;
    }

    std::shared_ptr<EfecteIntern> creaModul (const std::string& clau, const std::atomic<double>* tempo)
    {
        if (clau == "eq")
        {
            auto q = std::make_shared<eq::Equalitzador>();
            eq::Banda b;
            b.usada = true; b.tipus = eq::HighPass; b.freq = 90.0f;
            q->posaBanda (0, b);
            return q;
        }
        if (clau == "compressor" || clau == "deesser" || clau == "porta")
        {
            auto d = std::make_shared<dinamica::Dinamica>();
            using namespace dinamica;
            if (clau == "compressor") { d->posa (ModeP, Compressor); d->posa (Llindar, -20); d->posa (Ratio, 4); d->posa (Atac, 5); d->posa (Alliberament, 120); d->posa (Guany, 3); }
            else if (clau == "deesser") { d->posa (ModeP, DeEsser); d->posa (FreqDeEsser, 6500); d->posa (Llindar, -28); d->posa (Ratio, 4); d->posa (Atac, 1); d->posa (Alliberament, 60); }
            else { d->posa (ModeP, Gate); d->posa (Llindar, -45); d->posa (Rang, 30); d->posa (Atac, 1); d->posa (Alliberament, 120); }
            return d;
        }
        if (clau == "decapitor") { auto s = std::make_shared<saturacio::Saturacio>(); s->posa (saturacio::Drive, 2.0f); s->posa (saturacio::Mix, 60.0f); return s; }
        if (clau == "drive") { auto d = std::make_shared<drive::Drive>(); d->posa (drive::ModeP, drive::Crunch); d->posa (drive::Gain, 3.0f); d->posa (drive::Mix, 50.0f); return d; }
        if (clau == "doubler") return std::make_shared<doubler::Doubler>();
        if (clau == "harmonitzador") return std::make_shared<harmonitzador::Harmonitzador>();   // 0.73
        if (clau == "octava") return std::make_shared<octava::Octava>();                        // 0.73
        if (clau == "tune") return std::make_shared<tune::Tune>();                              // 0.79
        if (clau == "delay")
        {
            auto d = std::make_shared<delay::Delay>();
            d->posaTempo (tempo);
            d->posa (delay::Sync, 1); d->posa (delay::Nota, 8); d->posa (delay::Feedback, 25); d->posa (delay::Mix, 15);
            d->posa (delay::LowCut, 300); d->posa (delay::HighCut, 5000);
            return d;
        }
        if (clau == "reverb")
        {
            auto r = std::make_shared<reverb::Reverb>();
            r->posaTempo (tempo);
            r->posa (reverb::Alg, reverb::Plate); r->posa (reverb::Mix, 15); r->posa (reverb::Decay, 1.4f); r->posa (reverb::PreDelay, 30);
            return r;
        }
        // ---- 0.94: Mastering Studio ----
        if (clau == "eqm")
        {
            auto q = std::make_shared<eq::Equalitzador>();
            eq::Banda b; b.usada = true; b.tipus = eq::HighPass; b.freq = 25.0f;
            q->posaBanda (0, b);
            return q;
        }
        if (clau == "glue" || clau == "deessm")
        {
            auto d = std::make_shared<dinamica::Dinamica>();
            using namespace dinamica;
            if (clau == "glue") { d->posa (ModeP, Compressor); d->posa (Llindar, -18); d->posa (Ratio, 2); d->posa (Atac, 30); d->posa (Alliberament, 150);
                                  d->posa (Genoll, 6); d->posa (FiltreGreus, 100); d->posa (Detector, 1); d->posa (Guany, 1); }
            else { d->posa (ModeP, DeEsser); d->posa (FreqDeEsser, 7500); d->posa (Llindar, -30); d->posa (Ratio, 3); d->posa (Atac, 1); d->posa (Alliberament, 80); }
            return d;
        }
        if (clau == "colorm")
        {
            auto s = std::make_shared<saturacio::Saturacio>();
            s->posa (saturacio::EstilP, saturacio::A); s->posa (saturacio::Drive, 1.5f); s->posa (saturacio::Mix, 40.0f); s->posa (saturacio::LowCut, 20.0f);
            return s;
        }
        if (clau == "estereo") return std::make_shared<estereo::Estereo>();
        if (clau == "multibanda") return std::make_shared<multibanda::Multibanda>();   // 0.99
        if (clau == "lowend") { auto g = std::make_shared<lowend::LowEnd>(); g->posa (lowend::Mono, 120); return g; }
        if (clau == "transitoris") { auto t = std::make_shared<transitoris::Transitoris>(); t->posa (transitoris::Atac, 20); return t; }   // 0.96
        if (clau == "clipper") { auto c = std::make_shared<clipper::Clipper>(); c->posa (clipper::Drive, 1.5f); c->posa (clipper::Ceiling, -1.5f); return c; }
        if (clau == "loudness") return std::make_shared<loudness::Modul>();
        if (clau == "limitm") { auto l = std::make_shared<limitador::Limitador>(); l->posa (limitador::Gain, 3); l->posa (limitador::Ceiling, -1); l->posa (limitador::TruePeak, 1); return l; }
        if (clau == "limitador") { auto l = std::make_shared<limitador::Limitador>(); l->posa (limitador::Gain, 0); l->posa (limitador::Ceiling, -1); return l; }
        return nullptr;
    }

    std::string nomModul (const std::string& clau, const EfecteIntern* fx)
    {
        if (auto* d = dynamic_cast<const dinamica::Dinamica*> (fx))
        {
            const int m = (int) d->valor (dinamica::ModeP);
            return m == dinamica::Compressor ? "Compressor" : m == dinamica::DeEsser ? "De-esser" : m == dinamica::Gate ? "Porta" : "Expansor";
        }
        if (auto* t = busca (clau)) return t->nom;
        return clau;
    }

    // ------------------------------------------------------------------

    Rack::Rack (Tipus t) : tipusR (t)
    {
        for (int i = 0; i < numParams; ++i) { float mn, mx, def; rangParam (i, mn, mx, def); par[(size_t) i].store (def); }
        static const char* const nomsDefecte[numMacros] = { "Cos", "Presència", "Aire", "Compressió", "Escalfor", "Doble", "Espai", "Eco" };
        static const char* const nomsMaster[numMacros] = { "Escalfor", "Presència", "Punch", "Amplada", "Greus", "Densitat", "Aire", "Loudness" };   // 0.94
        for (int m = 0; m < numMacros; ++m) noms[(size_t) m] = esMaster() ? nomsMaster[m] : nomsDefecte[m];
        if (esMaster()) { lIn = std::make_unique<loudness::Mesurador>(); lOut = std::make_unique<loudness::Mesurador>(); }
        publica (std::make_shared<Cadena>());
    }

    Rack::~Rack() { actual.store (nullptr); }

    const loudness::Mesures* Rack::loudnessEntrada() const noexcept { return lIn != nullptr ? &lIn->mesures() : nullptr; }
    const loudness::Mesures* Rack::loudnessSortida() const noexcept { return lOut != nullptr ? &lOut->mesures() : nullptr; }
    float Rack::lufsObjectiu() const noexcept
    {
        const auto& v = loudness::objectius();
        const auto& o = v[(size_t) std::clamp (objectiu(), 0, (int) v.size() - 1)];
        return o.lufs > -0.5f ? 0.0f : (o.configurable ? lufsPersonal() : o.lufs);
    }
    float Rack::truePeakObjectiu() const noexcept
    {
        const auto& v = loudness::objectius();
        return v[(size_t) std::clamp (objectiu(), 0, (int) v.size() - 1)].truePeak;
    }

    void Rack::reiniciaMesures() noexcept { if (lIn != nullptr) lIn->reinicia(); if (lOut != nullptr) lOut->reinicia(); }

    void rangParam (int p, float& mn, float& mx, float& def)
    {
        if (p >= Envia1 && p < Retorn1) { mn = minDbRetorn; mx = 6.0f; def = 0.0f; }
        else if (p >= Retorn1 && p < PanRetorn1) { mn = minDbRetorn; mx = 6.0f; def = -12.0f; }
        else if (p >= PanRetorn1 && p < Ducking1) { mn = -100.0f; mx = 100.0f; def = 0.0f; }
        else if (p >= Ducking1 && p < numParams) { mn = 0.0f; mx = 24.0f; def = 0.0f; }
        else if (p == Entrada || p == Sortida) { mn = -maxDb; mx = maxDb; def = 0.0f; }
        else { mn = 0.0f; mx = 1.0f; def = 0.0f; }
    }

    void Rack::Linia::prepara (int minim)
    {
        int m = 1;
        while (m < minim) m <<= 1;
        l.assign ((size_t) m, 0.0f);
        r.assign ((size_t) m, 0.0f);
        mascara = m - 1;
        w = 0;
    }

    void Rack::Linia::passa (float* L, float* R, int n, int retard) noexcept
    {
        if (l.empty()) return;
        retard = std::clamp (retard, 0, mascara);
        for (int i = 0; i < n; ++i)
        {
            l[(size_t) w] = L[i];
            r[(size_t) w] = R[i];
            const int j = (w - retard) & mascara;
            L[i] = l[(size_t) j];
            R[i] = r[(size_t) j];
            w = (w + 1) & mascara;
        }
    }

    void Rack::posaValor (int p, float v) noexcept
    {
        if (p < Envia1 || p >= numParams || ! std::isfinite (v)) return;
        float mn, mx, def;
        rangParam (p, mn, mx, def);
        par[(size_t) p].store (std::clamp (v, mn, mx), std::memory_order_relaxed);
    }

    void Rack::prepara (double f, int bloc)
    {
        fs = f > 1000.0 ? f : 48000.0;
        maxBloc = std::max (16, bloc);
        preparat = true;
        for (auto& m : ui->moduls) preparaModul (*m);
        const int lin = (int) (0.25 * fs) + maxBloc;   // 0.76: fins a 250 ms d'alineació entre camins
        liniaPrincipal.prepara (lin);
        for (int k = 0; k < maxRetorns; ++k)
        {
            liniesRetorn[(size_t) k].prepara (lin);
            retL[(size_t) k].assign ((size_t) maxBloc, 0.0f);
            retR[(size_t) k].assign ((size_t) maxBloc, 0.0f);
        }
        primerBloc = true;
        if (esMaster())   // 0.94
        {
            lIn->prepara (fs);
            lOut->prepara (fs);
            liniaSec.prepara ((int) (0.25 * fs) + maxBloc);
            secL.assign ((size_t) maxBloc, 0.0f);
            secR.assign ((size_t) maxBloc, 0.0f);
            gProc = gSec = 1.0f;
            fosa = escoltaOriginal.load() ? 1.0f : 0.0f;
        }
    }

    void Rack::preparaModul (Modul& m)
    {
        if (! preparat) return;
        m.fx->prepara (fs, maxBloc);
        m.secL.assign ((size_t) maxBloc, 0.0f);
        m.secR.assign ((size_t) maxBloc, 0.0f);
    }

    int Rack::latencia() const noexcept
    {
        const Cadena* c = actual.load (std::memory_order_acquire);
        if (c == nullptr) return 0;
        int r = 0, ret[maxRetorns] {};
        for (size_t i = 0; i < c->moduls.size(); ++i)
        {
            const auto& m = c->moduls[i];
            if (! m->on.load (std::memory_order_relaxed)) continue;
            const int k = i < c->camins.size() ? c->camins[i] : 0;
            (k == 0 ? r : ret[k - 1]) += std::max (0, m->fx->latencia());
        }
        int mx = 0;
        for (int k = 0; k < maxRetorns; ++k) if (c->retorns[(size_t) k].actiu) mx = std::max (mx, ret[k]);
        return r + mx;   // 0.76: el camí més lent mana (els altres s'hi alineen)
    }

    void Rack::posaDirecte (bool d) noexcept
    {
        const Cadena* c = actual.load (std::memory_order_acquire);
        if (c == nullptr) return;
        for (const auto& m : c->moduls)
            if (m != nullptr && m->fx != nullptr) m->fx->posaDirecte (d);
    }

    void Rack::processa (float* L, float* R, int n) noexcept
    {
        // 0.76: els retorns fan servir buffers de «maxBloc»: un bloc més gran es fa a trossos
        for (int o = 0; o < n; o += maxBloc)
        {
            const int k = std::min (maxBloc, n - o);
            if (esMaster() && (int) secL.size() >= k) processaMaster (L + o, R + o, k);   // 0.94
            else processaBloc (L + o, R + o, k);
        }
        if (n <= 0) epoca.fetch_add (1, std::memory_order_release);
    }

    void Rack::aplica (Modul& m, float* L, float* R, int n) noexcept
    {
        const bool on = m.on.load (std::memory_order_relaxed);
        float objectiu = on ? std::clamp (m.mix.load (std::memory_order_relaxed), 0.0f, 1.0f) : 0.0f;
        if (on && m.fx->latencia() > 0) objectiu = 1.0f;   // desfasat: sense barreja
        if (objectiu <= 0.0f && m.mixAnt <= 0.0f)
        {
            if (m.estavaOn) { m.fx->buida(); m.estavaOn = false; }   // apagat del tot: la cua vella, fora
            m.canviSuau = 0.0f;
            m.canviDb.store (0.0f, std::memory_order_relaxed);
            return;
        }
        m.estavaOn = true;
        // 0.94: quant canvia el volum aquest mòdul (RMS de sortida respecte d'entrada), per a la vista compacta
        double eIn = 0.0;
        for (int i = 0; i < n; ++i) eIn += (double) L[i] * L[i] + (double) R[i] * R[i];
        auto mesuraCanvi = [&m, L, R, n, eIn]
        {
            double eOut = 0.0;
            for (int i = 0; i < n; ++i) eOut += (double) L[i] * L[i] + (double) R[i] * R[i];
            if (eIn > 1.0e-9 * n)
            {
                const float d = (float) std::clamp (10.0 * std::log10 (std::max (eOut, 1.0e-20) / eIn), -24.0, 24.0);
                m.canviSuau += (d - m.canviSuau) * 0.05f;
                m.canviDb.store (m.canviSuau, std::memory_order_relaxed);
            }
        };
        const int cap = (int) m.secL.size();
        if (objectiu >= 0.9999f && m.mixAnt >= 0.9999f) { m.fx->processa (L, R, n); m.mixAnt = 1.0f; mesuraCanvi(); return; }
        for (int ofs = 0; ofs < n; ofs += std::max (1, cap))
        {
            const int k = std::min (n - ofs, cap);
            if (k <= 0) { m.fx->processa (L + ofs, R + ofs, n - ofs); break; }
            std::copy_n (L + ofs, k, m.secL.data());
            std::copy_n (R + ofs, k, m.secR.data());
            m.fx->processa (L + ofs, R + ofs, k);
            const float g0 = m.mixAnt + (objectiu - m.mixAnt) * (float) ofs / (float) n;
            const float g1 = m.mixAnt + (objectiu - m.mixAnt) * (float) (ofs + k) / (float) n;
            const float dg = (g1 - g0) / (float) k;
            float g = g0;
            for (int i = 0; i < k; ++i)
            {
                g += dg;
                L[ofs + i] = m.secL[(size_t) i] + (L[ofs + i] - m.secL[(size_t) i]) * g;
                R[ofs + i] = m.secR[(size_t) i] + (R[ofs + i] - m.secR[(size_t) i]) * g;
            }
        }
        m.mixAnt = objectiu;
        mesuraCanvi();
    }

    void Rack::processaBloc (float* L, float* R, int n) noexcept
    {
        Cadena* c = actual.load (std::memory_order_acquire);
        const float tIn = dbALin (par[Entrada].load (std::memory_order_relaxed));
        const float tOut = dbALin (par[Sortida].load (std::memory_order_relaxed));
        if (primerBloc) { gIn = tIn; gOut = tOut; primerBloc = false; }
        // entrada
        {
            const float d = (tIn - gIn) / (float) std::max (1, n);
            float pic = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                gIn += d;
                L[i] *= gIn; R[i] *= gIn;
                pic = std::max (pic, std::max (std::abs (L[i]), std::abs (R[i])));
            }
            gIn = tIn;
            if (pic > mes.entrada.load (std::memory_order_relaxed)) mes.entrada.store (pic, std::memory_order_relaxed);
        }
        if (c != nullptr)
        {
            const size_t nm = c->moduls.size();
            auto camiDe = [c] (size_t i) { return i < c->camins.size() ? c->camins[i] : 0; };
            for (size_t i = 0; i < nm; ++i)
                if (camiDe (i) == 0) aplica (*c->moduls[i], L, R, n);

            // ---- 0.76: retorns en paral·lel ----
            bool hiHa = false;
            int latRet[maxRetorns] {}, maxLat = 0;
            for (size_t i = 0; i < nm; ++i)
                if (camiDe (i) > 0 && c->moduls[i]->on.load (std::memory_order_relaxed))
                    latRet[camiDe (i) - 1] += std::max (0, c->moduls[i]->fx->latencia());
            for (int k = 0; k < maxRetorns; ++k)
                if (c->retorns[(size_t) k].actiu && ! retL[(size_t) k].empty()) { hiHa = true; maxLat = std::max (maxLat, latRet[k]); }
            if (hiHa)
            {
                const float inv = 1.0f / (float) std::max (1, n);
                for (int k = 0; k < maxRetorns; ++k)
                {
                    if (! c->retorns[(size_t) k].actiu || retL[(size_t) k].empty()) continue;
                    const float dbE = par[(size_t) (Envia1 + k)].load (std::memory_order_relaxed);
                    const float tE = dbE <= minDbRetorn + 0.05f ? 0.0f : dbALin (dbE);
                    const float dE = (tE - gEnvia[(size_t) k]) * inv;
                    float* rl = retL[(size_t) k].data();
                    float* rr = retR[(size_t) k].data();
                    float g = gEnvia[(size_t) k];
                    for (int i = 0; i < n; ++i) { g += dE; rl[i] = L[i] * g; rr[i] = R[i] * g; }
                    gEnvia[(size_t) k] = tE;
                    for (size_t i = 0; i < nm; ++i)
                        if (camiDe (i) == k + 1) aplica (*c->moduls[i], rl, rr, n);
                    liniesRetorn[(size_t) k].passa (rl, rr, n, maxLat - latRet[k]);
                }
                liniaPrincipal.passa (L, R, n, maxLat);
                // ducking: la clau és la veu principal (ja alineada); atac 5 ms, retorn 250 ms
                const float kA = 1.0f - std::exp (-1.0f / (0.005f * (float) fs)), kR = 1.0f - std::exp (-1.0f / (0.25f * (float) fs));
                float tR[maxRetorns], dR[maxRetorns], duck[maxRetorns], pl[maxRetorns], pr[maxRetorns];
                for (int k = 0; k < maxRetorns; ++k)
                {
                    const float dbR = par[(size_t) (Retorn1 + k)].load (std::memory_order_relaxed);
                    tR[k] = (! c->retorns[(size_t) k].actiu || dbR <= minDbRetorn + 0.05f) ? 0.0f : dbALin (dbR);
                    dR[k] = (tR[k] - gRetorn[(size_t) k]) * inv;
                    duck[k] = par[(size_t) (Ducking1 + k)].load (std::memory_order_relaxed);
                    const float p = std::clamp (par[(size_t) (PanRetorn1 + k)].load (std::memory_order_relaxed) / 100.0f, -1.0f, 1.0f);
                    pl[k] = p > 0.0f ? 1.0f - p : 1.0f;
                    pr[k] = p < 0.0f ? 1.0f + p : 1.0f;
                }
                for (int i = 0; i < n; ++i)
                {
                    const float clau = std::max (std::abs (L[i]), std::abs (R[i]));
                    envClau += (clau - envClau) * (clau > envClau ? kA : kR);
                    if ((i & 15) == 0)
                    {
                        // la veu a -45 dB o menys: res; a -10 dB o més: tot el ducking
                        const float nivell = std::clamp ((20.0f * std::log10 (envClau + 1e-9f) + 45.0f) / 35.0f, 0.0f, 1.0f);
                        for (int k = 0; k < maxRetorns; ++k) gDuck[(size_t) k] = duck[k] > 0.01f ? dbALin (-duck[k] * nivell) : 1.0f;
                    }
                    float sl = L[i], sr = R[i];
                    for (int k = 0; k < maxRetorns; ++k)
                    {
                        gRetorn[(size_t) k] += dR[k];
                        if (! c->retorns[(size_t) k].actiu || retL[(size_t) k].empty()) continue;
                        const float g = gRetorn[(size_t) k] * gDuck[(size_t) k];
                        sl += g * pl[k] * retL[(size_t) k][(size_t) i];
                        sr += g * pr[k] * retR[(size_t) k][(size_t) i];
                    }
                    L[i] = sl;
                    R[i] = sr;
                }
                for (int k = 0; k < maxRetorns; ++k) gRetorn[(size_t) k] = tR[k];
            }
        }
        // sortida
        {
            const float d = (tOut - gOut) / (float) std::max (1, n);
            float pic = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                gOut += d;
                L[i] *= gOut; R[i] *= gOut;
                pic = std::max (pic, std::max (std::abs (L[i]), std::abs (R[i])));
            }
            gOut = tOut;
            if (pic > mes.sortida.load (std::memory_order_relaxed)) mes.sortida.store (pic, std::memory_order_relaxed);
        }
        epoca.fetch_add (1, std::memory_order_release);
    }

    void Rack::processaMaster (float* L, float* R, int n) noexcept
    {
        // l'original: es copia abans del rack i es retarda tant com la cadena, perquè l'A/B no salti en el temps
        std::copy_n (L, n, secL.data());
        std::copy_n (R, n, secR.data());
        processaBloc (L, R, n);
        liniaSec.passa (secL.data(), secR.data(), n, latencia());
        lIn->processa (secL.data(), secR.data(), n);
        lOut->processa (L, R, n);
        // volum igualat: el que sona més fort baixa fins a l'altre (Short-Term), lentament perquè no bombi
        float tProc = 1.0f, tSec = 1.0f;
        if (igualatAB.load (std::memory_order_relaxed))
        {
            const float a = lIn->mesures().shortTerm.load (std::memory_order_relaxed), b = lOut->mesures().shortTerm.load (std::memory_order_relaxed);
            if (a > -60.0f && b > -60.0f)
            {
                const float d = std::clamp (b - a, -24.0f, 24.0f);
                if (d > 0.0f) tProc = dbALin (-d); else tSec = dbALin (d);
                compDb.store (-d, std::memory_order_relaxed);
            }
        }
        else compDb.store (0.0f, std::memory_order_relaxed);
        const float tFosa = escoltaOriginal.load (std::memory_order_relaxed) ? 1.0f : 0.0f;
        const float kG = 1.0f - std::exp (-(float) n / (0.4f * (float) fs));
        const float kF = 1.0f - std::exp (-(float) n / (0.02f * (float) fs));
        const float p0 = gProc, s0 = gSec, f0 = fosa;
        gProc += (tProc - gProc) * kG;
        gSec += (tSec - gSec) * kG;
        fosa += (tFosa - fosa) * kF;
        if (std::abs (fosa - tFosa) < 1.0e-4f) fosa = tFosa;
        const float inv = 1.0f / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (i + 1) * inv;
            const float gp = p0 + (gProc - p0) * t, gs = s0 + (gSec - s0) * t, f = f0 + (fosa - f0) * t;
            L[i] = L[i] * gp * (1.0f - f) + secL[(size_t) i] * gs * f;
            R[i] = R[i] * gp * (1.0f - f) + secR[(size_t) i] * gs * f;
        }
    }

    // ---- paràmetres ----
    InfoParametre Rack::infoParametre (int i) const
    {
        InfoParametre r;
        if (i == Entrada) { r.id = "entrada"; r.nom = "Entrada"; }
        else if (i == Sortida) { r.id = "sortida"; r.nom = "Sortida"; }
        else if (i >= Envia1 && i < numParams)   // 0.76
        {
            const int k = (i - Envia1) % maxRetorns + 1;
            const char* q = i < Retorn1 ? "envia" : i < PanRetorn1 ? "retorn" : i < Ducking1 ? "panretorn" : "ducking";
            const char* nq = i < Retorn1 ? "Envia" : i < PanRetorn1 ? "Retorn" : i < Ducking1 ? "Pan retorn" : "Ducking";
            r.id = std::string (q) + std::to_string (k);
            r.nom = std::string (nq) + " " + std::to_string (k) + (ui->retorns[(size_t) k - 1].nom.empty() ? "" : " (" + ui->retorns[(size_t) k - 1].nom + ")");
        }
        else if (i >= Macro1 && i < numParams)
        {
            r.id = "macro" + std::to_string (i - Macro1 + 1);
            r.nom = "M" + std::to_string (i - Macro1 + 1) + " " + noms[(size_t) (i - Macro1)];
        }
        return r;
    }

    void Rack::posaParametre (int i, float v01) noexcept
    {
        if (i < 0 || i >= numParams || ! std::isfinite (v01)) return;
        v01 = std::clamp (v01, 0.0f, 1.0f);
        if (i == Entrada || i == Sortida) { par[(size_t) i].store (-maxDb + 2.0f * maxDb * v01, std::memory_order_relaxed); return; }
        if (i >= Envia1) { float mn, mx, def; rangParam (i, mn, mx, def); par[(size_t) i].store (mn + (mx - mn) * v01, std::memory_order_relaxed); return; }
        const float vell = par[(size_t) i].exchange (v01, std::memory_order_relaxed);
        if (std::abs (vell - v01) < 1.0e-7f) return;
        if (const Cadena* c = actual.load (std::memory_order_acquire)) aplicaMacro (*c, i - Macro1, v01);
    }

    float Rack::parametre (int i) const noexcept
    {
        if (i < 0 || i >= numParams) return 0.0f;
        const float v = par[(size_t) i].load (std::memory_order_relaxed);
        if (i >= Envia1) { float mn, mx, def; rangParam (i, mn, mx, def); return (v - mn) / (mx - mn); }
        return (i == Entrada || i == Sortida) ? (v + maxDb) / (2.0f * maxDb) : v;
    }

    std::string Rack::textParametre (int i, float v01) const
    {
        char b[40];
        if (i == Entrada || i == Sortida) std::snprintf (b, sizeof b, "%+.1f dB", (double) (-maxDb + 2.0f * maxDb * v01));
        else if (i >= Envia1)
        {
            float mn, mx, def;
            rangParam (i, mn, mx, def);
            const float v = mn + (mx - mn) * v01;
            if (i < PanRetorn1) { if (v <= minDbRetorn + 0.05f) return "-inf"; std::snprintf (b, sizeof b, "%+.1f dB", (double) v); }
            else if (i < Ducking1) { const int p = (int) std::lround (v); if (p == 0) return "C"; std::snprintf (b, sizeof b, "%d %s", std::abs (p), p < 0 ? "E" : "D"); }
            else std::snprintf (b, sizeof b, "-%.1f dB", (double) v);
        }
        else std::snprintf (b, sizeof b, "%.1f", (double) (v01 * 10.0f));
        std::string s (b);
        for (auto& ch : s) if (ch == '.') ch = ',';
        return s;
    }

    void Rack::aplicaMacro (const Cadena& c, int m, float v) noexcept
    {
        for (const auto& e : c.enllacos)
        {
            if (e.macro != m) continue;
            if (e.modul == -1) { if (e.param >= Envia1 && e.param < numParams) posaParametre (e.param, e.mn + (e.mx - e.mn) * v); continue; }   // 0.76
            if (e.modul < 0 || e.modul >= (int) c.moduls.size()) continue;
            auto& fx = *c.moduls[(size_t) e.modul]->fx;
            if (e.param < 0 || e.param >= fx.numParametres()) continue;
            fx.posaParametre (e.param, e.mn + (e.mx - e.mn) * v);
        }
    }

    void Rack::aplicaMacro (int m) noexcept
    {
        if (m >= 0 && m < numMacros) aplicaMacro (*ui, m, par[(size_t) (Macro1 + m)].load (std::memory_order_relaxed));
    }

    // ---- la cadena (interfície) ----
    std::shared_ptr<Cadena> Rack::copia() const
    {
        auto c = std::make_shared<Cadena>();
        c->moduls = ui->moduls;
        c->camins = ui->camins;
        c->camins.resize (c->moduls.size(), 0);
        c->retorns = ui->retorns;
        c->enllacos = ui->enllacos;
        return c;
    }

    void Rack::publica (std::shared_ptr<Cadena> c)
    {
        auto vella = ui;
        ui = std::move (c);
        actual.store (ui.get(), std::memory_order_release);
        if (vella != nullptr) guardades.push_back ({ vella, epoca.load (std::memory_order_acquire) });
        ++canvis;
        recull();
    }

    void Rack::recull()
    {
        const auto e = epoca.load (std::memory_order_acquire);
        guardades.erase (std::remove_if (guardades.begin(), guardades.end(), [e] (const Guardada& g) { return e >= g.retirada + 2; }), guardades.end());
    }

    int Rack::afegeix (const std::string& clau, int pos, int cami)
    {
        return afegeix (clau, creaModul (clau, tempo), pos, cami);
    }

    int Rack::afegeix (const std::string& clau, std::shared_ptr<EfecteIntern> fx, int pos, int cami)
    {
        if (fx == nullptr || numModuls() >= maxModuls) return -1;
        auto m = std::make_shared<Modul>();
        m->clau = clau;
        m->fx = std::move (fx);
        m->mixAnt = 1.0f;
        preparaModul (*m);
        auto c = copia();
        const int n = (int) c->moduls.size();
        if (pos < 0 || pos > n) pos = n;
        c->moduls.insert (c->moduls.begin() + pos, m);
        c->camins.insert (c->camins.begin() + pos, std::clamp (cami, 0, maxRetorns));
        if (cami > 0) c->retorns[(size_t) (std::clamp (cami, 1, maxRetorns) - 1)].actiu = true;
        for (auto& e : c->enllacos) if (e.modul >= pos) ++e.modul;
        publica (c);
        return pos;
    }

    void Rack::treu (int i)
    {
        if (i < 0 || i >= numModuls()) return;
        auto c = copia();
        c->moduls.erase (c->moduls.begin() + i);
        c->camins.erase (c->camins.begin() + i);
        std::vector<Enllac> v;
        for (auto e : c->enllacos)
        {
            if (e.modul == -1) { v.push_back (e); continue; }
            if (e.modul == i) continue;
            if (e.modul > i) --e.modul;
            v.push_back (e);
        }
        c->enllacos = std::move (v);
        publica (c);
    }

    void Rack::mou (int de, int a)
    {
        const int n = numModuls();
        if (de < 0 || de >= n || a < 0 || a >= n || de == a) return;
        auto c = copia();
        auto m = c->moduls[(size_t) de];
        const int cm = c->camins[(size_t) de];
        c->moduls.erase (c->moduls.begin() + de);
        c->moduls.insert (c->moduls.begin() + a, m);
        c->camins.erase (c->camins.begin() + de);
        c->camins.insert (c->camins.begin() + a, cm);
        // on va a parar cada índex vell
        for (auto& e : c->enllacos)
        {
            if (e.modul == -1) continue;
            if (e.modul == de) e.modul = a;
            else if (de < a && e.modul > de && e.modul <= a) --e.modul;
            else if (a < de && e.modul >= a && e.modul < de) ++e.modul;
        }
        publica (c);
    }

    void Rack::posaCami (int i, int cm)
    {
        if (i < 0 || i >= numModuls()) return;
        auto c = copia();
        c->camins[(size_t) i] = std::clamp (cm, 0, maxRetorns);
        if (cm > 0) c->retorns[(size_t) (std::clamp (cm, 1, maxRetorns) - 1)].actiu = true;
        publica (c);
    }

    void Rack::activaRetorn (int k, bool actiu, const std::string& nom)
    {
        if (k < 0 || k >= maxRetorns) return;
        auto c = copia();
        c->retorns[(size_t) k].actiu = actiu;
        if (! nom.empty()) c->retorns[(size_t) k].nom = nom;
        publica (c);
    }

    void Rack::posaOn (int i, bool on) { if (auto* m = modul (i)) { m->on.store (on); ++canvis; } }
    void Rack::posaMix (int i, float v) { if (auto* m = modul (i)) { m->mix.store (std::clamp (v, 0.0f, 1.0f)); ++canvis; } }

    bool Rack::enllaca (const Enllac& e)
    {
        if (e.macro < 0 || e.macro >= numMacros) return false;
        if (e.modul == -1) { if (e.param < Envia1 || e.param >= numParams) return false; }   // 0.76: un paràmetre del rack
        else
        {
            if (modul (e.modul) == nullptr) return false;
            if (e.param < 0 || e.param >= modul (e.modul)->fx->numParametres()) return false;
        }
        if ((int) ui->enllacos.size() >= maxEnllacos) return false;
        auto c = copia();
        for (auto& x : c->enllacos)
            if (x.macro == e.macro && x.modul == e.modul && x.param == e.param) { x = e; publica (c); return true; }
        c->enllacos.push_back (e);
        publica (c);
        return true;
    }

    void Rack::treuEnllac (int k)
    {
        if (k < 0 || k >= (int) ui->enllacos.size()) return;
        auto c = copia();
        c->enllacos.erase (c->enllacos.begin() + k);
        publica (c);
    }

    void Rack::posaEnllac (int k, const Enllac& e)
    {
        if (k < 0 || k >= (int) ui->enllacos.size()) return;
        auto c = copia();
        c->enllacos[(size_t) k] = e;
        publica (c);
    }

    void Rack::buida() noexcept
    {
        for (auto& m : ui->moduls) m->fx->buida();
        liniaPrincipal.buida();
        for (auto& l : liniesRetorn) l.buida();
        liniaSec.buida();
        envClau = 0.0f;
    }

    // ---- estat (JSON) ----
    std::string Rack::estat() const
    {
        nlohmann::ordered_json j;
        j["mibase-rack"] = 1;
        if (esMaster()) { j["tipus"] = "master"; j["igualat"] = igualatAB.load(); j["objectiu"] = objectiuLufs.load(); j["lufsPersonal"] = lufsPers.load(); }   // 0.94; 0.95
        j["entrada"] = par[Entrada].load();
        j["sortida"] = par[Sortida].load();
        auto mac = nlohmann::ordered_json::array();
        for (int m = 0; m < numMacros; ++m) mac.push_back ({ { "nom", noms[(size_t) m] }, { "valor", par[(size_t) (Macro1 + m)].load() } });
        j["macros"] = mac;
        auto mods = nlohmann::ordered_json::array();
        for (size_t i = 0; i < ui->moduls.size(); ++i)
        {
            const auto& m = ui->moduls[i];
            nlohmann::ordered_json x { { "clau", m->clau }, { "on", m->on.load() }, { "mix", m->mix.load() }, { "estat", m->fx->estat() } };
            if (cami ((int) i) > 0) x["cami"] = cami ((int) i);   // 0.76
            mods.push_back (x);
        }
        j["moduls"] = mods;
        auto rets = nlohmann::ordered_json::array();   // 0.76
        for (int k = 0; k < maxRetorns; ++k)
            rets.push_back ({ { "nom", ui->retorns[(size_t) k].nom }, { "actiu", ui->retorns[(size_t) k].actiu },
                              { "envia", par[(size_t) (Envia1 + k)].load() }, { "retorn", par[(size_t) (Retorn1 + k)].load() },
                              { "pan", par[(size_t) (PanRetorn1 + k)].load() }, { "ducking", par[(size_t) (Ducking1 + k)].load() } });
        j["retorns"] = rets;
        if (! presetNom.empty()) { j["preset"] = presetNom; j["presetModificat"] = presetMod; }   // 0.79
        auto en = nlohmann::ordered_json::array();
        for (const auto& e : ui->enllacos) en.push_back ({ e.macro, e.modul, e.param, e.mn, e.mx });
        j["enllacos"] = en;
        return j.dump();
    }

    void Rack::posaEstat (const std::string& s)
    {
        if (s.empty()) { posaEstat (estatInicial (tipusR)); return; }
        nlohmann::json j;
        try { j = nlohmann::json::parse (s); }
        catch (...) { return; }
        if (! j.is_object() || ! j.contains ("mibase-rack")) return;
        try
        {
            auto c = std::make_shared<Cadena>();
            for (const auto& x : j.value ("moduls", nlohmann::json::array()))
            {
                if ((int) c->moduls.size() >= maxModuls) break;
                const auto clau = x.value ("clau", std::string());
                auto fx = creaModul (clau, tempo);
                if (fx == nullptr) continue;
                fx->posaEstat (x.value ("estat", std::string()));
                auto m = std::make_shared<Modul>();
                m->clau = clau;
                m->fx = fx;
                m->on.store (x.value ("on", true));
                m->mix.store (std::clamp (x.value ("mix", 1.0f), 0.0f, 1.0f));
                m->mixAnt = m->on.load() ? m->mix.load() : 0.0f;
                m->estavaOn = m->on.load();
                preparaModul (*m);
                c->moduls.push_back (m);
                c->camins.push_back (std::clamp (x.value ("cami", 0), 0, maxRetorns));
            }
            presetNom = j.value ("preset", std::string());   // 0.79
            if (esMaster())   // 0.94
            {
                igualatAB.store (j.value ("igualat", false));   // 0.95
                objectiuLufs.store (std::clamp (j.value ("objectiu", 1), 0, (int) loudness::objectius().size() - 1));
                lufsPers.store (std::clamp (j.value ("lufsPersonal", -10.0f), -24.0f, -5.0f));   // 0.95
            }
            presetMod = j.value ("presetModificat", false);
            const auto rets = j.value ("retorns", nlohmann::json::array());   // 0.76
            for (int k = 0; k < maxRetorns; ++k)
            {
                float mn, mx, def;
                for (int q = 0; q < 4; ++q) { const int p = Envia1 + q * maxRetorns + k; rangParam (p, mn, mx, def); par[(size_t) p].store (def); }
                if (k >= (int) rets.size() || ! rets[(size_t) k].is_object()) continue;
                const auto& x = rets[(size_t) k];
                c->retorns[(size_t) k].nom = x.value ("nom", std::string());
                c->retorns[(size_t) k].actiu = x.value ("actiu", false);
                posaValor (Envia1 + k, x.value ("envia", 0.0f));
                posaValor (Retorn1 + k, x.value ("retorn", -12.0f));
                posaValor (PanRetorn1 + k, x.value ("pan", 0.0f));
                posaValor (Ducking1 + k, x.value ("ducking", 0.0f));
            }
            for (const auto& x : j.value ("enllacos", nlohmann::json::array()))
            {
                if (! x.is_array() || x.size() < 5 || (int) c->enllacos.size() >= maxEnllacos) continue;
                Enllac e { x[0].get<int>(), x[1].get<int>(), x[2].get<int>(), x[3].get<float>(), x[4].get<float>() };
                if (e.macro < 0 || e.macro >= numMacros) continue;
                if (e.modul == -1) { if (e.param < Envia1 || e.param >= numParams) continue; }   // 0.76
                else
                {
                    if (e.modul < 0 || e.modul >= (int) c->moduls.size()) continue;
                    if (e.param < 0 || e.param >= c->moduls[(size_t) e.modul]->fx->numParametres()) continue;
                }
                e.mn = std::clamp (e.mn, 0.0f, 1.0f); e.mx = std::clamp (e.mx, 0.0f, 1.0f);
                c->enllacos.push_back (e);
            }
            par[Entrada].store (std::clamp (j.value ("entrada", 0.0f), -maxDb, maxDb));
            par[Sortida].store (std::clamp (j.value ("sortida", 0.0f), -maxDb, maxDb));
            const auto mac = j.value ("macros", nlohmann::json::array());
            for (int m = 0; m < numMacros; ++m)
            {
                if (m < (int) mac.size() && mac[(size_t) m].is_object())
                {
                    noms[(size_t) m] = mac[(size_t) m].value ("nom", noms[(size_t) m]);
                    par[(size_t) (Macro1 + m)].store (std::clamp (mac[(size_t) m].value ("valor", 0.0f), 0.0f, 1.0f));
                }
            }
            publica (c);
        }
        catch (...) {}
    }
}
