#include "mibase/Saturacio.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>

namespace mibase::saturacio
{
    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c { "Veus", "Guitarres", "Baix", "Bateria", "Teclats i sintes", "Busos i mescla", "Màster", "Extrem" };
        return c;
    }

    namespace
    {
        // estil (0 A, 1 E, 2 N, 3 T, 4 P, 5 Suau), drive, punish, low cut, thump, tone, high cut, steep, mix, sortida
        struct V { const char* cat; const char* nom; const char* desc; int estil; float drive; int punish; float lc; int thump; float tone; float hc; int steep; float mix, out; };
        const V taula[] = {
            { "Veus", "Veu càlida", "Una mica de cinta: la veu més rodona i present.", 0, 2.5f, 0, 80, 0, 0.0f, 16000, 0, 100, 0 },
            { "Veus", "Veu endavant", "Consola brillant: la veu talla la mescla.", 1, 3.0f, 0, 100, 0, 1.5f, 18000, 0, 100, 0 },
            { "Veus", "Veu gruixuda", "Preamp de transistors: cos als mitjos.", 2, 3.5f, 0, 90, 0, -1.0f, 15000, 0, 100, 0 },
            { "Veus", "Veu rock bruta", "Triode amb drive: veu cridada amb mossegada.", 3, 6.0f, 0, 150, 0, 1.0f, 12000, 0, 80, 0 },
            { "Veus", "Veu de megàfon", "Estreta i trencada, per a efectes.", 4, 7.0f, 0, 600, 1, 3.0f, 4500, 1, 100, 0 },
            { "Veus", "Paral·lel subtil", "Molta saturació barrejada a poc: densitat sense que es noti.", 3, 7.0f, 0, 120, 0, 0.0f, 14000, 0, 25, 0 },
            { "Guitarres", "Acústica amb cos", "Cinta suau per a guitarra acústica.", 0, 3.0f, 0, 90, 0, -0.5f, 17000, 0, 100, 0 },
            { "Guitarres", "Elèctrica neta calenta", "El punt d'un ampli de vàlvules abans de trencar.", 3, 4.0f, 0, 80, 0, 0.0f, 12000, 0, 100, 0 },
            { "Guitarres", "Pèntode rock", "Agressiu i dens, per a riffs.", 4, 7.0f, 0, 90, 1, 0.0f, 9000, 1, 100, 0 },
            { "Baix", "Baix rodó", "Cinta: greus més plens sense perdre definició.", 0, 4.0f, 0, 30, 0, -1.0f, 10000, 0, 100, 0 },
            { "Baix", "Baix amb grinyol", "Preamp de transistors que fa sortir les notes.", 2, 6.0f, 0, 40, 0, 1.5f, 9000, 0, 100, 0 },
            { "Baix", "Baix distorsionat", "Pèntode a fons barrejat amb l'original: greus nets i aguts bruts.", 4, 7.0f, 0, 40, 0, 1.0f, 6000, 1, 45, 0 },
            { "Bateria", "Bateria aixafada", "Pèntode a fons en paral·lel: energia.", 4, 8.0f, 1, 60, 0, 1.0f, 12000, 0, 35, 0 },
            { "Bateria", "Caixa amb cos", "Triode per donar gruix a la caixa.", 3, 5.0f, 0, 120, 1, 0.5f, 14000, 0, 100, 0 },
            { "Bateria", "Bombo amb cop", "Thump al tall de greus i una mica de cinta.", 0, 4.0f, 0, 45, 1, -1.0f, 9000, 1, 100, 0 },
            { "Bateria", "Sala trencada", "Overheads o sala destrossats, barrejats.", 4, 9.0f, 1, 200, 0, 2.0f, 8000, 1, 30, 0 },
            { "Teclats i sintes", "Piano vintage", "Cinta suau: un piano amb aire antic.", 0, 3.0f, 0, 60, 0, -2.0f, 12000, 0, 100, 0 },
            { "Teclats i sintes", "Orgue amb vàlvula", "Triode a fons per a orgue rock.", 3, 6.0f, 0, 70, 0, 0.5f, 8000, 0, 100, 0 },
            { "Teclats i sintes", "Sinte agressiu", "Consola a fons: un lead que talla.", 1, 7.0f, 0, 80, 0, 2.0f, 16000, 0, 100, 0 },
            { "Busos i mescla", "Escalfor suau", "La saturació de sempre, neutra: cos sense canviar el to.", 5, 3.0f, 0, 20, 0, 0.0f, 20000, 0, 100, 0 },
            { "Veus", "Veu amb escalfor", "Saturació suau i neutra per a qualsevol veu.", 5, 3.5f, 0, 90, 0, 0.0f, 18000, 0, 100, 0 },
            { "Màster", "Màster suau", "Molt poca saturació neutra per arrodonir els pics.", 5, 1.0f, 0, 20, 0, 0.0f, 20000, 0, 100, 0 },
            { "Busos i mescla", "Cola de bus", "Una mica de cinta per enganxar un grup.", 0, 1.5f, 0, 20, 0, 0.0f, 20000, 0, 100, 0 },
            { "Busos i mescla", "Calidesa de consola", "Com passar per una consola: subtil.", 2, 1.5f, 0, 20, 0, 0.0f, 20000, 0, 100, 0 },
            { "Màster", "Màster cinta", "Molt subtil: arrodoneix els pics del màster.", 0, 1.0f, 0, 20, 0, -0.3f, 20000, 0, 100, 0 },
            { "Màster", "Màster brillant", "Una mica de consola per obrir els aguts.", 1, 1.0f, 0, 20, 0, 0.8f, 20000, 0, 100, 0 },
            { "Extrem", "Destrucció", "Punish i pèntode: tot trencat.", 4, 10.0f, 1, 100, 0, 0.0f, 10000, 0, 100, 0 },
            { "Extrem", "Ràdio vella", "Estret, trencat i sense greus.", 3, 8.0f, 0, 700, 1, 2.0f, 3500, 1, 100, 0 },
            { "Extrem", "Telèfon", "Banda estreta i brut.", 1, 7.0f, 0, 900, 0, 3.0f, 3000, 1, 100, 0 },
        };
    }

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> v = []
        {
            std::vector<Preset> r;
            for (const auto& x : taula)
            {
                Saturacio d;
                d.posa (EstilP, (float) x.estil); d.posa (Drive, x.drive); d.posa (Punish, (float) x.punish); d.posa (LowCut, x.lc);
                d.posa (Thump, (float) x.thump); d.posa (Tone, x.tone); d.posa (HighCut, x.hc); d.posa (Steep, (float) x.steep);
                d.posa (Mix, x.mix); d.posa (Sortida, x.out);
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
                if (! p.nom.empty() && p.estat.rfind ("mibase-saturacio=", 0) == 0 && busca (p.nom) == nullptr) usuari.push_back (std::move (p));
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
