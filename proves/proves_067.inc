// 0.67: Drive (Mibase), el pedal; Saturació amb l'estil Suau.
#include "mibase/Drive.h"
namespace
{
    void proves067()
    {
        using namespace mibase;
        auto sinus = [] (float amp, float hz, int n)
        {
            std::vector<float> v ((size_t) n);
            for (int i = 0; i < n; ++i) v[(size_t) i] = amp * (float) std::sin (2.0 * 3.141592653589793 * hz * i / 48000.0);
            return v;
        };
        auto harm = [] (const std::vector<float>& v, int a, int b, double hz)
        {
            double re = 0, im = 0;
            for (int i = a; i < b; ++i) { const double w = 2.0 * 3.141592653589793 * hz * i / 48000.0; re += v[(size_t) i] * std::cos (w); im += v[(size_t) i] * std::sin (w); }
            return 2.0 * std::sqrt (re * re + im * im) / (b - a);
        };
        auto rms = [] (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / std::max (1, b - a)); };
        auto passa = [] (drive::Drive& d, std::vector<float>& L, std::vector<float>& R)
        {
            for (int i = 0; i < (int) L.size(); i += 256) d.processa (L.data() + i, R.data() + i, std::min (256, (int) L.size() - i));
        };

        prova ("0.67 Drive: els quatre modes distorsionen, més gain = més harmònics, volum raonable", [&]
        {
            for (int m = 0; m < drive::numModes; ++m)
            {
                auto thd = [&] (float gain, double* nivell)
                {
                    drive::Drive d;
                    d.prepara (48000.0, 256);
                    d.posa (drive::ModeP, (float) m); d.posa (drive::Gain, gain);
                    auto L = sinus (0.3f, 220.0f, 48000), R = L;
                    passa (d, L, R);
                    if (nivell) *nivell = 20.0 * std::log10 (rms (L, 24000, 48000) / (0.3 / std::sqrt (2.0)));
                    const bool finit = std::all_of (L.begin(), L.end(), [] (float x) { return std::isfinite (x); });
                    return finit ? (harm (L, 24000, 48000, 660.0) + harm (L, 24000, 48000, 440.0)) / harm (L, 24000, 48000, 220.0) : -1.0;
                };
                double niv = 0;
                const double poc = thd (1.0f, nullptr), molt = thd (9.0f, &niv);
                std::printf ("        %s: gain 1 → %.2f, gain 9 → %.2f, volum %+.1f dB\n", drive::nomMode (m), poc, molt, niv);
                comprova (poc >= 0 && molt > poc && molt > 0.15, std::string (drive::nomMode (m)) + ": distorsiona i el gain hi fa");
                comprova (niv > -12.0 && niv < 9.0, std::string (drive::nomMode (m)) + ": amb Level a 0 el volum queda a prop de l'original");
            }
        });

        prova ("0.67 Drive: gate, mix, estat i presets", [&]
        {
            // gate: un so molt fluix (brunzit) queda tallat
            drive::Drive d;
            d.prepara (48000.0, 256);
            d.posa (drive::ModeP, drive::Distorsio); d.posa (drive::Gain, 9); d.posa (drive::Gate, -50);
            auto L = sinus (0.0005f, 100.0f, 48000), R = L;   // -66 dB
            passa (d, L, R);
            comprova (rms (L, 24000, 48000) < 1e-5, "el gate talla el brunzit");
            drive::Drive s;
            s.prepara (48000.0, 256);
            s.posa (drive::ModeP, drive::Distorsio); s.posa (drive::Gain, 9);
            auto L2 = sinus (0.0005f, 100.0f, 48000), R2 = L2;
            passa (s, L2, R2);
            comprova (rms (L2, 24000, 48000) > 1e-4, "sense gate, el brunzit s'amplifica");
            // mix 0: el so net
            drive::Drive m;
            m.prepara (48000.0, 256);
            m.posa (drive::Mix, 0);
            auto L3 = sinus (0.3f, 220.0f, 24000), R3 = L3;
            const auto orig = L3;
            passa (m, L3, R3);
            float dif = 0; for (size_t i = 12000; i < orig.size(); ++i) dif = std::max (dif, std::fabs (L3[i] - orig[i]));
            comprova (dif < 1e-3f, "amb Mix a 0 surt el so net");
            // estat i presets
            drive::Drive a;
            a.posa (drive::ModeP, drive::Fuzz); a.posa (drive::Gain, 7.5f); a.posa (drive::Tight, 1);
            drive::Drive b;
            b.posaEstat (a.estat());
            comprova (b.estat() == a.estat(), "l'estat es desa i es llegeix igual");
            b.posaEstat ("mibase-saturacio=1;estil=2");
            comprova (b.estat() == a.estat(), "un estat d'un altre efecte no hi entra");
            const auto& v = drive::presetsFabrica();
            std::printf ("        (%zu presets)\n", v.size());
            std::set<std::string> noms;
            bool be = true;
            for (const auto& p : v)
            {
                noms.insert (p.nom);
                drive::Drive x;
                x.posaEstat (p.estat);
                be = be && x.estat() == p.estat && ! p.descripcio.empty()
                     && std::find (drive::categories().begin(), drive::categories().end(), p.categoria) != drive::categories().end();
            }
            comprova (v.size() >= 20 && noms.size() == v.size() && be, "presets vàlids i sense noms repetits");
        });

        prova ("0.67 Saturació: l'estil Suau i els presets sense estils de pedal", [&]
        {
            comprova (saturacio::numEstils == 6 && std::string (saturacio::nomEstil (saturacio::Suau)) == "Suau", "6 estils, l'últim Suau");
            bool be = true;
            for (const auto& p : saturacio::presetsFabrica())
            {
                saturacio::Saturacio x;
                x.posaEstat (p.estat);
                be = be && x.valor (saturacio::EstilP) < saturacio::numEstils;
            }
            comprova (be && saturacio::presetsFabrica().size() >= 24, "tots els presets fan servir estils que existeixen");
        });
    }
}
