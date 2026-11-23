#include "mibase/DeteccioAcords.h"
#include "mibase/Edicio.h"
#include "mibase/Elastic.h"
#include "mibase/FFT.h"
#include "mibase/Harmonia.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace mibase::acordsAudio
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        using Croma = std::array<double, 12>;
        using fft::FFT;
        using fft::C;

        struct Plantilla { const char* sufix; std::vector<std::pair<int, double>> notes; double preferencia; };
        const std::vector<Plantilla>& plantilles()
        {
            // Pes de cada nota de l'acord; la «preferència» fa que, en cas de dubte, guanyin les tríades.
            static const std::vector<Plantilla> v {
                { "",     { { 0, 1.0 }, { 4, 0.9 }, { 7, 0.8 } }, 1.00 },
                { "m",    { { 0, 1.0 }, { 3, 0.9 }, { 7, 0.8 } }, 1.00 },
                { "7",    { { 0, 1.0 }, { 4, 0.9 }, { 7, 0.7 }, { 10, 0.7 } }, 0.96 },
                { "m7",   { { 0, 1.0 }, { 3, 0.9 }, { 7, 0.7 }, { 10, 0.7 } }, 0.96 },
                { "maj7", { { 0, 1.0 }, { 4, 0.9 }, { 7, 0.7 }, { 11, 0.7 } }, 0.95 },
                { "dim",  { { 0, 1.0 }, { 3, 0.9 }, { 6, 0.8 } }, 0.93 },
            };
            return v;
        }

        double cosinus (const Croma& x, const Plantilla& pl, int arrel)
        {
            double dot = 0.0, nx = 0.0, nt = 0.0;
            // Cada nota real també fa sonar els seus harmònics: la quinta de dalt (3r) i la tercera major (5è).
            // Si no es tinguessin en compte, un C major (amb el si que fa el mi) semblaria un Cmaj7.
            Croma t {};
            for (auto& [iv, w] : pl.notes)
            {
                const int n0 = arrel + iv;
                t[(size_t) (n0 % 12)] += w;
                t[(size_t) ((n0 + 7) % 12)] += w * 0.33;
                t[(size_t) ((n0 + 4) % 12)] += w * 0.15;
            }
            for (int i = 0; i < 12; ++i) { dot += x[(size_t) i] * t[(size_t) i]; nx += x[(size_t) i] * x[(size_t) i]; nt += t[(size_t) i] * t[(size_t) i]; }
            return nx > 0.0 && nt > 0.0 ? dot / std::sqrt (nx * nt) : 0.0;
        }
    }

    std::vector<AcordTrobat> detecta (const Projecte& p, const Clip& c, const AudioPresa& a)
    {
        std::vector<AcordTrobat> r;
        if (a.longitud <= 0 || a.fs <= 0.0 || c.presa.empty()) return r;
        const double fs = a.fs;

        // ---- Cromagrama per finestres de ~170 ms (8192 mostres a 48 kHz), amb el baix a part ----
        const int n = fs > 60000.0 ? 16384 : 8192, salt = n / 2;
        FFT transformada (n);
        std::vector<C> buf ((size_t) n);
        std::vector<float> fin ((size_t) n);
        for (int i = 0; i < n; ++i) fin[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / n));
        // per a cada bin: la nota (classe de to) i el pes (1 al centre del semitò, 0 a mig camí)
        const int kMin = std::max (1, (int) std::floor (50.0 * n / fs)), kMax = std::min (n / 2 - 1, (int) std::ceil (2500.0 * n / fs));
        std::vector<int> pcBin ((size_t) n / 2, -1);
        std::vector<float> pesBin ((size_t) n / 2, 0.0f);
        std::vector<bool> baixBin ((size_t) n / 2, false);
        for (int k = kMin; k <= kMax; ++k)
        {
            const double f = k * fs / n;
            const double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
            const double dist = std::abs (midi - std::round (midi));
            pcBin[(size_t) k] = (((int) std::lround (midi)) % 12 + 12) % 12;
            pesBin[(size_t) k] = (float) std::max (0.0, 1.0 - 2.0 * dist);
            baixBin[(size_t) k] = f < 200.0;
        }
        struct Finestra { double t = 0.0; Croma croma {}, baix {}; double energia = 0.0; };
        std::vector<Finestra> fins;
        const long long l0 = std::max (0LL, (long long) std::floor (std::max (0.0, c.inici) * fs) - n / 2);
        const long long l1 = std::min (a.longitud, (long long) std::ceil (c.fi * fs) + n / 2);
        for (long long s = l0; s + n <= std::max (l1, l0 + n) && s + n <= a.longitud; s += salt)
        {
            double e = 0.0;
            for (int i = 0; i < n; ++i)
            {
                float v = 0.0f;
                for (int ch = 0; ch < a.canals; ++ch) v += a.dades[ch][(size_t) (s + i)];
                v /= (float) std::max (1, a.canals);
                e += (double) v * v;
                buf[(size_t) i] = { v * fin[(size_t) i], 0.0f };
            }
            transformada.fa (buf.data(), false);
            Finestra f;
            f.t = (double) (s + n / 2) / fs;
            f.energia = e / n;
            for (int k = kMin; k <= kMax; ++k)
            {
                const int pc = pcBin[(size_t) k];
                if (pc < 0) continue;
                const double mag = std::sqrt ((double) buf[(size_t) k].r * buf[(size_t) k].r + (double) buf[(size_t) k].i * buf[(size_t) k].i);
                const double v = std::sqrt (mag) * pesBin[(size_t) k];   // una mica comprimit: que una nota forta no ho tapi tot
                f.croma[(size_t) pc] += v;
                if (baixBin[(size_t) k]) f.baix[(size_t) pc] += v;
            }
            fins.push_back (f);
        }
        if (fins.empty()) return r;
        double energiaMax = 0.0;
        for (auto& f : fins) energiaMax = std::max (energiaMax, f.energia);
        if (energiaMax < 1.0e-10) return r;   // silenci

        // ---- Trossos de mig compàs (seguint el tempo del projecte) dins la part visible del clip ----
        const double t0 = c.iniciVisible(), t1 = c.fiVisible();
        struct Tros { double ini, fi; std::string nom; double puntuacio = 0.0, marge = 0.0; bool silenci = false; };
        std::vector<Tros> trossos;
        const double compas0 = std::floor (edicio::compasDeSegons (p, std::max (0.0, t0)) * 2.0 + 1e-6) / 2.0;
        for (double cp = compas0;; cp += 0.5)
        {
            const double a0 = std::max (t0, edicio::segonsDeCompas (p, cp)), a1 = std::min (t1, edicio::segonsDeCompas (p, cp + 0.5));
            if (a0 >= t1) break;
            if (a1 - a0 < 0.15) continue;   // un bocinet a la vora del clip
            trossos.push_back ({ a0, a1, {} });
            if (trossos.size() > 20000) break;
        }
        const auto ton = harmonia::tonalitatEfectiva (p);
        for (auto& tr : trossos)
        {
            Croma x {}, b {};
            double e = 0.0;
            int quantes = 0;
            for (auto& f : fins)
            {
                const double tl = c.pos + elastic::aClip (c, f.t);
                if (tl < tr.ini || tl >= tr.fi) continue;
                for (int i = 0; i < 12; ++i) { x[(size_t) i] += f.croma[(size_t) i]; b[(size_t) i] += f.baix[(size_t) i]; }
                e += f.energia;
                ++quantes;
            }
            if (quantes == 0 || e / quantes < energiaMax * 1.0e-4) { tr.silenci = true; continue; }
            // Que la nota més fluixa no compti (soroll de fons, bateria): es resta el mínim.
            const double mn = *std::min_element (x.begin(), x.end());
            for (auto& v : x) v -= mn;
            const double bMax = *std::max_element (b.begin(), b.end());
            // La millor plantilla de cada fonamental; després, la millor fonamental i la segona (per saber si és clar).
            std::array<double, 12> perArrel;
            std::array<size_t, 12> quina {};
            perArrel.fill (-1.0);
            for (size_t pl = 0; pl < plantilles().size(); ++pl)
                for (int arrel = 0; arrel < 12; ++arrel)
                {
                    double s = cosinus (x, plantilles()[pl], arrel) * plantilles()[pl].preferencia;
                    if (bMax > 0.0) s += 0.05 * b[(size_t) arrel] / bMax;   // el baix sol tocar la fonamental
                    if (s > perArrel[(size_t) arrel]) { perArrel[(size_t) arrel] = s; quina[(size_t) arrel] = pl; }
                }
            const int arrelMillor = (int) (std::max_element (perArrel.begin(), perArrel.end()) - perArrel.begin());
            const double millor = perArrel[(size_t) arrelMillor];
            double segon = 0.0;
            for (int i = 0; i < 12; ++i) if (i != arrelMillor) segon = std::max (segon, perArrel[(size_t) i]);
            tr.nom = harmonia::nomTo (arrelMillor, ton) + plantilles()[quina[(size_t) arrelMillor]].sufix;
            // Inversions: si el baix toca clarament una altra nota de l'acord, «C/E».
            if (bMax > 0.0)
            {
                const int pcBaix = (int) (std::max_element (b.begin(), b.end()) - b.begin());
                double segonBaix = 0.0;
                for (int i = 0; i < 12; ++i) if (i != pcBaix) segonBaix = std::max (segonBaix, b[(size_t) i]);
                bool delAcord = false;
                for (auto& nt : plantilles()[quina[(size_t) arrelMillor]].notes) delAcord = delAcord || (arrelMillor + nt.first) % 12 == pcBaix;
                if (pcBaix != arrelMillor && delAcord && segonBaix < bMax * 0.6)
                    tr.nom += "/" + harmonia::nomTo (pcBaix, ton);
            }
            tr.puntuacio = millor;
            tr.marge = millor - segon;
        }

        // ---- Neteja: un mig compàs dubtós entre dos d'iguals pren el seu acord; després s'ajunten els iguals ----
        auto dubtos = [] (const Tros& t) { return t.puntuacio < 0.80 || t.marge < 0.03; };
        for (size_t i = 1; i + 1 < trossos.size(); ++i)
            if (! trossos[i].silenci && dubtos (trossos[i]) && ! trossos[i - 1].silenci && trossos[i - 1].nom == trossos[i + 1].nom
                && trossos[i].nom != trossos[i - 1].nom)
            {
                trossos[i].nom = trossos[i - 1].nom;
                trossos[i].puntuacio = std::min (trossos[i - 1].puntuacio, trossos[i + 1].puntuacio);
                trossos[i].marge = std::min (trossos[i - 1].marge, trossos[i + 1].marge);
            }
        for (auto& tr : trossos)
        {
            if (tr.silenci || tr.nom.empty()) continue;
            const float seg = (float) std::clamp ((tr.puntuacio - 0.6) / 0.35 * 0.6 + tr.marge * 4.0, 0.0, 1.0);
            if (! r.empty() && r.back().nom == tr.nom && std::abs (r.back().fi - tr.ini) < 1e-6)
            {
                r.back().fi = tr.fi;
                r.back().seguretat = std::max (r.back().seguretat, seg);
                r.back().dubtos = r.back().dubtos && dubtos (tr);
                continue;
            }
            r.push_back ({ tr.ini, tr.fi, tr.nom, seg, dubtos (tr) });
        }
        return r;
    }
}
