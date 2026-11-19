#include "mibase/Delay.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mibase::delay
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        const char* const nomsCar[numCaracters] = { "Digital", "Cinta", "Analògic", "Lo-fi" };
        const char* const descCar[numCaracters] = {
            "Net i precís: cada eco igual que l'original (només el que facin els filtres).",
            "Com un eco de cinta: ecos més foscos que es van saturant, amb wow i flutter.",
            "Com un pedal analògic (BBD): cada eco més fosc i rodó; com més llarg, més fosc.",
            "Ecos bruts i petits: menys bits, menys aguts i menys greus.",
        };
        const char* const nomsEnc[numEncaminaments] = { "Estèreo", "Ping-pong", "Mono" };

        struct NotaInfo { const char* nom; double negres; };
        const NotaInfo notes[] = { { "1/32", 0.125 }, { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/8T", 1.0 / 3.0 }, { "1/16·", 0.375 },
                                   { "1/8", 0.5 }, { "1/4T", 2.0 / 3.0 }, { "1/8·", 0.75 }, { "1/4", 1.0 }, { "1/2T", 4.0 / 3.0 },
                                   { "1/4·", 1.5 }, { "1/2", 2.0 }, { "1/2·", 3.0 }, { "1/1", 4.0 } };
        constexpr int nNotes = (int) (sizeof (notes) / sizeof (notes[0]));

        // clau, nom, min, max, defecte, enter, log, ajuda
        const InfoParam infos[numParams] = {
            { "caracter", "Caràcter", 0, numCaracters - 1, 0, true, false, "Digital, Cinta, Analògic o Lo-fi: com sona cada repetició." },
            { "encaminament", "Ecos", 0, numEncaminaments - 1, 0, true, false, "Estèreo: cada costat el seu. Ping-pong: els ecos van d'un costat a l'altre. Mono: al mig." },
            { "sync", "Sync", 0, 1, 1, true, false, "Encès: el temps és una nota i segueix el tempo de la cançó. Apagat: en mil·lisegons." },
            { "temps", "Time", 1, 3000, 375, false, true, "Temps entre ecos, en mil·lisegons (amb Sync apagat)." },
            { "nota", "Nota", 0, nNotes - 1, 7, true, false, "Temps entre ecos en notes (amb Sync encès): 1/8· és el clàssic «dotted eighth»." },
            { "desfas", "Offset R", -50, 50, 0, false, false, "El costat dret, més curt o més llarg que l'esquerre (%). Obre l'estèreo." },
            { "feedback", "Feedback", 0, 110, 35, false, false, "Quantes repeticions: com més alt, més ecos. Per sobre de 100 % creix sol (amb cura)." },
            { "lowcut", "Low Cut", 20, 2000, 20, false, true, "Treu greus dels ecos (cada repetició en té menys)." },
            { "highcut", "High Cut", 1000, 20000, 20000, false, true, "Treu aguts dels ecos (cada repetició és més fosca)." },
            { "drive", "Drive", 0, 100, 0, false, false, "Saturació dins de la repetició: els ecos es van embrutant." },
            { "modrate", "Mod Rate", 0.05f, 10, 0.6f, false, true, "Velocitat de la modulació del temps (Hz)." },
            { "moddepth", "Mod Depth", 0, 100, 0, false, false, "Quantitat de modulació: wow, flutter o chorus als ecos." },
            { "difusio", "Diffusion", 0, 100, 0, false, false, "Difumina els ecos fins que semblen una reverb." },
            { "duckamt", "Ducking", 0, 30, 0, false, false, "Mentre toques, els ecos baixen tants dB; quan pares, tornen a pujar." },
            { "duckthr", "Duck Thr", -60, 0, -30, false, false, "A partir de quin volum de la pista baixen els ecos (dB)." },
            { "duckrel", "Duck Rel", 20, 2000, 250, false, true, "Com de ràpid tornen a pujar els ecos quan pares (ms)." },
            { "amplada", "Width", 0, 200, 100, false, false, "Amplada estèreo dels ecos: 0 % mono, 100 % normal, 200 % molt ample." },
            { "mix", "Mix", 0, 100, 30, false, false, "Quant eco: fins a 50 % l'original sona sencer; 100 % només ecos." },
            { "sortida", "Output", -24, 12, 0, false, false, "Guany de sortida (dB)." },
            { "freeze", "Freeze", 0, 1, 0, true, false, "Congela els ecos que sonen ara i els repeteix sense fi (no hi entra res de nou)." },
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
        std::string hz (double v) { return v >= 1000 ? num (v / 1000.0, 1) + " kHz" : num (v, 0) + " Hz"; }
        /** Lleuger fins a ±1 i després es corba (perquè un feedback alt mai s'escapi). */
        inline float limita (float x) noexcept
        {
            const float a = std::fabs (x);
            if (a <= 1.0f) return x;
            const float y = 1.0f + std::tanh (a - 1.0f);
            return x < 0.0f ? -y : y;
        }
    }

    const char* nomCaracter (int c) { return nomsCar[std::clamp (c, 0, numCaracters - 1)]; }
    const char* descripcioCaracter (int c) { return descCar[std::clamp (c, 0, numCaracters - 1)]; }
    const char* nomEncaminament (int e) { return nomsEnc[std::clamp (e, 0, numEncaminaments - 1)]; }
    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }
    int numNotes() { return nNotes; }
    const char* nomNota (int i) { return notes[std::clamp (i, 0, nNotes - 1)].nom; }
    double notaEnNegres (int i) { return notes[std::clamp (i, 0, nNotes - 1)].negres; }

    std::string textValor (int p, float v)
    {
        switch (p)
        {
            case CaracterP: return nomCaracter ((int) std::lround (v));
            case EncaminamentP: return nomEncaminament ((int) std::lround (v));
            case Nota: return nomNota ((int) std::lround (v));
            case Temps: return v < 10 ? num (v, 1) + " ms" : num (v, 0) + " ms";
            case Desfas: return (v > 0.05f ? "+" : "") + num (v, 0) + " %";
            case Feedback: case Drive: case ModDepth: case Difusio: case Amplada: case Mix: return num (v, 0) + " %";
            case LowCut: return v <= 20.5f ? "Off" : hz (v);
            case HighCut: return v >= 19999.0f ? "Off" : hz (v);
            case ModRate: return num (v, v < 1 ? 2 : 1) + " Hz";
            case DuckAmt: return v < 0.05f ? "Off" : "-" + num (v, 1) + " dB";
            case DuckThr: return num (v, 0) + " dB";
            case DuckRel: return num (v, 0) + " ms";
            case Sortida: return (v > 0.05f ? "+" : "") + num (v, 1) + " dB";
            default: return v > 0.5f ? "Sí" : "No";
        }
    }

    void Delay::Bq::passaBaixos (double f, double fs) noexcept
    {
        const double w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2 * 0.7071), a0 = 1 + al;
        b0 = (float) ((1 - c) / 2 / a0); b1 = (float) ((1 - c) / a0); b2 = b0;
        a1 = (float) (-2 * c / a0); a2 = (float) ((1 - al) / a0);
    }
    void Delay::Bq::passaAlts (double f, double fs) noexcept
    {
        const double w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2 * 0.7071), a0 = 1 + al;
        b0 = (float) ((1 + c) / 2 / a0); b1 = (float) (-(1 + c) / a0); b2 = b0;
        a1 = (float) (-2 * c / a0); a2 = (float) ((1 - al) / a0);
    }
    float Delay::AllPass::operator() (float x, float g) noexcept
    {
        const float d = buf[pos];
        const float v = x + g * d;
        buf[pos] = v;
        if (++pos >= len) pos = 0;
        return d - g * v;
    }

    Delay::Delay()
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def);
    }

    void Delay::prepara (double nouFs, int)
    {
        fs = nouFs > 0.0 ? nouFs : 48000.0;
        int mida = 1;
        while (mida < (int) (fs * (maxSegons + 0.1)) + 8) mida <<= 1;
        for (auto& b : buf) b.assign ((size_t) mida, 0.0f);
        mascara = mida - 1;
        escriu = 0;
        // difusió: 4 all-pass per costat (temps primers entre si, el dret una mica diferent)
        static const double ms[2][4] = { { 4.1, 5.9, 7.7, 11.3 }, { 4.7, 6.3, 8.9, 12.1 } };
        size_t total = 0;
        int lens[2][4];
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k) { lens[c][k] = std::max (1, (int) (ms[c][k] * 0.001 * fs)); total += (size_t) lens[c][k]; }
        difBuf.assign (total, 0.0f);
        size_t o = 0;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k) { dif[c][k] = { 0, lens[c][k], difBuf.data() + o }; o += (size_t) lens[c][k]; }
        for (int c = 0; c < 2; ++c) { hp[c] = lp[c] = lpCar[c] = Bq {}; sh[c] = 0.0f; }
        darrerHp = darrerLp = darrerCar = -1.0f;
        darrerCaracter = -1;
        fos = 1.0f;
        faseLfo = faseFlutter = 0.0f;
        envDuck = 0.0f;
        shComptador = 0;
        primer = true;
    }

    void Delay::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        v = lim (v, infos[p].mn, infos[p].mx);
        if (infos[p].enter) v = (float) std::lround (v);
        par[(size_t) p].store (v, std::memory_order_relaxed);
    }

    float Delay::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }

    float Delay::normal (int p, float v) noexcept
    {
        const auto& f = info (p);
        if (f.log) return lim ((float) (std::log (v / f.mn) / std::log (f.mx / f.mn)), 0.0f, 1.0f);
        return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f);
    }
    float Delay::deNormal (int p, float x) noexcept
    {
        const auto& f = info (p);
        x = lim (x, 0.0f, 1.0f);
        return f.log ? f.mn * std::pow (f.mx / f.mn, x) : f.mn + (f.mx - f.mn) * x;
    }

    InfoParametre Delay::infoParametre (int i) const
    {
        const auto& f = info (i);
        InfoParametre r;
        r.id = f.clau;
        r.nom = f.nom;
        r.passos = f.enter ? (int) (f.mx - f.mn) + 1 : 0;
        r.automatitzable = true;
        return r;
    }
    void Delay::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Delay::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Delay::textParametre (int i, float v01) const
    {
        float v = deNormal (i, v01);
        if (info (i).enter) v = (float) std::lround (v);
        return textValor (i, v);
    }

    std::string Delay::estat() const
    {
        std::string s = "mibase-delay=1";
        char b[64];
        for (int i = 0; i < numParams; ++i)
        {
            std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i));
            s += b;
        }
        return s;
    }

    void Delay::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-delay=", 0) != 0) return;
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

    double Delay::tempsL() const noexcept
    {
        const double t = valor (Sync) > 0.5f ? notaEnNegres ((int) valor (Nota)) * 60.0 / bpm() : valor (Temps) * 0.001;
        return std::clamp (t, 0.001, maxSegons);
    }
    double Delay::tempsR() const noexcept
    {
        return std::clamp (tempsL() * (1.0 + valor (Desfas) * 0.01), 0.001, maxSegons);
    }

    std::vector<Eco> Delay::ecos (int maxEcos, double finsA) const
    {
        std::vector<Eco> v;
        const int enc = (int) valor (EncaminamentP);
        const bool fr = valor (Freeze) > 0.5f;
        const float fb = fr ? 1.0f : valor (Feedback) * 0.01f;
        const int car = (int) valor (CaracterP);
        // cada repetició perd una mica pels filtres i el caràcter (aproximat, només per dibuixar)
        float perdua = 1.0f;
        if (valor (HighCut) < 19999.0f) perdua *= 0.93f;
        if (valor (LowCut) > 20.5f) perdua *= 0.95f;
        if (car == Cinta) perdua *= 0.9f;
        if (car == Analogic) perdua *= 0.88f;
        if (car == LoFi) perdua *= 0.85f;
        if (fr) perdua = 1.0f;
        const float g = std::min (1.0f, fb * perdua);
        const double tl = tempsL(), tr = tempsR();
        if (enc == PingPong)
        {
            // L, R, L, R… (el temps de cada costat el seu)
            double t = 0.0;
            float a = 1.0f;
            for (int k = 0; k < maxEcos; ++k)
            {
                t += (k % 2 == 0) ? tl : tr;
                if (t > finsA || a < 0.01f) break;
                v.push_back ({ t, k % 2 == 0 ? a : 0.0f, k % 2 == 0 ? 0.0f : a });
                a *= g;
            }
            return v;
        }
        // estèreo i mono: els dos costats amb el seu temps (mono: tots dos al mig)
        for (int c = 0; c < (enc == Mono ? 1 : 2); ++c)
        {
            const double dt = c == 0 ? tl : tr;
            float a = 1.0f;
            for (int k = 1; k <= maxEcos; ++k)
            {
                const double t = dt * k;
                if (t > finsA || a < 0.01f) break;
                if (enc == Mono) v.push_back ({ t, a, a });
                else v.push_back ({ t, c == 0 ? a : 0.0f, c == 0 ? 0.0f : a });
                a *= g;
            }
        }
        std::sort (v.begin(), v.end(), [] (const Eco& a, const Eco& b) { return a.temps < b.temps; });
        if ((int) v.size() > maxEcos) v.resize ((size_t) maxEcos);
        return v;
    }

    float Delay::llegeix (int c, double d) const noexcept
    {
        // Hermite de 4 punts (d en mostres, des de la posició d'escriure)
        const auto& b = buf[c];
        double pos = (double) escriu - std::max (3.0, d);
        const int i = (int) std::floor (pos);
        const float f = (float) (pos - i);
        const float xm1 = b[(size_t) ((i - 1) & mascara)], x0 = b[(size_t) (i & mascara)], x1 = b[(size_t) ((i + 1) & mascara)], x2 = b[(size_t) ((i + 2) & mascara)];
        const float c0 = x0, c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }

    void Delay::processa (float* L, float* R, int n) noexcept
    {
        if (buf[0].empty() || n <= 0) return;
        if (demanaBuidar.exchange (false))
        {
            for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
            std::fill (difBuf.begin(), difBuf.end(), 0.0f);
            for (int c = 0; c < 2; ++c) { hp[c].neteja(); lp[c].neteja(); lpCar[c].neteja(); }
        }
        mes.blocs.fetch_add (1, std::memory_order_relaxed);

        const int car = (int) valor (CaracterP), enc = (int) valor (EncaminamentP);
        const bool freeze = valor (Freeze) > 0.5f;
        const double tl = tempsL(), tr = tempsR();
        const double objectiu[2] = { std::max (4.0, tl * fs), std::max (4.0, (enc == Mono ? tl : tr) * fs) };

        // filtres de la repetició (es recalculen només si canvien)
        const float fHp = valor (LowCut), fLp = valor (HighCut);
        float fCar = 20000.0f, hpCar = 20.0f;
        if (car == Cinta) { fCar = 7500.0f; hpCar = 60.0f; }
        if (car == Analogic) fCar = (float) (2600.0 + 6000.0 * (1.0 - std::clamp (tl / 1.2, 0.0, 1.0)));
        if (car == LoFi) { fCar = 3500.0f; hpCar = 250.0f; }
        const float hpEf = std::max (fHp, hpCar);
        if (hpEf != darrerHp || car != darrerCaracter) { for (auto& f : hp) f.passaAlts (hpEf, fs); darrerHp = hpEf; }
        if (fLp != darrerLp) { for (auto& f : lp) f.passaBaixos (fLp, fs); darrerLp = fLp; }
        if (std::fabs (fCar - darrerCar) > 50.0f || car != darrerCaracter) { for (auto& f : lpCar) f.passaBaixos (fCar, fs); darrerCar = fCar; }
        darrerCaracter = car;
        const bool ambHp = hpEf > 20.5f, ambLp = fLp < 19999.0f, ambCar = car != Digital;

        // valors de destí (es suavitzen mostra a mostra)
        const float fbObj = freeze ? 1.0f : valor (Feedback) * 0.01f;
        const float mixObj = valor (Mix) * 0.01f;
        const float outObj = std::pow (10.0f, valor (Sortida) / 20.0f);
        const float drive = valor (Drive) * 0.01f + (car == Cinta ? 0.12f : (car == Analogic ? 0.06f : (car == LoFi ? 0.1f : 0.0f)));
        const float guanyDrive = 1.0f + drive * 6.0f;
        const float amplada = valor (Amplada) * 0.01f;
        const float difG = valor (Difusio) * 0.0065f;   // fins a 0,65
        const float duckAmt = valor (DuckAmt), duckThr = valor (DuckThr);
        const float relDuck = std::exp (-1.0f / (float) (valor (DuckRel) * 0.001 * fs)), atkDuck = std::exp (-1.0f / (float) (0.002 * fs));
        const float sm = 1.0f - std::exp (-1.0f / (float) (0.02 * fs));   // ~20 ms
        // modulació: profunditat en mostres segons el caràcter
        const float baseMs = car == Cinta ? 0.12f : (car == Analogic ? 0.06f : 0.0f);
        const float maxMs = car == Digital ? 3.0f : (car == Cinta ? 4.0f : (car == Analogic ? 3.0f : 5.0f));
        const float profund = (float) ((baseMs + valor (ModDepth) * 0.01f * maxMs) * 0.001 * fs);
        const float incLfo = (float) (valor (ModRate) / fs), incFlutter = (float) ((car == Cinta ? 6.3 : 4.1) / fs);
        const float bits = 64.0f;   // lo-fi: ~7 bits
        const int pasSh = car == LoFi ? 3 : 1;

        if (primer)
        {
            for (int c = 0; c < 2; ++c) dAct[c] = dVell[c] = dNou[c] = objectiu[c];
            sFb = fbObj; sMix = mixObj; sOut = outObj;
            primer = false;
        }
        // Digital: si el temps canvia, fos entre el temps vell i el nou (sense canvi d'afinació); la resta llisca (com una cinta)
        if (car == Digital && fos >= 1.0f && (std::fabs (objectiu[0] - dNou[0]) > 1.0 || std::fabs (objectiu[1] - dNou[1]) > 1.0))
        {
            for (int c = 0; c < 2; ++c) { dVell[c] = dNou[c]; dNou[c] = objectiu[c]; }
            fos = 0.0f;
        }
        const float incFos = (float) (1.0 / (0.03 * fs));
        const double llisca = 1.0 - std::exp (-1.0 / (0.12 * fs));

        float picE = 0.0f, picS = 0.0f, picW = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float xl = L[i], xr = R[i];
            picE = std::max (picE, std::max (std::fabs (xl), std::fabs (xr)));
            sFb += (fbObj - sFb) * sm;
            sMix += (mixObj - sMix) * sm;
            sOut += (outObj - sOut) * sm;

            // modulació (sinus lent + flutter per a la cinta i el lo-fi)
            faseLfo += incLfo; if (faseLfo >= 1.0f) faseLfo -= 1.0f;
            faseFlutter += incFlutter; if (faseFlutter >= 1.0f) faseFlutter -= 1.0f;
            float mod[2] = { 0.0f, 0.0f };
            if (profund > 0.0f)
            {
                const float fl = (car == Cinta || car == LoFi) ? 0.25f * std::sin (2.0f * (float) pi * faseFlutter) : 0.0f;
                mod[0] = profund * (0.5f + 0.5f * std::sin (2.0f * (float) pi * faseLfo) + fl);
                mod[1] = profund * (0.5f + 0.5f * std::sin (2.0f * (float) pi * (faseLfo + 0.25f)) + fl);
            }

            // llegir els ecos
            float y[2];
            for (int c = 0; c < 2; ++c)
            {
                if (car == Digital)
                {
                    if (fos < 1.0f) y[c] = llegeix (c, dVell[c] + mod[c]) * (1.0f - fos) + llegeix (c, dNou[c] + mod[c]) * fos;
                    else y[c] = llegeix (c, dNou[c] + mod[c]);
                    dAct[c] = dNou[c];
                }
                else
                {
                    dAct[c] += (objectiu[c] - dAct[c]) * llisca;
                    dNou[c] = dAct[c];
                    y[c] = llegeix (c, dAct[c] + mod[c]);
                }
            }
            if (fos < 1.0f) fos = std::min (1.0f, fos + incFos);
            if (enc == Mono) y[1] = y[0];

            // el que s'escriu (el que entra + la repetició) passa pels filtres, el caràcter i la saturació:
            // el primer eco ja sona amb el caràcter i cada repetició hi torna a passar. En freeze, intacte.
            float w[2];
            if (freeze) { w[0] = y[0]; w[1] = enc == Mono ? y[0] : y[1]; }
            else
            {
                const float mono = 0.5f * (xl + xr);
                if (enc == PingPong) { w[0] = mono + y[1] * sFb; w[1] = y[0] * sFb; }
                else if (enc == Mono) { w[0] = w[1] = mono + y[0] * sFb; }
                else { w[0] = xl + y[0] * sFb; w[1] = xr + y[1] * sFb; }
                for (int c = 0; c < (enc == Mono ? 1 : 2); ++c)
                {
                    float v = w[c];
                    if (ambHp) v = hp[c] (v);
                    if (ambLp) v = lp[c] (v);
                    if (ambCar) v = lpCar[c] (v);
                    if (drive > 0.001f) v = std::tanh (v * guanyDrive) / guanyDrive;
                    if (car == LoFi)
                    {
                        if (shComptador == 0) sh[c] = std::round (v * bits) / bits;
                        v = sh[c];
                    }
                    w[c] = v;
                }
                if (enc == Mono) w[1] = w[0];
                if (car == LoFi && ++shComptador >= pasSh) shComptador = 0;
            }
            buf[0][(size_t) escriu] = limita (w[0]);
            buf[1][(size_t) escriu] = limita (w[1]);
            escriu = (escriu + 1) & mascara;

            // el so dels ecos: difusió, amplada i ducking
            float el = y[0], er = y[1];
            if (difG > 0.001f)
            {
                for (int k = 0; k < 4; ++k) { el = dif[0][k] (el, difG); er = dif[1][k] (er, difG); }
            }
            const float mid = 0.5f * (el + er), side = 0.5f * (el - er) * amplada;
            el = mid + side; er = mid - side;
            if (duckAmt > 0.05f)
            {
                const float a = std::max (std::fabs (xl), std::fabs (xr));
                envDuck = a > envDuck ? a + (envDuck - a) * atkDuck : a + (envDuck - a) * relDuck;
                const float nivell = envDuck > 1e-6f ? 20.0f * std::log10 (envDuck) : -120.0f;
                const float red = duckAmt * lim ((nivell - duckThr) / 12.0f, 0.0f, 1.0f);
                const float gObj = std::pow (10.0f, -red / 20.0f);
                sDuck += (gObj - sDuck) * (gObj < sDuck ? 0.01f : 0.002f);
            }
            else sDuck += (1.0f - sDuck) * 0.01f;
            el *= sDuck; er *= sDuck;
            picW = std::max (picW, std::max (std::fabs (el), std::fabs (er)));

            const float gSec = std::min (1.0f, 2.0f * (1.0f - sMix)), gHum = std::min (1.0f, 2.0f * sMix);
            float ol = (xl * gSec + el * gHum) * sOut, orr = (xr * gSec + er * gHum) * sOut;
            if (! std::isfinite (ol)) ol = 0.0f;
            if (! std::isfinite (orr)) orr = 0.0f;
            L[i] = ol; R[i] = orr;
            picS = std::max (picS, std::max (std::fabs (ol), std::fabs (orr)));
        }
        mes.entrada.store (std::max (picE, mes.entrada.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.sortida.store (std::max (picS, mes.sortida.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.eco.store (std::max (picW, mes.eco.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.ducking.store (sDuck < 0.999f ? -20.0f * std::log10 (std::max (1e-6f, sDuck)) : 0.0f, std::memory_order_relaxed);
    }
}
