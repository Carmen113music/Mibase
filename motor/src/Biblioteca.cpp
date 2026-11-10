#include "mibase/Biblioteca.h"
#include "mibase/Bateria.h"
#include "mibase/ProjecteDisc.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>

namespace mibase::biblioteca
{
    namespace fs = std::filesystem;
    using json = nlohmann::ordered_json;

    const Sample* Index::busca (const std::string& f) const
    {
        for (auto& s : samples) if (s.fitxer == f) return &s;
        return nullptr;
    }
    Sample* Index::busca (const std::string& f)
    {
        for (auto& s : samples) if (s.fitxer == f) return &s;
        return nullptr;
    }

    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c { "Bombo", "Caixa", "Hi-hat", "Percussió", "Baix", "Veu", "Sintetitzador", "FX", "Ambient", "Altres" };
        return c;
    }

    namespace
    {
        /** Minúscules i sense accents (UTF-8 de les vocals catalanes i castellanes més habituals). */
        std::string pla (const std::string& s)
        {
            static const std::pair<const char*, char> accents[] = {
                { "à", 'a' }, { "á", 'a' }, { "À", 'a' }, { "Á", 'a' }, { "è", 'e' }, { "é", 'e' }, { "È", 'e' }, { "É", 'e' },
                { "í", 'i' }, { "ì", 'i' }, { "ï", 'i' }, { "Í", 'i' }, { "ò", 'o' }, { "ó", 'o' }, { "Ò", 'o' }, { "Ó", 'o' },
                { "ú", 'u' }, { "ù", 'u' }, { "ü", 'u' }, { "Ú", 'u' }, { "ç", 'c' }, { "Ç", 'c' }, { "ñ", 'n' }, { "Ñ", 'n' } };
            std::string r;
            for (size_t i = 0; i < s.size();)
            {
                bool fet = false;
                if ((unsigned char) s[i] >= 0x80)
                    for (auto& [a, b] : accents)
                        if (s.compare (i, std::char_traits<char>::length (a), a) == 0) { r += b; i += std::char_traits<char>::length (a); fet = true; break; }
                if (! fet) { r += (char) std::tolower ((unsigned char) s[i]); ++i; }
            }
            return r;
        }

        bool conte (const std::string& text, std::initializer_list<const char*> paraules)
        {
            for (auto* p : paraules) if (text.find (p) != std::string::npos) return true;
            return false;
        }
    }

    std::string categoriaPerNom (const std::string& nomFitxer)
    {
        const auto n0 = pla (fs::path (nomFitxer).stem().string());
        // abans que la bateria: «bass» sol és un baix (no un bombo), i els «gooool» són veu
        if (conte (n0, { "808", "bassline", "sub bass", "subbass", "baix", "bajo" })
            || (conte (n0, { "bass" }) && ! conte (n0, { "drum", "kick", "bd" }))) return "Baix";
        if (conte (n0, { "gol", "goo" })) return "Veu";
        const auto t = bateria::classificaNom (nomFitxer);
        if (t == "kick") return "Bombo";
        if (t == "snare" || t == "rim" || t == "clap") return "Caixa";
        if (t == "hihat" || t == "hihat-obert" || t == "hihat-pedal" || t == "ride" || t == "crash") return "Hi-hat";
        if (! t.empty() && t != "altres") return "Percussió";
        const auto n = pla (fs::path (nomFitxer).stem().string());
        if (conte (n, { "bass", "baix", "bajo", "808", "sub" })) return "Baix";
        if (conte (n, { "vocal", "vox", "voice", "veu", "voz", "chant", "choir", "cor", "gol", "crit" })) return "Veu";
        if (conte (n, { "synth", "sinte", "lead", "pad", "pluck", "arp", "chord", "acord", "keys", "piano", "organ" })) return "Sintetitzador";
        if (conte (n, { "fx", "sfx", "riser", "sweep", "impact", "whoosh", "noise", "soroll", "glitch", "drop" })) return "FX";
        if (conte (n, { "ambient", "amb", "atmo", "drone", "texture", "field", "pluja", "rain", "wind", "vent" })) return "Ambient";
        if (conte (n, { "perc", "shaker", "conga", "bongo" })) return "Percussió";
        return "Altres";
    }

    double bpmPerNom (const std::string& nomFitxer)
    {
        const auto n = pla (fs::path (nomFitxer).stem().string());
        static const std::regex r1 (R"((\d{2,3}(?:\.\d+)?)\s*[_\- ]?\s*bpm)"), r2 (R"(bpm\s*[_\- ]?\s*(\d{2,3}(?:\.\d+)?))");
        std::smatch m;
        if (std::regex_search (n, m, r1) || std::regex_search (n, m, r2))
        {
            const double v = std::stod (m[1].str());
            if (v >= 40.0 && v <= 300.0) return v;
        }
        return 0.0;
    }

    std::string formaPerNom (const std::string& nomFitxer)
    {
        const auto n = pla (fs::path (nomFitxer).stem().string());
        if (conte (n, { "one shot", "oneshot", "one-shot", "one_shot", "1shot", "hit", "stab", "shot" })) return oneShot;
        if (conte (n, { "loop", "groove", "break", "beat" }) || bpmPerNom (nomFitxer) > 0.0) return loop;
        return {};
    }

    std::string detectaForma (const float* x, long long n, double fs, double& bpmEstimat)
    {
        bpmEstimat = 0.0;
        if (x == nullptr || n <= 0 || fs <= 0.0) return oneShot;
        const double dur = (double) n / fs;
        if (dur < 1.0) return oneShot;
        // volum per finestres de 20 ms
        const long long w = std::max<long long> (1, (long long) (0.02 * fs));
        std::vector<double> rms;
        for (long long i = 0; i + w <= n; i += w)
        {
            double s = 0.0;
            for (long long k = i; k < i + w; ++k) s += (double) x[k] * x[k];
            rms.push_back (std::sqrt (s / (double) w));
        }
        if (rms.size() < 10) return oneShot;
        const double pic = *std::max_element (rms.begin(), rms.end());
        if (pic < 1e-6) return oneShot;
        const double cua = (rms[rms.size() - 1] + rms[rms.size() - 2] + rms[rms.size() - 3]) / 3.0;
        if (cua / pic < 0.05) return oneShot;                // s'apaga al final
        int cops = 0;                                        // atacs: puja de cop i és prou fort
        for (size_t i = 2; i < rms.size(); ++i)
            if (rms[i] > 0.25 * pic && rms[i] > 2.0 * rms[i - 2]) { ++cops; i += 4; }
        if (cops < 3) return oneShot;                        // una nota llarga tallada (un pad), no un loop
        double millor = 0.0;
        for (int temps : { 2, 4, 8, 16, 32, 64 })
        {
            const double b = 60.0 * temps / dur;
            if (b >= 70.0 && b <= 180.0 && (millor == 0.0 || std::abs (b - 120.0) < std::abs (millor - 120.0))) millor = b;
        }
        bpmEstimat = std::round (millor * 10.0) / 10.0;
        return loop;
    }

    bool esAudio (const fs::path& f)
    {
        auto e = pla (f.extension().string());
        return e == ".wav" || e == ".aif" || e == ".aiff" || e == ".flac" || e == ".mp3" || e == ".ogg";
    }

    Index llegeix (const fs::path& carpeta)
    {
        Index idx;
        std::ifstream in (carpeta / nomIndex, std::ios::binary);
        if (! in) return idx;
        try
        {
            const auto j = json::parse (in);
            for (auto& e : j.value ("samples", json::array()))
            {
                Sample s;
                s.fitxer = e.value ("fitxer", std::string());
                if (s.fitxer.empty()) continue;
                s.nom = e.value ("nom", fs::path (disc::cami (s.fitxer)).stem().string());
                s.categoria = e.value ("categoria", std::string ("Altres"));
                if (std::find (categories().begin(), categories().end(), s.categoria) == categories().end()) s.categoria = "Altres";
                for (auto& x : e.value ("etiquetes", json::array())) if (x.is_string()) s.etiquetes.push_back (x.get<std::string>());
                s.durada = e.value ("durada", 0.0);
                s.bpm = e.value ("bpm", 0.0);
                s.tonalitat = e.value ("tonalitat", std::string());
                s.forma = e.value ("forma", std::string());
                s.data = e.value ("data", 0LL);
                idx.samples.push_back (std::move (s));
            }
        }
        catch (...) {}
        return idx;
    }

    bool desa (const fs::path& carpeta, const Index& idx)
    {
        json j;
        j["versio"] = 1;
        auto arr = json::array();
        for (auto& s : idx.samples)
            arr.push_back ({ { "fitxer", s.fitxer }, { "nom", s.nom }, { "categoria", s.categoria }, { "etiquetes", s.etiquetes },
                             { "durada", s.durada }, { "bpm", s.bpm }, { "tonalitat", s.tonalitat }, { "forma", s.forma }, { "data", s.data } });
        j["samples"] = arr;
        std::error_code ec;
        fs::create_directories (carpeta, ec);
        const auto tmp = carpeta / (std::string (nomIndex) + ".tmp");
        {
            std::ofstream out (tmp, std::ios::binary | std::ios::trunc);
            if (! out) return false;
            out << j.dump (2);
            if (! out) return false;
        }
        fs::rename (tmp, carpeta / nomIndex, ec);
        return ! ec;
    }

    std::vector<std::string> sincronitza (const fs::path& carpeta, Index& idx)
    {
        std::vector<std::string> nous;
        std::error_code ec;
        idx.samples.erase (std::remove_if (idx.samples.begin(), idx.samples.end(),
                                           [&] (const Sample& s) { return ! fs::exists (carpeta / disc::cami (s.fitxer), ec); }),
                           idx.samples.end());
        if (! fs::is_directory (carpeta, ec)) return nous;
        for (auto it = fs::recursive_directory_iterator (carpeta, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment (ec))
        {
            if (ec) break;
            if (! it->is_regular_file (ec) || ! esAudio (it->path())) continue;
            auto rel = disc::text (fs::relative (it->path(), carpeta, ec));
            std::replace (rel.begin(), rel.end(), '\\', '/');
            if (idx.busca (rel) == nullptr) nous.push_back (rel);
        }
        std::sort (nous.begin(), nous.end());
        return nous;
    }

    std::string nomLliure (const fs::path& carpeta, const std::string& base, const std::string& extensio)
    {
        std::error_code ec;
        std::string nom = base + extensio;
        for (int k = 2; fs::exists (carpeta / disc::cami (nom), ec); ++k) nom = base + " " + std::to_string (k) + extensio;
        return nom;
    }

    std::vector<int> filtra (const Index& idx, const std::string& text, const std::vector<std::string>& cats,
                             const std::vector<std::string>& formes, Ordre ordre, bool descendent)
    {
        const auto t = pla (text);
        std::vector<int> r;
        for (int i = 0; i < (int) idx.samples.size(); ++i)
        {
            const auto& s = idx.samples[(size_t) i];
            if (! cats.empty() && std::find (cats.begin(), cats.end(), s.categoria) == cats.end()) continue;
            if (! formes.empty() && std::find (formes.begin(), formes.end(), s.forma) == formes.end()) continue;
            if (! t.empty())
            {
                std::string tot = pla (s.nom) + "|" + pla (s.categoria) + "|" + pla (s.tonalitat) + "|" + pla (s.forma);
                for (auto& e : s.etiquetes) tot += "|" + pla (e);
                bool totes = true;   // totes les paraules del text
                size_t p = 0;
                while (p < t.size())
                {
                    auto q = t.find (' ', p);
                    if (q == std::string::npos) q = t.size();
                    if (q > p && tot.find (t.substr (p, q - p)) == std::string::npos) { totes = false; break; }
                    p = q + 1;
                }
                if (! totes) continue;
            }
            r.push_back (i);
        }
        auto clau = [&] (int i) { return &idx.samples[(size_t) i]; };
        std::stable_sort (r.begin(), r.end(), [&] (int a, int b)
        {
            const auto *x = clau (a), *y = clau (b);
            bool menor = false, igual = false;
            switch (ordre)
            {
                case Ordre::Nom:       menor = pla (x->nom) < pla (y->nom); igual = pla (x->nom) == pla (y->nom); break;
                case Ordre::Categoria: menor = x->categoria < y->categoria || (x->categoria == y->categoria && pla (x->nom) < pla (y->nom));
                                       igual = x->categoria == y->categoria && pla (x->nom) == pla (y->nom); break;
                case Ordre::Durada:    menor = x->durada < y->durada; igual = x->durada == y->durada; break;
                case Ordre::Data:      menor = x->data < y->data; igual = x->data == y->data; break;
            }
            if (igual) return false;
            return descendent ? ! menor : menor;
        });
        return r;
    }

    std::vector<int> filtra (const Index& idx, const std::string& text, const std::string& categoria, Ordre ordre, bool descendent,
                             const std::string& forma)
    {
        std::vector<std::string> c, f;
        if (! categoria.empty()) c.push_back (categoria);
        if (! forma.empty()) f.push_back (forma);
        return filtra (idx, text, c, f, ordre, descendent);
    }

    std::vector<std::string> etiquetesDeText (const std::string& text)
    {
        std::vector<std::string> r;
        std::string cur;
        auto tanca = [&]
        {
            auto a = cur.find_first_not_of (" \t"), b = cur.find_last_not_of (" \t");
            if (a != std::string::npos)
            {
                auto e = cur.substr (a, b - a + 1);
                if (std::find (r.begin(), r.end(), e) == r.end()) r.push_back (e);
            }
            cur.clear();
        };
        for (char c : text) { if (c == ',' || c == ';' || c == '\n') tanca(); else cur += c; }
        tanca();
        return r;
    }

    std::string textDeEtiquetes (const std::vector<std::string>& e)
    {
        std::string r;
        for (auto& x : e) r += (r.empty() ? "" : ", ") + x;
        return r;
    }
}
