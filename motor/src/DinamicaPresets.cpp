#include "mibase/Dinamica.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>

namespace mibase::dinamica
{
    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c { "Veus", "Guitarres", "Baix", "Bateria", "Teclats i sintes", "Busos i mescla",
                                                  "Sidechain i ducking", "De-essers i neteja" };
        return c;
    }

    namespace
    {
        // mode, llindar, ratio, atac, alliberament, genoll, rang, guany, detector (0 peak/1 rms), hpf, lpf, freq, mix
        struct V { const char* cat; const char* nom; const char* desc; int mode; float thr, ratio, atk, rel, knee, rang, guany; int det; float hpf, lpf, freq, mix; };
        const V taula[] = {
            // ---- Veus ----
            { "Veus", "Veu suau", "Compressió lleugera per igualar una veu sense que es noti.", 0, -20, 2.5f, 8, 120, 6, 40, 3, 1, 80, 20000, 6000, 100 },
            { "Veus", "Veu pop endavant", "La veu sempre al davant: més ratio i atac ràpid.", 0, -24, 4, 3, 90, 4, 40, 6, 0, 100, 20000, 6000, 100 },
            { "Veus", "Veu rock", "Contundent, per a veus cridades i amb molta dinàmica.", 0, -26, 6, 2, 70, 3, 40, 7, 0, 120, 20000, 6000, 100 },
            { "Veus", "Veu parlada", "Per a locució: igualada i propera.", 0, -22, 3.5f, 5, 150, 6, 40, 5, 1, 90, 20000, 6000, 100 },
            { "Veus", "Veu íntima", "Acústic i balades: control suau i sense bombeig.", 0, -18, 2, 15, 200, 8, 40, 2, 1, 70, 20000, 6000, 100 },
            { "Veus", "Veu en paral·lel", "Aixafa molt i ho barreja amb l'original: densitat sense perdre vida.", 0, -35, 10, 1, 80, 2, 40, 8, 0, 100, 20000, 6000, 40 },
            { "Veus", "Cors", "Enganxa els cors en un bloc.", 0, -22, 3, 10, 150, 6, 40, 3, 1, 150, 20000, 6000, 100 },
            { "Veus", "Rap", "Ràpid i ferm perquè cada síl·laba se senti.", 0, -24, 5, 1, 60, 3, 40, 6, 0, 100, 20000, 6000, 100 },
            { "Veus", "Veu vintage", "Atac lent i alliberament llarg, com un compressor d'època.", 0, -20, 3, 20, 400, 10, 40, 4, 1, 80, 20000, 6000, 100 },
            { "Veus", "Expander de veu", "Baixa el soroll i les respiracions entre frases.", 2, -45, 2, 2, 150, 0, 15, 0, 1, 100, 20000, 6000, 100 },
            // ---- Guitarres ----
            { "Guitarres", "Acústica natural", "Iguala els rasguejos sense matar els atacs.", 0, -20, 2.5f, 15, 120, 6, 40, 3, 1, 80, 20000, 6000, 100 },
            { "Guitarres", "Acústica brillant", "Atac lent: deixa passar la pua i aguanta el cos.", 0, -24, 4, 25, 150, 4, 40, 5, 0, 120, 20000, 6000, 100 },
            { "Guitarres", "Arpegis nets", "Sustain llarg per a guitarra elèctrica neta.", 0, -28, 4, 10, 300, 6, 40, 6, 1, 80, 20000, 6000, 100 },
            { "Guitarres", "Funk", "Ràpid i curt: cada nota igual, molt de groove.", 0, -26, 6, 2, 50, 3, 40, 6, 0, 100, 20000, 6000, 100 },
            { "Guitarres", "Solo sustain", "Allarga les notes del solo.", 0, -32, 8, 5, 400, 6, 40, 9, 1, 100, 20000, 6000, 100 },
            { "Guitarres", "Distorsió controlada", "Una mica de control a guitarres ja distorsionades.", 0, -16, 2, 20, 150, 6, 40, 2, 1, 100, 20000, 6000, 100 },
            { "Guitarres", "Gate per a distorsió", "Talla el brunzit entre riffs.", 1, -45, 1, 0.5f, 60, 0, 60, 0, 0, 100, 20000, 6000, 100 },
            { "Guitarres", "Gate staccato", "Talls secs per a riffs de metal.", 1, -35, 1, 0.2f, 25, 0, 80, 0, 0, 150, 20000, 6000, 100 },
            { "Guitarres", "Paral·lel acústica", "Densitat a l'acústica sense perdre la dinàmica.", 0, -34, 8, 3, 100, 3, 40, 8, 0, 100, 20000, 6000, 45 },
            { "Guitarres", "Expander suau", "Netegeta suau per a pistes amb soroll de fons.", 2, -50, 1.8f, 3, 200, 0, 12, 0, 1, 80, 20000, 6000, 100 },
            // ---- Baix ----
            { "Baix", "Baix igualat", "Totes les notes al mateix volum.", 0, -22, 4, 10, 150, 6, 40, 4, 1, 20, 20000, 6000, 100 },
            { "Baix", "Baix pop", "Ferm i rodó, assegut a la mescla.", 0, -24, 5, 5, 120, 4, 40, 5, 1, 20, 20000, 6000, 100 },
            { "Baix", "Baix rock", "Més mossegada i més control.", 0, -26, 6, 3, 100, 3, 40, 6, 0, 20, 20000, 6000, 100 },
            { "Baix", "Slap", "Atac molt ràpid per domar els cops de slap.", 0, -24, 8, 0.5f, 80, 2, 40, 6, 0, 20, 20000, 6000, 100 },
            { "Baix", "Baix de sinte", "Control ferm per a baixos de sintetitzador.", 0, -20, 4, 5, 100, 4, 40, 3, 1, 20, 20000, 6000, 100 },
            { "Baix", "Limitador de baix", "Talla els pics sense tocar la resta.", 0, -10, 20, 0.5f, 60, 1, 40, 2, 0, 20, 20000, 6000, 100 },
            { "Baix", "Paral·lel de baix", "Densitat i sustain barrejats amb l'original.", 0, -36, 10, 2, 120, 2, 40, 9, 0, 20, 20000, 6000, 50 },
            { "Baix", "Baix vintage", "Lent i musical, com un compressor òptic.", 0, -20, 3, 25, 500, 10, 40, 4, 1, 20, 20000, 6000, 100 },
            { "Baix", "Expander de baix", "Treu el soroll de l'amplificador entre notes.", 2, -48, 2, 2, 150, 0, 15, 0, 1, 20, 20000, 6000, 100 },
            { "Baix", "Baix DI net", "Molt suau, per a baix directe a la targeta.", 0, -18, 2, 15, 200, 8, 40, 2, 1, 20, 20000, 6000, 100 },
            // ---- Bateria ----
            { "Bateria", "Bombo punch", "Atac lent: el cop passa, el cos es controla.", 0, -20, 4, 20, 80, 3, 40, 4, 0, 20, 20000, 6000, 100 },
            { "Bateria", "Bombo gate", "Talla la ressonància del bombo.", 1, -30, 1, 0.3f, 80, 0, 40, 0, 0, 20, 2000, 6000, 100 },
            { "Bateria", "Caixa ferma", "La caixa més igualada i present.", 0, -22, 4, 8, 100, 3, 40, 4, 0, 100, 20000, 6000, 100 },
            { "Bateria", "Caixa gate", "Neteja el que entra pel micro de la caixa.", 1, -28, 1, 0.3f, 120, 0, 30, 0, 0, 150, 12000, 6000, 100 },
            { "Bateria", "Toms gate", "Només sonen quan es toquen.", 1, -32, 1, 0.5f, 200, 0, 30, 0, 0, 60, 8000, 6000, 100 },
            { "Bateria", "Overheads", "Iguala els plats i dona cohesió.", 0, -18, 2.5f, 15, 200, 6, 40, 2, 1, 150, 20000, 6000, 100 },
            { "Bateria", "Room aixafat", "La sala de bateria molt comprimida.", 0, -36, 10, 1, 60, 2, 40, 10, 0, 100, 20000, 6000, 100 },
            { "Bateria", "Bus de bateria", "Enganxa tota la bateria.", 0, -18, 3, 10, 120, 6, 40, 2, 1, 80, 20000, 6000, 100 },
            { "Bateria", "Bateria en paral·lel", "La compressió de Nova York: força sense perdre els cops.", 0, -38, 10, 1, 80, 2, 40, 10, 0, 20, 20000, 6000, 40 },
            { "Bateria", "Caixa gran", "Atac lent i molta compressió per a caixes de rock.", 0, -28, 6, 25, 150, 3, 40, 6, 0, 100, 20000, 6000, 100 },
            // ---- Teclats i sintes ----
            { "Teclats i sintes", "Piano natural", "Molt suau, respecta la dinàmica del piano.", 0, -18, 2, 15, 200, 8, 40, 2, 1, 40, 20000, 6000, 100 },
            { "Teclats i sintes", "Piano pop", "Piano endavant i igualat.", 0, -24, 4, 5, 150, 4, 40, 4, 1, 60, 20000, 6000, 100 },
            { "Teclats i sintes", "Rhodes", "Càlid i rodó.", 0, -22, 3, 12, 250, 8, 40, 3, 1, 60, 20000, 6000, 100 },
            { "Teclats i sintes", "Orgue", "Control lleuger per a orgues.", 0, -20, 2.5f, 20, 200, 6, 40, 2, 1, 60, 20000, 6000, 100 },
            { "Teclats i sintes", "Pad", "Iguala pads sense bombeig.", 0, -20, 2, 30, 500, 10, 40, 2, 1, 80, 20000, 6000, 100 },
            { "Teclats i sintes", "Lead de sinte", "Ferm, perquè el lead no s'escapi.", 0, -22, 5, 3, 100, 4, 40, 5, 0, 80, 20000, 6000, 100 },
            { "Teclats i sintes", "Pluck", "Atac lent per deixar passar el cop.", 0, -24, 4, 20, 80, 3, 40, 4, 0, 80, 20000, 6000, 100 },
            { "Teclats i sintes", "Arp ritmic", "Ràpid i curt per a arpegiadors.", 0, -24, 5, 2, 60, 3, 40, 5, 0, 80, 20000, 6000, 100 },
            { "Teclats i sintes", "Gate trance", "Talla el pad al ritme (sense sidechain, amb el seu propi so).", 1, -24, 1, 0.5f, 40, 0, 50, 0, 0, 60, 20000, 6000, 100 },
            { "Teclats i sintes", "Paral·lel de teclat", "Densitat barrejada amb l'original.", 0, -34, 8, 3, 120, 3, 40, 8, 0, 60, 20000, 6000, 45 },
            // ---- Busos i mescla ----
            { "Busos i mescla", "Cola de mescla", "Enganxa la mescla: ratio baixa, atac lent.", 0, -14, 2, 30, 200, 6, 40, 1.5f, 1, 20, 20000, 6000, 100 },
            { "Busos i mescla", "Cola de mescla forta", "Més enganxat, per a pop i rock.", 0, -18, 4, 10, 150, 6, 40, 3, 1, 20, 20000, 6000, 100 },
            { "Busos i mescla", "Bus de guitarres", "Unifica les guitarres.", 0, -18, 2.5f, 20, 150, 6, 40, 2, 1, 80, 20000, 6000, 100 },
            { "Busos i mescla", "Bus de veus", "Totes les veus en un bloc.", 0, -20, 3, 10, 150, 6, 40, 2, 1, 100, 20000, 6000, 100 },
            { "Busos i mescla", "Màster suau", "Per a la cadena de màstering, gairebé no es nota.", 0, -10, 1.5f, 30, 300, 8, 40, 1, 1, 20, 20000, 6000, 100 },
            { "Busos i mescla", "Limitador", "Talla els pics al final de la cadena.", 0, -6, 20, 0.2f, 80, 1, 40, 4, 0, 20, 20000, 6000, 100 },
            { "Busos i mescla", "Mescla en paral·lel", "Energia a tota la mescla, barrejada al 30 %.", 0, -30, 6, 5, 100, 4, 40, 6, 1, 20, 20000, 6000, 30 },
            { "Busos i mescla", "Retorn de reverb", "Controla la cua d'una reverb.", 0, -22, 3, 20, 300, 6, 40, 2, 1, 150, 20000, 6000, 100 },
            { "Busos i mescla", "Bus de vents", "Per a metalls i vents.", 0, -20, 3, 8, 120, 6, 40, 2, 1, 100, 20000, 6000, 100 },
            { "Busos i mescla", "Bus de cordes", "Molt suau per a cordes i pads.", 0, -18, 2, 25, 300, 10, 40, 2, 1, 60, 20000, 6000, 100 },
            // ---- Sidechain i ducking ----
            { "Sidechain i ducking", "Bombo baixa el baix", "Amb el bombo al SC: el baix s'aparta a cada cop.", 0, -24, 6, 1, 100, 3, 40, 0, 0, 20, 200, 6000, 100 },
            { "Sidechain i ducking", "Pumping EDM", "Amb el bombo al SC: l'efecte de bombeig.", 0, -30, 20, 0.5f, 180, 1, 40, 0, 0, 20, 200, 6000, 100 },
            { "Sidechain i ducking", "Veu baixa guitarres", "Amb la veu al SC: les guitarres deixen lloc a la veu.", 0, -26, 2.5f, 10, 250, 6, 40, 0, 1, 200, 5000, 6000, 100 },
            { "Sidechain i ducking", "Veu baixa música", "Amb la veu al SC: tota la música baixa (locucions, podcast).", 0, -30, 6, 10, 400, 6, 40, 0, 1, 150, 6000, 6000, 100 },
            { "Sidechain i ducking", "Veu baixa el delay", "Amb la veu al SC: el delay només se sent als forats.", 0, -30, 8, 5, 300, 4, 40, 0, 1, 150, 8000, 6000, 100 },
            { "Sidechain i ducking", "Caixa baixa la reverb", "Amb la caixa al SC: la reverb no tapa els cops.", 0, -24, 4, 2, 150, 3, 40, 0, 0, 150, 12000, 6000, 100 },
            { "Sidechain i ducking", "Gate amb clau", "Gate obert per una altra pista (SC): el baix només sona amb el bombo.", 1, -30, 1, 0.3f, 80, 0, 60, 0, 0, 20, 300, 6000, 100 },
            { "Sidechain i ducking", "Ducking suau", "Amb SC: baixa una mica, per mesclar sense que es noti.", 0, -22, 2, 15, 300, 8, 40, 0, 1, 20, 20000, 6000, 100 },
            { "Sidechain i ducking", "Bombo baixa els pads", "Amb el bombo al SC: els pads respiren amb el ritme.", 0, -28, 8, 1, 250, 2, 40, 0, 0, 20, 200, 6000, 100 },
            { "Sidechain i ducking", "Trance gate amb clau", "Amb un hi-hat o patró al SC: talla un pad al ritme.", 1, -30, 1, 0.3f, 40, 0, 50, 0, 0, 2000, 20000, 6000, 100 },
            // ---- De-essers i neteja ----
            { "De-essers i neteja", "De-esser suau", "Treu una mica les S d'una veu.", 3, -30, 3, 1, 80, 4, 40, 0, 0, 20, 20000, 6500, 100 },
            { "De-essers i neteja", "De-esser fort", "Per a veus amb sibilants molt marcades.", 3, -38, 6, 0.5f, 60, 3, 40, 0, 0, 20, 20000, 6000, 100 },
            { "De-essers i neteja", "De-esser veu greu", "Sibilants més baixes (veus masculines).", 3, -32, 4, 1, 80, 4, 40, 0, 0, 20, 20000, 4500, 100 },
            { "De-essers i neteja", "De-esser veu aguda", "Sibilants més altes (veus femenines).", 3, -32, 4, 1, 80, 4, 40, 0, 0, 20, 20000, 8000, 100 },
            { "De-essers i neteja", "Domar plats", "De-esser per als plats massa brillants.", 3, -30, 3, 2, 120, 6, 40, 0, 1, 20, 20000, 9000, 100 },
            { "De-essers i neteja", "Guitarra acústica: fregaments", "Suavitza els fregaments de dits a les cordes.", 3, -34, 3, 1, 100, 4, 40, 0, 0, 20, 20000, 3500, 100 },
            { "De-essers i neteja", "Gate de soroll", "Talla el soroll de fons quan no hi ha so.", 1, -55, 1, 1, 150, 0, 40, 0, 1, 20, 20000, 6000, 100 },
            { "De-essers i neteja", "Expander de soroll", "Com el gate però més natural.", 2, -50, 2, 2, 200, 0, 20, 0, 1, 20, 20000, 6000, 100 },
            { "De-essers i neteja", "Respiracions", "Abaixa les respiracions entre frases.", 2, -40, 2.5f, 3, 150, 0, 12, 0, 1, 100, 20000, 6000, 100 },
            { "De-essers i neteja", "Brunzit d'ampli", "Gate per a amplificadors amb soroll.", 1, -50, 1, 1, 100, 0, 30, 0, 0, 100, 20000, 6000, 100 },
        };
    }

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> v = []
        {
            std::vector<Preset> r;
            for (const auto& x : taula)
            {
                Dinamica d;
                d.posa (ModeP, (float) x.mode); d.posa (Llindar, x.thr); d.posa (Ratio, x.ratio); d.posa (Atac, x.atk); d.posa (Alliberament, x.rel);
                d.posa (Genoll, x.knee); d.posa (Rang, x.rang); d.posa (Guany, x.guany); d.posa (Detector, (float) x.det); d.posa (FiltreGreus, x.hpf);
                d.posa (FiltreAguts, x.lpf); d.posa (FreqDeEsser, x.freq); d.posa (Mix, x.mix);
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
                if (! p.nom.empty() && p.estat.rfind ("mibase-dinamica=", 0) == 0 && busca (p.nom) == nullptr) usuari.push_back (std::move (p));
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
