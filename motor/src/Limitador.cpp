#include "mibase/Limitador.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mibase::limitador
{
    namespace
    {
        using S = saturacio::Saturacio;
        const char* const noms[numEstils] = { "Transparent", "Modern", "Punch", "Agressiu" };
        const char* const descs[numEstils] = {
            "Transparent: l'atac més suau (mira 5 ms endavant). El que menys es nota; per a acústics i màsters delicats.",
            "Modern: equilibrat i net (mira 2 ms). El de cada dia per al màster.",
            "Punch: un retallador suau abans deixa passar el cop de la bateria i el limitador treballa menys.",
            "Agressiu: retallador més dur i atac ràpid. Per fer-ho sonar molt fort (rock, electrònica).",
        };
        const double mirarMs[numEstils] = { 5.0, 2.0, 1.5, 1.0 };
        const float retallador[numEstils] = { 0.0f, 0.0f, 1.35f, 1.12f };   // nivell del retallador, relatiu al sostre (0 = sense)
        // clau, nom, min, max, defecte, enter, log, ajuda
        const InfoParam infos[numParams] = {
            { "estil", "Style", 0, numEstils - 1, 1, true, false, "Com treballa: Transparent, Modern, Punch o Agressiu." },
            { "gain", "Gain", 0, 24, 0, false, false, "Quant puja el volum abans de limitar (dB). Com més, més fort i més limitació." },
            { "ceiling", "Ceiling", -12, 0, -1, false, false, "El sostre: cap pic de la sortida en passa (dB). -1 dB és el més habitual." },
            { "release", "Release", 1, 1000, 80, false, true, "Com de ràpid torna a pujar el volum després d'un pic (ms). Amb Auto, és la base." },
            { "auto", "Auto", 0, 1, 1, true, false, "Release automàtic: ràpid per als cops curts i lent quan limita estona (menys bombeig)." },
            { "link", "Link", 0, 100, 100, false, false, "Estèreo enllaçat: 100 % els dos costats baixen igual (la imatge no es mou)." },
            { "truepeak", "True Peak", 0, 1, 1, true, false, "Vigila també els pics entre mostres (els que apareixen en convertir a MP3 o en reproduir)." },
            { "igualat", "Igualat", 0, 1, 0, true, false, "Escolta igualat: abaixa el que has pujat amb Gain, per comparar només el so (no el volum)." },
        };
        inline float lim (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
        std::string num (double x, int d)
        {
            char b[32];
            std::snprintf (b, sizeof b, "%.*f", d, x);
            std::string s = b;
            for (auto& c : s) if (c == '.') c = ',';
            return s;
        }
        /** Hermite: el valor a mig camí entre x0 i x1 (per al true peak aproximat). */
        inline float entre (float xm1, float x0, float x1, float x2, float f) noexcept
        {
            const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }
    }

    const char* nomEstil (int e) { return noms[std::clamp (e, 0, numEstils - 1)]; }
    const char* descripcioEstil (int e) { return descs[std::clamp (e, 0, numEstils - 1)]; }
    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }

    std::string textValor (int p, float v)
    {
        switch (p)
        {
            case EstilP: return nomEstil ((int) std::lround (v));
            case Gain: return (v > 0.05f ? "+" : "") + num (v, 1) + " dB";
            case Ceiling: return num (v, 1) + " dB";
            case Release: return num (v, v < 10 ? 1 : 0) + " ms";
            case Link: return num (v, 0) + " %";
            default: return v > 0.5f ? "Sí" : "No";
        }
    }

    Limitador::Limitador()
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def);
    }

    void Limitador::prepara (double nouFs, int)
    {
        fs = nouFs > 0.0 ? nouFs : 48000.0;
        lat = std::max (1, (int) std::lround (lookaheadMs * 0.001 * fs));
        mida = 1;
        while (mida < lat * 2 + 8) mida <<= 1;
        mascara = mida - 1;
        for (int c = 0; c < 2; ++c)
        {
            audio[c].assign ((size_t) mida, 0.0f);
            req[c].assign ((size_t) mida, 1.0f);
            suau[c].assign ((size_t) mida, 1.0f);
            gDelay[c].assign ((size_t) mida, 1.0f);
            dqT[c].assign ((size_t) mida, 0);
            dqV[c].assign ((size_t) mida, 1.0f);
            // filtre K (ITU-R BS.1770): prestatge alt +4 dB i passa-alts a 38 Hz
            S::prestatgeAlt (k1[c], 1681.0, 4.0, fs);
            S::passaAlts (k2[c], 38.0, 0.5, fs);
        }
        neteja();
    }

    void Limitador::neteja() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            std::fill (audio[c].begin(), audio[c].end(), 0.0f);
            std::fill (req[c].begin(), req[c].end(), 1.0f);
            std::fill (suau[c].begin(), suau[c].end(), 1.0f);
            std::fill (gDelay[c].begin(), gDelay[c].end(), 1.0f);
            suma[c] = 0.0;
            relEnv[c] = 1.0f;
            for (auto& h : hist4[c]) h = 0.0f;
            k1[c].neteja(); k2[c].neteja();
        }
        w = 0;
        t = 0;
        for (int c = 0; c < 2; ++c) dqCap[c] = dqCua[c] = 0;
        finAnterior = -1;
        primer = true;
        energia = 0.0; comptaE = 0; numTrossos = 0; posTros = 0;
        grMitja = 0.0f;
    }

    void Limitador::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        v = lim (v, infos[p].mn, infos[p].mx);
        if (infos[p].enter) v = (float) std::lround (v);
        par[(size_t) p].store (v, std::memory_order_relaxed);
    }
    float Limitador::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }
    float Limitador::normal (int p, float v) noexcept
    {
        const auto& f = info (p);
        if (f.log) return lim ((float) (std::log (v / f.mn) / std::log (f.mx / f.mn)), 0.0f, 1.0f);
        return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f);
    }
    float Limitador::deNormal (int p, float x) noexcept
    {
        const auto& f = info (p);
        x = lim (x, 0.0f, 1.0f);
        return f.log ? f.mn * std::pow (f.mx / f.mn, x) : f.mn + (f.mx - f.mn) * x;
    }
    InfoParametre Limitador::infoParametre (int i) const
    {
        const auto& f = info (i);
        InfoParametre r;
        r.id = f.clau;
        r.nom = f.nom;
        r.passos = f.enter ? (int) (f.mx - f.mn) + 1 : 0;
        r.automatitzable = true;
        return r;
    }
    void Limitador::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Limitador::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Limitador::textParametre (int i, float v01) const
    {
        float v = deNormal (i, v01);
        if (info (i).enter) v = (float) std::lround (v);
        return textValor (i, v);
    }
    std::string Limitador::estat() const
    {
        std::string s = "mibase-limitador=1";
        char b[64];
        for (int i = 0; i < numParams; ++i)
        {
            std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i));
            s += b;
        }
        return s;
    }
    void Limitador::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-limitador=", 0) != 0) return;
        for (int i = 0; i < numParams; ++i) posa (i, infos[i].def);
        size_t a = 0;
        while (a < s.size())
        {
            size_t b = s.find (';', a);
            if (b == std::string::npos) b = s.size();
            const auto parell = s.substr (a, b - a);
            const auto eq = parell.find ('=');
            if (eq != std::string::npos)
                for (int i = 0; i < numParams; ++i)
                    if (parell.compare (0, eq, infos[i].clau) == 0 && std::string (infos[i].clau).size() == eq)
                        posa (i, (float) std::strtod (parell.c_str() + eq + 1, nullptr));
            a = b + 1;
        }
    }


    void Limitador::processa (float* L, float* R, int n) noexcept
    {
        if (n <= 0 || audio[0].empty()) return;
        mes.blocs.fetch_add (1, std::memory_order_relaxed);
        if (demanaBuidar.exchange (false)) neteja();
        const int e = (int) valor (EstilP);
        // finestra de l'estil (≤ lat) i retard del guany perquè tot quadri amb la latència fixa
        const int fin = std::clamp ((int) std::lround (mirarMs[std::clamp (e, 0, numEstils - 1)] * 0.001 * fs), 1, lat);
        const int retG = lat - fin + 1;   // el so va retardat «lat»: el guany calculat ara és per a la mostra de fa «fin - 1»
        if (fin != finAnterior)
        {
            for (int c = 0; c < 2; ++c)
            {
                double s = 0.0;
                for (int k = 0; k < fin; ++k) s += suau[c][(size_t) ((w - 1 - k) & mascara)];
                suma[c] = s;
                // la cua del mínim, de nou amb les últimes «fin» mostres
                dqCap[c] = dqCua[c] = 0;
                for (int k = fin - 1; k >= 0; --k)
                {
                    const float v = req[c][(size_t) ((w - 1 - k) & mascara)];
                    while (dqCua[c] > dqCap[c] && dqV[c][(size_t) ((dqCua[c] - 1) & mascara)] >= v) --dqCua[c];
                    dqT[c][(size_t) (dqCua[c] & mascara)] = t - 1 - k;
                    dqV[c][(size_t) (dqCua[c] & mascara)] = v;
                    ++dqCua[c];
                }
            }
            finAnterior = fin;
        }
        const float gainObj = std::pow (10.0f, valor (Gain) / 20.0f);
        const float ceilObj = std::pow (10.0f, valor (Ceiling) / 20.0f);
        if (primer) { sGain = gainObj; sCeil = ceilObj; primer = false; }
        const float link = valor (Link) * 0.01f;
        const bool tp = valor (TruePeak) > 0.5f, autoR = valor (AutoRel) > 0.5f, igualat = valor (Igualat) > 0.5f;
        const float relMs = valor (Release);
        const float clip = retallador[std::clamp (e, 0, numEstils - 1)];
        const float sm = 1.0f - std::exp (-1.0f / (float) (0.02 * fs));
        const float coefGr = 1.0f - std::exp (-1.0f / (float) (0.5 * fs));
        const int mostresTros = (int) (0.1 * fs);
        const int mostresHist = (int) (0.05 * fs);
        float picE = 0.0f, picS = 0.0f, grMax = 0.0f;
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            sGain += (gainObj - sGain) * sm;
            sCeil += (ceilObj - sCeil) * sm;
            float x[2], g[2];
            for (int c = 0; c < 2; ++c)
            {
                float v = io[c][i];
                picE = std::max (picE, std::fabs (v));
                v *= sGain;
                if (clip > 0.0f)   // Punch / Agressiu: retallador suau abans del limitador
                {
                    const float C = sCeil * clip, k = 0.7f * C, a = std::fabs (v);
                    if (a > k) v = (v < 0 ? -1.0f : 1.0f) * (k + (C - k) * std::tanh ((a - k) / (C - k)));
                }
                x[c] = v;
                // pic (amb els pics entre mostres, si cal)
                float p = std::fabs (v);
                if (tp)
                {
                    auto& h = hist4[c];   // h[0] = x[n-3], h[1] = x[n-2], h[2] = x[n-1]
                    for (float f : { 0.25f, 0.5f, 0.75f }) p = std::max (p, std::fabs (entre (h[0], h[1], h[2], v, f)));
                    h[0] = h[1]; h[1] = h[2]; h[2] = v;
                }
                g[c] = p > sCeil ? sCeil / p : 1.0f;
            }
            // enllaç estèreo
            const float gm = std::min (g[0], g[1]);
            for (int c = 0; c < 2; ++c) g[c] = link * gm + (1.0f - link) * g[c];

            float y[2];
            for (int c = 0; c < 2; ++c)
            {
                // 1) mínim dels últims «fin» (finestra) — fet amb la cua circular
                req[c][(size_t) w] = g[c];
                {
                    auto& cap = dqCap[c]; auto& cua = dqCua[c];
                    while (cua > cap && dqV[c][(size_t) ((cua - 1) & mascara)] >= g[c]) --cua;
                    dqT[c][(size_t) (cua & mascara)] = t;
                    dqV[c][(size_t) (cua & mascara)] = g[c];
                    ++cua;
                    while (dqT[c][(size_t) (cap & mascara)] <= t - fin) ++cap;
                }
                const float mn = dqV[c][(size_t) (dqCap[c] & mascara)];
                // 2) release (el guany puja a poc a poc; baixa de cop)
                float rel = relMs;
                if (autoR) rel = relMs * (0.4f + 2.5f * std::min (1.0f, grMitja / 6.0f));   // limita estona: més lent
                const float cR = 1.0f - std::exp (-1.0f / (float) (std::max (1.0f, rel) * 0.001f * fs));
                float& r = relEnv[c];
                r = mn < r ? mn : r + (mn - r) * cR;
                // 3) mitjana de «fin» mostres (atac suau que acaba just a temps)
                suma[c] += r - suau[c][(size_t) ((w - fin) & mascara)];
                suau[c][(size_t) w] = r;
                float gs = (float) (suma[c] / fin);
                // 4) retard del guany per quadrar amb el so (latència fixa)
                gDelay[c][(size_t) w] = gs;
                gs = gDelay[c][(size_t) ((w - retG) & mascara)];
                // el so, retardat «lat» mostres (la latència que es diu al PDC)
                audio[c][(size_t) w] = x[c];
                float o = audio[c][(size_t) ((w - lat) & mascara)] * std::min (1.0f, gs);
                // seguretat: mai per sobre del sostre
                if (std::fabs (o) > sCeil * 1.0001f) { o = o > 0 ? sCeil : -sCeil; mes.retallades.fetch_add (1, std::memory_order_relaxed); }
                y[c] = o;
                grMax = std::max (grMax, gs < 1.0f ? -20.0f * std::log10 (std::max (1e-6f, gs)) : 0.0f);
            }
            w = (w + 1) & mascara;
            ++t;
            const float grAra = std::max (relEnv[0] < 1.0f ? -20.0f * std::log10 (std::max (1e-6f, relEnv[0])) : 0.0f,
                                          relEnv[1] < 1.0f ? -20.0f * std::log10 (std::max (1e-6f, relEnv[1])) : 0.0f);
            grMitja += (grAra - grMitja) * coefGr;
            // LUFS (abans del «igualat»: és el volum que surt de veritat)
            const float kl = k2[0] (k1[0] (y[0])), kr = k2[1] (k1[1] (y[1]));
            energia += (double) kl * kl + (double) kr * kr;
            if (++comptaE >= mostresTros)
            {
                trossos[(size_t) posTros] = energia / comptaE;
                posTros = (posTros + 1) % (int) trossos.size();
                numTrossos = std::min (numTrossos + 1, (int) trossos.size());
                energia = 0.0; comptaE = 0;
                double s = 0.0;
                for (int k = 0; k < numTrossos; ++k) s += trossos[(size_t) k];
                const double m = numTrossos > 0 ? s / numTrossos : 0.0;
                mes.lufsCurt.store (m > 1e-12 ? (float) (-0.691 + 10.0 * std::log10 (m)) : -100.0f, std::memory_order_relaxed);
            }
            maxHist = std::max (maxHist, grAra);
            if (++comptaHist >= mostresHist)
            {
                const int p = mes.posHist.load (std::memory_order_relaxed);
                mes.hist[(size_t) p].store (maxHist, std::memory_order_relaxed);
                mes.posHist.store ((p + 1) % Mesures::numHist, std::memory_order_relaxed);
                maxHist = 0.0f; comptaHist = 0;
            }
            const float aIg = igualat ? 1.0f / sGain : 1.0f;
            for (int c = 0; c < 2; ++c)
            {
                float o = y[c] * aIg;
                if (! std::isfinite (o)) o = 0.0f;
                io[c][i] = o;
                picS = std::max (picS, std::fabs (o));
            }
        }
        mes.entrada.store (std::max (picE, mes.entrada.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.sortida.store (std::max (picS, mes.sortida.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.reduccio.store (grMax, std::memory_order_relaxed);
        mes.reduccioMax.store (std::max (grMax, mes.reduccioMax.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    }
}
