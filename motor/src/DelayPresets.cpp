#include "mibase/Delay.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>

namespace mibase::delay
{
    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c { "Slapback i curts", "Rítmics", "Ping-pong i estèreo", "Cinta i analògic", "Veus",
                                                  "Guitarres", "Ambient i textures", "Lo-fi i especials" };
        return c;
    }

    namespace
    {
        // Notes: 2 = 1/16, 3 = 1/8T, 5 = 1/8, 6 = 1/4T, 7 = 1/8·, 8 = 1/4, 10 = 1/4·, 11 = 1/2, 13 = 1/1
        // caràcter (0 dig, 1 cinta, 2 anal, 3 lofi), ecos (0 est, 1 ping, 2 mono), sync, ms, nota, offset R, feedback, low cut, high cut,
        // drive, mod rate, mod depth, difusió, ducking, duck thr, amplada, mix
        struct V { const char* cat; const char* nom; const char* desc; int car, enc, sync; float ms; int nota; float off, fb, lc, hc, drive, rate, depth, dif, duck, thr, amp, mix; };
        const V taula[] = {
            // ---- Slapback i curts ----
            { "Slapback i curts", "Slapback rockabilly", "Un sol eco curt i ferm, com als discos dels 50.", 1, 2, 0, 110, 5, 0, 8, 120, 6000, 20, 0.6f, 5, 0, 0, -30, 100, 35 },
            { "Slapback i curts", "Slapback de veu", "Engreixa la veu amb un eco curt gairebé imperceptible.", 0, 2, 0, 90, 5, 0, 0, 200, 7000, 0, 0.6f, 0, 0, 0, -30, 100, 22 },
            { "Slapback i curts", "Doblat estèreo", "Dos ecos molt curts, un a cada costat: sembla una segona presa.", 0, 0, 0, 18, 5, 40, 0, 150, 12000, 0, 0.8f, 15, 0, 0, -30, 160, 40 },
            { "Slapback i curts", "Habitació petita", "Ecos curtíssims difuminats: dona una mica d'espai.", 0, 0, 0, 35, 5, 25, 30, 200, 6000, 0, 0.5f, 10, 70, 0, -30, 130, 25 },
            // ---- Rítmics ----
            { "Rítmics", "Corxera amb punt", "El clàssic 1/8 amb punt (The Edge): les notes es multipliquen.", 0, 0, 1, 375, 7, 0, 40, 150, 8000, 0, 0.6f, 0, 0, 0, -30, 100, 30 },
            { "Rítmics", "Negra", "Un eco a cada temps, net.", 0, 0, 1, 500, 8, 0, 35, 120, 9000, 0, 0.6f, 0, 0, 0, -30, 100, 25 },
            { "Rítmics", "Corxera", "Ecos a corxeres: omple sense embrutar.", 0, 0, 1, 250, 5, 0, 30, 150, 8000, 0, 0.6f, 0, 0, 0, -30, 100, 22 },
            { "Rítmics", "Tresets", "Ecos en tresets de negra: groove arrossegat.", 0, 0, 1, 333, 6, 0, 35, 150, 7500, 0, 0.6f, 0, 0, 0, -30, 100, 25 },
            { "Rítmics", "Semicorxeres", "Ecos ràpids que fan un trèmolo rítmic.", 0, 0, 1, 125, 2, 0, 45, 300, 6000, 0, 0.6f, 0, 0, 0, -30, 100, 25 },
            // ---- Ping-pong i estèreo ----
            { "Ping-pong i estèreo", "Ping-pong corxera", "Els ecos salten d'un costat a l'altre a corxeres.", 0, 1, 1, 250, 5, 0, 45, 150, 8000, 0, 0.6f, 0, 0, 0, -30, 100, 30 },
            { "Ping-pong i estèreo", "Ping-pong amb punt", "Corxera amb punt d'un costat a l'altre: molt ample.", 0, 1, 1, 375, 7, 0, 45, 200, 7000, 0, 0.6f, 0, 0, 0, -30, 110, 28 },
            { "Ping-pong i estèreo", "Estèreo desfasat", "Esquerra a negra, dreta més llarga: ecos que s'entrecreuen.", 0, 0, 1, 500, 8, 50, 40, 150, 8000, 0, 0.6f, 0, 0, 0, -30, 120, 25 },
            { "Ping-pong i estèreo", "Ping-pong de cinta", "Ping-pong amb caràcter de cinta: ample i càlid.", 1, 1, 1, 375, 7, 0, 50, 150, 6000, 15, 0.5f, 20, 0, 0, -30, 110, 28 },
            // ---- Cinta i analògic ----
            { "Cinta i analògic", "Space echo", "Eco de cinta amb wow i ecos que s'embruten.", 1, 0, 1, 375, 7, 0, 55, 120, 5500, 30, 0.5f, 30, 0, 0, -30, 100, 30 },
            { "Cinta i analògic", "Cinta gastada", "Molta modulació i aguts tous: una cinta vella.", 1, 2, 1, 500, 8, 0, 50, 200, 4000, 40, 0.8f, 70, 0, 0, -30, 100, 30 },
            { "Cinta i analògic", "Pedal analògic", "Com un pedal BBD: ecos foscos i rodons.", 2, 2, 1, 375, 7, 0, 50, 100, 20000, 10, 0.6f, 10, 0, 0, -30, 100, 30 },
            { "Cinta i analògic", "Autooscil·lació", "Feedback per sobre de 100 %: els ecos creixen. Juga amb el temps!", 1, 2, 1, 375, 7, 0, 104, 150, 5000, 40, 0.4f, 30, 0, 0, -30, 100, 35 },
            // ---- Veus ----
            { "Veus", "Veu balada", "Eco a negra que s'aparta mentre cantes (ducking).", 0, 0, 1, 500, 8, 10, 35, 250, 6000, 0, 0.6f, 5, 15, 10, -30, 110, 30 },
            { "Veus", "Veu pop moderna", "Ecos a corxera amb punt, difuminats i amb ducking.", 0, 1, 1, 375, 7, 0, 35, 300, 7000, 0, 0.6f, 0, 35, 12, -32, 120, 28 },
            { "Veus", "Llançament de frase", "Eco llarg (blanca) per al final de les frases.", 1, 0, 1, 1000, 11, 15, 45, 300, 5000, 15, 0.5f, 20, 25, 6, -35, 120, 30 },
            // ---- Guitarres ----
            { "Guitarres", "Solo de guitarra", "Ecos llargs que s'aparten mentre toques i omplen quan pares.", 2, 0, 1, 375, 7, 20, 45, 150, 20000, 10, 0.6f, 15, 0, 12, -30, 120, 35 },
            { "Guitarres", "Arpegis amb punt", "Corxera amb punt brillant per a arpegis nets.", 0, 0, 1, 375, 7, 0, 45, 200, 9000, 0, 0.6f, 10, 10, 0, -30, 110, 35 },
            { "Guitarres", "Rock amb cinta", "Slapback de cinta per a guitarra distorsionada.", 1, 2, 0, 140, 5, 0, 15, 150, 5000, 20, 0.6f, 10, 0, 0, -30, 100, 25 },
            // ---- Ambient i textures ----
            { "Ambient i textures", "Núvol", "Ecos molt difuminats i llargs: gairebé una reverb.", 0, 1, 1, 750, 10, 0, 70, 250, 5000, 0, 0.3f, 20, 90, 0, -30, 150, 40 },
            { "Ambient i textures", "Atmosfera de cinta", "Ecos llargs, foscos i modulats per a pads i guitarres.", 1, 0, 1, 1000, 11, 30, 75, 300, 3500, 20, 0.3f, 50, 60, 0, -30, 140, 40 },
            { "Ambient i textures", "Infinit", "Feedback molt alt i difusió: una cua que no s'acaba. Prova el Freeze.", 2, 1, 1, 1500, 10, 0, 92, 300, 4000, 10, 0.2f, 30, 80, 0, -30, 150, 45 },
            // ---- Lo-fi i especials ----
            { "Lo-fi i especials", "Lo-fi", "Ecos petits i bruts, com una ràdio vella.", 3, 2, 1, 375, 7, 0, 45, 150, 20000, 30, 0.7f, 20, 0, 0, -30, 100, 30 },
            { "Lo-fi i especials", "Telèfon", "Eco estret i sense greus.", 3, 2, 0, 200, 5, 0, 35, 800, 3000, 40, 0.6f, 0, 0, 0, -30, 0, 30 },
            { "Lo-fi i especials", "Chorus d'eco", "Un sol eco molt curt i molt modulat: fa de chorus.", 0, 0, 0, 12, 5, 30, 0, 100, 20000, 0, 1.2f, 60, 0, 0, -30, 150, 45 },
        };
    }

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> v = []
        {
            std::vector<Preset> r;
            for (const auto& x : taula)
            {
                Delay d;
                d.posa (CaracterP, (float) x.car); d.posa (EncaminamentP, (float) x.enc); d.posa (Sync, (float) x.sync); d.posa (Temps, x.ms);
                d.posa (Nota, (float) x.nota); d.posa (Desfas, x.off); d.posa (Feedback, x.fb); d.posa (LowCut, x.lc); d.posa (HighCut, x.hc);
                d.posa (Drive, x.drive); d.posa (ModRate, x.rate); d.posa (ModDepth, x.depth); d.posa (Difusio, x.dif); d.posa (DuckAmt, x.duck);
                d.posa (DuckThr, x.thr); d.posa (Amplada, x.amp); d.posa (Mix, x.mix);
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
                if (! p.nom.empty() && p.estat.rfind ("mibase-delay=", 0) == 0 && busca (p.nom) == nullptr) usuari.push_back (std::move (p));
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
