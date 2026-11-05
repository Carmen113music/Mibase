#include "mibase/SinteIntern.h"

#include <algorithm>
#include <cmath>

namespace mibase
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr int subbloc = 32;   // cada quantes mostres es recalcula el filtre i el to

        inline float blep (double t, double dt) noexcept
        {
            if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
            return 0.0f;
        }

        inline float ona (int tipus, double fase, double inc) noexcept
        {
            switch (tipus)
            {
                case 0: return (float) std::sin (2.0 * pi * fase);
                case 1: return (float) (2.0 * fase - 1.0) - blep (fase, inc);
                case 2:
                {
                    float v = fase < 0.5 ? 1.0f : -1.0f;
                    v += blep (fase, inc);
                    double f2 = fase + 0.5; if (f2 >= 1.0) f2 -= 1.0;
                    v -= blep (f2, inc);
                    return v;
                }
                case 3: return (float) (4.0 * std::abs (fase - 0.5) - 1.0);
                default: return 0.0f;
            }
        }

        inline double avanca (double f, double inc) noexcept
        {
            f += inc;
            if (f >= 1.0) f -= std::floor (f);
            return f;
        }
    }

    const SinteIntern::Parametres& SinteIntern::parametres (int so) noexcept
    {
        static const Parametres sons[] = {
            // Piano: FM curta que s'apaga (cop de martell) + una serra suau filtrada.
            [] { Parametres p; p.ona1 = 4; p.fmRati = 1.0f; p.fmIndex = 1.6f; p.fmCaiguda = 0.35f; p.ona2 = 1; p.mix2 = 0.18f; p.cents2 = 3.0f;
                 p.a = 0.002f; p.d = 2.2f; p.s = 0.0f; p.r = 0.35f; p.tall = 2600.0f; p.envFiltre = 1.8f; p.caigudaFiltre = 0.5f;
                 p.sensVel = 0.85f; p.volum = 0.55f; return p; }(),
            // Piano elèctric.
            [] { Parametres p; p.ona1 = 4; p.fmRati = 1.0f; p.fmIndex = 1.1f; p.fmCaiguda = 0.6f; p.ona2 = 0; p.mix2 = 0.25f; p.octava2 = 1.0f;
                 p.a = 0.002f; p.d = 2.6f; p.s = 0.15f; p.r = 0.3f; p.tall = 6000.0f; p.sensVel = 0.8f; p.vibrato = 0.0f; p.volum = 0.5f; return p; }(),
            // Orgue: sinus + octava.
            [] { Parametres p; p.ona1 = 0; p.ona2 = 0; p.mix2 = 0.45f; p.octava2 = 1.0f; p.a = 0.006f; p.d = 0.1f; p.s = 1.0f; p.r = 0.06f;
                 p.tall = 9000.0f; p.sensVel = 0.15f; p.vibrato = 0.08f; p.volum = 0.38f; return p; }(),
            // Pad.
            [] { Parametres p; p.ona1 = 1; p.ona2 = 1; p.mix2 = 0.5f; p.cents2 = 9.0f; p.a = 0.6f; p.d = 1.0f; p.s = 0.8f; p.r = 1.2f;
                 p.tall = 1300.0f; p.ressonancia = 0.15f; p.envFiltre = 0.6f; p.caigudaFiltre = 1.5f; p.sensVel = 0.4f; p.volum = 0.32f; return p; }(),
            // Baix.
            [] { Parametres p; p.ona1 = 1; p.ona2 = 2; p.mix2 = 0.45f; p.octava2 = -1.0f; p.a = 0.003f; p.d = 0.5f; p.s = 0.55f; p.r = 0.08f;
                 p.tall = 420.0f; p.ressonancia = 0.3f; p.envFiltre = 2.4f; p.caigudaFiltre = 0.22f; p.sensVel = 0.7f; p.volum = 0.6f; return p; }(),
            // Pinçat (corda pinçada / pluck).
            [] { Parametres p; p.ona1 = 1; p.ona2 = 3; p.mix2 = 0.3f; p.a = 0.001f; p.d = 0.45f; p.s = 0.0f; p.r = 0.25f;
                 p.tall = 700.0f; p.ressonancia = 0.2f; p.envFiltre = 3.2f; p.caigudaFiltre = 0.14f; p.sensVel = 0.8f; p.volum = 0.5f; return p; }(),
            // Solista.
            [] { Parametres p; p.ona1 = 2; p.ona2 = 1; p.mix2 = 0.4f; p.cents2 = 6.0f; p.a = 0.01f; p.d = 0.25f; p.s = 0.8f; p.r = 0.18f;
                 p.tall = 2400.0f; p.ressonancia = 0.35f; p.envFiltre = 1.2f; p.caigudaFiltre = 0.3f; p.sensVel = 0.5f; p.vibrato = 0.12f; p.volum = 0.33f; return p; }(),
            // Cordes.
            [] { Parametres p; p.ona1 = 1; p.ona2 = 1; p.mix2 = 0.5f; p.cents2 = 13.0f; p.a = 0.28f; p.d = 0.6f; p.s = 0.85f; p.r = 0.6f;
                 p.tall = 2100.0f; p.ressonancia = 0.08f; p.envFiltre = 0.3f; p.caigudaFiltre = 0.8f; p.sensVel = 0.5f; p.vibrato = 0.1f; p.volum = 0.32f; return p; }(),
        };
        return sons[std::clamp (so, 0, (int) (sizeof (sons) / sizeof (sons[0])) - 1)];
    }

    SinteIntern::SinteIntern (int s) { setSo (s); }

    void SinteIntern::setSo (int s) noexcept { so.store (std::clamp (s, 0, 7), std::memory_order_relaxed); }

    void SinteIntern::prepara (double nouFs, int)
    {
        fs = nouFs > 0.0 ? nouFs : 48000.0;
    }

    void SinteIntern::apagaTot() noexcept
    {
        for (auto& v : veus)
            if (v.fase != Fase::Lliure)
            {
                v.fase = Fase::Alliberament;
                v.alliberamentRapid = 1.0f;
                v.sostinguda = false;
            }
        pedal = false;
        bend = 0.0f;
        modulacio = 0.0f;
        volumCC = expressio = brillantor = 1.0f;
    }

    void SinteIntern::allibera (Veu& v) noexcept
    {
        if (v.fase != Fase::Lliure && v.fase != Fase::Alliberament)
            v.fase = Fase::Alliberament;
    }

    void SinteIntern::notaOn (int nota, int vel) noexcept
    {
        Veu* triada = nullptr;
        for (auto& v : veus)
            if (v.fase != Fase::Lliure && v.nota == nota) { triada = &v; break; }   // la mateixa nota: es torna a tocar
        if (triada == nullptr)
            for (auto& v : veus)
                if (v.fase == Fase::Lliure) { triada = &v; break; }
        if (triada == nullptr)
        {
            // Totes ocupades: la més antiga de les que s'apaguen, o la més antiga de totes.
            for (auto& v : veus)
                if (v.fase == Fase::Alliberament && (triada == nullptr || v.ordre < triada->ordre)) triada = &v;
            if (triada == nullptr)
                for (auto& v : veus)
                    if (triada == nullptr || v.ordre < triada->ordre) triada = &v;
        }

        auto& v = *triada;
        const bool nova = v.fase == Fase::Lliure;
        v.so = so.load (std::memory_order_relaxed);
        const auto& p = parametres (v.so);
        v.nota = nota;
        v.vel = std::clamp ((float) vel / 127.0f, 0.0f, 1.0f);
        v.fase = Fase::Atac;
        v.sostinguda = false;
        v.alliberamentRapid = 0.0f;
        v.ordre = ++comptador;
        v.envF = 1.0f;
        v.fm = p.fmIndex * (0.45f + 0.55f * v.vel);
        v.pan = std::clamp ((float) (nota - 60) / 60.0f * 0.35f, -0.35f, 0.35f);
        if (nova)
        {
            v.nivell = 0.0f;
            v.fase1 = 0.0; v.fase2 = 0.25; v.faseM = 0.0;
            v.ic1 = v.ic2 = 0.0f;
        }
    }

    void SinteIntern::notaOff (int nota) noexcept
    {
        for (auto& v : veus)
            if (v.nota == nota && v.fase != Fase::Lliure && v.fase != Fase::Alliberament)
            {
                if (pedal) v.sostinguda = true;
                else allibera (v);
            }
    }

    void SinteIntern::missatge (const EventInstrument& e) noexcept
    {
        const int tipus = e.estat & 0xF0;
        if (tipus == 0x90 && e.dada2 > 0) notaOn (e.dada1 & 0x7F, e.dada2);
        else if (tipus == 0x80 || (tipus == 0x90 && e.dada2 == 0)) notaOff (e.dada1 & 0x7F);
        else if (tipus == 0xB0)
        {
            const float v = (float) e.dada2 / 127.0f;
            switch (e.dada1)
            {
                case 1:  modulacio = v; break;
                case 7:  volumCC = v; break;
                case 11: expressio = v; break;
                case 74: brillantor = 0.25f + 1.75f * v; break;
                case 64:
                    pedal = e.dada2 >= 64;
                    if (! pedal)
                        for (auto& x : veus)
                            if (x.sostinguda) { x.sostinguda = false; allibera (x); }
                    break;
                case 120: case 123: apagaTot(); break;
                case 121: modulacio = 0.0f; volumCC = expressio = brillantor = 1.0f; bend = 0.0f; break;
                default: break;
            }
        }
        else if (tipus == 0xE0)
        {
            const int valor = (e.dada1 & 0x7F) | ((e.dada2 & 0x7F) << 7);
            bend = (float) (valor - 8192) / 8192.0f * 2.0f;
        }
    }

    void SinteIntern::processa (const EventInstrument* events, int numEvents, float* L, float* R, int n) noexcept
    {
        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        int fet = 0;
        for (int k = 0; k < numEvents; ++k)
        {
            const int on = std::clamp (events[k].offset, fet, n);
            if (on > fet) { renderitza (L, R, fet, on); fet = on; }
            missatge (events[k]);
        }
        if (fet < n) renderitza (L, R, fet, n);

        // Limitador suau: per sota de 0,8 no toca res; per sobre, corba fins a 1 (mai satura de cop).
        auto suau = [] (float x) noexcept
        {
            const float a = std::abs (x);
            if (a <= 0.8f) return x;
            const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f);
            return x < 0.0f ? -y : y;
        };
        for (int i = 0; i < n; ++i) { L[i] = suau (L[i]); R[i] = suau (R[i]); }
    }

    void SinteIntern::renderitza (float* L, float* R, int de, int fins) noexcept
    {
        const float objectiu = volumCC * expressio * 0.45f;   // marge: els acords grans no saturen
        actives = 0;
        const double dt = 1.0 / fs;

        for (int ini = de; ini < fins; ini += subbloc)
        {
            const int fi = std::min (fins, ini + subbloc);
            const int m = fi - ini;
            const float vib = (float) std::sin (2.0 * pi * faseVibrato);
            faseVibrato = avanca (faseVibrato, 5.2 * m * dt);
            const float g0 = guanySuau, g1 = guanySuau + (objectiu - guanySuau) * 0.2f;
            guanySuau = g1;

            for (auto& v : veus)
            {
                if (v.fase == Fase::Lliure) continue;
                const auto& p = parametres (v.so);
                const float profVib = (p.vibrato + modulacio * 0.5f) * vib;
                const double semis = (double) v.nota - 69.0 + bend + profVib;
                const double f = 440.0 * std::pow (2.0, semis / 12.0);
                const double inc1 = std::min (0.45, f * dt);
                const double inc2 = std::min (0.45, f * std::pow (2.0, p.octava2 + p.cents2 / 1200.0) * dt);
                const double incM = std::min (0.45, f * p.fmRati * dt);

                // Filtre (TPT de dues etapes), recalculat cada subbloc.
                const double fc = std::clamp ((double) p.tall * brillantor * std::pow (2.0, (double) (p.envFiltre * v.envF)) * (0.55 + 0.45 * v.vel),
                                              30.0, fs * 0.45);
                const float g = (float) std::tan (pi * fc / fs);
                const float kq = 2.0f - 1.9f * std::clamp (p.ressonancia, 0.0f, 0.95f);
                const float a1 = 1.0f / (1.0f + g * (g + kq)), a2 = g * a1, a3 = g * a2;

                const float velAmp = (1.0f - p.sensVel) + p.sensVel * v.vel * v.vel;
                const float pl = (1.0f - std::max (0.0f, v.pan)), pr = (1.0f + std::min (0.0f, v.pan));

                const float cAtac = p.a > 0.0005f ? (float) (dt / p.a) : 1.0f;
                const float cCaiguda = 1.0f - (float) std::exp (-dt / std::max (0.002, (double) p.d * 0.25));
                const float tauAll = v.alliberamentRapid > 0.0f ? 0.004f : std::max (0.003f, p.r * 0.22f);
                const float cAll = (float) std::exp (-dt / tauAll);
                const float cFm = (float) std::exp (-dt / std::max (0.01f, p.fmCaiguda));

                for (int i = ini; i < fi; ++i)
                {
                    switch (v.fase)
                    {
                        case Fase::Atac:
                            v.nivell += cAtac;
                            if (v.nivell >= 1.0f) { v.nivell = 1.0f; v.fase = Fase::Caiguda; }
                            break;
                        case Fase::Caiguda:
                            v.nivell += (p.s - v.nivell) * cCaiguda;
                            if (std::abs (v.nivell - p.s) < 1.0e-4f) { v.nivell = p.s; v.fase = Fase::Sosteniment; }
                            if (p.s <= 0.0f && v.nivell < 1.0e-4f) { v.fase = Fase::Lliure; }
                            break;
                        case Fase::Sosteniment: break;
                        case Fase::Alliberament:
                            v.nivell *= cAll;
                            if (v.nivell < 1.0e-4f) v.fase = Fase::Lliure;
                            break;
                        case Fase::Lliure: break;
                    }
                    if (v.fase == Fase::Lliure) { v.nivell = 0.0f; v.nota = -1; break; }

                    float o1;
                    if (p.ona1 == 4)
                    {
                        const double mod = v.fm * std::sin (2.0 * pi * v.faseM);
                        o1 = (float) std::sin (2.0 * pi * v.fase1 + mod);
                        v.faseM = avanca (v.faseM, incM);
                        v.fm *= cFm;
                    }
                    else o1 = ona (p.ona1, v.fase1, inc1);
                    const float o2 = p.mix2 > 0.0f ? ona (p.ona2 == 4 ? 0 : p.ona2, v.fase2, inc2) : 0.0f;
                    v.fase1 = avanca (v.fase1, inc1);
                    v.fase2 = avanca (v.fase2, inc2);
                    const float x = o1 * (1.0f - 0.5f * p.mix2) + o2 * p.mix2;

                    // Filtre passa-baixos.
                    const float v3 = x - v.ic2;
                    const float v1 = a1 * v.ic1 + a2 * v3;
                    const float v2 = v.ic2 + a2 * v.ic1 + a3 * v3;
                    v.ic1 = 2.0f * v1 - v.ic1;
                    v.ic2 = 2.0f * v2 - v.ic2;

                    const float gain = g0 + (g1 - g0) * (float) (i - ini) / (float) m;
                    const float y = v2 * v.nivell * velAmp * p.volum * gain;
                    L[i] += y * pl;
                    R[i] += y * pr;
                }
                if (v.fase != Fase::Lliure)
                {
                    v.envF *= (float) std::exp (-(double) m * dt / std::max (0.01f, p.caigudaFiltre));
                    if (! std::isfinite (v.ic1) || ! std::isfinite (v.ic2)) { v.ic1 = v.ic2 = 0.0f; }
                }
            }
        }
        for (auto& v : veus) if (v.fase != Fase::Lliure) ++actives;
    }
}
