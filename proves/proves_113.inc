// 1.13: EVOLVE (fase 1): motor, matriu d'Evolution, quatre mòduls, presets i atzar.
#include "mibase/Evolve.h"
#include <set>
#include <random>
namespace
{
    struct Senyal113 { std::vector<float> L, R; };

    /** Cinc fonts de prova (3 s a 48 kHz): veu, guitarra, baix, bateria i sinte. */
    Senyal113 font113 (int tipus, double fs = 48000.0, double segons = 3.0)
    {
        const int n = (int) (fs * segons);
        Senyal113 s; s.L.assign ((size_t) n, 0.0f); s.R.assign ((size_t) n, 0.0f);
        std::mt19937 rng (7 + tipus);
        std::uniform_real_distribution<float> soroll (-1.0f, 1.0f);
        const double pi = 3.14159265358979323846;
        double ph1 = 0, ph2 = 0, ph3 = 0;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / fs;
            float x = 0.0f;
            switch (tipus)
            {
                case 0:   // «veu»: to de 220 Hz amb vibrato i formants (sumes d'harmònics)
                {
                    const double f = 220.0 * (1.0 + 0.01 * std::sin (2 * pi * 5.5 * t));
                    ph1 += f / fs;
                    for (int h = 1; h <= 12; ++h)
                    {
                        const double fh = f * h;
                        const double form = std::exp (-std::pow ((fh - 700.0) / 300.0, 2)) + 0.6 * std::exp (-std::pow ((fh - 1200.0) / 400.0, 2)) + 0.1;
                        x += (float) (form * std::sin (2 * pi * ph1 * h) / h);
                    }
                    x *= 0.35f * (float) (0.6 + 0.4 * std::sin (2 * pi * 0.7 * t) * std::sin (2 * pi * 0.7 * t));
                    break;
                }
                case 1:   // «guitarra»: una nota punteja cada mig segon (serra que s'apaga)
                {
                    const double tn = std::fmod (t, 0.5);
                    ph1 += 196.0 / fs;
                    x = (float) ((2.0 * (ph1 - std::floor (ph1)) - 1.0) * std::exp (-tn * 6.0) * 0.6);
                    break;
                }
                case 2: ph1 += 55.0 / fs; x = (float) (0.7 * std::sin (2 * pi * ph1)); break;   // «baix»
                case 3:   // «bateria»: cop de soroll cada negra (120 bpm) i bombo a cada temps
                {
                    const double tn = std::fmod (t, 0.5);
                    ph1 += (50.0 + 100.0 * std::exp (-tn * 30.0)) / fs;
                    x = (float) (0.8 * std::sin (2 * pi * ph1) * std::exp (-tn * 12.0) + 0.3 * soroll (rng) * std::exp (-std::fmod (t + 0.25, 0.5) * 40.0));
                    break;
                }
                default:  // «sinte»: acord de serres
                    ph1 += 261.6 / fs; ph2 += 329.6 / fs; ph3 += 392.0 / fs;
                    x = (float) (0.2 * ((2 * (ph1 - std::floor (ph1)) - 1) + (2 * (ph2 - std::floor (ph2)) - 1) + (2 * (ph3 - std::floor (ph3)) - 1)));
                    break;
            }
            s.L[(size_t) i] = x;
            s.R[(size_t) i] = x * 0.95f;
        }
        return s;
    }

    void passa113 (mibase::evolve::Evolve& e, Senyal113& s, int bloc = 256)
    {
        for (size_t o = 0; o < s.L.size(); o += (size_t) bloc)
        {
            const int n = (int) std::min ((size_t) bloc, s.L.size() - o);
            e.processa (s.L.data() + o, s.R.data() + o, n);
        }
    }
    float pic113 (const Senyal113& s) { float p = 0; for (size_t i = 0; i < s.L.size(); ++i) p = std::max ({ p, std::fabs (s.L[i]), std::fabs (s.R[i]) }); return p; }
    bool finit113 (const Senyal113& s) { for (size_t i = 0; i < s.L.size(); ++i) if (! std::isfinite (s.L[i]) || ! std::isfinite (s.R[i])) return false; return true; }
    double dif113 (const Senyal113& a, const Senyal113& b)
    {
        double d = 0, e = 0;
        for (size_t i = 0; i < a.L.size(); ++i) { d += (a.L[i] - b.L[i]) * (double) (a.L[i] - b.L[i]); e += a.L[i] * (double) a.L[i]; }
        return e > 0 ? std::sqrt (d / e) : 0.0;   // diferència relativa (0 = iguals)
    }
    /** 1.14: el so original retardat «lat» mostres (alguns presets tenen latència, que el PDC compensa). */
    Senyal113 retarda113 (const Senyal113& s, int lat)
    {
        if (lat <= 0) return s;
        Senyal113 r; r.L.assign (s.L.size(), 0.0f); r.R.assign (s.R.size(), 0.0f);
        for (size_t i = (size_t) lat; i < s.L.size(); ++i) { r.L[i] = s.L[i - (size_t) lat]; r.R[i] = s.R[i - (size_t) lat]; }
        return r;
    }
    const mibase::evolve::Preset& preset113 (const std::string& nom)
    {
        for (const auto& p : mibase::evolve::presetsFabrica()) if (p.nom == nom) return p;
        return mibase::evolve::presetsFabrica().front();
    }

    void proves113()
    {
        using namespace mibase::evolve;
        mibase::ContextMusical::global().sonant.store (false);

        prova ("1.13 EVOLVE: presets de fàbrica (noms, categories, estat)", []
        {
            const auto& v = presetsFabrica();
            comprova (v.size() >= 12, "almenys 12 presets a la fase 1");
            std::set<std::string> noms;
            for (const auto& p : v)
            {
                noms.insert (p.nom);
                comprova (std::find (categories().begin(), categories().end(), p.categoria) != categories().end(), "categoria coneguda: " + p.nom);
                Evolve e; e.posaEstat (p.estat);
                comprova (! e.rutes().empty(), "el preset té matriu d'evolució: " + p.nom);
                comprova (e.estat() == p.estat, "l'estat torna igual: " + p.nom);
            }
            comprova (noms.size() == v.size(), "noms sense repetir");
        });

        prova ("1.13 EVOLVE: 0 % = bypass real (el so surt idèntic)", []
        {
            for (const auto& p : presetsFabrica())
            {
                Evolve e; e.posaEstat (p.estat);
                e.posa (Evolution, 0.0f);
                e.posa (Entrada, 6.0f); e.posa (Sortida, -6.0f);
                e.prepara (48000.0, 512);
                auto s = font113 (1, 48000.0, 3.0);
                const auto orig = retarda113 (s, e.latencia());
                passa113 (e, s);
                comprova (s.L == orig.L && s.R == orig.R, "idèntic bit a bit (amb la latència compensada): " + p.nom);
            }
        });

        prova ("1.13 EVOLVE: a 100 % no satura mai (cinc fonts, tots els presets, entrada +12 dB)", []
        {
            for (const auto& p : presetsFabrica())
                for (int f = 0; f < 5; ++f)
                {
                    Evolve e; e.posaEstat (p.estat);
                    e.posa (Evolution, 100.0f); e.posa (Entrada, 12.0f); e.posa (Sortida, 12.0f);
                    e.prepara (48000.0, 512);
                    auto s = font113 (f, 48000.0, 2.0);
                    passa113 (e, s);
                    comprova (finit113 (s), "sense NaN: " + p.nom);
                    comprova (pic113 (s) < 1.0f, "pic < 0 dBFS: " + p.nom + " font " + std::to_string (f) + " (" + std::to_string (pic113 (s)) + ")");
                }
        });

        prova ("1.13 EVOLVE: cada preset sona, transforma i creix amb l'Evolution (cinc fonts)", []
        {
            for (const auto& p : presetsFabrica())
                for (int f = 0; f < 5; ++f)
                {
                    const auto font = font113 (f, 48000.0, 3.0);
                    double d[3] {};
                    const float evos[3] = { 20.0f, 60.0f, 100.0f };
                    for (int k = 0; k < 3; ++k)
                    {
                        Evolve e; e.posaEstat (p.estat); e.posa (Evolution, evos[k]); e.prepara (48000.0, 512);
                        auto s = font;
                        const auto orig = retarda113 (font, e.latencia());
                        passa113 (e, s);
                        d[k] = dif113 (s, orig);
                        double rms = 0; for (float x : s.L) rms += x * (double) x;
                        comprova (std::sqrt (rms / (double) s.L.size()) > 1e-3, "no queda mut: " + p.nom);
                    }
                    comprova (d[1] > 0.05, "a 60 % se sent la transformació: " + p.nom + " font " + std::to_string (f));
                    comprova (d[2] > d[0] * 0.9, "a 100 % canvia (almenys) tant com a 20 %: " + p.nom + " font " + std::to_string (f));
                }
        });

        prova ("1.13 EVOLVE: la matriu mou els paràmetres (corbes, punts, escala logarítmica, salts)", []
        {
            const Ruta r { FilFreq, 20, 80, 500, Lineal };
            comprova (valorEnEvolution (r, 20000, 10) == 20000.0f, "abans del punt d'entrada: el valor base");
            comprova (std::fabs (valorEnEvolution (r, 20000, 90) - 500.0f) < 0.01f, "després del punt final: l'objectiu");
            const float mig = valorEnEvolution (r, 20000, 50);
            comprova (std::fabs (mig - std::sqrt (20000.0f * 500.0f)) < 1.0f, "freqüència: al mig, la mitjana en octaves (no en Hz)");
            comprova (aplicaCorba (Suau, 0.5f) < 0.5f && aplicaCorba (Rapida, 0.5f) > 0.5f && std::fabs (aplicaCorba (EnS, 0.5f) - 0.5f) < 1e-6f, "les corbes");
            Evolve e; e.posaEstat (preset113 ("Rhythmic Pump").estat);
            comprova (e.efectiu (PmpDiv, 74) == 5.0f && e.efectiu (PmpDiv, 76) == 8.0f, "un salt (negres → corxeres al 75 %)");
            Evolve f; f.posaEstat (preset113 ("Massive Fuzz").estat);
            comprova (f.efectiu (ScrDrive, 0) < f.efectiu (ScrDrive, 50) && f.efectiu (ScrDrive, 50) < f.efectiu (ScrDrive, 100), "el drive creix");
        });

        prova ("1.13 EVOLVE: l'ordre de la cadena es respecta", []
        {
            auto fes = [] (std::array<int, numModuls> o)
            {
                Evolve e; e.posaEstat ("mibase-evolve=1");
                e.posa (Evolution, 50);
                e.posa (ScrMix, 100); e.posa (ScrDrive, 80); e.posa (ScrMode, 2);
                e.posa (FilMix, 100); e.posa (FilFreq, 400); e.posa (FilReso, 60);
                e.posa (PmpOn, 0); e.posa (ShmOn, 0);
                e.posaOrdre (o);
                e.prepara (48000.0, 512);
                auto s = font113 (4, 48000.0, 1.0);
                passa113 (e, s);
                return s;
            };
            const auto a = fes ({ MScreamer, MFiltre, MPumper, MShimmer, MReverser, MPitch, MScatter }), b = fes ({ MFiltre, MScreamer, MPumper, MShimmer, MReverser, MPitch, MScatter });
            comprova (dif113 (a, b) > 0.1, "distorsió → filtre no sona com filtre → distorsió");
            Evolve e; e.posaOrdre ({ MShimmer, MPumper, MFiltre, MScreamer, MScatter, MPitch, MReverser });
            comprova (e.ordre()[0] == MShimmer && e.ordre()[3] == MScreamer && e.ordre()[6] == MReverser, "l'ordre es desa");
            Evolve g; g.posaEstat (e.estat());
            comprova (g.ordre() == e.ordre(), "i es recupera de l'estat");
            e.posaOrdre ({ 0, 0, 1, 2, 3, 4, 5 });
            comprova (e.ordre()[0] == MPitch && e.ordre()[6] == MShimmer, "un ordre impossible torna al de sempre");
        });

        prova ("1.13 EVOLVE: cada mòdul sol fa el que ha de fer", []
        {
            auto sol = [] (int modul, std::vector<std::pair<int, float>> v)
            {
                Evolve e; e.posaEstat ("mibase-evolve=1"); e.posa (Evolution, 50);
                for (int m = 0; m < numModuls; ++m) e.posa (paramOn (m), m == modul ? 1.0f : 0.0f);
                for (auto [p, x] : v) e.posa (p, x);
                e.prepara (48000.0, 512);
                auto s = font113 (4, 48000.0, 2.0);
                const auto orig = s;
                passa113 (e, s);
                return std::pair<Senyal113, Senyal113> (s, orig);
            };
            for (int mode = 0; mode < 5; ++mode)
            {
                auto [s, o] = sol (MScreamer, { { ScrMix, 100 }, { ScrDrive, 70 }, { ScrMode, (float) mode } });
                comprova (dif113 (s, o) > 0.2 && finit113 (s), std::string ("Screamer distorsiona: ") + nomModeScreamer (mode));
            }
            {
                auto [s, o] = sol (MFiltre, { { FilMix, 100 }, { FilFreq, 300 }, { FilTipus, 0 } });
                double eS = 0, eO = 0; for (size_t i = 0; i < s.L.size(); ++i) { eS += s.L[i] * (double) s.L[i]; eO += o.L[i] * (double) o.L[i]; }
                comprova (eS < eO * 0.5, "el LP a 300 Hz treu energia a un acord de serres");
            }
            {
                // Pumper: ducking a negres, 120 bpm, comptador propi des de 0: just al començament del temps baixa; abans del següent, torna
                Evolve e; e.posaEstat ("mibase-evolve=1"); e.posa (Evolution, 50);
                e.posa (ScrOn, 0); e.posa (FilOn, 0); e.posa (ShmOn, 0);
                e.posa (PmpMode, 0); e.posa (PmpDiv, 5); e.posa (PmpDepth, 100); e.posa (PmpForma, 50);
                std::atomic<double> t120 { 120.0 };
                e.posaTempo (&t120);
                e.prepara (48000.0, 512);
                Senyal113 s; s.L.assign (48000, 0.5f); s.R.assign (48000, 0.5f);
                passa113 (e, s, 128);
                // segon temps: comença a 0,5 s (mostra 24000)
                comprova (s.L[24000 + 200] < 0.15f, "just després del temps, el volum ha baixat");
                comprova (s.L[24000 - 300] > 0.4f, "just abans del temps següent, ha tornat");
            }
            {
                auto [s, o] = sol (MShimmer, { { ShmMix, 50 }, { ShmDecay, 6 } });
                // la cua continua després que el so pari: allarguem amb silenci
                Evolve e; e.posaEstat ("mibase-evolve=1"); e.posa (Evolution, 50);
                e.posa (ScrOn, 0); e.posa (FilOn, 0); e.posa (PmpOn, 0); e.posa (ShmMix, 50); e.posa (ShmDecay, 6);
                e.prepara (48000.0, 512);
                auto x = font113 (1, 48000.0, 1.0);
                x.L.resize (96000, 0.0f); x.R.resize (96000, 0.0f);
                passa113 (e, x);
                double cua = 0; for (size_t i = 72000; i < 96000; ++i) cua += x.L[i] * (double) x.L[i];
                comprova (cua > 1e-3, "el Shimmer deixa cua quan el so s'acaba");
                comprova (dif113 (s, o) > 0.1, "i transforma el so");
            }
        });

        prova ("1.13 EVOLVE: canvis suaus (moure l'Evolution de cop no fa clics)", []
        {
            for (const auto& nom : { "Cinematic Pad", "Rhythmic Pump", "Angel Choir" })
            {
                Evolve e; e.posaEstat (preset113 (nom).estat);
                e.posa (Evolution, 0.0f);
                e.prepara (48000.0, 512);
                Senyal113 s; s.L.resize (96000); s.R.resize (96000);
                for (int i = 0; i < 96000; ++i) s.L[(size_t) i] = s.R[(size_t) i] = 0.4f * (float) std::sin (2 * 3.14159265 * 110.0 * i / 48000.0);
                float maxSalt = 0.0f;
                for (size_t o = 0; o < s.L.size(); o += 256)
                {
                    if (o == 24064) e.posa (Evolution, 100.0f);   // de 0 a 100 de cop
                    if (o == 60160) e.posa (Evolution, 0.0f);     // i tornada al bypass
                    e.processa (s.L.data() + o, s.R.data() + o, 256);
                }
                for (size_t i = 1; i < s.L.size(); ++i) maxSalt = std::max (maxSalt, std::fabs (s.L[i] - s.L[i - 1]));
                // un sinus de 110 Hz a 0,4 canvia com a molt 0,006 per mostra; deixem marge per a la reverb i el pumper suavitzats
                comprova (maxSalt < 0.08f, std::string ("sense salts bruscos: ") + nom + " (" + std::to_string (maxSalt) + ")");
                comprova (finit113 (s), std::string ("sense NaN: ") + nom);
            }
        });

        prova ("1.13 EVOLVE: Hold manté l'Evolution", []
        {
            Evolve e; e.posaEstat (preset113 ("Dream Clouds").estat);
            e.posa (Evolution, 40.0f);
            e.prepara (48000.0, 512);
            auto s = font113 (0, 48000.0, 0.5);
            passa113 (e, s);
            e.posa (Hold, 1.0f);
            e.posa (Evolution, 90.0f);
            auto t = font113 (0, 48000.0, 0.5);
            passa113 (e, t);
            comprova (std::fabs (e.mesures().evolution.load() - 40.0f) < 0.01f, "amb Hold, l'Evolution que sona es queda a 40");
            e.posa (Hold, 0.0f);
            auto u = font113 (0, 48000.0, 0.5);
            passa113 (e, u);
            comprova (e.mesures().evolution.load() > 89.0f, "sense Hold, va on diu el pom");
        });

        prova ("1.13 EVOLVE: automatització (ids, noms i passos) i estat amb rutes", []
        {
            Evolve e;
            comprova (e.numParametres() == numParams, "tots els paràmetres");
            comprova (e.infoParametre (Evolution).id == "evolution" && e.infoParametre (Evolution).automatitzable, "Evolution automatitzable");
            comprova (! e.infoParametre (Vista).automatitzable, "la vista no s'automatitza");
            comprova (e.infoParametre (FilFreq).nom == "Filter · Cutoff", "nom amb el mòdul (Filter · Cutoff)");
            comprova (e.infoParametre (PmpDiv).passos == 14, "el ritme va a salts (14 divisions)");
            e.posaParametre (Evolution, 0.75f);
            comprova (std::fabs (e.valor (Evolution) - 75.0f) < 1e-3f, "0,75 normalitzat = 75 %");
            e.posaRutes ({ { FilFreq, 10, 90, 800, EnS }, { Evolution, 0, 100, 50, Lineal } });
            comprova (e.rutes().size() == 1, "l'Evolution no pot ser destí d'una ruta");
            Evolve g; g.posaEstat (e.estat());
            comprova (g.rutes().size() == 1 && g.rutes()[0].param == FilFreq && g.rutes()[0].corba == EnS && std::fabs (g.rutes()[0].valor - 800.0f) < 0.01f,
                      "les rutes tornen de l'estat");
            Evolve h; h.posaEstat ("altra cosa");
            comprova (! h.rutes().empty(), "un estat que no és d'EVOLVE no canvia res");
        });

        prova ("1.13 EVOLVE: atzar musical i segur", []
        {
            Evolve base; base.posa (Evolution, 100.0f);
            int diferents = 0;
            std::string abans;
            for (unsigned k = 1; k <= 25; ++k)
            {
                const auto st = base.estatAtzar (k);
                if (st != abans) ++diferents;
                abans = st;
                Evolve e; e.posaEstat (st);
                int actius = 0;
                for (int m = 0; m < numModuls; ++m) actius += e.valor (paramOn (m)) > 0.5f ? 1 : 0;
                comprova (actius >= 2 && actius <= 4, "2–4 mòduls");
                comprova (e.valor (Evolution) == 100.0f, "es manté l'Evolution d'ara");
                e.posa (Entrada, 12.0f);
                e.prepara (48000.0, 512);
                auto s = font113 ((int) (k % 5), 48000.0, 1.5);
                passa113 (e, s);
                comprova (finit113 (s) && pic113 (s) < 1.0f, "sense NaN ni saturació, llavor " + std::to_string (k));
            }
            comprova (diferents >= 20, "cada vegada surt una combinació diferent");
        });

        prova ("1.13 EVOLVE: blocs de mida diferent sonen igual", []
        {
            auto fes = [] (int bloc)
            {
                Evolve e; e.posaEstat (preset113 ("Pulsating Synth").estat); e.posa (Evolution, 70.0f);
                std::atomic<double> t { 100.0 };
                e.posaTempo (&t);
                e.prepara (48000.0, 1024);
                auto s = font113 (4, 48000.0, 1.0);
                passa113 (e, s, bloc);
                return s;
            };
            const auto a = fes (64), b = fes (1000);
            comprova (dif113 (a, b) < 1e-3, "64 o 1000 mostres per bloc: el mateix so");
        });
    }
}
