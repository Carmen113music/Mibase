// 1.15: EVOLVE fase 3: Granular, Shimmer +24 i el valor que sona ara (per als anells de modulació).
#include "mibase/Evolve.h"
#include "mibase/Reverb.h"
#include "mibase/Drive.h"
#include <filesystem>
namespace
{
    void proves115()
    {
        using namespace mibase::evolve;
        mibase::ContextMusical::global().sonant.store (false);
        static std::atomic<double> t120b { 120.0 };

        prova ("1.15 EVOLVE: presets amb el Granular i Shimmer a +24", []
        {
            comprova (presetsFabrica().size() >= 34, "almenys 34 presets");
            Evolve e; e.posaEstat (preset113 ("Vocal Texture").estat);
            bool ruta = false;
            for (const auto& r : e.rutes()) ruta = ruta || r.param == GraMix;
            comprova (preset113 ("Vocal Texture").nom == "Vocal Texture" && ruta, "Vocal Texture fa servir el Granular");
            comprova (mibase::reverb::numTransposicions() == 5 && mibase::reverb::transposicio (4) == 24 && mibase::reverb::transposicio (3) == -12,
                      "la Reverb té +24 al final (les d'abans no es mouen)");
            comprova (textValor (ShmPitch, 4) == "+24", "EVOLVE: Shimmer +24");
        });

        prova ("1.15 EVOLVE: Granular (to, freeze, mix 0, sempre igual, sense saturar)", []
        {
            auto e = sol114 (MGranular, { { GraPitch, 12 }, { GraMix, 100 }, { GraSize, 80 }, { GraDens, 30 }, { GraSpray, 20 }, { GraWidth, 0 } }, &t120b);
            Senyal114 s; s.L.resize (96000); s.R.resize (96000);
            for (int i = 0; i < 96000; ++i) s.L[(size_t) i] = s.R[(size_t) i] = 0.5f * (float) std::sin (2 * 3.14159265 * 220.0 * i / 48000.0);
            passa113 (*e, s);
            const double f = freq114 (s.L, 60000, 4000, 300, 700);
            comprova (std::fabs (f - 440.0) < 10.0, "grans una octava amunt: 440 Hz (" + std::to_string (f) + ")");
            comprova (finit113 (s) && pic113 (s) < 1.0f, "sense NaN ni saturació");

            // freeze: el núvol continua quan el so s'atura
            auto fes = [] (bool freeze)
            {
                auto g = sol114 (MGranular, { { GraMix, 100 }, { GraSize, 100 }, { GraDens, 25 }, { GraPos, 200 }, { GraSpray, 300 } }, &t120b);
                auto x = font113 (1, 48000.0, 1.0);
                x.L.resize (48000 * 4, 0.0f); x.R.resize (48000 * 4, 0.0f);
                for (size_t o = 0; o < x.L.size(); o += 256)
                {
                    if (freeze && o == 48128) g->posa (GraFreeze, 1.0f);
                    g->processa (x.L.data() + o, x.R.data() + o, (int) std::min<size_t> (256, x.L.size() - o));
                }
                return energia114 (x.L, 3.0, 4.0);
            };
            const double amb = fes (true), sense = fes (false);
            comprova (amb > 1.0, "amb Freeze, continua sonant 2 s després (" + std::to_string (amb) + ")");
            comprova (sense < 1e-6, "sense Freeze, s'apaga (" + std::to_string (sense) + ")");

            {
                auto g = sol114 (MGranular, { { GraMix, 100 }, { GraFreeze, 1 } }, &t120b);
                auto x = font113 (1, 48000.0, 3.0);
                passa113 (*g, x);
                comprova (energia114 (x.L, 2.0, 3.0) > 1.0, "amb Freeze des del començament, primer grava prou i després congela");
            }
            auto z = sol114 (MGranular, { { GraMix, 0 } }, &t120b);
            auto a = font113 (0, 48000.0, 1.0);
            const auto orig = a;
            passa113 (*z, a);
            comprova (a.L == orig.L, "amb Mix 0, el so no es toca");

            auto fes2 = []
            {
                auto g = sol114 (MGranular, { { GraMix, 80 }, { GraRandPitch, 60 }, { GraReverse, 50 } }, &t120b);
                auto x = font113 (4, 48000.0, 2.0);
                passa113 (*g, x);
                return x;
            };
            comprova (fes2().L == fes2().L, "sempre fa el mateix (també en exportar)");

            auto h = sol114 (MGranular, { { GraMix, 100 }, { GraDens, 80 }, { GraSize, 500 }, { GraFeedback, 90 }, { GraWidth, 100 }, { Entrada, 12 } }, &t120b);
            auto y = font113 (4, 48000.0, 4.0);
            passa113 (*h, y);
            comprova (finit113 (y) && pic113 (y) < 1.0f, "al màxim (densitat, gra, feedback, +12 dB): no satura (" + std::to_string (pic113 (y)) + ")");
        });

        prova ("1.15 Presets propis: tornen en obrir (EVOLVE, sintes i Guitar Rack fan servir la biblioteca del Drive)", []
        {
            const auto f = std::filesystem::temp_directory_path() / "mibase_prova_115_presets.json";
            mibase::drive::BibliotecaPresets b;
            b.desaPreset ({ "Meu evolve", "Meus", "", "mibase-evolve=1;evolution=40" });
            b.desaPreset ({ "Meu baix", "Meus", "", "mibase-subbass=1;cutoff=300" });
            b.desaPreset ({ "Meu ampli", "Meus", "", "{\"v\":1,\"amp\":\"x\"}" });
            comprova (b.desa (f), "es desa");
            mibase::drive::BibliotecaPresets c;
            comprova (c.carrega (f) && c.usuari.size() == 3, "tots tres tornen (abans: cap)");
            mibase::drive::BibliotecaPresets d;
            d.prefix = "mibase-evolve=";
            comprova (d.carrega (f) && d.usuari.size() == 1 && d.usuari[0].nom == "Meu evolve", "amb filtre (l'EVOLVE), només els seus");
            std::filesystem::remove (f);
        });

        prova ("1.15 EVOLVE: el valor que sona ara (rutes + LFO) i estats de la 1.14", []
        {
            auto e = sol114 (MFiltre, { { FilMix, 100 }, { FilFreq, 1000 }, { L1Forma, 2 }, { L1Depth, 100 } }, &t120b);
            e->posaAssignacions ({ { 0, FilFreq, 30 } });
            e->posaRutes ({ { FilMix, 0, 100, 50, Lineal } });
            Senyal114 s; s.L.assign (2048, 0.1f); s.R.assign (2048, 0.1f);
            passa113 (*e, s);
            comprova (std::fabs (e->mesures().efectiu[FilFreq].load() - 1000.0f) > 100.0f, "l'LFO mou el Cutoff que sona");
            comprova (std::fabs (e->mesures().efectiu[FilMix].load() - 75.0f) < 0.5f, "la ruta: a 50 %, el Mix és 75");
            Evolve w; w.posaAssignacions ({ { 0, GraSize, 30 }, { 1, L1Rate, 30 } });
            comprova (w.assignacions().size() == 1 && w.assignacions()[0].param == GraSize, "un LFO pot moure el Granular (però no un altre LFO)");
            Evolve v; v.posaEstat ("mibase-evolve=1;evolution=30;ordre=5,1,2,6,4,0,3");
            comprova (v.ordre()[6] == MShimmer && v.ordre()[7] == MGranular, "un estat de la 1.14: el Granular, al final");
            comprova (v.valor (GraMix) == 0.0f && v.latencia() == 0, "i no sona ni retarda");
        });
    }
}
