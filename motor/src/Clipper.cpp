#include "mibase/Clipper.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mibase::clipper
{
    namespace
    {
        const char* const noms[numModes] = { "Soft", "Hard", "Analog", "Smooth", "Modern" };
        const InfoParam infos[numParams] = {
            { "mode", "Mode", 0, numModes - 1, Soft, "Soft: corba suau des del genoll. Hard: talla en sec. Analog: com una cinta, arrodoneix tota l'ona. Smooth: genoll ample. Modern: gairebé sec." },
            { "drive", "Drive", 0, 18, 0, "Quants dB entren al clipper: com més, més retalla (i més fort sona)." },
            { "ceiling", "Ceiling", -12, 0, -1, "On talla (dB). Davant del limitador, normalment entre -1 i -3 dB." },
            { "shape", "Shape", 0, 100, 50, "Només Soft: on comença a doblegar-se. 0 % = gairebé Hard; 100 % = molt suau." },
            { "os", "Oversampling", 0, 1, 1, "x4: menys aspror als aguts (12 mostres de retard, Mibase les compensa)." },
            { "link", "Link", 0, 1, 1, "El mateix retall als dos canals (l'estèreo no es mou)." },
            { "mix", "Mix", 0, 100, 100, "Barreja amb el so sense retallar." },
            { "sortida", "Sortida", -12, 12, 0, "Volum de sortida (dB)." },
        };
        inline float lim (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
        inline float dbALin (float db) { return std::pow (10.0f, db / 20.0f); }
        constexpr double pi = 3.14159265358979323846;
        float genoll (float x, float t, float k)
        {
            const float a = std::abs (x), kt = k * t;
            if (a <= kt) return x;
            const float r = t - kt;
            const float y = r > 1e-9f ? kt + r * std::tanh ((a - kt) / r) : t;
            return x < 0.0f ? -y : y;
        }
    }

    const char* nomMode (int m) { return noms[std::clamp (m, 0, numModes - 1)]; }
    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }

    float corba (int mode, float x, float t, float s) noexcept
    {
        switch (mode)
        {
            case Hard: return lim (x, -t, t);
            case Analog: return t * std::tanh (x / t);
            case Smooth: return genoll (x, t, 0.5f);
            case Modern: return genoll (x, t, 0.92f);
            default: return genoll (x, t, 1.0f - 0.9f * lim (s, 0.0f, 1.0f));
        }
    }

    Clipper::Clipper()
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def);
        // FIR de x4 (sinc amb finestra de Blackman, 49 coeficients; el centre és el 24)
        for (int i = 0; i < taps; ++i)
        {
            const double x = (i - 24) / 4.0;
            const double s = std::abs (x) < 1e-9 ? 1.0 : std::sin (pi * x) / (pi * x);
            const double w = 0.42 - 0.5 * std::cos (2.0 * pi * i / (taps - 1)) + 0.08 * std::cos (4.0 * pi * i / (taps - 1));
            fir[(size_t) i] = (float) (s * w);
        }
    }

    void Clipper::buidaAra() noexcept
    {
        for (auto& h : hUp) h.fill (0.0f);
        for (auto& h : hDown) h.fill (0.0f);
        for (auto& h : sec) h.fill (0.0f);
        pUp = pDown = pSec = 0;
    }

    void Clipper::prepara (double, int) { buidaAra(); primer = true; }

    void Clipper::processa (float* L, float* R, int n) noexcept
    {
        if (demanaBuidar.exchange (false)) buidaAra();
        if (n <= 0) return;
        const int mode = (int) std::lround (valor (ModeP));
        const float t = dbALin (valor (Ceiling)), s = valor (Shape) / 100.0f;
        const bool os = valor (Oversampling) >= 0.5f, link = valor (Link) >= 0.5f;
        const float tD = dbALin (valor (Drive)), tO = dbALin (valor (Sortida)), tM = valor (Mix) / 100.0f;
        if (primer) { gDrive = tD; gOut = tO; gMix = tM; primer = false; }
        const float inv = 1.0f / (float) n, dD = (tD - gDrive) * inv, dO = (tO - gOut) * inv, dM = (tM - gMix) * inv;
        float picIn = 0.0f, picOut = 0.0f;
        int tocades = 0;
        auto clipa = [&] (float& a, float& b)
        {
            const float pa = std::abs (a), pb = std::abs (b);
            picIn = std::max (picIn, std::max (pa, pb));
            float ya, yb;
            if (link)
            {
                const float m = std::max (pa, pb);
                const float g = m > 1e-9f ? std::abs (corba (mode, m, t, s)) / m : 1.0f;
                ya = a * g; yb = b * g;
            }
            else { ya = corba (mode, a, t, s); yb = corba (mode, b, t, s); }
            if (std::abs (ya - a) > 1e-3f * pa || std::abs (yb - b) > 1e-3f * pb) ++tocades;
            a = ya; b = yb;
        };
        for (int i = 0; i < n; ++i)
        {
            gDrive += dD; gOut += dO; gMix += dM;
            const float dl = L[i], dr = R[i];
            float ol, orr;
            if (! os)
            {
                float a = dl * gDrive, b = dr * gDrive;
                clipa (a, b);
                ol = a; orr = b;
            }
            else
            {
                hUp[0][(size_t) pUp] = dl * gDrive;
                hUp[1][(size_t) pUp] = dr * gDrive;
                ol = orr = 0.0f;
                for (int p = 0; p < 4; ++p)
                {
                    // fase p de la senyal x4: coeficients 4j + p sobre les mostres d'entrada n − j
                    float a = 0.0f, b = 0.0f;
                    int idx = pUp;
                    for (int k = p; k < taps; k += 4)
                    {
                        a += fir[(size_t) k] * hUp[0][(size_t) idx];
                        b += fir[(size_t) k] * hUp[1][(size_t) idx];
                        idx = idx == 0 ? taps - 1 : idx - 1;
                    }
                    clipa (a, b);
                    hDown[0][(size_t) pDown] = a;
                    hDown[1][(size_t) pDown] = b;
                    if (p == 0)   // la sortida cau a la fase 0: retard exacte de 48 mostres x4 = 12
                    {
                        int j = pDown;
                        for (int k = 0; k < taps; ++k)
                        {
                            ol += fir[(size_t) k] * hDown[0][(size_t) j];
                            orr += fir[(size_t) k] * hDown[1][(size_t) j];
                            j = j == 0 ? taps - 1 : j - 1;
                        }
                        ol *= 0.25f; orr *= 0.25f;
                    }
                    pDown = (pDown + 1) % taps;
                }
                pUp = (pUp + 1) % taps;
            }
            // el sec, alineat
            float sl = dl, sr = dr;
            if (os)
            {
                sec[0][(size_t) pSec] = dl; sec[1][(size_t) pSec] = dr;
                const int j = (pSec - latenciaOs + 16) % 16;
                sl = sec[0][(size_t) j]; sr = sec[1][(size_t) j];
                pSec = (pSec + 1) % 16;
            }
            picOut = std::max (picOut, std::max (std::abs (ol), std::abs (orr)));
            L[i] = (sl + (ol - sl) * gMix) * gOut;
            R[i] = (sr + (orr - sr) * gMix) * gOut;
        }
        gDrive = tD; gOut = tO; gMix = tM;
        const float r = picIn > t && picOut > 1e-9f ? std::max (0.0f, 20.0f * std::log10 (picIn / std::max (picOut, t * 0.5f))) : 0.0f;
        mes.retall.store (r, std::memory_order_relaxed);
        if (r > mes.retallMax.load (std::memory_order_relaxed)) mes.retallMax.store (r, std::memory_order_relaxed);
        const float pc = 100.0f * (float) tocades / (float) (os ? 4 * n : n);
        percentSuau += (pc - percentSuau) * 0.1f;
        mes.percent.store (percentSuau, std::memory_order_relaxed);
    }

    InfoParametre Clipper::infoParametre (int i) const
    {
        InfoParametre r;
        r.id = info (i).clau;
        r.nom = info (i).nom;
        if (i == ModeP) r.passos = numModes;
        if (i == Oversampling || i == Link) r.passos = 2;
        return r;
    }
    float Clipper::normal (int p, float v) noexcept { const auto& f = info (p); return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f); }
    float Clipper::deNormal (int p, float v01) noexcept { const auto& f = info (p); return f.mn + lim (v01, 0.0f, 1.0f) * (f.mx - f.mn); }
    void Clipper::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Clipper::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Clipper::textParametre (int i, float v01) const
    {
        const float v = deNormal (i, v01);
        char b[48];
        switch (i)
        {
            case ModeP: return nomMode ((int) std::lround (v));
            case Oversampling: return v >= 0.5f ? "x4" : "no";
            case Link: return v >= 0.5f ? "sí" : "no";
            case Drive: case Ceiling: case Sortida: std::snprintf (b, sizeof b, "%+.1f dB", (double) v); break;
            default: std::snprintf (b, sizeof b, "%d %%", (int) std::lround (v)); break;
        }
        std::string s (b);
        for (auto& c : s) if (c == '.') c = ',';
        return s;
    }
    void Clipper::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        if (p == ModeP || p == Oversampling || p == Link) v = std::round (v);
        par[(size_t) p].store (lim (v, infos[p].mn, infos[p].mx), std::memory_order_relaxed);
    }
    float Clipper::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }
    std::string Clipper::estat() const
    {
        std::string s = "mibase-clipper=1";
        char b[64];
        for (int i = 0; i < numParams; ++i) { std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i)); s += b; }
        return s;
    }
    void Clipper::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-clipper=", 0) != 0) return;
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
}
