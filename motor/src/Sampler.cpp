#include "mibase/Sampler.h"
#include "mibase/Veu.h"

#include <algorithm>
#include <cmath>

namespace mibase::sampler
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        float dbAGuany (double db) noexcept { return (float) std::pow (10.0, db / 20.0); }
    }

    double Preparada::aMostra (double segons) const noexcept
    {
        const double s = girada ? duradaOriginal - segons : segons;
        return s * factorTemps * m.fs;
    }

    std::string signatura (const ParamsSampler& p)
    {
        const double sp = p.mantenTo ? p.speed : 1.0;
        return p.fitxer + "|" + (p.reversa ? "r" : "-") + (p.smono ? "m" : "-") + "|" + std::to_string (std::lround (sp * 1000.0));
    }

    //==========================================================================
    // Time-stretch WSOLA: finestres de Hann de 40 ms, salt de sortida N/2; cada finestra es busca (± N/4) on s'assembla
    // més a la continuació natural de l'anterior. Els desplaçaments es decideixen amb un senyal guia (mono) i s'apliquen
    // igual a tots els canals, perquè l'estèreo no es desfaci.
    namespace
    {
        std::vector<long long> posicionsWsola (const std::vector<float>& g, double factor, int N, int tol, long long outLen)
        {
            const int Hs = N / 2;
            const double Ha = Hs / factor;
            const long long len = (long long) g.size();
            auto mostra = [&] (long long i) -> float { return i >= 0 && i < len ? g[(size_t) i] : 0.0f; };
            auto correlacio = [&] (long long a, long long b, int pas)
            {
                double s = 0.0;
                for (int i = 0; i < N; i += pas) s += (double) mostra (a + i) * mostra (b + i);
                return s;
            };
            std::vector<long long> pos;
            const long long frames = (outLen + N) / Hs + 1;
            pos.reserve ((size_t) frames);
            pos.push_back (0);
            for (long long k = 1; k < frames; ++k)
            {
                const long long nat = pos.back() + Hs;
                const long long objectiu = (long long) std::llround (k * Ha);
                long long millor = objectiu;
                double maxim = -1e300;
                const long long d0 = std::max<long long> (-tol, -objectiu);
                for (long long d = d0; d <= tol; d += 4)
                {
                    const double c = correlacio (objectiu + d, nat, 4);
                    if (c > maxim) { maxim = c; millor = objectiu + d; }
                }
                const long long grossa = millor;
                maxim = -1e300;
                for (long long d = -3; d <= 3; ++d)
                {
                    const long long c0 = grossa + d;
                    if (c0 < 0) continue;
                    const double c = correlacio (c0, nat, 2);
                    if (c > maxim) { maxim = c; millor = c0; }
                }
                pos.push_back (millor);
            }
            return pos;
        }

        std::vector<float> aplicaWsola (const std::vector<float>& xp, const std::vector<long long>& pos, int N, long long outLen)
        {
            const int Hs = N / 2, P = N / 2;
            std::vector<float> w ((size_t) N);
            for (int i = 0; i < N; ++i) w[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / N));
            std::vector<float> out ((size_t) (outLen + N + Hs), 0.0f);
            const long long len = (long long) xp.size();
            for (size_t k = 0; k < pos.size(); ++k)
            {
                const long long o = (long long) k * Hs;
                if (o >= (long long) out.size()) break;
                for (int i = 0; i < N && o + i < (long long) out.size(); ++i)
                {
                    const long long j = pos[k] + i;
                    if (j >= 0 && j < len) out[(size_t) (o + i)] += xp[(size_t) j] * w[(size_t) i];
                }
            }
            return std::vector<float> (out.begin() + P, out.begin() + P + outLen);
        }

        std::vector<float> ambMarge (const std::vector<float>& x, int P)
        {
            std::vector<float> xp ((size_t) P, 0.0f);
            xp.insert (xp.end(), x.begin(), x.end());
            return xp;
        }

        int midaFinestra (double fs) { return std::max (256, (int) std::lround (0.04 * fs) / 2 * 2); }
    }

    std::vector<float> estira (const std::vector<float>& x, double factor, double fs, const std::vector<float>* guia)
    {
        if (x.empty() || std::abs (factor - 1.0) < 1e-4) return x;
        const int N = midaFinestra (fs);
        const long long outLen = (long long) std::llround ((double) x.size() * factor);
        const auto xp = ambMarge (x, N / 2);
        const auto gp = guia != nullptr ? ambMarge (*guia, N / 2) : xp;
        const auto pos = posicionsWsola (gp, factor, N, N / 4, outLen);
        return aplicaWsola (xp, pos, N, outLen);
    }

    void estiraEstereo (const std::vector<float>& L, const std::vector<float>& R, double factor, double fs,
                        std::vector<float>& outL, std::vector<float>& outR)
    {
        if (R.empty()) { outL = estira (L, factor, fs); outR.clear(); return; }
        std::vector<float> g (L.size());
        for (size_t i = 0; i < L.size(); ++i) g[i] = 0.5f * (L[i] + (i < R.size() ? R[i] : 0.0f));
        outL = estira (L, factor, fs, &g);
        outR = estira (R, factor, fs, &g);
    }

    //==========================================================================
    std::shared_ptr<const Preparada> prepara (std::shared_ptr<const Mostra> original, const ParamsSampler& p)
    {
        auto r = std::make_shared<Preparada>();
        if (original == nullptr || original->L.empty()) { r->m.fs = 48000.0; r->m.L.assign (16, 0.0f); r->duradaOriginal = 0.0; return r; }
        r->m.fs = original->fs;
        r->m.L = original->L;
        r->m.R = original->R;
        r->duradaOriginal = (double) original->L.size() / original->fs;
        if (p.smono && ! r->m.R.empty())
        {
            for (size_t i = 0; i < r->m.L.size(); ++i) r->m.L[i] = 0.5f * (r->m.L[i] + (i < r->m.R.size() ? r->m.R[i] : 0.0f));
            r->m.R.clear();
        }
        if (p.reversa)
        {
            std::reverse (r->m.L.begin(), r->m.L.end());
            std::reverse (r->m.R.begin(), r->m.R.end());
            r->girada = true;
        }
        if (p.mantenTo && std::abs (p.speed - 1.0) > 1e-3)
        {
            const double factor = 1.0 / std::clamp (p.speed, 0.25, 4.0);
            std::vector<float> L, R;
            estiraEstereo (r->m.L, r->m.R, factor, r->m.fs, L, R);
            r->factorTemps = (double) L.size() / (double) r->m.L.size();
            r->m.L = std::move (L);
            r->m.R = std::move (R);
        }
        const size_t n = r->m.L.size();
        r->pics.assign (n / Preparada::midaPic + 1, 0.0f);
        for (size_t i = 0; i < n; ++i)
        {
            float v = std::abs (r->m.L[i]);
            if (i < r->m.R.size()) v = std::max (v, std::abs (r->m.R[i]));
            auto& pc = r->pics[i / Preparada::midaPic];
            pc = std::max (pc, v);
        }
        return r;
    }

    double zeroCrossing (const Mostra& m, double segons)
    {
        const long long n = (long long) m.L.size();
        if (n < 2) return segons;
        auto v = [&] (long long i) { return m.L[(size_t) i] + (i < (long long) m.R.size() ? m.R[(size_t) i] : 0.0f); };
        const long long c = std::clamp ((long long) std::llround (segons * m.fs), 0LL, n - 1);
        const long long marge = (long long) (0.01 * m.fs);
        for (long long d = 0; d <= marge; ++d)
            for (long long i : { c - d, c + d })
                if (i >= 1 && i < n)
                {
                    const float a = v (i - 1), b = v (i);
                    if ((a <= 0.0f && b >= 0.0f) || (a >= 0.0f && b <= 0.0f))
                        return (double) (std::abs (a) <= std::abs (b) ? i - 1 : i) / m.fs;
                }
        return segons;
    }

    int detectaRoot (const Mostra& m)
    {
        const long long n = (long long) m.L.size();
        if (n < (long long) (0.05 * m.fs)) return -1;
        const long long llarg = std::min (n, (long long) (3.0 * m.fs));
        const long long ini = (n - llarg) / 2;
        std::vector<float> x ((size_t) llarg);
        for (long long i = 0; i < llarg; ++i)
            x[(size_t) i] = m.L[(size_t) (ini + i)] * (m.R.empty() ? 1.0f : 0.5f) + (m.R.empty() ? 0.0f : 0.5f * m.R[(size_t) (ini + i)]);
        const auto an = veu::analitza (x.data(), llarg, m.fs);
        std::vector<double> notes;
        for (int i = 0; i < an.punts(); ++i)
            if (an.f0[(size_t) i] > 0.0f && an.prob[(size_t) i] > 0.5f) notes.push_back (veu::midiDeHz (an.f0[(size_t) i]));
        if (notes.size() < 5) return -1;
        std::nth_element (notes.begin(), notes.begin() + (long) notes.size() / 2, notes.end());
        return std::clamp ((int) std::lround (notes[notes.size() / 2]), 0, 127);
    }

    //==========================================================================
    Samplejador::Samplejador (std::shared_ptr<const Preparada> m) : mostra (std::move (m))
    {
        if (mostra == nullptr) mostra = sampler::prepara (nullptr, ParamsSampler {});
        posaParams ({});
    }

    void Samplejador::posaParams (const ParamsSampler& p) noexcept
    {
        const auto& P = *mostra;
        const double dur = P.duradaOriginal;
        const double fiS = p.fi < 0.0 ? dur : std::min (p.fi, dur);
        const double iniS = std::clamp (p.inici, 0.0, std::max (0.0, fiS));
        double i0 = P.aMostra (iniS), i1 = P.aMostra (fiS);
        if (i0 > i1) std::swap (i0, i1);
        const double maxim = (double) P.m.L.size() - 1.0;
        i0 = std::clamp (i0, 0.0, maxim);
        i1 = std::clamp (i1, i0, maxim);
        double b0 = -1.0, b1 = -1.0;
        if (p.bucleDe >= 0.0 && p.bucleA > p.bucleDe)
        {
            b0 = P.aMostra (p.bucleDe);
            b1 = P.aMostra (p.bucleA);
            if (b0 > b1) std::swap (b0, b1);
            b0 = std::clamp (b0, i0, i1);
            b1 = std::clamp (b1, i0, i1);
            if (b1 - b0 < 8.0) b0 = b1 = -1.0;
        }
        a.inici = i0; a.fi = i1; a.bucleDe = b0; a.bucleA = b1;
        a.crossfade = std::max (0.0, p.crossfade) * P.m.fs * P.factorTemps;
        a.fadeIn = std::max (0.0, p.fadeIn) * P.m.fs * P.factorTemps;
        a.fadeOut = std::max (0.002, p.fadeOut) * P.m.fs * P.factorTemps;
        a.modeBucle = std::clamp (p.modeBucle, 0, 3);
        a.modeToc = std::clamp (p.modeToc, 0, 2);
        a.veus = std::clamp (p.veus, 0, 2);
        a.maxVeus = std::clamp (p.maxVeus, 1, 32);
        a.root = std::clamp (p.root, 0, 127);
        a.filtre = std::clamp (p.filtre, 0, 4);
        a.pendent = p.pendent == 24 ? 24 : 12;
        float flat = 1.0f;
        if (p.flat)
        {
            float pic = 0.0f;
            const size_t c0 = (size_t) (i0 / Preparada::midaPic), c1 = std::min (P.pics.size() - 1, (size_t) (i1 / Preparada::midaPic));
            for (size_t c = c0; c <= c1 && c < P.pics.size(); ++c) pic = std::max (pic, P.pics[c]);
            if (pic > 1e-5f) flat = 0.966f / pic;
        }
        a.guany = dbAGuany (p.guanyDb + p.volDb) * flat;
        a.pan = (float) std::clamp (p.pan, -1.0, 1.0);
        a.width = (float) std::clamp (p.width, 0.0, 2.0);
        a.tune = (float) (p.tune + p.fine / 100.0);
        a.cutoff = (float) std::clamp (p.cutoff, 20.0, 20000.0);
        a.reso = (float) std::clamp (p.reso, 0.0, 1.0);
        a.drive = (float) std::clamp (p.drive, 0.0, 1.0);
        a.atac = (float) std::max (0.0, p.atac);
        a.decai = (float) std::max (0.0, p.decai);
        a.sost = (float) std::clamp (p.sosteniment, 0.0, 1.0);
        a.allib = (float) std::max (0.0, p.alliberament);
        a.vel = (float) std::clamp (p.vel, 0.0, 1.0);
        a.ritme = (float) (p.mantenTo ? 1.0 : std::clamp (p.speed, 0.25, 4.0));
        a.durFixa = p.durFixa;
    }

    void Samplejador::prepara (double f, int)
    {
        fs = f > 0.0 ? f : 48000.0;
        const int G = std::max (64, (int) std::lround (0.05 * fs) / 2 * 2);
        finestra.resize ((size_t) G);
        for (int i = 0; i < G; ++i) finestra[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / G));
    }

    double Samplejador::pasDe (int nota) const noexcept
    {
        const double semis = nota - a.root.load() + a.tune.load() + bend;
        return std::pow (2.0, semis / 12.0) * a.ritme.load() * mostra->m.fs / fs;
    }

    Samplejador::Veu* Samplejador::veuLliure() noexcept
    {
        for (auto& v : veus) if (! v.actiu()) return &v;
        Veu* r = nullptr;   // cap de lliure: la que s'està morint fa més temps, o la més vella
        for (auto& v : veus) if (v.env == Env::Mort && (r == nullptr || v.ordre < r->ordre)) r = &v;
        if (r == nullptr) for (auto& v : veus) if (r == nullptr || v.ordre < r->ordre) r = &v;
        if (r != nullptr && r->actiu()) --actives;
        if (r != nullptr) r->env = Env::Res;
        return r;
    }

    void Samplejador::mata (Veu& v) noexcept
    {
        if (v.env == Env::Res || v.env == Env::Mort) return;
        v.env = Env::Mort;
        v.mort = 1.0f;
    }

    void Samplejador::allibera (Veu& v) noexcept
    {
        v.premuda = false;
        v.pedal = false;
        const int toc = a.modeToc.load();
        if (toc == 1) { v.alliberada = true; return; }        // one-shot: sona fins al final
        if (toc == 2) { mata (v); return; }                   // gate: s'acaba ara
        v.alliberada = true;
        if (a.modeBucle.load() == 3 && a.bucleA.load() > 0.0) return;   // el release arriba quan s'acaba la cua (renderitza)
        if (v.env != Env::Mort && v.env != Env::Res) v.env = Env::Allib;
    }

    void Samplejador::notaOn (int nota, int velo) noexcept
    {
        const float s = a.vel.load();
        const float gv = (1.0f - s) + s * (float) std::pow (velo / 127.0, 1.6);
        const int modeVeus = a.veus.load();
        // la llista de premudes (Mono / Legato)
        for (int i = 0; i < numPremudes; ++i)
            if (premudes[(size_t) i] == nota) { for (int j = i; j < numPremudes - 1; ++j) premudes[(size_t) j] = premudes[(size_t) j + 1]; --numPremudes; break; }
        if (numPremudes == (int) premudes.size()) { for (int j = 0; j < numPremudes - 1; ++j) premudes[(size_t) j] = premudes[(size_t) j + 1]; --numPremudes; }
        premudes[(size_t) numPremudes++] = nota;

        if (modeVeus == 2)
            for (auto& v : veus)
                if (v.actiu() && v.env != Env::Mort && v.premuda)
                {
                    v.nota = nota; v.pas = pasDe (nota); v.guanyVel = gv;   // legato: canvia de nota sense tornar a començar
                    return;
                }
        if (modeVeus >= 1)
            for (auto& v : veus) mata (v);
        else
        {
            for (auto& v : veus) if (v.actiu() && v.nota == nota && v.premuda) allibera (v);
            int vives = 0;
            for (auto& v : veus) if (v.actiu() && v.env != Env::Mort) ++vives;
            while (vives >= a.maxVeus.load())
            {
                Veu* vella = nullptr;   // roba la més vella (primer les ja alliberades)
                for (auto& v : veus)
                    if (v.actiu() && v.env != Env::Mort)
                        if (vella == nullptr || (v.alliberada && ! vella->alliberada) || (v.alliberada == vella->alliberada && v.ordre < vella->ordre)) vella = &v;
                if (vella == nullptr) break;
                mata (*vella);
                --vives;
            }
        }
        Veu* v = veuLliure();
        if (v == nullptr) return;
        *v = Veu {};
        v->nota = nota;
        v->premuda = true;
        v->pos = a.inici.load();
        v->pas = pasDe (nota);
        v->guanyVel = gv;
        v->ordre = ++comptador;
        v->env = a.atac.load() > 0.0005f ? Env::Atac : Env::Decai;
        v->nivell = v->env == Env::Atac ? 0.0f : 1.0f;
        ++actives;
    }

    void Samplejador::notaOff (int nota) noexcept
    {
        for (int i = 0; i < numPremudes; ++i)
            if (premudes[(size_t) i] == nota) { for (int j = i; j < numPremudes - 1; ++j) premudes[(size_t) j] = premudes[(size_t) j + 1]; --numPremudes; break; }
        const int modeVeus = a.veus.load();
        for (auto& v : veus)
        {
            if (! v.actiu() || v.nota != nota || ! v.premuda || v.audicio) continue;
            if (modeVeus >= 1 && numPremudes > 0 && ! pedal)
            {
                const int anterior = premudes[(size_t) numPremudes - 1];
                if (modeVeus == 2) { v.nota = anterior; v.pas = pasDe (anterior); }
                else
                {
                    const float gv = v.guanyVel;
                    mata (v);
                    notaOn (anterior, 100);
                    for (auto& w : veus) if (w.actiu() && w.nota == anterior && w.premuda) w.guanyVel = gv;   // notaOn la deixa al final
                }
                continue;
            }
            if (pedal) v.pedal = true;
            else allibera (v);
        }
    }

    void Samplejador::apagaTot() noexcept
    {
        for (auto& v : veus) mata (v);
        numPremudes = 0;
        pedal = false;
    }

    float Samplejador::llegeix (const float* x, double pos) const noexcept
    {
        const long long n = (long long) mostra->m.L.size();
        const long long i = (long long) std::floor (pos);
        const float t = (float) (pos - (double) i);
        auto m = [&] (long long k) { return k >= 0 && k < n ? x[k] : 0.0f; };
        const float y0 = m (i - 1), y1 = m (i), y2 = m (i + 1), y3 = m (i + 2);
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    void Samplejador::renderitza (Veu& v, float* L, float* R, int ini, int fi) noexcept
    {
        const auto& M = mostra->m;
        const float* xL = M.L.data();
        const float* xR = M.R.empty() ? xL : M.R.data();
        const double i0 = a.inici.load(), i1 = a.fi.load();
        const double b0 = a.bucleDe.load(), b1 = a.bucleA.load();
        const int toc = a.modeToc.load();
        const int modeB = toc == 1 || v.audicio ? 0 : a.modeBucle.load();
        const bool hiHaBucle = modeB > 0 && b0 >= 0.0 && b1 > b0;
        const double llarg = b1 - b0;
        const double xf = hiHaBucle ? std::min ({ a.crossfade.load(), llarg * 0.5, b0 - i0 }) : 0.0;
        const double fIn = a.fadeIn.load(), fOut = a.fadeOut.load();
        const float atac = a.atac.load(), decai = a.decai.load(), sost = a.sost.load(), allib = a.allib.load();
        const float pasAtac = atac > 0.0005f ? 1.0f / (atac * (float) fs) : 1.0f;
        const float pasDecai = decai > 0.0005f ? (1.0f - sost) / (decai * (float) fs) : 1.0f;
        const float pasMort = 1.0f / (0.005f * (float) fs);
        // filtre
        const int tipusF = a.filtre.load();
        const int etapes = a.pendent.load() == 24 ? 2 : 1;
        const double fc = std::min ((double) a.cutoff.load(), fs * 0.45);
        const float g = (float) std::tan (pi * fc / fs);
        const float k = 2.0f - 1.96f * a.reso.load();
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float drive = a.drive.load();
        const float dg = 1.0f + 8.0f * drive, dn = 1.0f / (1.0f + 2.0f * drive);
        // durada fixa
        const double ritme0 = a.ritme.load() * M.fs / fs;
        const bool durFixa = a.durFixa.load() && ! v.audicio && ! finestra.empty();
        const int G = (int) finestra.size(), H = G / 2;

        for (int s = ini; s < fi; ++s)
        {
            if (! v.actiu()) return;
            // continuar amb el bucle?
            const bool bucleViu = hiHaBucle && (modeB != 3 || ! v.alliberada);
            // release diferit del mode 3: quan ja som a la cua i queda menys temps que el release
            if (modeB == 3 && v.alliberada && ! bucleViu && (v.env == Env::Atac || v.env == Env::Decai || v.env == Env::Sost))
            {
                const double queda = (i1 - v.pos) / std::max (1e-9, durFixa ? ritme0 : v.pas);
                if (queda <= allib * fs) v.env = Env::Allib;
            }
            // so
            const bool gran = durFixa && std::abs (v.pas - ritme0) > ritme0 * 1e-3;
            const double avanc = gran ? ritme0 : v.pas;
            float sl = 0.0f, sr = 0.0f;
            if (gran)
            {
                // un punt del sample per a un gra: amb el bucle (i el crossfade) aplicat, i res més enllà del final
                auto lectura = [&] (const float* x, double q) -> float
                {
                    if (bucleViu && modeB != 2 && q >= b1) q = b0 + std::fmod (q - b0, llarg);
                    if (bucleViu && modeB != 2 && xf > 1.0 && q >= b1 - xf && q < b1)
                    {
                        const double t = (q - (b1 - xf)) / xf;
                        return llegeix (x, q) * (float) std::cos (t * pi * 0.5) + llegeix (x, q - llarg) * (float) std::sin (t * pi * 0.5);
                    }
                    const float fiG = (float) std::clamp ((i1 - q) / fOut, 0.0, 1.0);
                    return fiG > 0.0f ? llegeix (x, q) * fiG : 0.0f;
                };
                if (! v.gran)
                {
                    v.gran = true;
                    v.gViu[0] = true; v.gPla[0] = true; v.gJ[0] = 0; v.gSrc[0] = v.pos;
                    v.gViu[1] = false;
                    v.gCompte = 0;
                }
                for (int g = 0; g < 2; ++g)
                {
                    if (! v.gViu[g]) continue;
                    const int j = v.gJ[g];
                    const float w = v.gPla[g] && j < H ? 1.0f : finestra[(size_t) j];
                    const double q = v.gSrc[g] + j * v.pas;
                    const float l = lectura (xL, q);
                    sl += w * l;
                    sr += w * (xR == xL ? l : lectura (xR, q));
                    if (++v.gJ[g] >= G) v.gViu[g] = false;
                }
                if (++v.gCompte >= H)
                {
                    const int g = ! v.gViu[0] ? 0 : ! v.gViu[1] ? 1 : (v.gJ[0] > v.gJ[1] ? 0 : 1);
                    const int vell = 1 - g;
                    double inici = v.pos;
                    if (v.gViu[vell])
                    {
                        // el gra nou comença on més s'assembla al que el vell sonaria ara (així no hi ha salts de fase)
                        const long long n = (long long) M.L.size();
                        auto mostraR = [&] (double q) -> float
                        {
                            if (bucleViu && modeB != 2 && q >= b1) q = b0 + std::fmod (q - b0, llarg);
                            const long long k = (long long) q;
                            return k >= 0 && k < n ? xL[k] : 0.0f;
                        };
                        constexpr int K = 256;
                        float ref[K / 2];
                        for (int k = 0; k < K / 2; ++k) ref[k] = mostraR (v.gSrc[vell] + (v.gJ[vell] + 2 * k) * v.pas);
                        const int marge = (int) (0.006 * M.fs);
                        double millor = -1e30;
                        auto prova = [&] (int d)
                        {
                            double c = 0.0;
                            for (int k = 0; k < K / 2; ++k) c += (double) ref[k] * mostraR (v.pos + d + 2 * k * v.pas);
                            if (c > millor) { millor = c; inici = v.pos + d; }
                        };
                        for (int d = -marge; d <= marge; d += 3) prova (d);   // gruixut i, després, fi
                        const int centre = (int) std::lround (inici - v.pos);
                        prova (centre - 1);
                        prova (centre + 1);
                    }
                    v.gViu[g] = true; v.gPla[g] = false; v.gJ[g] = 0; v.gSrc[g] = inici;
                    v.gCompte = 0;
                }
            }
            else
            {
                v.gran = false;
                sl = llegeix (xL, v.pos);
                sr = xR == xL ? sl : llegeix (xR, v.pos);
                if (bucleViu && modeB != 2 && v.dir > 0 && xf > 1.0 && v.pos >= b1 - xf && v.pos < b1)
                {
                    const double t = (v.pos - (b1 - xf)) / xf;
                    const float ga = (float) std::cos (t * pi * 0.5), gb = (float) std::sin (t * pi * 0.5);
                    sl = sl * ga + llegeix (xL, v.pos - llarg) * gb;
                    sr = (xR == xL ? sl : sr * ga + llegeix (xR, v.pos - llarg) * gb);
                }
            }
            float fade = 1.0f;
            if (! v.dinsBucle && fIn > 1.0 && v.pos - i0 < fIn) fade = (float) std::max (0.0, (v.pos - i0) / fIn);
            if (i1 - v.pos < fOut) fade *= (float) std::max (0.0, (i1 - v.pos) / fOut);
            // envolupant
            switch (v.env)
            {
                case Env::Atac:  v.nivell += pasAtac; if (v.nivell >= 1.0f) { v.nivell = 1.0f; v.env = Env::Decai; } break;
                case Env::Decai: v.nivell -= pasDecai; if (v.nivell <= sost) { v.nivell = sost; v.env = Env::Sost; } break;
                case Env::Sost:  if (! v.audicio) v.nivell += (sost - v.nivell) * 0.001f; break;
                case Env::Allib:
                {
                    const float pas = allib > 0.0005f ? 1.0f / (allib * (float) fs) : 1.0f;
                    v.nivell -= pas;
                    if (v.nivell <= 0.0f) { v.nivell = 0.0f; v.env = Env::Res; --actives; return; }
                    break;
                }
                case Env::Mort:
                    v.mort -= pasMort;
                    if (v.mort <= 0.0f) { v.env = Env::Res; --actives; return; }
                    break;
                case Env::Res: return;
            }
            float gain = v.nivell * v.guanyVel * fade * (v.env == Env::Mort ? v.mort : 1.0f);
            float x[2] = { sl * gain, sr * gain };
            if (drive > 0.001f) for (auto& c : x) c = std::tanh (c * dg) * dn;
            if (tipusF > 0)
                for (int c = 0; c < 2; ++c)
                    for (int e = 0; e < etapes; ++e)
                    {
                        float& ic1 = e == 0 ? v.s1[c][0] : v.s2[c][0];
                        float& ic2 = e == 0 ? v.s1[c][1] : v.s2[c][1];
                        const float in = x[c];
                        const float v3 = in - ic2;
                        const float v1 = a1 * ic1 + a2 * v3;
                        const float v2 = ic2 + a2 * ic1 + a3 * v3;
                        ic1 = 2.0f * v1 - ic1;
                        ic2 = 2.0f * v2 - ic2;
                        const float hp = in - k * v1 - v2;
                        x[c] = tipusF == 1 ? v2 : tipusF == 2 ? hp : tipusF == 3 ? v1 : v2 + hp;
                    }
            L[s] += x[0];
            R[s] += x[1];
            // avançar
            v.pos += avanc * v.dir;
            if (bucleViu)
            {
                if (v.dir > 0 && v.pos >= b1)
                {
                    v.dinsBucle = true;
                    if (modeB == 2) { v.pos = b1 - (v.pos - b1); v.dir = -1; }
                    else v.pos -= llarg;
                }
                else if (v.dir < 0 && v.pos <= b0) { v.pos = b0 + (b0 - v.pos); v.dir = 1; }
            }
            else if (v.dir < 0) v.dir = 1;   // ping-pong alliberat (mode 3 no ho és mai): torna endavant
            if (v.pos >= i1) { v.env = Env::Res; --actives; return; }
        }
    }

    void Samplejador::processa (const EventInstrument* ev, int num, float* L, float* R, int n) noexcept
    {
        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        if (const double pet = peticio.exchange (-1.0, std::memory_order_relaxed); pet != -1.0)
        {
            for (auto& v : veus) if (v.actiu() && v.audicio) mata (v);
            if (pet >= 0.0)
                if (Veu* v = veuLliure())
                {
                    *v = Veu {};
                    v->audicio = true;
                    v->nota = a.root.load();
                    v->pas = pasDe (v->nota);
                    v->pos = std::clamp (mostra->aMostra (pet), a.inici.load(), a.fi.load());
                    v->ordre = ++comptador;
                    v->env = Env::Sost;
                    v->nivell = 1.0f;
                    v->dinsBucle = true;   // sense fade d'entrada
                    ++actives;
                }
        }
        int cur = 0, e = 0;
        while (cur < n)
        {
            const int fins = e < num ? std::clamp (ev[e].offset, cur, n) : n;
            if (fins > cur && actives > 0)
                for (auto& v : veus) if (v.actiu()) renderitza (v, L, R, cur, fins);
            cur = fins;
            while (e < num && std::clamp (ev[e].offset, 0, n) <= cur)
            {
                const auto& m = ev[e++];
                const int tipus = m.estat & 0xF0;
                if (tipus == 0x90 && m.dada2 > 0) notaOn (m.dada1, m.dada2);
                else if (tipus == 0x80 || (tipus == 0x90 && m.dada2 == 0)) notaOff (m.dada1);
                else if (tipus == 0xB0)
                {
                    if (m.dada1 == 64)
                    {
                        const bool ara = m.dada2 >= 64;
                        if (pedal && ! ara) for (auto& v : veus) if (v.actiu() && v.pedal) allibera (v);
                        pedal = ara;
                    }
                    else if (m.dada1 == 120 || m.dada1 == 123) apagaTot();
                }
                else if (tipus == 0xE0)
                {
                    bend = (float) ((((int) m.dada2 << 7) | m.dada1) - 8192) / 8192.0f * 2.0f;
                    for (auto& v : veus) if (v.actiu()) v.pas = pasDe (v.nota);
                }
            }
            if (e >= num && cur >= n) break;
        }
        // volum, amplada i panorama (balanç, com a les pistes)
        const float gn = a.guany.load(), w = a.width.load(), p = a.pan.load();
        const float gl = gn * (p > 0.0f ? 1.0f - p : 1.0f), gr = gn * (p < 0.0f ? 1.0f + p : 1.0f);
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]), sd = 0.5f * (L[i] - R[i]) * w;
            L[i] = (m + sd) * gl;
            R[i] = (m - sd) * gr;
        }
        double vis = -1.0;
        std::uint64_t ord = 0;
        for (auto& v : veus) if (v.actiu() && v.env != Env::Mort && v.ordre > ord) { ord = v.ordre; vis = v.pos; }
        posVisible.store (vis, std::memory_order_relaxed);
        if (actives < 0) actives = 0;
    }
}
