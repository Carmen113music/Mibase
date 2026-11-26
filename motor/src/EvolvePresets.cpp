#include "mibase/Evolve.h"

namespace mibase::evolve
{
    const std::vector<std::string>& categories()
    {
        // 1.13 (fase 1). «GLITCH & EXPERIMENTAL» arribarà amb el Scatter i el Granular.
        static const std::vector<std::string> c { "ATMOSPHERES", "VOCALS", "GUITARS", "RHYTHMIC", "EXTREME" };
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
            std::array<int, numModuls> ordre { MScreamer, MFiltre, MPumper, MShimmer };
        };

        std::string construeix (const Def& d)
        {
            Evolve x;
            x.posaEstat ("mibase-evolve=1");   // tot als valors per defecte, sense rutes
            for (const auto& b : d.base) x.posa (b.p, b.v);
            x.posa (Evolution, d.evo);
            x.posaRutes (d.rutes);
            x.posaOrdre (d.ordre);
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
