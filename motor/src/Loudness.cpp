#include "mibase/Loudness.h"
#include <algorithm>
#include <cmath>

namespace mibase::loudness
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        inline float aLufs (double e) { return e > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (e)) : silenci; }
        inline float aDb (double v) { return v > 1.0e-6 ? (float) (20.0 * std::log10 (v)) : silenci; }
        void maxAtom (std::atomic<float>& a, float v) { if (v > a.load (std::memory_order_relaxed)) a.store (v, std::memory_order_relaxed); }
    }

    const std::vector<Objectiu>& objectius()
    {
        static const std::vector<Objectiu> v {
            { "Cap (només mesura)", 0.0f, -1.0f, "Sense objectiu: només es mostren les mesures." },
            { "Streaming (Spotify, YouTube)", -14.0f, -1.0f, "Aquestes plataformes abaixen el que passa de -14 LUFS: més fort no sona més fort, només més aixafat." },
            { "Apple Music", -16.0f, -1.0f, "Sound Check normalitza al voltant de -16 LUFS." },
            { "Dinàmic / acústic", -16.0f, -1.0f, "Per a cançons tranquil·les, folk o balades: respira més." },
            { "Pop i rock comercial", -10.0f, -1.0f, "Habitual en discos de pop i rock d'avui (entre -11 i -8)." },
            { "Rock modern fort", -8.0f, -1.0f, "Molt fort: vigila que no es perdi el punch de la bateria." },
            { "Ràdio i TV (EBU R128)", -23.0f, -1.0f, "La norma europea de difusió." },
        };
        return v;
    }

    Mesurador::Mesurador() { prepara (48000.0); }

    void Mesurador::prepara (double f)
    {
        fs = f > 1000.0 ? f : 48000.0;
        // Filtre K (BS.1770): prestatge alt + passa-alts, calculats per a qualsevol freqüència de mostreig
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (pi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            for (auto& b : k1)
            {
                b = {};
                b.b0 = (Vh + Vb * K / Q + K * K) / a0; b.b1 = 2.0 * (K * K - Vh) / a0; b.b2 = (Vh - Vb * K / Q + K * K) / a0;
                b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0;
            }
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (pi * f0 / fs), a0 = 1.0 + K / Q + K * K;
            for (auto& b : k2)
            {
                b = {};
                b.b0 = 1.0; b.b1 = -2.0; b.b2 = 1.0;
                b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0;
            }
        }
        // FIR de sobremostreig x4 (sinc amb finestra de Blackman, 48 coeficients)
        {
            constexpr int L = 4, N = L * tapsFase;
            const double c = (N - 1) / 2.0;
            for (int i = 0; i < N; ++i)
            {
                const double x = (i - c) / L;
                const double s = std::abs (x) < 1e-9 ? 1.0 : std::sin (pi * x) / (pi * x);
                const double w = 0.42 - 0.5 * std::cos (2.0 * pi * i / (N - 1)) + 0.08 * std::cos (4.0 * pi * i / (N - 1));
                fir[(size_t) (i % L)][(size_t) (i / L)] = (float) (s * w);
            }
        }
        midaSub = std::max (1, (int) std::lround (fs * 0.1));
        reiniciaAra();
    }

    void Mesurador::reiniciaAra() noexcept
    {
        for (auto& b : k1) b.z1 = b.z2 = 0.0;
        for (auto& b : k2) b.z1 = b.z2 = 0.0;
        posSub = 0;
        eK = eLin = eLL = eRR = eLR = eM = eS = 0.0;
        picSub = tpSub = 0.0f;
        subs.fill ({});
        iSub = plens = 0;
        histI.fill (0.0); energiaI.fill (0.0); histS.fill (0.0); energiaS.fill (0.0);
        for (auto& h : hist) h.fill (0.0f);
        hPos = 0;
        mostres = 0;
        for (auto* a : { &mes.momentary, &mes.shortTerm, &mes.integrated, &mes.momentaryMax, &mes.shortMax, &mes.truePeak, &mes.truePeakMax,
                         &mes.samplePeak, &mes.samplePeakMax, &mes.rms, &mes.sideMid })
            a->store (silenci, std::memory_order_relaxed);
        mes.lra.store (0.0f); mes.crest.store (0.0f); mes.correlacio.store (1.0f);
        mes.clips.store (0); mes.segons.store (0.0);
    }

    float Mesurador::truePeakMostra (float x, int c) noexcept
    {
        auto& h = hist[(size_t) c];
        h[(size_t) hPos] = x;
        float m = std::abs (x);
        for (int p = 0; p < 4; ++p)
        {
            float y = 0.0f;
            int j = hPos;
            for (int k = 0; k < tapsFase; ++k) { y += fir[(size_t) p][(size_t) k] * h[(size_t) j]; j = j == 0 ? tapsFase - 1 : j - 1; }
            m = std::max (m, std::abs (y));
        }
        return m;
    }

    void Mesurador::processa (const float* L, const float* R, int n) noexcept
    {
        if (demanaReinici.exchange (false, std::memory_order_relaxed)) reiniciaAra();
        if (L == nullptr || R == nullptr || n <= 0) return;
        int clips = 0;
        for (int i = 0; i < n; ++i)
        {
            const float l = std::isfinite (L[i]) ? L[i] : 0.0f, r = std::isfinite (R[i]) ? R[i] : 0.0f;
            const double kl = k2[0] (k1[0] (l)), kr = k2[1] (k1[1] (r));
            eK += kl * kl + kr * kr;
            eLin += (double) l * l + (double) r * r;
            eLL += (double) l * l; eRR += (double) r * r; eLR += (double) l * r;
            const double m = 0.5 * (l + r), s = 0.5 * (l - r);
            eM += m * m; eS += s * s;
            const float pic = std::max (std::abs (l), std::abs (r));
            picSub = std::max (picSub, pic);
            if (pic >= 0.99999f) ++clips;
            tpSub = std::max (tpSub, std::max (truePeakMostra (l, 0), truePeakMostra (r, 1)));
            hPos = (hPos + 1) % tapsFase;
            if (++posSub >= midaSub) tancaSubbloc();
        }
        if (clips > 0) mes.clips.fetch_add (clips, std::memory_order_relaxed);
        mostres += n;
        mes.segons.store ((double) mostres / fs, std::memory_order_relaxed);
    }

    void Mesurador::tancaSubbloc() noexcept
    {
        subs[(size_t) iSub] = { eK, eLin, eLL, eRR, eLR, eM, eS, picSub };
        iSub = (iSub + 1) % numSubs;
        plens = std::min (plens + 1, numSubs);
        const double N = (double) midaSub;
        // pics del bloc (100 ms) i màxims
        const float sp = aDb (picSub), tp = aDb (tpSub);
        mes.samplePeak.store (sp, std::memory_order_relaxed); maxAtom (mes.samplePeakMax, sp);
        mes.truePeak.store (tp, std::memory_order_relaxed); maxAtom (mes.truePeakMax, tp);
        posSub = 0;
        eK = eLin = eLL = eRR = eLR = eM = eS = 0.0;
        picSub = tpSub = 0.0f;

        auto suma = [this] (int quants, double Sub::* camp)
        {
            double s = 0.0;
            for (int k = 1; k <= quants; ++k) s += subs[(size_t) ((iSub - k + numSubs) % numSubs)].*camp;
            return s;
        };
        auto bin = [] (float lu) { return std::clamp ((int) ((lu - binMin) / 0.1f), 0, numBins - 1); };

        // Momentary (400 ms): també és el bloc de la porta de l'Integrated (cada 100 ms, 75 % de solapament)
        if (plens >= 4)
        {
            const double e = suma (4, &Sub::eK) / (4.0 * N);
            const float lm = aLufs (e);
            mes.momentary.store (lm, std::memory_order_relaxed);
            maxAtom (mes.momentaryMax, lm);
            if (lm > binMin) { const int b = bin (lm); histI[(size_t) b] += 1.0; energiaI[(size_t) b] += e; }
        }
        // Short-Term (3 s; al principi, el que hi ha)
        if (plens >= 4)
        {
            const double e = suma (plens, &Sub::eK) / ((double) plens * N);
            const float ls = aLufs (e);
            mes.shortTerm.store (ls, std::memory_order_relaxed);
            if (plens >= numSubs)
            {
                maxAtom (mes.shortMax, ls);
                if (ls > binMin) { const int b = bin (ls); histS[(size_t) b] += 1.0; energiaS[(size_t) b] += e; }
            }
        }
        // RMS i crest (3 s)
        {
            const double e = suma (plens, &Sub::eLin) / (2.0 * (double) plens * N);
            const float rmsDb = aDb (std::sqrt (e));
            float pic = 0.0f;
            for (int k = 1; k <= plens; ++k) pic = std::max (pic, subs[(size_t) ((iSub - k + numSubs) % numSubs)].pic);
            mes.rms.store (rmsDb, std::memory_order_relaxed);
            mes.crest.store (rmsDb > silenci + 1.0f ? std::max (0.0f, aDb (pic) - rmsDb) : 0.0f, std::memory_order_relaxed);
        }
        // correlació i Side/Mid (300 ms)
        {
            const int q = std::min (plens, 3);
            const double ll = suma (q, &Sub::eLL), rr = suma (q, &Sub::eRR), lr = suma (q, &Sub::eLR);
            const double m = suma (q, &Sub::eM), s = suma (q, &Sub::eS);
            const double den = std::sqrt (ll * rr);
            mes.correlacio.store (den > 1.0e-9 ? (float) std::clamp (lr / den, -1.0, 1.0) : 1.0f, std::memory_order_relaxed);
            mes.sideMid.store (m > 1.0e-9 ? (float) (10.0 * std::log10 (std::max (s, 1.0e-12) / m)) : silenci, std::memory_order_relaxed);
        }
        calculaPortes();
    }

    void Mesurador::calculaPortes() noexcept
    {
        auto centre = [] (int b) { return binMin + ((float) b + 0.5f) * 0.1f; };
        // Integrated: porta absoluta (-70, ja aplicada en omplir) i relativa (-10 LU)
        {
            double n = 0.0, e = 0.0;
            for (int b = 0; b < numBins; ++b) { n += histI[(size_t) b]; e += energiaI[(size_t) b]; }
            if (n > 0.0)
            {
                const float porta = aLufs (e / n) - 10.0f;
                double n2 = 0.0, e2 = 0.0;
                for (int b = 0; b < numBins; ++b)
                    if (centre (b) >= porta) { n2 += histI[(size_t) b]; e2 += energiaI[(size_t) b]; }
                mes.integrated.store (n2 > 0.0 ? aLufs (e2 / n2) : silenci, std::memory_order_relaxed);
            }
        }
        // LRA: porta relativa -20 LU i percentils 10 i 95
        {
            double n = 0.0, e = 0.0;
            for (int b = 0; b < numBins; ++b) { n += histS[(size_t) b]; e += energiaS[(size_t) b]; }
            if (n > 0.0)
            {
                const float porta = aLufs (e / n) - 20.0f;
                double total = 0.0;
                int b0 = 0;
                for (int b = 0; b < numBins; ++b) if (centre (b) >= porta) { if (total == 0.0) b0 = b; total += histS[(size_t) b]; }
                if (total > 0.0)
                {
                    double acc = 0.0;
                    float p10 = centre (b0), p95 = centre (b0);
                    bool fet10 = false;
                    for (int b = b0; b < numBins; ++b)
                    {
                        if (centre (b) < porta) continue;
                        acc += histS[(size_t) b];
                        if (! fet10 && acc >= 0.10 * total) { p10 = centre (b); fet10 = true; }
                        if (acc >= 0.95 * total) { p95 = centre (b); break; }
                    }
                    mes.lra.store (std::max (0.0f, p95 - p10), std::memory_order_relaxed);
                }
            }
        }
    }

    Resum Mesurador::resum() const noexcept
    {
        Resum r;
        r.integrated = mes.integrated.load(); r.lra = mes.lra.load();
        r.truePeakMax = mes.truePeakMax.load(); r.samplePeakMax = mes.samplePeakMax.load();
        r.shortMax = mes.shortMax.load(); r.momentaryMax = mes.momentaryMax.load();
        r.correlacio = mes.correlacio.load(); r.clips = mes.clips.load(); r.segons = mes.segons.load();
        return r;
    }

    Resum analitza (const float* L, const float* R, std::size_t n, double fs)
    {
        Mesurador m;
        m.prepara (fs);
        constexpr std::size_t tros = 4096;
        for (std::size_t o = 0; o < n; o += tros)
            m.processa (L + o, R + o, (int) std::min (tros, n - o));
        return m.resum();
    }
}
