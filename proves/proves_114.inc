// 1.14: EVOLVE fase 2: Reverser, Pitch, Scatter, LFO, nivells per mòdul, solo i latència.
#include "mibase/Evolve.h"
namespace
{
    using Senyal114 = Senyal113;

    /** Un EVOLVE amb un sol mòdul encès i a punt de sonar (Evolution 50 %, tempo 120). */
    std::unique_ptr<mibase::evolve::Evolve> sol114 (int modul, std::vector<std::pair<int, float>> v, std::atomic<double>* tempo)
    {
        using namespace mibase::evolve;
        auto e = std::make_unique<Evolve>();
        e->posaEstat ("mibase-evolve=1");
        e->posa (Evolution, 50);
        for (int m = 0; m < numModuls; ++m) e->posa (paramOn (m), m == modul ? 1.0f : 0.0f);
        for (auto [p, x] : v) e->posa (p, x);
        e->posaTempo (tempo);
        e->prepara (48000.0, 512);
        return e;
    }
    double energia114 (const std::vector<float>& v, double de, double a, double fs = 48000.0)
    {
        double s = 0;
        for (size_t i = (size_t) (de * fs); i < (size_t) (a * fs) && i < v.size(); ++i) s += v[i] * (double) v[i];
        return s;
    }
    /** La freqüència dominant (autocorrelació) d'un tros, entre fMin i fMax. */
    double freq114 (const std::vector<float>& v, size_t de, size_t n, double fMin, double fMax, double fs = 48000.0)
    {
        int millor = 0; double max = -1e30;
        for (int lag = (int) (fs / fMax); lag <= (int) (fs / fMin); ++lag)
        {
            double c = 0;
            for (size_t i = de; i < de + n; ++i) c += v[i] * (double) v[i + (size_t) lag];
            if (c > max) { max = c; millor = lag; }
        }
        return millor > 0 ? fs / millor : 0.0;
    }

    void proves114()
    {
        using namespace mibase::evolve;
        mibase::ContextMusical::global().sonant.store (false);
        static std::atomic<double> t120 { 120.0 };

        prova ("1.14 EVOLVE: presets nous (30 en total, 6 categories, GLITCH)", []
        {
            comprova (presetsFabrica().size() >= 30, "almenys 30 presets");
            int glitch = 0;
            for (const auto& p : presetsFabrica()) glitch += p.categoria == "GLITCH & EXPERIMENTAL" ? 1 : 0;
            comprova (glitch >= 5, "5 presets de GLITCH & EXPERIMENTAL");
            comprova (categories().size() == 6, "6 categories");
        });

        prova ("1.14 EVOLVE: latència (Reverse reverb = 2 trossos; Pitch Veu = el motor de la veu; la resta, 0)", []
        {
            auto a = sol114 (MReverser, { { RevMode, 1 }, { RevDiv, 2 }, { RevMix, 50 } }, &t120);
            comprova (a->latencia() == 96000, "1/2 a 120 bpm = 1 s; 2 trossos = 96000 mostres (" + std::to_string (a->latencia()) + ")");
            a->posa (RevMode, 0);
            comprova (a->latencia() == 0, "el Reverse delay no retarda");
            auto b = sol114 (MPitch, { { PitMode, 1 }, { PitMix, 50 } }, &t120);
            comprova (b->latencia() > 1000 && b->latencia() < 2500, "Pitch Veu: uns 36 ms (" + std::to_string (b->latencia()) + ")");
            b->posa (PitOn, 0);
            comprova (b->latencia() == 0, "apagat no retarda");
            Evolve c; c.posaRutes ({ { RevMode, 0, 100, 1, Lineal }, { PitMode, 0, 100, 1, Lineal }, { RevDiv, 0, 100, 3, Lineal } });
            comprova (c.rutes().empty(), "els paràmetres que canvien la latència no poden anar amb rutes");
            c.posaAssignacions ({ { 0, PitMode, 50 }, { 0, L2Rate, 50 }, { 1, FilFreq, 50 } });
            comprova (c.assignacions().size() == 1 && c.assignacions()[0].param == FilFreq, "ni amb LFO (ni un LFO pot moure un LFO)");
        });

        prova ("1.14 EVOLVE: Reverse reverb: la cua invertida puja just abans de la nota", []
        {
            auto e = sol114 (MReverser, { { RevMode, 1 }, { RevDiv, 5 }, { RevMix, 100 }, { RevDecay, 3 } }, &t120);   // trossos de 0,5 s; latència 1 s
            const int lat = e->latencia();
            comprova (lat == 48000, "latència de 2 negres a 120 = 1 s");
            Senyal114 s; s.L.assign (48000 * 5, 0.0f); s.R.assign (48000 * 5, 0.0f);
            for (int i = 0; i < 2400; ++i) s.L[(size_t) (96000 + i)] = s.R[(size_t) (96000 + i)] = 0.8f * (float) std::sin (i * 0.06) * (1.0f - i / 2400.0f);   // nota a 2,0 s
            passa113 (*e, s);
            // la nota sona a 2,0 + 1,0 = 3,0 s; la cua invertida, just abans (de 2,5 a 3,0 s), creixent
            const double abans = energia114 (s.L, 2.55, 2.98), molt = energia114 (s.L, 1.5, 2.4), comenca = energia114 (s.L, 2.52, 2.62), acaba = energia114 (s.L, 2.88, 2.98);
            comprova (abans > 0.5 && abans > 50.0 * (molt + 1e-9), "hi ha cua just abans de la nota (" + std::to_string (abans) + " vs " + std::to_string (molt) + ")");
            comprova (acaba > 3.0 * comenca, "i puja (" + std::to_string (comenca) + " → " + std::to_string (acaba) + ")");
            comprova (energia114 (s.L, 3.0, 3.05) > 1.0, "la nota seca surt a 3,0 s (el PDC ho deixa al seu lloc)");
        });

        prova ("1.14 EVOLVE: Reverse delay: el tros d'abans sona al revés", []
        {
            auto e = sol114 (MReverser, { { RevMode, 0 }, { RevDiv, 5 }, { RevMix, 100 }, { RevFeedback, 0 } }, &t120);
            Senyal114 s; s.L.assign (48000 * 2, 0.0f); s.R.assign (48000 * 2, 0.0f);
            for (int i = 0; i < 24000; ++i) s.L[(size_t) i] = s.R[(size_t) i] = (float) i / 24000.0f * (i % 2 == 0 ? 0.5f : -0.5f);   // creix
            passa113 (*e, s);
            const double primer = energia114 (s.L, 0.55, 0.65), darrer = energia114 (s.L, 0.85, 0.95);
            comprova (primer > 4.0 * darrer, "al revés: comença fort i s'apaga (" + std::to_string (primer) + " / " + std::to_string (darrer) + ")");
            comprova (energia114 (s.L, 0.05, 0.45) < 1e-6, "mentre grava el primer tros, amb Mix 100 no sona res");
        });

        prova ("1.14 EVOLVE: Pitch universal (octava amunt, harmonia) i Pitch Veu", []
        {
            auto e = sol114 (MPitch, { { PitMode, 0 }, { PitSemi1, 12 }, { PitMix, 100 } }, &t120);
            Senyal114 s; s.L.resize (48000); s.R.resize (48000);
            for (int i = 0; i < 48000; ++i) s.L[(size_t) i] = s.R[(size_t) i] = 0.5f * (float) std::sin (2 * 3.14159265 * 220.0 * i / 48000.0);
            passa113 (*e, s);
            const double f = freq114 (s.L, 24000, 4000, 300, 700);
            comprova (std::fabs (f - 440.0) < 8.0, "220 Hz +12 = 440 Hz (" + std::to_string (f) + ")");
            comprova (pic113 (s) < 1.0f && finit113 (s), "sense saturar ni NaN");
            auto v = sol114 (MPitch, { { PitMode, 1 }, { PitSemi1, 12 }, { PitMix, 100 } }, &t120);
            auto x = font113 (0, 48000.0, 2.0);
            passa113 (*v, x);
            const double fv = freq114 (x.L, 48000, 4000, 300, 700);
            comprova (std::fabs (fv - 440.0) < 15.0, "la veu de 220 Hz, una octava amunt (" + std::to_string (fv) + ")");
            comprova (finit113 (x), "sense NaN");
        });

        prova ("1.14 EVOLVE: Pitch Veu sense forats en sortir i tornar del 0 %", []
        {
            Evolve e; e.posaEstat (preset113 ("Frozen Voice").estat);
            e.posa (Evolution, 60.0f);
            e.posaTempo (&t120);
            e.prepara (48000.0, 512);
            auto s = font113 (0, 48000.0, 3.0);
            for (size_t o = 0; o < s.L.size(); o += 256)
            {
                if (o == 48128) e.posa (Evolution, 0.0f);
                if (o == 72192) e.posa (Evolution, 60.0f);
                e.processa (s.L.data() + o, s.R.data() + o, (int) std::min<size_t> (256, s.L.size() - o));
            }
            double minim = 1e9;
            for (double t = 0.2; t < 2.9; t += 0.01) minim = std::min (minim, energia114 (s.L, t, t + 0.01));
            comprova (minim > 1e-4, "cap tros de 10 ms en silenci (" + std::to_string (minim) + ")");
        });

        prova ("1.14 EVOLVE: Scatter (sempre igual per al mateix tall, i transforma)", []
        {
            auto fes = [] (float mix, float dens)
            {
                auto e = sol114 (MScatter, { { ScaDiv, 11 }, { ScaDensity, dens }, { ScaScatter, 60 }, { ScaMix, mix } }, &t120);
                auto s = font113 (4, 48000.0, 3.0);
                passa113 (*e, s);
                return s;
            };
            const auto orig = font113 (4, 48000.0, 3.0);
            const auto a = fes (100, 80), b = fes (100, 80), c = fes (0, 80);
            comprova (a.L == b.L, "el mateix tall fa sempre el mateix (també en exportar)");
            comprova (dif113 (a, orig) > 0.2, "trenca el so");
            comprova (c.L == orig.L, "amb Mix 0, res");
            comprova (pic113 (a) < 1.0f && finit113 (a), "sense saturar ni NaN");
        });

        prova ("1.14 EVOLVE: LFO (forma, tempo, profunditat) i assignacions", []
        {
            comprova (std::fabs (formaLfo (0, 0.25f, 0) - 1.0f) < 1e-5f && formaLfo (2, 0.75f, 0) == -1.0f && formaLfo (3, 0.0f, 0) == -1.0f, "formes");
            comprova (formaLfo (4, 0.1f, 7) == formaLfo (4, 0.9f, 7) && formaLfo (4, 0.1f, 7) != formaLfo (4, 0.1f, 8), "atzar: un valor per cicle");
            auto e = sol114 (MFiltre, { { FilMix, 100 }, { FilFreq, 1000 }, { L1Forma, 2 }, { L1Sync, 1 }, { L1Div, 5 }, { L1Depth, 100 } }, &t120);
            e->posaAssignacions ({ { 0, FilFreq, 30 } });
            Senyal114 s; s.L.assign (256, 0.1f); s.R.assign (256, 0.1f);
            float mn = 2, mx = -2;
            for (int k = 0; k < 400; ++k) { auto t = s; e->processa (t.L.data(), t.R.data(), 256); const float v = e->mesures().lfo[0].load(); mn = std::min (mn, v); mx = std::max (mx, v); }
            comprova (mn < -0.99f && mx > 0.99f, "quadrada a tota profunditat: de -1 a 1");
            Evolve g; g.posaEstat (e->estat());
            comprova (g.assignacions().size() == 1 && g.assignacions()[0].lfo == 0 && g.assignacions()[0].param == FilFreq, "les assignacions es desen");
        });

        prova ("1.14 EVOLVE: solo d'un mòdul i nivells d'entrada/sortida", []
        {
            auto e = sol114 (MScreamer, { { ScrMix, 100 }, { ScrDrive, 40 } }, &t120);
            auto f = sol114 (MScreamer, { { ScrMix, 100 }, { ScrDrive, 40 }, { (int) NivOut + (int) MScreamer, -12 } }, &t120);
            auto a = font113 (1, 48000.0, 2.0), b = a;
            passa113 (*e, a); passa113 (*f, b);
            const double r = 10.0 * std::log10 (energia114 (b.L, 0.5, 2.0) / energia114 (a.L, 0.5, 2.0));
            comprova (std::fabs (r + 12.0) < 0.5, "Out -12 dB del Screamer: -12 dB (" + std::to_string (r) + ")");
            // solo: tot el preset, però només sona el Shimmer = el mateix que només el Shimmer encès
            Evolve x; x.posaEstat (preset113 ("Glitch Explosion").estat); x.posa (Evolution, 80); x.posaTempo (&t120); x.prepara (48000.0, 512);
            x.posaSolo (MShimmer);
            Evolve y; y.posaEstat (preset113 ("Glitch Explosion").estat); y.posa (Evolution, 80); y.posaTempo (&t120);
            for (int m = 0; m < numModuls; ++m) if (m != MShimmer) y.posa (paramOn (m), 0.0f);
            y.prepara (48000.0, 512);
            auto c = font113 (1, 48000.0, 2.0), d = c;
            passa113 (x, c); passa113 (y, d);
            comprova (dif113 (c, d) < 1e-4, "solo = només aquell mòdul");
            comprova (x.estat().find ("solo") == std::string::npos, "el solo no es desa");
        });

        prova ("1.14 EVOLVE: un estat de la 1.13 s'obre igual", []
        {
            const std::string vell = "mibase-evolve=1;evolution=40;scrmix=0;filmix=0;ordre=3,2,1,0;rutes=shmmix:0:60:40:1";
            Evolve e; e.posaEstat (vell);
            const auto o = e.ordre();
            comprova (o[0] == MShimmer && o[1] == MPumper && o[2] == MFiltre && o[3] == MScreamer, "l'ordre d'abans es respecta");
            comprova (o[4] == MPitch && o[5] == MScatter && o[6] == MReverser && o[7] == MGranular, "els mòduls nous, al final");
            comprova (e.latencia() == 0 && e.rutes().size() == 1 && e.valor (Evolution) == 40.0f, "sense latència i amb les rutes");
            comprova (e.valor (RevMix) == 0.0f && e.valor (PitMix) == 0.0f && e.valor (ScaMix) == 0.0f, "els mòduls nous no sonen");
        });
    }
}
