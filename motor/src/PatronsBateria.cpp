#include <mibase/PatronsBateria.h>
#include <mibase/Bateria.h>
#include <mibase/Midi.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace mibase::bateria
{
    namespace
    {
        const char* const etiquetes[numPistesPatro] = { "HH", "RD", "CR", "SN", "KK", "T1", "T2", "FT" };
        constexpr int C = casellesPatro;

        bool buida (const std::string& s) { return s.find_first_not_of ('-') == std::string::npos; }
        /** Copia el compàs «de» al compàs «a» (0..3). */
        void copiaCompas (std::string& s, int de, int a) { s.replace ((size_t) (a * C), (size_t) C, s, (size_t) (de * C), (size_t) C); }

        std::uint32_t barreja (std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d)
        {
            std::uint32_t h = a * 2654435761u ^ (b + 0x9E3779B9u) * 40503u ^ (c + 1u) * 2246822519u ^ (d + 7u) * 3266489917u;
            h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
            return h;
        }

        /** La nota de la primera fila que sigui d'un d'aquests tipus (per ordre de preferència); -1 si no n'hi ha cap.
            Amb una pista sense files, la General MIDI del primer. */
        int notaDe (const std::vector<FilaBateria>& files, std::initializer_list<const char*> tipusPref)
        {
            if (files.empty()) return notaDeTipus (*tipusPref.begin());
            for (auto* t : tipusPref)
                for (auto& f : files) if (f.tipus == t) return f.nota;
            return -1;
        }

        int notaDeCop (const std::vector<FilaBateria>& files, int pista, char ch)
        {
            switch (pista)
            {
                case PHH: return ch == 'o' ? notaDe (files, { "hihat-obert", "hihat" }) : notaDe (files, { "hihat", "hihat-pedal", "hihat-obert" });
                case PRD: return ch == 'X' ? notaDe (files, { "ride", "hihat-obert", "hihat" }) : notaDe (files, { "ride", "hihat" });
                case PCR: return notaDe (files, { "crash", "ride", "hihat-obert" });
                case PSN: return notaDe (files, { "snare", "rim", "clap" });
                case PKK: return notaDe (files, { "kick" });
                case PT1: return notaDe (files, { "tom-agut", "tom", "tom-mig", "tom-greu", "snare" });
                case PT2: return notaDe (files, { "tom-mig", "tom", "tom-agut", "tom-greu", "snare" });
                case PFT: return notaDe (files, { "tom-greu", "tom", "tom-mig", "tom-agut", "snare" });
                default: return -1;
            }
        }

        /** Un cop «de mans» (caixa o toms) que no és una nota fantasma. */
        bool copDeMans (const GraellaPatro& g, int k)
        {
            if (k < 0 || k >= llargPatro) return false;
            for (int pi : { (int) PSN, (int) PT1, (int) PT2, (int) PFT })
            {
                const char c = g.p[(size_t) pi][(size_t) k];
                if (c != '-' && c != 'g') return true;
            }
            return false;
        }
    }

    const char* nomCapa (int k)
    {
        static const char* const n[numCapes] = { "Bombo", "Caixa", "Hihat", "Plats", "Toms" };
        return k >= 0 && k < numCapes ? n[k] : "";
    }

    const std::vector<PatroBat>& patronsBat()
    {
        static const std::vector<PatroBat> v = {
#include "PatronsBateriaDades.inc"
        };
        return v;
    }

    const PatroBat* patroBat (int num)
    {
        for (auto& p : patronsBat()) if (p.num == num) return &p;
        return nullptr;
    }

    const char* nomVariacio (Variacio v)
    {
        switch (v)
        {
            case Variacio::Original:   return "Original";
            case Variacio::Estrofa:    return "Estrofa";
            case Variacio::PreTornada: return "Pre-tornada";
            case Variacio::Tornada:    return "Tornada";
            case Variacio::Pont:       return "Pont";
            case Variacio::SenseTall:  return "Sense tall";
        }
        return "";
    }

    const char* ajudaVariacio (Variacio v)
    {
        switch (v)
        {
            case Variacio::Original:   return "Tal com és: tres compassos de ritme i el quart amb el tall";
            case Variacio::Estrofa:    return "Més suau: hihat tancat, sense crash ni tall, tot una mica més fluix";
            case Variacio::PreTornada: return "Creix: caixa a totes les negres al tercer compàs i redoble que puja al quart";
            case Variacio::Tornada:    return "Oberta i forta: el hihat passa al ride amb la campana a les negres i crash a l'entrada";
            case Variacio::Pont:       return "Mig temps: la caixa només al tercer temps, bombo i plats més espaiats; al final, el tall per sortir-ne";
            case Variacio::SenseTall:  return "El ritme sense el tall del quart compàs: per repetir-lo seguit";
        }
        return "";
    }

    unsigned capaDePista (int pista)
    {
        switch (pista)
        {
            case PHH: return CapaHihat;
            case PRD: case PCR: return CapaPlats;
            case PSN: return CapaCaixa;
            case PKK: return CapaBombo;
            case PT1: case PT2: case PFT: return CapaToms;
            default: return 0;
        }
    }

    unsigned capaDeTipus (const std::string& t)
    {
        if (t == "kick") return CapaBombo;
        if (t == "snare" || t == "rim" || t == "clap") return CapaCaixa;
        if (t.rfind ("hihat", 0) == 0) return CapaHihat;
        if (t == "ride" || t == "crash") return CapaPlats;
        if (t.rfind ("tom", 0) == 0) return CapaToms;
        return 0;
    }

    unsigned capesDe (const GraellaPatro& g)
    {
        unsigned m = 0;
        for (int i = 0; i < numPistesPatro; ++i) if (! buida (g.p[(size_t) i])) m |= capaDePista (i);
        return m;
    }

    GraellaPatro graella (const PatroBat& p, Variacio v)
    {
        GraellaPatro g;
        for (auto& s : g.p) s.assign ((size_t) llargPatro, '-');
        for (auto& [et, tira] : p.files)
            for (int i = 0; i < numPistesPatro; ++i)
                if (std::strcmp (et, etiquetes[i]) == 0)
                {
                    const size_t n = std::min (std::strlen (tira), (size_t) llargPatro);
                    for (size_t k = 0; k < n; ++k) g.p[(size_t) i][k] = tira[k];
                }
        if (v == Variacio::Original) return g;

        const GraellaPatro orig = g;
        auto& HH = g.p[PHH]; auto& RD = g.p[PRD]; auto& CR = g.p[PCR]; auto& SN = g.p[PSN]; auto& KK = g.p[PKK];
        auto treuTall = [&] { for (auto& s : g.p) copiaCompas (s, 1, 3); };
        /** La segona meitat del quart compàs torna a ser la de l'original (el tall). */
        auto tallFinal = [&]
        {
            for (int i = 0; i < numPistesPatro; ++i)
                g.p[(size_t) i].replace ((size_t) (3 * C + C / 2), (size_t) (C / 2), orig.p[(size_t) i], (size_t) (3 * C + C / 2), (size_t) (C / 2));
        };

        switch (v)
        {
            case Variacio::SenseTall:
                treuTall();
                break;

            case Variacio::Estrofa:
            {
                treuTall();
                CR.assign ((size_t) llargPatro, '-');
                if (buida (HH) && ! buida (RD)) { HH = RD; RD.assign ((size_t) llargPatro, '-'); }   // el ride baixa al hihat
                for (auto& c : HH) if (c == 'o' || c == 'X') c = 'x';
                for (auto& c : RD) if (c == 'X') c = 'x';
                // una petita obertura a l'última corxera, per marcar el final de la frase
                if (HH[(size_t) (llargPatro - 2)] != '-') HH[(size_t) (llargPatro - 2)] = 'o';
                break;
            }

            case Variacio::PreTornada:
            {
                treuTall();
                for (int s : { 0, 8 }) if (SN[(size_t) (2 * C + s)] == '-') SN[(size_t) (2 * C + s)] = 'x';   // caixa a totes les negres
                const size_t b = (size_t) (3 * C);
                for (int s = 0; s < C; ++s)
                {
                    SN[b + (size_t) s] = s < 8 ? (s % 2 == 0 ? 'x' : '-') : (s == C - 1 ? 'X' : 'x');
                    KK[b + (size_t) s] = s % 4 == 0 ? 'X' : '-';
                    CR[b + (size_t) s] = '-';
                    if (s >= 8) for (int i : { (int) PHH, (int) PRD, (int) PT1, (int) PT2, (int) PFT }) g.p[(size_t) i][b + (size_t) s] = '-';
                }
                break;
            }

            case Variacio::Tornada:
            {
                if (buida (RD) && ! buida (HH))
                {
                    for (int k = 0; k < llargPatro; ++k)
                        if (HH[(size_t) k] != '-' && k % 2 == 0) RD[(size_t) k] = k % 4 == 0 ? 'X' : 'x';
                    // el tall del final es queda al hihat (les obertures de l'original)
                    for (int k = 0; k < llargPatro; ++k) if (! (k >= 3 * C + C / 2 && HH[(size_t) k] == 'o')) HH[(size_t) k] = '-';
                    for (int k = 3 * C + C / 2; k < llargPatro; ++k) if (HH[(size_t) k] == 'o') RD[(size_t) k] = '-';
                }
                else if (! buida (RD))
                {
                    for (int k = 0; k < llargPatro; k += 4) if (RD[(size_t) k] != '-') RD[(size_t) k] = 'X';
                }
                for (int k : { 0, 2 * C })
                {
                    CR[(size_t) k] = 'X';
                    KK[(size_t) k] = 'X';
                    HH[(size_t) k] = '-';   // la mà va al crash
                    RD[(size_t) k] = '-';
                }
                break;
            }

            case Variacio::Pont:
            {
                int backbeats = 0;
                for (int b = 0; b < 3; ++b) for (int s : { 4, 12 }) { const char c = orig.p[PSN][(size_t) (b * C + s)]; if (c != '-' && c != 'g') ++backbeats; }
                const bool jaMigTemps = backbeats == 0;
                for (int k = 0; k < llargPatro; ++k)
                {
                    const int s = k % C;
                    if (! jaMigTemps)
                    {
                        if (SN[(size_t) k] != 'g') SN[(size_t) k] = s == 8 ? 'X' : '-';
                        if (! (s == 0 || s == 6 || s == 7 || s == 10 || s == 11)) KK[(size_t) k] = '-';
                        for (auto* t : { &HH, &RD }) if (s % 2 != 0) (*t)[(size_t) k] = '-';
                    }
                    else
                    {
                        if (! (s == 0 || s == 10 || s == 11)) KK[(size_t) k] = '-';
                        for (auto* t : { &HH, &RD }) if (s % 4 != 0) (*t)[(size_t) k] = '-';
                    }
                    if (s == 0) KK[(size_t) k] = 'X';
                    if (k != 0) CR[(size_t) k] = '-';
                }
                tallFinal();
                break;
            }

            case Variacio::Original: break;
        }
        return g;
    }

    int velocitatPatro (const PatroBat& p, const GraellaPatro& g, Variacio v, int pista, int pas)
    {
        if (pista < 0 || pista >= numPistesPatro || pas < 0 || pas >= llargPatro) return 0;
        const auto& tira = g.p[(size_t) pista];
        const char ch = tira[(size_t) pas];
        if (ch == '-') return 0;
        const int compas = pas / C, s = pas % C;
        const std::uint32_t h = barreja ((std::uint32_t) p.num, (std::uint32_t) pista, (std::uint32_t) pas, (std::uint32_t) v);
        auto hiHa = [&] (int k) { return k >= 0 && k < llargPatro && tira[(size_t) k] != '-'; };

        // Notes fantasma: molt fluixes, i no les toca cap escala.
        if (ch == 'g') return 36 + (int) (h % 9u);

        double vel = 90.0;
        int ball = 3;   // quant balla a l'atzar (±)
        switch (pista)
        {
            case PKK:
                vel = s == 0 ? 118.0 : s % 4 == 0 ? 110.0 : s % 2 == 0 ? 104.0 : 98.0;
                if (ch == 'x') vel -= 12.0;
                if (s % 4 != 0 && hiHa (pas + 1)) vel -= 12.0;      // doble de bombo: el primer cop, més fluix
                break;

            case PSN: case PT1: case PT2: case PFT:
            {
                if (pista == PSN) vel = (s == 4 || s == 12 || s == 8) ? 116.0 : 104.0;
                else vel = pista == PFT ? 108.0 : 106.0;
                if (ch == 'x') vel -= 13.0;
                else if (pista != PSN && s % 4 != 0) vel -= 5.0;                      // als toms, el temps pesa més
                // Redobles i talls: una tirada curta de cops seguits comença fluixa i creix. Primer es mira la
                // tirada del mateix tambor; si no en fa, la de caixa i toms junts (un tall que baixa pels toms).
                // Una tirada llarga no és un tall, és el ritme (un ostinato de goliat): es queda amb els seus accents.
                auto tirada = [&] (auto&& toca, int& i0, int& i1)
                {
                    i0 = i1 = pas;
                    while (toca (i0 - 1)) --i0;
                    while (toca (i1 + 1)) ++i1;
                    const int n = i1 - i0 + 1;
                    return n <= 8 || (n <= C && i0 >= 3 * C) ? n : 0;     // un compàs sencer només compta si és l'últim
                };
                auto propi = [&] (int k) { return k >= 0 && k < llargPatro && tira[(size_t) k] != '-' && tira[(size_t) k] != 'g'; };
                int i0 = pas, i1 = pas;
                int llarg = tirada (propi, i0, i1);
                if (llarg > 0 && llarg < 3)
                {
                    int j0 = pas, j1 = pas;
                    const int junts = tirada ([&] (int k) { return copDeMans (g, k); }, j0, j1);
                    if (junts >= 3) { llarg = junts; i0 = j0; i1 = j1; }
                }
                if (llarg >= 3)
                {
                    const double t = (double) (pas - i0) / (double) (llarg - 1);
                    vel = (pista == PSN ? 82.0 : 84.0) + 38.0 * t;
                    if ((pas - i0) % 2 == 1 && pas != i1) vel -= 4.0;     // la mà feble
                }
                else if (llarg == 2)
                    vel += pas == i0 ? -12.0 : 2.0;
                if (v == Variacio::PreTornada && compas == 3 && pista == PSN) vel = 72.0 + 3.3 * (double) s;   // tot el compàs puja
                break;
            }

            case PHH:
            {
                bool semis = false;   // el compàs va a semicorxeres?
                for (int k = compas * C + 1; k < (compas + 1) * C; k += 2) if (tira[(size_t) k] != '-') { semis = true; break; }
                static const double perSemis[4] = { 100.0, 62.0, 82.0, 66.0 };
                vel = semis ? perSemis[s % 4] : (s % 4 == 0 ? 98.0 : s % 2 == 0 ? 78.0 : 70.0);
                if (ch == 'X') vel = std::max (vel + 14.0, 110.0);
                if (ch == 'o') vel = s % 4 == 0 ? 106.0 : 98.0;
                ball = 5;
                break;
            }

            case PRD:
                vel = ch == 'X' ? (s % 4 == 0 ? 112.0 : 102.0) : (s % 4 == 0 ? 94.0 : s % 2 == 0 ? 76.0 : 66.0);
                ball = 5;
                break;

            case PCR:
                vel = ch == 'X' ? 120.0 : 104.0;
                break;

            default: break;
        }
        if (v == Variacio::PreTornada && compas == 3 && pista == PKK) vel = 100.0 + (double) s;
        // Cada compàs, una mica diferent (el segon afluixa, el quart empeny), i un petit ball a cada cop.
        vel += compas == 1 ? -1.0 : compas == 3 ? 2.0 : 0.0;
        vel += (double) ((int) (h % (std::uint32_t) (2 * ball + 1)) - ball);
        vel *= v == Variacio::Estrofa ? 0.88 : v == Variacio::Tornada ? 1.05 : v == Variacio::Pont ? 0.94 : 1.0;
        return std::clamp ((int) std::lround (vel), 1, 127);
    }

    std::vector<Nota> notesDePatro (const PatroBat& p, Variacio v, unsigned capes, int compassos, long long ticksCompas,
                                    const std::vector<FilaBateria>& files, long long iniTicks)
    {
        std::vector<Nota> out;
        if (compassos <= 0 || ticksCompas <= 0) return out;
        const GraellaPatro g = graella (p, v);
        for (int b = 0; b < compassos; ++b)
            for (int pi = 0; pi < numPistesPatro; ++pi)
            {
                if ((capaDePista (pi) & capes) == 0) continue;
                for (int s = 0; s < C; ++s)
                {
                    const int pas = (b % compassosPatro) * C + s;
                    const char ch = g.p[(size_t) pi][(size_t) pas];
                    if (ch == '-') continue;
                    const int nota = notaDeCop (files, pi, ch);
                    if (nota < 0) continue;
                    Nota n;
                    n.ini = iniTicks + b * ticksCompas + s * ticksCompas / C;
                    n.dur = std::max (1LL, ticksCompas / C);
                    n.to = nota;
                    n.vel = velocitatPatro (p, g, v, pi, pas);
                    // dues pistes que cauen a la mateixa fila (una pista sense ride, per exemple): un sol cop, el més fort
                    auto it = std::find_if (out.begin(), out.end(), [&] (const Nota& x) { return x.ini == n.ini && x.to == n.to; });
                    if (it != out.end()) it->vel = std::max (it->vel, n.vel);
                    else out.push_back (n);
                }
            }
        std::vector<int> cap;
        midi::ordena (out, cap);
        return out;
    }

    int posaPatro (Clip& c, const PatroBat& p, Variacio v, unsigned capes, const std::vector<FilaBateria>& files,
                   long long ticksCompas, long long a, long long fi)
    {
        if (ticksCompas <= 0 || fi <= a || (capes & CapaTot) == 0) return 0;
        auto capaDeNota = [&] (int to) -> unsigned
        {
            for (auto& f : files) if (f.nota == to) return capaDeTipus (f.tipus);
            for (auto& t : tipus()) if (t.nota == to) return capaDeTipus (t.id);
            return 0;
        };
        c.notes.erase (std::remove_if (c.notes.begin(), c.notes.end(), [&] (const Nota& x)
                       { return x.ini >= a && x.ini < fi && (capaDeNota (x.to) & capes) != 0; }), c.notes.end());
        const int compassos = (int) ((fi - a + ticksCompas - 1) / ticksCompas);
        int posades = 0;
        for (auto& n : notesDePatro (p, v, capes, compassos, ticksCompas, files, a))
            if (n.ini < fi) { c.notes.push_back (n); ++posades; }
        std::vector<int> cap;
        midi::ordena (c.notes, cap);
        return posades;
    }
}
