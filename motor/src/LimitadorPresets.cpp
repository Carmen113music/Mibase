#include "mibase/Limitador.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>

namespace mibase::limitador
{
    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c { "Màster", "Busos", "Pistes", "Seguretat", "Especials" };
        return c;
    }

    namespace
    {
        // estil (0 transparent, 1 modern, 2 punch, 3 agressiu), gain, ceiling, release, auto, link, true peak
        struct V { const char* cat; const char* nom; const char* desc; int estil; float gain, ceil, rel; int aut; float link; int tp; };
        const V taula[] = {
            { "Màster", "Màster transparent", "Només atura els pics, sense que es noti.", 0, 2.0f, -1.0f, 120.0f, 1, 100.0f, 1 },
            { "Màster", "Màster equilibrat", "El de cada dia: més volum i net.", 1, 5.0f, -1.0f, 80.0f, 1, 100.0f, 1 },
            { "Màster", "Màster streaming", "Pensat per a plataformes (al voltant de -14 LUFS, segons la cançó).", 1, 6.0f, -1.0f, 100.0f, 1, 100.0f, 1 },
            { "Màster", "Màster rock", "Fort i amb cop: guitarres i bateria endavant.", 2, 8.0f, -1.0f, 60.0f, 1, 100.0f, 1 },
            { "Màster", "Màster molt fort", "Per sonar tan fort com es pugui (amb cura!).", 3, 11.0f, -0.8f, 40.0f, 1, 100.0f, 1 },
            { "Màster", "Màster acústic", "Molt suau per a música tranquil·la i acústica.", 0, 3.0f, -1.0f, 200.0f, 1, 100.0f, 1 },
            { "Màster", "Màster maqueta", "Puja la maqueta per escoltar-la al mòbil o al cotxe.", 1, 7.0f, -1.0f, 80.0f, 1, 100.0f, 1 },
            { "Busos", "Bus de bateria", "Controla els cops de la bateria i hi dona cos.", 2, 4.0f, -3.0f, 40.0f, 1, 80.0f, 0 },
            { "Busos", "Bus de veus", "Iguala els pics de les veus.", 0, 3.0f, -3.0f, 120.0f, 1, 100.0f, 0 },
            { "Busos", "Bus de guitarres", "Manté les guitarres a ratlla.", 1, 3.0f, -3.0f, 100.0f, 1, 100.0f, 0 },
            { "Busos", "Bus d'instruments", "Cola per a tot l'acompanyament.", 1, 2.0f, -2.0f, 150.0f, 1, 100.0f, 0 },
            { "Pistes", "Baix sota control", "Talla els pics del baix perquè sigui constant.", 1, 4.0f, -4.0f, 60.0f, 1, 100.0f, 0 },
            { "Pistes", "Caixa amb cop", "Retalla el pic de la caixa i en puja el cos.", 2, 6.0f, -3.0f, 30.0f, 0, 100.0f, 0 },
            { "Pistes", "Veu protegida", "Només els crits: la veu no passa mai d'aquí.", 0, 0.0f, -6.0f, 120.0f, 1, 100.0f, 0 },
            { "Pistes", "Sinte constant", "Per a sintes i pads que han de sonar iguals.", 1, 5.0f, -4.0f, 150.0f, 1, 100.0f, 0 },
            { "Seguretat", "Només pics", "Gain 0: no toca res fins que un pic passa de -0,3 dB.", 1, 0.0f, -0.3f, 60.0f, 1, 100.0f, 1 },
            { "Seguretat", "Per a l'emissió", "Sostre a -2 dB per a vídeo i ràdio.", 0, 2.0f, -2.0f, 100.0f, 1, 100.0f, 1 },
            { "Seguretat", "Gravació en directe", "Protecció per a preses en directe amb sorpreses.", 1, 0.0f, -1.0f, 100.0f, 1, 100.0f, 1 },
            { "Especials", "Bombeig", "Release curt i molta limitació: l'efecte de «respirar».", 3, 14.0f, -1.0f, 10.0f, 0, 100.0f, 0 },
            { "Especials", "Destrucció controlada", "Agressiu a fons: només per a efecte.", 3, 18.0f, -1.0f, 20.0f, 0, 100.0f, 0 },
        };
    }

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> v = []
        {
            std::vector<Preset> r;
            for (const auto& x : taula)
            {
                Limitador d;
                d.posa (EstilP, (float) x.estil); d.posa (Gain, x.gain); d.posa (Ceiling, x.ceil); d.posa (Release, x.rel);
                d.posa (AutoRel, (float) x.aut); d.posa (Link, x.link); d.posa (TruePeak, (float) x.tp);
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
                if (! p.nom.empty() && p.estat.rfind ("mibase-limitador=", 0) == 0 && busca (p.nom) == nullptr) usuari.push_back (std::move (p));
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
