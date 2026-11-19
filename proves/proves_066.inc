// 0.66: Saturació (Mibase); patrons de guitarra una octava més amunt.
#include "mibase/Saturacio.h"
namespace
{
    void proves066()
    {
        using namespace mibase;

        auto sinus = [] (float amp, float hz, int n, double fs = 48000.0)
        {
            std::vector<float> v ((size_t) n);
            for (int i = 0; i < n; ++i) v[(size_t) i] = amp * (float) std::sin (2.0 * 3.141592653589793 * hz * i / fs);
            return v;
        };
        auto passa = [] (saturacio::Saturacio& s, std::vector<float>& L, std::vector<float>& R)
        {
            for (int i = 0; i < (int) L.size(); i += 256) s.processa (L.data() + i, R.data() + i, std::min (256, (int) L.size() - i));
        };
        auto rms = [] (const std::vector<float>& v, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt (s / std::max (1, b - a)); };
        // amplitud d'un harmònic (DFT d'una freqüència)
        auto harm = [] (const std::vector<float>& v, int a, int b, double hz)
        {
            double re = 0, im = 0;
            for (int i = a; i < b; ++i) { const double w = 2.0 * 3.141592653589793 * hz * i / 48000.0; re += v[(size_t) i] * std::cos (w); im += v[(size_t) i] * std::sin (w); }
            return 2.0 * std::sqrt (re * re + im * im) / (b - a);
        };

        prova ("0.66 Saturació: amb drive fa harmònics; a 0 gairebé net; cap valor dolent", [&]
        {
            for (int e = 0; e < saturacio::numEstils; ++e)
            {
                saturacio::Saturacio s;
                s.prepara (48000.0, 256);
                s.posa (saturacio::EstilP, (float) e); s.posa (saturacio::Drive, 8); s.posa (saturacio::Auto, 0);
                auto L = sinus (0.5f, 200.0f, 48000), R = L;
                passa (s, L, R);
                const double f1 = harm (L, 24000, 48000, 200.0), h2 = harm (L, 24000, 48000, 400.0), h3 = harm (L, 24000, 48000, 600.0);
                const bool finit = std::all_of (L.begin(), L.end(), [] (float x) { return std::isfinite (x); });
                comprova (finit && (h2 + h3) / f1 > 0.03, std::string ("l'estil ") + saturacio::nomLlargEstil (e) + " fa harmònics");
            }
            saturacio::Saturacio s;
            s.prepara (48000.0, 256);
            s.posa (saturacio::Drive, 0); s.posa (saturacio::Auto, 0);
            auto L = sinus (0.05f, 200.0f, 48000), R = L;
            passa (s, L, R);
            comprova (harm (L, 24000, 48000, 400.0) / harm (L, 24000, 48000, 200.0) < 0.01, "drive 0 i so fluix: gairebé net");
        });

        prova ("0.66 Saturació: el T i el N fan parells, el P i l'E gairebé només senars", [&]
        {
            auto parells = [&] (int e)
            {
                saturacio::Saturacio s;
                s.prepara (48000.0, 256);
                s.posa (saturacio::EstilP, (float) e); s.posa (saturacio::Drive, 7); s.posa (saturacio::Auto, 0);
                auto L = sinus (0.5f, 300.0f, 48000), R = L;
                passa (s, L, R);
                return harm (L, 24000, 48000, 600.0) / harm (L, 24000, 48000, 900.0);
            };
            comprova (parells (saturacio::T) > 0.15 && parells (saturacio::N) > 0.15, "T i N: 2n harmònic important");
            comprova (parells (saturacio::P) < 0.1 && parells (saturacio::E) < 0.1, "P i E: simètrics");
        });

        prova ("0.66 Saturació: Auto manté el volum, sense alias i estat/presets", [&]
        {
            auto volum = [&] (float drive, bool aut)
            {
                saturacio::Saturacio s;
                s.prepara (48000.0, 256);
                s.posa (saturacio::Drive, drive); s.posa (saturacio::Auto, aut ? 1.0f : 0.0f); s.posa (saturacio::EstilP, saturacio::E);
                auto L = sinus (0.1f, 440.0f, 96000), R = L;
                passa (s, L, R);
                return 20.0 * std::log10 (rms (L, 72000, 96000) / rms (sinus (0.1f, 440.0f, 4800), 0, 4800));
            };
            const double senseA = volum (9, false), ambA = volum (9, true);
            std::printf ("        drive 9: sense Auto %+.1f dB, amb Auto %+.1f dB\n", senseA, ambA);
            comprova (senseA > 6.0 && std::fabs (ambA) < 1.5, "amb Auto el volum queda igual (±1,5 dB)");
            // alias: un 7 kHz molt saturat no ha de deixar gaire so a freqüències que no són harmònics (p. ex. 48k - 3×7k = 27k → 21 kHz reflectit)
            saturacio::Saturacio s;
            s.prepara (48000.0, 256);
            s.posa (saturacio::EstilP, saturacio::P); s.posa (saturacio::Drive, 10); s.posa (saturacio::Auto, 0);
            auto L = sinus (0.5f, 7000.0f, 48000), R = L;
            passa (s, L, R);
            const double fona = harm (L, 24000, 48000, 7000.0), alias = harm (L, 24000, 48000, 13000.0);   // 5×7k = 35k → 48-35 = 13 kHz
            std::printf ("        alias a 13 kHz: %.1f dB per sota\n", 20.0 * std::log10 (fona / std::max (1e-9, alias)));
            comprova (alias < fona * 0.03, "el sobremostreig treu els harmònics fantasma (>30 dB per sota)");
            // estat i presets
            saturacio::Saturacio a;
            a.posa (saturacio::EstilP, saturacio::T); a.posa (saturacio::Drive, 6.5f); a.posa (saturacio::Thump, 1);
            saturacio::Saturacio b;
            b.posaEstat (a.estat());
            comprova (b.estat() == a.estat(), "l'estat es desa i es llegeix igual");
            const auto& v = saturacio::presetsFabrica();
            std::printf ("        (%zu presets)\n", v.size());
            std::set<std::string> noms;
            bool be = true;
            for (const auto& p : v)
            {
                noms.insert (p.nom);
                saturacio::Saturacio x;
                x.posaEstat (p.estat);
                be = be && x.estat() == p.estat && ! p.descripcio.empty()
                     && std::find (saturacio::categories().begin(), saturacio::categories().end(), p.categoria) != saturacio::categories().end();
            }
            comprova (v.size() >= 24 && noms.size() == v.size() && be, "presets vàlids i sense noms repetits");
        });

        prova ("0.66 Patrons de guitarra una octava més amunt (i es desa)", [&]
        {
            using namespace mibase::edicio;
            Projecte q;
            q.tempo = 120.0;
            afegeixPista (q, "Guitarra", "midi");
            afegeixPista (q, "Acords", "acords");
            const int G = esAcords (q.pistes[0]) ? 1 : 0, A = 1 - G;
            Clip c; c.midi = true; c.pos = 0.0; c.inici = 0.0; c.fi = 4.0;
            q.pistes[(size_t) G].clips.push_back (c);
            Clip k; k.acord = "Am"; k.pos = 0.0; k.fi = 4.0; q.pistes[(size_t) A].clips.push_back (k);
            q.pistes[(size_t) G].segueixAcords = true;
            auto& cl = q.pistes[(size_t) G].clips[0];
            guitarra::posaBloc (cl, guitarra::blocDe (guitarra::patro ("folk"), 0, 3840, 0.0, 1, 88));
            guitarra::actualitza (q);
            auto abans = cl.notes;
            cl.blocs[0].octava = 1;
            comprova (guitarra::actualitza (q), "canviar l'octava reescriu les notes");
            bool totes = abans.size() == cl.notes.size() && ! abans.empty();
            for (size_t i = 0; totes && i < abans.size(); ++i) totes = cl.notes[i].to == abans[i].to + 12 && cl.notes[i].ini == abans[i].ini;
            comprova (totes, "les mateixes notes, 12 semitons més amunt");
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_066";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            comprova (disc::desa (dir / "p.mibase", q, "2026-11-19T15:00:00", "x", false).correcte, "desat");
            const auto l = disc::llegeix (dir / "p.mibase");
            comprova (l.has_value() && l->pistes[(size_t) G].clips[0].blocs[0].octava == 1, "l'octava es torna a llegir");
            std::filesystem::remove_all (dir);
        });
    }
}
