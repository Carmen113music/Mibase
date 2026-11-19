#include "mibase/Drive.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>

namespace mibase::drive
{
    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c { "Crunch", "Overdrive", "Distorsió", "Fuzz", "Baix i altres" };
        return c;
    }

    namespace
    {
        // mode (0 crunch, 1 overdrive, 2 distorsió, 3 fuzz), gain, tone, level, bass, mix, gate, tight
        struct V { const char* cat; const char* nom; const char* desc; int mode; float gain, tone, level, bass, mix, gate; int tight; };
        const V taula[] = {
            { "Crunch", "Crunch net", "Gairebé net: només trenca quan toques fort.", 0, 3.0f, 5.0f, 0.0f, 0.0f, 100.0f, -80.0f, 0 },
            { "Crunch", "Crunch clàssic", "El so d'un ampli britànic al límit: acords amb pèl.", 0, 5.5f, 5.5f, 0.0f, 1.0f, 100.0f, -80.0f, 0 },
            { "Crunch", "Crunch brillant", "Per a rítmiques que han de sortir a la mescla.", 0, 5.0f, 7.5f, 0.0f, -1.0f, 100.0f, -80.0f, 0 },
            { "Crunch", "Blues", "Crunch càlid i rodó, per a solos de blues.", 0, 6.5f, 4.0f, 0.0f, 2.0f, 100.0f, -80.0f, 0 },
            { "Crunch", "Indie", "Crunch amb aguts i poc greu, per a guitarres que s'entrellacen.", 0, 4.5f, 7.0f, 0.0f, -3.0f, 100.0f, -80.0f, 1 },
            { "Overdrive", "Overdrive suau", "El pedal verd amb poc gain: empeny l'ampli.", 1, 2.0f, 5.0f, 0.0f, 0.0f, 100.0f, -80.0f, 0 },
            { "Overdrive", "Overdrive rock", "Acords de rock amb cos i sustain.", 1, 5.0f, 5.5f, 0.0f, 0.0f, 100.0f, -80.0f, 0 },
            { "Overdrive", "Overdrive de solo", "Mitjos endavant i molt sustain per cantar amb la guitarra.", 1, 7.5f, 6.0f, 0.0f, 1.0f, 100.0f, -70.0f, 0 },
            { "Overdrive", "Overdrive fosc", "Gruixut i rodó, per a melodies.", 1, 6.0f, 3.0f, 0.0f, 2.0f, 100.0f, -80.0f, 0 },
            { "Overdrive", "Pop-rock", "Overdrive moderat i brillant per a la banda.", 1, 4.0f, 6.5f, 0.0f, -1.0f, 100.0f, -80.0f, 0 },
            { "Distorsió", "Distorsió rock", "Riffs de rock: dens i potent.", 2, 5.0f, 5.0f, 0.0f, 1.0f, 100.0f, -65.0f, 0 },
            { "Distorsió", "Hard rock", "Més gain i més mitjos aguts.", 2, 7.0f, 6.0f, 0.0f, 0.0f, 100.0f, -62.0f, 1 },
            { "Distorsió", "Metal", "Molt gain, Tight i gate per a riffs ràpids.", 2, 9.0f, 6.0f, 0.0f, 2.0f, 100.0f, -55.0f, 1 },
            { "Distorsió", "Grunge", "Distorsió bruta i fosca.", 2, 7.5f, 3.5f, 0.0f, 3.0f, 100.0f, -70.0f, 0 },
            { "Distorsió", "Punk", "Distorsió directa, brillant i sense manies.", 2, 6.0f, 7.0f, 0.0f, 0.0f, 100.0f, -70.0f, 0 },
            { "Distorsió", "Solo amb distorsió", "Molt sustain i aguts suaus.", 2, 8.0f, 4.5f, 0.0f, 1.0f, 100.0f, -60.0f, 0 },
            { "Fuzz", "Fuzz anys 60", "Fuzz clàssic, quadrat i gruixut.", 3, 6.0f, 4.5f, 0.0f, 0.0f, 100.0f, -80.0f, 0 },
            { "Fuzz", "Fuzz desert", "Fuzz greu i enorme per a stoner.", 3, 8.5f, 3.0f, 0.0f, 4.0f, 100.0f, -65.0f, 0 },
            { "Fuzz", "Fuzz de solo", "Fuzz amb més aguts per als solos.", 3, 7.0f, 7.0f, 0.0f, -1.0f, 100.0f, -70.0f, 0 },
            { "Fuzz", "Fuzz trencat", "El fuzz a fons: s'ensorra.", 3, 10.0f, 5.0f, 0.0f, 0.0f, 100.0f, -80.0f, 0 },
            { "Baix i altres", "Baix amb overdrive", "Overdrive barrejat amb el so net: greus intactes.", 1, 6.0f, 5.0f, 0.0f, 2.0f, 50.0f, -80.0f, 1 },
            { "Baix i altres", "Baix fuzz", "Fuzz barrejat amb el baix net.", 3, 7.0f, 5.0f, 0.0f, 0.0f, 40.0f, -80.0f, 1 },
            { "Baix i altres", "Orgue rock", "Overdrive per a orgue.", 1, 6.0f, 5.0f, 0.0f, 0.0f, 100.0f, -80.0f, 0 },
            { "Baix i altres", "Veu distorsionada", "Crunch a la veu, per a efectes.", 0, 7.0f, 6.5f, 0.0f, -4.0f, 100.0f, -80.0f, 1 },
        };
    }

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> v = []
        {
            std::vector<Preset> r;
            for (const auto& x : taula)
            {
                Drive d;
                d.posa (ModeP, (float) x.mode); d.posa (Gain, x.gain); d.posa (Tone, x.tone); d.posa (Level, x.level); d.posa (Bass, x.bass);
                d.posa (Mix, x.mix); d.posa (Gate, x.gate); d.posa (Tight, (float) x.tight);
                r.push_back ({ x.nom, x.cat, x.desc, d.estat() });
            }
            return r;
        }();
        return v;
    }

    // ---- Presets de l'usuari (com els de la Reverb) ----
    bool BibliotecaPresets::carrega (const std::filesystem::path& fitxer)
    {
        usuari.clear();
        favorits.clear();
        std::error_code ec;
        if (! std::filesystem::exists (fitxer, ec)) return true;
        try
        {
            std::ifstream f (fitxer, std::ios::binary);
            const auto j = nlohmann::json::parse (f);
            for (const auto& x : j.value ("presets", nlohmann::json::array()))
            {
                Preset p { x.value ("nom", std::string()), x.value ("carpeta", std::string()), x.value ("descripcio", std::string()),
                           x.value ("estat", std::string()) };
                if (! p.nom.empty() && p.estat.rfind ("mibase-drive=", 0) == 0 && busca (p.nom) == nullptr) usuari.push_back (std::move (p));
            }
            for (const auto& x : j.value ("favorits", nlohmann::json::array()))
                if (x.is_string()) favorits.push_back (x.get<std::string>());
            return true;
        }
        catch (...) { return false; }
    }

    bool BibliotecaPresets::desa (const std::filesystem::path& fitxer) const
    {
        nlohmann::json j;
        j["versio"] = 1;
        j["presets"] = nlohmann::json::array();
        for (const auto& p : usuari)
            j["presets"].push_back ({ { "nom", p.nom }, { "carpeta", p.categoria }, { "descripcio", p.descripcio }, { "estat", p.estat } });
        j["favorits"] = favorits;
        std::error_code ec;
        std::filesystem::create_directories (fitxer.parent_path(), ec);
        auto tmp = fitxer;
        tmp += ".tmp";
        {
            std::ofstream f (tmp, std::ios::binary | std::ios::trunc);
            if (! f) return false;
            f << j.dump (1);
            if (! f) return false;
        }
        std::filesystem::rename (tmp, fitxer, ec);
        if (ec) { std::filesystem::remove (fitxer, ec); std::filesystem::rename (tmp, fitxer, ec); }
        return ! ec;
    }

    void BibliotecaPresets::desaPreset (const Preset& p)
    {
        for (auto& x : usuari) if (x.nom == p.nom) { x = p; return; }
        usuari.push_back (p);
    }
    bool BibliotecaPresets::esborra (const std::string& nom)
    {
        const auto it = std::find_if (usuari.begin(), usuari.end(), [&] (const Preset& p) { return p.nom == nom; });
        if (it == usuari.end()) return false;
        usuari.erase (it);
        favorits.erase (std::remove (favorits.begin(), favorits.end(), clauUsuari (nom)), favorits.end());
        return true;
    }
    bool BibliotecaPresets::reanomena (const std::string& nom, const std::string& nou)
    {
        if (nou.empty() || nou == nom || busca (nou) != nullptr) return false;
        for (auto& p : usuari)
            if (p.nom == nom)
            {
                p.nom = nou;
                for (auto& f : favorits) if (f == clauUsuari (nom)) f = clauUsuari (nou);
                return true;
            }
        return false;
    }
    bool BibliotecaPresets::mou (const std::string& nom, const std::string& carpeta)
    {
        for (auto& p : usuari) if (p.nom == nom) { p.categoria = carpeta; return true; }
        return false;
    }
    const Preset* BibliotecaPresets::busca (const std::string& nom) const
    {
        for (const auto& p : usuari) if (p.nom == nom) return &p;
        return nullptr;
    }
    std::vector<std::string> BibliotecaPresets::carpetes() const
    {
        std::vector<std::string> v;
        for (const auto& p : usuari) if (std::find (v.begin(), v.end(), p.categoria) == v.end()) v.push_back (p.categoria);
        std::sort (v.begin(), v.end());
        return v;
    }
    bool BibliotecaPresets::esFavorit (const std::string& clau) const { return std::find (favorits.begin(), favorits.end(), clau) != favorits.end(); }
    void BibliotecaPresets::commutaFavorit (const std::string& clau)
    {
        const auto it = std::find (favorits.begin(), favorits.end(), clau);
        if (it != favorits.end()) favorits.erase (it); else favorits.push_back (clau);
    }
    std::string BibliotecaPresets::nomLliure (const std::string& base) const
    {
        if (busca (base) == nullptr) return base;
        for (int i = 2;; ++i) { auto n = base + " " + std::to_string (i); if (busca (n) == nullptr) return n; }
    }
}
