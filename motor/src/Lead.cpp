#include "mibase/Lead.h"
#include "mibase/FFT.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>

namespace mibase::lead
{
    using sinte::tanhR; using sinte::blep; using sinte::coef; using sinte::num; using sinte::ms;
    using sinte::Lineal; using sinte::Enter; using sinte::Log; using sinte::Cub;

    namespace
    {
        constexpr float pi = 3.14159265358979f;
        constexpr double dpi = 3.14159265358979323846;
        const char* const ones = "Sinus|Triangle|Serra|Quadrada|Pols|Supersaw|Wavetable|Soroll";
        const char* const taules = "Bàsica|PWM|Vocal|Harmònics|Digital|Campana|Sync|Metall";
        const char* const onesLfo = "Sinus|Triangle|Serra|Quadrada|Atzar";
        const char* const destins = "To|Tall|Forma|Volum|FM|Pan";
        const char* const noSi = "No|Sí";
        const char* const notesDelay = "1/16|1/8T|1/8|1/8.|1/4T|1/4|1/4.|1/2";
        const int notaDelay[] = { 2, 3, 5, 7, 6, 8, 10, 11 };   // índex de delay::nomNota

        const sinte::InfoParam infos[numParams] = {
            { "o1w", "Ona", SOsc, 0, 7, 2, Enter, ones, "Forma d'ona de l'oscil·lador 1" },
            { "o1o", "Octava", SOsc, -3, 3, 0, Enter, nullptr, "Octava de l'oscil·lador 1" },
            { "o1s", "Semitò", SOsc, -12, 12, 0, Enter, nullptr, "Semitons de l'oscil·lador 1" },
            { "o1f", "Fi", SOsc, -100, 100, 0, Lineal, nullptr, "Afinació fina de l'oscil·lador 1 (centèsimes)" },
            { "o1sh", "Forma", SOsc, 0, 1, 0, Lineal, nullptr, "Pols: amplada. Supersaw: obertura. Wavetable: posició. Serra: brillantor. Sinus: plec. Triangle: inclinació" },
            { "o1t", "Taula", SOsc, 0, 7, 0, Enter, taules, "Wavetable de l'oscil·lador 1" },
            { "o2w", "Ona", SOsc, 0, 7, 2, Enter, ones, "Forma d'ona de l'oscil·lador 2" },
            { "o2o", "Octava", SOsc, -3, 3, 0, Enter, nullptr, "Octava de l'oscil·lador 2" },
            { "o2s", "Semitò", SOsc, -12, 12, 0, Enter, nullptr, "Semitons de l'oscil·lador 2" },
            { "o2f", "Fi", SOsc, -100, 100, 7, Lineal, nullptr, "Afinació fina de l'oscil·lador 2: el batec amb l'1" },
            { "o2sh", "Forma", SOsc, 0, 1, 0, Lineal, nullptr, "Pols: amplada. Supersaw: obertura. Wavetable: posició. Serra: brillantor. Sinus: plec. Triangle: inclinació" },
            { "o2t", "Taula", SOsc, 0, 7, 0, Enter, taules, "Wavetable de l'oscil·lador 2" },
            { "o3w", "Ona", SOsc, 0, 7, 3, Enter, ones, "Forma d'ona de l'oscil·lador 3" },
            { "o3o", "Octava", SOsc, -3, 3, -1, Enter, nullptr, "Octava de l'oscil·lador 3" },
            { "o3s", "Semitò", SOsc, -12, 12, 0, Enter, nullptr, "Semitons de l'oscil·lador 3" },
            { "o3f", "Fi", SOsc, -100, 100, 0, Lineal, nullptr, "Afinació fina de l'oscil·lador 3" },
            { "o3sh", "Forma", SOsc, 0, 1, 0, Lineal, nullptr, "Pols: amplada. Supersaw: obertura. Wavetable: posició. Serra: brillantor. Sinus: plec. Triangle: inclinació" },
            { "o3t", "Taula", SOsc, 0, 7, 0, Enter, taules, "Wavetable de l'oscil·lador 3" },
            { "sync2", "Sync 2", SOsc, 0, 1, 0, Enter, noSi, "Hard sync: l'oscil·lador 2 torna a començar a cada cicle de l'1 (puja l'octava o els semitons del 2)" },
            { "sync3", "Sync 3", SOsc, 0, 1, 0, Enter, noSi, "Hard sync de l'oscil·lador 3 amb l'1" },
            { "fm12", "FM 1→2", SOsc, 0, 1, 0, Lineal, nullptr, "L'oscil·lador 1 modula la fase del 2" },
            { "fm31", "FM 3→1", SOsc, 0, 1, 0, Lineal, nullptr, "L'oscil·lador 3 modula la fase de l'1" },
            { "uni", "Unison", SOsc, 1, 7, 1, Enter, nullptr, "Còpies de cada oscil·lador (el supersaw ja en porta 7)" },
            { "unid", "Detune", SOsc, 0, 1, 0.25f, Lineal, nullptr, "Desafinació de les còpies de l'unison" },
            { "unia", "Obertura", SOsc, 0, 1, 0.6f, Lineal, nullptr, "Com s'obren les còpies a esquerra i dreta" },
            { "lliure", "Fase lliure", SOsc, 0, 1, 1, Enter, noSi, "Sí: els oscil·ladors no es reinicien a cada nota (analògic). No: cada nota comença igual (atac definit)" },
            { "deriva", "Deriva", SOsc, 0, 1, 0, Lineal, nullptr, "Imperfeccions analògiques: cada veu es desafina i es filtra una mica diferent i es mou sola" },
            { "n1", "Osc 1", SMix, 0, 1, 0.8f, Lineal, nullptr, "Volum de l'oscil·lador 1" },
            { "n2", "Osc 2", SMix, 0, 1, 0.6f, Lineal, nullptr, "Volum de l'oscil·lador 2" },
            { "n3", "Osc 3", SMix, 0, 1, 0, Lineal, nullptr, "Volum de l'oscil·lador 3" },
            { "subn", "Sub", SMix, 0, 1, 0, Lineal, nullptr, "Volum del sub-oscil·lador" },
            { "subo", "Sub oct", SMix, 0, 1, 0, Enter, "-1 oct|-2 oct", "Octava del sub" },
            { "subw", "Sub ona", SMix, 0, 1, 0, Enter, "Sinus|Quadrada", "Ona del sub" },
            { "nz", "Soroll", SMix, 0, 1, 0, Lineal, nullptr, "Soroll: aire, buf" },
            { "nzc", "Color", SMix, 0, 1, 0.3f, Lineal, nullptr, "Color del soroll: blanc (0) o fosc (1)" },
            { "ring", "Ring", SMix, 0, 1, 0, Lineal, nullptr, "Modulació en anell (osc 1 × osc 2): metàl·lic i dissonant" },
            { "ft", "Tipus", SFiltre, 0, 3, 0, Enter, "LP|HP|BP|Ladder", "LP: passa-baixos. HP: passa-alts. BP: passa-banda. Ladder: passa-baixos analògic, rodó i saturat" },
            { "fp", "Pendent", SFiltre, 0, 2, 1, Enter, "12 dB|24 dB|36 dB", "Com de fort talla el filtre" },
            { "cut", "Tall", SFiltre, 30, 18000, 2400, Log, nullptr, "Freqüència de tall" },
            { "res", "Ressonància", SFiltre, 0, 1, 0.2f, Lineal, nullptr, "Ressonància: realça el tall" },
            { "fenv", "Envolupant", SFiltre, -1, 1, 0.3f, Lineal, nullptr, "Quant mou el tall l'envolupant del filtre" },
            { "key", "Seguiment", SFiltre, 0, 1, 0.5f, Lineal, nullptr, "Seguiment de teclat: les notes agudes obren més" },
            { "fdrv", "Saturació", SFiltre, 0, 1, 0.1f, Lineal, nullptr, "Saturació a l'entrada del filtre" },
            { "aa", "A", SEnv, 0.3f, 5000, 3, Log, nullptr, "Atac del volum" },
            { "ad", "D", SEnv, 5, 8000, 500, Log, nullptr, "Caiguda del volum" },
            { "as", "S", SEnv, 0, 1, 0.85f, Lineal, nullptr, "Sostingut del volum" },
            { "ar", "R", SEnv, 3, 8000, 250, Log, nullptr, "Alliberament del volum" },
            { "fa", "A", SEnv, 0.3f, 5000, 3, Log, nullptr, "Atac del filtre" },
            { "fd", "D", SEnv, 5, 8000, 600, Log, nullptr, "Caiguda del filtre" },
            { "fs", "S", SEnv, 0, 1, 0.4f, Lineal, nullptr, "Sostingut del filtre" },
            { "fr", "R", SEnv, 3, 8000, 300, Log, nullptr, "Alliberament del filtre" },
            { "l1w", "Ona", SLfo, 0, 4, 0, Enter, onesLfo, "Forma de l'LFO 1" },
            { "l1r", "Velocitat", SLfo, 0.05f, 30, 5.5f, Log, nullptr, "Velocitat de l'LFO 1 (si no va amb el tempo)" },
            { "l1s", "Tempo", SLfo, 0, 11, 0, Enter, sinte::ritmesLfo, "LFO 1 lliure o amb el tempo" },
            { "l1d", "Destí", SLfo, 0, 5, 0, Enter, destins, "Què mou l'LFO 1" },
            { "l1q", "Quantitat", SLfo, 0, 1, 0, Lineal, nullptr, "Quant mou l'LFO 1" },
            { "l1del", "Retard", SLfo, 0, 3000, 0, Cub, nullptr, "L'LFO 1 entra a poc a poc després de la nota (vibrato expressiu)" },
            { "l1ret", "Reinicia", SLfo, 0, 1, 0, Enter, noSi, "L'LFO 1 torna a començar a cada nota nova" },
            { "l2w", "Ona", SLfo, 0, 4, 1, Enter, onesLfo, "Forma de l'LFO 2" },
            { "l2r", "Velocitat", SLfo, 0.05f, 30, 0.4f, Log, nullptr, "Velocitat de l'LFO 2" },
            { "l2s", "Tempo", SLfo, 0, 11, 0, Enter, sinte::ritmesLfo, "LFO 2 lliure o amb el tempo" },
            { "l2d", "Destí", SLfo, 0, 5, 2, Enter, destins, "Què mou l'LFO 2" },
            { "l2q", "Quantitat", SLfo, 0, 1, 0, Lineal, nullptr, "Quant mou l'LFO 2" },
            { "l2del", "Retard", SLfo, 0, 3000, 0, Cub, nullptr, "L'LFO 2 entra a poc a poc" },
            { "l2ret", "Reinicia", SLfo, 0, 1, 0, Enter, noSi, "L'LFO 2 torna a començar a cada nota nova" },
            { "dt", "Tipus", SFx, 0, 3, 1, Enter, "Saturació|Overdrive|Distorsió|Fuzz", "Tipus de distorsió (a cada veu, després del filtre)" },
            { "dq", "Drive", SFx, 0, 1, 0, Lineal, nullptr, "Quanta distorsió" },
            { "dto", "To", SFx, 0, 1, 0.65f, Lineal, nullptr, "To de la distorsió" },
            { "chm", "Chorus", SFx, 0, 1, 0, Lineal, nullptr, "Quantitat de chorus" },
            { "chr", "Vel. chorus", SFx, 0.1f, 5, 0.7f, Log, nullptr, "Velocitat del chorus" },
            { "chp", "Prof. chorus", SFx, 0, 1, 0.5f, Lineal, nullptr, "Profunditat del chorus" },
            { "dlt", "Temps", SFx, 0, 7, 3, Enter, notesDelay, "Temps del delay, amb el tempo del projecte" },
            { "dlf", "Feedback", SFx, 0, 0.92f, 0.35f, Lineal, nullptr, "Repeticions del delay" },
            { "dlm", "Delay", SFx, 0, 1, 0, Lineal, nullptr, "Quantitat de delay" },
            { "dlpp", "Ping-pong", SFx, 0, 1, 1, Enter, noSi, "Els ecos van d'un costat a l'altre" },
            { "rvs", "Mida", SFx, 0, 1, 0.5f, Lineal, nullptr, "Mida i durada de la reverb" },
            { "rvm", "Reverb", SFx, 0, 1, 0, Lineal, nullptr, "Quantitat de reverb" },
            { "mwv", "Roda → vib", SMod, 0, 1, 0.5f, Lineal, nullptr, "La roda de modulació fa vibrato" },
            { "mwc", "Roda → tall", SMod, -1, 1, 0, Lineal, nullptr, "La roda de modulació obre o tanca el filtre" },
            { "atv", "AT → vib", SMod, 0, 1, 0.4f, Lineal, nullptr, "L'aftertouch (pressió de la tecla) fa vibrato" },
            { "atc", "AT → tall", SMod, -1, 1, 0.2f, Lineal, nullptr, "L'aftertouch obre o tanca el filtre" },
            { "vv", "Vel → vol", SMod, 0, 1, 0.4f, Lineal, nullptr, "Com canvia el volum amb la força de la nota" },
            { "vf", "Vel → filt", SMod, 0, 1, 0.3f, Lineal, nullptr, "Com obre el filtre la força de la nota" },
            { "bend", "Bend", SMod, 0, 24, 2, Enter, nullptr, "Rang del pitch bend (semitons)" },
            { "pe", "Env. to", SMod, -24, 24, 0, Lineal, nullptr, "La nota comença més aguda (o greu) i va al to" },
            { "pet", "Temps to", SMod, 2, 2000, 60, Log, nullptr, "Quant triga l'envolupant de to" },
            { "vibr", "Vel. vib", SMod, 2, 9, 5.5f, Lineal, nullptr, "Velocitat del vibrato de la roda i de l'aftertouch" },
            { "mode", "Mode", SVeu, 0, 2, 1, Enter, "Mono|Legato|Poli", "Mono: una nota i torna a atacar. Legato: les lligades llisquen sense atacar. Poli: acords (8 veus)" },
            { "glide", "Glide", SVeu, 0, 2000, 0, Cub, nullptr, "Temps del portament entre notes" },
            { "vol", "Volum", SVeu, -24, 6, -4, Lineal, nullptr, "Volum de sortida" },
            { "width", "Amplada", SVeu, 0, 1.5f, 1, Lineal, nullptr, "Amplada estèreo" },
            { "arp", "Arp", SArp, 0, 7, 0, Enter, sinte::modesArp, "Arpegiador / seqüenciador: amb una tecla o un acord premut, toca sol amb el tempo" },
            { "arpr", "Ritme", SArp, 0, 5, 3, Enter, sinte::ritmesArp, "Durada de cada pas" },
            { "arpo", "Octaves", SArp, 1, 4, 1, Enter, nullptr, "Octaves que recorre" },
            { "arpg", "Gate", SArp, 0.05f, 1, 0.5f, Lineal, nullptr, "Durada de cada nota (al màxim, lligades)" },
            { "arpsw", "Swing", SArp, 0, 0.6f, 0, Lineal, nullptr, "Swing" },
            { "arpp", "Patró", SArp, 0, 15, 0, Enter, sinte::nomsPatrons, "Patró del seqüenciador (mode «Patró»)" },
        };
    }

    const sinte::InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }

    std::string textValor (int p, float v)
    {
        const auto& i = info (p);
        if (i.opcions != nullptr)
        {
            const auto o = sinte::opcions (i);
            return o[(size_t) std::clamp ((int) std::lround (v - i.mn), 0, (int) o.size() - 1)];
        }
        switch (p)
        {
            case O1Oct: case O2Oct: case O3Oct: return (v > 0 ? "+" : "") + num (v, 0) + " oct";
            case O1Semi: case O2Semi: case O3Semi: return (v > 0 ? "+" : "") + num (v, 0) + " st";
            case BendRang: return num (v, 0) + " st";
            case O1Fi: case O2Fi: case O3Fi: return (v > 0 ? "+" : "") + num (v, 0) + " ct";
            case PEnvQuant: return (v > 0 ? "+" : "") + num (v, 1) + " st";
            case UniDetune: return num (v * v * 60.0, 0) + " ct";
            case Unison: return v < 1.5f ? std::string ("Off") : num (v, 0) + " veus";
            case FTall: return v < 1000.0f ? num (v, 0) + " Hz" : num (v / 1000.0, v < 10000.0f ? 2 : 1) + " kHz";
            case L1Vel: case L2Vel: case ChVel: return num (v, v < 10.0f ? 2 : 1) + " Hz";
            case VibVel: return num (v, 1) + " Hz";
            case AA: case AD: case AR: case FA: case FD: case FR: case PEnvTemps: return ms (v);
            case Glide: case L1Retard: case L2Retard: return v < 0.5f ? std::string ("Off") : ms (v);
            case Volum: return (v > 0 ? "+" : "") + num (v, 1) + " dB";
            case FEnv: case RodaTall: case AtTall: return (v > 0 ? "+" : "") + num (v * 100.0, 0) + " %";
            case ArpOct: return num (v, 0) + " oct";
            case ArpGate: return v >= 0.99f ? std::string ("Lligat") : num (v * 100.0, 0) + " %";
            default: return num (v * 100.0, 0) + " %";
        }
    }

    //==========================================================================
    namespace wt
    {
        namespace
        {
            std::vector<float>& dades() { static std::vector<float> d; return d; }
            std::once_flag fet;
            inline size_t index (int t, int f, int l) { return (((size_t) t * marcs + (size_t) f) * nivells + (size_t) l) * mida; }

            void construeix()
            {
                auto& d = dades();
                d.assign ((size_t) numTaules * marcs * nivells * mida, 0.0f);
                fft::FFT fft (mida);
                std::vector<fft::C> S ((size_t) mida), Y ((size_t) mida);
                std::vector<double> amp (mida / 2 + 1);
                for (int t = 0; t < numTaules; ++t)
                    for (int f = 0; f < marcs; ++f)
                    {
                        const double x = (double) f / (marcs - 1);
                        bool temps = false;
                        std::vector<float> td ((size_t) mida);
                        std::fill (amp.begin(), amp.end(), 0.0);
                        auto serra = [] (int h) { return (h % 2 ? 1.0 : -1.0) * 2.0 / (dpi * h); };
                        auto tri = [] (int h) { return h % 2 ? ((h / 2) % 2 ? -1.0 : 1.0) * 8.0 / (dpi * dpi * h * h) : 0.0; };
                        auto quad = [] (int h) { return h % 2 ? 4.0 / (dpi * h) : 0.0; };
                        switch (t)
                        {
                            case 0:   // sinus → triangle → serra → quadrada
                                for (int h = 1; h < mida / 2; ++h)
                                {
                                    const double si = h == 1 ? 1.0 : 0.0;
                                    if (x < 1.0 / 3.0) { const double k = x * 3.0; amp[(size_t) h] = si * (1 - k) + tri (h) * k; }
                                    else if (x < 2.0 / 3.0) { const double k = x * 3.0 - 1.0; amp[(size_t) h] = tri (h) * (1 - k) + serra (h) * k; }
                                    else { const double k = x * 3.0 - 2.0; amp[(size_t) h] = serra (h) * (1 - k) + quad (h) * k; }
                                }
                                break;
                            case 1:   // PWM
                            {
                                temps = true;
                                const double w = 0.5 - 0.46 * x;
                                for (int i = 0; i < mida; ++i) td[(size_t) i] = (double) i / mida < w ? 1.0f : -1.0f;
                                break;
                            }
                            case 2:   // vocal: a, e, i, o, u
                            {
                                const double fm[5][2] = { { 8, 13 }, { 4, 20 }, { 3, 25 }, { 5, 9 }, { 3, 7 } };
                                const double q = x * 4.0;
                                const int a = std::min (3, (int) q);
                                const double k = q - a;
                                const double f1 = fm[a][0] * (1 - k) + fm[a + 1][0] * k, f2 = fm[a][1] * (1 - k) + fm[a + 1][1] * k;
                                for (int h = 1; h < mida / 2; ++h)
                                {
                                    const double g = std::exp (-0.5 * std::pow ((h - f1) / 2.2, 2.0)) + 0.6 * std::exp (-0.5 * std::pow ((h - f2) / 3.2, 2.0)) + 0.04;
                                    amp[(size_t) h] = g / std::pow (h, 0.6);
                                }
                                break;
                            }
                            case 3:   // harmònics que van entrant
                            {
                                const double N = 1.0 + x * x * 60.0;
                                for (int h = 1; h < mida / 2; ++h) amp[(size_t) h] = (1.0 / h) * std::clamp (N + 1.0 - h, 0.0, 1.0);
                                break;
                            }
                            case 4:   // digital
                                for (int h = 1; h < mida / 2; ++h) amp[(size_t) h] = std::abs (std::sin (h * (0.7 + 5.0 * x))) / std::pow (h, 0.8) * (h <= 128 ? 1.0 : 0.3);
                                break;
                            case 5:   // campana
                            {
                                const int pas = 3 + (int) (x * 5.0);
                                for (int h = 1; h < mida / 2; ++h) amp[(size_t) h] = h == 1 ? 1.0 : (h % pas == 1 ? 0.8 / std::sqrt ((double) h) : 0.0);
                                break;
                            }
                            case 6:   // sync
                            {
                                temps = true;
                                const double r = 1.0 + x * 7.0;
                                for (int i = 0; i < mida; ++i) { double ph = r * i / mida; ph -= std::floor (ph); td[(size_t) i] = (float) (2.0 * ph - 1.0); }
                                break;
                            }
                            default:  // metall (FM)
                            {
                                temps = true;
                                const double I = x * 5.0;
                                for (int i = 0; i < mida; ++i) { const double tt = (double) i / mida; td[(size_t) i] = (float) std::sin (2.0 * dpi * tt + I * std::sin (2.0 * dpi * 3.0 * tt)); }
                                break;
                            }
                        }
                        if (temps)
                        {
                            for (int i = 0; i < mida; ++i) S[(size_t) i] = { td[(size_t) i], 0.0f };
                            fft.fa (S.data(), false);
                        }
                        else
                        {
                            for (auto& c : S) c = { 0.0f, 0.0f };
                            for (int h = 1; h < mida / 2; ++h) S[(size_t) h] = { 0.0f, (float) (-amp[(size_t) h] * mida / 2.0) };
                        }
                        float escala = 1.0f;
                        for (int l = 0; l < nivells; ++l)
                        {
                            const int H = std::min (mida / 2 - 1, (mida / 2) >> l);
                            for (auto& c : Y) c = { 0.0f, 0.0f };
                            for (int h = 1; h <= H; ++h)
                            {
                                Y[(size_t) h] = S[(size_t) h];
                                Y[(size_t) (mida - h)] = { S[(size_t) h].r, -S[(size_t) h].i };
                            }
                            fft.fa (Y.data(), true);
                            float* out = d.data() + index (t, f, l);
                            if (l == 0)
                            {
                                float pic = 1e-9f;
                                for (int i = 0; i < mida; ++i) pic = std::max (pic, std::abs (Y[(size_t) i].r));
                                escala = 1.0f / pic;
                            }
                            for (int i = 0; i < mida; ++i) out[i] = Y[(size_t) i].r * escala;
                        }
                    }
            }
        }

        const char* nomTaula (int t)
        {
            static const char* n[] = { "Bàsica", "PWM", "Vocal", "Harmònics", "Digital", "Campana", "Sync", "Metall" };
            return n[std::clamp (t, 0, numTaules - 1)];
        }

        void prepara() { std::call_once (fet, construeix); }

        float llegeix (int taula, double fase, float pos, double dt) noexcept
        {
            const auto& d = dades();
            if (d.empty()) return 0.0f;
            const double hmax = 0.5 / std::max (1e-6, dt);
            int l = 0;
            while (l < nivells - 1 && (double) ((mida / 2) >> l) > hmax) ++l;
            const float fp = std::clamp (pos, 0.0f, 1.0f) * (marcs - 1);
            const int f0 = std::min (marcs - 2, (int) fp);
            const float fr = fp - f0;
            const double x = fase * mida;
            const int i0 = (int) x & (mida - 1), i1 = (i0 + 1) & (mida - 1);
            const float k = (float) (x - std::floor (x));
            const float* a = d.data() + index (std::clamp (taula, 0, numTaules - 1), f0, l);
            const float* b = a + (size_t) nivells * mida;
            const float va = a[i0] + (a[i1] - a[i0]) * k, vb = b[i0] + (b[i1] - b[i0]) * k;
            return va + (vb - va) * fr;
        }
    }

    //==========================================================================
    Lead::Lead() : sinte::Base (infos, numParams, "mibase-lead", { ArpMode, ArpRitme, ArpOct, ArpGate, ArpSwing, ArpPatro })
    {
        wt::prepara();
        assignaCC (74, FTall);
        assignaCC (71, FRes);
        dl.posaTempo (&sinte::tempo());
        rv.posaTempo (&sinte::tempo());
        dl.posa (delay::Sync, 1.0f);
        dl.posa (delay::CaracterP, (float) delay::Digital);
        dl.posa (delay::HighCut, 7000.0f);
        dl.posa (delay::LowCut, 150.0f);
        rv.posa (reverb::Alg, (float) reverb::Plate);
        rv.posa (reverb::PreDelay, 15.0f);
        rv.posa (reverb::LowCut, 200.0f);
    }

    std::string Lead::nomComplet (int i) const
    {
        static const char* seccions[] = { "Osc", "Mix", "Filtre", "Env", "LFO", "FX", "Mod", "Veu", "Arp" };
        const auto& x = info (i);
        if (i >= O1Ona && i <= O1Taula) return std::string ("Osc 1 · ") + x.nom;
        if (i >= O2Ona && i <= O2Taula) return std::string ("Osc 2 · ") + x.nom;
        if (i >= O3Ona && i <= O3Taula) return std::string ("Osc 3 · ") + x.nom;
        if (i >= AA && i <= AR) return std::string ("Env volum · ") + x.nom;
        if (i >= FA && i <= FR) return std::string ("Env filtre · ") + x.nom;
        if (i >= L1Ona && i <= L1Reinicia) return std::string ("LFO 1 · ") + x.nom;
        if (i >= L2Ona && i <= L2Reinicia) return std::string ("LFO 2 · ") + x.nom;
        return std::string (seccions[x.seccio]) + " · " + x.nom;
    }

    void Lead::preparaSo (double f)
    {
        fsI = f * sobre;
        for (auto& v : veus) v = Veu();
        delL.buida(); delR.buida();
        numPila = 0;
        dcL = dcR = dcxL = dcxR = 0.0f;
        chL.assign ((size_t) (f * 0.05) + 4, 0.0f);
        chR.assign ((size_t) (f * 0.05) + 4, 0.0f);
        chPos = 0;
        dl.prepara (f, tros);
        rv.prepara (f, tros);
        for (auto& x : darrerDl) x = -1.0f;
        for (auto& x : darrerRv) x = -1.0f;
    }

    bool Lead::veusSonen() const noexcept
    {
        for (const auto& v : veus) if (v.viva) return true;
        return valor (DlMix) > 0.001f || valor (RvMix) > 0.001f ? nivell() > 1e-4f : false;
    }
    int Lead::numVeusActives() const noexcept { int n = 0; for (const auto& v : veus) n += v.viva ? 1 : 0; return n; }
    void Lead::apagaVeus() noexcept
    {
        for (auto& v : veus) if (v.viva) { v.etapaA = 4; v.tecla = false; v.etapaF = 3; }
        numPila = 0;
    }

    // ---- notes ----
    void Lead::dispara (Veu& v, int nota, float vel, bool retriga, bool glide) noexcept
    {
        const bool estava = v.viva && v.envA > 1e-4f;
        bool algunaTecla = false;
        for (const auto& w : veus) if (w.tecla && w.viva) algunaTecla = true;
        if (! algunaTecla)
        {
            tRetard = 0.0f;   // els LFO amb retard tornen a entrar
            if (valor (L1Reinicia) > 0.5f) fase1 = 0.0;
            if (valor (L2Reinicia) > 0.5f) fase2 = 0.0;
        }
        v.nota = nota;
        v.objectiu = nota;
        if (! glide || ! v.viva) v.semis = nota;
        v.vel = vel;
        v.tecla = true;
        v.ordre = ++comptaOrdre;
        if (retriga)
        {
            v.etapaA = 1;
            v.etapaF = 1;
            v.tPEnv = 0.0f;
            if (! estava)
            {
                v.envA = 0.0f;
                v.envF = 0.0f;
                const bool lliure = valor (FaseLliure) > 0.5f;
                for (int o = 0; o < 3; ++o)
                    for (int k = 0; k < maxUnison; ++k)
                    {
                        double ph = lliure ? 0.5 + 0.5 * aleat() : k * 0.1373 + o * 0.31;
                        v.ph[o][k] = ph - std::floor (ph);
                    }
                v.fSub = 0.0;
                for (int d = 0; d < 4; ++d) { v.deriva[d] = aleat(); v.derivaObj[d] = aleat(); }
                for (auto& f : v.fl) f.buida();
                v.drvTo[0] = v.drvTo[1] = 0.0f;
            }
        }
        v.viva = true;
        posaHz ((float) (440.0 * std::pow (2.0, (nota - 69) / 12.0)));
    }

    void Lead::allibera (Veu& v) noexcept
    {
        v.tecla = false;
        if (v.etapaA != 0 && v.etapaA != 4) v.etapaA = 3;
        if (v.etapaF != 0) v.etapaF = 3;
    }

    void Lead::notaOn (int nota, int vel) noexcept
    {
        const float vf = std::clamp (vel / 127.0f, 0.0f, 1.0f);
        const int mode = (int) valor (Mode);
        const bool ambGlide = valor (Glide) > 0.5f;
        if (mode == 2)
        {
            Veu* tria = nullptr;
            for (auto& v : veus) if (! v.viva) { tria = &v; break; }
            if (tria == nullptr) for (auto& v : veus) if (! v.tecla && (tria == nullptr || v.envA < tria->envA)) tria = &v;
            if (tria == nullptr) for (auto& v : veus) if (tria == nullptr || v.ordre < tria->ordre) tria = &v;
            const bool g = ambGlide && darreraNota >= 0.0;
            if (g && ! tria->viva) { tria->semis = darreraNota; tria->viva = true; tria->envA = 0.0f; tria->etapaA = 0; }
            dispara (*tria, nota, vf, true, g);
            darreraNota = nota;
            return;
        }
        for (int i = 0; i < numPila; ++i)
            if (pila[i] == nota) { std::memmove (pila + i, pila + i + 1, sizeof (int) * (size_t) (numPila - i - 1)); --numPila; break; }
        const bool lligada = numPila > 0;
        if (numPila < 32) pila[numPila++] = nota;
        for (int i = 1; i < maxVeus; ++i) if (veus[i].viva) { veus[i].etapaA = 4; veus[i].tecla = false; }
        auto& v = veus[0];
        if (mode == 1) dispara (v, nota, lligada ? v.vel : vf, ! lligada, lligada);
        else dispara (v, nota, vf, true, ambGlide && v.viva && v.envA > 1e-4f);
        darreraNota = nota;
    }

    void Lead::notaOff (int nota) noexcept
    {
        const int mode = (int) valor (Mode);
        if (mode == 2)
        {
            for (auto& v : veus) if (v.viva && v.tecla && v.nota == nota) allibera (v);
            return;
        }
        for (int i = 0; i < numPila; ++i)
            if (pila[i] == nota) { std::memmove (pila + i, pila + i + 1, sizeof (int) * (size_t) (numPila - i - 1)); --numPila; break; }
        auto& v = veus[0];
        if (v.nota != nota) return;
        if (numPila > 0) dispara (v, pila[numPila - 1], v.vel, mode == 0, mode == 1 || valor (Glide) > 0.5f);
        else allibera (v);
    }

    // ---- so ----
    float Lead::valorLfo (int o, double ph, float sh) const noexcept
    {
        switch (o)
        {
            case 0: return std::sin (2.0f * pi * (float) ph);
            case 1: return 1.0f - 4.0f * std::abs ((float) ph - 0.5f);
            case 2: return 1.0f - 2.0f * (float) ph;
            case 3: return ph < 0.5 ? 1.0f : -1.0f;
            default: return sh;
        }
    }

    void Lead::renderitza (float* L, float* R, int n) noexcept
    {
        const int nI = n * sobre;
        std::memset (bufL, 0, sizeof (float) * (size_t) nI);
        std::memset (bufR, 0, sizeof (float) * (size_t) nI);
        constexpr int sub = 16;
        auto hzLfo = [&] (int pRate, int pRitme)
        {
            const int r = (int) valor (pRitme);
            if (r <= 0) return (double) valor (pRate);
            return std::max (20.0, sinte::tempo().load (std::memory_order_relaxed)) / 60.0 / sinte::negresRitme (r);
        };
        const double h1 = hzLfo (L1Vel, L1Ritme), h2 = hzLfo (L2Vel, L2Ritme);
        const double hv = valor (VibVel);
        const float ret1 = valor (L1Retard) / 1000.0f, ret2 = valor (L2Retard) / 1000.0f;
        for (int s = 0; s < nI; s += sub)
        {
            const int m = std::min (sub, nI - s);
            const double dtS = (double) m / fsI;
            fase1 += h1 * dtS; if (fase1 >= 1.0) { fase1 -= std::floor (fase1); sh1 = aleat(); }
            fase2 += h2 * dtS; if (fase2 >= 1.0) { fase2 -= std::floor (fase2); sh2 = aleat(); }
            faseVib += hv * dtS; if (faseVib >= 1.0) faseVib -= std::floor (faseVib);
            tRetard += (float) dtS;
            const float r1 = ret1 < 0.001f ? 1.0f : std::clamp (tRetard / ret1, 0.0f, 1.0f);
            const float r2 = ret2 < 0.001f ? 1.0f : std::clamp (tRetard / ret2, 0.0f, 1.0f);
            const int w1 = (int) valor (L1Ona), w2 = (int) valor (L2Ona);
            float a1 = valorLfo (w1, fase1, sh1), a2 = valorLfo (w2, fase2, sh2);
            lfoV1 += (a1 - lfoV1) * (w1 >= 3 ? 0.5f : 1.0f);
            lfoV2 += (a2 - lfoV2) * (w2 >= 3 ? 0.5f : 1.0f);
            posaLfo (lfoV1);
            lfo2V.store (lfoV2, std::memory_order_relaxed);
            const float vib = std::sin (2.0f * pi * (float) faseVib);
            for (auto& v : veus)
                if (v.viva) renderitzaVeu (v, s, m, lfoV1 * r1, lfoV2 * r2, vib);
        }
        float oL[tros], oR[tros];
        delL.processa (bufL, oL, n);
        delR.processa (bufR, oR, n);
        for (int i = 0; i < n; ++i)
        {
            const float yl = oL[i], yr = oR[i];
            const float ol = yl - dcxL + 0.9995f * dcL; dcxL = yl; dcL = ol;
            const float orr = yr - dcxR + 0.9995f * dcR; dcxR = yr; dcR = orr;
            oL[i] = ol; oR[i] = orr;
        }
        efectes (oL, oR, n);
        const float volObj = std::pow (10.0f, valor (Volum) / 20.0f);
        const float amp = valor (Ample);
        for (int i = 0; i < n; ++i)
        {
            volSuau += (volObj - volSuau) * 0.002f;
            const float mid = 0.5f * (oL[i] + oR[i]), sid = 0.5f * (oL[i] - oR[i]) * amp;
            auto sl = [] (float x) { const float ax = std::abs (x); return ax <= 0.8f ? x : std::copysign (0.8f + 0.2f * tanhR ((ax - 0.8f) / 0.2f), x); };
            L[i] = sl ((mid + sid) * volSuau);
            R[i] = sl ((mid - sid) * volSuau);
        }
    }

    void Lead::efectes (float* L, float* R, int n) noexcept
    {
        // chorus
        const float chm = valor (ChMix);
        if (chm > 0.001f && ! chL.empty())
        {
            const int N = (int) chL.size();
            const double dph = valor (ChVel) / fs;
            const float prof = valor (ChProf) * 0.004f * (float) fs, base = 0.007f * (float) fs;
            for (int i = 0; i < n; ++i)
            {
                chL[(size_t) chPos] = L[i];
                chR[(size_t) chPos] = R[i];
                chFase += dph; if (chFase >= 1.0) chFase -= 1.0;
                const float dLs = base + prof * (0.5f + 0.5f * std::sin (2.0f * pi * (float) chFase));
                const float dRs = base + prof * (0.5f + 0.5f * std::cos (2.0f * pi * (float) chFase));
                auto llegeix = [&] (const std::vector<float>& b, float d)
                {
                    float x = (float) chPos - d;
                    while (x < 0.0f) x += (float) N;
                    const int a = (int) x, c = (a + 1) % N;
                    const float k = x - (float) a;
                    return b[(size_t) a] + (b[(size_t) c] - b[(size_t) a]) * k;
                };
                const float wl = llegeix (chL, dLs), wr = llegeix (chR, dRs);
                L[i] = L[i] * (1.0f - 0.45f * chm) + wr * 0.75f * chm;
                R[i] = R[i] * (1.0f - 0.45f * chm) + wl * 0.75f * chm;
                chPos = (chPos + 1) % N;
            }
        }
        // delay de Mibase
        const float dm = valor (DlMix);
        if (dm > 0.001f)
        {
            const float v[4] = { (float) notaDelay[std::clamp ((int) valor (DlNota), 0, 7)], valor (DlFeedback) * 100.0f, dm * 100.0f, valor (DlPingPong) };
            if (v[0] != darrerDl[0]) dl.posa (delay::Nota, v[0]);
            if (v[1] != darrerDl[1]) dl.posa (delay::Feedback, v[1]);
            if (v[2] != darrerDl[2]) dl.posa (delay::Mix, v[2]);
            if (v[3] != darrerDl[3]) dl.posa (delay::EncaminamentP, v[3] > 0.5f ? (float) delay::PingPong : (float) delay::Estereo);
            for (int k = 0; k < 4; ++k) darrerDl[k] = v[k];
            dl.processa (L, R, n);
        }
        // reverb de Mibase (plate)
        const float rm = valor (RvMix);
        if (rm > 0.001f)
        {
            const float mida = valor (RvMida);
            if (mida != darrerRv[0]) { rv.posa (reverb::Decay, 0.6f + mida * mida * 6.0f); rv.posa (reverb::Size, 30.0f + 70.0f * mida); darrerRv[0] = mida; }
            if (rm != darrerRv[1]) { rv.posa (reverb::Mix, rm * 100.0f); darrerRv[1] = rm; }
            rv.processa (L, R, n);
        }
    }

    void Lead::renderitzaVeu (Veu& v, int s0, int m, float l1, float l2, float vib) noexcept
    {
        const int fi = s0 + m;
        const double dtS = (double) m / fsI;
        const float q1 = valor (L1Quant), q2 = valor (L2Quant);
        const int d1 = (int) valor (L1Desti), d2 = (int) valor (L2Desti);
        float mod[6] {};   // to, tall, forma, volum, fm, pan
        mod[d1] += l1 * q1;
        mod[d2] += l2 * q2;
        const float qv[6] { (d1 == 0 ? q1 : 0.0f) + (d2 == 0 ? q2 : 0.0f), 0, 0, (d1 == 3 ? q1 : 0.0f) + (d2 == 3 ? q2 : 0.0f), 0, 0 };

        // deriva analògica (un passeig lent)
        const float der = valor (Deriva);
        v.tDeriva += (float) dtS;
        if (v.tDeriva > 0.45f) { v.tDeriva = 0.0f; for (auto& d : v.derivaObj) d = aleat(); }
        for (int d = 0; d < 4; ++d) v.deriva[d] += (v.derivaObj[d] - v.deriva[d]) * (float) std::min (1.0, dtS * 2.5);

        // to
        const float gl = valor (Glide);
        if (gl < 0.5f) v.semis = v.objectiu;
        else v.semis += (v.objectiu - v.semis) * (1.0 - std::exp (-dtS / (gl / 1000.0 / 3.0)));
        double semis = v.semis + bendCru * valor (BendRang);
        const float pe = valor (PEnvQuant);
        if (std::abs (pe) > 1e-3f) semis += pe * std::exp (-v.tPEnv / (valor (PEnvTemps) / 1000.0f));
        v.tPEnv += (float) dtS;
        semis += mod[0] * std::abs (qv[0]) * 12.0f;
        semis += vib * (valor (RodaVib) * rodaMod + valor (AtVib) * pressioAra) * 0.6f;
        const double hzNota = 440.0 * std::pow (2.0, (semis - 69.0) / 12.0);

        // oscil·ladors
        int ona[3], cnt[3], taula[3];
        float forma[3], niv[3];
        double dt[3][maxUnison];
        float gL[3][maxUnison], gR[3][maxUnison];
        const int nu = std::clamp ((int) valor (Unison), 1, maxUnison);
        const float uniD = valor (UniDetune), uniA = valor (UniAmple);
        bool stereo = false;
        static const float ss[7] = { -1.0f, -0.68f, -0.38f, 0.0f, 0.38f, 0.68f, 1.0f };
        for (int o = 0; o < 3; ++o)
        {
            const int b = o * 6;
            ona[o] = (int) valor (O1Ona + b);
            taula[o] = (int) valor (O1Taula + b);
            forma[o] = std::clamp (valor (O1Forma + b) + mod[2] * 0.5f, 0.0f, 1.0f);
            niv[o] = valor (Niv1 + o) * 0.45f;
            const double hz = hzNota * std::pow (2.0, valor (O1Oct + b) + valor (O1Semi + b) / 12.0 + (valor (O1Fi + b) + v.deriva[o] * der * 14.0f) / 1200.0);
            if (ona[o] == Supersaw)
            {
                cnt[o] = 7;
                const float obr = forma[o] * std::sqrt (forma[o]) * 55.0f + 4.0f;
                for (int k = 0; k < 7; ++k)
                {
                    dt[o][k] = std::min (0.45, hz * std::pow (2.0, ss[k] * obr / 1200.0) / fsI);
                    const float g = (k == 3 ? 1.0f : 0.72f) / 2.2f;
                    const float pan = ss[k] * std::max (0.35f, uniA);
                    gL[o][k] = g * std::sqrt (0.5f * (1.0f - pan)) * 1.4142f;
                    gR[o][k] = g * std::sqrt (0.5f * (1.0f + pan)) * 1.4142f;
                }
                stereo = stereo || niv[o] > 0.0f;
            }
            else
            {
                cnt[o] = ona[o] == Soroll ? 1 : nu;
                const float gu = 1.0f / std::sqrt ((float) cnt[o]);
                for (int k = 0; k < cnt[o]; ++k)
                {
                    const float x = cnt[o] == 1 ? 0.0f : (float) k / (cnt[o] - 1) * 2.0f - 1.0f;
                    dt[o][k] = std::min (0.45, hz * std::pow (2.0, x * uniD * uniD * 60.0 / 1200.0) / fsI);
                    const float pan = x * uniA;
                    gL[o][k] = gu * std::sqrt (0.5f * (1.0f - pan)) * 1.4142f;
                    gR[o][k] = gu * std::sqrt (0.5f * (1.0f + pan)) * 1.4142f;
                }
                if (cnt[o] > 1 && uniA > 0.001f && niv[o] > 0.0f) stereo = true;
            }
        }
        const bool sync2 = valor (Sync2) > 0.5f, sync3 = valor (Sync3) > 0.5f;
        const float fm12 = std::clamp (valor (Fm12) + (mod[4] > 0 ? mod[4] : 0.0f), 0.0f, 1.0f), fm31 = valor (Fm31);
        const float ring = valor (Ring) * 0.6f;
        const double dSub = std::min (0.45, hzNota * (valor (SubOct) > 0.5f ? 0.25 : 0.5) / fsI);
        const bool subQuad = valor (SubOna) > 0.5f;
        const float nSub = valor (SubNiv) * 0.5f;
        const float nz = valor (SorollNiv) * 0.5f, nzC = 1.0f - 0.97f * valor (SorollColor);

        // envolupants
        auto cA = [&] (int p) { return coef (valor (p) / 1000.0 / 1.609, fsI); };
        auto cDR = [&] (int p) { return coef (valor (p) / 1000.0 / 4.0, fsI); };
        const float caA = cA (AA), cdA = cDR (AD), crA = cDR (AR), sA = valor (AS);
        const float caF = cA (FA), cdF = cDR (FD), crF = cDR (FR), sF = valor (FS);
        const float crRapid = coef (0.004, fsI);

        // filtre
        const int tipus = (int) valor (FTipus), pend = (int) valor (FPendent);
        double oct = valor (FEnv) * 7.0 * v.envF + valor (FKey) * (semis - 60.0) / 12.0 + valor (VelFiltre) * 4.0 * (v.vel - 0.6)
                   + valor (RodaTall) * rodaMod * 4.0 + valor (AtTall) * pressioAra * 4.0 + mod[1] * 4.0 + v.deriva[3] * der * 0.35;
        const double fc = std::clamp (valor (FTall) * std::pow (2.0, oct), 20.0, std::min (20000.0, fsI * 0.45));
        const float g = (float) std::tan (dpi * fc / fsI);
        const float res = valor (FRes);
        const int etapes = pend + 1;
        const float k0 = 2.0f - 1.97f * res, kb = 1.2f;
        const float A1 = 1.0f / (1.0f + g * (g + k0)), A2 = g * A1, A3 = g * A2;
        const float B1 = 1.0f / (1.0f + g * (g + kb)), B2 = g * B1, B3 = g * B2;
        const int N = pend == 0 ? 2 : 4;
        const float kl = res * (N == 4 ? 4.1f : 2.6f);
        const float G = g / (1.0f + g), GN = N == 4 ? G * G * G * G : G * G;
        const float fDrv = 1.0f + 3.0f * valor (FDrive);
        const float comp = (tipus == 3 ? 1.0f + kl * 0.28f : 1.0f) / (1.0f + 0.5f * valor (FDrive));

        // drive
        const int dtp = (int) valor (DTipus);
        const float dq = valor (DQuant);
        const float dA = 1.0f + dq * dq * 40.0f;
        const float dMake = 1.0f / std::pow (dA, 0.4f);
        const float cTo = (float) (1.0 - std::exp (-2.0 * dpi * (900.0 * std::pow (20.0, (double) valor (DTo))) / fsI));

        float vca = 1.0f - valor (VelVol) * (1.0f - v.vel);
        vca *= std::max (0.0f, 1.0f - 0.5f * (std::abs (qv[3]) - mod[3]));   // trèmolo
        const float pan = std::clamp (mod[5], -1.0f, 1.0f);
        const float pL = std::sqrt (0.5f * (1.0f - pan)) * 1.4142f, pR = std::sqrt (0.5f * (1.0f + pan)) * 1.4142f;

        auto gen = [&] (int o, int k, double pm, bool& volta, double& frac) noexcept
        {
            double& ph = v.ph[o][k];
            const double d = dt[o][k];
            double q = ph;
            const bool amb = pm == 0.0;
            if (! amb) { q = ph + pm; q -= std::floor (q); }
            float out;
            const float f = forma[o];
            switch (ona[o])
            {
                case Sinus: { const float s = std::sin (2.0f * pi * (float) q); out = f > 0.001f ? std::sin (s * (1.0f + 4.0f * f) * 0.5f * pi) : s; break; }
                case Triangle: { const float w = 0.5f + 0.49f * f, x = (float) q; out = x < w ? -1.0f + 2.0f * x / w : 1.0f - 2.0f * (x - w) / (1.0f - w); break; }
                case Serra: case Supersaw:
                {
                    out = (float) (2.0 * q - 1.0) - (amb ? blep (q, d) : 0.0f);
                    if (f > 0.001f && ona[o] == Serra)
                    {
                        double q2 = 2.0 * q; q2 -= std::floor (q2);
                        out = (out + f * ((float) (2.0 * q2 - 1.0) - (amb ? blep (q2, 2.0 * d) : 0.0f))) / (1.0f + 0.5f * f);
                    }
                    break;
                }
                case Quadrada: case Pols:
                {
                    const double w = ona[o] == Quadrada ? 0.5 - 0.45 * f : 0.25 - 0.22 * f;
                    out = q < w ? 1.0f : -1.0f;
                    if (amb) { out += blep (q, d); double r = q + 1.0 - w; if (r >= 1.0) r -= 1.0; out -= blep (r, d); }
                    break;
                }
                case Wavetable: out = wt::llegeix (taula[o], q, f, d); break;
                default: out = aleat(); break;
            }
            ph += d;
            volta = false;
            if (ph >= 1.0) { ph -= 1.0; volta = true; frac = ph / d; }
            return out;
        };

        for (int i = s0; i < fi; ++i)
        {
            switch (v.etapaA)
            {
                case 1: v.envA += (1.25f - v.envA) * caA; if (v.envA >= 1.0f) { v.envA = 1.0f; v.etapaA = 2; } break;
                case 2: v.envA += (sA - v.envA) * cdA; break;
                case 3: v.envA += (0.0f - v.envA) * crA; break;
                case 4: v.envA += (0.0f - v.envA) * crRapid; break;
                default: break;
            }
            switch (v.etapaF)
            {
                case 1: v.envF += (1.25f - v.envF) * caF; if (v.envF >= 1.0f) { v.envF = 1.0f; v.etapaF = 2; } break;
                case 2: v.envF += (sF - v.envF) * cdF; break;
                case 3: v.envF += (0.0f - v.envF) * crF; break;
                default: break;
            }
            float aL = 0.0f, aR = 0.0f, m1 = 0.0f, m2 = 0.0f, m3 = 0.0f;
            bool volta1[maxUnison] {}; double frac1[maxUnison] {};
            // osc 3 (modula l'1)
            if (niv[2] > 0.0f || fm31 > 0.0f)
                for (int k = 0; k < cnt[2]; ++k)
                {
                    bool vo; double fr = 0.0;
                    const float x = gen (2, k, 0.0, vo, fr);
                    m3 += x / cnt[2];
                    if (sync3 && k < cnt[0]) { /* el sync es fa després de l'1 */ }
                    aL += x * niv[2] * gL[2][k]; aR += x * niv[2] * gR[2][k];
                }
            // osc 1
            for (int k = 0; k < cnt[0]; ++k)
            {
                const float x = gen (0, k, fm31 > 0.001f ? (double) (fm31 * fm31 * 1.5f * m3) : 0.0, volta1[k], frac1[k]);
                m1 += x / cnt[0];
                aL += x * niv[0] * gL[0][k]; aR += x * niv[0] * gR[0][k];
            }
            // sync del 3 amb l'1 (per a la mostra següent)
            if (sync3)
                for (int k = 0; k < cnt[2]; ++k) { const int j = k < cnt[0] ? k : 0; if (volta1[j]) v.ph[2][k] = frac1[j] * dt[2][k]; }
            // osc 2
            if (niv[1] > 0.0f || ring > 0.0f)
                for (int k = 0; k < cnt[1]; ++k)
                {
                    const int j = k < cnt[0] ? k : 0;
                    if (sync2 && volta1[j]) v.ph[1][k] = frac1[j] * dt[1][k];
                    bool vo; double fr = 0.0;
                    const float x = gen (1, k, fm12 > 0.001f ? (double) (fm12 * fm12 * 1.6f * m1) : 0.0, vo, fr);
                    m2 += x / cnt[1];
                    aL += x * niv[1] * gL[1][k]; aR += x * niv[1] * gR[1][k];
                }
            float extra = m1 * m2 * ring;
            if (nSub > 0.0f)
            {
                const double p = v.fSub;
                float sb = subQuad ? (p < 0.5 ? 1.0f : -1.0f) + blep (p, dSub) - blep (p >= 0.5 ? p - 0.5 : p + 0.5, dSub) : std::sin (2.0f * pi * (float) p);
                extra += sb * nSub;
                v.fSub += dSub; if (v.fSub >= 1.0) v.fSub -= 1.0;
            }
            if (nz > 0.0f) { v.soroll += (aleat() - v.soroll) * nzC; extra += v.soroll * nz * (1.0f + (1.0f - nzC) * 2.0f); }
            float xL = (stereo ? aL : 0.5f * (aL + aR)) + extra, xR = (stereo ? aR : 0.5f * (aL + aR)) + extra;
            float y[2];
            for (int c = 0; c < (stereo ? 2 : 1); ++c)
            {
                auto& fl = v.fl[c];
                float x = (c == 0 ? xL : xR) * fDrv + 1e-20f;
                x = tanhR (x * 0.7f) * 1.4286f;
                float out = x;
                if (tipus == 3)
                {
                    float S = 0.0f;
                    for (int st = 0; st < N; ++st) S = S * G + (1.0f - G) * fl.s[st];
                    const float yN = (GN * x + S) / (1.0f + kl * GN);
                    float u = tanhR (x - kl * yN);
                    for (int st = 0; st < N; ++st) { const float vv = (u - fl.s[st]) * G; const float yy = vv + fl.s[st]; fl.s[st] = yy + vv; u = yy; }
                    out = u;
                    if (pend == 2)
                    {
                        const float v3 = out - fl.a2[2];
                        const float v1 = B1 * fl.a1[2] + B2 * v3, v2 = fl.a2[2] + B2 * fl.a1[2] + B3 * v3;
                        fl.a1[2] = 2.0f * v1 - fl.a1[2]; fl.a2[2] = 2.0f * v2 - fl.a2[2];
                        out = v2;
                    }
                }
                else
                    for (int st = 0; st < etapes; ++st)
                    {
                        const float a1 = st == 0 ? A1 : B1, a2 = st == 0 ? A2 : B2, a3 = st == 0 ? A3 : B3, kk = st == 0 ? k0 : kb;
                        const float v3 = out - fl.a2[st];
                        const float v1 = a1 * fl.a1[st] + a2 * v3, v2 = fl.a2[st] + a2 * fl.a1[st] + a3 * v3;
                        fl.a1[st] = 2.0f * v1 - fl.a1[st]; fl.a2[st] = 2.0f * v2 - fl.a2[st];
                        out = tipus == 0 ? v2 : tipus == 2 ? v1 * (st == 0 ? 1.0f : 1.4f) : out - kk * v1 - v2;
                    }
                out *= comp;
                if (dq > 0.001f)
                {
                    const float xa = out * dA;
                    float yd;
                    switch (dtp)
                    {
                        case 0: yd = tanhR (xa); break;
                        case 1: { const float z = xa + 0.2f; yd = (z > 0.0f ? tanhR (z) : tanhR (z * 0.6f) / 0.6f * 0.7f) - 0.15f; break; }
                        case 2: yd = std::copysign (1.0f - std::exp (-std::abs (xa) * 1.6f), xa); break;
                        default: { const float t = tanhR (xa * 1.5f); yd = std::clamp (0.6f * t + 0.6f * std::abs (t) - 0.25f, -1.0f, 1.0f); break; }
                    }
                    v.drvTo[c] += (yd * dMake * 1.6f - v.drvTo[c]) * cTo;
                    out = v.drvTo[c];
                }
                y[c] = out;
            }
            if (! stereo) y[1] = y[0];
            const float a = v.envA * vca;
            bufL[i] += y[0] * a * pL;
            bufR[i] += y[1] * a * pR;
        }
        if ((v.etapaA == 3 || v.etapaA == 4) && v.envA < 1e-5f) { v.viva = false; v.etapaA = 0; v.envA = 0.0f; }
    }

    //==========================================================================
    float previsualitza (const std::string& estat, float* sortida, int n, int nota)
    {
        auto sp = std::make_unique<Lead>();
        auto& s = *sp;
        s.posaEstat (estat.empty() ? estatInicial() : estat);
        for (int p : { (int) Volum, (int) Ample, (int) L1Quant, (int) L2Quant, (int) ChMix, (int) DlMix, (int) RvMix, (int) ArpMode, (int) Deriva })
            s.posa (p, p == Volum ? 0.0f : 0.0f);
        const double fs = 48000.0;
        s.prepara (fs, 512);
        EventInstrument on { 0, 0x90, (std::uint8_t) nota, 110 };
        std::vector<float> L (256), R (256);
        const int espera = (int) (fs * std::clamp ((double) s.valor (AA) / 1000.0 + 0.06, 0.06, 0.4));
        s.processa (&on, 1, L.data(), R.data(), 256);
        int fet = 256;
        while (fet < espera) { s.processa (nullptr, 0, L.data(), R.data(), 256); fet += 256; }
        const double hz = 440.0 * std::pow (2.0, (nota - 69) / 12.0);
        const int periode = (int) std::lround (fs / hz);
        std::vector<float> buf;
        for (int k = 0; k < 4 * periode / 256 + 2; ++k) { s.processa (nullptr, 0, L.data(), R.data(), 256); buf.insert (buf.end(), L.begin(), L.end()); }
        int ini = 0;
        for (int i = 1; i < periode && i + 1 < (int) buf.size(); ++i) if (buf[(size_t) i - 1] < 0.0f && buf[(size_t) i] >= 0.0f) { ini = i; break; }
        float pic = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const double x = ini + (double) i / std::max (1, n - 1) * 2.0 * periode;
            const int a = std::min ((int) buf.size() - 2, (int) x);
            const float t = (float) (x - a);
            sortida[i] = buf[(size_t) a] * (1.0f - t) + buf[(size_t) a + 1] * t;
            pic = std::max (pic, std::abs (sortida[i]));
        }
        return pic;
    }
}
