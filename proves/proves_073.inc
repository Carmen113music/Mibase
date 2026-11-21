#include <chrono>
// 0.73: canvi de to en temps real (Harmonitzador, Octava i formant) i context musical.
#include "mibase/Harmonitzador.h"
#include "mibase/Veu.h"
#include "mibase/Rack.h"
#include "mibase/Doubler.h"
#include "mibase/Reverb.h"
#include "mibase/Edicio.h"
#include "mibase/Projecte.h"
namespace
{
    void proves073()
    {
        using namespace mibase;
        constexpr double fs = 48000.0;
        // Una «veu» amb to: harmònics que baixen (com els polsos de la glotis), amb una mica de vibrato.
        auto veuSintetica = [] (double hz, int n)
        {
            std::vector<float> v ((size_t) n);
            double fase = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double f = hz * (1.0 + 0.003 * std::sin (2.0 * 3.141592653589793 * 5.0 * i / fs));
                fase += 2.0 * 3.141592653589793 * f / fs;
                double s = 0.0;
                for (int k = 1; k <= 12; ++k) s += std::sin (k * fase) / (k * 1.2);
                v[(size_t) i] = (float) (0.25 * s);
            }
            return v;
        };
        auto passa = [] (Efecte& d, std::vector<float>& L, std::vector<float>& R, int bloc = 256)
        {
            for (int i = 0; i < (int) L.size(); i += bloc) d.processa (L.data() + i, R.data() + i, std::min (bloc, (int) L.size() - i));
        };
        // L'afinació que surt (mediana dels punts amb to després del primer mig segon), amb l'anàlisi del Voice Editor.
        auto afinacio = [] (const std::vector<float>& x)
        {
            const int de = (int) (0.5 * fs);
            auto a = veu::analitza (x.data() + de, (long long) x.size() - de, fs);
            std::vector<float> f;
            for (int i = 0; i < a.punts(); ++i) if (a.f0[(size_t) i] > 0.0f) f.push_back (a.f0[(size_t) i]);
            if (f.size() < 20) return 0.0f;
            std::nth_element (f.begin(), f.begin() + (long) f.size() / 2, f.end());
            return f[f.size() / 2];
        };
        auto aprop = [] (float hz, double esperat) { return hz > 0.0f && std::abs (12.0 * std::log2 (hz / esperat)) < 0.3; };
        auto finit = [] (const std::vector<float>& v) { return std::all_of (v.begin(), v.end(), [] (float x) { return std::isfinite (x); }); };

        prova ("0.73 Octava: una octava avall i una quinta amunt, sense perdre el to", [&]
        {
            for (auto [st, hz] : { std::pair { -12, 110.0 }, std::pair { 7, 329.63 }, std::pair { 12, 440.0 } })
            {
                octava::Octava o;
                o.prepara (fs, 256);
                o.posa (octava::To, (float) st);
                o.posa (octava::Mescla, 100);
                auto L = veuSintetica (220.0, (int) (2.5 * fs)), R = L;
                passa (o, L, R);
                const float f = afinacio (L);
                comprova (finit (L) && aprop (f, hz), "To " + std::to_string (st) + ": surt " + std::to_string (f) + " Hz (esperat " + std::to_string (hz) + ")");
            }
        });

        prova ("0.73 Octava: latència i veu seca alineada", [&]
        {
            octava::Octava o;
            o.prepara (fs, 256);
            o.posa (octava::Mescla, 0);
            const int lat = o.latencia();
            comprova (lat > 0 && lat < (int) (0.05 * fs), "latència d'uns 35 ms (" + std::to_string (lat) + " mostres)");
            auto L = veuSintetica (220.0, 20000), R = L;
            const auto L0 = L;
            passa (o, L, R, 333);
            bool igual = true;
            for (int i = lat; i < 20000 && igual; ++i) igual = std::abs (L[(size_t) i] - L0[(size_t) (i - lat)]) < 1e-5f;
            comprova (igual, "amb mescla 0, la sortida és l'entrada retardada exactament la latència");
        });

        prova ("0.73 Harmonitzador: tonalitat, acords i teclat", [&]
        {
            ContextMusical ctx;
            ctx.tonica.store (0);            // Do major
            ctx.mascaraEscala.store (0xAB5);
            auto prova1 = [&] (int mode, int interval, double esperat, const std::string& que)
            {
                harmonitzador::Harmonitzador h;
                h.posaContext (&ctx);
                h.prepara (fs, 256);
                h.posa (harmonitzador::ModeP, (float) mode);
                h.posa (harmonitzador::Sec, 0);
                h.posa (harmonitzador::Harmonies, 100);
                h.posa (harmonitzador::Humanitza, 0);
                h.posa (harmonitzador::Interval1, (float) interval);
                h.posa (harmonitzador::Interval1 + 1, 0);
                h.posa (harmonitzador::Pan1, 0);
                auto L = veuSintetica (220.0, (int) (2.5 * fs)), R = L;   // A3 (57)
                passa (h, L, R);
                const float f = afinacio (L);
                comprova (finit (L) && aprop (f, esperat), que + ": surt " + std::to_string (f) + " Hz (esperat " + std::to_string (esperat) + ")");
                return std::abs (h.notaDetectada() - 57.0f) < 0.3f;
            };
            comprova (prova1 (harmonitzador::Tonalitat, 2, 261.63, "Tonalitat Do major, 3a amunt de La = Do"), "detecta La3");
            prova1 (harmonitzador::Tonalitat, -2, 174.61, "Tonalitat Do major, 3a avall de La = Fa");
            ctx.publicaAcords ({ { 0.0, 100.0, (1 << 0) | (1 << 4) | (1 << 7), 0 } });   // Do major tot el temps
            ctx.segons.store (10.0);
            prova1 (harmonitzador::Acords, 2, 329.63, "Acords (Do), 2a de l'acord amunt de La = Mi");
            ctx.notaOn (64 + 7);   // Si4
            prova1 (harmonitzador::Teclat, 0, 493.88, "Teclat: la nota premuda (Si4)");
            ctx.totesOff();
            comprova (ctx.escoltantTeclat.load() == 0, "en desaparèixer, l'harmonitzador deixa d'escoltar el teclat");
        });

        prova ("0.73 Presets del Vocal Studio: noms d'artista i cançó, i tots sonen", [&]
        {
            const auto& ps = rack::presetsFabrica();
            int ambArtista = 0, ambVeusNoves = 0;
            bool tots = true;
            for (const auto& p : ps)
            {
                if (p.nom.find (" · ") != std::string::npos) ++ambArtista;
                rack::Rack r;
                r.posaEstat (p.estat);
                r.prepara (fs, 256);
                bool nou = false;
                for (int i = 0; i < r.numModuls(); ++i) nou = nou || r.modul (i)->clau == "harmonitzador" || r.modul (i)->clau == "octava";
                if (nou) { ++ambVeusNoves; tots = tots && r.latencia() > 0; }
                auto L = veuSintetica (196.0, (int) (0.6 * fs)), R = L;
                passa (r, L, R);
                tots = tots && finit (L) && finit (R);
            }
            comprova (ps.size() >= 28 && ambArtista >= 14 && ambVeusNoves >= 8 && tots,
                      std::to_string (ps.size()) + " presets, " + std::to_string (ambArtista) + " amb artista i cançó, "
                      + std::to_string (ambVeusNoves) + " amb harmonies o octava; tots sonen i declaren la latència");
        });

        prova ("0.74 Harmonitzador sense clics: canvis de nota, d'acord al teclat i en arrencar", [&]
        {
            // melodia legato (300 ms per nota, lliscant 8 ms), forma d'ona suau perquè només es vegin els clics de debò
            auto melodia = []
            {
                const double notes[] = { 57, 59, 60, 64, 62, 60, 57, 64, 55, 60 };
                const int per = (int) (0.3 * fs);
                std::vector<float> v ((size_t) (10 * per));
                double fase = 0.0, m = notes[0];
                for (int i = 0; i < (int) v.size(); ++i)
                {
                    m += (notes[i / per] - m) * (1.0 - std::exp (-1.0 / (0.008 * fs)));
                    const double f = 440.0 * std::pow (2.0, (m - 69.0) / 12.0) * (1.0 + 0.004 * std::sin (2.0 * 3.141592653589793 * 5.5 * i / fs));
                    fase += 2.0 * 3.141592653589793 * f / fs;
                    double x = 0.0;
                    for (int h = 1; h <= 14; ++h) x += std::sin (h * fase) / (h * h * 0.6);
                    v[(size_t) i] = (float) (0.3 * x);
                }
                return v;
            };
            // el pitjor salt: energia de la 2a diferència en finestres de 5 ms comparada amb les 20 veïnes
            auto pitjorClic = [] (const std::vector<float>& x)
            {
                std::vector<double> e;
                for (size_t i = (size_t) (0.1 * fs); i + 240 < x.size(); i += 240)
                {
                    double q = 0.0;
                    for (int j = 0; j < 240; ++j) { const double d = x[i + (size_t) j] - 2.0 * x[i + (size_t) j - 1] + x[i + (size_t) j - 2]; q += d * d; }
                    e.push_back (q);
                }
                double pitjor = 0.0;
                for (size_t i = 10; i + 10 < e.size(); ++i)
                {
                    std::vector<double> w;
                    for (int j = -10; j <= 10; ++j) if (j != 0) w.push_back (e[i + (size_t) j]);
                    std::nth_element (w.begin(), w.begin() + 10, w.end());
                    pitjor = std::max (pitjor, e[i] / (w[10] + 1e-12));
                }
                return pitjor;
            };
            ContextMusical ctx;
            ctx.publicaAcords ({ { 0.0, 100.0, 1 | 16 | 128, 0 } });
            ctx.segons.store (1.0);
            for (int mode = 0; mode < 3; ++mode)
            {
                harmonitzador::Harmonitzador h;
                h.posaContext (&ctx);
                h.prepara (fs, 256);
                h.posa (harmonitzador::ModeP, (float) mode);
                h.posa (harmonitzador::Sec, 0);
                h.posa (harmonitzador::Harmonies, 100);
                h.posa (harmonitzador::Pan1, 0);
                h.posa (harmonitzador::Interval1 + 1, 0);
                h.posa (harmonitzador::Lliscament, 0);
                auto L = melodia(), R = L;
                const int per = (int) (0.3 * fs);
                for (int i = 0; i < (int) L.size(); i += 256)
                {
                    if (mode == harmonitzador::Teclat)   // acords que canvien cada 300 ms, amb 30 ms sense cap tecla
                    {
                        const int k = i / per, dins = i % per;
                        ctx.totesOff();
                        if (dins > (int) (0.03 * fs)) ctx.notaOn (k % 2 == 0 ? 67 : 69);
                    }
                    h.processa (L.data() + i, R.data() + i, std::min (256, (int) L.size() - i));
                }
                const double p = pitjorClic (L);
                comprova (finit (L) && p < 6.0, std::string ("mode ") + harmonitzador::nomMode (mode) + ": cap clic (pitjor " + std::to_string (p) + "x)");
            }
            ctx.totesOff();
        });

        prova ("0.75 Doubler: la variació mou cada còpia uns mil·lisegons, i sona més", [&]
        {
            auto retards = [] (float variacio)
            {
                doubler::Doubler d;
                d.prepara (fs, 256);
                d.posa (doubler::Quantitat, 100); d.posa (doubler::Desafina, 0); d.posa (doubler::Amplada, 100);
                d.posa (doubler::Color, 0); d.posa (doubler::Variacio, variacio);
                const int per = (int) (0.25 * fs), n = 40 * per;
                std::vector<float> L ((size_t) n, 0.0f), R ((size_t) n, 0.0f);
                for (int i = 0; i < n; i += per) L[(size_t) i] = R[(size_t) i] = 1.0f;
                const auto sec = L;
                for (int i = 0; i < n; i += 256) d.processa (L.data() + i, R.data() + i, std::min (256, n - i));
                std::vector<double> r;   // on cau el pic de la còpia (L - sec) després de cada cop, en ms
                for (int k = 2; k < 40; ++k)
                {
                    int mx = 0; float v = 0.0f;
                    for (int j = 1; j < per; ++j) { const float c = std::abs (L[(size_t) (k * per + j)] - sec[(size_t) (k * per + j)]); if (c > v) { v = c; mx = j; } }
                    r.push_back (1000.0 * mx / fs);
                }
                return r;
            };
            auto marge = [] (const std::vector<double>& r) { return *std::max_element (r.begin(), r.end()) - *std::min_element (r.begin(), r.end()); };
            const auto quiet = retards (0), viu = retards (100);
            comprova (marge (quiet) < 0.5 && marge (viu) > 2.5,
                      "sense variació la còpia no es mou (" + std::to_string (marge (quiet)) + " ms); amb variació es mou " + std::to_string (marge (viu)) + " ms");
            doubler::Doubler d;
            comprova (d.valor (doubler::Quantitat) == 60 && d.valor (doubler::Retard) == 22 && d.valor (doubler::Variacio) == 60, "valors per defecte nous");
        });

        prova ("0.76 Rack: retorns en paral·lel (enviament, retorn, pan, ducking, alineació, estat, macros)", [&]
        {
            // Un retorn amb l'Octava (36 ms): la veu principal s'ha de retardar igual perquè quadrin, i el rack ho declara.
            rack::Rack r;
            r.posaEstat (R"({"mibase-rack":1,"moduls":[]})");
            r.prepara (fs, 256);
            auto o = std::make_shared<octava::Octava>();
            o->posa (octava::To, 0); o->posa (octava::Mescla, 0);   // mescla 0: el retorn és la veu retardada (exacte)
            r.afegeix ("octava", o, -1, 1);
            r.activaRetorn (0, true, "Prova");
            r.posaValor (rack::Envia1, 0.0f); r.posaValor (rack::Retorn1, 0.0f); r.posaValor (rack::PanRetorn1, 0.0f);
            const int lat = o->latencia();
            comprova (r.cami (0) == 1 && r.retorn (0).actiu && r.latencia() == lat, "el retorn és al camí 1 i el rack declara la seva latència (" + std::to_string (r.latencia()) + ")");
            const int n = 20000;
            std::vector<float> L ((size_t) n), R ((size_t) n);
            for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) std::sin (0.01 * i) * 0.3f;
            const auto x = L;
            for (int i = 0; i < n; i += 300) r.processa (L.data() + i, R.data() + i, std::min (300, n - i));
            bool doble = true;   // principal (retardada) + retorn (retardat igual) = el doble, alineat
            for (int i = lat + 2000; i < n && doble; ++i) doble = std::abs (L[(size_t) i] - 2.0f * x[(size_t) (i - lat)]) < 2e-3f;
            comprova (doble, "principal i retorn surten alineats (la suma és exactament el doble)");

            // pan i retorn a -inf
            rack::Rack r2;
            r2.posaEstat (R"({"mibase-rack":1,"moduls":[]})");
            r2.prepara (fs, 256);
            r2.activaRetorn (1, true, "Esquerra");
            r2.posaValor (rack::Retorn1 + 1, 0.0f); r2.posaValor (rack::PanRetorn1 + 1, -100.0f);
            std::vector<float> a (512, 0.5f), b (512, 0.5f);
            for (int k = 0; k < 4; ++k) { std::fill (a.begin(), a.end(), 0.5f); std::fill (b.begin(), b.end(), 0.5f); r2.processa (a.data(), b.data(), 512); }
            comprova (std::abs (a[500] - 1.0f) < 1e-3f && std::abs (b[500] - 0.5f) < 1e-3f, "pan del retorn tot a l'esquerra");
            r2.posaValor (rack::Retorn1 + 1, rack::minDbRetorn);
            for (int k = 0; k < 4; ++k) { std::fill (a.begin(), a.end(), 0.5f); std::fill (b.begin(), b.end(), 0.5f); r2.processa (a.data(), b.data(), 512); }
            comprova (std::abs (a[500] - 0.5f) < 1e-4f, "retorn a -inf: no suma res");

            // ducking: amb veu forta el retorn baixa; sense veu, no
            rack::Rack r3;
            r3.posaEstat (R"({"mibase-rack":1,"moduls":[]})");
            r3.prepara (fs, 256);
            auto rv = std::make_shared<reverb::Reverb>();
            rv->posa (reverb::Mix, 100); rv->posa (reverb::Decay, 3.0f);
            r3.afegeix ("reverb", rv, -1, 1);
            r3.activaRetorn (0, true, "Reverb");
            r3.posaValor (rack::Retorn1, 0.0f);
            auto cua = [&] (float duck)
            {
                r3.posaValor (rack::Ducking1, duck);
                r3.buida();
                std::vector<float> l ((size_t) fs), rr ((size_t) fs);
                for (int i = 0; i < (int) fs; ++i) l[(size_t) i] = rr[(size_t) i] = i < (int) (0.5 * fs) ? (float) (0.5 * std::sin (0.05 * i)) : 0.0f;
                double mentre = 0.0;
                for (int i = 0; i < (int) fs; i += 256) r3.processa (l.data() + i, rr.data() + i, std::min (256, (int) fs - i));
                for (int i = (int) (0.3 * fs); i < (int) (0.45 * fs); ++i) mentre += std::abs (l[(size_t) i] - (float) (0.5 * std::sin (0.05 * i)));
                return mentre;
            };
            const double sense = cua (0), amb = cua (18);
            comprova (amb < 0.3 * sense, "ducking: mentre la veu sona, el retorn baixa (" + std::to_string (amb / sense) + ")");

            // estat i macros sobre paràmetres del rack
            rack::Rack r4;
            r4.posaEstat (r3.estat());
            comprova (r4.numModuls() == 1 && r4.cami (0) == 1 && r4.retorn (0).nom == "Reverb" && std::abs (r4.valor (rack::Ducking1) - 18.0f) < 1e-4f, "es desen els camins i els retorns");
            comprova (r4.enllaca ({ 6, -1, rack::Retorn1, 0.0f, 1.0f }), "un macro pot moure el volum d'un retorn");
            r4.posaMacro (6, 0.5f);
            comprova (std::abs (r4.parametre (rack::Retorn1) - 0.5f) < 1e-4f, "el macro el mou");
            rack::Rack r5;
            r5.posaEstat (r4.estat());
            comprova (r5.enllacos().size() == 1 && r5.enllacos()[0].modul == -1, "l'enllaç al retorn es desa");
        });

        prova ("0.76 Les 30 cadenes de la guia: hi són, amb retorns, i sonen", [&]
        {
            const char* guia[] = { "When the Party's Over", "Bad Guy", "White Ferrari", "Vampire", "Drivers License", "715 - CRΣΣKS", "Woods",
                                   "Saoko", "Hide and Seek", "Retrograde", "Don't Start Now", "Levitating", "Blinding Lights", "Chandelier",
                                   "Espresso", "Fix You", "Believer", "As It Was", "Rolling in the Deep", "Supermassive Black Hole",
                                   "What You Know", "The Less I Know the Better", "Somebody Else", "1901", "Do I Wanna Know?", "Kill Bill",
                                   "Circles", "HUMBLE.", "Royals", "Sicko Mode" };
            int trobades = 0, ambRetorns = 0, repetits = 0;
            double pitjorCpu = 0.0;
            std::string pitjorNom;
            const auto& ps = rack::presetsFabrica();
            for (size_t i = 0; i < ps.size(); ++i) for (size_t j = i + 1; j < ps.size(); ++j) if (ps[i].nom == ps[j].nom) ++repetits;
            bool sonen = true;
            for (const char* g : guia)
                for (const auto& p : ps)
                {
                    if (p.nom.find (g) == std::string::npos) continue;
                    ++trobades;
                    rack::Rack r;
                    r.posaEstat (p.estat);
                    r.prepara (fs, 256);
                    bool ret = false;
                    for (int k = 0; k < rack::maxRetorns; ++k) ret = ret || r.retorn (k).actiu;
                    for (int i = 0; i < r.numModuls(); ++i) ret = ret || r.cami (i) > 0;
                    if (ret) ++ambRetorns;
                    auto L = veuSintetica (196.0, (int) fs), R = L;
                    const auto t0 = std::chrono::steady_clock::now();
                    passa (r, L, R);
                    const double cpu = 100.0 * std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                    if (cpu > pitjorCpu) { pitjorCpu = cpu; pitjorNom = p.nom; }
                    sonen = sonen && finit (L) && finit (R);
                    break;
                }
            comprova (trobades == 30 && ambRetorns >= 27 && repetits == 0 && sonen,
                      std::to_string (trobades) + " de la guia, " + std::to_string (ambRetorns) + " amb retorns, " + std::to_string (ps.size()) + " presets en total, sense repetits; tots sonen");
            std::printf ("        el que gasta més: «%s», %.1f %% d'un nucli\n", pitjorNom.c_str(), pitjorCpu);
        });

        prova ("0.77 Detector: sense errors d'octava amb un 2n harmònic fort, i l'Humanitza no tremola", [&]
        {
            // una veu amb el 2n i el 3r harmònics més forts que la fonamental (com moltes veus reals): YIN tendeix a saltar d'octava
            std::vector<float> v ((size_t) (3 * fs));
            double fase = 0.0;
            for (size_t i = 0; i < v.size(); ++i)
            {
                const double f = 196.0 * (1.0 + 0.006 * std::sin (2.0 * 3.141592653589793 * 5.0 * (double) i / fs));
                fase += 2.0 * 3.141592653589793 * f / fs;
                v[i] = (float) (0.1 * std::sin (fase) + 0.35 * std::sin (2 * fase + 0.3) + 0.25 * std::sin (3 * fase + 1.1) + 0.08 * std::sin (4 * fase));
            }
            canvito::Analitzador an;
            an.prepara (fs);
            int salts = 0;
            float ant = 0.0f;
            for (size_t i = 0; i < v.size(); ++i)
            {
                an.entra (v[i]);
                if (i % 240 == 0 && i > (size_t) (0.2 * fs))
                {
                    const float f = an.f0();
                    if (f > 0.0f && ant > 0.0f && std::abs (12.0 * std::log2 (f / ant)) > 7.0) ++salts;
                    if (f > 0.0f) ant = f;
                }
            }
            comprova (salts == 0 && std::abs (12.0 * std::log2 (ant / 196.0)) < 0.5, "cap salt d'octava (" + std::to_string (salts) + "), es queda a 196 Hz");
            // un salt d'octava curt (60 ms, com els errors del detector amb una veu real) no s'accepta; un de llarg (400 ms) sí
            {
                std::vector<float> w ((size_t) (1.6 * fs));
                double fa = 0.0;
                for (size_t i = 0; i < w.size(); ++i)
                {
                    const double t = (double) i / fs;
                    const bool amunt = (t >= 0.5 && t < 0.56) || t >= 1.1;
                    fa += 2.0 * 3.141592653589793 * (amunt ? 392.0 : 196.0) / fs;
                    double x = 0.0;
                    for (int k = 1; k <= 8; ++k) x += std::sin (k * fa) / (k * k * 0.6);
                    w[i] = (float) (0.3 * x);
                }
                canvito::Analitzador b;
                b.prepara (fs);
                float maxCurt = 0.0f, finalLlarg = 0.0f;
                for (size_t i = 0; i < w.size(); ++i)
                {
                    b.entra (w[i]);
                    const double t = (double) i / fs;
                    if (t > 0.5 && t < 0.65) maxCurt = std::max (maxCurt, b.f0());
                    if (t > 1.55) finalLlarg = b.f0();
                }
                comprova (maxCurt < 260.0f && std::abs (finalLlarg - 392.0f) < 10.0f,
                          "salt curt ignorat (com a molt " + std::to_string (maxCurt) + " Hz); salt llarg acceptat (" + std::to_string (finalLlarg) + " Hz)");
            }
            // Humanitza al 100 %: la desafinació és lenta (a 5 ms, la nota de la veu nova canvia molt poc)
            ContextMusical ctx;
            harmonitzador::Harmonitzador h;
            h.posaContext (&ctx);
            h.prepara (fs, 256);
            h.posa (harmonitzador::Sec, 0); h.posa (harmonitzador::Harmonies, 100); h.posa (harmonitzador::Humanitza, 100);
            h.posa (harmonitzador::Interval1, 2); h.posa (harmonitzador::Interval1 + 1, 0); h.posa (harmonitzador::Pan1, 0);
            auto L = veuSintetica (220.0, (int) (3 * fs)), R = L;
            passa (h, L, R);
            auto a = veu::analitza (L.data() + (int) fs, (long long) L.size() - (int) fs, fs);
            double tremolor = 0.0;
            int n = 0;
            for (int i = 1; i < a.punts(); ++i)
                if (a.f0[(size_t) i] > 0.0f && a.f0[(size_t) i - 1] > 0.0f) { tremolor += std::abs (1200.0 * std::log2 (a.f0[(size_t) i] / a.f0[(size_t) i - 1])); ++n; }
            tremolor = n > 0 ? tremolor / n : 99.0;
            comprova (finit (L) && tremolor < 4.0, "Humanitza 100 %: canvia " + std::to_string (tremolor) + " cèntims cada 5 ms (lent)");
        });

        prova ("0.78 Implementa a pistes reals: busos d'efectes, enviaments, ducking amb sidechain, i treure-ho", [&]
        {
            Projecte p;
            edicio::afegeixPista (p, "Guitarra", "audio");
            edicio::afegeixPista (p, "Veu", "audio");
            std::string est;
            for (const auto& x : rack::presetsFabrica()) if (x.nom == "Dua Lipa · Don't Start Now") est = x.estat;
            Insert in;
            in.id = "i" + std::to_string (++p.comptador);
            in.plugin = rack::idPlugin;
            in.nom = rack::nomPlugin;
            in.estat = est;
            p.pistes[1].inserts.push_back (in);
            const auto r = rack::desplega (p, 1, in.id);
            comprova (r.correcte && r.busos == 4 && p.pistes.size() == 6, "4 retorns → 4 busos (" + std::to_string (r.busos) + ")");
            const int veu = edicio::pistaPerId (p, p.pistes[1].id);
            const auto bs = rack::busosDesplegats (p, veu);
            bool noms = bs.size() == 4, carpeta = true, ducking = false, petit = false;
            for (int b : bs)
            {
                const auto& t = p.pistes[(size_t) b];
                noms = noms && t.nom.rfind ("VS · Veu · ", 0) == 0 && t.tipus == "fx" && ! t.inserts.empty();
                carpeta = carpeta && t.carpeta == r.carpeta;
                for (const auto& x : t.inserts)
                {
                    if (x.plugin == dinamica::idPlugin && ! x.sidechain.empty() && x.sidechain[0].pista == p.pistes[(size_t) veu].id) ducking = true;
                    if (x.plugin == rack::idPlugin) petit = true;   // el Doubler, dins d'un Vocal Studio petit
                }
            }
            comprova (noms && carpeta && edicio::carpeta (p, r.carpeta) != nullptr && edicio::carpeta (p, r.carpeta)->nom == "VS · Veu",
                      "busos «VS · Veu · …» dins de la carpeta «VS · Veu»");
            comprova (ducking && petit, "el delay porta el ducking amb la veu de sidechain; el Doubler va dins d'un Vocal Studio petit");
            comprova (p.pistes[(size_t) veu].enviaments.size() == 4, "la veu envia a cada bus");
            rack::Rack queda;
            queda.posaEstat (p.pistes[(size_t) veu].inserts[0].estat);
            bool nomesPrincipal = queda.numModuls() > 0;
            for (int i = 0; i < queda.numModuls(); ++i) nomesPrincipal = nomesPrincipal && queda.cami (i) == 0;
            for (int k = 0; k < rack::maxRetorns; ++k) nomesPrincipal = nomesPrincipal && ! queda.retorn (k).actiu;
            comprova (nomesPrincipal, "al Vocal Studio de la veu només hi queda la cadena principal");
            // el fader del bus de la plate = el retorn d'abans
            rack::Rack abans;
            abans.posaEstat (est);
            bool fader = false;
            for (int b : bs) if (p.pistes[(size_t) b].nom.find ("Plate") != std::string::npos) fader = std::abs (p.pistes[(size_t) b].faderDb - abans.valor (rack::Retorn1 + 1)) < 1e-3;
            comprova (fader, "el fader del bus és el volum del retorn");
            const int tretes = rack::treuDesplegament (p, veu);
            comprova (tretes == 4 && p.pistes.size() == 2 && p.pistes[1].enviaments.empty() && p.carpetes.empty(), "«Treu la cadena» ho esborra tot (busos, enviaments, carpeta)");
            comprova (! rack::desplega (p, 1, in.id).correcte, "sense retorns, no es pot desplegar");
        });

        prova ("0.73 Context i escales", [&]
        {
            using namespace harmonitzador;
            comprova (mouDinsMascara (60, 0xAB5, 2) == 64 && mouDinsMascara (60, 0xAB5, -2) == 57 && mouDinsMascara (61, 0xAB5, 1) == 62,
                      "graus dins de l'escala (Do+2 = Mi, Do-2 = La; Do# s'ajusta a Do i puja a Re)");
            ContextMusical c;
            c.publicaAcords ({ { 0.0, 2.0, 1, 0 }, { 2.0, 4.0, 2, 1 } });
            ContextMusical::Tram t;
            comprova (c.acordA (1.0, t) && t.mascara == 1 && c.acordA (3.9, t) && t.mascara == 2 && ! c.acordA (5.0, t), "busca l'acord del capçal");
            c.notaOn (60); c.notaOn (67); c.notaOn (64);
            int n[4];
            comprova (c.notesPremudes (n, 4) == 3 && n[0] == 60 && n[1] == 64 && n[2] == 67, "notes premudes de greu a agut");
            harmonitzador::Harmonitzador h;
            h.posa (harmonitzador::Interval1, 3.4f);
            harmonitzador::Harmonitzador h2;
            h2.posaEstat (h.estat());
            comprova (h2.valor (harmonitzador::Interval1) == 3.0f, "els intervals són enters i es desen");
        });
    }
}
