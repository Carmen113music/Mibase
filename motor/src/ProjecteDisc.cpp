#include "mibase/ProjecteDisc.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::ordered_json;   // conserva l'ordre dels camps (més fàcil de llegir)

namespace mibase::disc
{
    namespace
    {
        bool llegeixText (const fs::path& p, std::string& sortida)
        {
            std::ifstream f (p, std::ios::binary);
            if (! f)
                return false;
            std::ostringstream ss;
            ss << f.rdbuf();
            sortida = ss.str();
            return true;
        }

        bool escriuText (const fs::path& p, const std::string& contingut, std::string* error)
        {
            // Primer a un fitxer temporal i després es canvia de nom: si falla a mitges, l'original queda bé.
            auto temporal = p;
            temporal += ".tmp";

            {
                std::ofstream f (temporal, std::ios::binary | std::ios::trunc);
                if (! f)
                {
                    if (error) *error = "No s'ha pogut escriure a " + text (temporal.parent_path()) + ".";
                    return false;
                }
                f << contingut;
                if (! f.good())
                {
                    if (error) *error = "Error en escriure el fitxer (disc ple?).";
                    return false;
                }
            }

            std::error_code ec;
            fs::rename (temporal, p, ec);
            if (ec)
            {
                fs::remove (temporal, ec);
                if (error) *error = "No s'ha pogut desar " + text (p.filename()) + ".";
                return false;
            }
            return true;
        }

        std::string minuscules (std::string s)
        {
            for (auto& c : s)
                c = (char) std::tolower ((unsigned char) c);
            return s;
        }

        bool acabaAmb (const std::string& s, const std::string& final)
        {
            return s.size() >= final.size() && s.compare (s.size() - final.size(), final.size(), final) == 0;
        }

        std::string capcaleraEvolucio (const std::string& nom)
        {
            return "Evolució del projecte «" + nom + "» (el més recent a dalt)\n\n";
        }
    }

    fs::path cami (const std::string& utf8)
    {
        return fs::path (std::u8string (reinterpret_cast<const char8_t*> (utf8.data()), utf8.size()));
    }

    std::string text (const fs::path& p)
    {
        const auto u = p.u8string();
        return std::string (reinterpret_cast<const char*> (u.data()), u.size());
    }

    Validacio validaNom (const std::string& nom)
    {
        Validacio v;

        if (nom.empty() || nom.find_first_not_of (' ') == std::string::npos)
        {
            v.missatge = "Escriu un nom.";
            return v;
        }

        for (unsigned char c : nom)
        {
            if (c < 32 || std::string ("\\/:*?\"<>|").find ((char) c) != std::string::npos)
            {
                v.missatge = "Els noms no poden portar \\ / : * ? \" < > |";
                return v;
            }
        }

        if (nom.back() == '.' || nom.back() == ' ')
        {
            v.missatge = "El nom no pot acabar amb un punt ni amb un espai.";
            return v;
        }

        if (nom.front() == ' ')
        {
            v.missatge = "El nom no pot començar amb un espai.";
            return v;
        }

        if (nom.size() > 120)
        {
            v.missatge = "El nom és massa llarg.";
            return v;
        }

        // Noms reservats de Windows (també amb extensió: «con.txt»).
        auto base = minuscules (nom.substr (0, nom.find ('.')));
        static const char* reservats[] = { "con", "prn", "aux", "nul",
                                           "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
                                           "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9" };
        for (auto* r : reservats)
        {
            if (base == r)
            {
                v.missatge = "Windows no deixa fer servir aquest nom.";
                return v;
            }
        }

        v.correcte = true;
        return v;
    }

    std::string netejaNom (const std::string& nom)
    {
        std::string r;
        for (unsigned char c : nom)
            r += (c < 32 || std::string ("\\/:*?\"<>|").find ((char) c) != std::string::npos) ? '_' : (char) c;

        while (! r.empty() && (r.back() == '.' || r.back() == ' '))
            r.pop_back();
        while (! r.empty() && r.front() == ' ')
            r.erase (0, 1);

        return r.empty() ? std::string ("Pista") : r;
    }

    fs::path carpetaDe (const fs::path& carpetaMare, const std::string& nom)
    {
        return carpetaMare / cami (nom);
    }

    Resultat creaProjecte (const fs::path& carpetaMare, const Projecte& projecte,
                           const std::string& araIso, const std::string& araText)
    {
        Resultat r;

        const auto v = validaNom (projecte.nom);
        if (! v.correcte)
        {
            r.error = v.missatge;
            return r;
        }

        std::error_code ec;
        if (! fs::is_directory (carpetaMare, ec))
        {
            r.error = "La carpeta on s'havia de crear no existeix.";
            return r;
        }

        const auto carpeta = carpetaDe (carpetaMare, projecte.nom);
        if (fs::exists (carpeta, ec))
        {
            r.error = "Ja hi ha una carpeta que es diu «" + projecte.nom + "».";
            return r;
        }

        if (! fs::create_directory (carpeta, ec) || ec)
        {
            r.error = "No s'ha pogut crear la carpeta del projecte.";
            return r;
        }

        fs::create_directory (carpeta / cami (carpetaAudios), ec);
        fs::create_directory (carpeta / cami (carpetaCopies), ec);

        Projecte p = projecte;
        p.creat = araIso;
        p.jsonOriginal.clear();

        const auto fitxer = carpeta / cami (projecte.nom + extensio);
        auto desat = desa (fitxer, p, araIso, araIso);
        if (! desat.correcte)
            return desat;

        // Diari.
        std::ostringstream que;
        que << "Projecte creat (" << (p.plantilla == "buit" ? std::string ("buit") : "plantilla «" + p.plantilla + "»")
            << ") · " << (int) p.tempo << " bpm · " << p.compasNum << "/" << p.compasDen;
        escriuText (carpeta / cami (fitxerEvolucio), capcaleraEvolucio (p.nom), nullptr);
        afegeixEvolucio (carpeta, araText, que.str());

        r.correcte = true;
        r.fitxer = fitxer;
        return r;
    }

    std::optional<Projecte> llegeix (const fs::path& fitxer, std::string* error)
    {
        std::string contingut;
        if (! llegeixText (fitxer, contingut))
        {
            if (error) *error = "No s'ha pogut obrir el fitxer.";
            return std::nullopt;
        }

        json j = json::parse (contingut, nullptr, false);
        if (j.is_discarded() || ! j.is_object())
        {
            if (error) *error = "El fitxer del projecte està malmès (no és un projecte de Mibase vàlid).";
            return std::nullopt;
        }

        Projecte p;
        p.nom = j.value ("nom", text (fitxer.stem()));
        p.tempo = j.value ("tempo", 120.0);
        if (j.contains ("compas") && j["compas"].is_object())
        {
            p.compasNum = j["compas"].value ("num", 4);
            p.compasDen = j["compas"].value ("den", 4);
        }
        p.plantilla = j.value ("plantilla", std::string ("buit"));
        p.creat = j.value ("creat", std::string());
        p.modificat = j.value ("modificat", std::string());
        p.jsonOriginal = contingut;

        if (p.tempo < 20.0 || p.tempo > 400.0) p.tempo = 120.0;
        if (p.compasNum < 1 || p.compasNum > 32) p.compasNum = 4;
        if (p.compasDen != 2 && p.compasDen != 4 && p.compasDen != 8 && p.compasDen != 16) p.compasDen = 4;
        return p;
    }

    Resultat desa (const fs::path& fitxer, Projecte p, const std::string& araIso, const std::string& araPerNom)
    {
        Resultat r;
        r.fitxer = fitxer;

        json j = json::object();
        if (! p.jsonOriginal.empty())
        {
            auto original = json::parse (p.jsonOriginal, nullptr, false);
            if (original.is_object())
                j = original;   // conserva el que aquesta versió no coneix
        }

        p.modificat = araIso;
        if (! j.contains ("format"))
        {
            // Projecte nou: aquests dos camps primer.
            json primer = json::object();
            primer["format"] = "mibase";
            primer["versioFormat"] = 1;
            primer.update (j);
            j = primer;
        }
        j["format"] = "mibase";
        j["versioFormat"] = 1;
        j["nom"] = p.nom;
        j["tempo"] = p.tempo;
        j["compas"] = { { "num", p.compasNum }, { "den", p.compasDen } };
        j["plantilla"] = p.plantilla;
        j["creat"] = p.creat;
        j["modificat"] = p.modificat;
        if (! j.contains ("pistes"))
            j["pistes"] = json::array();

        std::error_code ec;

        // Còpia de l'anterior, si n'hi havia.
        if (fs::exists (fitxer, ec))
        {
            const auto copies = fitxer.parent_path() / cami (carpetaCopies);
            fs::create_directories (copies, ec);

            auto desti = copies / cami (p.nom + " " + araPerNom + extensio);
            for (int n = 2; fs::exists (desti, ec) && n < 100; ++n)
                desti = copies / cami (p.nom + " " + araPerNom + " (" + std::to_string (n) + ")" + extensio);

            fs::copy_file (fitxer, desti, fs::copy_options::overwrite_existing, ec);

            // Només les 10 últimes.
            std::vector<fs::path> totes;
            for (auto& e : fs::directory_iterator (copies, ec))
                if (e.is_regular_file (ec) && e.path().extension() == extensio)
                    totes.push_back (e.path());

            std::sort (totes.begin(), totes.end(), [] (const fs::path& a, const fs::path& b)
            {
                return fs::last_write_time (a) != fs::last_write_time (b)
                    ? fs::last_write_time (a) < fs::last_write_time (b)
                    : a.filename() < b.filename();
            });

            while ((int) totes.size() > maxCopies)
            {
                fs::remove (totes.front(), ec);
                totes.erase (totes.begin());
            }
        }

        std::string error;
        if (! escriuText (fitxer, j.dump (2) + "\n", &error))
        {
            r.error = error;
            return r;
        }

        r.correcte = true;
        return r;
    }

    bool afegeixEvolucio (const fs::path& carpetaProjecte, const std::string& quan, const std::string& entrada)
    {
        const auto f = carpetaProjecte / cami (fitxerEvolucio);
        std::string anterior;
        if (! llegeixText (f, anterior) || anterior.empty())
            anterior = capcaleraEvolucio (text (carpetaProjecte.filename()));

        // El títol és fins a la primera línia en blanc.
        auto fiCapcalera = anterior.find ("\n\n");
        if (fiCapcalera == std::string::npos)
        {
            anterior = capcaleraEvolucio (text (carpetaProjecte.filename())) + anterior;
            fiCapcalera = anterior.find ("\n\n");
        }
        fiCapcalera += 2;

        const std::string nou = anterior.substr (0, fiCapcalera) + quan + " · " + entrada + "\n" + anterior.substr (fiCapcalera);
        return escriuText (f, nou, nullptr);
    }

    std::vector<EntradaEvolucio> llegeixEvolucio (const fs::path& carpetaProjecte, int maxim)
    {
        std::vector<EntradaEvolucio> r;
        std::string contingut;
        if (! llegeixText (carpetaProjecte / cami (fitxerEvolucio), contingut))
            return r;

        std::istringstream ss (contingut);
        std::string linia;
        bool capcalera = true;
        while (std::getline (ss, linia) && (int) r.size() < maxim)
        {
            if (! linia.empty() && linia.back() == '\r')
                linia.pop_back();

            if (capcalera)
            {
                if (linia.empty())
                    capcalera = false;
                continue;
            }

            if (linia.empty())
                continue;

            const auto sep = linia.find (" · ");
            if (sep == std::string::npos)
                r.push_back ({ "", linia });
            else
                r.push_back ({ linia.substr (0, sep), linia.substr (sep + std::string (" · ").size()) });
        }
        return r;
    }

    std::string nomPresa (const fs::path& carpetaAudios, const std::string& nomPista)
    {
        const auto prefix = netejaNom (nomPista) + " - presa ";
        int maxim = 0;

        std::error_code ec;
        for (auto& e : fs::directory_iterator (carpetaAudios, ec))
        {
            const auto nom = text (e.path().filename());
            if (nom.rfind (prefix, 0) != 0 || ! acabaAmb (minuscules (nom), ".wav"))
                continue;

            const auto numero = nom.substr (prefix.size(), nom.size() - prefix.size() - 4);
            if (! numero.empty() && std::all_of (numero.begin(), numero.end(), [] (unsigned char c) { return std::isdigit (c); }))
                maxim = std::max (maxim, std::stoi (numero));
        }

        char num[16];
        std::snprintf (num, sizeof (num), "%02d", maxim + 1);
        return prefix + num + ".wav";
    }

    int comptaPreses (const fs::path& carpetaProjecte)
    {
        int n = 0;
        std::error_code ec;
        for (auto& e : fs::directory_iterator (carpetaProjecte / cami (carpetaAudios), ec))
            if (e.is_regular_file (ec) && minuscules (text (e.path().extension())) == ".wav")
                ++n;
        return n;
    }

    fs::path fitxerDe (const fs::path& carpeta)
    {
        std::error_code ec;
        auto preferit = carpeta / carpeta.filename();
        preferit += extensio;
        if (fs::is_regular_file (preferit, ec))
            return preferit;

        for (auto& e : fs::directory_iterator (carpeta, ec))
            if (e.is_regular_file (ec) && e.path().extension() == extensio)
                return e.path();

        return {};
    }

    bool esCarpetaDeProjecte (const fs::path& carpeta)
    {
        return ! fitxerDe (carpeta).empty();
    }

    int comptaProjectes (const fs::path& carpeta)
    {
        int n = 0;
        std::error_code ec;
        for (auto& e : fs::directory_iterator (carpeta, fs::directory_options::skip_permission_denied, ec))
            if (e.is_directory (ec) && esCarpetaDeProjecte (e.path()))
                ++n;
        return n;
    }
}
