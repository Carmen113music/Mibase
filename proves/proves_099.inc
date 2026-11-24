// 0.99: Multibanda i Low End Focus del Mastering Studio.
#include "mibase/LowEnd.h"
#include "mibase/Multibanda.h"
namespace
{
    /** Energia d'una sinusoide a la sortida (dB), després d'un temps d'entrada. */
    float guanySinus099 (mibase::Efecte& fx, double hz, double dbfs = -20.0)
    {
        auto s = sinus094 (48000, 1.0, hz, dbfs);
        processa094 (fx, s);
        const float a = std::pow (10.0f, (float) dbfs / 20.0f) / std::sqrt (2.0f);
        return 20.0f * std::log10 (std::max (1e-9f, rms096 (s.L, 24000, 48000)) / a);
    }

    void proves099()
    {
        using namespace mibase;
        prova ("0.99 Multibanda: sense comprimir, la suma de bandes és plana (2, 3 i 4 bandes)", []
        {
            using namespace mibase::multibanda;
            for (int nb = 2; nb <= 4; ++nb)
            {
                float pitjor = 0;
                for (double hz : { 40.0, 120.0, 400.0, 800.0, 2000.0, 5000.0, 9000.0 })
                {
                    Multibanda m; m.posa (Bandes, (float) nb);
                    for (int b = 0; b < maxBandes; ++b) m.posa (index (b, Llindar), 0);
                    m.prepara (48000, 512);
                    pitjor = std::max (pitjor, std::abs (guanySinus099 (m, hz)));
                }
                comprova (pitjor < 0.1f, std::to_string (nb) + " bandes: ±" + std::to_string (pitjor) + " dB");
            }
        });

        prova ("0.99 Multibanda: comprimeix només la banda que passa del llindar; solo i mute", []
        {
            using namespace mibase::multibanda;
            Multibanda m; m.posa (Bandes, 3); m.posa (X1, 150); m.posa (X2, 3000);
            m.posa (index (0, Llindar), -30); m.posa (index (0, Ratio), 4);
            m.posa (index (1, Llindar), 0); m.posa (index (2, Llindar), 0);
            m.prepara (48000, 512);
            const float greu = guanySinus099 (m, 60, -10);
            comprova (greu < -5.0f && m.mesures().reduccio[0].load() > 5.0f, "60 Hz fort: es comprimeix (" + std::to_string (greu) + " dB)");
            Multibanda m2; m2.posa (Bandes, 3); m2.posa (X1, 150); m2.posa (X2, 3000); m2.posa (index (0, Llindar), -30); m2.posa (index (0, Ratio), 4);
            m2.posa (index (1, Llindar), 0); m2.posa (index (2, Llindar), 0); m2.prepara (48000, 512);
            const float mig = guanySinus099 (m2, 1000, -10);
            comprova (std::abs (mig) < 0.3f, "1 kHz: la banda del mig no es toca (" + std::to_string (mig) + " dB)");
            Multibanda s; s.posa (Bandes, 3); s.posa (index (2, Solo), 1); s.prepara (48000, 512);
            comprova (guanySinus099 (s, 60) < -30.0f, "solo dels aguts: el greu desapareix");
            Multibanda u; u.posa (Bandes, 3); u.posa (X1, 150); u.posa (X2, 3000); u.posa (index (1, Mute), 1); u.prepara (48000, 512);
            comprova (guanySinus099 (u, 1000) < -25.0f && std::abs (guanySinus099 (u, 8000)) < 0.5f, "mute dels mitjos");
            Multibanda e; e.posa (index (2, Ratio), 3.5f); e.posa (X2, 1234);
            Multibanda f; f.posaEstat (e.estat());
            comprova (std::abs (f.valor (index (2, Ratio)) - 3.5f) < 1e-3f && std::abs (f.valor (X2) - 1234.0f) < 0.5f, "es desa i es recupera");
        });

        prova ("0.99 Low End Focus: neutre, Sub, Fang, Mono, Tight i mesura dels greus", []
        {
            using namespace mibase::lowend;
            float pitjor = 0;
            for (double hz : { 40.0, 100.0, 250.0, 1000.0, 6000.0 }) { LowEnd g; g.prepara (48000, 512); pitjor = std::max (pitjor, std::abs (guanySinus099 (g, hz))); }
            comprova (pitjor < 0.1f, "tot a zero: pla (±" + std::to_string (pitjor) + " dB)");
            LowEnd s; s.posa (Sub, 6); s.prepara (48000, 512);
            comprova (guanySinus099 (s, 45) > 4.0f, "Sub +6: puja els 45 Hz");
            LowEnd f; f.posa (Fang, 100); f.prepara (48000, 512);
            comprova (guanySinus099 (f, 250) < -5.0f, "Fang 100 %: -6 dB a 250 Hz");
            LowEnd mo; mo.posa (Mono, 150); mo.prepara (48000, 512);
            auto lat = sinus094 (48000, 1, 50, -6, 0.0, true);
            processa094 (mo, lat);
            comprova (pic (lat.L, 24000, 48000) < 0.05f, "Mono 150: el greu als costats desapareix");
            auto cua = [] (float tight)
            {
                LowEnd t; t.posa (Tight, tight); t.prepara (48000, 512);
                auto s = cops096 (48000, 3);
                processa094 (t, s);
                float e = 0; for (size_t a = 48000 * 2; a + 12000 <= s.L.size(); a += 12000) e += rms096 (s.L, a + 2880, a + 9600);
                return e;
            };
            comprova (cua (100) < cua (0) * 0.85f, "Tight escurça la cua dels greus");
            LowEnd m; m.posa (Sub, -6); m.prepara (48000, 512);
            auto x = cops096 (48000, 1); processa094 (m, x);
            comprova (m.mesures().greusIn.load() > m.mesures().greusOut.load(), "mesura els greus abans i després");
        });

        prova ("0.99 Mastering Studio: multibanda i low end als presets; macros Greus i Densitat", []
        {
            using namespace mibase::rack;
            Rack r (Tipus::Master);
            bool hi = false;
            for (const auto& p : presetsFabrica (Tipus::Master)) if (p.nom == "Greus controlats") { r.posaEstat (p.estat); hi = true; }
            comprova (hi, "hi ha el preset «Greus controlats»");
            int iMb = -1, iLe = -1;
            for (int i = 0; i < r.numModuls(); ++i) { if (r.modul (i)->clau == "multibanda") iMb = i; if (r.modul (i)->clau == "lowend") iLe = i; }
            comprova (iMb >= 0 && iLe > iMb, "multibanda i després low end");
            bool greus = false, dens = false;
            for (const auto& e : r.enllacos()) { greus = greus || (e.macro == 4 && e.modul == iLe); dens = dens || (e.macro == 5 && e.modul == iMb); }
            comprova (greus && dens, "Greus mou el Low End Focus; Densitat, el Mix del multibanda");
            comprova (busca ("multibanda") != nullptr && busca ("lowend") != nullptr, "es poden afegir des del menú");
        });
    }
}
