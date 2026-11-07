// 0.22: proves del Vocal Editor (anàlisi de veu i PSOLA), amb veus sintètiques.
#include "mibase/Veu.h"
#include "mibase/ProjecteDisc.h"

namespace
{
    namespace vv = mibase::veu;

    /** Una «veu» sintètica: harmònics amb una envolupant de formants (vocal «a»), seguint una corba d'afinació (MIDI
        per mostra; NaN = silenci). «formants» en Hz. */
    std::vector<float> veuSintetica (const std::vector<double>& midi, double fs, std::vector<double> formants = { 700.0, 1200.0, 2600.0 },
                                     double febleFonamental = 1.0)
    {
        std::vector<float> y (midi.size(), 0.0f);
        double fase = 0.0, env = 0.0;
        for (size_t i = 0; i < midi.size(); ++i)
        {
            const bool so = ! std::isnan (midi[i]);
            env += ((so ? 1.0 : 0.0) - env) * 0.004;
            if (! so && env < 1e-4) { fase = 0.0; continue; }
            const double f0 = vv::hzDeMidi (so ? midi[i] : 60.0);
            fase += 2.0 * 3.14159265358979 * f0 / fs;
            double v = 0.0;
            for (int h = 1; h * f0 < fs * 0.45 && h < 60; ++h)
            {
                const double fh = h * f0;
                double a = 0.0;
                for (double fm : formants) a += 1.0 / (1.0 + std::pow ((fh - fm) / 90.0, 2.0));
                a = (a + 0.02) / h * (h == 1 ? febleFonamental : 1.0);
                v += a * std::sin (h * fase);
            }
            y[i] = (float) (0.25 * env * v);
        }
        return y;
    }

    std::vector<double> corbaNotes (const std::vector<std::pair<double, double>>& notes, double fs)   // (midi, segons); midi NaN = silenci
    {
        std::vector<double> c;
        for (auto [m, s] : notes) c.insert (c.end(), (size_t) (s * fs), m);
        return c;
    }

    double mitjanaF0 (const vv::Analisi& a, double de, double fins)
    {
        double s = 0.0; int n = 0;
        for (int i = (int) (de / vv::pas); i < std::min (a.punts(), (int) (fins / vv::pas)); ++i)
            if (a.f0[(size_t) i] > 0.0f) { s += vv::midiDeHz (a.f0[(size_t) i]); ++n; }
        return n > 0 ? s / n : -1.0;
    }

    double fraccioSonora (const vv::Analisi& a, double de, double fins)
    {
        int s = 0, n = 0;
        for (int i = (int) (de / vv::pas); i < std::min (a.punts(), (int) (fins / vv::pas)); ++i) { ++n; if (a.f0[(size_t) i] > 0.0f) ++s; }
        return n > 0 ? (double) s / n : 0.0;
    }

    /** Magnitud de la DFT a una freqüència (per mirar els harmònics). */
    double dft (const std::vector<float>& x, size_t de, size_t n, double f, double fs)
    {
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < n && de + i < x.size(); ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * (double) i / (double) n);
            re += w * x[de + i] * std::cos (2.0 * 3.14159265358979 * f * (double) i / fs);
            im += w * x[de + i] * std::sin (2.0 * 3.14159265358979 * f * (double) i / fs);
        }
        return std::sqrt (re * re + im * im);
    }

    std::vector<float> renderitzaTot (const std::vector<float>& x, double fs, const vv::Analisi& a, const mibase::VeuPresa& v)
    {
        std::vector<float> y = x;
        const float* in[1] { x.data() };
        float* out[1] { y.data() };
        const auto c = vv::corbes (a, v);
        for (auto t : vv::trossosPerRefer (a, {}, c)) vv::renderitza (in, 1, (long long) x.size(), fs, a, c, out, t.de, t.a);
        return y;
    }

    void provesVeu()
    {
        const double fs = 44100.0;

        prova ("veu: nota llarga i afinació", [fs]
        {
            const auto x = veuSintetica (corbaNotes ({ { NAN, 0.2 }, { 57.0, 1.0 }, { NAN, 0.2 } }, fs), fs);
            const auto a = vv::analitza (x.data(), (long long) x.size(), fs);
            comprova (a.punts() == (int) std::ceil (1.4 / vv::pas), "un punt cada 5 ms");
            comprova (std::abs (mitjanaF0 (a, 0.3, 1.1) - 57.0) < 0.05, "A3 dins de 5 cents: " + std::to_string (mitjanaF0 (a, 0.3, 1.1)));
            comprova (fraccioSonora (a, 0.25, 1.15) > 0.97, "la nota té to");
            comprova (fraccioSonora (a, 0.0, 0.15) < 0.05 && fraccioSonora (a, 1.27, 1.4) < 0.05, "el silenci no en té");
            const auto n = vv::detectaNotes (a);
            comprova (n.size() == 1 && std::abs (n[0].centre - 57.0) < 0.05, "una nota a A3");
            if (n.size() == 1) comprova (n[0].ini > 0.15 && n[0].ini < 0.3 && n[0].fi > 1.1 && n[0].fi < 1.3, "on toca");
        });

        prova ("veu: el vibrato no fa notes de més", [fs]
        {
            for (double ritme : { 4.2, 5.5, 7.0 })
            {
                std::vector<double> c = corbaNotes ({ { NAN, 0.1 } }, fs);
                for (int i = 0; i < (int) (1.6 * fs); ++i) c.push_back (64.0 + 0.9 * std::sin (2.0 * 3.14159265358979 * ritme * i / fs));
                const auto rr = corbaNotes ({ { NAN, 0.1 } }, fs); c.insert (c.end(), rr.begin(), rr.end());
                const auto x = veuSintetica (c, fs);
                const auto a = vv::analitza (x.data(), (long long) x.size(), fs);
                const auto n = vv::detectaNotes (a);
                comprova (n.size() == 1, "vibrato de " + std::to_string (ritme) + " Hz: una nota (" + std::to_string (n.size()) + ")");
                if (! n.empty()) comprova (std::abs (n[0].centre - 64.0) < 0.15, "centre a E4: " + std::to_string (n[0].centre));
            }
        });

        prova ("veu: notes ràpides lligades", [fs]
        {
            const auto x = veuSintetica (corbaNotes ({ { NAN, 0.1 }, { 60.0, 0.15 }, { 62.0, 0.15 }, { 64.0, 0.15 }, { 65.0, 0.15 }, { 67.0, 0.3 }, { NAN, 0.1 } }, fs), fs);
            const auto a = vv::analitza (x.data(), (long long) x.size(), fs);
            const auto n = vv::detectaNotes (a);
            comprova (n.size() == 5, "cinc notes (" + std::to_string (n.size()) + ")");
            const double esperat[] { 60, 62, 64, 65, 67 };
            for (size_t k = 0; k < std::min<size_t> (5, n.size()); ++k)
                comprova (std::abs (n[k].centre - esperat[k]) < 0.25, "nota " + std::to_string (k) + ": " + std::to_string (n[k].centre));
        });

        prova ("veu: la deriva lenta no parteix la nota", [fs]
        {
            std::vector<double> c = corbaNotes ({ { NAN, 0.1 } }, fs);
            for (int i = 0; i < (int) (1.2 * fs); ++i) c.push_back (62.0 - 0.7 * i / (1.2 * fs));
            const auto rr = corbaNotes ({ { NAN, 0.1 } }, fs); c.insert (c.end(), rr.begin(), rr.end());
            const auto x = veuSintetica (c, fs);
            const auto n = vv::detectaNotes (vv::analitza (x.data(), (long long) x.size(), fs));
            comprova (n.size() == 1, "una nota que baixa és una sola nota (" + std::to_string (n.size()) + ")");
        });

        prova ("veu: harmònics forts no enganyen l'octava", [fs]
        {
            const auto x = veuSintetica (corbaNotes ({ { NAN, 0.1 }, { 45.0, 1.0 }, { NAN, 0.1 } }, fs), fs, { 500.0, 1500.0, 2500.0 }, 0.1);
            const auto a = vv::analitza (x.data(), (long long) x.size(), fs);
            comprova (std::abs (mitjanaF0 (a, 0.2, 1.0) - 45.0) < 0.1, "A2 amb la fonamental fluixa: " + std::to_string (mitjanaF0 (a, 0.2, 1.0)));
        });

        prova ("veu: consonants, respiracions i soroll", [fs]
        {
            auto x = veuSintetica (corbaNotes ({ { NAN, 0.1 }, { 60.0, 0.5 }, { NAN, 0.25 }, { 64.0, 0.5 }, { NAN, 0.25 } }, fs), fs);
            std::uint32_t llavor = 12345;
            auto soroll = [&] { llavor = llavor * 1664525u + 1013904223u; return ((llavor >> 8) / 8388608.0f) - 1.0f; };
            // «s» entre les dues notes (soroll agut) i una respiració al final (soroll suau).
            float ant = 0.0f;
            for (size_t i = (size_t) (0.62 * fs); i < (size_t) (0.82 * fs); ++i) { const float s = soroll(); x[i] += 0.12f * (s - ant); ant = s; }
            for (size_t i = (size_t) (1.37 * fs); i < (size_t) (1.58 * fs); ++i) x[i] += 0.02f * soroll();
            // Soroll de fons moderat (-40 dB) a tot arreu.
            for (auto& v : x) v += 0.004f * soroll();
            const auto a = vv::analitza (x.data(), (long long) x.size(), fs);
            const auto n = vv::detectaNotes (a);
            comprova (n.size() == 2, "dues notes, cap per a la «s» ni la respiració (" + std::to_string (n.size()) + ")");
            comprova (fraccioSonora (a, 0.65, 0.8) < 0.1, "la «s» no té to");
            if (n.size() == 2) comprova (std::abs (n[0].centre - 60.0) < 0.1 && std::abs (n[1].centre - 64.0) < 0.1, "C4 i E4");
        });

        prova ("veu: sense canvis, el so és idèntic", [fs]
        {
            const auto x = veuSintetica (corbaNotes ({ { NAN, 0.1 }, { 60.0, 0.6 }, { NAN, 0.1 } }, fs), fs);
            const auto a = vv::analitza (x.data(), (long long) x.size(), fs);
            mibase::VeuPresa v; v.notes = vv::detectaNotes (a); v.analitzada = true;
            comprova (! vv::teCanvis (v) && vv::trossosAmbCanvis (a, vv::corbes (a, v)).empty(), "res per fer");
            comprova (renderitzaTot (x, fs, a, v) == x, "idèntic");
        });

        prova ("veu: PSOLA canvia l'afinació i conserva els formants", [fs]
        {
            // Un sol formant estret a 1000 Hz i F0 = 200 Hz (el 5è harmònic és el més fort).
            const double m0 = vv::midiDeHz (200.0);
            const auto x = veuSintetica (corbaNotes ({ { NAN, 0.1 }, { m0, 1.0 }, { NAN, 0.1 } }, fs), fs, { 1000.0 });
            const auto a = vv::analitza (x.data(), (long long) x.size(), fs);
            mibase::VeuPresa v; v.notes = vv::detectaNotes (a); v.analitzada = true;
            comprova (v.notes.size() == 1, "una nota");
            if (v.notes.size() != 1) return;
            v.notes[0].desplac = 3.0;
            const auto y = renderitzaTot (x, fs, a, v);
            const auto b = vv::analitza (y.data(), (long long) y.size(), fs);
            comprova (std::abs (mitjanaF0 (b, 0.35, 0.9) - (m0 + 3.0)) < 0.08, "puja 3 semitons: " + std::to_string (mitjanaF0 (b, 0.35, 0.9) - m0));
            const double f1 = vv::hzDeMidi (m0 + 3.0);   // ~238 Hz: harmònics a 952 i 1189 Hz
            const size_t de = (size_t) (0.4 * fs), n = 8192;
            comprova (dft (y, de, n, 4 * f1, fs) > 1.5 * dft (y, de, n, 5 * f1, fs), "el formant es queda a 1000 Hz (preservat)");
            float maxDif = 0.0f, maxDifX = 0.0f;
            for (size_t i = 1; i < y.size(); ++i) { maxDif = std::max (maxDif, std::abs (y[i] - y[i - 1])); maxDifX = std::max (maxDifX, std::abs (x[i] - x[i - 1])); }
            comprova (maxDif < 2.0f * maxDifX && totFinit (y), "sense clics ni valors estranys");

            v.notes[0].formant = 3.0;
            const auto z = renderitzaTot (x, fs, a, v);
            comprova (dft (z, de, n, 5 * f1, fs) > 1.5 * dft (z, de, n, 4 * f1, fs), "amb el formant a +3, el formant puja");

            v.notes[0].desplac = -4.0; v.notes[0].formant = 0.0;
            const auto w = renderitzaTot (x, fs, a, v);
            const auto c = vv::analitza (w.data(), (long long) w.size(), fs);
            comprova (std::abs (mitjanaF0 (c, 0.35, 0.9) - (m0 - 4.0)) < 0.08, "baixa 4 semitons: " + std::to_string (mitjanaF0 (c, 0.35, 0.9) - m0));
            float pk = 0.0f, pkx = 0.0f;
            for (size_t i = de; i < de + n; ++i) { pk = std::max (pk, std::abs (w[i])); pkx = std::max (pkx, std::abs (x[i])); }
            comprova (pk > 0.6f * pkx && pk < 1.6f * pkx, "el volum es manté en baixar");
        });

        prova ("veu: en moure molt la nota no queda l'original a sota (sense veu doblada)", [fs]
        {
            const double m0 = vv::midiDeHz (200.0);
            const auto x = veuSintetica (corbaNotes ({ { NAN, 0.1 }, { m0, 1.2 }, { NAN, 0.1 } }, fs), fs);
            const auto a = vv::analitza (x.data(), (long long) x.size(), fs);
            mibase::VeuPresa v; v.notes = vv::detectaNotes (a); v.analitzada = true;
            comprova (v.notes.size() == 1, "una nota");
            if (v.notes.size() != 1) return;
            const size_t de = (size_t) (0.4 * fs), n = 16384;
            for (double st : { -8.0, -7.0, -3.0, 4.0, 7.0 })
            {
                v.notes[0].desplac = st;
                const auto y = renderitzaTot (x, fs, a, v);
                const double f1 = vv::hzDeMidi (m0 + st);
                // Energia als harmònics nous i als de l'original (els que no coincideixen amb els nous).
                double nous = 0.0, vells = 0.0;
                for (int h = 1; h <= 8; ++h) nous += dft (y, de, n, h * f1, fs);
                for (int h = 1; h <= 8; ++h)
                {
                    const double fh = h * 200.0;
                    const double dist = std::abs (fh / f1 - std::round (fh / f1)) * f1;
                    if (dist > 25.0) vells += dft (y, de, n, fh, fs);
                }
                comprova (vells < 0.15 * nous, std::to_string ((int) st) + " st: harmònics de l'original " + std::to_string (vells / nous));
            }
        });

        prova ("veu: treure el vibrato i corregir la deriva", [fs]
        {
            std::vector<double> c = corbaNotes ({ { NAN, 0.1 } }, fs);
            for (int i = 0; i < (int) (1.5 * fs); ++i) c.push_back (64.0 + 0.8 * std::sin (2.0 * 3.14159265358979 * 5.5 * i / fs) - 0.6 * i / (1.5 * fs));
            const auto rr = corbaNotes ({ { NAN, 0.1 } }, fs); c.insert (c.end(), rr.begin(), rr.end());
            const auto x = veuSintetica (c, fs);
            const auto a = vv::analitza (x.data(), (long long) x.size(), fs);
            mibase::VeuPresa v; v.notes = vv::detectaNotes (a); v.analitzada = true;
            comprova (v.notes.size() == 1, "una nota");
            if (v.notes.size() != 1) return;
            auto desviacio = [] (const vv::Analisi& b, double de, double fins)
            {
                const double m = mitjanaF0 (b, de, fins); double s = 0; int n = 0;
                for (int i = (int) (de / vv::pas); i < (int) (fins / vv::pas); ++i)
                    if (b.f0[(size_t) i] > 0.0f) { const double d = vv::midiDeHz (b.f0[(size_t) i]) - m; s += d * d; ++n; }
                return n > 0 ? std::sqrt (s / n) : 0.0;
            };
            const double abans = desviacio (a, 0.4, 1.3);
            v.notes[0].modulacio = 0.0;
            v.notes[0].deriva = 0.0;
            const auto y = renderitzaTot (x, fs, a, v);
            const auto b = vv::analitza (y.data(), (long long) y.size(), fs);
            const double despres = desviacio (b, 0.4, 1.3);
            comprova (despres < 0.25 * abans, "la corba queda plana: " + std::to_string (abans) + " → " + std::to_string (despres));
            const auto cb = vv::corbes (a, v);
            comprova (std::abs (mitjanaF0 (b, 0.4, 1.3) - v.notes[0].centre) < 0.1, "al centre de la nota");
            comprova (! cb.editada.empty() && ! std::isnan (cb.editada[(size_t) (0.8 / vv::pas)]), "la corba editada es pot dibuixar");
        });

        prova ("veu: correcció automàtica i escala", []
        {
            mibase::VeuPresa v;
            v.notes = { { 0.0, 0.5, 60.3 }, { 0.5, 1.0, 61.2 }, { 1.0, 1.5, 64.9 } };
            vv::Correccio c; c.centre = 1.0;
            vv::corregeix (v, {}, c);
            comprova (std::abs (vv::centreFinal (v.notes[0]) - 60.0) < 1e-9 && std::abs (vv::centreFinal (v.notes[1]) - 61.0) < 1e-9, "cromàtic al 100 %");
            c.centre = 0.5; vv::corregeix (v, { 0 }, c);
            comprova (std::abs (v.notes[0].desplac + 0.15) < 1e-9 && std::abs (v.notes[1].desplac + 0.2) < 1e-9, "al 50 %, només la triada");
            c.centre = 1.0; c.desti = vv::Desti::Escala; c.tonalitat = { 0, 0 };   // C major
            vv::corregeix (v, {}, c);
            comprova (std::abs (vv::centreFinal (v.notes[1]) - 62.0) < 1e-9, "C# 61,2 → D en C major (la més propera de l'escala)");
            comprova (std::abs (vv::centreFinal (v.notes[2]) - 65.0) < 1e-9, "64,9 → F");
            c.deriva = 0.7; c.vibrato = 0.4; vv::corregeix (v, { 2 }, c);
            comprova (std::abs (v.notes[2].deriva - 0.3) < 1e-9 && std::abs (v.notes[2].modulacio - 0.4) < 1e-9, "deriva i vibrato");
            comprova (vv::teCanvis (v), "té canvis");
            vv::restaura (v, {});
            comprova (! vv::teCanvis (v), "es torna a l'original");
        });

        prova ("veu: tonalitat de la presa i moure per graus", []
        {
            mibase::VeuPresa v;
            const mibase::Tonalitat canco { 0, 0 };   // C major
            comprova (vv::notaObjectiu (61.2, v, canco) == 62, "de la cançó: C# → D");
            comprova (vv::mouGraus (64.1, 1, v, canco) == 65 && vv::mouGraus (64.1, -1, v, canco) == 62, "E +1 grau = F, -1 grau = D");
            comprova (vv::mouGraus (60.0, 2, v, canco) == 64, "C +2 graus = E");
            v.modeTon = 1;
            comprova (vv::notaObjectiu (61.2, v, canco) == 61 && vv::mouGraus (61.0, 1, v, canco) == 62, "cromàtic: semitons");
            v.modeTon = 2; v.to = 9; v.escala = 1;   // A menor natural
            comprova (vv::tonalitatDe (v, canco).to == 9 && vv::notaObjectiu (68.4, v, canco) == 69, "A menor: G# 68,4 → A");
            comprova (vv::mouGraus (69.0, 1, v, canco) == 71, "A +1 grau = B");
        });

        prova ("veu: només es refà el que canvia", [fs]
        {
            const auto x = veuSintetica (corbaNotes ({ { NAN, 0.1 }, { 60.0, 0.5 }, { NAN, 0.3 }, { 64.0, 0.5 }, { NAN, 0.1 } }, fs), fs);
            const auto a = vv::analitza (x.data(), (long long) x.size(), fs);
            mibase::VeuPresa v; v.notes = vv::detectaNotes (a); v.analitzada = true;
            comprova (v.notes.size() == 2, "dues notes");
            if (v.notes.size() != 2) return;
            v.notes[0].desplac = 1.0;
            const auto c1 = vv::corbes (a, v);
            auto y = renderitzaTot (x, fs, a, v);
            v.notes[1].desplac = -1.0;
            const auto c2 = vv::corbes (a, v);
            const auto t = vv::trossosPerRefer (a, c1, c2);
            comprova (t.size() == 1 && t[0].de > (long long) (0.65 * fs), "només el tros de la segona nota");
            const float* in[1] { x.data() }; float* out[1] { y.data() };
            for (auto tr : t) vv::renderitza (in, 1, (long long) x.size(), fs, a, c2, out, tr.de, tr.a);
            comprova (y == renderitzaTot (x, fs, a, v), "el resultat és el mateix que fer-ho tot");
            // Tornar la primera a l'original: es refà i torna a ser idèntica a l'original.
            v.notes[0].desplac = 0.0;
            const auto c3 = vv::corbes (a, v);
            for (auto tr : vv::trossosPerRefer (a, c2, c3)) vv::renderitza (in, 1, (long long) x.size(), fs, a, c3, out, tr.de, tr.a);
            comprova (std::equal (y.begin(), y.begin() + (long) (0.6 * fs), x.begin()), "la primera nota torna a l'original");
        });

        prova ("veu: el pla fa sonar la presa corregida només amb l'insert encès", []
        {
            mibase::Projecte q;
            mibase::Presa s; s.id = "s1"; s.durada = 1.0; q.preses.push_back (s);
            mibase::Pista t; t.id = "t1"; t.tipus = "audio";
            mibase::Clip c; c.presa = "s1"; c.fi = 1.0; t.clips.push_back (c);
            q.pistes.push_back (t);
            auto orig = std::make_shared<mibase::AudioPresa>(); orig->longitud = 100; orig->dades[0].assign (100, 0.1f);
            auto corr = std::make_shared<mibase::AudioPresa>(); corr->longitud = 100; corr->dades[0].assign (100, 0.2f);
            mibase::MapaAudios m { { "s1", orig } }, mc { { "s1", corr } };
            comprova (mibase::construeixPla (q, m, {}, {}, &mc)->pistes[0].clips[0].audio == orig.get(), "sense Vocal Editor, l'original");
            mibase::Insert x; x.id = "i1"; x.plugin = mibase::pluginVeu; x.nom = "Vocal Editor";
            q.pistes[0].inserts.push_back (x);
            int demanats = 0;
            auto pla = mibase::construeixPla (q, m, {}, [&] (const mibase::Insert&) { ++demanats; return std::shared_ptr<mibase::Efecte>(); }, &mc);
            comprova (pla->pistes[0].clips[0].audio == corr.get(), "encès, la corregida");
            comprova (demanats == 0, "l'insert intern no es demana com a plugin");
            q.pistes[0].inserts[0].actiu = false;
            comprova (mibase::construeixPla (q, m, {}, {}, &mc)->pistes[0].clips[0].audio == orig.get(), "apagat, l'original (A/B)");
        });

        prova ("veu: es desa amb el projecte i l'anàlisi al disc", [fs]
        {
            const auto arrel = std::filesystem::temp_directory_path() / "mibase_proves_veu";
            std::filesystem::remove_all (arrel);
            std::filesystem::create_directories (arrel);
            mibase::Projecte q;
            q.nom = "Veu";
            mibase::Presa s; s.id = "s1"; s.fitxer = "Àudios/Veu - presa 01.wav"; s.nom = "Veu"; s.durada = 2.0;
            s.veu.analitzada = true; s.veu.transicio = 0.08; s.veu.modeTon = 2; s.veu.to = 4; s.veu.escala = 1;
            s.veu.notes = { { 0.1, 0.6, 60.12, 0.5, 0.3, 0.2, 1.5, -3.0 } };
            q.preses.push_back (s);
            auto rc = mibase::disc::creaProjecte (arrel, q, "2026-11-07T16:00:00", "07/11/2026 16:00");
            auto l = mibase::disc::llegeix (rc.fitxer);
            comprova (l && l->preses.size() == 1 && l->preses[0].veu == s.veu, "les notes i els canvis es desen i es llegeixen igual");
            // 0.25: el mode d'escala és de cada pista.
            mibase::Pista ta; ta.id = "t1"; ta.modeEscala = 2;
            mibase::Pista tb; tb.id = "t2";
            q.modeEscala = mibase::ModeEscala::Assistit;
            q.pistes = { ta, tb };
            mibase::disc::desa (rc.fitxer, q, "2026-11-07T21:30:00", "2026-11-07 213000", false);
            auto l2 = mibase::disc::llegeix (rc.fitxer);
            comprova (l2 && l2->pistes.size() == 2 && mibase::modeEscalaDe (*l2, l2->pistes[0]) == mibase::ModeEscala::Bloquejat
                      && mibase::modeEscalaDe (*l2, l2->pistes[1]) == mibase::ModeEscala::Assistit, "mode d'escala per pista (i el del projecte per a les velles)");
            const auto x = veuSintetica (corbaNotes ({ { NAN, 0.1 }, { 60.0, 0.4 } }, fs), fs);
            const auto a = vv::analitza (x.data(), (long long) x.size(), fs);
            vv::Analisi b;
            comprova (vv::desaAnalisi (a, arrel / "a.veu") && vv::llegeixAnalisi (arrel / "a.veu", b), "l'anàlisi es desa");
            comprova (b.f0 == a.f0 && b.rms == a.rms && b.fs == a.fs && b.mostres == a.mostres, "i es llegeix igual");
            comprova (! vv::llegeixAnalisi (arrel / "no-hi-es.veu", b), "si no hi és, ho diu");
            std::filesystem::remove_all (arrel);
        });
    }
}
