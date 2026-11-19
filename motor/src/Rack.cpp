#include "mibase/Rack.h"
#include "mibase/Delay.h"
#include "mibase/Doubler.h"
#include "mibase/Drive.h"
#include "mibase/Equalitzador.h"
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
            { "delay", delay::idPlugin, "Delay", "Ecos al tempo de la cançó." },
            { "reverb", reverb::idPlugin, "Reverb", "L'espai: habitació, plate, sala…" },
            { "limitador", limitador::idPlugin, "Limitador", "Al final de tot: cap pic passa del sostre (5 ms de retard)." },
        };
        return t;
    }

    const TipusModul* busca (const std::string& clau)
    {
        for (const auto& t : tipus()) if (clau == t.clau) return &t;
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

    Rack::Rack()
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (0.0f);
        static const char* const nomsDefecte[numMacros] = { "Cos", "Presència", "Aire", "Compressió", "Escalfor", "Doble", "Espai", "Eco" };
        for (int m = 0; m < numMacros; ++m) noms[(size_t) m] = nomsDefecte[m];
        publica (std::make_shared<Cadena>());
    }

    Rack::~Rack() { actual.store (nullptr); }

    void Rack::prepara (double f, int bloc)
    {
        fs = f > 1000.0 ? f : 48000.0;
        maxBloc = std::max (16, bloc);
        preparat = true;
        for (auto& m : ui->moduls) preparaModul (*m);
        primerBloc = true;
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
        int r = 0;
        for (const auto& m : c->moduls)
            if (m->on.load (std::memory_order_relaxed)) r += std::max (0, m->fx->latencia());
        return r;
    }

    void Rack::processa (float* L, float* R, int n) noexcept
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
            for (auto& mp : c->moduls)
            {
                Modul& m = *mp;
                const bool on = m.on.load (std::memory_order_relaxed);
                float objectiu = on ? std::clamp (m.mix.load (std::memory_order_relaxed), 0.0f, 1.0f) : 0.0f;
                if (on && m.fx->latencia() > 0) objectiu = 1.0f;   // desfasat: sense barreja
                if (objectiu <= 0.0f && m.mixAnt <= 0.0f)
                {
                    if (m.estavaOn) { m.fx->buida(); m.estavaOn = false; }   // apagat del tot: la cua vella, fora
                    continue;
                }
                m.estavaOn = true;
                const int cap = (int) m.secL.size();
                if (objectiu >= 0.9999f && m.mixAnt >= 0.9999f) { m.fx->processa (L, R, n); m.mixAnt = 1.0f; continue; }
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

    // ---- paràmetres ----
    InfoParametre Rack::infoParametre (int i) const
    {
        InfoParametre r;
        if (i == Entrada) { r.id = "entrada"; r.nom = "Entrada"; }
        else if (i == Sortida) { r.id = "sortida"; r.nom = "Sortida"; }
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
        const float vell = par[(size_t) i].exchange (v01, std::memory_order_relaxed);
        if (std::abs (vell - v01) < 1.0e-7f) return;
        if (const Cadena* c = actual.load (std::memory_order_acquire)) aplicaMacro (*c, i - Macro1, v01);
    }

    float Rack::parametre (int i) const noexcept
    {
        if (i < 0 || i >= numParams) return 0.0f;
        const float v = par[(size_t) i].load (std::memory_order_relaxed);
        return (i == Entrada || i == Sortida) ? (v + maxDb) / (2.0f * maxDb) : v;
    }

    std::string Rack::textParametre (int i, float v01) const
    {
        char b[40];
        if (i == Entrada || i == Sortida) std::snprintf (b, sizeof b, "%+.1f dB", (double) (-maxDb + 2.0f * maxDb * v01));
        else std::snprintf (b, sizeof b, "%.1f", (double) (v01 * 10.0f));
        std::string s (b);
        for (auto& ch : s) if (ch == '.') ch = ',';
        return s;
    }

    void Rack::aplicaMacro (const Cadena& c, int m, float v) noexcept
    {
        for (const auto& e : c.enllacos)
        {
            if (e.macro != m || e.modul < 0 || e.modul >= (int) c.moduls.size()) continue;
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

    int Rack::afegeix (const std::string& clau, int pos)
    {
        return afegeix (clau, creaModul (clau, tempo), pos);
    }

    int Rack::afegeix (const std::string& clau, std::shared_ptr<EfecteIntern> fx, int pos)
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
        for (auto& e : c->enllacos) if (e.modul >= pos) ++e.modul;
        publica (c);
        return pos;
    }

    void Rack::treu (int i)
    {
        if (i < 0 || i >= numModuls()) return;
        auto c = copia();
        c->moduls.erase (c->moduls.begin() + i);
        std::vector<Enllac> v;
        for (auto e : c->enllacos)
        {
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
        c->moduls.erase (c->moduls.begin() + de);
        c->moduls.insert (c->moduls.begin() + a, m);
        // on va a parar cada índex vell
        for (auto& e : c->enllacos)
        {
            if (e.modul == de) e.modul = a;
            else if (de < a && e.modul > de && e.modul <= a) --e.modul;
            else if (a < de && e.modul >= a && e.modul < de) ++e.modul;
        }
        publica (c);
    }

    void Rack::posaOn (int i, bool on) { if (auto* m = modul (i)) { m->on.store (on); ++canvis; } }
    void Rack::posaMix (int i, float v) { if (auto* m = modul (i)) { m->mix.store (std::clamp (v, 0.0f, 1.0f)); ++canvis; } }

    bool Rack::enllaca (const Enllac& e)
    {
        if (e.macro < 0 || e.macro >= numMacros || modul (e.modul) == nullptr) return false;
        if (e.param < 0 || e.param >= modul (e.modul)->fx->numParametres()) return false;
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
    }

    // ---- estat (JSON) ----
    std::string Rack::estat() const
    {
        nlohmann::ordered_json j;
        j["mibase-rack"] = 1;
        j["entrada"] = par[Entrada].load();
        j["sortida"] = par[Sortida].load();
        auto mac = nlohmann::ordered_json::array();
        for (int m = 0; m < numMacros; ++m) mac.push_back ({ { "nom", noms[(size_t) m] }, { "valor", par[(size_t) (Macro1 + m)].load() } });
        j["macros"] = mac;
        auto mods = nlohmann::ordered_json::array();
        for (const auto& m : ui->moduls)
            mods.push_back ({ { "clau", m->clau }, { "on", m->on.load() }, { "mix", m->mix.load() }, { "estat", m->fx->estat() } });
        j["moduls"] = mods;
        auto en = nlohmann::ordered_json::array();
        for (const auto& e : ui->enllacos) en.push_back ({ e.macro, e.modul, e.param, e.mn, e.mx });
        j["enllacos"] = en;
        return j.dump();
    }

    void Rack::posaEstat (const std::string& s)
    {
        if (s.empty()) { posaEstat (estatInicial()); return; }
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
            }
            for (const auto& x : j.value ("enllacos", nlohmann::json::array()))
            {
                if (! x.is_array() || x.size() < 5 || (int) c->enllacos.size() >= maxEnllacos) continue;
                Enllac e { x[0].get<int>(), x[1].get<int>(), x[2].get<int>(), x[3].get<float>(), x[4].get<float>() };
                if (e.macro < 0 || e.macro >= numMacros || e.modul < 0 || e.modul >= (int) c->moduls.size()) continue;
                if (e.param < 0 || e.param >= c->moduls[(size_t) e.modul]->fx->numParametres()) continue;
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
