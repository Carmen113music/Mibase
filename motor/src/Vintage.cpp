#include "mibase/Vintage.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#if defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)
 #include <xmmintrin.h>
 #define MIBASE_VINTAGE_FTZ 1
#endif

namespace mibase::analogic
{
    using sinte::tanhR; using sinte::blep; using sinte::num; using sinte::ms;
    using sinte::Lineal; using sinte::Enter; using sinte::Log; using sinte::Cub;

    namespace
    {
        constexpr float pi = 3.14159265358979f;
        constexpr double dpi = 3.14159265358979323846;
        const char* const ones = "Serra|Quadrada|Triangle|Sinus|Pols";
        const char* const noSi = "No|Sí";
        const char* const notesDelay = "1/16|1/8T|1/8|1/8.|1/4T|1/4|1/4.|1/2";
        const int notaDelay[] = { 2, 3, 5, 7, 6, 8, 10, 11 };

#define OSC(n, ona, peu, niv) \
        { "o" #n "w", "Ona", SOsc, 0, 4, ona, Enter, ones, "Forma d'ona de l'oscil·lador " #n }, \
        { "o" #n "o", "Peu", SOsc, -2, 2, peu, Enter, nullptr, "Registre de l'oscil·lador " #n ", com als aparells antics: 32', 16', 8', 4' o 2'" }, \
        { "o" #n "s", "Semitò", SOsc, -12, 12, 0, Enter, nullptr, "Semitons de l'oscil·lador " #n " (quintes, octaves…)" }, \
        { "o" #n "f", "Fi", SOsc, -50, 50, 0, Lineal, nullptr, "Afinació fina (centèsimes): una mica desafinats sonen més grossos" }, \
        { "o" #n "pw", "Amplada", SOsc, 0.05f, 0.5f, 0.25f, Lineal, nullptr, "Amplada del pols (ona «Pols»): 50 % és quadrada; com més estreta, més nasal" }, \
        { "o" #n "n", "Nivell", SOsc, 0, 1, niv, Lineal, nullptr, "Volum de l'oscil·lador " #n " al mesclador" },

        const sinte::InfoParam infos[] = {
            OSC (1, 0, 0, 0.8f)
            OSC (2, 0, 0, 0.0f)
            OSC (3, 0, -1, 0.0f)
            { "o3lo", "Osc 3 lent", SMod, 0, 1, 0, Enter, noSi, "Sí: l'oscil·lador 3 va a freqüència d'LFO i no segueix el teclat (no sona; serveix per modular, com al Minimoog)" },
            { "sync", "Sync", SMod, 0, 1, 0, Enter, noSi, "L'oscil·lador 2 es reinicia amb l'1: el so «que talla» dels leads de sync" },
            { "env2", "Env → osc 2", SMod, -24, 24, 0, Lineal, nullptr, "L'envolupant del filtre mou el to de l'oscil·lador 2 (amb sync: les escombrades clàssiques)" },
            { "fm", "FM 3 → 1", SMod, 0, 1, 0, Cub, nullptr, "L'oscil·lador 3 modula la freqüència de l'1 (FM bàsica: campanes, metalls, gruny)" },
            { "subn", "Sub", SMix, 0, 1, 0, Lineal, nullptr, "Oscil·lador sub, per sota de l'oscil·lador 1" },
            { "subo", "Sub oct.", SMix, 0, 1, 0, Enter, "−1 oct|−2 oct", "Una o dues octaves per sota" },
            { "subw", "Sub ona", SMix, 0, 1, 0, Enter, "Quadrada|Sinus", "Forma del sub" },
            { "nzn", "Soroll", SMix, 0, 1, 0, Lineal, nullptr, "Generador de soroll al mesclador" },
            { "nzc", "Color", SMix, 0, 1, 0, Enter, "Blanc|Rosa", "Soroll blanc (brillant) o rosa (més fosc)" },
            { "ring", "Ring", SMix, 0, 1, 0, Lineal, nullptr, "Modulador en anell (osc 1 × osc 2): sons metàl·lics" },
            { "ft", "Filtre", SFiltre, 0, 3, 0, Enter, "Ladder|OTA|SEM|MS-20", "Ladder: gras i rodó (Moog). OTA: net i precís (Prophet, Jupiter, Juno). SEM: suau i obert (Oberheim). MS-20: agressiu i que xiula (Korg)" },
            { "fp", "Pendent", SFiltre, 0, 1, 1, Enter, "12 dB|24 dB", "Pendent dels filtres Ladder i OTA (el SEM i el MS-20 són de 12 dB)" },
            { "fmo", "Mode", SFiltre, 0, 3, 0, Enter, "LP|BP|HP|Notch", "Passa-baixos, passa-banda, passa-alts o notch" },
            { "cut", "Tall", SFiltre, 20, 18000, 1800, Log, nullptr, "Freqüència de tall del filtre" },
            { "res", "Ressonància", SFiltre, 0, 1, 0.2f, Lineal, nullptr, "Ressonància (el Ladder i el MS-20 xiulen sols al màxim)" },
            { "fenv", "Envolupant", SFiltre, -1, 1, 0.4f, Lineal, nullptr, "Quant obre (o tanca) el filtre la seva envolupant" },
            { "fkey", "Teclat", SFiltre, 0, 1, 0.5f, Lineal, nullptr, "El filtre segueix les notes (a 100 %, igual que el to)" },
            { "fvel", "Vel.", SFiltre, 0, 1, 0.2f, Lineal, nullptr, "Com més fort toques, més s'obre" },
            { "drv", "Drive", SFiltre, 0, 1, 0.15f, Lineal, nullptr, "Entrada del filtre més forta: el mesclador saturat dels aparells antics" },
            { "hpf", "HPF", SFiltre, 10, 1500, 10, Log, nullptr, "Passa-alts final (com el del Juno): treu greus per fer lloc" },
            { "aa", "Atac", SEnv, 0.5f, 10000, 3, Cub, nullptr, "Atac del volum" },
            { "ad", "Caiguda", SEnv, 5, 10000, 600, Cub, nullptr, "Caiguda del volum" },
            { "as", "Sost.", SEnv, 0, 1, 0.8f, Lineal, nullptr, "Sosteniment del volum" },
            { "ar", "Allib.", SEnv, 5, 12000, 300, Cub, nullptr, "Alliberament del volum" },
            { "fa", "Atac", SEnv, 0.5f, 10000, 2, Cub, nullptr, "Atac del filtre" },
            { "fd", "Caiguda", SEnv, 5, 10000, 500, Cub, nullptr, "Caiguda del filtre" },
            { "fs", "Sost.", SEnv, 0, 1, 0.3f, Lineal, nullptr, "Sosteniment del filtre" },
            { "fr", "Allib.", SEnv, 5, 12000, 400, Cub, nullptr, "Alliberament del filtre" },
            { "vv", "Vel → vol", SEnv, 0, 1, 0.4f, Lineal, nullptr, "Com canvia el volum amb la força de la nota" },
            { "lw", "Ona", SLfo, 0, 5, 0, Enter, "Triangle|Sinus|Serra|Rampa|Quadrada|Atzar", "Forma de l'LFO" },
            { "lr", "Velocitat", SLfo, 0.05f, 30, 5, Log, nullptr, "Velocitat de l'LFO (si no va amb el tempo)" },
            { "ls", "Tempo", SLfo, 0, 11, 0, Enter, sinte::ritmesLfo, "LFO lliure o amb el tempo" },
            { "lret", "Retard", SLfo, 0, 4000, 0, Cub, nullptr, "L'LFO entra a poc a poc després de la nota (vibrato de violí, com al Juno)" },
            { "lv", "Per veu", SLfo, 0, 1, 0, Enter, noSi, "No: un sol LFO per a totes les veus. Sí: cada nota el seu (comença de zero)" },
            { "lf", "Font", SLfo, 0, 2, 0, Enter, "LFO|Osc 3|Soroll", "Què modula: l'LFO, l'oscil·lador 3 o un soroll lent (com el «Mod Mix» del Minimoog)" },
            { "lto", "→ To", SLfo, 0, 12, 0, Cub, nullptr, "Vibrato (semitons)" },
            { "lpw", "→ PWM", SLfo, 0, 1, 0, Lineal, nullptr, "Mou l'amplada del pols (el cor dels pads de Juno)" },
            { "lcut", "→ Tall", SLfo, 0, 1, 0, Lineal, nullptr, "Mou el filtre" },
            { "lvol", "→ Vol", SLfo, 0, 1, 0, Lineal, nullptr, "Trèmolo" },
            { "rto", "Roda vib", SLfo, 0, 2, 0.35f, Lineal, nullptr, "Vibrato amb la roda de modulació (semitons al màxim)" },
            { "rcut", "Roda tall", SLfo, 0, 1, 0, Lineal, nullptr, "La roda fa que la font de modulació mogui el filtre" },
            { "atc", "AT tall", SLfo, 0, 1, 0.2f, Lineal, nullptr, "Aftertouch: prement més, el filtre s'obre (com al CS-80)" },
            { "an", "Analògic", SAnal, 0, 1, 1, Enter, "Off|On", "Off: so digital net (sense cap imperfecció). On: els controls de caràcter" },
            { "ader", "Deriva", SAnal, 0, 1, 0.25f, Lineal, nullptr, "Els oscil·ladors es desafinen a poc a poc, cadascun pel seu compte" },
            { "ains", "Inestable", SAnal, 0, 1, 0.15f, Lineal, nullptr, "Petites irregularitats de cicle a cicle i de fase en cada nota" },
            { "afil", "Var. filtre", SAnal, 0, 1, 0.2f, Lineal, nullptr, "Cada veu té el filtre una mica diferent (com les targetes de veu reals)" },
            { "acal", "Calidesa", SAnal, 0, 1, 0.3f, Lineal, nullptr, "Harmònics parells i aguts més rodons" },
            { "asat", "Saturació", SAnal, 0, 1, 0.2f, Lineal, nullptr, "Saturació suau a l'amplificador de cada veu" },
            { "asor", "Soroll", SAnal, 0, 1, 0.1f, Lineal, nullptr, "Soroll de fons i brunzit de la xarxa (molt fluix)" },
            { "aedat", "Edat", SAnal, 0, 1, 0.15f, Lineal, nullptr, "Components envellits: afinació i envolupants menys exactes, més deriva i menys aguts" },
            { "aest", "Var. estèreo", SAnal, 0, 1, 0.3f, Lineal, nullptr, "Cada veu una mica cap a un costat" },
            { "ch", "Chorus", SFx, 0, 3, 0, Enter, "Off|I|II|I+II", "Chorus de BBD com el del Juno: I suau, II més profund, I+II ràpid i tremolós" },
            { "phm", "Phaser", SFx, 0, 1, 0, Lineal, nullptr, "Phaser de 4 etapes (com els pedals dels 70)" },
            { "phr", "Vel. ph.", SFx, 0.05f, 8, 0.4f, Log, nullptr, "Velocitat del phaser" },
            { "dlm", "Delay", SFx, 0, 1, 0, Lineal, nullptr, "Quantitat de delay" },
            { "dlt", "Temps", SFx, 0, 7, 5, Enter, notesDelay, "Temps del delay amb el tempo" },
            { "dlf", "Feedback", SFx, 0, 0.95f, 0.35f, Lineal, nullptr, "Repeticions del delay" },
            { "dlc", "Tipus", SFx, 0, 1, 0, Enter, "Cinta|BBD", "Delay de cinta (amb wow) o de BBD (fosc i granulós)" },
            { "rvm", "Reverb", SFx, 0, 1, 0, Lineal, nullptr, "Quantitat de reverb" },
            { "rvt", "Tipus", SFx, 0, 3, 0, Enter, "Molla|Placa|Cambra|Sala", "Reverb de molla, de placa, de cambra o de sala" },
            { "rvs", "Mida", SFx, 0, 1, 0.5f, Lineal, nullptr, "Mida i durada de la reverb" },
            { "width", "Amplada", SFx, 0, 1.5f, 1, Lineal, nullptr, "Amplada estèreo final" },
            { "vol", "Volum", SFx, -30, 12, -6, Lineal, nullptr, "Volum de sortida" },
            { "mode", "Mode", SVeu, 0, 2, 2, Enter, "Mono|Legato|Poli", "Mono, Legato (les notes lligades llisquen sense tornar a atacar) o Poli" },
            { "veus", "Veus", SVeu, 1, 16, 8, Enter, nullptr, "Polifonia màxima (en mode Poli)" },
            { "uni", "Unison", SVeu, 1, 8, 1, Enter, nullptr, "Veus per nota (apilades i desafinades)" },
            { "unid", "Uni. det.", SVeu, 0, 1, 0.3f, Lineal, nullptr, "Desafinació entre les veus de l'unison" },
            { "glide", "Glide", SVeu, 0, 2000, 0, Cub, nullptr, "Portament" },
            { "bend", "Bend", SVeu, 0, 12, 2, Enter, nullptr, "Rang del pitch bend (semitons)" },
            { "fase", "Fase", SVeu, 0, 1, 0, Enter, "Lliure|Reinicia", "Lliure: els oscil·ladors no s'aturen mai (com als analògics). Reinicia: cada nota comença igual (més cop)" },
            { "arp", "Arp", SArp, 0, 7, 0, Enter, sinte::modesArp, "Arpegiador / seqüenciador: amb una tecla o un acord, toca sol amb el tempo" },
            { "arpr", "Ritme", SArp, 0, 5, 3, Enter, sinte::ritmesArp, "Durada de cada pas" },
            { "arpo", "Octaves", SArp, 1, 4, 1, Enter, nullptr, "Octaves que recorre" },
            { "arpg", "Gate", SArp, 0.05f, 1, 0.5f, Lineal, nullptr, "Durada de cada nota (al màxim, lligades)" },
            { "arpsw", "Swing", SArp, 0, 0.6f, 0, Lineal, nullptr, "Swing" },
            { "arpp", "Patró", SArp, 0, 15, 0, Enter, sinte::nomsPatrons, "Patró del seqüenciador (mode «Patró»)" },
            { "vista", "Vista", SVeu, 0, 1, 0, Enter, "Simple|Avançat", "Simple: els controls essencials. Avançat: tots" },
        };
#undef OSC
        static_assert (sizeof (infos) / sizeof (infos[0]) == numParams, "la taula de paràmetres no quadra");

        constexpr int midaSin = 2048;
        float taulaSin[midaSin + 1];
        const bool sinFet = []
        {
            for (int i = 0; i <= midaSin; ++i) taulaSin[i] = (float) std::sin (2.0 * dpi * i / midaSin);
            return true;
        }();
        inline float sinT (double fase) noexcept
        {
            fase -= std::floor (fase);
            const double x = fase * midaSin;
            const int i = (int) x;
            const float k = (float) (x - i);
            return taulaSin[i] + (taulaSin[i + 1] - taulaSin[i]) * k;
        }
        inline float formaOna (int tipus, double t, double dt, float pw) noexcept
        {
            switch (tipus)
            {
                case OSerra: return (float) (2.0 * t - 1.0) - blep (t, dt);
                case OQuadrada: pw = 0.5f; [[fallthrough]];
                case OPols:
                {
                    double t2 = t - pw + 1.0; t2 -= std::floor (t2);
                    const float n = t < pw ? 1.0f : -1.0f;
                    return n + blep (t, dt) - blep (t2, dt) - (2.0f * pw - 1.0f);   // sense continua (acoblat en alterna)
                }
                case OTriangle: return (float) (4.0 * std::abs (t - 0.5) - 1.0);
                default: return sinT (t);
            }
        }
        inline float gTan (double hz, double fs) noexcept { return (float) std::tan (dpi * std::min (hz, fs * 0.42) / fs); }
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
            case O1Peu: case O2Peu: case O3Peu:
            {
                static const char* peus[] = { "32'", "16'", "8'", "4'", "2'" };
                return peus[std::clamp ((int) std::lround (v) + 2, 0, 4)];
            }
            case O1Semi: case O2Semi: case O3Semi: return (v > 0 ? "+" : "") + num (v, 0) + " st";
            case O1Fi: case O2Fi: case O3Fi: return (v > 0 ? "+" : "") + num (v, 0) + " ct";
            case O1Pw: case O2Pw: case O3Pw: return num (v * 100.0, 0) + " %";
            case EnvA2: return (v > 0 ? "+" : "") + num (v, 1) + " st";
            case FTall: return v < 1000.0f ? num (v, 0) + " Hz" : num (v / 1000.0, v < 10000.0f ? 2 : 1) + " kHz";
            case FHpf: return v < 12.0f ? std::string ("Off") : num (v, 0) + " Hz";
            case AA: case AD: case AR: case FA: case FD: case FR: return ms (v);
            case Glide: case LRetard: return v < 0.5f ? std::string ("Off") : ms (v);
            case LVel: case PhVel: return num (v, v < 10.0f ? 2 : 1) + " Hz";
            case LTo: case RodaTo: return num (v, 2) + " st";
            case Volum: return (v > 0 ? "+" : "") + num (v, 1) + " dB";
            case BendRang: return num (v, 0) + " st";
            case Veus: return num (v, 0);
            case Unison: return v < 1.5f ? std::string ("Off") : num (v, 0) + " veus";
            case UniDetune: return num (v * 40.0, 0) + " ct";
            case ArpOct: return num (v, 0) + " oct";
            case ArpGate: return v >= 0.99f ? std::string ("Lligat") : num (v * 100.0, 0) + " %";
            default:
                if (i.mn < 0.0f) return (v > 0 ? "+" : "") + num (v * 100.0, 0) + " %";
                return num (v * 100.0, 0) + " %";
        }
    }

    //==========================================================================
    Vintage::Vintage() : sinte::Base (infos, numParams, "mibase-vintage", { ArpMode, ArpRitme, ArpOct, ArpGate, ArpSwing, ArpPatro })
    {
        (void) sinFet;
        assignaCC (74, FTall);
        assignaCC (71, FRes);
        assignaCC (73, AA);
        assignaCC (72, AR);
        dl.posaTempo (&sinte::tempo());
        rv.posaTempo (&sinte::tempo());
        dl.posa (delay::Sync, 1.0f);
        dl.posa (delay::HighCut, 5000.0f);
        dl.posa (delay::LowCut, 150.0f);
        rv.posa (reverb::PreDelay, 8.0f);
        rv.posa (reverb::LowCut, 150.0f);
        veus.resize (maxVeus);
        // les toleràncies de cada targeta: sempre les mateixes (com un aparell concret)
        std::uint32_t s = 0x9e3779b9u;
        auto r = [&s] { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 8388608.0f - 1.0f; };
        for (auto& t : tol) { for (auto& x : t.to) x = r(); t.tall = r(); t.res = r(); t.env = r(); t.pan = r(); }
    }

    std::string Vintage::nomComplet (int i) const
    {
        static const char* seccions[] = { "Osc", "Modulació", "Mesclador", "Filtre", "Env", "LFO", "Analògic", "FX", "Veu", "Arp" };
        const auto& x = info (i);
        auto amb = [&] (const char* pre) { return std::string (pre) + " · " + x.nom; };
        if (i >= O1Ona && i <= O1Niv) return amb ("Osc 1");
        if (i >= O2Ona && i <= O2Niv) return amb ("Osc 2");
        if (i >= O3Ona && i <= O3Niv) return amb ("Osc 3");
        if (i >= AA && i <= AR) return amb ("Env volum");
        if (i >= FA && i <= FR) return amb ("Env filtre");
        return std::string (seccions[x.seccio]) + " · " + x.nom;
    }

    void Vintage::preparaSo (double f)
    {
        fsI = f * sobre;
        veus.assign (maxVeus, Veu());
        delL.buida(); delR.buida();
        numPila = 0;
        dcL = dcR = dcxL = dcxR = 0.0f;
        calL = calR = 0.0f;
        chL.assign ((size_t) (f * 0.02) + 4, 0.0f);
        chR.assign ((size_t) (f * 0.02) + 4, 0.0f);
        chPos = 0;
        std::memset (phAp, 0, sizeof (phAp));
        phFb[0] = phFb[1] = 0.0f;
        dl.prepara (f, tros);
        rv.prepara (f, tros);
        for (auto& x : darrerDl) x = -1.0f;
        for (auto& x : darrerRv) x = -1.0f;
        // les veus comencen amb fases diferents (oscil·ladors lliures)
        for (auto& v : veus) for (auto& p : v.ph) p = 0.5 + 0.5 * aleat();
    }

    bool Vintage::veusSonen() const noexcept
    {
        for (const auto& v : veus) if (v.viva) return true;
        return valor (DlMix) > 0.001f || valor (RvMix) > 0.001f ? nivell() > 1e-4f : false;
    }
    int Vintage::numVeusActives() const noexcept { int n = 0; for (const auto& v : veus) n += v.viva ? 1 : 0; return n; }
    void Vintage::apagaVeus() noexcept
    {
        for (auto& v : veus) if (v.viva) { v.etapaA = 4; v.tecla = false; v.etapaF = 3; }
        numPila = 0;
    }
    int Vintage::numUnison() const noexcept { return std::clamp ((int) std::lround (valor (Unison)), 1, 8); }

    // ---- notes ----
    void Vintage::dispara (Veu& v, int nota, float vel, bool retriga, bool glide) noexcept
    {
        const bool estava = v.viva && v.envA > 1e-4f;
        v.nota = nota;
        v.objectiu = nota;
        if (! glide || ! v.viva) v.semis = nota;
        v.vel = vel;
        v.tecla = true;
        v.ordre = ++comptaOrdre;
        if (retriga)
        {
            v.etapaA = v.etapaF = 1;
            v.temps = 0.0f;
            if (valor (LPerVeu) > 0.5f) { v.faseLfo = 0.0; v.shLfo = aleat(); v.shAnt = v.shLfo; }
            const bool an = valor (Analogic) > 0.5f;
            const float ins = an ? valor (Inestable) : 0.0f;
            if (valor (FaseLliure) > 0.5f)   // reinicia: totes comencen al mateix punt (amb una mica de dispersió si és inestable)
                for (auto& p : v.ph) { p = 0.02 * ins * (0.5 + 0.5 * aleat()); }
            else if (! estava)
                for (auto& p : v.ph) p += 0.1 * ins * aleat();   // lliures: segueixen on eren
            if (! estava)
            {
                v.envA = v.envF = 0.0f;
                for (auto& x : v.ld) x = 0.0f;
                v.sv1 = v.sv2 = v.hp = v.hpx = 0.0f;
                v.phSub = 0.0;
            }
        }
        v.viva = true;
        posaHz ((float) (440.0 * std::pow (2.0, (nota - 69) / 12.0)));
    }

    void Vintage::allibera (Veu& v) noexcept
    {
        v.tecla = false;
        if (v.etapaA != 0 && v.etapaA != 4) v.etapaA = 3;
        if (v.etapaF != 0) v.etapaF = 3;
    }

    void Vintage::notaOn (int nota, int vel) noexcept
    {
        const float vf = std::clamp (vel / 127.0f, 0.0f, 1.0f);
        const int mode = (int) valor (Mode);
        const bool ambGlide = valor (Glide) > 0.5f;
        const int U = numUnison();
        const float det = valor (UniDetune) * 0.4f;
        auto posaUnison = [&] (Veu& v, int u)
        {
            const float k = U > 1 ? 2.0f * (float) u / (float) (U - 1) - 1.0f : 0.0f;
            v.uniDet = k * det;
            v.uniPan = U > 1 ? k * 0.7f : 0.0f;
        };
        if (mode == 2)
        {
            const int limit = std::clamp ((int) valor (Veus) * U, 1, maxVeus);
            for (int i = limit; i < maxVeus; ++i) if (veus[(size_t) i].viva) { veus[(size_t) i].etapaA = 4; veus[(size_t) i].tecla = false; }
            ++comptaGrup;
            for (int u = 0; u < U; ++u)
            {
                Veu* tria = nullptr;
                for (int i = 0; i < limit; ++i) if (! veus[(size_t) i].viva) { tria = &veus[(size_t) i]; break; }
                if (tria == nullptr) for (int i = 0; i < limit; ++i) { auto& v = veus[(size_t) i]; if (v.grup != comptaGrup && ! v.tecla && (tria == nullptr || v.envA < tria->envA)) tria = &v; }
                if (tria == nullptr) for (int i = 0; i < limit; ++i) { auto& v = veus[(size_t) i]; if (v.grup != comptaGrup && (tria == nullptr || v.ordre < tria->ordre)) tria = &v; }
                if (tria == nullptr) break;
                const bool g = ambGlide && darreraNota >= 0.0;
                if (g && ! tria->viva) { tria->semis = darreraNota; tria->viva = true; tria->envA = 0.0f; tria->etapaA = 0; }
                posaUnison (*tria, u);
                tria->grup = comptaGrup;
                dispara (*tria, nota, vf, true, g);
            }
            darreraNota = nota;
            return;
        }
        for (int i = 0; i < numPila; ++i)
            if (pila[i] == nota) { std::memmove (pila + i, pila + i + 1, sizeof (int) * (size_t) (numPila - i - 1)); --numPila; break; }
        const bool lligada = numPila > 0;
        if (numPila < 32) pila[numPila++] = nota;
        for (int i = U; i < maxVeus; ++i) if (veus[(size_t) i].viva) { veus[(size_t) i].etapaA = 4; veus[(size_t) i].tecla = false; }
        for (int u = 0; u < U; ++u)
        {
            auto& v = veus[(size_t) u];
            posaUnison (v, u);
            if (mode == 1) dispara (v, nota, lligada && v.viva ? v.vel : vf, ! (lligada && v.viva), lligada && v.viva);
            else dispara (v, nota, vf, true, ambGlide && v.viva && v.envA > 1e-4f);
        }
        darreraNota = nota;
    }

    void Vintage::notaOff (int nota) noexcept
    {
        const int mode = (int) valor (Mode);
        if (mode == 2)
        {
            for (auto& v : veus) if (v.viva && v.tecla && v.nota == nota) allibera (v);
            return;
        }
        for (int i = 0; i < numPila; ++i)
            if (pila[i] == nota) { std::memmove (pila + i, pila + i + 1, sizeof (int) * (size_t) (numPila - i - 1)); --numPila; break; }
        const int U = numUnison();
        if (veus[0].nota != nota) return;
        for (int u = 0; u < U; ++u)
        {
            auto& v = veus[(size_t) u];
            if (numPila > 0) dispara (v, pila[numPila - 1], v.vel, mode == 0, mode == 1 || valor (Glide) > 0.5f);
            else allibera (v);
        }
    }

    float Vintage::valorLfo (double ph, float sh, float shAnt) const noexcept
    {
        (void) shAnt;
        switch ((int) valor (LOna))
        {
            case 0: return 1.0f - 4.0f * std::abs ((float) ph - 0.5f);
            case 1: return sinT (ph);
            case 2: return 1.0f - 2.0f * (float) ph;
            case 3: return 2.0f * (float) ph - 1.0f;
            case 4: return ph < 0.5 ? 1.0f : -1.0f;
            default: return sh;
        }
    }

    //==========================================================================
    void Vintage::renderitza (float* L, float* R, int n) noexcept
    {
#ifdef MIBASE_VINTAGE_FTZ
        struct Ftz { unsigned int csr = _mm_getcsr(); Ftz() { _mm_setcsr (csr | 0x8040); } ~Ftz() { _mm_setcsr (csr); } } ftz;
#endif
        const int nI = n * sobre;
        std::memset (bufL, 0, sizeof (float) * (size_t) nI);
        std::memset (bufR, 0, sizeof (float) * (size_t) nI);
        constexpr int sub = 16;
        const double tempoAra = std::max (20.0, sinte::tempo().load (std::memory_order_relaxed));
        const int ritme = (int) valor (LRitme);
        const double hzLfo = ritme <= 0 ? (double) valor (LVel) : tempoAra / 60.0 / sinte::negresRitme (ritme);
        float lfoG = 0.0f;
        for (int s = 0; s < nI; s += sub)
        {
            const int m = std::min (sub, nI - s);
            faseG += hzLfo * (double) m / fsI;
            if (faseG >= 1.0) { faseG -= std::floor (faseG); shGAnt = shG; shG = aleat(); }
            lfoG = valorLfo (faseG, shG, shGAnt);
            for (int i = 0; i < maxVeus; ++i) if (veus[(size_t) i].viva) renderitzaVeu (veus[(size_t) i], i, s, m, lfoG);
        }
        lfoV.store (lfoG, std::memory_order_relaxed);
        float oL[tros], oR[tros];
        delL.processa (bufL, oL, n);
        delR.processa (bufR, oR, n);

        // caràcter global: soroll de fons i brunzit (només mentre sona), calidesa i edat
        const bool an = valor (Analogic) > 0.5f;
        const float sor = an ? valor (Soroll) : 0.0f, cal = an ? valor (Calidesa) : 0.0f, edat = an ? valor (Edat) : 0.0f;
        bool alguna = false;
        for (const auto& v : veus) alguna = alguna || v.viva;
        const float cLp = (float) (1.0 - std::exp (-2.0 * dpi * (19000.0 - 8000.0 * cal - 6000.0 * edat) / fs));
        const bool filtraCal = cal > 0.001f || edat > 0.001f;
        for (int i = 0; i < n; ++i)
        {
            nivellVeus += ((alguna ? 1.0f : 0.0f) - nivellVeus) * (alguna ? 0.01f : 0.00005f);
            float yl = oL[i], yr = oR[i];
            if (sor > 0.001f && nivellVeus > 1e-4f)
            {
                faseHum += 50.0 / fs; if (faseHum >= 1.0) faseHum -= 1.0;
                const float hum = (sinT (faseHum) + 0.4f * sinT (faseHum * 3.0)) * 0.00025f;
                const float k = sor * (1.0f + edat) * nivellVeus;
                yl += k * (aleat() * 0.0007f + hum);
                yr += k * (aleat() * 0.0007f + hum);
            }
            if (cal > 0.001f) { yl += cal * 0.18f * yl * yl; yr += cal * 0.18f * yr * yr; }   // harmònics parells (la continua la treu el filtre de sota)
            if (filtraCal) { calL += (yl - calL) * cLp; calR += (yr - calR) * cLp; yl = calL; yr = calR; }
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
            auto sl = [] (float x) { const float ax = std::abs (x); return ax <= 0.8f ? x : std::copysign (0.8f + 0.18f * tanhR ((ax - 0.8f) / 0.18f), x); };
            L[i] = sl ((mid + sid) * volSuau);
            R[i] = sl ((mid - sid) * volSuau);
        }
        // per a la interfície: el filtre de la veu més nova
        const Veu* nova = nullptr;
        for (const auto& v : veus) if (v.viva && (nova == nullptr || v.ordre > nova->ordre)) nova = &v;
        filtreV.store (nova != nullptr ? nova->envF : 0.0f, std::memory_order_relaxed);
    }

    void Vintage::renderitzaVeu (Veu& v, int idx, int s0, int m, float lfoGlob) noexcept
    {
        const double fsV = fsI;
        const double dtS = (double) m / fsV;
        const auto& t = tol[(size_t) idx];
        const bool an = valor (Analogic) > 0.5f;
        const float der = an ? valor (Deriva) : 0.0f, ins = an ? valor (Inestable) : 0.0f, vfil = an ? valor (VarFiltre) : 0.0f;
        const float sat = an ? valor (Saturacio) : 0.0f, edat = an ? valor (Edat) : 0.0f, vest = an ? valor (VarEstereo) : 0.0f;
        v.temps += (float) dtS;

        // ---- glide ----
        const float gl = valor (Glide);
        if (gl > 0.5f) v.semis += (v.objectiu - v.semis) * (1.0 - std::exp (-dtS / (gl * 0.001 / 3.0)));
        else v.semis = v.objectiu;

        // ---- deriva (procés d'Ornstein-Uhlenbeck per oscil·lador: lent i sense memòria llarga) ----
        if (der > 0.0f || edat > 0.0f)
        {
            const float sq = (float) std::sqrt (dtS) * 1.732f * 1.7f;
            for (auto& d : v.deriva) d += -d * 1.5f * (float) dtS + sq * aleat();
        }
        // soroll lent per a la modulació
        v.sorollMod += (aleat() * 1.6f - v.sorollMod) * (float) std::min (1.0, dtS * 18.0);

        // ---- font de modulació ----
        float lfo;
        if (valor (LPerVeu) > 0.5f)
        {
            const int ritme = (int) valor (LRitme);
            const double hz = ritme <= 0 ? (double) valor (LVel) : std::max (20.0, sinte::tempo().load (std::memory_order_relaxed)) / 60.0 / sinte::negresRitme (ritme);
            v.faseLfo += hz * dtS;
            if (v.faseLfo >= 1.0) { v.faseLfo -= std::floor (v.faseLfo); v.shAnt = v.shLfo; v.shLfo = aleat(); }
            lfo = valorLfo (v.faseLfo, v.shLfo, v.shAnt);
        }
        else lfo = lfoGlob;
        const int font = (int) valor (LFont);
        float src = font == 0 ? lfo : font == 1 ? v.dVa : std::clamp (v.sorollMod, -1.0f, 1.0f);
        const float ret = valor (LRetard);
        const float entra = ret < 0.5f ? 1.0f : std::min (1.0f, v.temps * 1000.0f / ret);
        src *= entra;
        const float roda = rodaMod;
        const float modTo = src * (valor (LTo) + valor (RodaTo) * roda);
        const float modPw = src * valor (LPw) * 0.42f;
        const float modTall = src * (valor (LTall) + valor (RodaTall) * roda) * 3.0f;
        const float modVol = 1.0f - valor (LVol) * 0.5f * (1.0f + lfo * entra);

        // ---- envolupants (corbes de condensador; l'edat les fa menys exactes) ----
        const float envK = 1.0f + t.env * (edat * 0.25f + vfil * 0.1f);
        auto cf = [&] (float msT, float div) { return (float) (1.0 - std::exp (-1.0 / (std::max (0.0002, (double) msT * 0.001 * envK / div) * fsV))); };
        const float cAA = cf (valor (AA), 1.466f), cAD = cf (valor (AD), 4.6f), cAR = cf (valor (AR), 4.6f), sA = valor (AS);
        const float cFA = cf (valor (FA), 1.466f), cFD = cf (valor (FD), 4.6f), cFR = cf (valor (FR), 4.6f), sF = valor (FS);
        const float cRap = cf (3.0f, 4.6f);

        // ---- to de cada oscil·lador ----
        const double bend = (double) bendCru * valor (BendRang);
        const double baseSemis = v.semis + bend + modTo + v.uniDet;
        const float env2 = valor (EnvA2);
        const bool baixa = valor (O3Baixa) > 0.5f;
        double dt[3];
        int tipus[3];
        float pw[3], niv[3];
        for (int o = 0; o < 3; ++o)
        {
            const int b = o * 6;
            tipus[o] = (int) valor (O1Ona + b);
            niv[o] = valor (O1Niv + b);
            pw[o] = std::clamp (valor (O1Pw + b) + modPw, 0.04f, 0.5f);
            const double cents = valor (O1Fi + b) + (double) v.deriva[o] * der * 6.0 * (1.0 + edat) + (double) t.to[o] * edat * 8.0;
            double st = 12.0 * valor (O1Peu + b) + valor (O1Semi + b) + cents / 100.0;
            if (o == 2 && baixa)
            {
                dt[o] = std::pow (2.0, st / 12.0) / fsV;   // 1 Hz a 8' (de 0,25 a 4 Hz amb els peus)
                continue;
            }
            st += baseSemis;
            if (o == 1) st += (double) env2 * v.envF;
            dt[o] = 440.0 * std::pow (2.0, (st - 69.0) / 12.0) / fsV;
            if (ins > 0.0f) dt[o] *= 1.0 + 0.0012 * ins * aleat();
            dt[o] = std::min (dt[o], 0.45);
        }
        const float fm = valor (Fm) * 4.0f;
        const bool sync = valor (Sync) > 0.5f;
        const float subN = valor (SubNiv), nzN = valor (NzNiv), ringN = valor (Ring);
        const bool subSin = valor (SubOna) > 0.5f, rosaC = valor (NzColor) > 0.5f;
        const double dtSub = dt[0] / (valor (SubOct) > 0.5f ? 4.0 : 2.0);

        // ---- filtre ----
        const int ft = (int) valor (FTipus), fmo = (int) valor (FMode);
        const bool p24 = valor (FPendent) > 0.5f;
        const double octs = valor (FEnv) * v.envF * 7.0 + valor (FKey) * (v.semis - 60.0) / 12.0 + valor (FVel) * (v.vel - 0.6f) * 2.5
                          + modTall + valor (AtTall) * pressioAra * 2.5 + t.tall * (vfil * 0.4f + edat * 0.15f);
        const double fc = std::clamp ((double) valor (FTall) * std::pow (2.0, octs), 16.0, 20000.0);
        const float g = gTan (fc, fsV);
        const float G = g / (1.0f + g);
        float res = std::clamp (valor (FRes) + t.res * vfil * 0.12f, 0.0f, 1.0f);
        const float drv = valor (FDrive);
        const float guany = 1.0f + drv * 5.0f;
        // ladder / OTA
        const float k = ft == FLadder ? res * 4.15f : res * 3.98f;
        const float comp = ft == FLadder ? 0.3f : 0.7f;
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        // SEM / MS-20 (variable d'estat)
        const float kS = ft == FSem ? 2.0f - 1.9f * res : 2.0f - 2.12f * res;
        const float a1 = 1.0f / (1.0f + g * (g + kS)), a2 = g * a1, a3 = g * a2;
        const float hpfA = valor (FHpf) > 12.0f ? (float) std::exp (-2.0 * dpi * valor (FHpf) / fsV) : 1.0f;
        const float velG = 1.0f - valor (VelVol) * (1.0f - v.vel);
        const float satK = 1.0f + 3.0f * sat;
        float panV = std::clamp (v.uniPan + t.pan * vest * 0.6f, -1.0f, 1.0f);
        const float gl0 = std::sqrt (0.5f * (1.0f - panV)), gr0 = std::sqrt (0.5f * (1.0f + panV));
        const float sortida = 0.5f / (1.0f + drv * 1.2f);

        for (int i = 0; i < m; ++i)
        {
            // envolupants
            switch (v.etapaA)
            {
                case 1: v.envA += (1.3f - v.envA) * cAA; if (v.envA >= 1.0f) { v.envA = 1.0f; v.etapaA = 2; } break;
                case 2: v.envA += (sA - v.envA) * cAD; break;
                case 3: v.envA -= v.envA * cAR; break;
                case 4: v.envA -= v.envA * cRap; break;
                default: break;
            }
            switch (v.etapaF)
            {
                case 1: v.envF += (1.3f - v.envF) * cFA; if (v.envF >= 1.0f) { v.envF = 1.0f; v.etapaF = 2; } break;
                case 2: v.envF += (sF - v.envF) * cFD; break;
                case 3: v.envF -= v.envF * cFR; break;
                default: break;
            }
            // oscil·lador 3 primer (modula l'1)
            const float o3 = formaOna (tipus[2], v.ph[2], dt[2], pw[2]);
            v.dVa = o3;
            v.ph[2] += dt[2]; if (v.ph[2] >= 1.0) v.ph[2] -= 1.0;
            // oscil·lador 1 (amb FM lineal)
            double d1 = dt[0] * (1.0 + (double) (fm * o3));
            const float o1 = formaOna (tipus[0], v.ph[0], std::abs (d1), pw[0]);
            v.ph[0] += d1;
            bool reinicia = false; double frac = 0.0;
            if (v.ph[0] >= 1.0) { v.ph[0] -= 1.0; reinicia = true; frac = d1 > 0.0 ? v.ph[0] / d1 : 0.0; }
            else if (v.ph[0] < 0.0) v.ph[0] += 1.0;
            // oscil·lador 2 (sync a l'1)
            const float o2 = formaOna (tipus[1], v.ph[1], dt[1], pw[1]);
            v.ph[1] += dt[1]; if (v.ph[1] >= 1.0) v.ph[1] -= 1.0;
            if (sync && reinicia) v.ph[1] = frac * dt[1];
            // sub
            float sb = 0.0f;
            if (subN > 0.0f)
            {
                sb = subSin ? sinT (v.phSub) : formaOna (OQuadrada, v.phSub, dtSub, 0.5f);
                v.phSub += dtSub; if (v.phSub >= 1.0) v.phSub -= 1.0;
            }
            // soroll
            float nz = 0.0f;
            if (nzN > 0.0f)
            {
                const float w = aleat();
                if (rosaC)
                {
                    v.rosa[0] = 0.99765f * v.rosa[0] + w * 0.0990460f;
                    v.rosa[1] = 0.96300f * v.rosa[1] + w * 0.2965164f;
                    v.rosa[2] = 0.57000f * v.rosa[2] + w * 1.0526913f;
                    nz = (v.rosa[0] + v.rosa[1] + v.rosa[2] + w * 0.1848f) * 0.25f;
                }
                else nz = w * 0.6f;
            }
            float x = niv[0] * o1 + niv[1] * o2 + (baixa ? 0.0f : niv[2] * o3) + subN * sb + nzN * nz + ringN * o1 * o2;
            x *= guany;
            // filtre
            float y;
            if (ft == FLadder || ft == FOta)
            {
                const float xin = x * (1.0f + comp * k);
                const float b1 = (1.0f - G) * v.ld[0], b2 = (1.0f - G) * v.ld[1], b3 = (1.0f - G) * v.ld[2], b4 = (1.0f - G) * v.ld[3];
                const float y4e = (G4 * xin + G3 * b1 + G2 * b2 + G * b3 + b4) / (1.0f + k * G4);
                float u = xin - k * y4e;
                u = ft == FLadder ? tanhR (u) : tanhR (u * 0.8f) * 1.25f;
                auto etapa = [G] (float in, float& s) { const float vv = (in - s) * G; const float out = vv + s; s = out + vv; return out; };
                float y1, y2, y3, y4;
                if (ft == FLadder) { y1 = etapa (u, v.ld[0]); y2 = etapa (y1, v.ld[1]); y3 = etapa (y2, v.ld[2]); y4 = etapa (y3, v.ld[3]); }
                else { y1 = etapa (u, v.ld[0]); y2 = etapa (tanhR (y1), v.ld[1]); y3 = etapa (tanhR (y2), v.ld[2]); y4 = etapa (tanhR (y3), v.ld[3]); }
                if (p24)
                    y = fmo == MLp ? y4 : fmo == MBp ? 3.0f * (y2 - 2.0f * y3 + y4) : fmo == MHp ? u - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4
                                    : u - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + 2.0f * y4;
                else
                    y = fmo == MLp ? y2 : fmo == MBp ? 2.0f * (y1 - y2) : fmo == MHp ? u - 2.0f * y1 + y2 : u - 2.0f * y1 + 2.0f * y2;
            }
            else
            {
                const float xin = ft == FMs20 ? x * 1.4f : x;
                const float v3 = xin - v.sv2;
                const float v1 = a1 * v.sv1 + a2 * v3;
                const float v2 = v.sv2 + a2 * v.sv1 + a3 * v3;
                v.sv1 = 2.0f * v1 - v.sv1;
                v.sv2 = 2.0f * v2 - v.sv2;
                if (ft == FSem) v.sv1 = 2.0f * tanhR (v.sv1 * 0.5f);
                else { v.sv1 = tanhR (v.sv1 * 1.2f) / 1.2f; v.sv2 = tanhR (v.sv2 * 0.8f) / 0.8f; }   // el MS-20 xiula però no s'escapa
                const float lp = v2, bp = v1, hp = xin - kS * v1 - v2;
                y = fmo == MLp ? lp : fmo == MBp ? bp : fmo == MHp ? hp : lp + hp;
                if (ft == FMs20) y = tanhR (y * 1.1f);
            }
            // HPF
            if (hpfA < 1.0f) { const float h = hpfA * (v.hp + y - v.hpx); v.hpx = y; v.hp = h; y = h; }
            // amplificador
            float a = y * v.envA * velG * modVol * sortida;
            if (sat > 0.001f) a = tanhR (a * satK) / satK;
            bufL[s0 + i] += a * gl0;
            bufR[s0 + i] += a * gr0;
        }
        if ((v.etapaA == 3 || v.etapaA == 4) && v.envA < 2e-5f) { v.viva = false; v.etapaA = 0; v.etapaF = 0; v.tecla = false; }
        if (! std::isfinite (v.ld[3]) || ! std::isfinite (v.sv2)) { for (auto& q : v.ld) q = 0.0f; v.sv1 = v.sv2 = 0.0f; }
    }

    void Vintage::efectes (float* L, float* R, int n) noexcept
    {
        // phaser (4 passa-tot de primer ordre, escombrat exponencial de 180 Hz a 2,2 kHz)
        const float phm = valor (PhMix);
        if (phm > 0.001f)
        {
            const double dph = valor (PhVel) / fs;
            for (int i = 0; i < n; ++i)
            {
                phFase += dph; if (phFase >= 1.0) phFase -= 1.0;
                for (int c = 0; c < 2; ++c)
                {
                    const float l = 0.5f + 0.5f * sinT (phFase + c * 0.25);
                    const float hz = 180.0f * std::pow (12.0f, l);
                    const float tg = (float) std::tan (dpi * hz / fs);
                    const float co = (tg - 1.0f) / (tg + 1.0f);
                    float& x = c == 0 ? L[i] : R[i];
                    float s = x + phFb[c] * 0.45f;
                    for (int k = 0; k < 4; ++k) { const float yk = co * s + phAp[c][k]; phAp[c][k] = s - co * yk; s = yk; }
                    phFb[c] = s;
                    x = x * (1.0f - 0.5f * phm) + s * 0.5f * phm;
                }
            }
        }
        // chorus de BBD (Juno): dos retards amb un LFO de triangle en contrafase i un filtre fosc
        const int ch = (int) valor (Chorus);
        if (ch > 0 && ! chL.empty())
        {
            const int Nn = (int) chL.size();
            static const float hz[] = { 0.0f, 0.513f, 0.863f, 9.75f };
            static const float bas[] = { 0.0f, 3.5f, 3.5f, 3.3f }, prof[] = { 0.0f, 1.85f, 1.85f, 0.3f };
            const double dph = hz[ch] / fs;
            const float base = bas[ch] * 0.001f * (float) fs, pr = prof[ch] * 0.001f * (float) fs;
            const float cLp = (float) (1.0 - std::exp (-2.0 * dpi * 8000.0 / fs));
            auto llegeix = [&] (const std::vector<float>& b, float d)
            {
                float xx = (float) chPos - d;
                while (xx < 0.0f) xx += (float) Nn;
                int a = (int) xx;
                if (a >= Nn) a -= Nn;
                const int c = (a + 1) % Nn;
                const float kk = xx - std::floor (xx);
                return b[(size_t) a] + (b[(size_t) c] - b[(size_t) a]) * kk;
            };
            for (int i = 0; i < n; ++i)
            {
                const float mono = 0.5f * (L[i] + R[i]);
                chL[(size_t) chPos] = mono;
                chFase += dph; if (chFase >= 1.0) chFase -= 1.0;
                const float tri = 1.0f - 4.0f * std::abs ((float) chFase - 0.5f);
                const float wl = llegeix (chL, base + pr * tri), wr = llegeix (chL, base - pr * tri);
                chLpL += (wl - chLpL) * cLp; chLpR += (wr - chLpR) * cLp;
                L[i] = L[i] * 0.78f + chLpL * 0.62f;
                R[i] = R[i] * 0.78f + chLpR * 0.62f;
                chPos = (chPos + 1) % Nn;
            }
        }
        // delay
        const float dm = valor (DlMix);
        if (dm > 0.001f)
        {
            const float vv[4] = { (float) notaDelay[std::clamp ((int) valor (DlNota), 0, 7)], valor (DlFeedback) * 100.0f, dm * 100.0f, valor (DlTipus) };
            if (vv[0] != darrerDl[0]) dl.posa (delay::Nota, vv[0]);
            if (vv[1] != darrerDl[1]) dl.posa (delay::Feedback, vv[1]);
            if (std::abs (vv[2] - darrerDl[2]) > 0.2f) dl.posa (delay::Mix, vv[2]);
            if (vv[3] != darrerDl[3]) dl.posa (delay::CaracterP, vv[3] > 0.5f ? (float) delay::Analogic : (float) delay::Cinta);
            for (int k = 0; k < 4; ++k) darrerDl[k] = vv[k];
            dl.processa (L, R, n);
        }
        // reverb
        const float rm = valor (RvMix);
        if (rm > 0.001f)
        {
            static const int algs[] = { reverb::Spring, reverb::Plate, reverb::Chamber, reverb::Hall };
            const int t = std::clamp ((int) valor (RvTipus), 0, 3);
            const float mida = valor (RvMida);
            if ((float) t != darrerRv[2]) { rv.posa (reverb::Alg, (float) algs[t]); darrerRv[2] = (float) t; darrerRv[0] = -1.0f; }
            if (mida != darrerRv[0]) { rv.posa (reverb::Decay, 0.6f + mida * mida * 6.0f); rv.posa (reverb::Size, 30.0f + 70.0f * mida); darrerRv[0] = mida; }
            if (std::abs (rm - darrerRv[1]) > 0.002f) { rv.posa (reverb::Mix, rm * 100.0f); darrerRv[1] = rm; }
            rv.processa (L, R, n);
        }
    }

    //==========================================================================
    float previsualitza (const std::string& estat, float* sortida, int n, int nota)
    {
        auto sp = std::make_unique<Vintage>();
        auto& s = *sp;
        s.posaEstat (estat.empty() ? estatInicial() : estat);
        for (int p : { (int) Volum, (int) Ample, (int) DlMix, (int) RvMix, (int) PhMix, (int) ArpMode, (int) LTo, (int) LPw, (int) LTall, (int) LVol, (int) Chorus, (int) Analogic })
            s.posa (p, 0.0f);
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
            const int a = std::min ((int) x, (int) buf.size() - 2);
            const float k = (float) (x - a);
            sortida[i] = buf[(size_t) a] + (buf[(size_t) a + 1] - buf[(size_t) a]) * k;
            pic = std::max (pic, std::abs (sortida[i]));
        }
        if (pic > 1e-6f) for (int i = 0; i < n; ++i) sortida[i] /= pic;
        return pic;
    }
}
