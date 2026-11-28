#include "mibase/MixAssistant.h"
#include "mibase/FFT.h"
#include "mibase/GeneralMidi.h"
#include "mibase/Projecte.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <numeric>

namespace mibase::mix
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr int NB = diagnosi::numBandes;

        float db (double e) { return (float) (10.0 * std::log10 (std::max (e, 1e-20))); }

        /** «3,5» (coma decimal, com a la resta de Mibase). */
        std::string num (double v, int decimals = 1)
        {
            char b[32];
            std::snprintf (b, sizeof b, "%.*f", decimals, v);
            std::string s (b);
            for (auto& c : s) if (c == '.') c = ',';
            if (s == "-0" || s == "-0,0") s = s.substr (1);
            return s;
        }
        std::string hz (double f)
        {
            if (f >= 1000.0) return num (f / 1000.0, f >= 10000.0 ? 0 : 1) + " kHz";
            return std::to_string ((int) std::lround (f)) + " Hz";
        }
        std::string dbs (double v) { return (v > 0 ? "+" : v < 0 ? "−" : "") + num (std::abs (v)) + " dB"; }

        double percentil (std::vector<float> v, double p)
        {
            if (v.empty()) return 0.0;
            std::sort (v.begin(), v.end());
            const double x = p * (double) (v.size() - 1);
            const auto i = (size_t) x;
            const double f = x - (double) i;
            return i + 1 < v.size() ? v[i] * (1.0 - f) + v[i + 1] * f : v[i];
        }

        // ---- zones (per índex de banda de terç d'octava: 31,5 Hz … 16 kHz) ----
        enum Zona { Sub, Greus, Fang, Mitjos, Presencia, Brillantor, Aire, numZones };
        constexpr int zonaIni[numZones] = { 0, 3, 7, 13, 18, 23, 26 };
        constexpr int zonaFi[numZones]  = { 2, 6, 12, 17, 22, 25, 27 };
        const char* nomZona (int z)
        {
            static const char* n[] = { "subgreus", "greus", "mitjos baixos", "mitjos", "presència", "brillantor", "aire" };
            return n[z];
        }

        /** Mitjana d'energia per banda d'una zona (dB). */
        float nivellZona (const diagnosi::Analisi& a, int z)
        {
            double e = 0.0;
            for (int b = zonaIni[z]; b <= zonaFi[z]; ++b) e += a.mid[(size_t) b] + a.side[(size_t) b];
            return db (e / (zonaFi[z] - zonaIni[z] + 1));
        }
        /** La banda més forta d'una zona. */
        int bandaMaxima (const diagnosi::Analisi& a, int z)
        {
            int m = zonaIni[z];
            for (int b = zonaIni[z]; b <= zonaFi[z]; ++b)
                if (a.mid[(size_t) b] + a.side[(size_t) b] > a.mid[(size_t) m] + a.side[(size_t) m]) m = b;
            return m;
        }

        std::string senseAccents (const std::string& s)
        {
            static const std::pair<const char*, char> taula[] = {
                { "à", 'a' }, { "á", 'a' }, { "è", 'e' }, { "é", 'e' }, { "í", 'i' }, { "ï", 'i' }, { "ò", 'o' }, { "ó", 'o' },
                { "ú", 'u' }, { "ü", 'u' }, { "ç", 'c' }, { "ñ", 'n' }, { "À", 'a' }, { "È", 'e' }, { "É", 'e' }, { "Í", 'i' },
                { "Ò", 'o' }, { "Ó", 'o' }, { "Ú", 'u' }, { "Ç", 'c' }, { "Ñ", 'n' } };
            std::string r;
            for (size_t i = 0; i < s.size();)
            {
                bool fet = false;
                for (const auto& [u, c] : taula)
                {
                    const size_t m = std::char_traits<char>::length (u);
                    if (s.compare (i, m, u) == 0) { r += c; i += m; fet = true; break; }
                }
                if (fet) continue;
                const char c = s[i++];
                r += (c >= 'A' && c <= 'Z') ? (char) (c + 32) : c;
            }
            return r;
        }

        /** Paraules del nom: lletres i xifres separades («Gtr2 L» → gtr, 2, l). */
        std::vector<std::string> paraules (const std::string& nom)
        {
            const auto s = senseAccents (nom);
            std::vector<std::string> v;
            std::string ara;
            int tipus = 0;   // 1 lletra, 2 xifra
            for (char c : s)
            {
                const int t = (c >= 'a' && c <= 'z') ? 1 : (c >= '0' && c <= '9') ? 2 : 0;
                if (t != tipus && ! ara.empty()) { v.push_back (ara); ara.clear(); }
                tipus = t;
                if (t != 0) ara += c;
            }
            if (! ara.empty()) v.push_back (ara);
            return v;
        }

        struct Clau { const char* text; Instrument inst; bool prefix; };
        // l'ordre compta: les més concretes primer
        const Clau claus[] = {
            { "veu", Instrument::Veu, false }, { "vox", Instrument::Veu, false }, { "vocal", Instrument::Veu, true },
            { "voc", Instrument::Veu, false }, { "voz", Instrument::Veu, false }, { "cantant", Instrument::Veu, false },
            { "solista", Instrument::Veu, false }, { "lv", Instrument::Veu, false }, { "leadvox", Instrument::Veu, false },
            { "cors", Instrument::Cors, false }, { "coro", Instrument::Cors, true }, { "backing", Instrument::Cors, false },
            { "bv", Instrument::Cors, false }, { "bvs", Instrument::Cors, false }, { "harmony", Instrument::Cors, false },
            { "choir", Instrument::Cors, false }, { "veus", Instrument::Cors, false }, { "segones", Instrument::Cors, false },
            { "bombo", Instrument::Bombo, false }, { "kick", Instrument::Bombo, true }, { "bd", Instrument::Bombo, false },
            { "kik", Instrument::Bombo, false },
            { "caixa", Instrument::Caixa, false }, { "snare", Instrument::Caixa, true }, { "sn", Instrument::Caixa, false },
            { "sd", Instrument::Caixa, false }, { "redoblant", Instrument::Caixa, false }, { "tarola", Instrument::Caixa, false },
            { "tom", Instrument::Toms, false }, { "toms", Instrument::Toms, false }, { "timbal", Instrument::Toms, true },
            { "floortom", Instrument::Toms, false },
            { "plats", Instrument::Plats, false }, { "plat", Instrument::Plats, false }, { "overhead", Instrument::Plats, true },
            { "oh", Instrument::Plats, false }, { "hihat", Instrument::Plats, false }, { "hh", Instrument::Plats, false },
            { "hat", Instrument::Plats, false }, { "charles", Instrument::Plats, false }, { "cymbal", Instrument::Plats, true },
            { "ride", Instrument::Plats, false }, { "crash", Instrument::Plats, false }, { "platillo", Instrument::Plats, true },
            { "bateria", Instrument::Bateria, false }, { "drum", Instrument::Bateria, true }, { "kit", Instrument::Bateria, false },
            { "room", Instrument::Bateria, false }, { "bat", Instrument::Bateria, false },
            { "perc", Instrument::Percussio, true }, { "shaker", Instrument::Percussio, false }, { "pandereta", Instrument::Percussio, false },
            { "conga", Instrument::Percussio, true }, { "bongo", Instrument::Percussio, true }, { "cajon", Instrument::Percussio, false },
            { "clap", Instrument::Percussio, true }, { "tambori", Instrument::Percussio, false },
            { "baix", Instrument::Baix, false }, { "bass", Instrument::Baix, false }, { "bajo", Instrument::Baix, false },
            { "contrabaix", Instrument::Baix, false }, { "sub", Instrument::Baix, false }, { "subbass", Instrument::Baix, false },
            { "acustica", Instrument::GuitarraAcustica, false }, { "acustic", Instrument::GuitarraAcustica, false },
            { "acoustic", Instrument::GuitarraAcustica, false }, { "acus", Instrument::GuitarraAcustica, false },
            { "nylon", Instrument::GuitarraAcustica, false }, { "espanyola", Instrument::GuitarraAcustica, false },
            { "classica", Instrument::GuitarraAcustica, false }, { "ag", Instrument::GuitarraAcustica, false },
            { "electrica", Instrument::GuitarraElectrica, false }, { "electric", Instrument::GuitarraElectrica, false },
            { "elec", Instrument::GuitarraElectrica, false }, { "eg", Instrument::GuitarraElectrica, false },
            { "dist", Instrument::GuitarraElectrica, true }, { "riff", Instrument::GuitarraElectrica, true },
            { "guitarr", Instrument::GuitarraElectrica, true }, { "guitar", Instrument::GuitarraElectrica, true },
            { "gtr", Instrument::GuitarraElectrica, false }, { "guit", Instrument::GuitarraElectrica, false },
            { "gt", Instrument::GuitarraElectrica, false }, { "ritmica", Instrument::GuitarraElectrica, false },
            { "piano", Instrument::Piano, false }, { "pno", Instrument::Piano, false }, { "pf", Instrument::Piano, false },
            { "teclat", Instrument::Teclats, true }, { "keys", Instrument::Teclats, false }, { "key", Instrument::Teclats, false },
            { "rhodes", Instrument::Teclats, false }, { "wurli", Instrument::Teclats, true }, { "orgue", Instrument::Teclats, false },
            { "organ", Instrument::Teclats, true }, { "hammond", Instrument::Teclats, false }, { "clavi", Instrument::Teclats, true },
            { "epiano", Instrument::Teclats, false }, { "ep", Instrument::Teclats, false },
            { "pad", Instrument::Pad, false }, { "pads", Instrument::Pad, false }, { "atmos", Instrument::Pad, true },
            { "sinte", Instrument::Sinte, true }, { "synth", Instrument::Sinte, true }, { "syn", Instrument::Sinte, false },
            { "arp", Instrument::Sinte, true }, { "seq", Instrument::Sinte, false }, { "lead", Instrument::Sinte, false },
            { "corda", Instrument::Corda, false }, { "cordes", Instrument::Corda, false }, { "string", Instrument::Corda, true },
            { "str", Instrument::Corda, false }, { "violi", Instrument::Corda, true }, { "viola", Instrument::Corda, false },
            { "cello", Instrument::Corda, false }, { "violoncel", Instrument::Corda, false },
            { "vent", Instrument::Vents, true }, { "metall", Instrument::Vents, true }, { "brass", Instrument::Vents, false },
            { "trompeta", Instrument::Vents, false }, { "trumpet", Instrument::Vents, false }, { "sax", Instrument::Vents, true },
            { "trombo", Instrument::Vents, true }, { "flauta", Instrument::Vents, false }, { "flute", Instrument::Vents, false },
            { "horn", Instrument::Vents, true },
        };

        /** El que vol cada instrument (orientatiu; xifres relatives a la seva zona de referència). nan = no es mira. */
        struct Perfil
        {
            int ref = Mitjos;             // zona de referència
            float tallHz = 0.0f;          // passa-alts proposat (0 = cap)
            float maxSub = NAN;           // subgreus respecte de la referència
            float maxGreus = NAN, maxFang = NAN, minMitjos = NAN, minPres = NAN, maxPres = NAN, maxBrill = NAN;
            float maxRang = NAN;          // rang dinàmic (dB) a partir del qual es proposa compressió
            float ratio = 3.0f, atac = 10.0f, alliberament = 120.0f;
            bool sibilancia = false, monoGreus = false, ressonanciesMusicals = false, cops = false;
            float maxGreusPlats = NAN;    // plats: greus respecte de la brillantor
        };

        Perfil perfil (Instrument i)
        {
            Perfil p;
            switch (i)
            {
                case Instrument::Veu:
                    p.tallHz = 80; p.maxSub = -22; p.maxGreus = 6; p.maxFang = 8; p.minPres = -14; p.maxPres = 3; p.maxBrill = -2;
                    p.maxRang = 12; p.ratio = 3; p.atac = 10; p.alliberament = 120; p.sibilancia = true; p.ressonanciesMusicals = true; break;
                case Instrument::Cors:
                    p.tallHz = 120; p.maxSub = -20; p.maxGreus = 0; p.maxFang = 6; p.minPres = -16; p.maxPres = 3; p.maxBrill = -2;
                    p.maxRang = 12; p.ratio = 3; p.atac = 15; p.alliberament = 150; p.sibilancia = true; p.ressonanciesMusicals = true; break;
                case Instrument::GuitarraElectrica:
                    p.tallHz = 80; p.maxSub = -20; p.maxGreus = 6; p.maxFang = 9; p.minPres = -14; p.maxPres = 4; p.maxBrill = -4;
                    p.maxRang = 15; p.ratio = 3; p.atac = 20; p.alliberament = 150; p.ressonanciesMusicals = true; break;
                case Instrument::GuitarraAcustica:
                    p.tallHz = 80; p.maxSub = -20; p.maxGreus = 4; p.maxFang = 8; p.minPres = -16; p.maxPres = 4; p.maxBrill = 0;
                    p.maxRang = 14; p.ratio = 3; p.atac = 15; p.alliberament = 120; p.ressonanciesMusicals = true; break;
                case Instrument::Baix:
                    p.ref = Greus; p.maxFang = 2; p.minMitjos = -26; p.maxRang = 10; p.ratio = 4; p.atac = 20; p.alliberament = 150;
                    p.monoGreus = true; p.ressonanciesMusicals = true; break;
                case Instrument::Bombo:
                    p.ref = Greus; p.maxFang = -2; p.minPres = -34; p.monoGreus = true; p.ressonanciesMusicals = true; p.cops = true; break;
                case Instrument::Caixa:
                    p.tallHz = 80; p.maxSub = -16; p.maxFang = 10; p.minPres = -16; p.maxPres = 6; p.cops = true; break;
                case Instrument::Toms:
                    p.tallHz = 50; p.maxSub = -10; p.maxFang = 12; p.ressonanciesMusicals = true; p.cops = true; break;
                case Instrument::Plats:
                    p.ref = Brillantor; p.tallHz = 200; p.maxGreusPlats = -14; break;
                case Instrument::Piano:
                    p.maxFang = 8; p.minPres = -20; p.ressonanciesMusicals = true; break;
                case Instrument::Teclats:
                    p.tallHz = 40; p.maxSub = -14; p.maxFang = 8; p.ressonanciesMusicals = true; break;
                case Instrument::Pad:
                    p.tallHz = 60; p.maxSub = -14; p.maxFang = 8; p.ressonanciesMusicals = true; break;
                case Instrument::Sinte:
                    p.tallHz = 40; p.maxSub = -12; p.maxFang = 9; p.ressonanciesMusicals = true; break;
                case Instrument::Corda:
                    p.tallHz = 50; p.maxSub = -16; p.maxFang = 8; p.maxBrill = 0; p.ressonanciesMusicals = true; break;
                case Instrument::Vents:
                    p.tallHz = 80; p.maxSub = -20; p.maxFang = 8; p.maxBrill = 0; p.ressonanciesMusicals = true; break;
                case Instrument::Bateria: case Instrument::Percussio:
                    p.cops = true; p.ressonanciesMusicals = true; break;
                default: p.ressonanciesMusicals = true; break;
            }
            return p;
        }

        bool esTonal (Instrument i)
        {
            switch (i)
            {
                case Instrument::Bombo: case Instrument::Caixa: case Instrument::Toms: case Instrument::Plats:
                case Instrument::Bateria: case Instrument::Percussio: return false;
                default: return true;
            }
        }
    }

    // ---------------- Instruments ----------------
    const char* nomInstrument (Instrument i)
    {
        static const char* n[] = { "Sense identificar", "Veu principal", "Cors", "Bombo", "Caixa", "Toms", "Plats i charles",
                                   "Bateria (kit o ambient)", "Percussió", "Baix", "Guitarra elèctrica", "Guitarra acústica",
                                   "Piano", "Teclats i orgue", "Pad", "Sintetitzador", "Cordes", "Vents", "Altres" };
        const int k = (int) i;
        return k >= 0 && k < numInstruments ? n[k] : n[0];
    }
    const char* clauInstrument (Instrument i)
    {
        static const char* n[] = { "", "veu", "cors", "bombo", "caixa", "toms", "plats", "bateria", "percussio", "baix",
                                   "guitarra-electrica", "guitarra-acustica", "piano", "teclats", "pad", "sinte", "cordes", "vents", "altres" };
        const int k = (int) i;
        return k >= 0 && k < numInstruments ? n[k] : n[0];
    }
    Instrument instrumentDeClau (const std::string& clau)
    {
        if (clau.empty()) return Instrument::Desconegut;
        for (int k = 1; k < numInstruments; ++k) if (clau == clauInstrument ((Instrument) k)) return (Instrument) k;
        return Instrument::Desconegut;
    }

    Identificacio identificaPerNom (const std::string& nom)
    {
        Identificacio r;
        std::vector<Instrument> trobats;
        bool guitarra = false;
        for (const auto& w : paraules (nom))
            for (const auto& c : claus)
            {
                const std::string t (c.text);
                const bool ok = c.prefix ? w.rfind (t, 0) == 0 : w == t;
                if (! ok) continue;
                if (t == "guitarr" || t == "guitar" || t == "gtr" || t == "guit" || t == "gt") guitarra = true;
                trobats.push_back (c.inst);
                break;
            }
        if (trobats.empty()) { r.perque = "el nom no diu quin instrument és"; return r; }
        // «Guitarra acústica»: l'acústica mana sobre la genèrica
        if (std::find (trobats.begin(), trobats.end(), Instrument::GuitarraAcustica) != trobats.end())
        {
            r.instrument = Instrument::GuitarraAcustica;
            r.confianca = Confianca::Alta;
            r.perque = "pel nom de la pista";
            return r;
        }
        // «Lead vocal», «Lead gtr»: el «lead» no mana
        std::vector<Instrument> clars;
        for (auto i : trobats) if (i != Instrument::Sinte || trobats.size() == 1) clars.push_back (i);
        if (clars.empty()) clars = trobats;
        const bool totsIguals = std::all_of (clars.begin(), clars.end(), [&] (Instrument i) { return i == clars.front(); });
        r.instrument = clars.front();
        r.perque = "pel nom de la pista";
        // «Guitarra» tota sola: podria ser acústica
        const auto ws = paraules (nom);
        const bool nomesGuitarraGenerica = guitarra && r.instrument == Instrument::GuitarraElectrica
            && std::none_of (ws.begin(), ws.end(), [] (const std::string& w)
               { return w == "electrica" || w == "electric" || w == "elec" || w == "eg" || w.rfind ("dist", 0) == 0; });
        r.confianca = totsIguals && ! nomesGuitarraGenerica ? Confianca::Alta : Confianca::Mitjana;
        if (nomesGuitarraGenerica) r.perque = "pel nom (diu guitarra, però no si és elèctrica o acústica)";
        return r;
    }

    namespace
    {
        Identificacio perSo (const AnalisiPista& a)
        {
            Identificacio r;
            r.confianca = Confianca::Baixa;
            r.perque = "pel so (només és una suposició)";
            if (! a.valid) { r.perque = "no hi ha prou so per saber-ho"; return r; }
            const bool cops = a.crestaDb > 10.0f && a.atacsPerSegon > 0.8f;
            if (a.greusPart > 0.6f && cops) r.instrument = Instrument::Bombo;
            else if (a.greusPart > 0.5f) r.instrument = Instrument::Baix;
            else if (a.agutsPart > 0.45f) r.instrument = Instrument::Plats;
            else if (cops && a.greusPart < 0.25f && a.centroideHz > 800.0f && a.centroideHz < 5000.0f) r.instrument = Instrument::Caixa;
            else r.perque = "ni el nom ni el so no ho diuen prou clar";
            return r;
        }

        Identificacio perTipus (const Pista& t)
        {
            Identificacio r;
            r.perque = "per l'instrument que la fa sonar";
            const auto& in = t.instrument;
            if (esKit (t)) { r.instrument = Instrument::Bateria; r.confianca = Confianca::Alta; r.perque = "és un kit de bateria de Mibase"; return r; }
            if (! esMidi (t)) return {};
            if (in == "sinte:subbass") { r.instrument = Instrument::Baix; r.confianca = Confianca::Alta; return r; }
            if (in == "sinte:lead") { r.instrument = Instrument::Sinte; r.confianca = Confianca::Alta; return r; }
            if (in == "sinte:atmos") { r.instrument = Instrument::Pad; r.confianca = Confianca::Alta; return r; }
            if (in == "sinte:vintage" || in == "sinte:neo") { r.instrument = Instrument::Sinte; r.confianca = Confianca::Mitjana; return r; }
            if (in == "intern:piano") { r.instrument = Instrument::Piano; r.confianca = Confianca::Alta; return r; }
            if (gm::RefSf2 ref; gm::llegeixRef (in, ref))
            {
                const auto s = senseAccents (ref.fitxer + " " + ref.nom);
                const bool acust = s.find ("acoust") != std::string::npos || s.find ("acust") != std::string::npos
                                || s.find ("seagull") != std::string::npos || s.find ("ibanez") != std::string::npos
                                || s.find ("nylon") != std::string::npos;
                r.instrument = acust ? Instrument::GuitarraAcustica : Instrument::GuitarraElectrica;
                r.confianca = Confianca::Alta;
                return r;
            }
            if (const int p = gm::programa (in); p >= 0)
            {
                r.confianca = Confianca::Alta;
                const int f = p / 8;
                if (p <= 7) r.instrument = Instrument::Piano;
                else if (f == 1 || f == 2) r.instrument = Instrument::Teclats;
                else if (p == 24 || p == 25) r.instrument = Instrument::GuitarraAcustica;
                else if (f == 3) r.instrument = Instrument::GuitarraElectrica;
                else if (f == 4) r.instrument = Instrument::Baix;
                else if (p == 52 || p == 53 || p == 54) r.instrument = Instrument::Cors;
                else if (f == 5 || f == 6) r.instrument = Instrument::Corda;
                else if (f == 7 || f == 8 || f == 9) r.instrument = Instrument::Vents;
                else if (f == 10) r.instrument = Instrument::Sinte;
                else if (f == 11) r.instrument = Instrument::Pad;
                else if (f == 14) r.instrument = Instrument::Percussio;
                else { r.instrument = Instrument::Altres; r.confianca = Confianca::Mitjana; }
                return r;
            }
            return {};
        }
    }

    Identificacio identifica (const Pista& t, const AnalisiPista* a)
    {
        if (const auto i = instrumentDeClau (t.mixInstrument); i != Instrument::Desconegut)
        {
            Identificacio r;
            r.instrument = i;
            r.confianca = Confianca::Alta;
            r.triatPerTu = true;
            r.perque = "ho has triat tu";
            return r;
        }
        const auto tipus = perTipus (t);
        auto nom = identificaPerNom (t.nom);
        if (nom.instrument == Instrument::Desconegut && ! t.nomInstrument.empty())
        {
            nom = identificaPerNom (t.nomInstrument);
            if (nom.instrument != Instrument::Desconegut) nom.perque = "pel nom de l'instrument";
        }
        if (tipus.confianca == Confianca::Alta && tipus.instrument != Instrument::Desconegut)
        {
            // el kit és tota la bateria, però si el nom diu «Bombo», mana el nom
            if (tipus.instrument == Instrument::Bateria && nom.confianca == Confianca::Alta) return nom;
            return tipus;
        }
        if (nom.confianca == Confianca::Alta) return nom;
        if (tipus.instrument != Instrument::Desconegut) return tipus;
        if (nom.instrument != Instrument::Desconegut) return nom;
        if (a != nullptr) return perSo (*a);
        return nom;
    }

    // ---------------- Anàlisi ----------------
    AnalisiPista analitzaPista (const float* L, const float* R, std::size_t n, double fs, double inici)
    {
        AnalisiPista a;
        a.inici = inici;
        constexpr int N = 4096, hop = N / 2;
        a.fs = fs;
        a.midaFFT = N;
        if (L == nullptr || R == nullptr || n < (std::size_t) N || fs < 8000.0) return a;
        a.segons = (double) n / fs;
        a.salt = (double) hop / fs;
        a.global = diagnosi::analitza (L, R, n, fs);
        a.truePeakDb = a.global.loud.truePeakMax;

        // 1) nivell i correlació de cada finestra (al domini del temps)
        const size_t F = (n - (size_t) N) / (size_t) hop + 1;
        a.nivellDb.resize (F);
        a.correlacio.resize (F);
        double pic = 0.0, eLL = 0, eRR = 0, eSum = 0;
        for (size_t f = 0; f < F; ++f)
        {
            double ll = 0, rr = 0, lr = 0;
            const size_t o = f * (size_t) hop;
            for (int i = 0; i < N; ++i)
            {
                const double l = L[o + (size_t) i], r = R[o + (size_t) i];
                ll += l * l; rr += r * r; lr += l * r;
            }
            a.nivellDb[f] = db (0.5 * (ll + rr) / N);
            a.correlacio[f] = (ll > 1e-12 && rr > 1e-12) ? (float) (lr / std::sqrt (ll * rr)) : 1.0f;
        }
        for (size_t i = 0; i < n; ++i)
        {
            const double l = L[i], r = R[i];
            pic = std::max ({ pic, std::abs (l), std::abs (r) });
            eLL += l * l; eRR += r * r; eSum += (l + r) * (l + r);
        }
        a.picDb = db (pic * pic);
        a.perduaMonoDb = (eLL + eRR) > 1e-12 ? std::clamp (-db (eSum / (2.0 * (eLL + eRR))), 0.0f, 40.0f) : 0.0f;
        a.esMono = a.global.sideMidDb < -40.0f;

        // actiu: per sobre de −70 dBFS i a menys de 35 dB del més fort
        const float maxNiv = *std::max_element (a.nivellDb.begin(), a.nivellDb.end());
        const float llindar = std::max (-70.0f, maxNiv - 35.0f);
        a.actiu.resize (F);
        size_t nActius = 0;
        double eActiu = 0.0;
        for (size_t f = 0; f < F; ++f)
        {
            a.actiu[f] = a.nivellDb[f] > llindar ? 1 : 0;
            if (a.actiu[f]) { ++nActius; eActiu += std::pow (10.0, a.nivellDb[f] / 10.0); }
        }
        if (nActius == 0 || maxNiv < -80.0f) return a;
        a.valid = true;
        a.parteActiva = (float) nActius / (float) F;
        a.nivellActiuDb = db (eActiu / (double) nActius);
        a.crestaDb = a.picDb - a.nivellActiuDb;

        // trams on sona (forats de menys d'1 s units)
        {
            const double minForat = 1.0;
            for (size_t f = 0; f < F; ++f)
            {
                if (! a.actiu[f]) continue;
                const double t0 = inici + (double) f * a.salt, t1 = t0 + (double) N / fs;
                if (! a.trams.empty() && t0 - a.trams.back().second < minForat) a.trams.back().second = t1;
                else a.trams.push_back ({ t0, t1 });
            }
        }

        // transitoris: el nivell salta més de 6 dB d'una finestra a la següent
        {
            int atacs = 0, espera = 0;
            for (size_t f = 1; f < F; ++f)
            {
                if (espera > 0) { --espera; continue; }
                if (a.actiu[f] && a.nivellDb[f] - a.nivellDb[f - 1] > 6.0f) { ++atacs; espera = 2; }
            }
            a.atacsPerSegon = (float) atacs / (float) std::max (1.0, (double) nActius * a.salt);
        }

        // rang dinàmic: finestres de ~0,4 s amb la majoria de moments actius
        {
            const int g = std::max (1, (int) std::lround (0.4 / a.salt));
            std::vector<float> nivells;
            for (size_t f = 0; f + (size_t) g <= F; f += (size_t) g)
            {
                int act = 0;
                double e = 0.0;
                for (int k = 0; k < g; ++k) { act += a.actiu[f + (size_t) k]; e += std::pow (10.0, a.nivellDb[f + (size_t) k] / 10.0); }
                if (act * 2 > g) nivells.push_back (db (e / g));
            }
            if (nivells.size() >= 5) a.rangDinamicDb = (float) (percentil (nivells, 0.9) - percentil (nivells, 0.1));
            if (! nivells.empty()) a.nivellFortDb = (float) percentil (nivells, 0.9);   // 1.25
        }

        // 2) FFT de cada finestra: bandes, espectre fi dels moments actius, sibilància
        fft::FFT fft (N);
        std::vector<fft::C> x ((size_t) N);
        std::vector<float> w ((size_t) N);
        for (int i = 0; i < N; ++i) w[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / (N - 1)));
        std::vector<int> banda ((size_t) N / 2, -1);
        for (int k = 1; k < N / 2; ++k)
        {
            const double fk = (double) k * fs / N;
            const int b = (int) std::lround (3.0 * std::log2 (fk / 31.5));
            if (b >= 0 && b < NB && fk >= 28.0 && fk <= 18000.0) banda[(size_t) k] = b;
        }
        const int k1k = (int) (1000.0 * N / fs), k4k = (int) (4000.0 * N / fs);
        const int k5k = (int) (5000.0 * N / fs), k10k = std::min (N / 2 - 1, (int) (10000.0 * N / fs));
        std::vector<double> acum ((size_t) N / 2, 0.0), acumSib ((size_t) N / 2, 0.0);
        std::vector<double> pot ((size_t) N / 2, 0.0);
        a.bandesDb.resize (F);
        const float medianaActiu = (float) percentil ([&] { std::vector<float> v; for (size_t f = 0; f < F; ++f) if (a.actiu[f]) v.push_back (a.nivellDb[f]); return v; }(), 0.5);
        int nSib = 0;
        std::vector<std::pair<float, double>> sibs;   // {excés, segons}
        double eGreus = 0, eAguts = 0, eTot = 0, eSG = 0, eMG = 0, centre = 0;
        for (size_t f = 0; f < F; ++f)
        {
            const size_t o = f * (size_t) hop;
            for (int i = 0; i < N; ++i)
            {
                const float l = L[o + (size_t) i], r = R[o + (size_t) i];
                x[(size_t) i] = { 0.5f * (l + r) * w[(size_t) i], 0.5f * (l - r) * w[(size_t) i] };
            }
            fft.fa (x.data(), false);
            std::array<double, NB> eb {};
            for (int k = 1; k < N / 2; ++k)
            {
                const fft::C X = x[(size_t) k], Y { x[(size_t) (N - k)].r, -x[(size_t) (N - k)].i };
                const double mr = 0.5 * (X.r + Y.r), mi = 0.5 * (X.i + Y.i);
                const double sr = 0.5 * (X.i - Y.i), si = -0.5 * (X.r - Y.r);
                const double pm = mr * mr + mi * mi, ps = sr * sr + si * si;
                pot[(size_t) k] = pm + ps;
                if (const int b = banda[(size_t) k]; b >= 0) eb[(size_t) b] += pm + ps;
                if (a.actiu[f])
                {
                    const double fk = (double) k * fs / N;
                    if (fk < 150.0) { eSG += ps; eMG += pm; }
                }
            }
            for (int b = 0; b < NB; ++b) a.bandesDb[f][(size_t) b] = db (eb[(size_t) b]);
            if (! a.actiu[f]) continue;
            double eMid = 0, eSib = 0;
            for (int k = 1; k < N / 2; ++k)
            {
                acum[(size_t) k] += pot[(size_t) k];
                const double fk = (double) k * fs / N;
                eTot += pot[(size_t) k];
                centre += fk * pot[(size_t) k];
                if (fk < 150.0) eGreus += pot[(size_t) k];
                if (fk > 5000.0) eAguts += pot[(size_t) k];
                if (k >= k1k && k <= k4k) eMid += pot[(size_t) k];
                if (k >= k5k && k <= k10k) eSib += pot[(size_t) k];
            }
            const float exc = db (eSib) - db (eMid);
            if (exc > 3.0f && a.nivellDb[f] > medianaActiu - 12.0f)
            {
                ++nSib;
                sibs.push_back ({ exc, inici + (double) f * a.salt });
                for (int k = k5k; k <= k10k; ++k) acumSib[(size_t) k] += pot[(size_t) k];
            }
        }
        a.espectreDb.assign ((size_t) N / 2, -200.0f);
        for (int k = 1; k < N / 2; ++k) a.espectreDb[(size_t) k] = db (acum[(size_t) k] / (double) nActius);
        a.centroideHz = eTot > 0 ? (float) (centre / eTot) : 0.0f;
        a.greusPart = eTot > 0 ? (float) (eGreus / eTot) : 0.0f;
        a.agutsPart = eTot > 0 ? (float) (eAguts / eTot) : 0.0f;
        a.sideGreusDb = eMG > 1e-12 ? db (eSG) - db (eMG) : -100.0f;
        a.sibilancia = (float) nSib / (float) nActius;
        if (nSib > 0)
        {
            int km = k5k;
            for (int k = k5k; k <= k10k; ++k) if (acumSib[(size_t) k] > acumSib[(size_t) km]) km = k;
            a.freqSibilancia = (float) ((double) km * fs / N);
            std::sort (sibs.begin(), sibs.end(), [] (auto& p, auto& q) { return p.first > q.first; });
            for (const auto& s : sibs)
            {
                if (a.momentsSibilancia.size() >= 4) break;
                if (std::none_of (a.momentsSibilancia.begin(), a.momentsSibilancia.end(), [&] (double t) { return std::abs (t - s.second) < 2.0; }))
                    a.momentsSibilancia.push_back (s.second);
            }
            std::sort (a.momentsSibilancia.begin(), a.momentsSibilancia.end());
        }
        {
            std::vector<float> c;
            for (size_t f = 0; f < F; ++f) if (a.actiu[f]) c.push_back (a.correlacio[f]);
            a.correlacioMin = (float) percentil (c, 0.05);
        }
        // 1.24: soroll de fons: els moments sense música però que no són silenci digital, si són prou constants
        {
            std::vector<float> v;
            for (size_t f = 0; f < F; ++f) if (! a.actiu[f] && a.nivellDb[f] > -110.0f) v.push_back (a.nivellDb[f]);
            if ((double) v.size() * a.salt >= 1.5)
            {
                const double p50 = percentil (v, 0.5), p20 = percentil (v, 0.2), p80 = percentil (v, 0.8);
                if (p80 - p20 < 6.0) a.sorollDb = (float) p50;
            }
        }

        // 3) ressonàncies: pics estrets de l'espectre mitjà que hi són gairebé sempre (una nota no hi és sempre)
        // el nivell del voltant: la mediana de cada costat (±1/3 d'octava) i la més alta de les dues, perquè un graó
        // (un filtre que talla) no sembli un pic
        auto mediana = [] (const std::vector<float>& e, int k, int excl)
        {
            auto costat = [&] (int a0, int a1)
            {
                std::vector<float> v;
                for (int j = std::max (1, a0); j <= std::min ((int) e.size() - 1, a1); ++j) v.push_back (e[(size_t) j]);
                if (v.empty()) return e[(size_t) k];
                std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
                return v[v.size() / 2];
            };
            return std::max (costat ((int) std::floor (k / 1.26), k - excl - 1), costat (k + excl + 1, (int) std::ceil (k * 1.26)));
        };
        std::vector<Ressonancia> cand;
        {
            const int kIni = std::max (3, (int) (150.0 * N / fs)), kFi = std::min (N / 2 - 3, (int) (8000.0 * N / fs));
            const auto& e = a.espectreDb;
            for (int k = kIni; k <= kFi; ++k)
            {
                bool maxim = true;
                for (int j = -2; j <= 2 && maxim; ++j) if (j != 0 && e[(size_t) (k + j)] > e[(size_t) k]) maxim = false;
                if (! maxim) continue;
                const float exc = e[(size_t) k] - mediana (e, k, 2);
                if (exc < 6.0f) continue;
                int esq = k, dre = k;
                while (esq > 1 && e[(size_t) esq] > e[(size_t) k] - 3.0f) --esq;
                while (dre < N / 2 - 1 && e[(size_t) dre] > e[(size_t) k] - 3.0f) ++dre;
                const double f0 = (double) k * fs / N, bw = std::max (1.0, (double) (dre - esq)) * fs / N;
                Ressonancia r;
                r.freq = (float) f0;
                r.excesDb = exc;
                r.q = (float) std::clamp (f0 / bw, 1.5, 12.0);
                cand.push_back (r);
            }
            std::sort (cand.begin(), cand.end(), [] (auto& p, auto& q) { return p.excesDb > q.excesDb; });
            std::vector<Ressonancia> sep;
            for (const auto& r : cand)
            {
                if (sep.size() >= 6) break;
                if (std::none_of (sep.begin(), sep.end(), [&] (const Ressonancia& s) { return std::abs (std::log2 (s.freq / r.freq)) < 1.0 / 6.0; }))
                    sep.push_back (r);
            }
            cand = sep;
        }
        if (! cand.empty())
        {
            std::vector<int> compte (cand.size(), 0);
            std::vector<float> e ((size_t) N / 2);
            for (size_t f = 0; f < F; ++f)
            {
                if (! a.actiu[f]) continue;
                const size_t o = f * (size_t) hop;
                for (int i = 0; i < N; ++i)
                {
                    const float l = L[o + (size_t) i], r = R[o + (size_t) i];
                    x[(size_t) i] = { 0.5f * (l + r) * w[(size_t) i], 0.5f * (l - r) * w[(size_t) i] };
                }
                fft.fa (x.data(), false);
                for (int k = 1; k < N / 2; ++k)
                {
                    const fft::C X = x[(size_t) k], Y { x[(size_t) (N - k)].r, -x[(size_t) (N - k)].i };
                    const double mr = 0.5 * (X.r + Y.r), mi = 0.5 * (X.i + Y.i), sr = 0.5 * (X.i - Y.i), si = -0.5 * (X.r - Y.r);
                    e[(size_t) k] = db (mr * mr + mi * mi + sr * sr + si * si);
                }
                for (size_t c = 0; c < cand.size(); ++c)
                {
                    const int k = (int) std::lround (cand[c].freq * N / fs);
                    const float pk = std::max ({ e[(size_t) (k - 1)], e[(size_t) k], e[(size_t) (k + 1)] });
                    if (pk - mediana (e, k, 2) >= 5.0f) ++compte[c];
                }
            }
            for (size_t c = 0; c < cand.size(); ++c) cand[c].persistencia = (float) compte[c] / (float) nActius;
            a.ressonancies = cand;
        }

        // 4) gravació saturada: trossos de 4 o més mostres iguals arran del pic
        if (pic > 0.05)
        {
            for (const float* canal : { L, R })
            {
                int run = 0;
                for (size_t i = 1; i < n; ++i)
                {
                    const float v = canal[i];
                    if (std::abs (v) >= 0.9f * (float) pic && std::abs (v - canal[i - 1]) < 1e-7f) ++run;
                    else
                    {
                        if (run >= 3)
                        {
                            ++a.saturacions;
                            const double t = inici + (double) i / fs;
                            if (a.momentsSaturacio.size() < 4 && (a.momentsSaturacio.empty() || t - a.momentsSaturacio.back() > 2.0))
                                a.momentsSaturacio.push_back (t);
                        }
                        run = 0;
                    }
                }
                if (a.esMono) break;   // L = R: no cal mirar-ho dos cops
            }
        }
        return a;
    }

    // ---------------- Diagnòstic ----------------
    const char* nomPrioritat (Prioritat p)
    {
        switch (p) { case Prioritat::Alta: return "Important"; case Prioritat::Mitjana: return "A revisar"; default: return "Detall"; }
    }

    std::string textTemps (double s)
    {
        const int t = (int) std::max (0.0, std::floor (s));
        char b[16];
        std::snprintf (b, sizeof b, "%d:%02d", t / 60, t % 60);
        return b;
    }

    std::string tramDe (const std::vector<Tram>& trams, double s)
    {
        for (const auto& t : trams) if (s >= t.desDe && s < t.finsA) return t.nom;
        return {};
    }

    namespace
    {
        std::string textMoments (const std::vector<double>& m, const std::vector<Tram>& trams)
        {
            if (m.empty()) return {};
            std::string r = "sobretot a ";
            for (size_t i = 0; i < m.size(); ++i)
            {
                if (i > 0) r += i + 1 == m.size() ? " i " : ", ";
                const auto t = tramDe (trams, m[i]);
                r += textTemps (m[i]) + (t.empty() ? "" : " (" + t + ")");
            }
            return r;
        }

        Prioritat prioritatDe (float g) { return g >= 0.7f ? Prioritat::Alta : g >= 0.4f ? Prioritat::Mitjana : Prioritat::Baixa; }

        std::string textPan (double p)
        {
            if (std::abs (p) < 0.01) return "al mig";
            return std::to_string ((int) std::lround (std::abs (p) * 100.0)) + " % " + (p < 0 ? "esquerra" : "dreta");
        }

        std::string detallsEq (const Proposta& p)
        {
            switch (p.tipus)
            {
                case Proposta::PassaAlts: return p.eina + " · Passa-alts · " + hz (p.freq) + " · 12 dB/oct";
                case Proposta::Campana: return p.eina + " · Campana · " + hz (p.freq) + " · " + dbs (p.guany) + " · Q " + num (p.q);
                case Proposta::PrestatgeAguts: return p.eina + " · Prestatge d'aguts · " + hz (p.freq) + " · " + dbs (p.guany);
                case Proposta::PrestatgeGreus: return p.eina + " · Prestatge de greus · " + hz (p.freq) + " · " + dbs (p.guany);
                case Proposta::DeEsser: return p.eina + " · " + hz (p.freq) + " · que redueixi uns " + num (p.reduccioDb, 0) + " dB a les esses";
                case Proposta::Compressor: return p.eina + " · Ratio " + num (p.ratio, 0) + ":1 · atac " + num (p.atacMs, 0) + " ms · alliberament "
                                                + num (p.alliberamentMs, 0) + " ms · llindar perquè redueixi uns " + num (p.reduccioDb, 0) + " dB a les parts fortes";
                case Proposta::MonoGreus: return p.eina + " · Mono als greus: " + hz (p.freq);
                case Proposta::Fader: return p.eina + " · " + num (p.abans) + " dB → " + num (p.despres) + " dB";
                case Proposta::Panorama: return p.eina + " · " + textPan (p.abans) + " → " + textPan (p.despres);
                default: return p.eina;
            }
        }
    }

    DiagnosiPista diagnosticaPista (const std::string& idPista, const std::string& nom, const AnalisiPista& a,
                                    const Identificacio& id, const std::vector<Tram>& trams, bool esGravacio, float guanyDb)
    {
        DiagnosiPista d;
        d.pista = idPista;
        d.nom = nom;
        d.identificacio = id;
        if (! a.valid) { d.resum = "No s'hi sent res (o gairebé res): no s'ha pogut analitzar."; return d; }

        const auto inst = id.instrument;
        const auto P = perfil (inst);
        const bool segur = inst != Instrument::Desconegut && id.confianca != Confianca::Baixa;
        const Confianca confEq = segur && id.confianca == Confianca::Alta ? Confianca::Mitjana : Confianca::Baixa;
        const std::string elQue = inst == Instrument::Desconegut ? std::string ("aquesta pista") : std::string ("un instrument com aquest");
        auto nouId = [&] (const std::string& tipus, const std::string& zona) { return tipus + ":" + idPista + (zona.empty() ? "" : ":" + zona); };
        auto afegeix = [&] (Incidencia i)
        {
            i.pista = idPista;
            i.prioritat = prioritatDe (i.gravetat);
            if (i.confianca == Confianca::Baixa && i.prioritat == Prioritat::Alta) i.prioritat = Prioritat::Mitjana;
            if (i.detalls.empty() && i.proposta.tipus != Proposta::Cap) i.detalls = detallsEq (i.proposta);
            if (i.on.empty()) i.on = "a tota la pista";
            d.incidencies.push_back (std::move (i));
        };

        const float ref = nivellZona (a.global, P.ref);
        auto rel = [&] (int z) { return nivellZona (a.global, z) - ref; };

        // ---- gravació saturada (només a les pistes gravades: un sinte pot fer ones quadrades) ----
        if (esGravacio && a.saturacions >= 3)
        {
            Incidencia i;
            i.id = nouId ("saturacio", "");
            i.origen = true;
            i.gravetat = a.saturacions >= 20 ? 0.9f : 0.7f;
            i.confianca = Confianca::Mitjana;
            i.titol = "Sembla que la gravació satura";
            i.queVeig = "Hi ha " + std::to_string (a.saturacions) + " trossets on l'ona queda tallada pla, arran del màxim.";
            i.perque = "Una gravació saturada fa un so aspre i trencat que cap efecte no pot arreglar del tot.";
            i.solucio = "Escolta aquests moments. Si es nota, el millor és tornar-la a gravar amb el guany de l'entrada una mica més baix. Si no es nota a la mescla, deixa-la.";
            i.on = textMoments (a.momentsSaturacio, trams);
            i.proposta.tipus = Proposta::Revisa;
            i.proposta.eina = "Tornar a gravar";
            i.detalls = std::to_string (a.saturacions) + " trossos de 4 o més mostres iguals per sobre del 90 % del pic";
            afegeix (i);
        }

        // ---- fase i compatibilitat mono ----
        if (! a.esMono && (a.perduaMonoDb > 4.5f || a.correlacioMin < -0.3f))
        {
            Incidencia i;
            i.id = nouId ("fase", "");
            i.origen = true;
            i.gravetat = a.perduaMonoDb > 9.0f || a.global.correlacio < 0.0f ? 0.85f : 0.55f;
            i.confianca = Confianca::Alta;
            i.titol = "Problema de fase entre l'esquerra i la dreta";
            i.queVeig = "Quan se suma a mono (un mòbil, un altaveu petit) la pista perd " + num (a.perduaMonoDb) + " dB.";
            i.perque = "Els dos costats es contrarien: en mono s'aprima o gairebé desapareix, i en estèreo sona estrany, sense centre.";
            i.solucio = "Si són dos micros, prova d'invertir la polaritat d'un dels dos. Si és un efecte d'amplada, abaixa'l. Comprova-ho amb «Escolta en mono».";
            i.proposta.tipus = Proposta::Revisa;
            i.proposta.eina = "Estèreo (Escolta en mono)";
            i.detalls = "Correlació " + num (a.global.correlacio, 2) + " (la més baixa, " + num (a.correlacioMin, 2) + ") · pèrdua en mono " + num (a.perduaMonoDb) + " dB";
            afegeix (i);
        }

        // ---- 1.24: gravació fluixa (abans del fader) i soroll de fons ----
        if (esGravacio && ! std::isnan (guanyDb) && a.picDb - guanyDb < -24.0f)
        {
            Incidencia i;
            i.id = nouId ("fluixa", "");
            i.origen = true;
            i.gravetat = a.picDb - guanyDb < -36.0f ? 0.45f : 0.3f;
            i.confianca = Confianca::Alta;
            i.titol = "Gravació molt fluixa";
            i.queVeig = "El pic més fort de la gravació (abans del fader) és a " + num (a.picDb - guanyDb) + " dBFS.";
            i.perque = "Amb 24 bits no és greu, però com més fluixa, més se sent el soroll de la targeta i del micro quan l'apuges.";
            i.solucio = "A la propera presa, puja una mica el guany de l'entrada (que els pics quedin entre −18 i −8 dBFS).";
            i.proposta.tipus = Proposta::Revisa;
            i.proposta.eina = "Guany de la targeta";
            i.detalls = "Pic abans del fader: " + num (a.picDb - guanyDb) + " dBFS";
            afegeix (i);
        }
        if (esGravacio && a.sorollDb > -60.0f && a.nivellActiuDb - a.sorollDb < 40.0f)
        {
            Incidencia i;
            i.id = nouId ("soroll", "");
            i.origen = true;
            const float marge = a.nivellActiuDb - a.sorollDb;
            i.gravetat = marge < 25.0f ? 0.6f : 0.4f;
            i.confianca = Confianca::Mitjana;
            i.titol = "Soroll de fons entre frases";
            i.queVeig = "Quan no toca, hi queda un soroll constant a " + num (a.sorollDb) + " dBFS (només " + num (marge) + " dB per sota de la música).";
            i.perque = "Brunzit, ventilador o amplificador: amb compressió i sumat a altres pistes, embruta la mescla.";
            i.solucio = "Si pots, torna-la a gravar més lluny del soroll. Si no, retalla els clips als silencis o posa-hi un gate suau.";
            i.proposta.tipus = Proposta::Revisa;
            i.proposta.eina = "Dinàmica (gate) o retallar els clips";
            i.detalls = "Soroll " + num (a.sorollDb) + " dBFS · música " + num (a.nivellActiuDb) + " dBFS";
            afegeix (i);
        }

        // ---- greus en estèreo al baix i al bombo ----
        if (P.monoGreus && segur && ! a.esMono && a.sideGreusDb > -12.0f)
        {
            Incidencia i;
            i.id = nouId ("monogreus", "");
            i.gravetat = a.sideGreusDb > -6.0f ? 0.55f : 0.4f;
            i.confianca = confEq;
            i.titol = "Els greus són oberts als costats";
            i.queVeig = "Per sota de 150 Hz, la part dels costats és només " + num (-a.sideGreusDb) + " dB per sota del centre.";
            i.perque = "Els greus sonen més ferms i amb més pegada al mig; oberts, es dilueixen i donen problemes en mono.";
            i.solucio = "Posa els greus d'aquesta pista al mig.";
            i.proposta.tipus = Proposta::MonoGreus;
            i.proposta.freq = 120.0f;
            i.proposta.eina = "Estèreo";
            afegeix (i);
        }

        // ---- to (zones) ----
        if (segur)
        {
            // soroll de greus que no fan falta → passa-alts
            if (P.tallHz > 0.0f && ! std::isnan (P.maxSub) && rel (Sub) > P.maxSub)
            {
                Incidencia i;
                i.id = nouId ("tall", "");
                const float exc = rel (Sub) - P.maxSub;
                i.gravetat = std::clamp (0.35f + exc / 20.0f, 0.35f, 0.65f);
                i.confianca = confEq;
                i.titol = "Greus que sobren per sota de " + hz (P.tallHz);
                i.queVeig = "Hi ha força so per sota de " + hz (P.tallHz) + ", on " + elQue + " gairebé no hi té res a dir.";
                i.perque = "Sovint són cops al micro, vibracions o soroll de la sala: no se senten clars, però omplen els greus de la mescla i treuen marge.";
                i.solucio = "Un filtre passa-alts suau a " + hz (P.tallHz) + ".";
                i.proposta.tipus = Proposta::PassaAlts;
                i.proposta.freq = P.tallHz;
                i.proposta.eina = "EQ Pro";
                afegeix (i);
            }
            if (! std::isnan (P.maxGreusPlats) && rel (Greus) > P.maxGreusPlats)
            {
                Incidencia i;
                i.id = nouId ("tall", "");
                i.gravetat = 0.45f;
                i.confianca = confEq;
                i.titol = "Els plats porten greus";
                i.queVeig = "Els micros dels plats agafen força greus (bombo, toms, sala).";
                i.perque = "Aquests greus es barregen amb els del bombo i el baix i embruten la part de baix de la mescla.";
                i.solucio = "Un passa-alts a uns " + hz (P.tallHz) + " (escolta que el cos de la caixa no desaparegui).";
                i.proposta.tipus = Proposta::PassaAlts;
                i.proposta.freq = P.tallHz;
                i.proposta.eina = "EQ Pro";
                afegeix (i);
            }
            if (! std::isnan (P.maxGreus) && rel (Greus) > P.maxGreus)
            {
                Incidencia i;
                const int b = bandaMaxima (a.global, Greus);
                const float exc = rel (Greus) - P.maxGreus;
                i.id = nouId ("greus", "");
                i.gravetat = std::clamp (0.35f + exc / 15.0f, 0.35f, 0.7f);
                i.confianca = confEq;
                i.titol = "Massa greus";
                i.queVeig = "Els greus (" + hz (diagnosi::freqBanda (b)) + ") són " + num (exc) + " dB més forts del que és habitual per a " + nomInstrument (inst) + ".";
                i.perque = "Fa que soni gruixut i que trepitgi el baix i el bombo.";
                i.solucio = "Una retallada suau dels greus.";
                i.proposta.tipus = Proposta::PrestatgeGreus;
                i.proposta.freq = diagnosi::freqBanda (b) * 1.4f;
                i.proposta.guany = -std::clamp (std::round (exc * 0.6f * 2.0f) / 2.0f, 1.5f, 6.0f);
                i.proposta.eina = "EQ Pro";
                afegeix (i);
            }
            if (! std::isnan (P.maxFang) && rel (Fang) > P.maxFang)
            {
                Incidencia i;
                const int b = bandaMaxima (a.global, Fang);
                const float exc = rel (Fang) - P.maxFang;
                i.id = nouId ("fang", "");
                i.gravetat = std::clamp (0.4f + exc / 12.0f, 0.4f, 0.8f);
                i.confianca = confEq;
                const bool cops = inst == Instrument::Bombo || inst == Instrument::Caixa || inst == Instrument::Toms;
                i.titol = cops ? "So de caixa de cartró (mitjos baixos)" : "Excés de mitjos baixos";
                i.queVeig = "Hi ha massa energia a la zona de " + hz (diagnosi::freqBanda (b)) + ": " + num (exc) + " dB més del que és habitual.";
                i.perque = cops ? "Fa que el cop soni tancat i de cartró, sense pegada."
                                : "És la zona que fa que una mescla soni embolicada i tapada: quan molts instruments en tenen massa, tot es confon.";
                i.solucio = "Una retallada de " + dbs (-std::clamp (std::round (exc * 0.6f * 2.0f) / 2.0f, 1.5f, 6.0f)) + " a " + hz (diagnosi::freqBanda (b)) + ", no gaire estreta.";
                i.proposta.tipus = Proposta::Campana;
                i.proposta.freq = diagnosi::freqBanda (b);
                i.proposta.guany = -std::clamp (std::round (exc * 0.6f * 2.0f) / 2.0f, 1.5f, 6.0f);
                i.proposta.q = 1.2f;
                i.proposta.eina = "EQ Pro";
                afegeix (i);
            }
            if (! std::isnan (P.minMitjos) && rel (Mitjos) < P.minMitjos)
            {
                Incidencia i;
                const float falta = P.minMitjos - rel (Mitjos);
                i.id = nouId ("definicio", "mitjos");
                i.gravetat = std::clamp (0.35f + falta / 15.0f, 0.35f, 0.6f);
                i.confianca = Confianca::Baixa;
                i.titol = "Poca definició";
                i.queVeig = "Gairebé tot és greu: la zona de 700 Hz a 1,5 kHz, on se sent la nota i l'atac, és molt fluixa.";
                i.perque = "En un altaveu petit (un mòbil) els greus no se senten, i llavors el baix desapareix.";
                i.solucio = "Una mica més de mitjos al voltant de 800 Hz, o saturació suau perquè en surtin harmònics.";
                i.proposta.tipus = Proposta::Campana;
                i.proposta.freq = 800.0f;
                i.proposta.guany = std::clamp (std::round (falta * 0.4f * 2.0f) / 2.0f, 1.5f, 4.0f);
                i.proposta.q = 1.0f;
                i.proposta.eina = "EQ Pro";
                afegeix (i);
            }
            if (! std::isnan (P.maxPres) && rel (Presencia) > P.maxPres)
            {
                Incidencia i;
                const int b = bandaMaxima (a.global, Presencia);
                const float exc = rel (Presencia) - P.maxPres;
                i.id = nouId ("dur", "");
                i.gravetat = std::clamp (0.4f + exc / 12.0f, 0.4f, 0.75f);
                i.confianca = confEq;
                i.titol = "So dur o estrident";
                i.queVeig = "La zona de " + hz (diagnosi::freqBanda (b)) + " és " + num (exc) + " dB més forta del que és habitual.";
                i.perque = "És la zona on l'orella és més sensible: massa energia aquí cansa i fa que la pista soni agressiva.";
                i.solucio = "Una retallada de " + dbs (-std::clamp (std::round (exc * 0.6f * 2.0f) / 2.0f, 1.5f, 5.0f)) + " a " + hz (diagnosi::freqBanda (b)) + ".";
                i.proposta.tipus = Proposta::Campana;
                i.proposta.freq = diagnosi::freqBanda (b);
                i.proposta.guany = -std::clamp (std::round (exc * 0.6f * 2.0f) / 2.0f, 1.5f, 5.0f);
                i.proposta.q = 1.5f;
                i.proposta.eina = "EQ Pro";
                afegeix (i);
            }
            if (! std::isnan (P.minPres) && rel (Presencia) < P.minPres)
            {
                Incidencia i;
                const float falta = P.minPres - rel (Presencia);
                i.id = nouId ("definicio", "presencia");
                i.gravetat = std::clamp (0.35f + falta / 15.0f, 0.35f, 0.6f);
                i.confianca = Confianca::Baixa;
                const bool cops = inst == Instrument::Bombo;
                i.titol = cops ? "Al bombo li falta el clic" : "Poca presència";
                i.queVeig = cops ? "El bombo gairebé no té so a 3–4 kHz (el clic de la maceta)." : "La zona de 2 a 5 kHz és fluixa (" + num (falta) + " dB menys del que és habitual).";
                i.perque = cops ? "Sense el clic, el bombo se sent però no s'entén en una mescla plena." : "És la zona que fa que un instrument se senti a prop i clar; sense, queda darrere i apagat.";
                i.solucio = "Una mica més de " + std::string (cops ? "3,5 kHz" : "3 kHz") + ", suau i ampla (o abans, retalla el que sobri a sota).";
                i.proposta.tipus = Proposta::Campana;
                i.proposta.freq = cops ? 3500.0f : 3000.0f;
                i.proposta.guany = std::clamp (std::round (falta * 0.4f * 2.0f) / 2.0f, 1.5f, 3.5f);
                i.proposta.q = cops ? 1.2f : 0.9f;
                i.proposta.eina = "EQ Pro";
                afegeix (i);
            }
            if (! std::isnan (P.maxBrill) && rel (Brillantor) > P.maxBrill)
            {
                Incidencia i;
                const float exc = rel (Brillantor) - P.maxBrill;
                i.id = nouId ("aguts", "");
                i.gravetat = std::clamp (0.35f + exc / 15.0f, 0.35f, 0.65f);
                i.confianca = confEq;
                i.titol = "Excés d'aguts";
                i.queVeig = "De 6 a 10 kHz hi ha " + num (exc) + " dB més del que és habitual.";
                i.perque = inst == Instrument::GuitarraElectrica ? "En guitarres distorsionades és el «fizz»: un brunzit aspre que no aporta res."
                                                                  : "Sona prim i punxegut, i es menja l'espai dels plats i de l'aire de la mescla.";
                i.solucio = "Un prestatge d'aguts suau a 8 kHz.";
                i.proposta.tipus = Proposta::PrestatgeAguts;
                i.proposta.freq = 8000.0f;
                i.proposta.guany = -std::clamp (std::round (exc * 0.5f * 2.0f) / 2.0f, 1.5f, 5.0f);
                i.proposta.eina = "EQ Pro";
                afegeix (i);
            }
        }

        // ---- ressonàncies ----
        for (const auto& r : a.ressonancies)
        {
            if (r.persistencia < 0.6f || r.excesDb < 7.0f) continue;
            const bool tonal = esTonal (inst) && P.ressonanciesMusicals;
            // al baix, al bombo i als toms els pics són la nota o el cos: no es toquen
            if (inst == Instrument::Baix || inst == Instrument::Bombo || inst == Instrument::Toms) continue;
            Incidencia i;
            i.id = nouId ("ressonancia", std::to_string ((int) std::lround (6.0 * std::log2 (r.freq / 100.0))));
            i.confianca = (tonal && r.persistencia < 0.8f) || inst == Instrument::Desconegut ? Confianca::Baixa : r.excesDb >= 12.0f ? Confianca::Alta : Confianca::Mitjana;
            i.gravetat = std::clamp (0.3f + (r.excesDb - 7.0f) / 12.0f + (r.persistencia - 0.6f), 0.3f, 0.8f);
            i.titol = "Ressonància a " + hz (r.freq);
            i.queVeig = "Hi ha un pic estret a " + hz (r.freq) + " (" + num (r.excesDb) + " dB per sobre del voltant) que hi és gairebé sempre, toqui la nota que toqui.";
            i.perque = "Sol ser la sala, el cos de l'instrument o el micro: fa un xiulet o un «bum» que es repeteix i cansa.";
            i.solucio = "Una retallada estreta a " + hz (r.freq) + ". Escolta-ho abans: si és una nota que es repeteix molt, deixa-la.";
            i.proposta.tipus = Proposta::Campana;
            i.proposta.freq = r.freq;
            i.proposta.guany = -std::clamp (std::round (r.excesDb * 0.5f * 2.0f) / 2.0f, 2.0f, 8.0f);
            i.proposta.q = r.q;
            i.proposta.eina = "EQ Pro";
            i.detalls = detallsEq (i.proposta) + " · hi és el " + std::to_string ((int) std::lround (r.persistencia * 100.0f)) + " % del temps";
            i.origen = r.excesDb >= 12.0f && r.persistencia >= 0.8f;   // molt marcada: millor mirar el micro o la sala
            afegeix (i);
            if (d.incidencies.size() > 12) break;
        }

        // ---- sibilància ----
        if (P.sibilancia && segur && a.sibilancia > 0.03f && a.freqSibilancia > 0.0f)
        {
            Incidencia i;
            i.id = nouId ("sibilancia", "");
            i.gravetat = a.sibilancia > 0.08f ? 0.75f : 0.5f;
            i.confianca = Confianca::Mitjana;
            i.titol = "Esses fortes (sibilància)";
            i.queVeig = "Les esses i les ce trenquen fort, sobretot a " + hz (a.freqSibilancia) + ".";
            i.perque = "Punxen l'orella i encara es fan més fortes amb la compressió, l'EQ d'aguts i la reverb.";
            i.solucio = "Un de-esser a " + hz (a.freqSibilancia) + " que les abaixi uns quants dB només quan hi són.";
            i.on = textMoments (a.momentsSibilancia, trams);
            i.proposta.tipus = Proposta::DeEsser;
            i.proposta.freq = a.freqSibilancia;
            i.proposta.reduccioDb = a.sibilancia > 0.08f ? 6.0f : 4.0f;
            i.proposta.eina = "Dinàmica (de-esser)";
            afegeix (i);
        }

        // ---- dinàmica ----
        if (segur && ! std::isnan (P.maxRang) && a.rangDinamicDb > P.maxRang)
        {
            Incidencia i;
            const float exc = a.rangDinamicDb - P.maxRang;
            i.id = nouId ("dinamica", "");
            i.gravetat = std::clamp (0.4f + exc / 12.0f, 0.4f, 0.7f);
            i.confianca = Confianca::Mitjana;
            i.titol = "Massa diferència entre les parts fluixes i les fortes";
            i.queVeig = "Entre els moments fluixos i els forts hi ha " + num (a.rangDinamicDb) + " dB.";
            i.perque = inst == Instrument::Veu || inst == Instrument::Cors ? "Unes paraules es perden sota la música i d'altres sobresurten massa."
                                                                           : "A estones queda tapat i a estones salta per sobre de la resta.";
            i.solucio = "Una compressió suau perquè el volum sigui més constant (i, si cal, ajustos de volum del clip a les frases que es perden).";
            i.proposta.tipus = Proposta::Compressor;
            i.proposta.ratio = P.ratio;
            i.proposta.atacMs = P.atac;
            i.proposta.alliberamentMs = P.alliberament;
            i.proposta.reduccioDb = std::clamp (std::round (exc * 0.5f + 3.0f), 3.0f, 8.0f);
            i.proposta.eina = "Dinàmica (compressor)";
            afegeix (i);
        }
        if (P.cops && segur && a.crestaDb < 6.0f && a.atacsPerSegon > 0.5f)
        {
            Incidencia i;
            i.id = nouId ("aixafat", "");
            i.gravetat = 0.3f;
            i.confianca = Confianca::Mitjana;
            i.titol = "Cops molt aixafats";
            i.queVeig = "Entre el pic del cop i el nivell mitjà hi ha només " + num (a.crestaDb) + " dB.";
            i.perque = "Si la compressió es menja l'atac, el cop perd pegada.";
            i.solucio = "Si hi ha un compressor, prova un atac més lent o menys reducció.";
            i.proposta.tipus = Proposta::Revisa;
            i.proposta.eina = "Dinàmica";
            i.detalls = "Cresta " + num (a.crestaDb) + " dB";
            afegeix (i);
        }

        // ---- al llarg de la cançó: diferències entre parts ----
        if (! trams.empty() && ! a.nivellDb.empty())
        {
            std::vector<std::pair<std::string, float>> perPart;
            for (const auto& t : trams)
            {
                std::vector<float> v;
                for (size_t f = 0; f < a.nivellDb.size(); ++f)
                {
                    const double s = a.inici + (double) f * a.salt;
                    if (a.actiu[f] && s >= t.desDe && s < t.finsA) v.push_back (a.nivellDb[f]);
                }
                if (v.size() * a.salt >= 3.0) perPart.push_back ({ t.nom, (float) percentil (v, 0.5) });
            }
            if (perPart.size() >= 2)
            {
                auto [mn, mx] = std::minmax_element (perPart.begin(), perPart.end(), [] (auto& p, auto& q) { return p.second < q.second; });
                if (mx->second - mn->second > 9.0f)
                {
                    Incidencia i;
                    i.id = nouId ("parts", "");
                    i.gravetat = 0.3f;
                    i.confianca = Confianca::Mitjana;
                    i.titol = "Canvia molt de volum d'una part a l'altra";
                    i.queVeig = "A «" + mx->first + "» sona " + num (mx->second - mn->second) + " dB més fort que a «" + mn->first + "».";
                    i.perque = "Pot ser volgut (la tornada ha de pujar), però si és massa, en una part es perd i a l'altra tapa.";
                    i.solucio = "Si no és volgut, ajusta-ho amb automatització de volum o amb el volum dels clips.";
                    i.on = "entre «" + mx->first + "» i «" + mn->first + "»";
                    i.proposta.tipus = Proposta::Revisa;
                    i.proposta.eina = "Automatització de volum";
                    i.detalls = "Nivell mitjà per parts: ";
                    for (size_t k = 0; k < perPart.size(); ++k) i.detalls += (k ? ", " : "") + perPart[k].first + " " + num (perPart[k].second) + " dBFS";
                    afegeix (i);
                }
            }
        }

        std::sort (d.incidencies.begin(), d.incidencies.end(), [] (const Incidencia& p, const Incidencia& q) { return p.gravetat > q.gravetat; });

        int alta = 0, mitjana = 0;
        for (const auto& i : d.incidencies) { alta += i.prioritat == Prioritat::Alta; mitjana += i.prioritat == Prioritat::Mitjana; }
        if (d.incidencies.empty()) d.resum = "No hi veig res important.";
        else if (alta > 0) d.resum = std::to_string (alta) + (alta == 1 ? " cosa important" : " coses importants") + (mitjana > 0 ? " i " + std::to_string (mitjana) + " a revisar." : ".");
        else if (mitjana > 0) d.resum = std::to_string (mitjana) + (mitjana == 1 ? " cosa a revisar." : " coses a revisar.");
        else d.resum = "Només detalls petits.";
        if (! segur) d.resum += " (Sense saber quin instrument és, només miro les coses generals.)";

        d.mesures = "Nivell quan sona: " + num (a.nivellActiuDb) + " dBFS · Pic: " + num (a.picDb) + " dBFS · True peak: " + num (a.truePeakDb)
                  + " dBTP · Rang dinàmic: " + num (a.rangDinamicDb) + " dB · Cresta: " + num (a.crestaDb) + " dB · "
                  + (a.esMono ? std::string ("Mono") : "Correlació: " + num (a.global.correlacio, 2) + " · Pèrdua en mono: " + num (a.perduaMonoDb) + " dB")
                  + " · Sona el " + std::to_string ((int) std::lround (a.parteActiva * 100.0f)) + " % del temps";
        return d;
    }
}

// =====================================================================================================================
// 1.24: fase 2 — conflictes entre pistes, equilibri i la mescla sencera
namespace mibase::mix
{
    namespace
    {
        double ener (float dbv) { return std::pow (10.0, (double) dbv / 10.0); }
        double eZona (const AnalisiPista& a, size_t f, int b0, int b1)
        {
            double e = 0.0;
            for (int b = b0; b <= b1; ++b) e += ener (a.bandesDb[f][(size_t) b]);
            return e;
        }
        /** La finestra de «a» al moment t (segons de la cançó), o -1. */
        long frameA (const AnalisiPista& a, double t)
        {
            if (a.salt <= 0.0) return -1;
            const long f = std::lround ((t - a.inici) / a.salt);
            return f >= 0 && f < (long) a.nivellDb.size() ? f : -1;
        }
        /** Pes aproximat de l'oïda per banda (com una ponderació K senzilla): els subgreus compten poc, la presència una mica més. */
        double pes (int b)
        {
            const float f = diagnosi::freqBanda (b);
            return std::pow (10.0, (f < 60.0f ? -10.0 : f < 100.0f ? -4.0 : f < 1500.0f ? 0.0 : 3.0) / 10.0);
        }
        double eSonora (const AnalisiPista& a, size_t f)
        {
            double e = 0.0;
            for (int b = 0; b < NB; ++b) e += pes (b) * ener (a.bandesDb[f][(size_t) b]);
            return e;
        }
        bool es (Instrument i, std::initializer_list<Instrument> l) { return std::find (l.begin(), l.end(), i) != l.end(); }
        bool segura (const PistaMescla& p)
        {
            return p.analisi != nullptr && p.analisi->valid && p.ident.instrument != Instrument::Desconegut && p.ident.confianca != Confianca::Baixa;
        }
        std::string cm (const std::string& n) { return "«" + n + "»"; }
        bool esGuitarra (Instrument i) { return i == Instrument::GuitarraElectrica || i == Instrument::GuitarraAcustica; }

        /** On passa: les parts que hi surten més (si n'hi ha) o uns quants moments. */
        std::string onPassa (const std::vector<double>& moments, const std::vector<Tram>& trams)
        {
            if (moments.empty()) return {};
            if (! trams.empty())
            {
                std::vector<std::pair<int, std::string>> c;
                for (const auto& t : trams)
                {
                    int n = 0;
                    for (double m : moments) n += m >= t.desDe && m < t.finsA;
                    if (n == 0) continue;
                    auto it = std::find_if (c.begin(), c.end(), [&] (auto& x) { return x.second == t.nom; });
                    if (it != c.end()) it->first += n; else c.push_back ({ n, t.nom });
                }
                std::sort (c.begin(), c.end(), [] (auto& x, auto& y) { return x.first > y.first; });
                std::string r;
                int k = 0;
                for (const auto& [n, nom] : c)
                {
                    if (k >= 3 || n < (int) (0.2 * (double) moments.size())) break;
                    r += (k ? ", " : "") + cm (nom);
                    ++k;
                }
                if (! r.empty()) return "sobretot a " + r;
            }
            std::vector<double> m;
            for (double t : moments) if (m.empty() || t - m.back() > 8.0) m.push_back (t);
            if (m.size() > 3) m.resize (3);
            return textMoments (m, trams);
        }

        struct ReglaConflicte
        {
            Instrument tapada;
            std::vector<Instrument> tapen;
            int b0, b1;   // bandes de terç d'octava
        };
        const std::vector<ReglaConflicte>& reglesConflicte()
        {
            using I = Instrument;
            static const std::vector<ReglaConflicte> r {
                // la veu: presència i definició (1–5 kHz)
                { I::Veu, { I::GuitarraElectrica, I::GuitarraAcustica, I::Piano, I::Teclats, I::Pad, I::Sinte, I::Corda, I::Vents, I::Caixa, I::Cors }, 15, 22 },
                // el bombo i el baix (40–125 Hz)
                { I::Bombo, { I::Baix }, 1, 6 },
                // les guitarres i els teclats (250 Hz–2 kHz)
                { I::GuitarraElectrica, { I::Piano, I::Teclats, I::Pad, I::Sinte }, 9, 18 },
                { I::GuitarraAcustica, { I::Piano, I::Teclats, I::Pad, I::Sinte }, 9, 18 },
            };
            return r;
        }
    }

    std::vector<Incidencia> conflictes (const std::vector<PistaMescla>& pistes, const std::vector<Tram>& trams)
    {
        std::vector<Incidencia> v;
        for (const auto& A : pistes)
        {
            if (! segura (A)) continue;
            for (const auto& regla : reglesConflicte())
            {
                if (regla.tapada != A.ident.instrument) continue;
                for (const auto& B : pistes)
                {
                    if (&B == &A || ! segura (B)) continue;
                    if (std::find (regla.tapen.begin(), regla.tapen.end(), B.ident.instrument) == regla.tapen.end()) continue;
                    const auto& a = *A.analisi;
                    const auto& b = *B.analisi;
                    if (a.bandesDb.empty() || b.bandesDb.empty()) continue;
                    // la tapada ha de tenir alguna cosa a dir en aquesta zona
                    double zA = 0.0, totA = 0.0;
                    for (int k = 0; k < NB; ++k)
                    {
                        const double e = a.global.mid[(size_t) k] + a.global.side[(size_t) k];
                        totA += e;
                        if (k >= regla.b0 && k <= regla.b1) zA += e;
                    }
                    if (totA <= 0.0 || db (zA) - db (totA) < -15.0f) continue;
                    int comu = 0, tapat = 0;
                    double sumaDif = 0.0;
                    std::vector<double> sA ((size_t) NB, 0.0), sB ((size_t) NB, 0.0), moments;
                    for (size_t f = 0; f < a.nivellDb.size(); ++f)
                    {
                        if (! a.actiu[f]) continue;
                        const double t = a.inici + (double) f * a.salt;
                        const long g = frameA (b, t);
                        if (g < 0 || ! b.actiu[(size_t) g]) continue;
                        ++comu;
                        const double ea = eZona (a, f, regla.b0, regla.b1), eb = eZona (b, (size_t) g, regla.b0, regla.b1);
                        const double d = db (eb) - db (ea);
                        if (d <= -3.0) continue;
                        ++tapat;
                        sumaDif += d;
                        moments.push_back (t);
                        for (int k = regla.b0; k <= regla.b1; ++k)
                        {
                            sA[(size_t) k] += ener (a.bandesDb[f][(size_t) k]);
                            sB[(size_t) k] += ener (b.bandesDb[(size_t) g][(size_t) k]);
                        }
                    }
                    const double tComu = comu * a.salt;
                    if (tComu < 6.0 || comu == 0) continue;
                    const double part = (double) tapat / (double) comu;
                    if (part < 0.4) continue;   // comparteixen la zona, però no es tapen prou: és normal
                    const double mitjana = sumaDif / std::max (1, tapat);
                    int bm = regla.b0;
                    for (int k = regla.b0; k <= regla.b1; ++k) if (sA[(size_t) k] * sB[(size_t) k] > sA[(size_t) bm] * sB[(size_t) bm]) bm = k;
                    const bool bomboBaix = A.ident.instrument == Instrument::Bombo;
                    const int percent = (int) std::lround (part * 100.0);

                    Incidencia i;
                    i.id = "conflicte:" + A.id + ":" + B.id + ":" + std::to_string (regla.b0);
                    i.pista = B.id;
                    i.pistes = { A.id, B.id };
                    i.gravetat = (float) std::clamp (0.4 + (part - 0.4) * 0.8 + std::max (0.0, mitjana) / 20.0, 0.4, 0.85);
                    i.confianca = A.ident.confianca == Confianca::Alta && B.ident.confianca == Confianca::Alta ? Confianca::Mitjana : Confianca::Baixa;
                    i.on = onPassa (moments, trams);
                    i.proposta.tipus = Proposta::Campana;
                    i.proposta.freq = diagnosi::freqBanda (bm);
                    i.proposta.guany = -std::clamp (std::round ((float) (mitjana + 3.0) * 0.6f * 2.0f) / 2.0f, 2.0f, 5.0f);
                    i.proposta.q = bomboBaix ? 1.4f : 1.0f;
                    i.proposta.eina = "EQ Pro (a " + cm (B.nom) + ")";
                    const std::string zona = hz (diagnosi::freqBanda (regla.b0)) + "–" + hz (diagnosi::freqBanda (regla.b1));
                    if (bomboBaix)
                    {
                        i.titol = "El baix i el bombo es confonen";
                        i.queVeig = "Als greus (" + zona + "), quan sonen junts, " + cm (B.nom) + " té tanta energia com " + cm (A.nom)
                                  + " el " + std::to_string (percent) + " % del temps.";
                        i.perque = "Els greus s'amunteguen: o el bombo perd el cop o el baix perd la nota, i la mescla sona gruixuda i poc clara.";
                        i.solucio = "Fes un petit forat a " + cm (B.nom) + " a " + hz (i.proposta.freq) + " (on el bombo té el cop), d'uns "
                                  + dbs (i.proposta.guany) + ".";
                        i.altres = {
                            "Sidechain: que el bombo comprimeixi el baix 2–4 dB a cada cop (Dinàmica amb sidechain). Molt eficaç; si n'hi ha massa, se sent el bombeig.",
                            "Repartiu-vos els greus: el bombo amb més pes a sota (50–60 Hz) i el baix una mica més amunt (80–120 Hz), o al revés.",
                            "Que només un dels dos faci el sub: un passa-alts suau a l'altre (30–40 Hz).",
                        };
                    }
                    else
                    {
                        const bool veu = A.ident.instrument == Instrument::Veu;
                        i.titol = cm (B.nom) + (veu ? " tapa la veu" : " ocupa l'espai de " + cm (A.nom));
                        i.queVeig = "A la zona de " + zona + ", quan sonen junts, " + cm (B.nom) + " hi té tanta energia com " + cm (A.nom)
                                  + " el " + std::to_string (percent) + " % del temps.";
                        i.perque = veu ? "Quan sonen alhora, la veu perd definició: costa entendre-la i acabes apujant-la massa."
                                       : "Dos instruments d'acompanyament al mateix registre es fonen en una massa i cap dels dos es defineix.";
                        i.solucio = "Fes-li lloc: retalla " + cm (B.nom) + " " + dbs (i.proposta.guany) + " a " + hz (i.proposta.freq) + " (no gaire estret).";
                        i.altres = {
                            "EQ dinàmica a " + cm (B.nom) + " a " + hz (i.proposta.freq) + " que només actuï quan sona " + cm (A.nom)
                                + " (EQ Pro amb sidechain): no li treu res quan " + cm (A.nom) + " calla.",
                            "Sidechain: que " + cm (A.nom) + " comprimeixi una mica " + cm (B.nom) + " (Dinàmica amb sidechain). Eficaç; si és massa, es nota el bombeig.",
                            "Panoramitza " + cm (B.nom) + " cap a un costat (30–50 %) i deixa el mig per a " + cm (A.nom) + ". En mono no ajuda.",
                            "Baixa " + cm (B.nom) + " 1–2 dB: el més senzill, però perd presència a tota la cançó.",
                        };
                    }
                    i.detalls = "Sonen junts " + num (tComu, 0) + " s · es tapen el " + std::to_string (percent) + " % · de mitjana "
                              + num (mitjana) + " dB · " + detallsEq (i.proposta);
                    i.prioritat = prioritatDe (i.gravetat);
                    if (i.confianca == Confianca::Baixa && i.prioritat == Prioritat::Alta) i.prioritat = Prioritat::Mitjana;
                    v.push_back (std::move (i));
                }
            }
        }
        std::sort (v.begin(), v.end(), [] (const Incidencia& x, const Incidencia& y) { return x.gravetat > y.gravetat; });
        return v;
    }

    std::vector<Incidencia> balanc (const std::vector<PistaMescla>& pistes, std::vector<CanviBalanc>& proposta)
    {
        std::vector<Incidencia> v;
        proposta.clear();
        // una graella comuna (totes les pistes tenen el mateix salt: mateixa freqüència i mateixa FFT)
        std::vector<int> ok;
        double t0 = 1e30, t1 = -1e30, salt = 0.0;
        for (int i = 0; i < (int) pistes.size(); ++i)
        {
            const auto* a = pistes[(size_t) i].analisi;
            if (a == nullptr || ! a->valid || a->nivellDb.empty()) continue;
            ok.push_back (i);
            t0 = std::min (t0, a->inici);
            t1 = std::max (t1, a->inici + (double) a->nivellDb.size() * a->salt);
            salt = a->salt;
        }
        if (ok.size() < 2 || salt <= 0.0) return v;
        const size_t K = (size_t) std::ceil ((t1 - t0) / salt) + 1;
        std::vector<std::vector<double>> E (pistes.size());
        for (int i : ok)
        {
            const auto& a = *pistes[(size_t) i].analisi;
            E[(size_t) i].assign (K, 0.0);
            for (size_t f = 0; f < a.nivellDb.size(); ++f)
            {
                if (! a.actiu[f]) continue;
                const long k = std::lround ((a.inici + (double) f * a.salt - t0) / salt);
                if (k >= 0 && (size_t) k < K) E[(size_t) i][(size_t) k] = eSonora (a, f);
            }
        }
        auto quins = [&] (std::function<bool (const PistaMescla&)> f)
        {
            std::vector<int> r;
            for (int i : ok) if (f (pistes[(size_t) i])) r.push_back (i);
            return r;
        };
        /** Mediana de (qui − contra) en dB als moments que sonen tots dos. {dB, segons}. */
        auto relacio = [&] (const std::vector<int>& qui, const std::vector<int>& contra)
        {
            std::vector<float> d;
            for (size_t k = 0; k < K; ++k)
            {
                double a = 0.0, b = 0.0;
                for (int i : qui) a += E[(size_t) i][k];
                for (int i : contra) b += E[(size_t) i][k];
                if (a > 0.0 && b > 0.0) d.push_back (db (a) - db (b));
            }
            return std::make_pair (d.empty() ? 0.0 : percentil (d, 0.5), (double) d.size() * salt);
        };
        auto inst = [&] (Instrument x) { return quins ([x] (const PistaMescla& p) { return segura (p) && p.ident.instrument == x; }); };
        auto resta = [&] (const std::vector<int>& treu) { return quins ([&] (const PistaMescla& p)
            { const int i = (int) (&p - pistes.data()); return std::find (treu.begin(), treu.end(), i) == treu.end(); }); };

        std::map<std::string, CanviBalanc> canvis;
        auto canvi = [&] (const PistaMescla& p) -> CanviBalanc&
        {
            auto it = canvis.find (p.id);
            if (it == canvis.end())
            {
                CanviBalanc c;
                c.pista = p.id; c.nom = p.nom;
                c.faderAbans = c.fader = p.faderDb;
                c.panAbans = c.pan = p.pan;
                it = canvis.emplace (p.id, c).first;
            }
            return it->second;
        };
        auto nova = [&] (const PistaMescla& p, const std::string& clau, float gravetat, const std::string& titol, const std::string& queVeig,
                         const std::string& perque, double deltaDb, double mesura)
        {
            Incidencia i;
            i.id = "balanc:" + p.id + ":" + clau;
            i.pista = p.id;
            i.gravetat = gravetat;
            i.confianca = Confianca::Baixa;
            i.titol = titol;
            i.queVeig = queVeig;
            i.perque = perque;
            i.on = "quan sonen junts";
            if (p.autoVolum)
            {
                i.solucio = cm (p.nom) + " té automatització de volum: no en toco el fader. Puja o baixa la línia d'automatització uns "
                          + num (std::abs (deltaDb)) + " dB.";
                i.proposta.tipus = Proposta::Revisa;
                i.proposta.eina = "Automatització de volum";
            }
            else
            {
                auto& c = canvi (p);
                c.fader = std::clamp (c.fader + deltaDb, -60.0, 6.0);
                i.solucio = (deltaDb > 0 ? "Puja " : "Baixa ") + cm (p.nom) + " uns " + num (std::abs (deltaDb)) + " dB (és al Balanç proposat: escolta'l abans).";
                i.proposta.tipus = Proposta::Fader;
                i.proposta.abans = (float) c.faderAbans;
                i.proposta.despres = (float) c.fader;
                i.proposta.eina = "Fader de " + cm (p.nom);
            }
            i.detalls = detallsEq (i.proposta) + " · mesura: " + num (mesura) + " dB";
            i.prioritat = prioritatDe (gravetat);
            if (i.prioritat == Prioritat::Alta) i.prioritat = Prioritat::Mitjana;   // l'equilibri sempre és una proposta
            v.push_back (std::move (i));
        };
        auto arrodoneix = [] (double x) { return std::round (x * 2.0) / 2.0; };

        // ---- la veu respecte de la resta ----
        for (int iv : inst (Instrument::Veu))
        {
            const auto [d, seg] = relacio ({ iv }, resta ({ iv }));
            if (seg < 5.0) continue;
            const auto& p = pistes[(size_t) iv];
            if (d < -4.0)
                nova (p, "veu-enrere", (float) std::clamp (0.45 + (-4.0 - d) / 15.0, 0.45, 0.7), "La veu queda enrere",
                      "Quan canta, " + cm (p.nom) + " sona " + num (-d) + " dB per sota de la resta de la banda.",
                      "La veu és el que la gent escolta primer: si queda enrere, la cançó perd força i no s'entén la lletra.",
                      std::min (6.0, arrodoneix (-1.0 - d)), d);
            else if (d > 5.0)
                nova (p, "veu-davant", (float) std::clamp (0.4 + (d - 5.0) / 15.0, 0.4, 0.6), "La veu va massa davant",
                      "Quan canta, " + cm (p.nom) + " sona " + num (d) + " dB per sobre de la resta de la banda.",
                      "Sembla que la veu no estigui dins la cançó (com un karaoke) i la banda perd pes.",
                      -std::min (6.0, arrodoneix (d - 2.0)), d);
        }
        // ---- el bombo i el baix ----
        const auto bombos = inst (Instrument::Bombo), baixos = inst (Instrument::Baix);
        if (! bombos.empty() && ! baixos.empty())
        {
            const auto [d, seg] = relacio (bombos, baixos);
            if (seg >= 5.0 && std::abs (d) > 10.0)
            {
                const auto& p = pistes[(size_t) (d > 0 ? bombos.front() : baixos.front())];
                nova (p, d > 0 ? "bombo-tapa-baix" : "baix-tapa-bombo", 0.45f, d > 0 ? "El bombo tapa el baix" : "El baix es menja el bombo",
                      "Quan sonen junts, " + std::string (d > 0 ? "el bombo" : "el baix") + " sona " + num (std::abs (d)) + " dB per sobre de l'altre.",
                      "El bombo i el baix són la base: si un dels dos desapareix, la cançó perd groove.",
                      -arrodoneix ((std::abs (d) - 6.0) / 2.0), d);
            }
        }
        // ---- baix, bombo i caixa respecte de la resta ----
        struct Llindar { Instrument i; double min, max; const char* fluix; const char* fort; const char* perqueFluix; const char* perqueFort; };
        const Llindar llindars[] = {
            { Instrument::Baix, -16.0, 3.0, "El baix massa fluix", "El baix massa fort",
              "Sense baix la cançó queda prima i sense base.", "Els greus ho omplen tot: la mescla sona pesada i els altres perden espai." },
            { Instrument::Bombo, -16.0, 2.0, "El bombo massa baix", "El bombo massa dominant",
              "Sense bombo la cançó no empeny.", "Cada cop tapa tota la resta, i al màster farà que tot baixi." },
            { Instrument::Caixa, -16.0, 2.0, "La caixa sense presència", "La caixa massa forta",
              "La caixa marca el ritme: si no se sent, la cançó perd energia.", "Cada cop de caixa salta per sobre de la veu i de tot." },
        };
        for (const auto& L : llindars)
            for (int ix : inst (L.i))
            {
                const auto [d, seg] = relacio ({ ix }, resta ({ ix }));
                if (seg < 5.0) continue;
                const auto& p = pistes[(size_t) ix];
                if (d < L.min) nova (p, "fluix", 0.4f, L.fluix, cm (p.nom) + " sona " + num (-d) + " dB per sota de la resta quan toca.",
                                     L.perqueFluix, std::min (6.0, arrodoneix ((L.min + 4.0 - d))), d);
                else if (d > L.max) nova (p, "fort", 0.45f, L.fort, cm (p.nom) + " sona " + num (d) + " dB per sobre de la resta quan toca.",
                                          L.perqueFort, -std::min (6.0, arrodoneix (d - L.max + 2.0)), d);
            }
        // ---- les guitarres respecte de la veu ----
        {
            const auto gtrs = quins ([] (const PistaMescla& p) { return segura (p) && esGuitarra (p.ident.instrument); });
            const auto veus = inst (Instrument::Veu);
            if (! gtrs.empty() && ! veus.empty())
            {
                const auto [d, seg] = relacio (gtrs, veus);
                if (seg >= 5.0 && d > 5.0)
                    for (int ig : gtrs)
                        nova (pistes[(size_t) ig], "gtr-protagonistes", 0.45f, gtrs.size() > 1 ? "Les guitarres massa protagonistes" : "La guitarra massa protagonista",
                              "Quan canta la veu, " + std::string (gtrs.size() > 1 ? "les guitarres sonen" : "la guitarra sona") + " " + num (d) + " dB per sobre.",
                              "Acompanyen la veu, no hi competeixen: si manen elles, la veu s'apaga.", -arrodoneix (std::min (4.0, (d - 3.0) / 2.0)), d);
            }
        }
        // ---- qualsevol altra que domini ----
        for (int i : ok)
        {
            const auto& p = pistes[(size_t) i];
            if (! segura (p) || es (p.ident.instrument, { Instrument::Veu, Instrument::Baix, Instrument::Bombo, Instrument::Caixa })) continue;
            if (esGuitarra (p.ident.instrument) && ! inst (Instrument::Veu).empty()) continue;   // ja s'ha mirat amb la veu
            const auto [d, seg] = relacio ({ i }, resta ({ i }));
            if (seg >= 5.0 && d > 4.0 && ok.size() >= 3)
                nova (p, "domina", 0.4f, cm (p.nom) + " domina la mescla", cm (p.nom) + " sona " + num (d) + " dB per sobre de tota la resta junta.",
                      "Quan un instrument d'acompanyament mana tant, tapa la resta i la mescla perd equilibri.", -arrodoneix (std::min (6.0, d - 2.0)), d);
        }

        // ---- panorames ----
        auto pan = [&] (const PistaMescla& p, double nou, const std::string& clau, const std::string& titol, const std::string& perque)
        {
            Incidencia i;
            i.id = "balanc:" + p.id + ":" + clau;
            i.pista = p.id;
            i.gravetat = 0.35f;
            i.confianca = Confianca::Mitjana;
            i.titol = titol;
            i.perque = perque;
            if (p.autoPan)
            {
                i.queVeig = cm (p.nom) + " té automatització de panorama.";
                i.solucio = "No el toco: revisa la línia d'automatització.";
                i.proposta.tipus = Proposta::Revisa;
                i.proposta.eina = "Automatització de panorama";
            }
            else
            {
                auto& c = canvi (p);
                c.pan = nou;
                i.queVeig = cm (p.nom) + " és " + textPan (p.pan) + ".";
                i.solucio = "Posa-la " + textPan (nou) + " (és al Balanç proposat).";
                i.proposta.tipus = Proposta::Panorama;
                i.proposta.abans = (float) p.pan;
                i.proposta.despres = (float) nou;
                i.proposta.eina = "Panorama de " + cm (p.nom);
            }
            i.detalls = detallsEq (i.proposta);
            i.prioritat = Prioritat::Baixa;
            v.push_back (std::move (i));
        };
        for (int i : ok)
        {
            const auto& p = pistes[(size_t) i];
            if (segura (p) && es (p.ident.instrument, { Instrument::Veu, Instrument::Bombo, Instrument::Caixa, Instrument::Baix }) && std::abs (p.pan) > 0.25)
                pan (p, 0.0, "pan-mig", cm (p.nom) + " no és al mig",
                     "La veu, el bombo, la caixa i el baix són el centre de la cançó: al mig sonen més ferms i funcionen igual en mono.");
        }
        for (Instrument fam : { Instrument::GuitarraElectrica, Instrument::GuitarraAcustica })
        {
            std::vector<int> mig;
            for (int i : inst (fam)) if (std::abs (pistes[(size_t) i].pan) < 0.15) mig.push_back (i);
            if (mig.size() < 2) continue;
            for (size_t k = 0; k < 2; ++k)
                pan (pistes[(size_t) mig[k]], k == 0 ? -0.7 : 0.7, "pan-obre", "Les guitarres estan totes al mig",
                     "Dues guitarres al mateix lloc es tapen entre elles i tapen la veu. Una a cada costat obren la mescla i deixen el mig lliure.");
        }

        for (auto& [id, c] : canvis) if (c.tocaFader() || c.tocaPan()) proposta.push_back (c);
        std::sort (v.begin(), v.end(), [] (const Incidencia& x, const Incidencia& y) { return x.gravetat > y.gravetat; });
        return v;
    }

    std::vector<Incidencia> diagnosticaMescla (const AnalisiPista& mix, const std::vector<PistaMescla>& pistes, const std::vector<Tram>& trams)
    {
        std::vector<Incidencia> v;
        if (! mix.valid) return v;
        const auto corba = mix.global.corba();
        auto difZona = [&] (int z)
        {
            double d = 0.0;
            for (int b = zonaIni[z]; b <= zonaFi[z]; ++b) d += corba[(size_t) b] - diagnosi::referencia (b);
            return d / (zonaFi[z] - zonaIni[z] + 1);
        };
        /** Part de cada pista en una zona (%), de més a menys. */
        auto culpables = [&] (int z)
        {
            std::vector<std::pair<double, const PistaMescla*>> c;
            double tot = 0.0;
            for (const auto& p : pistes)
            {
                if (p.analisi == nullptr || ! p.analisi->valid) continue;
                double e = 0.0;
                for (int b = zonaIni[z]; b <= zonaFi[z]; ++b) e += p.analisi->global.mid[(size_t) b] + p.analisi->global.side[(size_t) b];
                c.push_back ({ e, &p });
                tot += e;
            }
            for (auto& x : c) x.first = tot > 0.0 ? 100.0 * x.first / tot : 0.0;
            std::sort (c.begin(), c.end(), [] (auto& x, auto& y) { return x.first > y.first; });
            return c;
        };
        auto llistaCulpables = [&] (const std::vector<std::pair<double, const PistaMescla*>>& c, std::vector<std::string>& ids)
        {
            std::string r;
            int k = 0;
            for (const auto& [pc, p] : c)
            {
                if (k >= 3 || pc < 10.0) break;
                r += (k ? ", " : "") + cm (p->nom) + " (" + std::to_string ((int) std::lround (pc)) + " %)";
                ids.push_back (p->id);
                ++k;
            }
            return r;
        };

        struct ZonaMix { int z; const char* sobra; const char* falta; const char* perqueSobra; const char* perqueFalta; };
        const ZonaMix zones[] = {
            { Greus, "A la mescla li sobren greus", "A la mescla li falten greus",
              "Sona pesada i bombollejant; als altaveus grans retruny i al màster tot haurà de baixar.", "Sona prima: li falta cos i empenta." },
            { Fang, "A la mescla li sobren mitjos baixos", "",
              "És el que fa que una mescla soni embolicada i tapada: tot es confon.", "" },
            { Presencia, "La mescla és dura", "A la mescla li falta presència",
              "Massa energia on l'oïda és més sensible: cansa i sona agressiva.", "Sona llunyana i tancada: les veus i els instruments no s'acaben d'entendre." },
            { Brillantor, "La mescla té massa aguts", "La mescla sona tancada",
              "Sona prima i punxeguda; als auriculars cansa.", "Li falta aire i obertura: sona apagada, com amb una manta al davant." },
        };
        for (const auto& zm : zones)
        {
            const double d = difZona (zm.z);
            const bool sobra = d > 3.5, falta = d < -4.5 && zm.falta[0] != 0;
            if (! sobra && ! falta) continue;
            Incidencia i;
            i.id = std::string ("mescla:") + (sobra ? "sobra:" : "falta:") + nomZona (zm.z);
            i.deMescla = true;
            i.gravetat = (float) std::clamp (0.4 + (std::abs (d) - 3.5) / 10.0, 0.4, 0.75);
            i.confianca = Confianca::Mitjana;
            const auto c = culpables (zm.z);
            std::vector<std::string> ids;
            const auto qui = llistaCulpables (c, ids);
            i.pistes = ids;
            // les guitarres que s'acumulen
            int nGtr = 0;
            double pcGtr = 0.0;
            for (const auto& [pc, p] : c) if (segura (*p) && esGuitarra (p->ident.instrument)) { ++nGtr; pcGtr += pc; }
            if (sobra && zm.z == Fang && nGtr >= 2 && pcGtr >= 50.0)
            {
                i.titol = "Les guitarres acumulen massa mitjos baixos";
                i.queVeig = "La mescla té " + num (d) + " dB de més a " + nomZona (zm.z) + " i les guitarres en fan el " + std::to_string ((int) std::lround (pcGtr)) + " %.";
                i.solucio = "Retalla una mica cada guitarra (−2 dB a uns 300 Hz, no gaire estret): sumades, fan molt, i retallar-ne una sola molt no sona igual.";
                i.pistes.clear();
                for (const auto& [pc, p] : c) if (segura (*p) && esGuitarra (p->ident.instrument)) i.pistes.push_back (p->id);
            }
            else
            {
                i.titol = sobra ? zm.sobra : zm.falta;
                i.queVeig = "Comparada amb una mescla de pop/rock d'avui (orientativa), té " + num (std::abs (d)) + " dB " + (sobra ? "de més" : "de menys")
                          + " a " + nomZona (zm.z) + "." + (qui.empty() ? std::string() : " Hi tenen més part: " + qui + ".");
                i.solucio = sobra ? (qui.empty() ? std::string ("Revisa les pistes que hi tenen més energia.") : "Comença per " + cm (c.front().second->nom) + ": arreglar-ho a la pista sona millor que no pas al màster.")
                                  : "Busca quina pista hauria de donar-ho i dona-li una mica més (o treu el que la tapa), abans que tocar-ho al màster.";
            }
            i.perque = sobra ? zm.perqueSobra : zm.perqueFalta;
            i.on = "a tota la cançó";
            i.proposta.tipus = Proposta::Revisa;
            i.proposta.eina = "EQ Pro a les pistes";
            i.detalls = "Diferència amb la corba orientativa: " + dbs (d) + " a " + nomZona (zm.z);
            i.prioritat = prioritatDe (i.gravetat);
            v.push_back (std::move (i));
        }

        // ---- quan sona tota la banda ----
        if (! mix.bandesDb.empty())
        {
            std::vector<int> n (mix.nivellDb.size(), 0);
            int maxN = 0;
            for (size_t f = 0; f < mix.nivellDb.size(); ++f)
            {
                const double t = mix.inici + (double) f * mix.salt;
                for (const auto& p : pistes)
                {
                    if (p.analisi == nullptr || ! p.analisi->valid) continue;
                    const long g = frameA (*p.analisi, t);
                    if (g >= 0 && p.analisi->actiu[(size_t) g]) ++n[f];
                }
                maxN = std::max (maxN, n[f]);
            }
            const int dens = std::max (3, (int) std::ceil (0.75 * maxN)), poc = std::max (1, maxN / 2);
            std::vector<float> fangDens, fangPoc, presDens, presPoc;
            std::vector<double> momentsDens;
            for (size_t f = 0; f < mix.nivellDb.size(); ++f)
            {
                if (! mix.actiu[f] || n[f] == 0) continue;
                double tot = 0.0;
                for (int b = 0; b < NB; ++b) tot += ener (mix.bandesDb[f][(size_t) b]);
                const float fang = db (eZona (mix, f, zonaIni[Fang], zonaFi[Fang])) - db (tot);
                const float pres = db (eZona (mix, f, zonaIni[Presencia], zonaFi[Presencia])) - db (tot);
                if (n[f] >= dens) { fangDens.push_back (fang); presDens.push_back (pres); momentsDens.push_back (mix.inici + (double) f * mix.salt); }
                else if (n[f] <= poc) { fangPoc.push_back (fang); presPoc.push_back (pres); }
            }
            if (maxN >= 3 && fangDens.size() * mix.salt >= 4.0 && fangPoc.size() * mix.salt >= 4.0)
            {
                const double dFang = percentil (fangDens, 0.5) - percentil (fangPoc, 0.5);
                const double dPres = percentil (presDens, 0.5) - percentil (presPoc, 0.5);
                if (dFang > 3.0 || dPres < -3.0)
                {
                    Incidencia i;
                    i.id = "mescla:densitat";
                    i.deMescla = true;
                    i.gravetat = (float) std::clamp (0.45 + (std::max (dFang, -dPres) - 3.0) / 10.0, 0.45, 0.75);
                    i.confianca = Confianca::Mitjana;
                    i.titol = "Quan entra tota la banda, es perd claredat";
                    i.queVeig = std::string ("Quan sonen ") + std::to_string (dens) + " pistes o més, "
                              + (dFang > 3.0 ? "els mitjos baixos pugen " + num (dFang) + " dB" : std::string())
                              + (dFang > 3.0 && dPres < -3.0 ? " i " : "")
                              + (dPres < -3.0 ? "la presència baixa " + num (-dPres) + " dB" : std::string()) + " respecte dels trossos més buits.";
                    i.perque = "Cada instrument sol sona bé, però junts s'amunteguen a la mateixa zona i la mescla s'embolica.";
                    const auto c = culpables (Fang);
                    std::vector<std::string> ids;
                    const auto qui = llistaCulpables (c, ids);
                    i.pistes = ids;
                    i.solucio = "Fes lloc a les pistes d'acompanyament a la zona de 200–500 Hz (" + (qui.empty() ? std::string ("les que en tenen més") : qui)
                              + "), sobretot a les que no són protagonistes en aquests trossos.";
                    i.altres = { "Automatitza el volum: abaixa 1–2 dB alguns instruments d'acompanyament quan entra tothom.",
                                 "Arranjament: si dos instruments fan el mateix al mateix registre, que un toqui una octava amunt o calli en aquest tros." };
                    i.on = onPassa (momentsDens, trams);
                    i.proposta.tipus = Proposta::Revisa;
                    i.proposta.eina = "EQ Pro a les pistes d'acompanyament";
                    i.detalls = "Mitjos baixos: " + dbs (dFang) + " · presència: " + dbs (dPres) + " (molts instruments respecte de pocs)";
                    i.prioritat = prioritatDe (i.gravetat);
                    v.push_back (std::move (i));
                }
            }
        }

        // ---- la mescla en mono ----
        if (! mix.esMono && mix.perduaMonoDb > 3.0f)
        {
            Incidencia i;
            i.id = "mescla:mono";
            i.deMescla = true;
            i.gravetat = mix.perduaMonoDb > 6.0f ? 0.7f : 0.45f;
            i.confianca = Confianca::Alta;
            i.titol = "La mescla perd molt en mono";
            i.queVeig = "Sumada a mono (mòbil, altaveu petit), la mescla baixa " + num (mix.perduaMonoDb) + " dB.";
            i.perque = "Alguna cosa té la fase girada o està massa oberta: en mono, hi ha instruments que gairebé desapareixen.";
            for (const auto& p : pistes)
                if (p.analisi != nullptr && p.analisi->valid && ! p.analisi->esMono && p.analisi->perduaMonoDb > 3.0f) i.pistes.push_back (p.id);
            i.solucio = i.pistes.empty() ? "Comprova-ho amb «Escolta en mono» del mòdul Estèreo." : "Mira primer les pistes que perden més en mono (són a la llista).";
            i.proposta.tipus = Proposta::Revisa;
            i.proposta.eina = "Estèreo (Escolta en mono)";
            i.detalls = "Pèrdua en mono: " + num (mix.perduaMonoDb) + " dB · correlació " + num (mix.global.correlacio, 2);
            i.prioritat = prioritatDe (i.gravetat);
            v.push_back (std::move (i));
        }
        std::sort (v.begin(), v.end(), [] (const Incidencia& x, const Incidencia& y) { return x.gravetat > y.gravetat; });
        return v;
    }
}
