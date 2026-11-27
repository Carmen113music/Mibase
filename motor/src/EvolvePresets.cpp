#include "mibase/Evolve.h"

namespace mibase::evolve
{
    const std::vector<std::string>& categories()
    {
        // 1.13 (fase 1); 1.14: GLITCH & EXPERIMENTAL (amb el Scatter, el Reverser i el Pitch).
        static const std::vector<std::string> c { "ATMOSPHERES", "GLITCH & EXPERIMENTAL", "VOCALS", "GUITARS", "RHYTHMIC", "EXTREME" };
        return c;
    }

    namespace
    {
        struct PV { int p; float v; };
        struct Def
        {
            const char* cat; const char* nom; const char* desc;
            float evo;
            std::vector<PV> base;
            std::vector<Ruta> rutes;
            std::array<int, numModuls> ordre { MPitch, MScreamer, MFiltre, MScatter, MPumper, MReverser, MShimmer };
            std::vector<Assignacio> lfos {};
        };

        std::string construeix (const Def& d)
        {
            Evolve x;
            x.posaEstat ("mibase-evolve=1");   // tot als valors per defecte, sense rutes
            for (const auto& b : d.base) x.posa (b.p, b.v);
            x.posa (Evolution, d.evo);
            x.posaRutes (d.rutes);
            x.posaOrdre (d.ordre);
            x.posaAssignacions (d.lfos);
            return x.estat();
        }

        const std::vector<Def>& defs()
        {
            static const std::vector<Def> v = {
                // ---------------- ATMOSPHERES ----------------
                { "ATMOSPHERES", "Dream Clouds", "Primer s'obre l'espai, després el filtre es tanca i es mou i, al final, el so respira.", 50,
                  { { FilTipus, 0 }, { FilReso, 15 }, { FilLfoDiv, 0 }, { ShmTipus, 0 }, { ShmPitch, 0 }, { ShmAmt, 30 }, { ShmDecay, 3 }, { ShmSize, 85 },
                    { ShmMod, 50 }, { PmpMode, 1 }, { PmpDiv, 2 }, { PmpForma, 10 }, { ScrOn, 0 } },
                  { { ShmMix, 5, 60, 45, Suau }, { ShmDecay, 20, 100, 14, Lineal }, { ShmAmt, 40, 100, 65, Lineal }, { FilMix, 10, 20, 100, Rapida },
                    { FilFreq, 30, 90, 2500, EnS }, { FilLfo, 50, 100, 35, Lineal }, { PmpDepth, 60, 100, 35, Suau } } },
                { "ATMOSPHERES", "Cinematic Pad", "El so es converteix en un pad ample: cua ambient enorme, sense greus i amb un bombeig lent al final.", 60,
                  { { ShmTipus, 2 }, { ShmPitch, 0 }, { ShmSize, 95 }, { ShmDiff, 90 }, { ShmDamp, 55 }, { ShmDecay, 4 }, { FilTipus, 1 }, { FilFreq, 20 },
                    { PmpMode, 0 }, { PmpDiv, 2 }, { PmpForma, 30 }, { ScrOn, 0 } },
                  { { ShmMix, 0, 55, 60, Suau }, { ShmDecay, 20, 90, 20, Lineal }, { ShmAmt, 40, 100, 50, Lineal }, { FilMix, 30, 35, 100, Rapida },
                    { FilFreq, 30, 100, 350, Suau }, { PmpDepth, 70, 100, 25, Suau } } },
                { "ATMOSPHERES", "Infinite Guitar", "Sustain infinit: la cua puja d'octava i s'allarga; a dalt de tot es congela.", 55,
                  { { ShmTipus, 0 }, { ShmPitch, 0 }, { ShmDecay, 6 }, { ShmSize, 90 }, { ShmMod, 45 }, { FilTipus, 0 }, { ScrOn, 0 }, { PmpOn, 0 } },
                  { { ShmMix, 0, 60, 55, Suau }, { ShmDecay, 30, 95, 20, Lineal }, { ShmAmt, 30, 100, 75, Lineal }, { FilMix, 0, 5, 100, Rapida },
                    { FilFreq, 50, 100, 4000, Lineal }, { ShmFreeze, 98, 98, 1, Lineal } } },

                // ---------------- VOCALS ----------------
                { "VOCALS", "Angel Choir", "La veu s'aprima i deixa una estela d'octava amunt, com un cor llunyà.", 50,
                  { { FilTipus, 1 }, { FilFreq, 20 }, { ShmTipus, 0 }, { ShmPitch, 0 }, { ShmDecay, 3 }, { ShmSize, 80 }, { ShmDamp, 30 },
                    { PmpMode, 1 }, { PmpDiv, 8 }, { PmpForma, 0 }, { ScrOn, 0 } },
                  { { FilMix, 0, 10, 100, Rapida }, { FilFreq, 10, 60, 250, Lineal }, { ShmMix, 0, 60, 50, Suau }, { ShmAmt, 20, 90, 85, Lineal },
                    { ShmDecay, 30, 100, 12, Lineal }, { PmpDepth, 80, 100, 15, Lineal } } },
                { "VOCALS", "Dark Vocal", "Saturació càlida, filtre que enfosqueix i una cua una octava avall.", 45,
                  { { ScrMode, 0 }, { ScrTone, 30 }, { FilTipus, 0 }, { FilReso, 20 }, { ShmTipus, 1 }, { ShmPitch, 3 }, { ShmAmt, 0 }, { ShmDecay, 2.5f },
                    { ShmDamp, 70 }, { PmpOn, 0 } },
                  { { ScrMix, 0, 40, 70, Lineal }, { ScrDrive, 0, 100, 45, Lineal }, { FilMix, 0, 10, 100, Rapida }, { FilFreq, 20, 90, 1200, EnS },
                    { ShmMix, 30, 90, 40, Suau }, { ShmAmt, 50, 100, 55, Lineal } } },
                { "VOCALS", "Ghost Voice", "Veu de fantasma: una banda estreta que es mou, cua d'una quinta amunt i trèmolo.", 55,
                  { { FilTipus, 2 }, { FilFreq, 1500 }, { FilReso, 30 }, { FilLfoDiv, 0 }, { ShmTipus, 2 }, { ShmPitch, 1 }, { ShmDecay, 4 },
                    { PmpMode, 1 }, { PmpDiv, 4 }, { PmpForma, 20 }, { ScrOn, 0 } },
                  { { FilMix, 0, 50, 100, Suau }, { FilLfo, 40, 100, 40, Lineal }, { ShmMix, 10, 70, 60, Suau }, { ShmAmt, 30, 100, 60, Lineal },
                    { ShmDecay, 40, 100, 15, Lineal }, { PmpDepth, 60, 100, 40, Lineal }, { Sortida, 10, 50, 5, Lineal } } },

                // ---------------- GUITARS ----------------
                { "GUITARS", "Shimmer Guitar", "La guitarra deixa una cua brillant d'octava amunt; al final, una mica de saturació.", 50,
                  { { ShmTipus, 0 }, { ShmPitch, 0 }, { ShmDecay, 3 }, { ShmSize, 80 }, { ScrMode, 0 }, { ScrTone, 55 }, { FilOn, 0 }, { PmpOn, 0 } },
                  { { ShmMix, 0, 50, 45, Suau }, { ShmAmt, 20, 80, 70, Lineal }, { ShmDecay, 30, 100, 12, Lineal }, { ScrMix, 50, 100, 40, Lineal },
                    { ScrDrive, 50, 100, 35, Lineal } } },
                { "GUITARS", "Massive Fuzz", "Fuzz que es va fent gegant; a partir de la meitat, el filtre el tanca i el fa respirar.", 60,
                  { { ScrMode, 3 }, { ScrTone, 45 }, { FilTipus, 0 }, { FilReso, 25 }, { FilLfoDiv, 8 }, { ShmTipus, 1 }, { ShmAmt, 0 }, { ShmDecay, 1.5f },
                    { PmpOn, 0 } },
                  { { ScrMix, 0, 15, 100, Rapida }, { ScrDrive, 0, 80, 90, Lineal }, { FilMix, 40, 45, 100, Rapida }, { FilFreq, 40, 100, 2500, EnS },
                    { FilLfo, 70, 100, 30, Lineal }, { ShmMix, 50, 100, 25, Suau } } },
                { "GUITARS", "Space Guitar", "La guitarra viatja d'un costat a l'altre i s'enlaira amb una cua d'una quinta.", 50,
                  { { PmpMode, 3 }, { PmpDiv, 2 }, { ShmTipus, 2 }, { ShmPitch, 1 }, { ShmDecay, 4 }, { FilTipus, 1 }, { FilFreq, 20 }, { ScrOn, 0 } },
                  { { PmpDepth, 10, 70, 70, Lineal }, { ShmMix, 0, 60, 50, Suau }, { ShmDecay, 30, 100, 16, Lineal }, { ShmAmt, 40, 100, 50, Lineal },
                    { FilMix, 50, 55, 100, Rapida }, { FilFreq, 50, 100, 300, Lineal } } },
                { "GUITARS", "Cinematic Guitar", "Espai gran, saturació suau i, al final, un bombeig a negres que empeny la cançó.", 55,
                  { { ScrMode, 0 }, { ScrTone, 50 }, { FilTipus, 0 }, { PmpMode, 0 }, { PmpDiv, 5 }, { PmpForma, 40 }, { ShmTipus, 1 }, { ShmPitch, 0 },
                    { ShmAmt, 0 }, { ShmDecay, 3 } },
                  { { ShmMix, 0, 50, 45, Suau }, { ShmDecay, 20, 100, 10, Lineal }, { ShmAmt, 60, 100, 40, Lineal }, { ScrMix, 30, 80, 50, Lineal },
                    { ScrDrive, 30, 100, 40, Lineal }, { PmpDepth, 60, 100, 50, Suau }, { FilMix, 40, 45, 100, Rapida }, { FilFreq, 40, 100, 3500, Lineal } } },

                // ---------------- RHYTHMIC ----------------
                { "RHYTHMIC", "Rhythmic Pump", "Bombeig a negres com un sidechain; al 75 % passa a corxeres.", 50,
                  { { PmpMode, 0 }, { PmpDiv, 5 }, { PmpForma, 35 }, { FilTipus, 1 }, { FilFreq, 20 }, { ShmTipus, 1 }, { ShmAmt, 0 }, { ShmDecay, 1.5f },
                    { ScrOn, 0 } },
                  { { PmpDepth, 0, 60, 85, Rapida }, { PmpDiv, 75, 75, 8, Lineal }, { ShmMix, 40, 100, 25, Suau }, { FilMix, 50, 55, 100, Rapida },
                    { FilFreq, 50, 100, 400, Lineal }, { Sortida, 0, 100, 4, Lineal } } },
                { "RHYTHMIC", "Pulsating Synth", "Gate rítmic a semicorxeres i un filtre que s'obre i es tanca amb el tempo.", 50,
                  { { PmpMode, 2 }, { PmpDiv, 11 }, { PmpPatro, 2 }, { PmpForma, 15 }, { FilTipus, 0 }, { FilReso, 45 }, { FilLfoDiv, 5 }, { ShmTipus, 1 },
                    { ShmAmt, 0 }, { ShmDecay, 1.2f }, { ScrOn, 0 } },
                  { { PmpDepth, 0, 40, 100, Rapida }, { FilMix, 10, 15, 100, Rapida }, { FilFreq, 10, 70, 1500, EnS }, { FilLfo, 30, 100, 55, Lineal },
                    { ShmMix, 60, 100, 20, Suau }, { PmpDiv, 90, 90, 13, Lineal } } },

                // ---------------- EXTREME ----------------
                { "EXTREME", "Sound Destruction", "L'ona es doblega, una banda ressonant s'hi passeja i el gate la talla a trossos.", 50,
                  { { ScrMode, 4 }, { ScrTone, 55 }, { FilTipus, 2 }, { FilFreq, 900 }, { FilReso, 55 }, { FilLfoDiv, 8 }, { PmpMode, 2 }, { PmpDiv, 11 },
                    { PmpPatro, 3 }, { PmpForma, 10 }, { ShmTipus, 1 }, { ShmPitch, 3 }, { ShmAmt, 50 }, { ShmDecay, 2 } },
                  { { ScrMix, 0, 20, 100, Rapida }, { ScrDrive, 0, 100, 95, Lineal }, { FilMix, 30, 60, 80, Lineal }, { FilLfo, 40, 100, 80, Lineal },
                    { FilReso, 50, 100, 80, Lineal }, { PmpDepth, 50, 80, 100, Lineal }, { PmpDiv, 85, 85, 13, Lineal }, { ShmMix, 60, 100, 30, Suau }, { Sortida, 30, 100, 6, Lineal } } },
                { "EXTREME", "Digital Storm", "Fuzz, un filtre que escombra a semicorxeres i una tempesta d'octaves amunt.", 55,
                  { { ScrMode, 3 }, { FilTipus, 2 }, { FilFreq, 1200 }, { FilReso, 40 }, { FilLfoDiv, 11 }, { ShmTipus, 0 }, { ShmPitch, 0 }, { ShmDecay, 3 },
                    { PmpMode, 2 }, { PmpPatro, 6 }, { PmpDiv, 11 }, { PmpForma, 10 } },
                  { { ScrMix, 0, 30, 80, Lineal }, { ScrDrive, 0, 100, 70, Lineal }, { FilMix, 20, 50, 70, Lineal }, { FilLfo, 30, 100, 90, Lineal },
                    { ShmMix, 40, 100, 45, Suau }, { ShmAmt, 40, 100, 80, Lineal }, { ShmDecay, 40, 100, 14, Lineal }, { PmpDepth, 70, 100, 90, Suau }, { Sortida, 30, 100, 5, Lineal } } },

                // ================= 1.14 (fase 2) =================
                // ---------------- GLITCH & EXPERIMENTAL ----------------
                { "GLITCH & EXPERIMENTAL", "Stutter Machine", "Repeticions a semicorxeres que van guanyant terreny; al final, tresets.", 50,
                  { { ScaDiv, 11 }, { ScaScatter, 10 }, { ScaFeedback, 30 }, { ScaDensity, 10 }, { ScrOn, 0 }, { FilOn, 0 }, { PmpOn, 0 }, { ShmTipus, 1 },
                    { ShmAmt, 0 }, { ShmDecay, 1.2f } },
                  { { ScaMix, 0, 5, 100, Rapida }, { ScaDensity, 0, 100, 75, Lineal }, { ScaScatter, 50, 100, 45, Lineal }, { ScaDiv, 85, 85, 12, Lineal },
                    { ShmMix, 70, 100, 18, Suau } } },
                { "GLITCH & EXPERIMENTAL", "Digital Chaos", "Talls que salten enrere, l'ona doblegada i una octava amunt que es desafina; un filtre a l'atzar.", 50,
                  { { ScaDiv, 11 }, { ScaScatter, 40 }, { ScrMode, 4 }, { ScrTone, 60 }, { PitSemi1, 12 }, { PitDetune, 12 }, { FilTipus, 2 }, { FilFreq, 1800 },
                    { FilReso, 35 }, { PmpOn, 0 }, { ShmOn, 0 }, { L1Forma, 4 }, { L1Div, 11 }, { L1Depth, 100 } },
                  { { ScaMix, 0, 10, 100, Rapida }, { ScaDensity, 0, 100, 80, Lineal }, { ScaScatter, 30, 100, 85, Lineal }, { ScrMix, 20, 70, 70, Lineal },
                    { ScrDrive, 20, 100, 70, Lineal }, { PitMix, 50, 90, 40, Suau }, { FilMix, 40, 60, 70, Lineal }, { Sortida, 40, 100, 7, Lineal } },
                  { MPitch, MScreamer, MFiltre, MScatter, MPumper, MReverser, MShimmer }, { { 0, FilFreq, 35 } } },
                { "GLITCH & EXPERIMENTAL", "Broken Signal", "Senyal que es trenca: talls a corxeres, una banda que salta a l'atzar i una mica de distorsió.", 50,
                  { { ScaDiv, 8 }, { ScaScatter, 30 }, { FilTipus, 2 }, { FilFreq, 1200 }, { FilReso, 45 }, { ScrMode, 2 }, { ScrTone, 45 }, { PmpOn, 0 },
                    { ShmOn, 0 }, { PitOn, 0 }, { L1Forma, 4 }, { L1Div, 11 }, { L1Depth, 100 } },
                  { { FilMix, 10, 60, 85, Suau }, { ScaMix, 10, 15, 100, Rapida }, { ScaDensity, 10, 100, 65, Lineal }, { ScrMix, 30, 80, 45, Lineal },
                    { ScrDrive, 30, 100, 50, Lineal }, { Sortida, 20, 100, 4, Lineal } },
                  { MPitch, MScreamer, MFiltre, MScatter, MPumper, MReverser, MShimmer }, { { 0, FilFreq, 30 } } },
                { "GLITCH & EXPERIMENTAL", "Glitch Explosion", "Repeticions i trossos al revés que exploten en una cua d'octaves.", 50,
                  { { ScaDiv, 11 }, { ScaScatter, 50 }, { RevMode, 0 }, { RevDiv, 8 }, { RevFeedback, 30 }, { ShmTipus, 0 }, { ShmPitch, 0 }, { ShmDecay, 3 },
                    { ScrOn, 0 }, { FilOn, 0 }, { PmpOn, 0 }, { PitOn, 0 } },
                  { { ScaMix, 0, 10, 100, Rapida }, { ScaDensity, 0, 80, 85, Lineal }, { RevMix, 30, 80, 45, Suau }, { ShmMix, 50, 100, 50, Suau },
                    { ShmDecay, 60, 100, 12, Lineal }, { ShmAmt, 60, 100, 75, Lineal } } },
                { "GLITCH & EXPERIMENTAL", "Fragmented Reality", "Fragments que tornen de més enrere, al revés i una octava avall, dins d'un espai ambient.", 55,
                  { { ScaDiv, 8 }, { ScaScatter, 80 }, { ScaFeedback, 50 }, { RevMode, 0 }, { RevDiv, 5 }, { RevPitch, 2 }, { ShmTipus, 2 }, { ShmPitch, 3 },
                    { ShmDecay, 4 }, { ScrOn, 0 }, { FilOn, 0 }, { PmpOn, 0 }, { PitOn, 0 } },
                  { { ScaMix, 0, 10, 100, Rapida }, { ScaDensity, 0, 100, 70, Lineal }, { RevMix, 20, 70, 40, Suau }, { ShmMix, 30, 90, 45, Suau },
                    { ShmAmt, 50, 100, 60, Lineal } } },

                // ---------------- ATMOSPHERES (fase 2) ----------------
                { "ATMOSPHERES", "Reverse Horizon", "La cua invertida puja just abans de cada temps (a blanques) i s'obre en un shimmer.", 50,
                  { { RevMode, 1 }, { RevDiv, 2 }, { RevDecay, 4 }, { ShmTipus, 0 }, { ShmPitch, 0 }, { ShmDecay, 4 }, { FilTipus, 1 }, { FilFreq, 20 },
                    { ScrOn, 0 }, { PmpOn, 0 }, { PitOn, 0 }, { ScaOn, 0 } },
                  { { RevMix, 0, 50, 80, Suau }, { RevDecay, 30, 100, 9, Lineal }, { ShmMix, 40, 100, 35, Suau }, { FilMix, 50, 55, 100, Rapida },
                    { FilFreq, 50, 100, 250, Lineal } } },
                { "ATMOSPHERES", "Ambient Piano", "Piano (o el que sigui) dins d'un núvol ambient, amb ecos al revés i una octava suau.", 45,
                  { { ShmTipus, 2 }, { ShmPitch, 0 }, { ShmDecay, 5 }, { ShmSize, 90 }, { RevMode, 0 }, { RevDiv, 5 }, { RevFeedback, 25 }, { PitSemi1, 12 },
                    { ScrOn, 0 }, { FilOn, 0 }, { PmpOn, 0 }, { ScaOn, 0 }, { L2Forma, 0 }, { L2Sync, 0 }, { L2Rate, 0.15f }, { L2Depth, 100 } },
                  { { ShmMix, 0, 60, 45, Suau }, { ShmDecay, 30, 100, 14, Lineal }, { RevMix, 30, 80, 30, Suau }, { PitMix, 60, 100, 20, Suau } },
                  { MPitch, MScreamer, MFiltre, MScatter, MPumper, MReverser, MShimmer }, { { 1, ShmMod, 30 } } },

                // ---------------- VOCALS (fase 2) ----------------
                { "VOCALS", "Reverse Vocal", "La veu deixa una cua invertida que puja abans de cada negra.", 50,
                  { { RevMode, 1 }, { RevDiv, 5 }, { RevDecay, 2.5f }, { FilTipus, 1 }, { FilFreq, 20 }, { ShmTipus, 1 }, { ShmAmt, 0 }, { ShmDecay, 2 },
                    { ScrOn, 0 }, { PmpOn, 0 }, { PitOn, 0 }, { ScaOn, 0 } },
                  { { RevMix, 0, 50, 75, Suau }, { RevDecay, 40, 100, 6, Lineal }, { FilMix, 0, 5, 100, Rapida }, { FilFreq, 20, 80, 200, Lineal },
                    { ShmMix, 50, 100, 25, Suau } } },
                { "VOCALS", "Frozen Voice", "Harmonies d'octava amb el color de la veu i, a dalt de tot, la cua es congela.", 50,
                  { { PitMode, 1 }, { PitSemi1, 12 }, { PitSemi2, 7 }, { PitFormant, 0 }, { ShmTipus, 0 }, { ShmPitch, 0 }, { ShmDecay, 6 }, { ScrOn, 0 },
                    { FilOn, 0 }, { PmpOn, 0 }, { ScaOn, 0 }, { RevOn, 0 } },
                  { { PitMix, 0, 50, 35, Suau }, { PitNiv2, 40, 90, 70, Lineal }, { ShmMix, 10, 70, 50, Suau }, { ShmDecay, 40, 95, 20, Lineal },
                    { ShmFreeze, 95, 95, 1, Lineal } } },
                { "VOCALS", "Alien Voice", "Veu d'un altre planeta: formants moguts, una quinta avall que oscil·la i un punt de saturació.", 50,
                  { { PitMode, 1 }, { PitSemi1, 0 }, { PitSemi2, -5 }, { PitFormant, 0 }, { ScrMode, 0 }, { ScrTone, 65 }, { FilOn, 0 }, { PmpOn, 0 },
                    { ScaOn, 0 }, { RevOn, 0 }, { ShmTipus, 1 }, { ShmAmt, 0 }, { ShmDecay, 1.5f }, { L1Forma, 1 }, { L1Div, 8 }, { L1Depth, 100 } },
                  { { PitMix, 0, 20, 100, Rapida }, { PitFormant, 0, 80, 6, Lineal }, { PitNiv2, 30, 80, 60, Lineal }, { ScrMix, 40, 90, 35, Lineal },
                    { ScrDrive, 40, 100, 35, Lineal }, { ShmMix, 60, 100, 20, Suau } },
                  { MPitch, MScreamer, MFiltre, MScatter, MPumper, MReverser, MShimmer }, { { 0, PitFi, 20 } } },

                // ---------------- GUITARS (fase 2) ----------------
                { "GUITARS", "Reverse Guitar", "Cada negra torna al revés, com una cinta girada; al final, una cua brillant.", 50,
                  { { RevMode, 0 }, { RevDiv, 5 }, { RevFeedback, 35 }, { ShmTipus, 0 }, { ShmPitch, 0 }, { ShmDecay, 3 }, { ScrOn, 0 }, { FilOn, 0 },
                    { PmpOn, 0 }, { PitOn, 0 }, { ScaOn, 0 } },
                  { { RevMix, 0, 50, 55, Suau }, { RevFeedback, 50, 100, 55, Lineal }, { ShmMix, 50, 100, 35, Suau }, { ShmAmt, 60, 100, 60, Lineal } } },
                { "GUITARS", "Infinite Sustain", "La nota no s'acaba: octava suau, cua llarguíssima i, a dalt, congelada.", 55,
                  { { PitSemi1, 12 }, { ScrMode, 0 }, { ScrTone, 45 }, { ShmTipus, 1 }, { ShmAmt, 0 }, { ShmDecay, 5 }, { ShmSize, 90 }, { FilOn, 0 },
                    { PmpOn, 0 }, { ScaOn, 0 }, { RevOn, 0 } },
                  { { ScrMix, 0, 30, 40, Lineal }, { ScrDrive, 0, 100, 40, Lineal }, { ShmMix, 0, 60, 50, Suau }, { ShmDecay, 30, 90, 20, Lineal },
                    { PitMix, 40, 90, 25, Suau }, { ShmFreeze, 92, 92, 1, Lineal } } },

                // ---------------- RHYTHMIC (fase 2) ----------------
                { "RHYTHMIC", "Stutter Beat", "Repeticions a semicorxeres i un bombeig a negres: per a bateries i loops.", 50,
                  { { ScaDiv, 11 }, { ScaScatter, 15 }, { ScaFeedback, 20 }, { PmpMode, 0 }, { PmpDiv, 5 }, { PmpForma, 40 }, { ScrOn, 0 }, { FilOn, 0 },
                    { ShmOn, 0 }, { PitOn, 0 }, { RevOn, 0 } },
                  { { ScaMix, 0, 5, 100, Rapida }, { ScaDensity, 0, 100, 60, Lineal }, { PmpDepth, 30, 80, 70, Suau }, { ScaDiv, 80, 80, 13, Lineal } } },
                { "RHYTHMIC", "Rhythmic Glitch", "Talls a tresets, un gate trencat i un filtre que salta en quadrat.", 50,
                  { { ScaDiv, 10 }, { ScaScatter, 45 }, { PmpMode, 2 }, { PmpDiv, 11 }, { PmpPatro, 3 }, { PmpForma, 10 }, { FilTipus, 0 }, { FilFreq, 4000 },
                    { FilReso, 40 }, { ScrOn, 0 }, { ShmOn, 0 }, { PitOn, 0 }, { RevOn, 0 }, { L1Forma, 2 }, { L1Div, 8 }, { L1Depth, 100 } },
                  { { ScaMix, 0, 5, 100, Rapida }, { ScaDensity, 0, 100, 60, Lineal }, { PmpDepth, 30, 70, 90, Lineal }, { FilMix, 20, 40, 100, Lineal } },
                  { MPitch, MScreamer, MFiltre, MScatter, MPumper, MReverser, MShimmer }, { { 0, FilFreq, 25 } } },

                // ---------------- EXTREME (fase 2) ----------------
                { "EXTREME", "Total Chaos", "Tot alhora: talls, octaves, fuzz, trossos al revés i una tempesta d'espai.", 55,
                  { { ScaDiv, 11 }, { ScaScatter, 70 }, { PitSemi1, 12 }, { PitSemi2, -12 }, { PitDetune, 20 }, { ScrMode, 3 }, { RevMode, 0 }, { RevDiv, 8 },
                    { RevPitch, 1 }, { ShmTipus, 0 }, { ShmDecay, 4 }, { FilOn, 0 }, { PmpOn, 0 }, { L1Forma, 4 }, { L1Div, 8 }, { L1Depth, 100 } },
                  { { ScaMix, 0, 10, 100, Rapida }, { ScaDensity, 0, 100, 90, Lineal }, { PitMix, 20, 70, 50, Lineal }, { PitNiv2, 40, 100, 70, Lineal },
                    { ScrMix, 30, 80, 70, Lineal }, { ScrDrive, 30, 100, 80, Lineal }, { RevMix, 50, 90, 45, Suau }, { ShmMix, 60, 100, 45, Suau },
                    { Sortida, 30, 100, 3, Lineal } },
                  { MPitch, MScreamer, MFiltre, MScatter, MPumper, MReverser, MShimmer }, { { 0, PitFi, 30 } } },
                { "EXTREME", "Frozen Universe", "Octaves amunt i avall dins d'un espai enorme que es congela del tot.", 60,
                  { { PitSemi1, 12 }, { PitSemi2, -12 }, { ShmTipus, 0 }, { ShmPitch, 0 }, { ShmDecay, 8 }, { ShmSize, 100 }, { RevMode, 0 }, { RevDiv, 2 },
                    { ScrOn, 0 }, { FilOn, 0 }, { PmpOn, 0 }, { ScaOn, 0 }, { L2Forma, 0 }, { L2Sync, 0 }, { L2Rate, 0.1f }, { L2Depth, 100 } },
                  { { PitMix, 0, 50, 45, Suau }, { PitNiv2, 20, 70, 80, Lineal }, { ShmMix, 0, 60, 60, Suau }, { ShmDecay, 30, 90, 20, Lineal },
                    { RevMix, 40, 80, 30, Suau }, { ShmFreeze, 75, 75, 1, Lineal }, { Sortida, 40, 100, 6, Lineal } },
                  { MPitch, MScreamer, MFiltre, MScatter, MPumper, MReverser, MShimmer }, { { 1, ShmMod, 40 } } },
            };
            return v;
        }
    }

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> v = []
        {
            std::vector<Preset> r;
            for (const auto& d : defs()) r.push_back ({ d.nom, d.cat, d.desc, construeix (d) });
            return r;
        }();
        return v;
    }
}
