#include "mibase/Guitarra.h"
#include "mibase/Midi.h"
#include "mibase/Edicio.h"
#include <nlohmann/json.hpp>
#include <map>
#include <algorithm>
#include <cstdint>
#include <cmath>

namespace mibase::guitarra
{
    namespace
    {
        int pc (int n) { return ((n % 12) + 12) % 12; }

        // Els acords oberts de tota la vida (i quatre celletes molt fetes servir), tal com es toquen.
        struct Obert { const char* acord; const char* pos; };
        constexpr Obert oberts[] = {
            { "C", "x32010" }, { "D", "xx0232" }, { "E", "022100" }, { "G", "320003" }, { "A", "x02220" },
            { "Am", "x02210" }, { "Dm", "xx0231" }, { "Em", "022000" },
            { "C7", "x32310" }, { "D7", "xx0212" }, { "E7", "020100" }, { "G7", "320001" }, { "A7", "x02020" }, { "B7", "x21202" },
            { "Am7", "x02010" }, { "Dm7", "xx0211" }, { "Em7", "022030" },
            { "Cmaj7", "x32000" }, { "Dmaj7", "xx0222" }, { "Amaj7", "x02120" }, { "Fmaj7", "xx3210" }, { "Gmaj7", "320002" }, { "Emaj7", "021100" },
            { "Dsus4", "xx0233" }, { "Dsus2", "xx0230" }, { "Asus4", "x02230" }, { "Asus2", "x02200" }, { "Esus4", "022200" },
            { "F", "133211" }, { "Fm", "133111" }, { "Bm", "x24432" }, { "B", "x24442" },
            { "Cadd9", "x32030" }, { "E9", "020102" }, { "A6", "x02222" }, { "E6", "022120" }, { "D6", "xx0202" }, { "C6", "x32210" },
            { "G6", "320000" }, { "Am6", "x02212" }, { "Em6", "022020" },
        };

        /** Les classes de to de l'acord (0..11, relatives a la fonamental). */
        std::vector<int> intervals (const harmonia::AcordPad& a)
        {
            auto b = a;
            b.inversio = 0;
            b.baix = -1;
            std::vector<int> r;
            for (int n : harmonia::notesPad (b))
            {
                const int i = pc (n - a.arrel);
                if (std::find (r.begin(), r.end(), i) == r.end()) r.push_back (i);
            }
            return r;
        }
    }

    bool Posicio::buida() const noexcept
    {
        for (int t : trast) if (t >= 0) return false;
        return true;
    }

    std::vector<int> Posicio::notes() const
    {
        std::vector<int> r;
        for (int s = 0; s < numCordes; ++s)
            if (trast[(size_t) s] >= 0) r.push_back (afinacio[(size_t) s] + trast[(size_t) s]);
        return r;
    }

    std::string Posicio::text() const
    {
        bool llarg = false;
        for (int t : trast) if (t > 9) llarg = true;
        std::string r;
        for (int s = 0; s < numCordes; ++s)
        {
            if (llarg && s > 0) r += '-';
            const int t = trast[(size_t) s];
            r += t < 0 ? std::string ("x") : std::to_string (t);
        }
        return r;
    }

    Posicio llegeixPosicio (const std::string& text)
    {
        Posicio p;
        int s = 0;
        for (size_t i = 0; i < text.size() && s < numCordes; ++i)
        {
            const char c = text[i];
            if (c == '-') continue;
            if (c == 'x' || c == 'X') { p.trast[(size_t) s++] = -1; continue; }
            if (c < '0' || c > '9') continue;
            int v = c - '0';
            if (text.find ('-') != std::string::npos)
                while (i + 1 < text.size() && text[i + 1] >= '0' && text[i + 1] <= '9') v = v * 10 + (text[++i] - '0');
            p.trast[(size_t) s++] = v;
        }
        return p;
    }

    Posicio posicio (const harmonia::AcordPad& a)
    {
        if (a.buit()) return {};
        const int baix = a.baix >= 0 ? a.baix : a.arrel;

        if (baix == a.arrel)
        {
            auto net = a;
            net.inversio = 0;
            net.baix = -1;
            for (auto& o : oberts)
                if (harmonia::llegeixAcord (o.acord) == net) return llegeixPosicio (o.pos);
        }

        // Les notes que hi han de ser i les que es poden deixar (amb 5 notes o més, la quinta; amb 6 o més, també la 9a i l'11a).
        const auto iv = intervals (a);
        std::vector<int> cal = iv;
        auto treu = [&cal] (int i) { cal.erase (std::remove (cal.begin(), cal.end(), i), cal.end()); };
        if (iv.size() >= 5) treu (7);
        if (cal.size() > 4) treu (2);
        if (cal.size() > 4) treu (5);
        bool esDe[12] = {}, esCal[12] = {};
        for (int i : iv) esDe[pc (a.arrel + i)] = true;
        for (int i : cal) esCal[pc (a.arrel + i)] = true;
        esDe[baix] = true;

        // Fora dels acords oberts de la taula, cordes a l'aire només si l'acord porta una altra nota greu (D/F#, G/B…):
        // la resta surten amb celleta, que és com es toquen.
        const bool permetAire = baix != a.arrel;
        Posicio millor;
        double millorPunts = 1e9;
        std::array<std::vector<int>, numCordes> opcions;
        for (int base = 1; base <= 10; ++base)
        {
            for (int s = 0; s < numCordes; ++s)
            {
                auto& o = opcions[(size_t) s];
                o.assign (1, -1);
                if (permetAire && base <= 2 && esDe[pc (afinacio[(size_t) s])]) o.push_back (0);
                for (int t = base; t <= base + 3; ++t)
                    if (esDe[pc (afinacio[(size_t) s] + t)]) o.push_back (t);
            }
            std::array<size_t, numCordes> k {};
            for (;;)
            {
                Posicio p;
                for (int s = 0; s < numCordes; ++s) p.trast[(size_t) s] = opcions[(size_t) s][k[(size_t) s]];

                // Vàlida? Cordes seguides, la greu fa el baix, hi són les notes que cal, i es pot posar amb 4 dits.
                int primera = -1, ultima = -1, quantes = 0, trepitjades = 0, minT = 99, maxT = 0, aire = 0;
                bool forat = false, te[12] = {};
                for (int s = 0; s < numCordes; ++s)
                {
                    const int t = p.trast[(size_t) s];
                    if (t < 0) continue;
                    if (primera < 0) primera = s;
                    if (ultima >= 0 && s != ultima + 1) forat = true;
                    ultima = s;
                    ++quantes;
                    te[pc (afinacio[(size_t) s] + t)] = true;
                    if (t == 0) ++aire;
                    else { ++trepitjades; minT = std::min (minT, t); maxT = std::max (maxT, t); }
                }
                bool val = quantes >= 4 && ! forat && pc (afinacio[(size_t) primera] + p.trast[(size_t) primera]) == baix;
                for (int n = 0; val && n < 12; ++n) if (esCal[n] && ! te[n]) val = false;
                int dits = trepitjades;
                if (val && aire == 0 && trepitjades > 0)
                {
                    dits = 1;   // la celleta
                    for (int t : p.trast) if (t > minT) ++dits;
                }
                if (val && dits > 4) val = false;
                if (val)
                {
                    int falten = 0;
                    for (int i : iv) if (! te[pc (a.arrel + i)]) ++falten;
                    // Com més avall del mànec, més cordes i menys dits, millor; les posicions molt obertes (4 trasts) costen.
                    const double punts = (trepitjades > 0 ? 1.5 * minT + (minT > 7 ? 4.0 : 0.0) : 0.0) - 3.0 * quantes + 1.5 * dits + 3.0 * falten + 1.0 * aire
                                         + (trepitjades > 0 && maxT - minT >= 3 ? 8.0 : 0.0) + (ultima < numCordes - 1 ? 2.0 : 0.0);
                    if (punts < millorPunts - 1e-9) { millorPunts = punts; millor = p; }
                }

                int s = numCordes - 1;
                while (s >= 0 && ++k[(size_t) s] >= opcions[(size_t) s].size()) k[(size_t) s--] = 0;
                if (s < 0) break;
            }
        }
        return millor;
    }

    std::vector<int> notes (const harmonia::AcordPad& a)
    {
        const auto p = posicio (a);
        if (! p.buida()) return p.notes();
        return harmonia::notesPad (a);
    }

    //==========================================================================
    std::vector<Pas> llegeixPassos (const std::string& text)
    {
        std::vector<Pas> r;
        size_t i = 0;
        while (i < text.size())
        {
            while (i < text.size() && text[i] == ' ') ++i;
            if (i >= text.size()) break;
            size_t f = i;
            while (f < text.size() && text[f] != ' ') ++f;
            std::string w = text.substr (i, f - i);
            i = f;
            Pas p;
            if (w[0] == '[')
            {
                // dits
                const auto tanca = w.find (']');
                const std::string d = w.substr (1, tanca == std::string::npos ? std::string::npos : tanca - 1);
                p.cop = Cop::Avall;
                if (d == "p" || d == "b") p.dit = Dit::P;
                else if (d == "p2") p.dit = Dit::P2;
                else if (d == "i") p.dit = Dit::I;
                else if (d == "m") p.dit = Dit::M;
                else if (d == "a") p.dit = Dit::A;
                else if (d == "pc" || d == "p+chord") p.dit = Dit::PolzeAcord;
                else if (d == "pl" || d == "pluck" || d == "ch") p.dit = Dit::Pluck;
                else if (d == "tap") p.dit = Dit::Tap;
                else if (d == "r") p.dit = Dit::Rasguejat;
                else continue;
                if (tanca != std::string::npos && w.find ('>', tanca) != std::string::npos) p.accent = true;
                r.push_back (p);
                continue;
            }
            size_t k = 0;
            if (w == ".") { r.push_back (p); continue; }
            if (w == "_") { p.cop = Cop::Silenci; r.push_back (p); continue; }
            // prefix
            char pre = 0;
            if (w.size() >= 2 && (w[0] == 'P' || w[0] == 'X' || w[0] == 'S') && std::string ("DUdu").find (w[1]) != std::string::npos)
                pre = w[k++];
            const char c = w[k];
            if (c == 'S') { p.cop = Cop::Avall; p.tec = Tecnica::Slap; ++k; }
            else if (c == 'X') { p.cop = Cop::MutAvall; ++k; }
            else if (c == 'x') { p.cop = Cop::MutAmunt; ++k; }
            else if (c == 'D' || c == 'd') { p.cop = Cop::Avall; p.suau = c == 'd'; ++k; }
            else if (c == 'U' || c == 'u') { p.cop = Cop::Amunt; p.suau = c == 'u'; ++k; }
            else continue;
            if (pre == 'P') p.tec = Tecnica::Palm;
            else if (pre == 'S') { p.tec = Tecnica::Slap; p.cop = Cop::Avall; }
            else if (pre == 'X') p.cop = p.cop == Cop::Amunt ? Cop::MutAmunt : Cop::MutAvall;
            for (; k < w.size(); ++k)
            {
                if (w[k] == '>') p.accent = true;
                else if (w[k] == '!') p.curt = true;
                else if (w.compare (k, 3, "(b)") == 0) { p.abast = Abast::Baix; k += 2; }
                else if (w.compare (k, 3, "(t)") == 0) { p.abast = Abast::Aguts; k += 2; }
            }
            r.push_back (p);
        }
        return r;
    }

    namespace
    {
        std::string dibuixa (const std::vector<Pas>& v);
    }
    std::string dibuixPassos (const std::vector<Pas>& v) { return dibuixa (v); }
    namespace
    {
        std::string dibuixa (const std::vector<Pas>& v)
        {
            std::string d;
            for (auto& s : v)
            {
                if (! d.empty()) d += ' ';
                const char* fl = s.cop == Cop::Amunt ? "\xe2\x86\x91" : "\xe2\x86\x93";
                if (s.dit != Dit::Cap)
                {
                    static const char* const noms[] = { "", "p", "p", "i", "m", "a", "pl", "p+", "tap", "r" };
                    d += noms[(int) s.dit];
                }
                else if (s.cop == Cop::Res) d += "\xc2\xb7";
                else if (s.cop == Cop::Silenci) d += "-";
                else if (s.cop == Cop::MutAvall || s.cop == Cop::MutAmunt) d += "x";
                else
                {
                    if (s.tec == Tecnica::Palm) d += "p";
                    if (s.tec == Tecnica::Slap) d += "s";
                    else d += fl;
                    if (s.abast == Abast::Baix) d += "b";
                    if (s.abast == Abast::Aguts) d += "t";
                }
                if (s.accent) d += ">";
            }
            return d;
        }
    }

    const std::vector<Patro>& patrons()
    {
        static const std::vector<Patro> llista = []
        {
            struct Def { const char* id; const char* grup; const char* nom; const char* compas; long long pas; const char* passos; double ms; int cresc; };
            static const char* const B = "Bàsics";
            static const char* const G1 = "Pop, rock i indie";
            static const char* const G2 = "Funk, percussiu i apagats";
            static const char* const G3 = "Ternaris (3/4, 6/8, 12/8)";
            static const char* const G4 = "Fingerpicking";
            static const char* const G5 = "Llatins i flamenc";
            static const char* const G6 = "Prog i metal (5/4, 7/8)";
            static const char* const V1 = "Apagats: pop rock clàssic";
            static const char* const V2 = "Apagats: pop modern i indie";
            static const char* const V3 = "Apagats: balades i ternaris";
            static const char* const C1 = "Cançons: pop i acústic";
            static const char* const C2 = "Cançons: rock i indie";
            static const char* const C3 = "Cançons: llatines i clàssics";
            const Def defs[] = {
                // ---- els de la 0.29 (no es canvien: els projectes ja els fan servir) ----
                { "un",         B, "Un sol rasgueig",            "",    0,   "D", 0, 0 },
                { "negres",     B, "Negres avall",               "4/4", 480, "D> . D . D . D .", 0, 0 },
                { "corxeres",   B, "Corxeres avall",             "4/4", 480, "D> D D D D> D D D", 0, 0 },
                { "seguit",     B, "Avall i amunt seguit",       "4/4", 480, "D> U D U D> U D U", 0, 0 },
                { "folk",       B, "Folk / pop",                 "4/4", 480, "D> . D U . U D U", 0, 0 },
                { "balada",     B, "Balada",                     "4/4", 480, "D> . D . . U D U", 0, 0 },
                { "percussiu",  B, "Percussiu",                  "4/4", 480, "D> . X U . U X U", 0, 0 },
                { "rock",       B, "Rock apagat",                "4/4", 480, "D> D X U D> D X U", 0, 0 },
                { "contratemps",B, "Contratemps",                "4/4", 480, "_ D> _ D> _ D> _ D>", 0, 0 },
                { "balada16",   B, "Balada 1/16",                "4/4", 240, "D> . . . D . D U . U D U D . D U", 0, 0 },
                { "funk",       B, "Funk 1/16",                  "4/4", 240, "D> . X U X U D . X U D> U X U X U", 0, 0 },
                { "vals",       B, "Vals (3/4)",                 "3/4", 480, "D> . D U D U", 0, 0 },
                { "accents",    B, "Accents i síncopa",          "4/4", 480, "D D> . U U D> U .", 0, 0 },
                { "poprock",    B, "Pop/rock estàndard",         "4/4", 480, "D . D U . U D U", 0, 0 },
                { "funkyx",     B, "Funky amb apagats",          "4/4", 480, "D U X U X U D U", 0, 0 },
                { "polzerasg",  B, "Polze + rasgueig (2/4)",     "2/4", 240, "[p] [a] [m] [i] D . U .", 0, 0 },
                // ---- Catàleg de 50 rasguejos moderns ----
                { "m01", G1, "Universal Pop",               "4/4", 480, "D . D U . U D U", 4, 0 },
                { "m02", G1, "Quatre a terra",              "4/4", 480, "D> . D> . D> . D> U", 0, 0 },
                { "m03", G1, "Corxeres seguides",           "4/4", 480, "D U D> U D U D> U", 3, 0 },
                { "m04", G1, "Indie rock",                  "4/4", 480, "D> . D U D U D U", 0, 0 },
                { "m05", G1, "Folk empès",                  "4/4", 480, "D . D . . U D U", 0, 0 },
                { "m06", G1, "Himne pop sincopat",          "4/4", 480, "D D> u . u D> u .", 0, 0 },
                { "m07", G1, "Balada suau",                 "4/4", 480, "d . . u d . . u", 12, 0 },
                { "m08", G1, "Alt-pop 1/16",                "4/4", 240, "D . D U U D . U", 0, 0 },
                { "m09", G1, "Power chords avall",          "4/4", 480, "D> D D D D> D D D", 0, 0 },
                { "m10", G1, "Foguera clàssic",             "4/4", 480, "D . D U D . D U", 0, 0 },
                { "m11", G1, "Indie botant",                "4/4", 480, "D U(t) . U(t) D U(t) . U(t)", 0, 0 },
                { "m12", G1, "Ràdio acústic",               "4/4", 480, "D . D U . u . u", 0, 0 },
                { "m13", G1, "Estrofa que creix",           "4/4", 480, "D . D . D U D U", 0, 30 },
                { "m14", G1, "Moviment 1/16",               "4/4", 240, "D U D U D . D U D U D U D . D U", 2, 0 },
                { "m15", G1, "Disco acústic",               "4/4", 480, ". U(t)! . U(t)! . U(t)! . U(t)!", 0, 0 },
                { "m16", G1, "Balada a mig temps",          "4/4", 480, "D . . . D> . U .", 0, 0 },
                { "m17", G1, "Tornada amb energia",         "4/4", 480, "D U D U D> U D U", 0, 0 },
                { "m18", G1, "Britpop",                     "4/4", 480, "D . D U D D U .", 0, 0 },
                { "m19", G2, "Funk scratch",                "4/4", 240, "D U X U D U X U", 0, 0 },
                { "m20", G2, "Estrofa rock amb palm",       "4/4", 480, "PD PD PD PD PD PD D U", 0, 0 },
                { "m21", G2, "Slap de caixa",               "4/4", 480, "D . S> . D . S> U", 0, 0 },
                { "m22", G2, "Funk 1/16 picat",             "4/4", 240, "D> X U x X D x U D> X U x X D x U", 0, 0 },
                { "m23", G2, "Pop acústic percussiu",       "4/4", 480, "D U(t) S U(t) . U(t) S U(t)", 0, 0 },
                { "m24", G2, "Clave llatina 3-3-2",         "4/4", 480, "D>! . . D>! . . D>! .", 0, 0 },
                { "m25", G2, "Reggae (skank)",              "4/4", 480, "_ D! _ D! _ D! _ D!", 2, 0 },
                { "m26", G2, "Reggaeton acústic",           "4/4", 480, "D> . . U> D . U> .", 0, 0 },
                { "m27", G2, "Arpegi funk apagat",          "4/4", 480, "X x D> x X x D> U", 0, 0 },
                { "m28", G2, "Country boom-chick",          "4/4", 480, "D(b) . D(t) . D(b) . D(t) .", 0, 0 },
                { "m29", G2, "Country baix i rasgueig",     "4/4", 480, "D(b) U D(t) U D(b) U D(t) U", 0, 0 },
                { "m30", G3, "Vals acústic",                "3/4", 480, "D(b)> . D(t) . D(t) .", 0, 0 },
                { "m31", G3, "Vals pop modern",             "3/4", 480, "D> . D u D u", 0, 0 },
                { "m32", G3, "Folk en 3/4",                 "3/4", 480, "D> U D U D U", 0, 0 },
                { "m33", G3, "Balada lenta 6/8",            "6/8", 240, "D> . D . D . D> . D . D .", 0, 0 },
                { "m34", G3, "6/8 arpegiat",                "6/8", 240, "D> U D U D U D> U D U D U", 5, 0 },
                { "m35", G3, "Blues soul 12/8",             "12/8",480, "D> . . D> . . D> . . D> . .", 0, 0 },
                { "m36", G4, "Travis picking",              "4/4", 480, "[p] [i] [p2] [m] [p] [i] [p2] [m]", 0, 0 },
                { "m37", G4, "Arpegi folk 1/16",            "4/4", 240, "[p] [i] [m] [a] [m] [i] [p] [i]", 0, 0 },
                { "m38", G4, "Pima + rasgueig (2/4)",       "2/4", 240, "[p] [a] [m] [i] D . U .", 0, 0 },
                { "m39", G4, "Clawhammer indie",            "4/4", 480, "[pc] . [pl] . [pc] . [pl] .", 0, 0 },
                { "m40", G4, "Arpegi lent 6/8",             "6/8", 480, "[p] [i] [m] [a] [m] [i]", 0, 0 },
                { "m41", G5, "Bossa nova (2/4)",            "2/4", 240, "[p] . [pl] [p] . [pl] . [pl]", 0, 0 },
                { "m42", G5, "Rumba catalana",              "4/4", 480, "D S U D S U D U", 2, 0 },
                { "m43", G5, "Rasgueado flamenc",           "4/4", 480, "[r] . D> . [r] . D> U", 0, 0 },
                { "m44", G5, "Balada pop espanyola",        "4/4", 480, "D> . D U X U D U", 0, 0 },
                { "m45", G6, "5/4 (3 + 2)",                 "5/4", 480, "D> . D U D . D> . D U", 0, 0 },
                { "m46", G6, "7/8 (2 + 2 + 3)",             "7/8", 480, "D> U D> U D> U D", 0, 0 },
                { "m47", G6, "Chug metal 1/16",             "4/4", 240, "PD PD PD PD D> PD PD PD", 0, 0 },
                { "m48", G6, "Djent sincopat",              "4/4", 480, "D> PD PD D> PD D> PD PD", 0, 0 },
                { "m49", G6, "Post-rock (swell)",           "4/4", 480, "d . . . . . . .", 30, 0 },
                { "m50", G6, "Math-rock amb tapping",       "4/4", 480, "D U [tap] U D [tap] U D", 0, 0 },
                // ---- Volum 2: 50 rasguejos pop/rock i apagats ----
                { "v51", V1, "Estrofa amb palm",            "4/4", 480, "PD PD PD PD PD PD D> U", 0, 0 },
                { "v52", V1, "Pop rock amb notes mortes",   "4/4", 480, "D . X U . U X U", 0, 0 },
                { "v53", V1, "Palm 1/16 bombejant",         "4/4", 240, "PD PU PD PU D> PU PD PU PD PU PD PU D> PU PD PU", 0, 0 },
                { "v54", V1, "Pop acústic amb slap",        "4/4", 480, "D u S u . u S u", 0, 0 },
                { "v55", V1, "Power pop dels 80",           "4/4", 480, "D . PD U D PD D U", 0, 0 },
                { "v56", V1, "Funk pop scratch",            "4/4", 240, "D x X U D x X U", 0, 0 },
                { "v57", V1, "Himne rock 3-3-2",            "4/4", 480, "D> PD PD D> PD PD D> PD", 0, 0 },
                { "v58", V1, "Indie: del palm a obert",     "4/4", 480, "PD . PD . D U X U", 0, 0 },
                { "v59", V1, "Acústic percussiu",           "4/4", 480, "D . S U PD U S U", 0, 0 },
                { "v60", V1, "Alt-rock amb notes mortes",   "4/4", 480, "D X D X D X D U", 0, 0 },
                { "v61", V1, "Pop suau apagat",             "4/4", 480, "PD PU PD PU D . . .", 0, 0 },
                { "v62", V1, "Punk: palm i explosió",       "4/4", 480, "PD PD PD PD D> D> D> D>", 0, 0 },
                { "v63", V1, "Pop apagat sincopat",         "4/4", 480, "D PD . PU D X . U", 0, 0 },
                { "v64", V1, "Dance pop picat",             "4/4", 480, "X D! X D! X D! X D!", 0, 0 },
                { "v65", V1, "Soft rock amb palm",          "4/4", 480, "PD . PD PU . PU D U", 0, 0 },
                { "v66", V1, "Funk-rock staccato",          "4/4", 240, "D! X X X D! X X X D! X X X D! U X X", 0, 0 },
                { "v67", V1, "Folk-rock amb slap",          "4/4", 480, "D U S . D U S .", 0, 0 },
                { "v68", V1, "Acústic amb palm al baix",    "4/4", 480, "PD U PD U PD U D U", 0, 0 },
                { "v69", V2, "Indie disco scratch",         "4/4", 480, "X D x D X D x D", 0, 0 },
                { "v70", V2, "Palm que creix",              "4/4", 480, "PD PD PD PD PD PD PD PD", 0, 60 },
                { "v71", V2, "Pop-punk: obre i apaga",      "4/4", 480, "D> PD PD D> PD PD D> D>", 0, 0 },
                { "v72", V2, "Pop amb slap i amunt",        "4/4", 480, "D . S . U D S .", 0, 0 },
                { "v73", V2, "Funk acústic de notes mortes","4/4", 480, "D x X x D x D U", 2, 0 },
                { "v74", V2, "Synth-pop apagat",            "4/4", 480, "PD PD . PU PD PD . PU", 0, 0 },
                { "v75", V2, "Britrock: amunts apagats",    "4/4", 480, "D PU D PU D PU D PU", 0, 0 },
                { "v76", V2, "Pop sincopat amb nota morta", "4/4", 480, "D . X U . U X .", 0, 0 },
                { "v77", V2, "Country pop modern",          "4/4", 480, "PD(b) D(t) PD(b) D(t) PD(b) D(t) D U", 0, 0 },
                { "v78", V2, "Garage rock",                 "4/4", 480, "X X D> X X X D> U", 0, 0 },
                { "v79", V2, "Ambient apagat",              "4/4", 480, "Pd . . Pu . . D .", 0, 0 },
                { "v80", V2, "Chug 1/16 amb accents",       "4/4", 240, "PD PU PD PU PD PU D> PU PD PU PD PU PD PU D> PU", 0, 0 },
                { "v81", V2, "Pop-funk staccato",           "4/4", 480, ". D! . X . D! . X", 0, 0 },
                { "v82", V2, "Redoble acústic",             "4/4", 240, "D S U S D S U S", 0, 0 },
                { "v83", V2, "Onada post-punk",             "4/4", 480, "PD PD D> PD PD PD D> PD", 0, 0 },
                { "v84", V2, "Bot acústic apagat",          "4/4", 480, "PD . U PD . U PD U", 0, 0 },
                { "v85", V2, "Rock dur amb notes mortes",   "4/4", 480, "D> X X D> X X D> D>", 0, 0 },
                { "v86", V3, "Balada rock 6/8 apagada",     "6/8", 480, "PD PD PD D> U U", 0, 0 },
                { "v87", V3, "Vals pop apagat (3/4)",       "3/4", 480, "PD(b) . D(t) X D(t) X", 0, 0 },
                { "v88", V3, "Balada 6/8 amb slap",         "6/8", 240, "D . . S . . D . U S . .", 0, 0 },
                { "v89", V3, "Estrofa lenta 12/8",          "12/8",480, "PD . . PD . . D> . . PD . .", 0, 0 },
                { "v90", V3, "Funk-pop 1/16 scratch",       "4/4", 240, "D> X X U X D X X D> X X U X D X X", 0, 0 },
                { "v91", V3, "Percussiu en 3/4",            "3/4", 480, "D U S> U D U", 0, 0 },
                { "v92", V3, "Arpegi pop apagat",           "4/4", 480, "PD PU PD PU PD PU PD U", 0, 0 },
                { "v93", V3, "Chug alt-rock",               "4/4", 480, "PD PD X D> PD PD X D>", 0, 0 },
                { "v94", V3, "Pop amb palm i slap",         "4/4", 480, "D PD S PU D PD S PU", 0, 0 },
                { "v95", V3, "Rock sincopat amb slap",      "4/4", 480, "D . S U . U S U", 0, 0 },
                { "v96", V3, "Indie amb octaves",           "4/4", 480, "PD PD D PD PD PD D PD", 0, 0 },
                { "v97", V3, "Grunge dels 90",              "4/4", 240, "D> X x D X x D> X x D X x D U . .", 0, 0 },
                { "v98", V3, "Pop staccato",                "4/4", 480, "PD! . PD! . PD! . PD! .", 0, 0 },
                { "v99", V3, "Onada percussiva",            "4/4", 480, "D U X U PD U X U", 0, 0 },
                { "v100",V3, "Híbrid pop-rock",             "4/4", 480, "D> PD S U D PD S U", 0, 0 },
                // ---- Volum 3: 50 cançons ----
                { "c01", C1, "Riptide (Vance Joy)",                     "4/4", 480, "D . D U . U D U", 2, 0 },
                { "c02", C1, "Good 4 U (Olivia Rodrigo)",               "4/4", 480, "PD PD PD PD PD PD D> U", 0, 0 },
                { "c03", C1, "Shape of You (Ed Sheeran)",               "4/4", 480, "D . S> . U D S> .", 0, 0 },
                { "c04", C1, "Wonderwall (Oasis)",                      "4/4", 240, "D . D U . U D U D U D U D U D U", 4, 0 },
                { "c05", C1, "As It Was (Harry Styles)",                "4/4", 480, "PD PU PD PU D> PU PD PU", 0, 0 },
                { "c06", C1, "Stick Season (Noah Kahan)",               "4/4", 480, "PD(b) D(t) PD(b) D(t) PD(b) D(t) D U", 0, 0 },
                { "c07", C1, "You Belong With Me (Taylor Swift)",       "4/4", 480, "D u D> u D u D> u", 0, 0 },
                { "c08", C1, "Boulevard of Broken Dreams (Green Day)",  "4/4", 480, "D> . D . D . D .", 0, 0 },
                { "c09", C1, "Flowers (Miley Cyrus)",                   "4/4", 480, "PD . D U PD . D U", 0, 0 },
                { "c10", C1, "Counting Stars (OneRepublic)",            "4/4", 480, "D . D U S U D U", 3, 0 },
                { "c11", C1, "Shallow (Gaga i Cooper)",                 "4/4", 480, "D(b) . D U D(b) . D U", 0, 0 },
                { "c12", C1, "Yellow (Coldplay)",                       "4/4", 480, "D . D U D D D U", 6, 0 },
                { "c13", C1, "Zombie (The Cranberries)",                "4/4", 480, "D U D U D> U D U", 0, 0 },
                { "c14", C1, "Riptide, pont (Vance Joy)",               "4/4", 480, "D> . D> . D> . D> .", 0, 0 },
                { "c15", C1, "Perfect (Ed Sheeran)",                    "12/8",480, "D> . . D . . D> . . D . .", 8, 0 },
                { "c16", C1, "Sweater Weather (The Neighbourhood)",     "4/4", 480, "PD PD X PD PD PD X PD", 0, 0 },
                { "c17", C1, "Havana (Camila Cabello)",                 "4/4", 480, "D! . . U! D! . U! .", 0, 0 },
                { "c18", C2, "Mr. Brightside (The Killers)",            "4/4", 240, "D U D U D U D U D U D U D U D U", 2, 0 },
                { "c19", C2, "Ho Hey (The Lumineers)",                  "4/4", 480, "D> . D> . D U D .", 0, 0 },
                { "c20", C2, "Dani California (RHCP)",                  "4/4", 480, "D . X U X U D U", 2, 0 },
                { "c21", C2, "Sex on Fire (Kings of Leon)",             "4/4", 480, "PD PD PD PD PD PD PD PD", 0, 0 },
                { "c22", C2, "Good Riddance (Green Day)",               "4/4", 480, "D(b) . D U . U D U", 0, 0 },
                { "c23", C2, "Drivers License (Olivia Rodrigo)",        "4/4", 480, "d . d . d . d .", 0, 0 },
                { "c24", C2, "Kill Bill (SZA)",                         "4/4", 480, "D . PD u . u PD u", 0, 0 },
                { "c25", C2, "Shut Up and Dance (Walk the Moon)",       "4/4", 240, "PD PU PD PU PD PU PD PU", 0, 0 },
                { "c26", C2, "Believer (Imagine Dragons)",              "12/8",480, "D . . S> . . D . . S> . .", 0, 0 },
                { "c27", C2, "Killing in the Name (RATM)",              "4/4", 480, "D> X PD D> X PD D> X", 0, 0 },
                { "c28", C2, "Superheroes (The Script)",                "4/4", 480, "D . D U D U D U", 0, 0 },
                { "c29", C2, "Love Yourself (Justin Bieber)",           "4/4", 480, "D . S . D . S .", 0, 0 },
                { "c30", C2, "Creep (Radiohead)",                       "4/4", 480, "D . D . D U D U", 0, 0 },
                { "c31", C2, "Stolen Dance (Milky Chance)",             "4/4", 480, "PD . X U PD . X U", 0, 0 },
                { "c32", C2, "Take Me to Church (Hozier)",              "6/8", 240, "D . D . D . D> . . . . .", 5, 0 },
                { "c33", C2, "Budapest (George Ezra)",                  "4/4", 480, "D U X U D U X U", 0, 0 },
                { "c34", C2, "Shivers (Ed Sheeran)",                    "4/4", 480, "D! . . U! S . U! .", 0, 0 },
                { "c35", C3, "Despacito (Luis Fonsi)",                  "4/4", 480, "D>! . . U>! D! . U>! .", 0, 0 },
                { "c36", C3, "Uptown Funk (Mark Ronson)",               "4/4", 240, ". D(t)! X U(t) . D(t)! X U(t)", 1, 0 },
                { "c37", C3, "Bailando (Enrique Iglesias)",             "4/4", 480, "D S U D S U D U", 2, 0 },
                { "c38", C3, "Hotel California (Eagles)",               "4/4", 480, "D . D U PD U D U", 5, 0 },
                { "c39", C3, "Fast Car (Tracy Chapman)",                "4/4", 480, "[pc] . [pl] . [pc] . [pl] .", 0, 0 },
                { "c40", C3, "La Camisa Negra (Juanes)",                "4/4", 480, "D X U D X U D U", 0, 0 },
                { "c41", C3, "Free Fallin' (Tom Petty)",                "4/4", 480, "D . . D D . D .", 0, 0 },
                { "c42", C3, "Cold Heart (Elton John i Dua Lipa)",      "4/4", 480, ". D! . D! . D! . D!", 0, 0 },
                { "c43", C3, "Over the Rainbow (IZ)",                   "4/4", 480, "d . d u d . d u", 10, 0 },
                { "c44", C3, "Crazy Little Thing Called Love (Queen)",  "4/4", 480, "D . D! U . U D! .", 0, 0 },
                { "c45", C3, "Let Her Go (Passenger)",                  "4/4", 480, "D(b) [i] [m] [a] D U D U", 0, 0 },
                { "c46", C3, "No Woman No Cry (Bob Marley)",            "4/4", 480, "_ D! _ U! _ D! _ U!", 0, 0 },
                { "c47", C3, "More Than Words (Extreme)",               "4/4", 480, "[pc] . S . [pl] . S .", 0, 0 },
                { "c48", C3, "Smooth (Santana)",                        "4/4", 480, "D . X U D X U D", 2, 0 },
                { "c49", C3, "I'm Yours (Jason Mraz)",                  "4/4", 480, ". D S u . D S u", 0, 0 },
                { "c50", C3, "Dust in the Wind (Kansas)",               "4/4", 240, "[p] [i] [m] [p] [a] [i] [m] [i]", 0, 0 },
            };
            std::vector<Patro> v;
            for (auto& d : defs)
            {
                Patro p;
                p.id = d.id;
                p.grup = d.grup;
                p.nom = d.nom;
                p.compas = d.compas;
                p.pasTicks = d.pas;
                p.passos = llegeixPassos (d.passos);
                p.msCorda = d.ms;
                p.crescendo = d.cresc;
                p.dibuix = dibuixa (p.passos);
                v.push_back (std::move (p));
            }
            return v;
        }();
        return llista;
    }

    namespace { std::vector<Patro>& elsPropis() { static std::vector<Patro> v; return v; } }

    const std::vector<Patro>& propis() { return elsPropis(); }
    namespace { std::map<std::string, Patro>& cauVariacions() { static std::map<std::string, Patro> m; return m; } }
    void posaPropis (std::vector<Patro> v) { elsPropis() = std::move (v); cauVariacions().clear(); }

    const Patro* patro (const std::string& id)
    {
        for (auto& p : patrons()) if (p.id == id) return &p;
        for (auto& p : elsPropis()) if (p.id == id) return &p;
        // 0.41: una variació («folk~ple») es fa a partir del seu patró
        if (const auto on = id.rfind ('~'); on != std::string::npos)
        {
            auto& cau = cauVariacions();
            if (auto it = cau.find (id); it != cau.end()) return &it->second;
            if (const auto* base = patro (id.substr (0, on)))
                for (auto& v : variacions (*base)) cau[v.id] = v;
            if (auto it = cau.find (id); it != cau.end()) return &it->second;
        }
        return nullptr;
    }

    std::string textPassos (const std::vector<Pas>& v)
    {
        std::string r;
        for (auto& s : v)
        {
            if (! r.empty()) r += ' ';
            if (s.dit != Dit::Cap)
            {
                static const char* const noms[] = { "", "p", "p2", "i", "m", "a", "pl", "pc", "tap", "r" };
                r += std::string ("[") + noms[(int) s.dit] + "]";
                if (s.accent) r += '>';
                continue;
            }
            if (s.cop == Cop::Res) { r += '.'; continue; }
            if (s.cop == Cop::Silenci) { r += '_'; continue; }
            if (s.cop == Cop::MutAvall) r += 'X';
            else if (s.cop == Cop::MutAmunt) r += 'x';
            else if (s.tec == Tecnica::Slap) r += 'S';
            else
            {
                if (s.tec == Tecnica::Palm) r += 'P';
                r += s.cop == Cop::Avall ? (s.suau ? 'd' : 'D') : (s.suau ? 'u' : 'U');
            }
            if (s.accent) r += '>';
            if (s.curt) r += '!';
            if (s.abast == Abast::Baix) r += "(b)";
            else if (s.abast == Abast::Aguts) r += "(t)";
        }
        return r;
    }

    Patro fesPropi (const std::string& nom, const std::string& compas, long long pasTicks, const std::vector<Pas>& passos)
    {
        Patro p;
        p.id = "propi:" + nom;
        p.nom = nom;
        p.grup = grupPropis;
        p.compas = compas;
        p.pasTicks = pasTicks;
        p.passos = passos;
        p.dibuix = dibuixPassos (passos);
        return p;
    }

    std::string textPropis (const std::vector<Patro>& v)
    {
        nlohmann::ordered_json j = nlohmann::ordered_json::array();
        for (auto& p : v)
            j.push_back ({ { "nom", p.nom }, { "compas", p.compas }, { "pasTicks", p.pasTicks }, { "passos", textPassos (p.passos) } });
        return nlohmann::ordered_json ({ { "rasgueigs", j } }).dump (2);
    }

    std::vector<Patro> llegeixPropis (const std::string& text)
    {
        std::vector<Patro> r;
        const auto j = nlohmann::ordered_json::parse (text, nullptr, false);
        if (! j.is_object() || ! j.contains ("rasgueigs") || ! j["rasgueigs"].is_array()) return r;
        for (auto& e : j["rasgueigs"])
        {
            if (! e.is_object()) continue;
            const std::string nom = e.value ("nom", std::string());
            const auto passos = llegeixPassos (e.value ("passos", std::string()));
            if (nom.empty() || passos.empty()) continue;
            r.push_back (fesPropi (nom, e.value ("compas", std::string ("4/4")), std::clamp (e.value ("pasTicks", 480LL), 60LL, 1920LL), passos));
        }
        return r;
    }

std::vector<Patro> variacions (const Patro& p)
    {
        std::vector<Patro> r;
        const int n = (int) p.passos.size();
        if (n < 2) return r;
        auto esCop = [] (const Pas& s) { return s.cop == Cop::Avall || s.cop == Cop::Amunt; };
        auto afegeix = [&] (const char* sufix, std::vector<Pas> v)
        {
            if (v == p.passos) return;
            for (auto& x : r) if (x.passos == v) return;
            Patro q = p;
            q.id = p.id + "~" + sufix;
            q.nom = p.nom + " · " + sufix;
            q.passos = std::move (v);
            q.dibuix = dibuixPassos (q.passos);
            r.push_back (std::move (q));
        };
        {   // més ple: els forats s'omplen amb cops fluixos (avall als passos parells, amunt als senars)
            auto v = p.passos;
            for (int i = 1; i < n; ++i)
                if (v[(size_t) i].cop == Cop::Res && v[(size_t) i].dit == Dit::Cap)
                {
                    v[(size_t) i] = Pas();
                    v[(size_t) i].cop = i % 2 == 0 ? Cop::Avall : Cop::Amunt;
                    v[(size_t) i].suau = true;
                }
            afegeix ("ple", v);
        }
        {   // més buit: fora els cops amunt sense accent (queda el que marca el ritme)
            auto v = p.passos;
            int trets = 0;
            for (int i = 1; i < n; ++i)
                if (v[(size_t) i].cop == Cop::Amunt && ! v[(size_t) i].accent && v[(size_t) i].dit == Dit::Cap && trets % 2 == 0) { v[(size_t) i] = Pas(); ++trets; }
                else if (v[(size_t) i].cop == Cop::Amunt && ! v[(size_t) i].accent) ++trets;
            afegeix ("buit", v);
        }
        {   // amb notes mortes: al segon i al quart temps (o, si el patró és curt, al mig), una nota morta
            auto v = p.passos;
            const int perTemps = (int) std::max (1LL, 960 / std::max (1LL, p.pasTicks));
            bool alguna = false;
            for (int i = perTemps; i < n; i += 2 * perTemps)
            {
                const bool accent = v[(size_t) i].accent;
                v[(size_t) i] = Pas();
                v[(size_t) i].cop = Cop::MutAvall;
                v[(size_t) i].accent = accent;
                alguna = true;
            }
            if (! alguna && esCop (v[(size_t) (n / 2)])) { v[(size_t) (n / 2)] = Pas(); v[(size_t) (n / 2)].cop = Cop::MutAvall; }
            afegeix ("mutes", v);
        }
        return r;
    }

    BlocPatro blocDe (const Patro* p, long long ini, long long dur, double variacio, unsigned llavor, int vel)
    {
        BlocPatro b;
        b.ini = ini; b.dur = dur; b.variacio = variacio; b.llavor = llavor; b.vel = vel;
        if (p != nullptr)
        {
            b.patro = p->id; b.nom = p->nom; b.passos = textPassos (p->passos); b.pasTicks = p->pasTicks;
            b.msCorda = p->msCorda; b.crescendo = p->crescendo;
        }
        return b;
    }

    Patro patroDeBloc (const BlocPatro& b)
    {
        Patro p;
        p.id = b.patro; p.nom = b.nom; p.pasTicks = b.pasTicks; p.passos = llegeixPassos (b.passos);
        p.msCorda = b.msCorda; p.crescendo = b.crescendo;
        return p;
    }

    std::vector<Tram> acordsDe (const Projecte& p, const Pista& t, const Clip& c)
    {
        std::vector<Tram> r;
        if (t.segueixAcords)
        {
            const double t0 = midi::ticks (p, c.pos);
            for (const auto& ta : p.pistes)
            {
                if (! esAcords (ta)) continue;
                for (const auto& k : ta.clips)
                {
                    if (! k.esAcord() || k.mut) continue;
                    const double a = k.iniciVisible(), b = k.fiVisible();
                    if (b - a < 0.01) continue;
                    r.push_back ({ std::llround (midi::ticks (p, a) - t0), std::llround (midi::ticks (p, b) - t0), k.acord });
                }
            }
        }
        else
            for (const auto& a : c.acords) r.push_back ({ a.ini, a.ini + a.dur, a.acord });
        std::stable_sort (r.begin(), r.end(), [] (const Tram& x, const Tram& y) { return x.ini < y.ini; });
        for (size_t i = 0; i + 1 < r.size(); ++i) r[i].fi = std::min (r[i].fi, r[i + 1].ini);   // el següent talla l'anterior
        std::erase_if (r, [] (const Tram& x) { return x.fi <= x.ini; });
        return r;
    }

    std::vector<Nota> notesBloc (const BlocPatro& b, const std::vector<Tram>& acords, double tempo)
    {
        std::vector<Nota> r;
        const long long bi = b.ini, bf = b.ini + b.dur;
        const auto p = patroDeBloc (b);
        for (const auto& a : acords)
        {
            const long long x = std::max (bi, a.ini), y = std::min (bf, a.fi);
            if (y - x < 10) continue;
            const auto cordes = notes (harmonia::llegeixAcord (a.acord));
            if (cordes.empty()) continue;
            if (p.passos.empty())
            {
                for (int to : cordes) r.push_back ({ x, y - x, to, b.vel, 0 });   // sense ritme: l'acord sencer
                continue;
            }
            Opcions o;
            o.vel = b.vel; o.tempo = tempo; o.variacio = b.variacio; o.llavor = b.llavor;
            for (auto n : rasgueja (cordes, p, b.dur, o))   // tot el bloc amb aquest acord; se'n queda el seu tros
            {
                n.ini += bi;
                if (n.ini < x || n.ini >= y) continue;
                n.dur = std::min (n.dur, y - n.ini);
                if (n.dur > 0) r.push_back (n);
            }
        }
        return r;
    }

    bool escriuBloc (Clip& c, int quin, const std::vector<Tram>& acords, double tempo)
    {
        if (quin < 0 || quin >= (int) c.blocs.size()) return false;
        const auto b = c.blocs[(size_t) quin];
        Clip nou;
        nou.midi = true;
        nou.notes = c.notes;
        std::erase_if (nou.notes, [&] (const Nota& n) { return n.ini >= b.ini && n.ini < b.ini + b.dur; });
        for (auto& n : nou.notes)
            if (n.ini < b.ini && n.ini + n.dur > b.ini) n.dur = b.ini - n.ini;   // el que venia sonant s'apaga aquí
        afegeix (nou, notesBloc (b, acords, tempo), 0);
        { std::vector<int> cap; midi::ordena (nou.notes, cap); }
        if (nou.notes == c.notes) return false;
        c.notes = std::move (nou.notes);
        return true;
    }

    namespace
    {
        /** Fa lloc a [ini, fi) dins d'una llista de trossos (blocs o acords): escurça o treu els que trepitja. */
        template <typename T>
        int faLloc (std::vector<T>& v, long long ini, long long fi)
        {
            for (size_t i = 0; i < v.size();)
            {
                auto& x = v[i];
                const long long xfi = x.ini + x.dur;
                if (x.ini < ini && xfi > ini) { x.dur = ini - x.ini; ++i; }
                else if (x.ini >= ini && x.ini < fi)
                {
                    if (xfi > fi) { x.dur = xfi - fi; x.ini = fi; ++i; }
                    else v.erase (v.begin() + (std::ptrdiff_t) i);
                }
                else ++i;
            }
            return (int) (std::find_if (v.begin(), v.end(), [&] (const T& x) { return x.ini > ini; }) - v.begin());
        }
    }

    int posaBloc (Clip& c, BlocPatro b)
    {
        if (b.dur <= 0) return -1;
        const int quin = faLloc (c.blocs, b.ini, b.ini + b.dur);
        std::erase_if (c.notes, [&] (const Nota& n) { return n.ini >= b.ini && n.ini < b.ini + b.dur; });
        c.blocs.insert (c.blocs.begin() + quin, b);
        return quin;
    }

    void treuBloc (Clip& c, int quin, bool ambNotes)
    {
        if (quin < 0 || quin >= (int) c.blocs.size()) return;
        const auto b = c.blocs[(size_t) quin];
        if (ambNotes) std::erase_if (c.notes, [&] (const Nota& n) { return n.ini >= b.ini && n.ini < b.ini + b.dur; });
        c.blocs.erase (c.blocs.begin() + quin);
    }

    int posaAcord (Clip& c, AcordClip a)
    {
        if (a.dur <= 0 || a.acord.empty()) return -1;
        const int quin = faLloc (c.acords, a.ini, a.ini + a.dur);
        c.acords.insert (c.acords.begin() + quin, a);
        return quin;
    }

    void treuAcord (Clip& c, int quin)
    {
        if (quin >= 0 && quin < (int) c.acords.size()) c.acords.erase (c.acords.begin() + quin);
    }

    bool actualitza (Projecte& p)
    {
        bool canvi = false;
        for (auto& t : p.pistes)
        {
            if (! esMidi (t)) continue;
            for (auto& c : t.clips)
            {
                if (! c.midi || c.blocs.empty()) continue;
                const auto acords = acordsDe (p, t, c);
                const double tempo = edicio::tramA (p, c.pos).tempo;
                for (int i = 0; i < (int) c.blocs.size(); ++i)
                    if (! c.blocs[(size_t) i].fix) canvi = escriuBloc (c, i, acords, tempo) || canvi;
            }
        }
        return canvi;
    }

    int marcaEditats (Clip& despres, const Clip& abans)
    {
        int quants = 0;
        for (auto& b : despres.blocs)
        {
            if (b.fix) continue;
            const bool hiEra = std::any_of (abans.blocs.begin(), abans.blocs.end(), [&] (const BlocPatro& x) { return x.ini == b.ini && x.dur == b.dur; });
            if (! hiEra) continue;   // un bloc nou o mogut no és un retoc de notes
            auto dins = [&] (const Clip& c)
            {
                std::vector<Nota> v;
                for (auto& n : c.notes) if (n.ini >= b.ini && n.ini < b.ini + b.dur) v.push_back (n);
                return v;
            };
            if (dins (despres) != dins (abans)) { b.fix = true; ++quants; }
        }
        return quants;
    }

    namespace
    {
        /** Atzar petit i sempre igual per a la mateixa llavor (splitmix64). */
        struct Atzar
        {
            std::uint64_t s;
            explicit Atzar (std::uint64_t llavor) : s (llavor) {}
            double u()   // 0..1
            {
                std::uint64_t z = (s += 0x9e3779b97f4a7c15ull);
                z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
                z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
                z ^= z >> 31;
                return (double) (z >> 11) * (1.0 / 9007199254740992.0);
            }
            double s1() { return u() * 2.0 - 1.0; }   // -1..1
        };
    }

    std::vector<Nota> rasgueja (const std::vector<int>& cordes, const Patro& p, long long durada, const Opcions& o)
    {
        std::vector<Nota> r;
        const int n = (int) cordes.size();
        if (n == 0 || durada <= 0 || p.passos.empty()) return r;

        const double v = std::clamp (o.variacio, 0.0, 1.0);
        const double ticksPerMs = std::max (20.0, o.tempo) / 60.0 * (double) ticksPerNegra / 1000.0;
        const long long pas = p.pasTicks > 0 ? p.pasTicks : durada;
        const long long llargPatro = (long long) p.passos.size();
        const double ms = p.msCorda > 0.0 ? p.msCorda : o.msPerCorda;
        // El rasgueig sencer no ha d'ocupar més de mig pas (a tempos molt ràpids s'estreny).
        const long long deltaMax = std::max (1LL, pas / (2 * (long long) std::max (1, n)));
        const long long deltaBase = std::max (1LL, std::min ((long long) std::llround (ms * ticksPerMs), deltaMax));
        const long long durMut = std::max (10LL, std::min ((long long) std::llround (45.0 * ticksPerMs), pas / 2));
        const long long durSlap = std::max (8LL, std::min ((long long) std::llround (14.0 * ticksPerMs), pas / 3));
        const long long durPalm = std::max (10LL, pas * 35 / 100);
        const long long durCurt = std::max (10LL, pas * 33 / 100);

        std::vector<int> sonant ((size_t) n, -1);   // per corda: la nota que encara sona (índex a r)
        auto apaga = [&] (int s, long long t)
        {
            const int i = sonant[(size_t) s];
            if (i >= 0) r[(size_t) i].dur = std::max (1LL, t - r[(size_t) i].ini);
            sonant[(size_t) s] = -1;
        };
        auto apagaTot = [&] (long long t) { for (int s = 0; s < n; ++s) apaga (s, t); };
        auto petita = [] (long long k, int s) { return (int) (((k * 7 + s * 13 + 3) * 2654435761u >> 16) % 7) - 3; };   // -3..+3, sempre igual
        // una nota: «llarg» = 0 vol dir que sona fins que es torna a tocar
        auto posa = [&] (int s, int to, long long ini, int vel, long long llarg)
        {
            if (ini >= durada || ini < 0) return;
            apaga (s, ini);
            Nota nota;
            nota.ini = ini;
            nota.dur = std::max (1LL, std::min (llarg > 0 ? llarg : durada - ini, durada - ini));
            nota.to = std::clamp (to, 0, 127);
            nota.vel = std::clamp (vel, 1, 127);
            r.push_back (nota);
            if (llarg <= 0) sonant[(size_t) s] = (int) r.size() - 1;
        };

        for (long long k = 0; k * pas < durada; ++k)
        {
            if (p.pasTicks <= 0 && k > 0) break;
            const long long idx = k % llargPatro;
            Pas ps = p.passos[(size_t) idx];
            long long t = k * pas;
            int velExtra = p.crescendo != 0 && llargPatro > 1 ? (int) std::llround (p.crescendo * ((double) idx / (double) (llargPatro - 1) - 0.5)) : 0;
            double escalaDelta = 1.0;
            int saltaGreus = 0, saltaAgudes = 0;

            if (v > 0.0)
            {
                // 0.32: variació. Cada pas té el seu atzar (llavor + número de pas): cada repetició canvia.
                Atzar a ((std::uint64_t) o.llavor * 0x100000001b3ull + (std::uint64_t) k * 0x9e37ull + 17);
                const bool primer = idx == 0;
                const bool rasgueig = ps.dit == Dit::Cap && ps.tec != Tecnica::Slap;
                if (ps.cop == Cop::Res && ! primer && pas >= 240 && rasgueig)
                {
                    const double x = a.u();
                    if (x < 0.10 * v) { ps.cop = Cop::Amunt; ps.suau = true; ps.abast = Abast::Aguts; }      // cop fantasma
                    else if (x < 0.15 * v) ps.cop = Cop::MutAmunt;                                            // rascada
                }
                else if (ps.cop == Cop::Amunt && ! ps.accent && ! primer && ps.dit == Dit::Cap && a.u() < 0.07 * v)
                    ps.cop = Cop::Res;                                                                        // amunt que no hi és
                if (! primer) t = std::max (0LL, t + (long long) std::llround (a.s1() * 12.0 * v));
                velExtra += (int) std::llround (a.s1() * ((ps.cop == Cop::MutAvall || ps.cop == Cop::MutAmunt) ? 14.0 : 8.0) * v);
                escalaDelta = 1.0 + a.s1() * 0.35 * v;
                if (ps.cop == Cop::Avall && rasgueig && ps.abast == Abast::Totes && n > 4 && a.u() < 0.22 * v) saltaGreus = 1;
                if (ps.cop == Cop::Amunt && n > 4 && a.u() < 0.25 * v) saltaAgudes = -1;   // de vegades n'agafa 5
            }
            if (ps.cop == Cop::Res) continue;
            if (ps.cop == Cop::Silenci) { apagaTot (t); continue; }
            const long long delta = std::max (1LL, std::min ((long long) std::llround ((double) deltaBase * escalaDelta), deltaMax));
            const int acc = ps.accent ? 22 : 0;
            const int fluix = ps.suau ? -18 : 0;

            // ---- dits ----
            if (ps.dit != Dit::Cap)
            {
                const int vp = o.vel + 5 + acc + fluix + velExtra, vd = o.vel - 12 + acc + fluix + velExtra;
                auto aguda = [&] (int desDalt) { return std::max (0, n - 1 - desDalt); };
                switch (ps.dit)
                {
                    case Dit::P:  posa (0, cordes[0], t, vp + petita (k, 0), 0); break;
                    case Dit::P2: { const int s = std::min (1, n - 1); posa (s, cordes[(size_t) s], t, vp - 4 + petita (k, s), 0); } break;
                    case Dit::I:  { const int s = aguda (2); posa (s, cordes[(size_t) s], t, vd + petita (k, s), 0); } break;
                    case Dit::M:  { const int s = aguda (1); posa (s, cordes[(size_t) s], t, vd + petita (k, s), 0); } break;
                    case Dit::A:  { const int s = aguda (0); posa (s, cordes[(size_t) s], t, vd + 2 + petita (k, s), 0); } break;
                    case Dit::PolzeAcord: posa (0, cordes[0], t, vp + petita (k, 0), 0); [[fallthrough]];
                    case Dit::Pluck:
                        for (int j = 0; j < 3 && j < n - 1; ++j) { const int s = aguda (j); posa (s, cordes[(size_t) s], t, vd + petita (k, s), 0); }
                        if (n == 1 && ps.dit == Dit::Pluck) posa (0, cordes[0], t, vd, 0);
                        break;
                    case Dit::Tap: { const int s = aguda (0); posa (s, cordes[(size_t) s] + 12, t, o.vel + 15 + acc + velExtra, pas / 2); } break;
                    case Dit::Rasguejat:
                    {
                        // quatre dits (e, a, m, i) avall molt seguits
                        const long long entre = std::max (4LL, std::min ((long long) std::llround (28.0 * ticksPerMs), pas / 5));
                        const long long d2 = std::max (1LL, std::min ((long long) std::llround (3.0 * ticksPerMs), entre / (long long) std::max (1, n)));
                        for (int f = 0; f < 4; ++f)
                            for (int s = 0; s < n; ++s)
                                posa (s, cordes[(size_t) s], t + f * entre + s * d2, o.vel - 6 + 4 * f + acc + velExtra - (s == 0 ? 0 : 2) + petita (k + f, s), 0);
                    } break;
                    case Dit::Cap: break;
                }
                continue;
            }

            const bool mut = ps.cop == Cop::MutAvall || ps.cop == Cop::MutAmunt;
            const bool slap = ps.tec == Tecnica::Slap;
            const bool palm = ps.tec == Tecnica::Palm;
            if (mut || slap || palm) apagaTot (t);
            const bool avall = ps.cop == Cop::Avall || ps.cop == Cop::MutAvall;

            // quines cordes (índexs de greu a aguda)
            int de = 0, fins = n;                        // [de, fins)
            if (slap) { de = 0; fins = n; }
            else if (ps.abast == Abast::Baix) { de = 0; fins = 1; }
            else if (ps.abast == Abast::Aguts) { de = std::max (0, n - 4); fins = n; }
            else if (palm && avall) { de = 0; fins = std::min (n, 4); }      // el palm mute agafa les greus
            else if (! avall) { de = std::max (0, n - 4 + saltaAgudes); fins = n; }   // amunt: la pua només arriba a les agudes
            de = std::min (de + saltaGreus, fins - 1);

            long long llarg = 0;
            if (mut) llarg = durMut;
            else if (slap) llarg = durSlap;
            else if (palm) llarg = durPalm;
            else if (ps.curt) llarg = durCurt;
            if (ps.curt && llarg > durCurt) llarg = durCurt;

            const int quantes = fins - de;
            for (int j = 0; j < quantes; ++j)
            {
                const int s = avall ? de + j : fins - 1 - j;
                const long long ini = t + (slap ? j * std::max (1LL, delta / 3) : j * delta);
                int vel;
                if (mut) vel = 44 - 2 * j + (ps.accent ? 8 : 0);
                else if (slap) vel = o.vel + 14 + acc - j;
                else
                {
                    vel = o.vel + acc + fluix + (palm ? -14 : 0);
                    vel += avall ? (j == 0 ? 10 : -2 * j) : (j == 0 ? -2 : -8 - 3 * j);
                }
                vel += velExtra + petita (k, s);
                posa (s, cordes[(size_t) s], ini, vel, llarg);
            }
            if (ps.curt && ! mut && ! slap && ! palm) { /* ja duren poc: les cordes no queden sonant */ }
        }
        std::stable_sort (r.begin(), r.end(), [] (const Nota& a, const Nota& b) { return a.ini < b.ini; });
        return r;
    }

    std::vector<int> afegeix (Clip& c, const std::vector<Nota>& notes, long long ini)
    {
        std::vector<Nota> noves;
        for (auto n : notes)
        {
            n.ini += ini;
            noves.push_back (n);
            c.notes.push_back (n);
        }
        std::vector<int> cap;
        midi::ordena (c.notes, cap);
        std::vector<int> r;
        for (int i = 0; i < (int) c.notes.size(); ++i)
            for (size_t j = 0; j < noves.size(); ++j)
                if (noves[j].to >= 0 && c.notes[(size_t) i] == noves[j]) { r.push_back (i); noves[j].to = -1; break; }
        return r;
    }
}
