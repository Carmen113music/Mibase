#include <mibase/Bateria.h>
#include <mibase/Midi.h>
#include <mibase/ProjecteDisc.h>
#include <mibase/Wav.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace mibase::bateria
{
    // =====================================================================
    // Tipus
    // =====================================================================
    const std::vector<Tipus>& tipus()
    {
        // L'ordre és el de les files del Drumpad (el mateix que a Riffs).
        static const std::vector<Tipus> t = {
            { "kick", "Bombo", 36, 0 },
            { "snare", "Caixa", 38, 0 },
            { "rim", "Rimshot", 37, 0 },
            { "clap", "Palmes", 39, 0 },
            { "hihat", "Hihat tancat", 42, 1 },
            { "hihat-obert", "Hihat obert", 46, 1 },
            { "hihat-pedal", "Hihat pedal", 44, 1 },
            { "tom-agut", "Tom agut", 50, 0 },
            { "tom-mig", "Tom mig", 47, 0 },
            { "tom-greu", "Tom greu", 43, 0 },
            { "tom", "Tom", 45, 0 },
            { "ride", "Ride", 51, 0 },
            { "crash", "Crash", 49, 0 },
            { "perc", "Percussió", 56, 0 },
            { "altres", "Altres", 60, 0 },
        };
        return t;
    }

    const Tipus* tipusPerId (const std::string& id)
    {
        for (auto& t : tipus()) if (id == t.id) return &t;
        return nullptr;
    }
    std::string nomTipus (const std::string& id) { auto* t = tipusPerId (id); return t != nullptr ? t->nom : "Altres"; }
    int notaDeTipus (const std::string& id) { auto* t = tipusPerId (id); return t != nullptr ? t->nota : 60; }

    // =====================================================================
    // Classificació pel nom (com a Riffs)
    // =====================================================================
    namespace
    {
        std::vector<std::string> paraules (const std::string& nom)
        {
            // Minúscules; separa per tot el que no és lletra o xifra, i també entre lletres i xifres («kick01» → kick, 01).
            std::vector<std::string> w;
            std::string ara;
            int ultim = 0;   // 1 lletra, 2 xifra
            for (unsigned char ch : nom)
            {
                const int tipusCar = std::isalpha (ch) ? 1 : std::isdigit (ch) ? 2 : (ch >= 0x80 ? 1 : 0);
                if (tipusCar == 0 || (ultim != 0 && tipusCar != ultim))
                {
                    if (! ara.empty()) w.push_back (ara);
                    ara.clear();
                }
                if (tipusCar != 0) ara += (char) std::tolower (ch);
                ultim = tipusCar;
            }
            if (! ara.empty()) w.push_back (ara);
            return w;
        }
        bool te (const std::vector<std::string>& w, std::initializer_list<const char*> l)
        {
            for (auto& x : w) for (auto* y : l) if (x == y) return true;
            return false;
        }
        bool conte (const std::string& j, std::initializer_list<const char*> l)
        {
            for (auto* y : l) if (j.find (y) != std::string::npos) return true;
            return false;
        }
    }

    std::string classificaNom (const std::string& nomFitxer)
    {
        std::string base = nomFitxer;
        if (auto p = base.find_last_of ("/\\"); p != std::string::npos) base = base.substr (p + 1);
        if (auto p = base.find_last_of ('.'); p != std::string::npos) base = base.substr (0, p);
        const auto w = paraules (base);
        std::string j;
        for (auto& x : w) j += x;

        if (te (w, { "clap", "claps", "handclap", "palmes", "palma", "palmas" }) || conte (j, { "handclap" })) return "clap";
        if (conte (j, { "kick", "bassdrum", "bombo" }) || te (w, { "bd", "kik", "kck", "bass" })) return "kick";
        if (conte (j, { "rimshot", "sidestick", "crossstick" }) || te (w, { "rim", "rs", "stick" })) return "rim";
        if (conte (j, { "snare", "caixa", "caja" }) || te (w, { "sd", "snr", "sn" })) return "snare";
        const bool hat = conte (j, { "hihat", "hat", "charles", "charleston" }) || te (w, { "hh", "chh", "ohh", "phh", "ch", "oh" });
        if (hat)
        {
            if (te (w, { "open", "opened", "obert", "abierto", "ohh", "oh" }) || conte (j, { "openhat", "hatopen" })) return "hihat-obert";
            if (te (w, { "pedal", "foot", "phh", "peu" })) return "hihat-pedal";
            return "hihat";
        }
        if (conte (j, { "ride" })) return "ride";
        if (conte (j, { "crash", "china", "splash", "cymbal", "plat" }) || te (w, { "cym" })) return "crash";
        if (conte (j, { "tom" }))
        {
            if (te (w, { "floor", "low", "lo", "greu", "grave", "baix", "l", "ft" })) return "tom-greu";
            if (te (w, { "mid", "middle", "mig", "medio", "m" })) return "tom-mig";
            if (te (w, { "high", "hi", "agut", "alto", "rack", "h" })) return "tom-agut";
            return "tom";
        }
        if (conte (j, { "cowbell", "shaker", "tamb", "conga", "bongo", "perc", "clave", "cabasa", "guiro", "triangle", "agogo",
                        "timbal", "maracas", "block", "esquella" }))
            return "perc";
        return {};
    }

    // =====================================================================
    // Classificació pel so
    // =====================================================================
    std::string classificaSo (const float* x, int n, double fs)
    {
        if (x == nullptr || n <= 0 || fs <= 0.0) return "perc";
        float pic = 0.0f;
        for (int i = 0; i < n; ++i) pic = std::max (pic, std::abs (x[i]));
        if (pic < 1.0e-5f) return "altres";

        // Llargada útil: fins on el so cau per sota de -40 dB del pic (en finestres de 10 ms).
        const int fin = std::max (1, (int) (fs * 0.01));
        int ultima = 0;
        for (int i = 0; i < n; i += fin)
        {
            float m = 0.0f;
            for (int k = i; k < std::min (n, i + fin); ++k) m = std::max (m, std::abs (x[k]));
            if (m > pic * 0.01f) ultima = std::min (n, i + fin);
        }
        const double durada = ultima / fs;

        // Energia en greus (< ~150 Hz), aguts (> ~5 kHz) i el total; passos per zero (soroll).
        const float aG = (float) (1.0 - std::exp (-2.0 * 3.14159265 * 150.0 / fs));
        const float aA = (float) (1.0 - std::exp (-2.0 * 3.14159265 * 5000.0 / fs));
        float lp = 0.0f, lpA = 0.0f;
        double eT = 0, eG = 0, eA = 0;
        int zeros = 0;
        const int m = std::max (1, ultima);
        for (int i = 0; i < m; ++i)
        {
            lp += aG * (x[i] - lp);
            lpA += aA * (x[i] - lpA);
            const float hp = x[i] - lpA;
            eT += (double) x[i] * x[i];
            eG += (double) lp * lp;
            eA += (double) hp * hp;
            if (i > 0 && ((x[i] >= 0.0f) != (x[i - 1] >= 0.0f))) ++zeros;
        }
        if (eT <= 0.0) return "altres";
        const double fG = eG / eT, fA = eA / eT;
        const double zcr = zeros / (m / fs);   // passos per zero per segon

        if (zcr > 15000.0 || fA > 0.33)   // soroll brillant: hihats i plats
        {
            if (durada > 1.9) return "crash";
            if (durada > 0.9) return fA > 0.3 ? "crash" : "ride";
            if (durada > 0.2) return "hihat-obert";
            return "hihat";
        }
        if (fG > 0.7 || (fG > 0.45 && zcr < 180.0)) return "kick";
        if (zcr > 6000.0)
        {
            if (durada > 1.0) return "ride";
            return fG < 0.05 ? "clap" : "snare";
        }
        if (zcr < 1000.0 && fG > 0.15) return "tom";
        if (durada < 0.08) return "rim";
        return "perc";
    }

    // =====================================================================
    // Kits al disc
    // =====================================================================
    namespace
    {
        bool esAudio (const fs::path& p)
        {
            auto e = disc::text (p.extension());
            std::transform (e.begin(), e.end(), e.begin(), [] (unsigned char c) { return (char) std::tolower (c); });
            return e == ".wav";
        }
    }

    std::optional<Kit> llegeixKit (const std::string& carpeta)
    {
        std::error_code ec;
        const auto dir = disc::cami (carpeta);
        if (! fs::is_directory (dir, ec)) return std::nullopt;
        Kit k;
        k.carpeta = carpeta;
        k.nom = disc::text (dir.filename());
        const auto fitxer = dir / "kit.json";
        if (fs::exists (fitxer, ec))
        {
            try
            {
                std::ifstream in (fitxer, std::ios::binary);
                const auto j = nlohmann::json::parse (in);
                k.nom = j.value ("nom", k.nom);
                if (j.contains ("sons") && j["sons"].is_array())
                    for (auto& s : j["sons"])
                    {
                        SoKit x;
                        x.fitxer = s.value ("fitxer", std::string());
                        x.nom = s.value ("nom", x.fitxer);
                        x.tipus = s.value ("tipus", std::string ("altres"));
                        x.original = s.value ("original", std::string());
                        if (! x.fitxer.empty() && fs::exists (dir / disc::cami (x.fitxer), ec)) k.sons.push_back (x);
                    }
                return k;
            }
            catch (...) {}
        }
        for (auto& e : fs::directory_iterator (dir, ec))
        {
            if (! e.is_regular_file (ec) || ! esAudio (e.path())) continue;
            SoKit x;
            x.fitxer = disc::text (e.path().filename());
            x.nom = disc::text (e.path().stem());
            x.tipus = classificaNom (x.fitxer);
            if (x.tipus.empty()) x.tipus = "altres";
            k.sons.push_back (x);
        }
        if (k.sons.empty()) return std::nullopt;
        std::sort (k.sons.begin(), k.sons.end(), [] (const SoKit& a, const SoKit& b) { return a.fitxer < b.fitxer; });
        return k;
    }

    bool desaKit (const Kit& k)
    {
        try
        {
            nlohmann::ordered_json j;
            j["nom"] = k.nom;
            auto sons = nlohmann::ordered_json::array();
            for (auto& s : k.sons)
                sons.push_back ({ { "fitxer", s.fitxer }, { "nom", s.nom }, { "tipus", s.tipus }, { "original", s.original } });
            j["sons"] = sons;
            std::ofstream out (disc::cami (k.carpeta) / "kit.json", std::ios::binary);
            out << j.dump (2);
            return (bool) out;
        }
        catch (...) { return false; }
    }

    std::vector<Kit> llegeixKits (const std::string& arrel)
    {
        std::vector<Kit> v;
        std::error_code ec;
        const auto dir = disc::cami (arrel);
        if (! fs::is_directory (dir, ec)) return v;
        for (auto& e : fs::directory_iterator (dir, ec))
            if (e.is_directory (ec))
                if (auto k = llegeixKit (disc::text (e.path())); k && ! k->sons.empty()) v.push_back (*k);
        std::sort (v.begin(), v.end(), [] (const Kit& a, const Kit& b) { return a.nom < b.nom; });
        return v;
    }

    std::string camiSo (const Kit& k, const SoKit& s)
    {
        return disc::text (disc::cami (k.carpeta).filename()) + "/" + s.fitxer;
    }

    std::vector<FilaBateria> filesDeKit (const Kit& k, const std::vector<FilaBateria>& actuals)
    {
        std::vector<FilaBateria> v = actuals;
        for (auto& t : tipus())
        {
            const SoKit* so = nullptr;
            for (auto& s : k.sons) if (s.tipus == t.id) { so = &s; break; }
            if (so == nullptr) continue;
            auto it = std::find_if (v.begin(), v.end(), [&] (const FilaBateria& f) { return f.tipus == t.id; });
            if (it != v.end()) { it->so = camiSo (k, *so); continue; }
            FilaBateria f;
            f.tipus = t.id;
            f.nom = t.nom;
            f.nota = t.nota;
            f.talla = t.talla;
            f.so = camiSo (k, *so);
            v.push_back (f);
        }
        // Per l'ordre dels tipus; dins el mateix tipus, com estaven.
        auto ordre = [] (const std::string& id) { for (size_t i = 0; i < tipus().size(); ++i) if (id == tipus()[i].id) return (int) i; return 99; };
        std::stable_sort (v.begin(), v.end(), [&] (const FilaBateria& a, const FilaBateria& b) { return ordre (a.tipus) < ordre (b.tipus); });
        return v;
    }

    std::vector<FilaBateria> filesInternes()
    {
        std::vector<FilaBateria> v;
        for (auto& t : tipus())
        {
            if (std::string (t.id) == "altres" || std::string (t.id) == "tom") continue;
            FilaBateria f;
            f.tipus = t.id;
            f.nom = t.nom;
            f.nota = t.nota;
            f.talla = t.talla;
            f.so = std::string ("intern:") + t.id;
            v.push_back (f);
        }
        return v;
    }

    // =====================================================================
    // Kit intern (sintetitzat)
    // =====================================================================
    namespace
    {
        struct Soroll
        {
            std::uint32_t s = 0x12345678u;
            float operator()() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) ((double) s / 4294967295.0 * 2.0 - 1.0); }
        };

        std::shared_ptr<Mostra> sintetitza (const std::string& t, double fs)
        {
            auto m = std::make_shared<Mostra>();
            m->fs = fs;
            const double pi2 = 2.0 * 3.14159265358979;
            Soroll rnd;
            auto mida = [&] (double s) { m->L.assign ((size_t) (s * fs), 0.0f); };
            auto exp_ = [] (double t, double tau) { return std::exp (-t / tau); };
            if (t == "kick")
            {
                mida (0.5);
                double fase = 0.0;
                for (size_t i = 0; i < m->L.size(); ++i)
                {
                    const double s = i / fs;
                    const double f = 48.0 + 110.0 * exp_ (s, 0.035);
                    fase += pi2 * f / fs;
                    m->L[i] = (float) (0.95 * std::sin (fase) * exp_ (s, 0.22) + 0.25 * rnd() * exp_ (s, 0.003));
                }
            }
            else if (t == "snare" || t == "rim" || t == "clap")
            {
                const bool rim = t == "rim", clap = t == "clap";
                mida (rim ? 0.08 : clap ? 0.35 : 0.3);
                double fase = 0.0;
                float bp1 = 0, bp2 = 0;
                for (size_t i = 0; i < m->L.size(); ++i)
                {
                    const double s = i / fs;
                    fase += pi2 * (rim ? 420.0 : 185.0) / fs;
                    float r = rnd();
                    // passa-banda senzill (dos filtres d'un pol)
                    bp1 += 0.35f * (r - bp1);
                    bp2 += 0.08f * (bp1 - bp2);
                    float soroll = bp1 - bp2;
                    double env;
                    if (clap)
                    {
                        const double cops[] = { 0.0, 0.011, 0.023 };
                        env = 0.0;
                        for (double c : cops) if (s >= c) env = std::max (env, exp_ (s - c, 0.008));
                        env = std::max (env, s >= 0.023 ? 0.7 * exp_ (s - 0.023, 0.11) : 0.0);
                        m->L[i] = (float) (1.8 * soroll * env);
                    }
                    else if (rim)
                        m->L[i] = (float) (0.8 * std::sin (fase) * exp_ (s, 0.012) + 0.9 * soroll * exp_ (s, 0.006));
                    else
                        m->L[i] = (float) (0.55 * std::sin (fase) * exp_ (s, 0.06) + 1.4 * soroll * exp_ (s, 0.075));
                }
            }
            else if (t.rfind ("hihat", 0) == 0 || t == "crash" || t == "ride")
            {
                const double llarg = t == "hihat" ? 0.12 : t == "hihat-pedal" ? 0.1 : t == "hihat-obert" ? 0.7 : t == "ride" ? 1.8 : 2.2;
                const double tau = t == "hihat" ? 0.025 : t == "hihat-pedal" ? 0.02 : t == "hihat-obert" ? 0.18 : t == "ride" ? 0.55 : 0.6;
                mida (llarg);
                float lp = 0;
                double f1 = 0, f2 = 0;
                for (size_t i = 0; i < m->L.size(); ++i)
                {
                    const double s = i / fs;
                    float r = rnd();
                    lp += 0.45f * (r - lp);
                    const float hp = r - lp;   // aguts
                    double v = hp * exp_ (s, tau);
                    if (t == "ride")
                    {
                        f1 += pi2 * 3150.0 / fs;
                        f2 += pi2 * 4730.0 / fs;
                        v = 0.45 * v + 0.25 * (std::sin (f1) + 0.6 * std::sin (f2)) * exp_ (s, 0.7);
                    }
                    if (t == "hihat-pedal") v *= 0.6;
                    m->L[i] = (float) (0.9 * v * std::min (1.0, s / 0.0008));
                }
            }
            else if (t.rfind ("tom", 0) == 0)
            {
                const double f0 = t == "tom-agut" ? 210.0 : t == "tom-mig" ? 160.0 : t == "tom-greu" ? 105.0 : 140.0;
                mida (0.6);
                double fase = 0.0;
                for (size_t i = 0; i < m->L.size(); ++i)
                {
                    const double s = i / fs;
                    fase += pi2 * f0 * (1.0 + 0.6 * exp_ (s, 0.04)) / fs;
                    m->L[i] = (float) (0.9 * std::sin (fase) * exp_ (s, 0.2) + 0.15 * rnd() * exp_ (s, 0.004));
                }
            }
            else if (t == "perc")
            {
                mida (0.35);
                for (size_t i = 0; i < m->L.size(); ++i)
                {
                    const double s = i / fs;
                    const double a = std::sin (pi2 * 545.0 * s) > 0 ? 1.0 : -1.0;
                    const double b = std::sin (pi2 * 815.0 * s) > 0 ? 1.0 : -1.0;
                    m->L[i] = (float) (0.3 * (a + b) * exp_ (s, 0.08));
                }
            }
            else return nullptr;
            return m;
        }
    }

    std::shared_ptr<const Mostra> mostraInterna (const std::string& so, double fs)
    {
        if (so.rfind ("intern:", 0) != 0) return nullptr;
        static std::mutex mtx;
        static std::map<std::pair<std::string, int>, std::shared_ptr<const Mostra>> cau;
        const std::lock_guard<std::mutex> l (mtx);
        auto clau = std::make_pair (so, (int) fs);
        if (auto it = cau.find (clau); it != cau.end()) return it->second;
        auto m = sintetitza (so.substr (7), fs);
        cau[clau] = m;
        return m;
    }

    // =====================================================================
    // Sampler
    // =====================================================================
    SamplerBateria::SamplerBateria (std::vector<So> sons)
    {
        for (auto& s : sons)
            if (s.nota >= 0 && s.nota < 128) mostres[(size_t) s.nota] = s.mostra;
    }

    void SamplerBateria::posaParams (const FilaBateria& f) noexcept
    {
        if (f.nota < 0 || f.nota > 127) return;
        auto& p = params[(size_t) f.nota];
        p.guany.store ((float) std::pow (10.0, f.volumDb / 20.0));
        p.pan.store ((float) f.pan);
        p.afina.store ((float) f.afina);
        p.decai.store ((float) f.decai);
        p.talla.store (f.talla);
        p.mut.store (f.mut);
    }

    void SamplerBateria::prepara (double fs, int) { fsSortida = fs > 0 ? fs : 48000.0; }

    void SamplerBateria::apagaTot() noexcept
    {
        for (auto& v : veus)
            if (v.nota >= 0) v.fadePas = std::max (v.fadePas, 1.0f / (float) (0.01 * fsSortida));
    }

    void SamplerBateria::dispara (int nota, int vel) noexcept
    {
        const auto* m = mostres[(size_t) nota].get();
        auto& p = params[(size_t) nota];
        if (m == nullptr || m->L.empty() || p.mut.load()) return;
        const int talla = p.talla.load();
        if (talla > 0)
            for (auto& v : veus)
                if (v.nota >= 0 && v.talla == talla)
                    v.fadePas = std::max (v.fadePas, 1.0f / (float) (0.005 * fsSortida));
        // Una veu lliure; si no n'hi ha, la que fa menys so.
        Veu* lliure = nullptr;
        for (auto& v : veus) if (v.nota < 0) { lliure = &v; break; }
        if (lliure == nullptr)
        {
            lliure = &veus[0];
            for (auto& v : veus) if (v.env * v.fade < lliure->env * lliure->fade) lliure = &v;
        }
        else ++actives;
        auto& v = *lliure;
        v.nota = nota;
        v.m = m;
        v.pos = 0.0;
        v.pas = std::pow (2.0, p.afina.load() / 12.0) * m->fs / fsSortida;
        const float g = p.guany.load() * std::pow ((float) std::clamp (vel, 1, 127) / 127.0f, 1.4f);
        const float pan = std::clamp (p.pan.load(), -1.0f, 1.0f);
        const float a = (pan + 1.0f) * 0.25f * 3.14159265f;
        v.gL = g * std::cos (a) * 1.41421356f;
        v.gR = g * std::sin (a) * 1.41421356f;
        v.env = 1.0f;
        const float decai = p.decai.load();
        if (decai < 0.999f)
        {
            const double mostresSortida = std::max (16.0, (double) m->L.size() / v.pas * decai);
            v.mult = (float) std::pow (0.001, 1.0 / mostresSortida);
        }
        else v.mult = 1.0f;
        v.fade = 1.0f;
        v.fadePas = 0.0f;
        v.talla = talla;
    }

    void SamplerBateria::processa (const EventInstrument* ev, int num, float* L, float* R, int n) noexcept
    {
        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        int e = 0, i = 0;
        while (i < n)
        {
            while (e < num && ev[e].offset <= i)
            {
                const int tipusEv = ev[e].estat & 0xF0;
                if (tipusEv == 0x90 && ev[e].dada2 > 0) dispara (ev[e].dada1 & 127, ev[e].dada2);
                else if (tipusEv == 0xB0 && (ev[e].dada1 == 120 || ev[e].dada1 == 123)) apagaTot();
                ++e;
            }
            const int fi = e < num ? std::clamp (ev[e].offset, i + 1, n) : n;
            for (auto& v : veus)
            {
                if (v.nota < 0) continue;
                const auto& mL = v.m->L;
                const auto& mR = v.m->R.empty() ? v.m->L : v.m->R;
                const double len = (double) mL.size() - 1.0;
                for (int k = i; k < fi; ++k)
                {
                    if (v.pos >= len || v.fade <= 0.0f || v.env < 1.0e-4f) { v.nota = -1; --actives; break; }
                    const int p0 = (int) v.pos;
                    const float fr = (float) (v.pos - p0);
                    const float a = mL[(size_t) p0] + fr * (mL[(size_t) p0 + 1] - mL[(size_t) p0]);
                    const float b = mR[(size_t) p0] + fr * (mR[(size_t) p0 + 1] - mR[(size_t) p0]);
                    const float g = v.env * v.fade;
                    L[k] += a * v.gL * g;
                    R[k] += b * v.gR * g;
                    v.pos += v.pas;
                    v.env *= v.mult;
                    if (v.fadePas > 0.0f) v.fade -= v.fadePas;
                }
            }
            i = fi;
        }
    }

    std::string SamplerBateria::signatura (const Pista& t)
    {
        std::string s;
        for (auto& f : t.files) s += std::to_string (f.nota) + "=" + f.so + "|";
        return s;
    }

    // =====================================================================
    // Edició
    // =====================================================================
    namespace
    {
        struct Atzar
        {
            std::uint32_t s;
            explicit Atzar (std::uint32_t llavor) : s (llavor * 2654435761u + 0x9E3779B9u) { if (s == 0) s = 1; }
            double operator()() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (double) s / 4294967295.0 * 2.0 - 1.0; }   // -1..1
        };
        std::vector<int> totesSiBuit (const Clip& c, const std::vector<int>& idx)
        {
            if (! idx.empty()) return idx;
            std::vector<int> v ((size_t) c.notes.size());
            for (size_t i = 0; i < v.size(); ++i) v[i] = (int) i;
            return v;
        }
        long long modul (long long a, long long b) { return b <= 0 ? 0 : ((a % b) + b) % b; }
    }

    void humanitza (Clip& c, std::vector<int>& idx, double msTemps, double pctVel, std::uint32_t llavor, bool fixaPrimer,
                    double tempo, long long ticksCompas, long long desplTicks)
    {
        const bool totes = idx.empty();
        auto sel = totesSiBuit (c, idx);
        Atzar rnd (llavor);
        const double ticksPerSeg = ticksPerNegra * tempo / 60.0;
        for (int i : sel)
        {
            if (i < 0 || i >= (int) c.notes.size()) continue;
            auto& n = c.notes[(size_t) i];
            const double rt = rnd(), rv = rnd();
            const long long r = modul (desplTicks + n.ini, ticksCompas), tol = ticksPerNegra / 32;
            const bool primer = fixaPrimer && ticksCompas > 0 && (r <= tol || ticksCompas - r <= tol);
            if (! primer) n.ini = std::max (0LL, n.ini + (long long) std::llround (rt * msTemps / 1000.0 * ticksPerSeg));
            n.vel = std::clamp ((int) std::lround (n.vel * (1.0 + rv * pctVel / 100.0)), 1, 127);
        }
        midi::ordena (c.notes, sel);
        if (! totes) idx = sel;
    }

    void swing (Clip& c, std::vector<int>& idx, double pct, long long pas, long long desplTicks)
    {
        if (pas <= 0) return;
        const bool totes = idx.empty();
        auto sel = totesSiBuit (c, idx);
        const double despl = (std::clamp (pct, 50.0, 75.0) / 50.0 - 1.0) * (double) pas;
        for (int i : sel)
        {
            if (i < 0 || i >= (int) c.notes.size()) continue;
            auto& n = c.notes[(size_t) i];
            const long long a = desplTicks + n.ini;
            const long long k = (long long) std::floor ((double) a / (double) pas + 0.5);
            if (std::abs ((double) (a - k * pas)) > (double) pas / 6.0 + 0.5) continue;   // no és a la graella
            if (modul (k, 2) != 1) continue;
            n.ini = std::max (0LL, k * pas + (long long) std::llround (despl) - desplTicks);
        }
        midi::ordena (c.notes, sel);
        if (! totes) idx = sel;
    }

    void redoble (Clip& c, std::vector<int>& idx, int cops, long long pas, double caiguda)
    {
        cops = std::clamp (cops, 2, 8);
        auto sel = totesSiBuit (c, idx);
        std::sort (sel.begin(), sel.end());
        std::vector<Nota> noves;
        for (auto it = sel.rbegin(); it != sel.rend(); ++it)
        {
            if (*it < 0 || *it >= (int) c.notes.size()) continue;
            const auto n = c.notes[(size_t) *it];
            const long long casella = pas > 0 ? pas : n.dur;
            const long long sub = std::max (1LL, casella / cops);
            for (int j = 0; j < cops; ++j)
            {
                Nota x = n;
                x.ini = n.ini + j * sub;
                x.dur = sub;
                const double f = 1.0 - std::clamp (caiguda, 0.0, 1.0) * (double) j / (double) (cops - 1);
                x.vel = std::clamp ((int) std::lround (n.vel * f), 1, 127);
                noves.push_back (x);
            }
            c.notes.erase (c.notes.begin() + *it);
        }
        std::vector<int> nou;
        for (auto& x : noves) { c.notes.push_back (x); nou.push_back ((int) c.notes.size() - 1); }
        midi::ordena (c.notes, nou);
        idx = nou;
    }

    void flam (Clip& c, std::vector<int>& idx, long long avanc, double velPct)
    {
        auto sel = totesSiBuit (c, idx);
        std::vector<Nota> gracies;
        for (int i : sel)
        {
            if (i < 0 || i >= (int) c.notes.size()) continue;
            const auto& n = c.notes[(size_t) i];
            if (n.ini - avanc < 0) continue;
            Nota g = n;
            g.ini = n.ini - avanc;
            g.dur = std::max (1LL, std::min (avanc, n.dur));
            g.vel = std::clamp ((int) std::lround (n.vel * std::clamp (velPct, 0.05, 1.0)), 1, 127);
            gracies.push_back (g);
        }
        for (auto& g : gracies) { c.notes.push_back (g); sel.push_back ((int) c.notes.size() - 1); }
        midi::ordena (c.notes, sel);
        idx = sel;
    }

    // =====================================================================
    // Ritmes de partida
    // =====================================================================
    const std::vector<Ritme>& ritmes()
    {
        static const std::vector<Ritme> r = {
            { "Rock bàsic", "Rock i pop", 4, 4, 16, { { "kick", "X-------X-x-----" }, { "snare", "----X-------X---" }, { "hihat", "x-x-x-x-x-x-x-x-" } } },
            { "Rock amb ride", "Rock i pop", 4, 4, 16, { { "kick", "X-----x-X-x-----" }, { "snare", "----X-------X---" }, { "ride", "x-x-x-x-x-x-x-x-" } } },
            { "Pop", "Rock i pop", 4, 4, 16, { { "kick", "X-----x-X-------" }, { "snare", "----X-------X---" }, { "hihat", "xox-xox-xox-xox-" } } },
            { "Pop amb semicorxeres", "Rock i pop", 4, 4, 16, { { "kick", "X--x--x-X-------" }, { "snare", "----X-------X---" }, { "hihat", "XoxoXoxoXoxoXoxo" } } },
            { "Indie (bombo a negres)", "Rock i pop", 4, 4, 16, { { "kick", "X---X---X---X---" }, { "snare", "----X-------X---" }, { "hihat", "x---x---x---x---" }, { "hihat-obert", "--x---x---x---x-" } } },
            { "Mig temps (half-time)", "Rock i pop", 4, 4, 16, { { "kick", "X-----x---x-----" }, { "snare", "--------X-------" }, { "hihat", "x-x-x-x-x-x-x-x-" } } },
            { "Balada", "Rock i pop", 4, 4, 16, { { "kick", "X-------X-x-----" }, { "rim", "----X-------X---" }, { "hihat", "x-x-x-x-x-x-x-x-" } } },
            { "Punk", "Rock i pop", 4, 4, 16, { { "kick", "X-x-X-x-X-x-X-x-" }, { "snare", "--X---X---X---X-" }, { "crash", "X-x-x-x-x-x-x-x-" } } },
            { "Funk 1", "Funk i soul", 4, 4, 16, { { "kick", "X-x---x---x-x---" }, { "snare", "----X--o-o--X--o" }, { "hihat", "XxoxXxoxXxoxXxox" } } },
            { "Funk 2", "Funk i soul", 4, 4, 16, { { "kick", "X--x--x---x--x--" }, { "snare", "----X-o-oo--X-o-" }, { "hihat", "xoxoxoxoxoxoxo-o" }, { "hihat-obert", "--------------x-" } } },
            { "Disco", "Funk i soul", 4, 4, 16, { { "kick", "X---X---X---X---" }, { "snare", "----X-------X---" }, { "hihat", "x-o-x-o-x-o-x-o-" }, { "hihat-obert", "--x---x---x---x-" } } },
            { "Motown", "Funk i soul", 4, 4, 16, { { "kick", "X-x-X-x-X-x-X-x-" }, { "snare", "X---X---X---X---" }, { "hihat", "x-x-x-x-x-x-x-x-" } } },
            { "Shuffle", "Funk i soul", 4, 4, 12, { { "kick", "X-----X-----" }, { "snare", "---X-----X--" }, { "hihat", "x-xx-xx-xx-x" } } },
            { "Hip-hop", "Altres", 4, 4, 16, { { "kick", "X------xX-x-----" }, { "snare", "----X-------X---" }, { "hihat", "x-x-x-x-x-x-x-x-" } } },
            { "Bossa nova", "Altres", 4, 4, 16, { { "kick", "X--xX--xX--xX--x" }, { "rim", "X--X--X---X--X--" }, { "hihat", "oooooooooooooooo" } } },
            { "Reggae (one drop)", "Altres", 4, 4, 16, { { "kick", "--------X-------" }, { "rim", "--------X-------" }, { "hihat", "x-x-x-x-x-x-x-x-" } } },
            { "Vals (3/4)", "Altres", 3, 4, 12, { { "kick", "X-----------" }, { "snare", "----o---o---" }, { "hihat", "x---x---x---" } } },
            { "6/8", "Altres", 6, 8, 12, { { "kick", "X---------x-" }, { "snare", "------X-----" }, { "hihat", "x-x-x-x-x-x-" } } },
        };
        return r;
    }

    namespace
    {
        int notaFila (const std::vector<FilaBateria>& files, const std::string& t)
        {
            for (auto& f : files) if (f.tipus == t) return f.nota;
            // Si la pista no en té: un tipus semblant que sí que hi sigui.
            auto alt = [&] (std::initializer_list<const char*> l) -> int
            {
                for (auto* x : l) for (auto& f : files) if (f.tipus == x) return f.nota;
                return -1;
            };
            int n = -1;
            if (t.rfind ("tom", 0) == 0) n = alt ({ "tom", "tom-mig", "tom-agut", "tom-greu", "snare" });
            else if (t == "rim" || t == "clap") n = alt ({ "snare" });
            else if (t == "ride" || t == "hihat-obert" || t == "hihat-pedal") n = alt ({ "hihat" });
            else if (t == "crash") n = alt ({ "ride", "hihat-obert" });
            return n >= 0 ? n : (files.empty() ? notaDeTipus (t) : -1);
        }
        int velCar (char ch) { return ch == 'X' ? 118 : ch == 'x' ? 92 : ch == 'o' ? 52 : 0; }
    }

    std::vector<Nota> notesDeRitme (const Ritme& r, int compassos, long long ticksCompas, const std::vector<FilaBateria>& files, long long iniTicks)
    {
        std::vector<Nota> v;
        if (r.caselles <= 0 || ticksCompas <= 0) return v;
        for (int b = 0; b < compassos; ++b)
            for (auto& [t, patro] : r.files)
            {
                const int nota = notaFila (files, t);
                if (nota < 0) continue;
                const std::string p = patro;
                for (int k = 0; k < r.caselles && k < (int) p.size(); ++k)
                {
                    const int vel = velCar (p[(size_t) k]);
                    if (vel == 0) continue;
                    Nota n;
                    n.ini = iniTicks + b * ticksCompas + k * ticksCompas / r.caselles;
                    n.dur = std::max (1LL, ticksCompas / r.caselles);
                    n.to = nota;
                    n.vel = vel;
                    v.push_back (n);
                }
            }
        std::vector<int> cap;
        midi::ordena (v, cap);
        return v;
    }

    int fill (Clip& c, const std::vector<FilaBateria>& files, long long ticksCompas, long long ticksTemps, long long desplTicks,
              long long iniTicks, long long fiTicks, int cada, int estil, bool llarg, std::uint32_t llavor)
    {
        if (ticksCompas <= 0 || ticksTemps <= 0 || cada <= 0) return 0;
        Atzar rnd (llavor);
        const int kick = notaFila (files, "kick");
        const int caixa = notaFila (files, "snare");
        const int crash = notaFila (files, "crash");
        const int toms[] = { notaFila (files, "tom-agut"), notaFila (files, "tom-mig"), notaFila (files, "tom-greu") };
        const long long trosLlarg = llarg ? std::min (ticksCompas, 2 * ticksTemps) : ticksTemps;
        const long long pas = std::max (1LL, ticksTemps / 4);
        int posats = 0;
        // Compassos de la cançó (índex intern) que cauen dins el clip.
        const long long b0 = (long long) std::floor ((double) (desplTicks + iniTicks) / (double) ticksCompas);
        for (long long b = std::max (1LL, b0); ; ++b)
        {
            const long long iniCompas = b * ticksCompas - desplTicks;
            if (iniCompas >= fiTicks) break;
            if (b % cada != 0) continue;
            const long long fiCompas = iniCompas + ticksCompas;
            const long long a = fiCompas - trosLlarg;
            if (a < iniTicks || fiCompas > fiTicks) continue;
            // Fora el que hi havia (menys el bombo).
            c.notes.erase (std::remove_if (c.notes.begin(), c.notes.end(), [&] (const Nota& n) { return n.ini >= a && n.ini < fiCompas && n.to != kick; }),
                           c.notes.end());
            const int passos = (int) (trosLlarg / pas);
            for (int k = 0; k < passos; ++k)
            {
                if (k + 1 < passos && k > 0 && rnd() > 0.7) continue;   // algun silenci, perquè no sigui una metralleta
                int nota = caixa;
                const bool ambToms = estil == 1 || (estil == 2 && k >= passos / 2);
                if (ambToms)
                {
                    const int fase = estil == 1 ? k * 3 / passos : (k - passos / 2) * 3 / std::max (1, passos - passos / 2);
                    nota = toms[std::clamp (fase, 0, 2)];
                    if (nota < 0) nota = caixa;
                }
                if (nota < 0) continue;
                Nota n;
                n.ini = a + k * pas;
                n.dur = pas;
                n.to = nota;
                n.vel = std::clamp (70 + (int) (50.0 * k / std::max (1, passos - 1)) + (int) (rnd() * 6.0), 1, 127);
                c.notes.push_back (n);
            }
            if (fiCompas < fiTicks)
            {
                if (crash >= 0) c.notes.push_back ({ fiCompas, pas, crash, 118, 0 });
                if (kick >= 0 && std::none_of (c.notes.begin(), c.notes.end(), [&] (const Nota& n) { return n.ini == fiCompas && n.to == kick; }))
                    c.notes.push_back ({ fiCompas, pas, kick, 112, 0 });
            }
            ++posats;
        }
        std::vector<int> cap;
        midi::ordena (c.notes, cap);
        return posats;
    }
}
