// 0.97: Font d'inspiració (suggeriments, edició, notes i reproductor).
#include "mibase/FontInspiracio.h"
#include <set>
namespace
{
    /** Un instrument de mentida que anota les notes que rep (amb la mostra absoluta). */
    struct InstrProva097 : mibase::Instrument
    {
        long long mostra = 0;
        std::vector<std::pair<long long, int>> encesos, apagats;   // (mostra, nota)
        int apagaTots = 0;
        void prepara (double, int) override {}
        void processa (const mibase::EventInstrument* ev, int num, float* L, float* R, int n) noexcept override
        {
            for (int i = 0; i < num; ++i)
            {
                if ((ev[i].estat & 0xF0) == 0x90) encesos.push_back ({ mostra + ev[i].offset, ev[i].dada1 });
                else apagats.push_back ({ mostra + ev[i].offset, ev[i].dada1 });
            }
            for (int i = 0; i < n; ++i) { L[i] = 0.0f; R[i] = 0.0f; }
            mostra += n;
        }
        void apagaTot() noexcept override { ++apagaTots; }
        bool fentSo() const noexcept override { return true; }
    };

    std::set<std::string> primers097 (const std::vector<mibase::font::Suggeriment>& v, size_t n)
    {
        std::set<std::string> s;
        for (size_t i = 0; i < n && i < v.size(); ++i) s.insert (v[i].acord);
        return s;
    }

    void proves097()
    {
        using namespace mibase;
        using namespace mibase::font;
        prova ("0.97 Font d'inspiració: suggeriments en tonalitat major i menor", []
        {
            Tonalitat sol { 7, harmonia::escalaPerId ("major") };
            auto v = suggeriments (sol, {}, 0);
            comprova (primers097 (v, 6) == std::set<std::string> { "G", "D", "C", "Em", "Bm", "Am" }, "SOL major: els sis primers són SOL, RE, DO, MIm, SIm i LAm");
            comprova (v.front().acord == "G", "per començar, la tònica (" + v.front().acord + ")");
            comprova (v.size() >= 20, "n'hi ha prou per explorar (" + std::to_string (v.size()) + ")");
            comprova (v.back().acord.empty(), "el silenci, al final");
            std::set<std::string> noms; for (auto& s : v) noms.insert (s.acord);
            comprova (noms.size() == v.size(), "sense repetits");
            comprova (noms.count ("D7") && noms.count ("A7") && noms.count ("F") && noms.count ("Cm") && noms.count ("Eb"), "hi ha setenes, dominants secundàries i manllevats");

            Tonalitat la { 9, harmonia::escalaPerId ("menor") };
            comprova (esMenor (la) && ! esMenor (sol), "sap quina és menor");
            auto m = suggeriments (la, {}, 0);
            comprova (primers097 (m, 6) == std::set<std::string> { "Am", "F", "G", "Dm", "C", "E" }, "LA menor: els sis primers");
            comprova (m.front().acord == "Am", "per començar, LAm");
        });

        prova ("0.97 Font d'inspiració: els suggeriments escolten els veïns", []
        {
            Tonalitat doM { 0, harmonia::escalaPerId ("major") };
            auto v = suggeriments (doM, { { "C", 4 }, { "G", 4 } }, 2);
            comprova (v.front().acord == "C" || v.front().acord == "Am", "després de SOL (V) ve DO o LAm (" + v.front().acord + ")");
            comprova (v.front().acord != "G", "no proposa repetir el mateix acord primer");
            auto w = suggeriments (doM, { { "C", 4 }, { "E7", 4 } }, 2);
            comprova (w.front().acord == "Am", "MI7 resol a LAm (" + w.front().acord + ")");
            // al mig: entre DO i LAm, la llista canvia segons el de després
            auto x = suggeriments (doM, { { "C", 4 }, { "F", 4 }, { "Am", 4 } }, 1);
            size_t iE7 = 99; for (size_t i = 0; i < x.size(); ++i) if (x[i].acord == "E7") iE7 = i;
            size_t iBb = 99; for (size_t i = 0; i < x.size(); ++i) if (x[i].acord == "Bb") iBb = i;
            comprova (iE7 < iBb, "abans de LAm, MI7 (dominant secundària) puja per sobre de SIb");
            comprova (mesProbable (doM, { { "C", 4 } }, 1) != "C", "el més probable per afegir no és el mateix");
        });

        prova ("0.97 Font d'inspiració: edició de durades i posicions sense forats", []
        {
            Idea i;
            afegeix (i, "G", 4); afegeix (i, "Bm", 2); afegeix (i, "D", 2); afegeix (i, "Em", 8);
            comprova (inici (i, 3) == 8.0 && durada (i) == 16.0, "posicions i durada");
            comprova (mouInici (i, 1, -0.5) && i.acords[0].temps == 3.5 && i.acords[1].temps == 2.5 && inici (i, 2) == 6.0,
                      "Ctrl+←: SIm entra mig temps abans i RE no es mou");
            comprova (! mouInici (i, 0, 0.5), "el primer no es mou");
            comprova (! mouInici (i, 2, 2.5), "no deixa cap acord per sota de mig temps");
            comprova (canviaDurada (i, 1, 1.0, 4) && i.acords[1].temps == 3.5 && inici (i, 3) == 9.0, "Maj+→: allarga i desplaça els de després");
            comprova (! canviaDurada (i, 2, -5.0, 4) || i.acords[2].temps == 0.5, "mínim mig temps");
            treu (i, 1);
            comprova (i.acords.size() == 3 && i.acords[1].acord == "D", "treure n'avança els de després");
            i.instrument = Instr::Guitarra; i.patroGuitarra = 2; i.bucle = false; i.tePropia = true; i.tonalitat = { 9, 1 };
            comprova (llegeixIdea (textIdea (i)) == i, "es desa i es llegeix igual");
            comprova (llegeixIdea ("{trencat").acords.empty(), "un text trencat no fa caure res");
        });

        prova ("0.97 Font d'inspiració: notes de piano i guitarra dins de cada acord", []
        {
            Idea i;
            afegeix (i, "G", 4); afegeix (i, "D", 2); afegeix (i, "", 2); afegeix (i, "Em", 4);
            for (int ins = 0; ins < 2; ++ins)
            {
                i.instrument = ins == 0 ? Instr::Piano : Instr::Guitarra;
                for (int p = 0; p < 4; ++p)
                {
                    (ins == 0 ? i.patroPiano : i.patroGuitarra) = p;
                    const auto v = notes (i, 4, 4, 100.0);
                    comprova (! v.empty(), std::string ("sonen: ") + opcionsPatrons (i.instrument)[(size_t) p].nom);
                    bool dinsSilenci = false, fora = false;
                    for (auto& n : v)
                    {
                        if (n.ini >= 6 * 960 && n.ini < 8 * 960) dinsSilenci = true;
                        if (n.ini < 0 || n.ini >= 12 * 960) fora = true;
                    }
                    comprova (! dinsSilenci && ! fora, "res al silenci ni fora de la fila");
                }
                const auto sol = notes (i, 4, 4, 100.0, 1);
                bool zero = ! sol.empty();
                for (auto& n : sol) if (n.ini < 0 || n.ini >= 2 * 960) zero = false;
                comprova (zero, "un sol acord (RE) comença a zero i dura el seu tros");
            }
            // piano: les inversions enllacen (la mà dreta no salta més d'una quarta entre acords)
            Idea p; p.instrument = Instr::Piano; p.patroPiano = 2;
            for (auto* a : { "C", "G", "Am", "F", "C", "E7", "Am" }) afegeix (p, a, 4);
            double anterior = -1; double salt = 0;
            for (int k = 0; k < (int) p.acords.size(); ++k)
            {
                const auto v = notes (p, 4, 4, 100.0, k);
                double s = 0; int q = 0; for (auto& n : v) if (n.to >= 52) { s += n.to; ++q; }
                const double h = q > 0 ? s / q : 0;
                if (anterior >= 0) salt = std::max (salt, std::abs (h - anterior));
                anterior = h;
            }
            comprova (salt <= 5.5, "piano: la mà dreta es mou poc d'acord a acord (" + std::to_string (salt) + ")");
        });

        prova ("0.97 Font d'inspiració: el reproductor (temps exacte, bucle, canvis en marxa i aturada)", []
        {
            auto ins = std::make_shared<InstrProva097>();
            Reproductor r;
            r.prepara (48000.0, 512);
            r.posaInstrument (ins);
            std::vector<Nota> n { { 0, 480, 60, 100, 0 }, { 960, 480, 64, 100, 0 } };
            r.toca (programa (n, 1920, true, 120.0), true);   // 1920 ticks = 2 negres = 1 s
            std::vector<float> L (512), R (512);
            for (int b = 0; b < 187; ++b) r.processa (L.data(), R.data(), 512);   // ~2 s
            comprova (ins->encesos.size() == 4, "en bucle: 4 notes en 2 s (" + std::to_string (ins->encesos.size()) + ")");
            if (ins->encesos.size() >= 4)
            {
                comprova (ins->encesos[0].first == 0 && ins->encesos[1].first == 24000 && ins->encesos[2].first == 48000,
                          "a la mostra exacta (" + std::to_string (ins->encesos[1].first) + ", " + std::to_string (ins->encesos[2].first) + ")");
                comprova (ins->encesos[1].second == 64, "la segona nota és la bona");
            }
            comprova (! ins->apagats.empty() && ins->apagats[0].first == 12000, "cada nota s'apaga al seu final (" + (ins->apagats.empty() ? std::string ("cap") : std::to_string (ins->apagats[0].first)) + ")");
            // canvi en marxa: continua des d'on era (no torna a zero)
            for (int b = 0; b < 150; ++b) r.processa (L.data(), R.data(), 512);
            const double abans = r.posicio();
            r.toca (programa ({ { 0, 480, 67, 100, 0 }, { 960, 480, 71, 100, 0 } }, 1920, true, 120.0), false);
            r.processa (L.data(), R.data(), 512);
            comprova (r.posicio() > abans, "el canvi no torna a començar");
            for (int b = 0; b < 100; ++b) r.processa (L.data(), R.data(), 512);
            comprova (ins->encesos.back().second == 67 || ins->encesos.back().second == 71, "ja sona el programa nou");
            r.atura();
            r.processa (L.data(), R.data(), 512);
            for (int b = 0; b < 10; ++b) r.processa (L.data(), R.data(), 512);
            comprova (! r.sonant(), "aturat");
            // una escolta (sense bucle) s'acaba sola
            const auto nEnc = ins->encesos.size();
            r.toca (programa ({ { 0, 960, 62, 100, 0 } }, 960, false, 120.0), true);
            for (int b = 0; b < 100; ++b) r.processa (L.data(), R.data(), 512);
            comprova (ins->encesos.size() == nEnc + 1 && ! r.sonant(), "l'escolta d'un acord sona un cop i s'acaba");
        });
    }
}
