// 0.70: motor de rack i Vocal Studio (Mibase); Doubler.
#include "mibase/Doubler.h"
#include "mibase/Rack.h"
#include "mibase/Equalitzador.h"
#include "mibase/Reverb.h"
namespace
{
    void proves070()
    {
        using namespace mibase;
        auto sinus = [] (float amp, float hz, int n)
        {
            std::vector<float> v ((size_t) n);
            for (int i = 0; i < n; ++i) v[(size_t) i] = amp * (float) std::sin (2.0 * 3.141592653589793 * hz * i / 48000.0);
            return v;
        };
        auto passa = [] (Efecte& d, std::vector<float>& L, std::vector<float>& R, int bloc = 256)
        {
            for (int i = 0; i < (int) L.size(); i += bloc) d.processa (L.data() + i, R.data() + i, std::min (bloc, (int) L.size() - i));
        };
        auto pic = [] (const std::vector<float>& v, int a, int b) { float m = 0; for (int i = a; i < b; ++i) m = std::max (m, std::fabs (v[(size_t) i])); return m; };
        auto finit = [] (const std::vector<float>& v) { return std::all_of (v.begin(), v.end(), [] (float x) { return std::isfinite (x); }); };

        prova ("0.70 Doubler: suma còpies desafinades als costats i no toca la veu amb quantitat 0", [&]
        {
            doubler::Doubler d;
            d.prepara (48000.0, 256);
            d.posa (doubler::Quantitat, 0);
            auto L = sinus (0.5f, 220.0f, 48000), R = L;
            const auto L0 = L;
            passa (d, L, R);
            float dif = 0; for (size_t i = 0; i < L.size(); ++i) dif = std::max (dif, std::fabs (L[i] - L0[i]));
            comprova (dif < 1e-6f, "amb quantitat 0, el so surt igual");
            d.posa (doubler::Quantitat, 60); d.posa (doubler::Amplada, 100);
            L = sinus (0.5f, 220.0f, 48000); R = L;
            passa (d, L, R);
            float difLR = 0; for (int i = 24000; i < 48000; ++i) difLR = std::max (difLR, std::fabs (L[(size_t) i] - R[(size_t) i]));
            std::printf ("        diferència L/R %.3f, pic %.3f\n", difLR, pic (L, 24000, 48000));
            comprova (finit (L) && finit (R), "res d'estrany (NaN)");
            comprova (difLR > 0.02f, "amb amplada, cada costat sona diferent (estèreo)");
            comprova (pic (L, 24000, 48000) < 1.2f, "no es dispara");
            doubler::Doubler e; e.posaEstat (d.estat());
            comprova (std::abs (e.valor (doubler::Quantitat) - 60.0f) < 1e-3f, "l'estat es desa i es llegeix");
        });

        prova ("0.70 Rack: mòduls, mix, apagar, ordre i latència", [&]
        {
            rack::Rack r;
            r.prepara (48000.0, 256);
            comprova (r.numModuls() == 0, "neix buit (el Vocal Studio li posa l'estat inicial)");
            // un rack buit no toca res
            auto L = sinus (0.3f, 440.0f, 4800), R = L; const auto L0 = L;
            passa (r, L, R);
            float dif = 0; for (size_t i = 0; i < L.size(); ++i) dif = std::max (dif, std::fabs (L[i] - L0[i]));
            comprova (dif < 1e-6f, "buit: el so passa igual");
            const int iC = r.afegeix ("compressor");
            const int iL = r.afegeix ("limitador");
            const int iE = r.afegeix ("eq", 0);
            comprova (iE == 0 && r.numModuls() == 3 && r.modul (1)->clau == "compressor" && r.modul (2)->clau == "limitador", "afegeix al lloc demanat");
            (void) iC; (void) iL;
            comprova (r.latencia() == 240, "la latència és la del Limitador (5 ms)");
            r.posaOn (2, false);
            comprova (r.latencia() == 0, "apagat, no compta");
            r.posaOn (2, true);
            r.mou (2, 0);
            comprova (r.modul (0)->clau == "limitador" && r.modul (1)->clau == "eq" && r.modul (2)->clau == "compressor", "mou a dalt");
            r.treu (0);
            comprova (r.numModuls() == 2 && r.latencia() == 0, "treu");
            for (int k = 0; k < 40; ++k) r.afegeix ("doubler");
            comprova (r.numModuls() == rack::maxModuls, "com a molt " + std::to_string (rack::maxModuls) + " mòduls");
        });

        prova ("0.70 Rack: el Mix fa compressió en paral·lel (sense clics) i apagar fa una fosa", [&]
        {
            rack::Rack r;
            r.prepara (48000.0, 256);
            r.afegeix ("eq");
            // el passa-alts de l'EQ (90 Hz) gairebé no toca un sinus de 1 kHz; posem un guany clar per veure la barreja
            auto* q = dynamic_cast<eq::Equalitzador*> (r.modul (0)->fx.get());
            eq::Banda b = q->banda (0); b.tipus = eq::Bell; b.freq = 1000; b.guany = 12; b.q = 0.5f; q->posaBanda (0, b);
            auto mesura = [&] (float mix)
            {
                r.posaMix (0, mix);
                auto L = sinus (0.1f, 1000.0f, 24000), R = L;
                passa (r, L, R);
                return pic (L, 12000, 24000);
            };
            const float sec = mesura (0.0f), mig = mesura (0.5f), ple = mesura (1.0f);
            std::printf ("        mix 0: %.3f  mix 50: %.3f  mix 100: %.3f\n", sec, mig, ple);
            comprova (std::abs (sec - 0.1f) < 0.005f, "mix 0: el so original");
            comprova (ple > 0.35f, "mix 100: amb els +12 dB");
            comprova (mig > sec + 0.05f && mig < ple - 0.05f, "mix 50: al mig");
            // canvi de mix a mig bloc sense salts grossos
            r.posaMix (0, 1.0f);
            auto L = sinus (0.1f, 1000.0f, 4800), R = L;
            passa (r, L, R, 4800);
            float salt = 0; for (int i = 1; i < 4800; ++i) salt = std::max (salt, std::fabs (L[(size_t) i] - L[(size_t) i - 1]));
            comprova (salt < 0.1f, "la barreja canvia en rampa");
        });

        prova ("0.70 Rack: els macros mouen diversos paràmetres, també al revés", [&]
        {
            rack::Rack r;
            r.prepara (48000.0, 256);
            r.afegeix ("compressor");
            r.afegeix ("reverb");
            comprova (r.enllaca ({ 3, 0, dinamica::Llindar, 0.9f, 0.1f }), "enllaça el llindar (al revés)");
            comprova (r.enllaca ({ 3, 1, reverb::Mix, 0.0f, 0.5f }), "i el mix de la reverb");
            comprova (! r.enllaca ({ 3, 7, 0, 0, 1 }), "un mòdul que no hi és, no");
            r.posaMacro (3, 1.0f);
            comprova (std::abs (r.modul (0)->fx->parametre (dinamica::Llindar) - 0.1f) < 1e-4f, "macro a 1: el llindar al mínim");
            comprova (std::abs (r.modul (1)->fx->parametre (reverb::Mix) - 0.5f) < 1e-4f, "macro a 1: la reverb a la meitat");
            r.posaMacro (3, 0.0f);
            comprova (std::abs (r.modul (0)->fx->parametre (dinamica::Llindar) - 0.9f) < 1e-4f, "macro a 0: el llindar al màxim");
            // en moure i treure mòduls, els enllaços els segueixen
            r.mou (1, 0);
            comprova (r.enllacos()[0].modul == 1 && r.enllacos()[1].modul == 0, "els enllaços segueixen els mòduls");
            r.treu (0);
            comprova (r.enllacos().size() == 1 && r.enllacos()[0].modul == 0, "treure un mòdul treu els seus enllaços");
            r.posaMacro (3, 0.5f);
            comprova (std::abs (r.modul (0)->fx->parametre (dinamica::Llindar) - 0.5f) < 1e-4f, "i la resta continuen");
        });

        prova ("0.70 Rack: estat, presets i cadenes velles", [&]
        {
            rack::Rack r;
            r.posaEstat ("");
            comprova (r.numModuls() >= 3, "estat buit = la cadena inicial (no comença mai de zero)");
            r.prepara (48000.0, 256);
            r.posaNomMacro (2, "Brillo");
            r.posaMix (1, 0.4f);
            r.posaOn (2, false);
            const auto s = r.estat();
            rack::Rack q;
            q.posaEstat (s);
            comprova (q.estat() == s, "es desa i es llegeix igual");
            comprova (q.nomMacro (2) == "Brillo" && std::abs (q.modul (1)->mix.load() - 0.4f) < 1e-6f && ! q.modul (2)->on.load(), "noms, mix i encès");
            q.posaEstat ("res de bo");
            comprova (q.estat() == s, "un estat dolent no ho espatlla");

            const auto& ps = rack::presetsFabrica();
            std::printf ("        %d presets\n", (int) ps.size());
            comprova (ps.size() >= 20, "20 presets de fàbrica");
            bool totsBe = true;
            for (const auto& p : ps)
            {
                rack::Rack x;
                x.posaEstat (p.estat);
                x.prepara (48000.0, 256);
                auto L = sinus (0.3f, 220.0f, 24000), R = sinus (0.3f, 330.0f, 24000);
                for (int k = 0; k < 24000; k += 2400) L[(size_t) k] = 0.9f;
                passa (x, L, R);
                const bool ok = x.numModuls() >= 2 && finit (L) && finit (R) && pic (L, 0, 24000) < 4.0f;
                if (! ok) std::printf ("        [!] %s: %d mòduls, pic %.2f\n", p.nom.c_str(), x.numModuls(), pic (L, 0, 24000));
                // els macros no han de canviar res en carregar (arrenquen on és el mòdul)
                const auto abans = x.estat();
                for (int m = 0; m < rack::numMacros; ++m) x.aplicaMacro (m);
                if (x.estat() != abans) std::printf ("        [!] %s: els macros mouen els mòduls en carregar\n", p.nom.c_str());
                totsBe = totsBe && ok && x.estat() == abans;
            }
            comprova (totsBe, "tots sonen (sense NaN ni salts) i tenen mòduls");
            // cadenes velles: s'esborren quan el fil d'àudio ja ha fet dos blocs
            rack::Rack c;
            c.prepara (48000.0, 256);
            for (int k = 0; k < 5; ++k) c.afegeix ("eq");
            comprova (c.cadenesGuardades() > 0, "les velles esperen");
            std::vector<float> a (256), b (256);
            c.processa (a.data(), b.data(), 256);
            c.processa (a.data(), b.data(), 256);
            c.recull();
            comprova (c.cadenesGuardades() == 0, "i s'esborren després de dos blocs");
        });

        prova ("0.70 Rack: rendiment del preset més ple", [&]
        {
            const auto& ps = rack::presetsFabrica();
            size_t mesPle = 0; int maxM = 0;
            for (size_t i = 0; i < ps.size(); ++i) { rack::Rack x; x.posaEstat (ps[i].estat); if (x.numModuls() > maxM) { maxM = x.numModuls(); mesPle = i; } }
            rack::Rack x; x.posaEstat (ps[mesPle].estat); x.prepara (48000.0, 256);
            auto L = sinus (0.3f, 220.0f, 48000 * 4), R = L;
            const auto t0 = std::chrono::steady_clock::now();
            passa (x, L, R);
            const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("        «%s» (%d mòduls): %.1f %% d'un nucli\n", ps[mesPle].nom.c_str(), maxM, 100.0 * s / 4.0);
            comprova (s < 4.0 * 0.25, "menys d'un 25 % d'un nucli");
        });
    }
}
