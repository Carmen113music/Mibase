// 0.96: Clipper i Transient Shaper del Mastering Studio.
#include "mibase/Clipper.h"
#include "mibase/Transitoris.h"
namespace
{
    /** Cops de bateria de mentida: un sinus de 120 Hz que cau ràpid, cada 250 ms. */
    Estereo094 cops096 (double fs, double segons, double amp = 0.5)
    {
        Estereo094 s;
        const size_t n = (size_t) (fs * segons), periode = (size_t) (fs * 0.25);
        s.L.resize (n); s.R.resize (n);
        for (size_t i = 0; i < n; ++i)
        {
            const double t = (double) (i % periode) / fs;
            const float v = (float) (amp * std::exp (-t / 0.04) * std::sin (2.0 * 3.14159265358979 * 120.0 * t));
            s.L[i] = v; s.R[i] = v;
        }
        return s;
    }
    float rms096 (const std::vector<float>& v, size_t a, size_t b) { double e = 0; for (size_t i = a; i < b; ++i) e += (double) v[i] * v[i]; return (float) std::sqrt (e / (double) std::max<size_t> (1, b - a)); }

    void proves096()
    {
        using namespace mibase;
        prova ("0.96 Clipper: talla al sostre, transparent per sota i retard exacte amb oversampling", []
        {
            using namespace mibase::clipper;
            for (int m = 0; m < numModes; ++m)
            {
                Clipper c; c.posa (ModeP, (float) m); c.posa (Oversampling, 0); c.posa (Ceiling, -6); c.prepara (48000, 512);
                auto s = sinus094 (48000, 0.5, 200, 0.0);
                processa094 (c, s);
                comprova (pic (s.L, 1000, s.L.size()) <= 0.5012f + 1e-4f, std::string ("") + nomMode (m) + ": cap mostra passa de -6 dB (" + std::to_string (pic (s.L, 1000, s.L.size())) + ")");
            }
            Clipper h; h.posa (ModeP, Hard); h.posa (Oversampling, 0); h.prepara (48000, 512);
            auto petit = sinus094 (48000, 0.2, 440, -20), orig = petit;
            processa094 (h, petit);
            float dif = 0; for (size_t i = 0; i < petit.L.size(); ++i) dif = std::max (dif, std::abs (petit.L[i] - orig.L[i]));
            comprova (dif < 1e-6f, "Hard per sota del sostre: no toca res");
            Clipper o; o.posa (Mix, 0); o.prepara (48000, 512);
            comprova (o.latencia() == latenciaOs, "amb oversampling diu el seu retard");
            auto imp = sinus094 (48000, 0.05, 1, -200);
            imp.L[100] = imp.R[100] = 0.5f;
            processa094 (o, imp, 64);
            comprova (std::abs (imp.L[100 + latenciaOs] - 0.5f) < 1e-6f, "Mix 0 %: el sec surt exactament 12 mostres tard");
            Clipper os; os.posa (ModeP, Soft); os.posa (Ceiling, -20); os.prepara (48000, 512);
            auto t = sinus094 (48000, 0.5, 1000, -30), t0 = t;
            processa094 (os, t);
            float e = 0; for (size_t i = 2000; i < t.L.size(); ++i) e = std::max (e, std::abs (t.L[i] - t0.L[i - latenciaOs]));
            comprova (e < 2e-3f, "amb oversampling, per sota del genoll gairebé igual (" + std::to_string (e) + ")");
            Clipper d; d.posa (Drive, 6); d.posa (Ceiling, -1); d.prepara (48000, 512);
            auto c = cops096 (48000, 2, 0.8);
            processa094 (d, c);
            comprova (d.mesures().retallMax.load() > 1.0f && totFinit (c.L), "amb drive, retalla i ho mesura (" + std::to_string (d.mesures().retallMax.load()) + " dB)");
        });

        prova ("0.96 Transient Shaper: neutre a 0; Atac puja el cop; Sustain allarga la cua", []
        {
            using namespace mibase::transitoris;
            auto mesura = [] (float atac, float sus)
            {
                Transitoris t; t.posa (Atac, atac); t.posa (Sustain, sus); t.prepara (48000, 512);
                auto s = cops096 (48000, 3);
                processa094 (t, s);
                // l'últim segon: el cop (primers 10 ms de cada període) i la cua (de 60 a 200 ms)
                float cop = 0, cua = 0;
                const size_t p = 12000;
                for (size_t a = 48000 * 2; a + p <= s.L.size(); a += p) { cop += rms096 (s.L, a, a + 480); cua += rms096 (s.L, a + 2880, a + 9600); }
                return std::make_pair (cop, cua);
            };
            const auto zero = mesura (0, 0);
            const auto ref = [] { auto s = cops096 (48000, 3); float cop = 0, cua = 0; for (size_t a = 48000 * 2; a + 12000 <= s.L.size(); a += 12000) { cop += rms096 (s.L, a, a + 480); cua += rms096 (s.L, a + 2880, a + 9600); } return std::make_pair (cop, cua); }();
            comprova (std::abs (zero.first - ref.first) < 1e-4f && std::abs (zero.second - ref.second) < 1e-4f, "a 0 no canvia res");
            const auto mes = mesura (100, 0), menys = mesura (-100, 0);
            comprova (mes.first > zero.first * 1.3f && menys.first < zero.first * 0.8f, "Atac +: més cop; Atac −: menys");
            const auto llarg = mesura (0, 100), curt = mesura (0, -100);
            comprova (llarg.second > zero.second * 1.2f && curt.second < zero.second * 0.85f, "Sustain +: cua més llarga; −: més curta");
        });

        prova ("0.96 Mastering Studio: presets amb clipper i transient shaper, macros Punch i Loudness", []
        {
            using namespace mibase::rack;
            bool clip = false, tr = false;
            for (const auto& p : presetsFabrica (Tipus::Master)) { clip = clip || p.estat.find ("\"clipper\"") != std::string::npos; tr = tr || p.estat.find ("\"transitoris\"") != std::string::npos; }
            comprova (clip && tr, "hi ha presets amb clipper i amb transient shaper");
            Rack r (Tipus::Master);
            for (const auto& p : presetsFabrica (Tipus::Master)) if (p.nom == "Rock contundent") r.posaEstat (p.estat);
            int iClip = -1, iLim = -1;
            for (int i = 0; i < r.numModuls(); ++i) { if (r.modul (i)->clau == "clipper") iClip = i; if (r.modul (i)->clau == "limitm") iLim = i; }
            comprova (iClip >= 0 && iLim == iClip + 1, "el clipper va just abans del limitador");
            bool loudClip = false, punchTr = false;
            for (const auto& e : r.enllacos()) { loudClip = loudClip || (e.macro == 7 && e.modul == iClip); punchTr = punchTr || (e.macro == 2 && r.modul (e.modul)->clau == "transitoris"); }
            comprova (loudClip && punchTr, "Loudness mou també el clipper; Punch, el transient shaper");
            comprova (busca ("clipper") != nullptr && busca ("transitoris") != nullptr, "es poden afegir des del menú");
        });
    }
}
