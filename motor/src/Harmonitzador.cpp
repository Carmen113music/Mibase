#include "mibase/Harmonitzador.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
    constexpr double pi = 3.141592653589793;
    constexpr double fMin = 90.0, fMax = 1000.0;
    inline float lim (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
    long long potencia2 (double minim)
    {
        long long m = 1;
        while ((double) m < minim) m <<= 1;
        return m;
    }

    /** Desa i llegeix els paràmetres com «clau=valor;…» (com els altres efectes interns). */
    template <class Info, class Posa>
    void llegeixText (const std::string& s, const std::string& cap, const Info* infos, int num, Posa posa)
    {
        if (s.rfind (cap + "=", 0) != 0) return;
        for (int i = 0; i < num; ++i) posa (i, infos[i].def);
        size_t a = 0;
        while (a < s.size())
        {
            size_t b = s.find (';', a);
            if (b == std::string::npos) b = s.size();
            const auto parell = s.substr (a, b - a);
            const auto eq = parell.find ('=');
            if (eq != std::string::npos)
                for (int i = 0; i < num; ++i)
                    if (parell.compare (0, eq, infos[i].clau) == 0 && std::string (infos[i].clau).size() == eq)
                        posa (i, (float) std::strtod (parell.c_str() + eq + 1, nullptr));
            a = b + 1;
        }
    }
    std::string ambComa (std::string s) { for (auto& c : s) if (c == '.') c = ','; return s; }
}

//==============================================================================
namespace mibase
{
    ContextMusical& ContextMusical::global() { static ContextMusical c; return c; }

    void ContextMusical::notaOn (int n) noexcept
    {
        if (n < 0 || n > 127) return;
        bits[(size_t) (n >> 5)].fetch_or (1u << (n & 31), std::memory_order_relaxed);
    }
    void ContextMusical::notaOff (int n) noexcept
    {
        if (n < 0 || n > 127) return;
        bits[(size_t) (n >> 5)].fetch_and (~(1u << (n & 31)), std::memory_order_relaxed);
    }
    void ContextMusical::totesOff() noexcept { for (auto& b : bits) b.store (0, std::memory_order_relaxed); }
    bool ContextMusical::nota (int n) const noexcept
    {
        return n >= 0 && n <= 127 && (bits[(size_t) (n >> 5)].load (std::memory_order_relaxed) & (1u << (n & 31))) != 0;
    }
    int ContextMusical::notesPremudes (int* sortida, int max) const noexcept
    {
        int k = 0;
        for (int b = 0; b < 4 && k < max; ++b)
        {
            std::uint32_t v = bits[(size_t) b].load (std::memory_order_relaxed);
            for (int i = 0; i < 32 && k < max; ++i)
                if (v & (1u << i)) sortida[k++] = b * 32 + i;
        }
        return k;
    }
    void ContextMusical::publicaAcords (const std::vector<Tram>& t) noexcept
    {
        const int nou = 1 - actiu.load (std::memory_order_acquire);
        const int n = (int) std::min<size_t> (t.size(), (size_t) maxTrams);
        for (int i = 0; i < n; ++i) trams[nou][i] = t[(size_t) i];
        nTrams[nou].store (n, std::memory_order_release);
        actiu.store (nou, std::memory_order_release);
    }
    bool ContextMusical::acordA (double t, Tram& sortida) const noexcept
    {
        for (int intent = 0; intent < 3; ++intent)
        {
            const int k = actiu.load (std::memory_order_acquire);
            const int n = nTrams[k].load (std::memory_order_acquire);
            int a = 0, b = n;   // el darrer tram amb de <= t
            while (a < b) { const int m = (a + b) / 2; if (trams[k][m].de <= t) a = m + 1; else b = m; }
            bool hi = false;
            if (a > 0 && t < trams[k][a - 1].a) { sortida = trams[k][a - 1]; hi = true; }
            if (actiu.load (std::memory_order_acquire) == k) return hi;
        }
        return false;
    }
}

//==============================================================================
namespace mibase::canvito
{
    void Analitzador::prepara (double f)
    {
        frq = f > 1000.0 ? f : 48000.0;
        tSord = 0.005 * frq;
        hmax = frq / fMin;
        lat = (int) std::ceil (3.2 * hmax + 8.0);
        const long long mida = potencia2 (std::max (0.3 * frq, 4.0 * lat));
        anell.assign ((size_t) mida, 0.0f);
        filtrat.assign ((size_t) mida, 0.0f);
        mascara = mida - 1;
        k900 = (float) (1.0 - std::exp (-2.0 * pi * 900.0 / frq));
        dec = std::max (1, (int) std::lround (frq / 12000.0));
        fd = frq / dec;
        kD = (float) (1.0 - std::exp (-2.0 * pi * std::min (0.35 * fd, 4500.0) / frq));
        w = (int) (fd * 0.025);
        tauMax = (int) std::ceil (fd / fMin) + 2;
        tauMin = std::max (2, (int) (fd / fMax));
        hop = std::max (1, (int) (fd * 0.005));
        const long long dm = potencia2 (2.0 * (w + tauMax + 4));
        dAnell.assign ((size_t) dm, 0.0f);
        dMascara = dm - 1;
        dif.assign ((size_t) tauMax + 2, 0.0f);
        cmnd.assign ((size_t) tauMax + 2, 0.0f);
        lin.assign ((size_t) (w + tauMax + 2), 0.0f);
        marques.assign (1024, Marca {});
        buida();
    }

    void Analitzador::buida() noexcept
    {
        std::fill (anell.begin(), anell.end(), 0.0f);
        std::fill (filtrat.begin(), filtrat.end(), 0.0f);
        std::fill (dAnell.begin(), dAnell.end(), 0.0f);
        n = dn = nMarques = 0;
        lp1 = lp2 = d1 = d2 = acum = 0.0f;
        comptaDec = comptaHop = 0;
        hist[0] = hist[1] = hist[2] = 0.0f;
        f0Ara = 0.0f;
        ultima = 0.0;
        ultimaSonor = false;
    }

    float Analitzador::mostra (double p) const noexcept
    {
        const long long i = (long long) std::floor (p);
        if (i < n - mascara || i + 1 > n - 1) return (i >= 0 && i <= n - 1 && i > n - mascara) ? anell[(size_t) (i & mascara)] : 0.0f;
        const float f = (float) (p - (double) i);
        const float a = anell[(size_t) (i & mascara)], b = anell[(size_t) ((i + 1) & mascara)];
        return a + (b - a) * f;
    }

    double Analitzador::pic (double de, double a) const noexcept
    {
        long long b0 = std::max ((long long) std::ceil (de), n - mascara + 1), b1 = std::min ((long long) std::floor (a), n - 1);
        if (b1 < b0) return std::max (de, 0.0);
        long long millor = b0;
        float mx = -1e30f;
        for (long long t = b0; t <= b1; ++t)
        {
            const float v = filtrat[(size_t) (t & mascara)];
            if (v > mx) { mx = v; millor = t; }
        }
        return (double) millor;
    }

    void Analitzador::detecta() noexcept
    {
        const int total = w + tauMax;
        if (dn < total) { f0Ara = 0.0f; return; }
        const long long s = dn - total;
        double energia = 0.0;
        for (int j = 0; j < total; ++j) lin[(size_t) j] = dAnell[(size_t) ((s + j) & dMascara)];
        for (int j = 0; j < w; ++j) energia += (double) lin[(size_t) j] * lin[(size_t) j];
        float f = 0.0f;
        if (std::sqrt (energia / w) > 0.003)   // sota uns -50 dB: silenci
        {
            const float* x = lin.data();
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                double d = 0.0;
                const float* y = x + tau;
                for (int j = 0; j < w; ++j) { const float e = x[j] - y[j]; d += (double) e * e; }
                dif[(size_t) tau] = (float) d;
            }
            cmnd[0] = 1.0f;
            double suma = 0.0;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                suma += dif[(size_t) tau];
                cmnd[(size_t) tau] = suma > 0.0 ? (float) (dif[(size_t) tau] * tau / suma) : 1.0f;
            }
            int t = -1;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (cmnd[(size_t) tau] < 0.18f)
                {
                    while (tau + 1 < tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                    t = tau;
                    break;
                }
            if (t > 0)
            {
                const double a = cmnd[(size_t) t - 1], b = cmnd[(size_t) t], c = cmnd[(size_t) t + 1];
                const double den = a - 2.0 * b + c;
                const double tf = t + (std::abs (den) > 1e-12 ? 0.5 * (a - c) / den : 0.0);
                const double hz = fd / tf;
                if (hz >= fMin && hz <= fMax) f = (float) hz;
            }
        }
        hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = f;
        // mitjana de les tres darreres (amb zeros = sense to): no salta per una lectura dolenta
        const float a = hist[0], b = hist[1], c = hist[2];
        f0Ara = std::max (std::min (a, b), std::min (std::max (a, b), c));
    }

    void Analitzador::marcaNova() noexcept
    {
        const double m = ultima;
        Marca mk;
        if (f0Ara > 0.0f)
        {
            const double P = frq / f0Ara;
            double seg = ultimaSonor ? pic (m + 0.85 * P, m + 1.15 * P) : pic (m + 0.5 * P, m + 1.5 * P);
            seg = std::max (seg, m + 0.5 * P);
            mk = { seg, (float) P, true };
        }
        else mk = { m + tSord, (float) tSord, false };
        marques[(size_t) (nMarques & (long long) (marques.size() - 1))] = mk;
        ++nMarques;
        ultima = mk.pos;
        ultimaSonor = mk.sonor;
    }

    void Analitzador::entra (float x) noexcept
    {
        if (anell.empty()) return;
        const size_t i = (size_t) (n & mascara);
        anell[i] = x;
        lp1 += k900 * (x - lp1);
        lp2 += k900 * (lp1 - lp2);
        filtrat[i] = lp2;
        d1 += kD * (x - d1);
        d2 += kD * (d1 - d2);
        acum += d2;
        if (++comptaDec >= dec)
        {
            dAnell[(size_t) (dn & dMascara)] = acum / (float) dec;
            ++dn;
            acum = 0.0f;
            comptaDec = 0;
            if (++comptaHop >= hop) { comptaHop = 0; detecta(); }
        }
        ++n;
        for (int guard = 0; guard < 8; ++guard)
        {
            const double cal = f0Ara > 0.0f ? ultima + 1.5 * frq / f0Ara : ultima + tSord;
            if ((double) (n - 1) < cal) break;
            marcaNova();
        }
    }

    //--------------------------------------------------------------------------
    void Veu::prepara (const Analitzador& a)
    {
        const long long mida = potencia2 (std::max (0.3 * a.fs(), 4.0 * a.latencia()));
        acc.assign ((size_t) mida, 0.0f);
        pes.assign ((size_t) mida, 0.0f);
        mascara = mida - 1;
        buida();
    }

    void Veu::buida() noexcept
    {
        std::fill (acc.begin(), acc.end(), 0.0f);
        std::fill (pes.begin(), pes.end(), 0.0f);
        tS = 0.0;
        cursor = 0;
    }

    float Veu::processa (const Analitzador& a, double semitons, double formant, float guanySord) noexcept
    {
        if (acc.empty()) return 0.0f;
        const long long n = a.mostres();
        const long long o = n - 1 - (long long) a.latencia();
        const double r = std::pow (2.0, std::clamp (semitons, -24.0, 24.0) / 12.0);
        const double kf = std::pow (2.0, std::clamp (formant, -4.0, 4.0) / 12.0);
        for (int guard = 0; guard < 16 && a.numMarques() > 0; ++guard)
        {
            long long k = std::max (cursor, a.primeraMarca());
            const long long ultimaK = a.numMarques() - 1;
            if (k > ultimaK || a.marca (ultimaK).pos < tS) break;   // encara no se sap quina és la marca més propera
            while (k + 1 <= ultimaK && std::abs (a.marca (k + 1).pos - tS) <= std::abs (a.marca (k).pos - tS)) ++k;
            cursor = k;
            const Marca mk = a.marca (k);
            double H, pasS, centre, kk;
            float g;
            if (mk.sonor)
            {
                // 0.74: el període de debò és la distància entre polsos veïns (la detecció va uns 20 ms endarrerida
                // i, en un canvi de nota, uns grans massa amples es trepitjaven i el so feia un forat)
                double P = mk.periode;
                const bool abans = k > a.primeraMarca() && a.marca (k - 1).sonor, despres = k < ultimaK && a.marca (k + 1).sonor;
                if (abans && despres) P = 0.5 * (a.marca (k + 1).pos - a.marca (k - 1).pos);
                else if (despres) P = a.marca (k + 1).pos - mk.pos;
                else if (abans) P = mk.pos - a.marca (k - 1).pos;
                P = std::clamp (P, 0.5 * mk.periode, 2.0 * mk.periode);
                H = std::min (P, a.hMax());   // grans de dos períodes (PSOLA clàssic): baixant, queden separats
                pasS = std::max (1.0, P / r);
                centre = mk.pos;
                kk = kf;
                g = 1.0f;
            }
            else
            {
                H = pasS = a.tu();
                centre = tS;
                kk = 1.0;
                g = guanySord;
            }
            if ((double) (n - 1) < centre + H * std::max (1.0, kk) + 1.0) break;
            // s'ha quedat enrere (o és l'inici): no s'escriu mai el que ja ha sortit ni abans de la mostra 0
            // (0.74: abans, el primer gra queia a posicions negatives de l'anell i sortia com un clic 0,34 s més tard)
            if (tS - H <= (double) std::max (o, -1LL))
            {
                tS = (double) o + 1.0 + H;
                ++salts;
                continue;
            }
            const long long i0 = (long long) std::ceil (tS - H), i1 = (long long) std::floor (tS + H);
            const double invH = pi / H;
            for (long long i = i0; i <= i1; ++i)
            {
                const double d = (double) i - tS;
                const float wv = (float) (0.5 + 0.5 * std::cos (d * invH));
                const size_t j = (size_t) (i & mascara);
                acc[j] += g * wv * a.mostra (centre + d * kk);
                pes[j] += wv;
            }
            tS += pasS;
        }
        if (o < 0) return 0.0f;
        const size_t j = (size_t) (o & mascara);
        const float y = acc[j] / std::max (pes[j], 0.6f);
        acc[j] = pes[j] = 0.0f;
        return y;
    }
}

//==============================================================================
namespace mibase::harmonitzador
{
    namespace
    {
        const InfoParam infos[numParams] = {
            { "mode", "Mode", 0, 2, 0, "D'on surten les notes de les veus: la tonalitat, la pista d'Acords o el teclat MIDI." },
            { "sec", "Veu", 0, 100, 100, "La veu original (la que cantes)." },
            { "harmonies", "Harmonies", 0, 100, 70, "El volum de totes les veus noves." },
            { "formant", "Formant", -4, 4, 0, "El color de les veus noves: menys, més fosc i gran; més, més prim." },
            { "humanitza", "Humanitza", 0, 100, 30, "Petites desafinacions lentes a cada veu perquè no sonin de robot." },
            { "lliscament", "Lliscament", 0, 200, 20, "Quant triguen les veus a arribar a la nota nova (ms)." },
            { "veu1", "Veu 1", -7, 7, 2, "On va la veu 1 (graus de l'escala o notes de l'acord; 0 = apagada)." },
            { "veu2", "Veu 2", -7, 7, 4, "On va la veu 2." },
            { "veu3", "Veu 3", -7, 7, 0, "On va la veu 3." },
            { "veu4", "Veu 4", -7, 7, 0, "On va la veu 4." },
            { "nivell1", "Nivell 1", -30, 6, -6, "Volum de la veu 1 (dB)." },
            { "nivell2", "Nivell 2", -30, 6, -6, "Volum de la veu 2 (dB)." },
            { "nivell3", "Nivell 3", -30, 6, -8, "Volum de la veu 3 (dB)." },
            { "nivell4", "Nivell 4", -30, 6, -8, "Volum de la veu 4 (dB)." },
            { "pan1", "Pan 1", -100, 100, -40, "On sona la veu 1 (esquerra / dreta)." },
            { "pan2", "Pan 2", -100, 100, 40, "On sona la veu 2." },
            { "pan3", "Pan 3", -100, 100, -70, "On sona la veu 3." },
            { "pan4", "Pan 4", -100, 100, 70, "On sona la veu 4." },
        };
        bool esEnter (int p) { return p == ModeP || (p >= Interval1 && p < Interval1 + numVeus); }
        int absoluta (int mascara, int tonica)
        {
            tonica = ((tonica % 12) + 12) % 12;
            return ((mascara << tonica) | (mascara >> (12 - tonica))) & 0xFFF;
        }
    }

    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }
    const char* nomMode (int m)
    {
        static const char* const n[numModes] = { "Tonalitat", "Acords", "Teclat MIDI" };
        return n[std::clamp (m, 0, numModes - 1)];
    }
    std::string textInterval (int mode, int v)
    {
        if (mode == Teclat) return "Del teclat";
        if (v == 0) return "Apagada";
        const int a = std::abs (v);
        char b[64];
        if (mode == Acords)
            std::snprintf (b, sizeof b, "%da de l'acord %s", a, v > 0 ? "amunt" : "avall");
        else if (a == 7) std::snprintf (b, sizeof b, "Octava %s", v > 0 ? "amunt" : "avall");
        else std::snprintf (b, sizeof b, "%da %s", a + 1, v > 0 ? "amunt" : "avall");
        return b;
    }

    int ajustaAMascara (int nota, int mascara) noexcept
    {
        if ((mascara & 0xFFF) == 0) return nota;
        for (int d = 0; d <= 6; ++d)
        {
            if ((mascara >> (((nota - d) % 12 + 12) % 12)) & 1) return nota - d;
            if ((mascara >> (((nota + d) % 12 + 12) % 12)) & 1) return nota + d;
        }
        return nota;
    }
    int mouDinsMascara (int nota, int mascara, int passos) noexcept
    {
        if ((mascara & 0xFFF) == 0) return nota;
        int x = ajustaAMascara (nota, mascara);
        const int dir = passos > 0 ? 1 : -1;
        for (int k = 0; k < std::abs (passos); ++k)
        {
            int guard = 0;
            do { x += dir; } while (! ((mascara >> ((x % 12 + 12) % 12)) & 1) && ++guard < 12);
        }
        return x;
    }

    Harmonitzador::Harmonitzador() : ctx (&ContextMusical::global())
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def);
        for (auto& n : notesVeus) n.store (-1);
    }
    Harmonitzador::~Harmonitzador() { registraTeclat (false); }

    void Harmonitzador::registraTeclat (bool vol) noexcept
    {
        bool era = escolta.load();
        if (era != vol && escolta.compare_exchange_strong (era, vol))
            ctx->escoltantTeclat.fetch_add (vol ? 1 : -1);
    }

    void Harmonitzador::prepara (double f, int)
    {
        fs = f > 1000.0 ? f : 48000.0;
        an.prepara (fs);
        for (auto& v : veus) v.prepara (an);
        const long long mida = potencia2 (2.0 * an.latencia() + 16);
        secL.assign ((size_t) mida, 0.0f);
        secR.assign ((size_t) mida, 0.0f);
        secMascara = mida - 1;
        secW = 0;
        semi.fill (0.0f); guany.fill (0.0f); deriva.fill (0.0f); desti.fill (0.0f);
        notaLead = -1;
        sSec = valor (Sec) / 100.0f;
    }

    void Harmonitzador::processa (float* L, float* R, int n) noexcept
    {
        if (secL.empty()) return;
        if (demanaBuidar.exchange (false))
        {
            an.buida();
            for (auto& v : veus) v.buida();
            std::fill (secL.begin(), secL.end(), 0.0f);
            std::fill (secR.begin(), secR.end(), 0.0f);
            guany.fill (0.0f);
        }
        const int mode = std::clamp ((int) std::lround (valor (ModeP)), 0, numModes - 1);
        registraTeclat (mode == Teclat);
        const float secDesti = valor (Sec) / 100.0f;
        const float harm = valor (Harmonies) / 100.0f;
        const double formant = valor (Formant);
        const float hum = valor (Humanitza) / 100.0f;
        const double glide = std::max (0.012, valor (Lliscament) * 0.001);   // 0.74: sempre una mica (sense salts secs)
        const int lat = an.latencia();
        const float kG = (float) (1.0 - std::exp (-1.0 / (0.02 * fs)));   // volums: 20 ms
        float gL[numVeus], gR[numVeus], gDesti[numVeus];
        for (int v = 0; v < numVeus; ++v)
        {
            const double p = (valor (Pan1 + v) / 100.0 + 1.0) * 0.25 * pi;
            gL[v] = (float) std::cos (p);
            gR[v] = (float) std::sin (p);
        }

        constexpr int tros = 64;
        for (int base = 0; base < n; base += tros)
        {
            const int m = std::min (tros, n - base);
            // ---- les notes de les veus (un cop per tros) ----
            const float f = an.f0();
            if (f > 0.0f)
            {
                const double det = 69.0 + 12.0 * std::log2 (f / 440.0);
                detectada.store ((float) det, std::memory_order_relaxed);
                // 0.74: la nota que cantes canvia només si la nova dura 40 ms (en lliscar d'una nota a l'altra, les veus
                // no salten per les del mig)
                const int cand = (int) std::lround (det);
                if (notaLead < 0) notaLead = cand;
                else if (std::abs (det - notaLead) > 0.7)
                {
                    if (cand == pendent) pendentMostres += m;
                    else { pendent = cand; pendentMostres = m; }
                    if (pendentMostres >= (long long) (0.04 * fs)) { notaLead = cand; pendent = -1; }
                }
                else pendent = -1;
                int premudes[numVeus] {};
                int nPrem = mode == Teclat ? ctx->notesPremudes (premudes, numVeus) : 0;
                if (mode == Teclat)   // 0.74: en canviar d'acord, les notes es mantenen 120 ms (no hi ha forat entre deixar i prémer)
                {
                    if (nPrem > 0) { nUltimes = nPrem; for (int v = 0; v < nPrem; ++v) ultimesNotes[v] = premudes[v]; senseNotes = 0; }
                    else if ((senseNotes += m) < (long long) (0.12 * fs)) { nPrem = nUltimes; for (int v = 0; v < nPrem; ++v) premudes[v] = ultimesNotes[v]; }
                    else nUltimes = 0;
                }
                ContextMusical::Tram tr;
                const bool hiHaAcord = mode == Acords && ctx->acordA (ctx->segons.load (std::memory_order_relaxed), tr) && (tr.mascara & 0xFFF) != 0;
                const int escala = absoluta (ctx->mascaraEscala.load (std::memory_order_relaxed), ctx->tonica.load (std::memory_order_relaxed));
                for (int v = 0; v < numVeus; ++v)
                {
                    int t = -1;
                    if (mode == Teclat) t = v < nPrem ? premudes[v] : -1;
                    else
                    {
                        const int k = (int) std::lround (valor (Interval1 + v));
                        if (k != 0)
                        {
                            const int masc = hiHaAcord ? tr.mascara : escala;
                            t = mouDinsMascara (notaLead, masc, k);
                            if (t == notaLead) t = mouDinsMascara (t, masc, k > 0 ? 1 : -1);
                        }
                    }
                    notesVeus[(size_t) v].store (t, std::memory_order_relaxed);
                    if (t >= 0)
                    {
                        // Teclat: la nota exacta. Tonalitat i acords: l'interval des de la nota que canta (segueix la seva expressió).
                        const double s = mode == Teclat ? (double) t - det : (double) (t - notaLead);
                        desti[(size_t) v] = (float) std::clamp (s, -24.0, 24.0);
                        gDesti[v] = harm * std::pow (10.0f, valor (Nivell1 + v) / 20.0f);
                        if (guany[(size_t) v] < 1e-4f) semi[(size_t) v] = desti[(size_t) v];   // entra directament a la nota
                    }
                    else gDesti[v] = 0.0f;
                }
            }
            else
            {
                detectada.store (-1.0f, std::memory_order_relaxed);
                for (int v = 0; v < numVeus; ++v)
                {
                    const bool activa = notesVeus[(size_t) v].load (std::memory_order_relaxed) >= 0;
                    gDesti[v] = activa ? harm * std::pow (10.0f, valor (Nivell1 + v) / 20.0f) : 0.0f;
                }
                if (mode == Teclat)   // sense notes premudes (més de 120 ms), cap veu (encara que no canti)
                {
                    int p[1];
                    if (ctx->notesPremudes (p, 1) == 0 && (senseNotes += m) >= (long long) (0.12 * fs))
                        for (int v = 0; v < numVeus; ++v) { gDesti[v] = 0.0f; notesVeus[(size_t) v].store (-1); }
                    else if (ctx->notesPremudes (p, 1) > 0) senseNotes = 0;
                }
            }
            const float kS = (float) (1.0 - std::exp (-(double) m / (glide * fs)));
            for (int v = 0; v < numVeus; ++v)
            {
                semi[(size_t) v] += (desti[(size_t) v] - semi[(size_t) v]) * kS;
                // humanitza: una deriva lenta de pocs cèntims, diferent per a cada veu
                llavor = llavor * 1664525u + 1013904223u;
                const float rnd = (float) ((llavor >> 8) & 0xFFFF) / 65535.0f - 0.5f;
                deriva[(size_t) v] = lim (deriva[(size_t) v] * 0.995f + rnd * hum * 0.8f, -14.0f * hum, 14.0f * hum);
            }

            for (int i = base; i < base + m; ++i)
            {
                const float l = L[i], r = R[i];
                an.entra (0.5f * (l + r));
                secL[(size_t) (secW & secMascara)] = l;
                secR[(size_t) (secW & secMascara)] = r;
                const long long rd = secW - lat;
                const float dl = rd >= 0 ? secL[(size_t) (rd & secMascara)] : 0.0f;
                const float dr = rd >= 0 ? secR[(size_t) (rd & secMascara)] : 0.0f;
                ++secW;
                sSec += (secDesti - sSec) * kG;
                float oL = sSec * dl, oR = sSec * dr;
                for (int v = 0; v < numVeus; ++v)
                {
                    const float y = veus[(size_t) v].processa (an, semi[(size_t) v] + deriva[(size_t) v] * 0.01, formant, 0.3f);
                    guany[(size_t) v] += (gDesti[v] - guany[(size_t) v]) * kG;
                    oL += guany[(size_t) v] * gL[v] * y;
                    oR += guany[(size_t) v] * gR[v] * y;
                }
                L[i] = oL;
                R[i] = oR;
            }
        }
    }

    InfoParametre Harmonitzador::infoParametre (int i) const
    {
        InfoParametre r;
        r.id = info (i).clau;
        r.nom = info (i).nom;
        if (i == ModeP) r.passos = numModes;
        else if (esEnter (i)) r.passos = 15;
        return r;
    }
    float Harmonitzador::normal (int p, float v) noexcept
    {
        const auto& f = info (p);
        return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f);
    }
    float Harmonitzador::deNormal (int p, float v01) noexcept
    {
        const auto& f = info (p);
        return f.mn + lim (v01, 0.0f, 1.0f) * (f.mx - f.mn);
    }
    void Harmonitzador::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Harmonitzador::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Harmonitzador::textParametre (int i, float v01) const
    {
        float v = deNormal (i, v01);
        if (esEnter (i)) v = std::round (v);
        char b[64];
        if (i == ModeP) return nomMode ((int) v);
        if (i >= Interval1 && i < Interval1 + numVeus) return textInterval ((int) std::lround (valor (ModeP)), (int) v);
        if (i >= Nivell1 && i < Nivell1 + numVeus) std::snprintf (b, sizeof b, "%.1f dB", (double) v);
        else if (i >= Pan1 && i < Pan1 + numVeus)
        {
            const int p = (int) std::lround (v);
            if (p == 0) return "C";
            std::snprintf (b, sizeof b, "%d %s", std::abs (p), p < 0 ? "E" : "D");
        }
        else if (i == Formant) std::snprintf (b, sizeof b, "%+.1f st", (double) v);
        else if (i == Lliscament) std::snprintf (b, sizeof b, "%d ms", (int) std::lround (v));
        else std::snprintf (b, sizeof b, "%d %%", (int) std::lround (v));
        return ambComa (b);
    }
    void Harmonitzador::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        v = lim (v, infos[p].mn, infos[p].mx);
        if (esEnter (p)) v = std::round (v);
        par[(size_t) p].store (v, std::memory_order_relaxed);
    }
    float Harmonitzador::valor (int p) const noexcept
    {
        return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f;
    }
    std::string Harmonitzador::estat() const
    {
        std::string s = "mibase-harmonitzador=1";
        char b[64];
        for (int i = 0; i < numParams; ++i) { std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i)); s += b; }
        return s;
    }
    void Harmonitzador::posaEstat (const std::string& s)
    {
        llegeixText (s, "mibase-harmonitzador", infos, numParams, [this] (int i, float v) { posa (i, v); });
    }
}

//==============================================================================
namespace mibase::octava
{
    namespace
    {
        const InfoParam infos[numParams] = {
            { "to", "To", -24, 24, -12, "Quants semitons puja o baixa la veu (-12 = una octava avall)." },
            { "fi", "Fi", -100, 100, 0, "Ajust fi, en cèntims." },
            { "formant", "Formant", -4, 4, 0, "El color de la veu: menys, més fosca i gran; més, més prima. Sense canviar la nota." },
            { "mescla", "Mescla", 0, 100, 50, "0 %: només la veu original. 100 %: només la veu nova." },
        };
    }
    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }

    Octava::Octava() { for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def); }

    void Octava::prepara (double f, int)
    {
        an.prepara (f > 1000.0 ? f : 48000.0);
        veu.prepara (an);
        const long long mida = potencia2 (2.0 * an.latencia() + 16);
        secL.assign ((size_t) mida, 0.0f);
        secR.assign ((size_t) mida, 0.0f);
        secMascara = mida - 1;
        secW = 0;
        primer = true;
    }

    void Octava::processa (float* L, float* R, int n) noexcept
    {
        if (secL.empty()) return;
        if (demanaBuidar.exchange (false))
        {
            an.buida();
            veu.buida();
            std::fill (secL.begin(), secL.end(), 0.0f);
            std::fill (secR.begin(), secR.end(), 0.0f);
        }
        const double semitons = valor (To) + valor (Fi) / 100.0;
        const double formant = valor (Formant);
        const float mescla = valor (Mescla) / 100.0f;
        if (primer) { sMescla = mescla; primer = false; }
        const float dm = (mescla - sMescla) / (float) std::max (1, n);
        const int lat = an.latencia();
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R[i];
            an.entra (0.5f * (l + r));
            const float y = veu.processa (an, semitons, formant, 1.0f);
            secL[(size_t) (secW & secMascara)] = l;
            secR[(size_t) (secW & secMascara)] = r;
            const long long rd = secW - lat;
            const float dl = rd >= 0 ? secL[(size_t) (rd & secMascara)] : 0.0f;
            const float dr = rd >= 0 ? secR[(size_t) (rd & secMascara)] : 0.0f;
            ++secW;
            sMescla += dm;
            L[i] = (1.0f - sMescla) * dl + sMescla * y;
            R[i] = (1.0f - sMescla) * dr + sMescla * y;
        }
        sMescla = mescla;
    }

    InfoParametre Octava::infoParametre (int i) const
    {
        InfoParametre r;
        r.id = info (i).clau;
        r.nom = info (i).nom;
        if (i == To) r.passos = 49;
        return r;
    }
    float Octava::normal (int p, float v) noexcept { const auto& f = info (p); return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f); }
    float Octava::deNormal (int p, float v01) noexcept { const auto& f = info (p); return f.mn + lim (v01, 0.0f, 1.0f) * (f.mx - f.mn); }
    void Octava::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Octava::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Octava::textParametre (int i, float v01) const
    {
        const float v = deNormal (i, v01);
        char b[48];
        switch (i)
        {
            case To:      std::snprintf (b, sizeof b, "%+d st", (int) std::lround (v)); break;
            case Fi:      std::snprintf (b, sizeof b, "%+d cèntims", (int) std::lround (v)); break;
            case Formant: std::snprintf (b, sizeof b, "%+.1f st", (double) v); break;
            default:      std::snprintf (b, sizeof b, "%d %%", (int) std::lround (v)); break;
        }
        return ambComa (b);
    }
    void Octava::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        v = lim (v, infos[p].mn, infos[p].mx);
        if (p == To) v = std::round (v);
        par[(size_t) p].store (v, std::memory_order_relaxed);
    }
    float Octava::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }
    std::string Octava::estat() const
    {
        std::string s = "mibase-octava=1";
        char b[64];
        for (int i = 0; i < numParams; ++i) { std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i)); s += b; }
        return s;
    }
    void Octava::posaEstat (const std::string& s)
    {
        llegeixText (s, "mibase-octava", infos, numParams, [this] (int i, float v) { posa (i, v); });
    }
}
