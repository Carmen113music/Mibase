// 0.73: canvi de to en temps real (Harmonitzador, Octava i formant) i context musical.
#include "mibase/Harmonitzador.h"
#include "mibase/Veu.h"
#include "mibase/Rack.h"
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
