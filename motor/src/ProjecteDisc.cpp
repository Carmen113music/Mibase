#include "mibase/ProjecteDisc.h"
#include "mibase/PresetsPads.h"
#include "mibase/Harmonia.h"
#include "mibase/Edicio.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::ordered_json;   // conserva l'ordre dels camps (més fàcil de llegir)

namespace
{
    // 0.19: inserts (plugins d'efecte).
    nlohmann::json insertsAJson (const std::vector<mibase::Insert>& v)
    {
        auto a = nlohmann::json::array();
        for (const auto& x : v)
            a.push_back ({ { "id", x.id }, { "plugin", x.plugin }, { "nom", x.nom }, { "actiu", x.actiu }, { "estat", x.estat } });
        return a;
    }

    std::vector<mibase::Insert> insertsDeJson (const nlohmann::json& j, const char* clau)
    {
        std::vector<mibase::Insert> v;
        if (! j.contains (clau) || ! j[clau].is_array()) return v;
        for (const auto& e : j[clau])
        {
            if (! e.is_object()) continue;
            mibase::Insert x;
            x.id = e.value ("id", std::string());
            x.plugin = e.value ("plugin", std::string());
            x.nom = e.value ("nom", std::string());
            x.actiu = e.value ("actiu", true);
            x.estat = e.value ("estat", std::string());
            if (! x.id.empty()) v.push_back (std::move (x));
        }
        return v;
    }
}

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
        p.comptador = j.value ("comptador", 0);
        p.clic = j.value ("clic", true);
        p.clicVolum = std::clamp (j.value ("clicVolum", 1.0), 0.0, 1.5);
        p.clicSo = std::clamp (j.value ("clicSo", 0), 0, 3);
        p.masterDb = std::clamp (j.value ("masterDb", 0.0), -60.0, 6.0);
        p.sortidaMaster = std::clamp (j.value ("sortidaMaster", 0), 0, 31);
        p.insertsMaster = insertsDeJson (j, "insertsMaster");
        {
            const auto snap = j.value ("snap", std::string ("compas"));
            p.snap = snap == "temps" ? Snap::Temps : snap == "lliure" ? Snap::Lliure : Snap::Compas;
            p.graella = std::clamp (j.value ("graella", 960LL), 30LL, 3840LL);
        }

        if (j.contains ("preses") && j["preses"].is_array())
        {
            for (auto& e : j["preses"])
            {
                if (! e.is_object()) continue;
                Presa s;
                s.id = e.value ("id", std::string());
                s.fitxer = e.value ("fitxer", std::string());
                s.nom = e.value ("nom", s.fitxer);
                s.durada = e.value ("durada", 0.0);
                s.primer = e.value ("primer", -1.0);
                if (e.contains ("veu") && e["veu"].is_object())
                {
                    const auto& v = e["veu"];
                    s.veu.analitzada = v.value ("analitzada", false);
                    s.veu.transicio = std::clamp (v.value ("transicio", 0.06), 0.0, 0.5);
                    if (v.contains ("tonalitat") && v["tonalitat"].is_array() && v["tonalitat"].size() == 3)
                    {
                        s.veu.modeTon = std::clamp (v["tonalitat"][0].get<int>(), 0, 2);
                        s.veu.to = std::clamp (v["tonalitat"][1].get<int>(), 0, 11);
                        s.veu.escala = std::max (0, v["tonalitat"][2].get<int>());
                    }
                    if (v.contains ("notes") && v["notes"].is_array())
                        for (auto& n : v["notes"])
                        {
                            if (! n.is_array() || n.size() < 8) continue;
                            NotaVeu x;
                            x.ini = n[0].get<double>(); x.fi = n[1].get<double>(); x.centre = n[2].get<double>();
                            x.desplac = n[3].get<double>(); x.modulacio = n[4].get<double>(); x.deriva = n[5].get<double>();
                            x.formant = n[6].get<double>(); x.guanyDb = n[7].get<double>();
                            if (x.fi > x.ini) s.veu.notes.push_back (x);
                        }
                }
                if (! s.id.empty()) p.preses.push_back (s);
            }
        }

        if (j.contains ("pistes") && j["pistes"].is_array())
        {
            for (auto& e : j["pistes"])
            {
                if (! e.is_object()) continue;
                Pista t;
                t.id = e.value ("id", std::string());
                t.nom = e.value ("nom", std::string ("Pista"));
                t.tipus = e.value ("tipus", std::string ("audio"));
                t.mut = e.value ("mut", false);
                t.solo = e.value ("solo", false);
                t.grava = e.value ("grava", false);
                t.faderDb = e.value ("faderDb", 0.0);
                t.canals = std::clamp (e.value ("canals", 1), 1, 2);
                t.color = std::clamp (e.value ("color", (int) p.pistes.size() % 8), 0, 7);
                t.pan = std::clamp (e.value ("pan", 0.0), -1.0, 1.0);
                t.entrada = std::clamp (e.value ("entrada", 0), 0, 63);
                t.rgb = e.value ("rgb", (std::uint32_t) 0);
                t.instrument = e.value ("instrument", std::string());
                t.nomInstrument = e.value ("nomInstrument", std::string());
                t.estatInstrument = e.value ("estatInstrument", std::string());
                t.inserts = insertsDeJson (e, "inserts");
                t.entradaMidi = e.value ("entradaMidi", std::string());
                t.entradaMidiNom = e.value ("entradaMidiNom", std::string());
                t.presetPads = e.value ("presetPads", std::string());
                if (e.contains ("pads") && e["pads"].is_array())
                    for (auto& x : e["pads"]) t.pads.push_back (x.is_string() ? x.get<std::string>() : std::string());
                t.pads = pads::adapta (t.pads, t.presetPads);   // la 0.9 en desava 16
                if (t.pads.empty() && t.presetPads == "Acords de la tonalitat") t.presetPads.clear();
                t.carpeta = e.value ("carpeta", std::string());
                // 0.33: sortida i enviaments (els projectes d'abans no en tenen: tot va a la sortida).
                t.sortida = e.value ("sortida", std::string());
                t.segueixAcords = e.value ("segueixAcords", false);   // 0.37
                if (e.contains ("enviaments") && e["enviaments"].is_array())
                    for (auto& v : e["enviaments"])
                    {
                        if (! v.is_object()) continue;
                        Enviament x;
                        x.id = v.value ("id", std::string());
                        x.desti = v.value ("desti", std::string());
                        x.nivellDb = std::clamp (v.value ("nivellDb", 0.0), -60.0, 6.0);
                        x.pan = std::clamp (v.value ("pan", 0.0), -1.0, 1.0);
                        x.actiu = v.value ("actiu", true);
                        x.mut = v.value ("mut", false);
                        const std::string m = v.value ("mode", std::string ("post"));
                        x.mode = m == "pre" ? 0 : (m == "postpan" ? 2 : 1);
                        if (! x.desti.empty()) t.enviaments.push_back (x);
                    }
                t.escalaTeclat = e.value ("escalaTeclat", true);
                t.modeEscala = std::clamp (e.value ("modeEscala", -1), -1, 2);
                t.kit = e.value ("kit", std::string());
                t.padsPerOrdre = e.value ("padsPerOrdre", false);
                if (t.tipus == "bateria") { t.tipus = "midi"; t.instrument = instrumentKit; }   // 0.39: la bateria és un instrument, no un tipus de pista
                if (e.contains ("files") && e["files"].is_array())
                    for (auto& f : e["files"])
                    {
                        if (! f.is_object()) continue;
                        FilaBateria x;
                        x.tipus = f.value ("tipus", std::string ("altres"));
                        x.so = f.value ("so", std::string());
                        x.nom = f.value ("nom", std::string());
                        x.nota = std::clamp (f.value ("nota", 36), 0, 127);
                        x.volumDb = std::clamp (f.value ("volumDb", 0.0), -40.0, 6.0);
                        x.pan = std::clamp (f.value ("pan", 0.0), -1.0, 1.0);
                        x.afina = std::clamp (f.value ("afina", 0.0), -12.0, 12.0);
                        x.decai = std::clamp (f.value ("decai", 1.0), 0.05, 1.0);
                        x.talla = std::clamp (f.value ("talla", 0), 0, 8);
                        x.mut = f.value ("mut", false);
                        t.files.push_back (x);
                    }
                if (e.contains ("guitarra") && e["guitarra"].is_object())   // 0.44
                {
                    auto& g = e["guitarra"];
                    t.guitarra.volumDb = std::clamp (g.value ("volumDb", 0.0), -24.0, 6.0);
                    t.guitarra.to = std::clamp (g.value ("to", 1.0), 0.0, 1.0);
                    t.guitarra.atac = std::clamp (g.value ("atac", 0.0), -1.0, 1.0);
                    t.guitarra.sustain = std::clamp (g.value ("sustain", 0.5), 0.0, 1.0);
                    t.guitarra.octava = std::clamp (g.value ("octava", 0), -2, 2);
                    t.guitarra.afina = std::clamp (g.value ("afina", 0.0), -50.0, 50.0);
                }
                if (e.contains ("sampler") && e["sampler"].is_object())
                {
                    auto& m = e["sampler"];
                    auto& x = t.sampler;
                    x.fitxer = m.value ("fitxer", std::string());
                    x.nom = m.value ("nom", std::string());
                    x.inici = std::max (0.0, m.value ("inici", 0.0));
                    x.fi = m.value ("fi", -1.0);
                    x.bucleDe = m.value ("bucleDe", -1.0);
                    x.bucleA = m.value ("bucleA", -1.0);
                    x.modeBucle = std::clamp (m.value ("modeBucle", 0), 0, 3);
                    x.crossfade = std::clamp (m.value ("crossfade", 0.02), 0.0, 1.0);
                    x.fadeIn = std::clamp (m.value ("fadeIn", 0.0), 0.0, 10.0);
                    x.fadeOut = std::clamp (m.value ("fadeOut", 0.005), 0.0, 10.0);
                    x.zeroCrossing = m.value ("zeroCrossing", true);
                    x.modeToc = std::clamp (m.value ("modeToc", 0), 0, 2);
                    x.veus = std::clamp (m.value ("veus", 0), 0, 2);
                    x.maxVeus = std::clamp (m.value ("maxVeus", 16), 1, 32);
                    x.guanyDb = std::clamp (m.value ("guanyDb", 0.0), -40.0, 24.0);
                    x.volDb = std::clamp (m.value ("volDb", 0.0), -60.0, 6.0);
                    x.pan = std::clamp (m.value ("pan", 0.0), -1.0, 1.0);
                    x.width = std::clamp (m.value ("width", 1.0), 0.0, 2.0);
                    x.root = std::clamp (m.value ("root", 60), 0, 127);
                    x.bpm = std::clamp (m.value ("bpm", 0.0), 0.0, 999.0);
                    x.tune = std::clamp (m.value ("tune", 0.0), -48.0, 48.0);
                    x.fine = std::clamp (m.value ("fine", 0.0), -100.0, 100.0);
                    x.speed = std::clamp (m.value ("speed", 1.0), 0.25, 4.0);
                    x.mantenTo = m.value ("mantenTo", true);
                    x.durFixa = m.value ("durFixa", true);
                    x.reversa = m.value ("reversa", false);
                    x.flat = m.value ("flat", false);
                    x.smono = m.value ("smono", false);
                    x.filtre = std::clamp (m.value ("filtre", 0), 0, 4);
                    x.pendent = m.value ("pendent", 12) == 24 ? 24 : 12;
                    x.cutoff = std::clamp (m.value ("cutoff", 20000.0), 20.0, 20000.0);
                    x.reso = std::clamp (m.value ("reso", 0.0), 0.0, 1.0);
                    x.drive = std::clamp (m.value ("drive", 0.0), 0.0, 1.0);
                    x.atac = std::clamp (m.value ("atac", 0.002), 0.0, 10.0);
                    x.decai = std::clamp (m.value ("decai", 0.2), 0.0, 10.0);
                    x.sosteniment = std::clamp (m.value ("sosteniment", 1.0), 0.0, 1.0);
                    x.alliberament = std::clamp (m.value ("alliberament", 0.15), 0.0, 20.0);
                    x.vel = std::clamp (m.value ("vel", 0.7), 0.0, 1.0);
                    x.entradaGrava = std::clamp (m.value ("entradaGrava", 0), 0, 63);
                    x.canalsGrava = m.value ("canalsGrava", 1) == 2 ? 2 : 1;
                }
                if (esMidi (t)) t.canals = 2;
                if (e.contains ("clips") && e["clips"].is_array())
                {
                    for (auto& k : e["clips"])
                    {
                        if (! k.is_object()) continue;
                        Clip c;
                        c.presa = k.value ("presa", std::string());
                        c.pos = k.value ("pos", 0.0);
                        c.inici = k.value ("inici", 0.0);
                        c.fi = k.value ("fi", c.inici + 1.0);
                        c.fadeIn = k.value ("fadeIn", 0.0);
                        c.fadeOut = k.value ("fadeOut", 0.0);
                        c.guanyDb = k.value ("guanyDb", 0.0);
                        c.mut = k.value ("mut", false);
                        c.rgb = k.value ("rgb", (std::uint32_t) 0);
                        c.midi = k.value ("midi", false);
                        c.acord = k.value ("acord", std::string());   // 0.36
if (k.contains ("blocs") && k["blocs"].is_array())   // 0.40
                            for (auto& b : k["blocs"])
                            {
                                if (! b.is_object()) continue;
                                BlocPatro x;
                                x.ini = b.value ("ini", 0LL);
                                x.dur = b.value ("dur", 0LL);
                                x.patro = b.value ("patro", std::string());
                                x.nom = b.value ("nom", std::string());
                                x.passos = b.value ("passos", std::string());
                                x.pasTicks = b.value ("pasTicks", 480LL);
                                x.msCorda = b.value ("msCorda", 0.0);
                                x.crescendo = b.value ("crescendo", 0);
                                x.variacio = std::clamp (b.value ("variacio", 0.0), 0.0, 1.0);
                                x.llavor = b.value ("llavor", 1u);
                                x.vel = std::clamp (b.value ("vel", 88), 1, 127);
                                x.fix = b.value ("fix", false);
                                if (x.dur <= 0) continue;
                                c.blocs.push_back (x);
                                // 0.40 desava l'acord dins del bloc: ara va a la llista d'acords del clip.
                                if (const std::string ac = b.value ("acord", std::string()); ! ac.empty()) c.acords.push_back ({ x.ini, x.dur, ac });
                            }
                        if (k.contains ("acordsClip") && k["acordsClip"].is_array())   // 0.41
                            for (auto& b : k["acordsClip"])
                                if (b.is_object() && b.value ("dur", 0LL) > 0 && ! b.value ("acord", std::string()).empty())
                                    c.acords.push_back ({ b.value ("ini", 0LL), b.value ("dur", 0LL), b.value ("acord", std::string()) });
                        // Notes: [inici, durada, to, velocitat, canal] (ticks).
                        if (k.contains ("notes") && k["notes"].is_array())
                            for (auto& n : k["notes"])
                            {
                                if (! n.is_array() || n.size() < 4) continue;
                                Nota nota;
                                if (! n[0].is_number() || ! n[1].is_number() || ! n[2].is_number() || ! n[3].is_number()) continue;
                                nota.ini = n[0].get<long long>();
                                nota.dur = std::max (1LL, n[1].get<long long>());
                                nota.to = std::clamp (n[2].get<int>(), 0, 127);
                                nota.vel = std::clamp (n[3].get<int>(), 1, 127);
                                nota.canal = n.size() > 4 && n[4].is_number() ? std::clamp (n[4].get<int>(), 0, 15) : 0;
                                c.notes.push_back (nota);
                            }
                        // Altres esdeveniments: [tick, tipus, número, valor, canal].
                        if (k.contains ("events") && k["events"].is_array())
                            for (auto& n : k["events"])
                            {
                                if (! n.is_array() || n.size() < 4) continue;
                                EventMidi ev;
                                if (! n[0].is_number() || ! n[1].is_number() || ! n[2].is_number() || ! n[3].is_number()) continue;
                                ev.tick = n[0].get<long long>();
                                ev.tipus = n[1].get<int>() & 0xF0;
                                ev.num = n[2].get<int>();
                                ev.valor = n[3].get<int>();
                                ev.canal = n.size() > 4 && n[4].is_number() ? std::clamp (n[4].get<int>(), 0, 15) : 0;
                                c.events.push_back (ev);
                            }
                        if (c.fi <= c.inici) c.fi = c.inici + edicio::longitudMinima;
                        t.clips.push_back (c);
                    }
                }
                if (t.id.empty()) t.id = "t" + std::to_string (++p.comptador);
                p.pistes.push_back (t);
            }
            // 0.34: els grups i retorns, a baix de tot; 0.36: la pista d'Acords, a dalt
            std::stable_partition (p.pistes.begin(), p.pistes.end(), [] (const Pista& q) { return ! esBus (q); });
            std::stable_partition (p.pistes.begin(), p.pistes.end(), [] (const Pista& q) { return esAcords (q); });
        }

        if (j.contains ("carpetes") && j["carpetes"].is_array())
            for (auto& e : j["carpetes"])
                if (e.is_object())
                    p.carpetes.push_back ({ e.value ("id", std::string()), e.value ("nom", std::string()),
                                            e.value ("plegada", false), e.value ("amagada", false) });
        for (auto& t : p.pistes)   // una carpeta que ja no hi és: la pista queda fora
            if (! t.carpeta.empty()
                && std::none_of (p.carpetes.begin(), p.carpetes.end(), [&t] (const Carpeta& c) { return c.id == t.carpeta; }))
                t.carpeta.clear();
        if (j.contains ("estructures") && j["estructures"].is_array())
            for (auto& e : j["estructures"])
            {
                if (! e.is_object()) continue;
                Estructura es { e.value ("id", std::string()), e.value ("nom", std::string()), {} };
                if (e.contains ("peces") && e["peces"].is_array())
                    for (auto& pc : e["peces"])
                    {
                        if (! pc.is_object()) continue;
                        PecaEstructura x { pc.value ("part", std::string()), {} };
                        if (pc.contains ("silencis") && pc["silencis"].is_array())
                            for (auto& s : pc["silencis"]) if (s.is_string()) x.silencis.push_back (s.get<std::string>());
                        es.peces.push_back (x);
                    }
                p.estructures.push_back (es);
            }
        p.estructuraActiva = j.value ("estructuraActiva", std::string());
        if (j.contains ("parts") && j["parts"].is_array())
            for (auto& e : j["parts"])
                if (e.is_object())
                {
                    Part x { e.value ("id", std::string()), e.value ("nom", std::string()), e.value ("de", 0.0), e.value ("a", 0.0),
                             e.value ("rgb", (std::uint32_t) 0) };
                    x.tempo = std::clamp (e.value ("tempo", 0.0), 0.0, 400.0);
                    x.compasNum = std::clamp (e.value ("compasNum", 0), 0, 32);
                    x.compasDen = std::clamp (e.value ("compasDen", 0), 0, 32);
                    if (x.tempo > 0.0 && x.tempo < 20.0) x.tempo = 0.0;
                    p.parts.push_back (x);
                }
        if (j.contains ("bucle") && j["bucle"].is_object())
        {
            p.bucle.marcat = j["bucle"].value ("marcat", false);
            p.bucle.actiu = j["bucle"].value ("actiu", false);
            p.bucle.de = j["bucle"].value ("de", 0.0);
            p.bucle.a = j["bucle"].value ("a", 0.0);
        }

        if (j.contains ("tonalitat") && j["tonalitat"].is_object())
        {
            const auto& t = j["tonalitat"];
            auto llegeixTon = [] (const json& x)
            {
                Tonalitat r;
                r.to = std::clamp (x.value ("to", 0), 0, 11);
                r.escala = std::max (0, harmonia::escalaPerId (x.value ("escala", std::string ("major"))));
                return r;
            };
            p.tonalitatAuto = t.value ("automatica", true);
            p.tonalitat = llegeixTon (t);
            const auto mode = t.value ("mode", std::string ("visual"));
            p.modeEscala = mode == "assistit" ? ModeEscala::Assistit : mode == "bloquejat" ? ModeEscala::Bloquejat : ModeEscala::Visual;
            p.ressaltaAcord = t.value ("acord", true);
            if (t.contains ("canvis") && t["canvis"].is_array())
                for (auto& c : t["canvis"])
                    if (c.is_object())
                        p.canvisTonalitat.push_back ({ c.value ("compas", 0), llegeixTon (c) });
        }

        p.jsonOriginal = contingut;

        if (p.tempo < 20.0 || p.tempo > 400.0) p.tempo = 120.0;
        if (p.compasNum < 1 || p.compasNum > 32) p.compasNum = 4;
        if (p.compasDen != 2 && p.compasDen != 4 && p.compasDen != 8 && p.compasDen != 16) p.compasDen = 4;
        return p;
    }

    Resultat desa (const fs::path& fitxer, Projecte p, const std::string& araIso, const std::string& araPerNom, bool ferCopia)
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
        j["snap"] = p.snap == Snap::Temps ? "temps" : p.snap == Snap::Lliure ? "lliure" : "compas";
        if (p.graella != 960) j["graella"] = p.graella;
        j["clic"] = p.clic;
        j["clicVolum"] = p.clicVolum;
        j["clicSo"] = p.clicSo;
        j["masterDb"] = p.masterDb;
        j["sortidaMaster"] = p.sortidaMaster;
        if (! p.insertsMaster.empty()) j["insertsMaster"] = insertsAJson (p.insertsMaster);
        j["comptador"] = p.comptador;
        {
            json canvis = json::array();
            for (auto& c : p.canvisTonalitat)
                canvis.push_back ({ { "compas", c.compas }, { "to", c.tonalitat.to },
                                    { "escala", harmonia::escales()[(size_t) std::clamp (c.tonalitat.escala, 0, (int) harmonia::escales().size() - 1)].id } });
            j["tonalitat"] = { { "automatica", p.tonalitatAuto }, { "to", p.tonalitat.to },
                               { "escala", harmonia::escales()[(size_t) std::clamp (p.tonalitat.escala, 0, (int) harmonia::escales().size() - 1)].id },
                               { "mode", p.modeEscala == ModeEscala::Assistit ? "assistit" : p.modeEscala == ModeEscala::Bloquejat ? "bloquejat" : "visual" },
                               { "acord", p.ressaltaAcord }, { "canvis", canvis } };
        }
        {
            json carpetes = json::array();
            for (auto& c : p.carpetes)
                carpetes.push_back ({ { "id", c.id }, { "nom", c.nom }, { "plegada", c.plegada }, { "amagada", c.amagada } });
            j["carpetes"] = carpetes;
            json parts = json::array();
            for (auto& c : p.parts)
            {
                json x = { { "id", c.id }, { "nom", c.nom }, { "de", c.de }, { "a", c.a }, { "rgb", c.rgb } };
                if (c.tempo > 0.0) x["tempo"] = c.tempo;
                if (c.compasNum > 0) { x["compasNum"] = c.compasNum; x["compasDen"] = c.compasDen; }
                parts.push_back (x);
            }
            j["parts"] = parts;
            json estructures = json::array();
            for (auto& e : p.estructures)
            {
                json peces = json::array();
                for (auto& pc : e.peces) peces.push_back ({ { "part", pc.part }, { "silencis", pc.silencis } });
                estructures.push_back ({ { "id", e.id }, { "nom", e.nom }, { "peces", peces } });
            }
            j["estructures"] = estructures;
            j["estructuraActiva"] = p.estructuraActiva;
        }
        j["bucle"] = { { "marcat", p.bucle.marcat }, { "actiu", p.bucle.actiu }, { "de", p.bucle.de }, { "a", p.bucle.a } };

        json preses = json::array();
        for (auto& s : p.preses)
        {
            json e = { { "id", s.id }, { "fitxer", s.fitxer }, { "nom", s.nom }, { "durada", s.durada }, { "primer", s.primer } };
            if (s.veu.analitzada || ! s.veu.notes.empty())
            {
                json notes = json::array();
                for (auto& n : s.veu.notes)
                    notes.push_back ({ n.ini, n.fi, n.centre, n.desplac, n.modulacio, n.deriva, n.formant, n.guanyDb });
                e["veu"] = { { "analitzada", s.veu.analitzada }, { "transicio", s.veu.transicio },
                             { "tonalitat", { s.veu.modeTon, s.veu.to, s.veu.escala } }, { "notes", notes } };
            }
            preses.push_back (e);
        }
        j["preses"] = preses;

        json pistes = json::array();
        for (auto& t : p.pistes)
        {
            json clips = json::array();
            for (auto& c : t.clips)
            {
                json k = { { "presa", c.presa }, { "pos", c.pos }, { "inici", c.inici }, { "fi", c.fi },
                           { "fadeIn", c.fadeIn }, { "fadeOut", c.fadeOut }, { "guanyDb", c.guanyDb }, { "mut", c.mut },
                           { "rgb", c.rgb } };
                if (c.esAcord()) k["acord"] = c.acord;   // 0.36
                if (! c.blocs.empty())   // 0.40
                {
                    json bl = json::array();
                    for (auto& b : c.blocs)
                        bl.push_back ({ { "ini", b.ini }, { "dur", b.dur }, { "patro", b.patro }, { "nom", b.nom },
                                        { "passos", b.passos }, { "pasTicks", b.pasTicks }, { "msCorda", b.msCorda },
                                        { "crescendo", b.crescendo }, { "variacio", b.variacio }, { "llavor", b.llavor }, { "vel", b.vel },
                                        { "fix", b.fix } });
                    k["blocs"] = bl;
                }
                if (! c.acords.empty())   // 0.41
                {
                    json al = json::array();
                    for (auto& x : c.acords) al.push_back ({ { "ini", x.ini }, { "dur", x.dur }, { "acord", x.acord } });
                    k["acordsClip"] = al;
                }
                if (c.midi)
                {
                    k["midi"] = true;
                    json notes = json::array();
                    for (auto& n : c.notes) notes.push_back (json::array ({ n.ini, n.dur, n.to, n.vel, n.canal }));
                    k["notes"] = notes;
                    json events = json::array();
                    for (auto& e : c.events) events.push_back (json::array ({ e.tick, e.tipus, e.num, e.valor, e.canal }));
                    k["events"] = events;
                }
                clips.push_back (k);
            }
            json pista = { { "id", t.id }, { "nom", t.nom }, { "tipus", t.tipus }, { "canals", t.canals }, { "color", t.color },
                           { "mut", t.mut }, { "solo", t.solo },
                           { "grava", t.grava }, { "faderDb", t.faderDb }, { "pan", t.pan }, { "entrada", t.entrada }, { "rgb", t.rgb },
                           { "clips", clips } };
            if (! t.instrument.empty()) pista["instrument"] = t.instrument;
            if (! t.nomInstrument.empty()) { pista["nomInstrument"] = t.nomInstrument; pista["estatInstrument"] = t.estatInstrument; }
            if (! t.inserts.empty()) pista["inserts"] = insertsAJson (t.inserts);
            if (! t.entradaMidi.empty()) { pista["entradaMidi"] = t.entradaMidi; pista["entradaMidiNom"] = t.entradaMidiNom; }
            if (! t.carpeta.empty()) pista["carpeta"] = t.carpeta;
            if (! t.sortida.empty()) pista["sortida"] = t.sortida;
            if (t.segueixAcords) pista["segueixAcords"] = true;   // 0.37
            if (! t.enviaments.empty())
            {
                json ev = json::array();
                for (auto& x : t.enviaments)
                    ev.push_back ({ { "id", x.id }, { "desti", x.desti }, { "nivellDb", x.nivellDb }, { "pan", x.pan }, { "actiu", x.actiu },
                                    { "mut", x.mut }, { "mode", x.mode == 0 ? "pre" : (x.mode == 2 ? "postpan" : "post") } });
                pista["enviaments"] = ev;
            }
            if (! t.pads.empty()) { pista["pads"] = t.pads; pista["presetPads"] = t.presetPads; }
            if (! t.escalaTeclat) pista["escalaTeclat"] = false;
            if (t.modeEscala >= 0) pista["modeEscala"] = t.modeEscala;
            if (! (t.guitarra == ParamsGuitarra()))   // 0.44
                pista["guitarra"] = { { "volumDb", t.guitarra.volumDb }, { "to", t.guitarra.to }, { "atac", t.guitarra.atac },
                                      { "sustain", t.guitarra.sustain }, { "octava", t.guitarra.octava }, { "afina", t.guitarra.afina } };
            if (esSampler (t))
            {
                const auto& x = t.sampler;
                pista["sampler"] = { { "fitxer", x.fitxer }, { "nom", x.nom }, { "inici", x.inici }, { "fi", x.fi },
                    { "bucleDe", x.bucleDe }, { "bucleA", x.bucleA }, { "modeBucle", x.modeBucle }, { "crossfade", x.crossfade },
                    { "fadeIn", x.fadeIn }, { "fadeOut", x.fadeOut }, { "zeroCrossing", x.zeroCrossing }, { "modeToc", x.modeToc },
                    { "veus", x.veus }, { "maxVeus", x.maxVeus }, { "guanyDb", x.guanyDb }, { "volDb", x.volDb }, { "pan", x.pan },
                    { "width", x.width }, { "root", x.root }, { "bpm", x.bpm }, { "tune", x.tune }, { "fine", x.fine },
                    { "speed", x.speed }, { "mantenTo", x.mantenTo }, { "durFixa", x.durFixa }, { "reversa", x.reversa }, { "flat", x.flat }, { "smono", x.smono },
                    { "filtre", x.filtre }, { "pendent", x.pendent }, { "cutoff", x.cutoff }, { "reso", x.reso }, { "drive", x.drive },
                    { "atac", x.atac }, { "decai", x.decai }, { "sosteniment", x.sosteniment }, { "alliberament", x.alliberament },
                    { "vel", x.vel }, { "entradaGrava", x.entradaGrava }, { "canalsGrava", x.canalsGrava } };
            }
            if (! t.files.empty() || esKit (t))   // 0.39: les files són de la pista (les fa servir el kit i l'eina Drumpad)
            {
                json filesJ = json::array();
                for (auto& f : t.files)
                    filesJ.push_back ({ { "tipus", f.tipus }, { "so", f.so }, { "nom", f.nom }, { "nota", f.nota }, { "volumDb", f.volumDb },
                                    { "pan", f.pan }, { "afina", f.afina }, { "decai", f.decai }, { "talla", f.talla }, { "mut", f.mut } });
                pista["files"] = filesJ;
                pista["kit"] = t.kit;
                pista["padsPerOrdre"] = t.padsPerOrdre;
            }
            pistes.push_back (pista);
        }
        j["pistes"] = pistes;

        std::error_code ec;

        // Còpia de l'anterior, si n'hi havia.
        if (ferCopia && fs::exists (fitxer, ec))
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

    fs::path camiRelatiu (const fs::path& carpetaProjecte, const std::string& relatiu)
    {
        auto r = carpetaProjecte;
        std::string tros;
        for (char c : relatiu)
        {
            if (c == '/' || c == '\\')
            {
                if (! tros.empty()) r /= cami (tros);
                tros.clear();
            }
            else tros += c;
        }
        if (! tros.empty()) r /= cami (tros);
        return r;
    }

    Resultat novaVersio (const fs::path& fitxerActual, const Projecte& projecte, const std::string& nomNou,
                         const std::string& araIso, const std::string& araText)
    {
        Resultat r;
        const auto v = validaNom (nomNou);
        if (! v.correcte) { r.error = v.missatge; return r; }

        const auto carpeta = fitxerActual.parent_path();
        const auto desti = carpeta / cami (nomNou + extensio);
        std::error_code ec;
        if (fs::exists (desti, ec)) { r.error = "Ja hi ha una versió que es diu «" + nomNou + "»."; return r; }

        Projecte q = projecte;
        q.nom = nomNou;
        q.creat = araIso;
        auto d = desa (desti, q, araIso, araIso, false);
        if (! d.correcte) return d;

        afegeixEvolucio (carpeta, araText, "Nova versió «" + nomNou + "» a partir de «" + projecte.nom + "»");
        r.correcte = true;
        r.fitxer = desti;
        return r;
    }

    Resultat recopila (const fs::path& carpetaProjecte, const Projecte& projecte, const fs::path& destiMare,
                       const std::string& araIso, const std::string& araText)
    {
        Resultat r;
        std::error_code ec;
        if (! fs::is_directory (destiMare, ec)) { r.error = "La carpeta de destí no existeix."; return r; }

        const auto carpeta = carpetaDe (destiMare, projecte.nom);
        if (fs::exists (carpeta, ec)) { r.error = "En aquest lloc ja hi ha una carpeta que es diu «" + projecte.nom + "»."; return r; }
        if (! fs::create_directory (carpeta, ec) || ec) { r.error = "No s'ha pogut crear la carpeta."; return r; }
        fs::create_directory (carpeta / cami (carpetaAudios), ec);
        fs::create_directory (carpeta / cami (carpetaCopies), ec);

        // Només les preses que fa servir algun clip.
        std::vector<std::string> usades;
        for (auto& t : projecte.pistes)
            for (auto& c : t.clips)
                if (! c.presa.empty() && std::find (usades.begin(), usades.end(), c.presa) == usades.end())
                    usades.push_back (c.presa);

        Projecte q = projecte;
        q.preses.clear();
        int copiats = 0;
        for (auto& s : projecte.preses)
        {
            if (std::find (usades.begin(), usades.end(), s.id) == usades.end()) continue;
            q.preses.push_back (s);
            const auto origen = camiRelatiu (carpetaProjecte, s.fitxer);
            const auto desti = camiRelatiu (carpeta, s.fitxer);
            fs::create_directories (desti.parent_path(), ec);
            if (fs::copy_file (origen, desti, fs::copy_options::overwrite_existing, ec) && ! ec) ++copiats;
            else r.avisos.push_back ("No s'ha trobat «" + s.fitxer + "»");
        }
        // 0.26: els samples dels samplejadors.
        for (auto& t : projecte.pistes)
        {
            if (! esSampler (t) || t.sampler.fitxer.empty()) continue;
            const auto origen = camiRelatiu (carpetaProjecte, t.sampler.fitxer);
            const auto desti = camiRelatiu (carpeta, t.sampler.fitxer);
            fs::create_directories (desti.parent_path(), ec);
            if (fs::copy_file (origen, desti, fs::copy_options::overwrite_existing, ec) && ! ec) ++copiats;
            else r.avisos.push_back ("No s'ha trobat «" + t.sampler.fitxer + "»");
        }

        const auto fitxer = carpeta / cami (projecte.nom + extensio);
        auto d = desa (fitxer, q, araIso, araIso, false);
        if (! d.correcte) return d;

        fs::copy_file (carpetaProjecte / cami (fitxerEvolucio), carpeta / cami (fitxerEvolucio), fs::copy_options::overwrite_existing, ec);
        afegeixEvolucio (carpeta, araText, "Projecte recopilat: " + std::to_string (copiats) + " àudios, des de " + text (carpetaProjecte));

        r.correcte = true;
        r.fitxer = fitxer;
        return r;
    }
}
