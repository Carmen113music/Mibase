// 1.23: Mix Assistant (fase 1): identificar l'instrument, analitzar una pista i diagnosticar-la.
#include "mibase/MixAssistant.h"
#include "mibase/FFT.h"
#include <random>

namespace
{
    namespace mx = mibase::mix;

    /** Soroll amb la forma que es vulgui (guany per freqüència), fet a la freqüència: ~10,9 s a 48 kHz. */
    std::vector<float> sorollAmb (const std::function<double (double)>& guany, unsigned llavor = 1, double fs = 48000.0)
    {
        constexpr int N = 1 << 19;
        std::mt19937 rng (llavor);
        std::normal_distribution<float> nd (0.0f, 1.0f);
        std::vector<mibase::fft::C> x ((size_t) N);
        for (auto& c : x) c = { nd (rng), 0.0f };
        mibase::fft::FFT f (N);
        f.fa (x.data(), false);
        for (int k = 0; k < N; ++k)
        {
            const int kk = k <= N / 2 ? k : N - k;
            const double fr = (double) kk * fs / N;
            const float g = fr < 20.0 ? 0.0f : (float) guany (fr);
            x[(size_t) k].r *= g; x[(size_t) k].i *= g;
        }
        f.fa (x.data(), true);
        std::vector<float> s ((size_t) N);
        float pic = 0.0f;
        for (int i = 0; i < N; ++i) { s[(size_t) i] = x[(size_t) i].r; pic = std::max (pic, std::abs (s[(size_t) i])); }
        for (auto& v : s) v *= 0.4f / pic;
        return s;
    }
    double rosa (double f) { return 1.0 / std::sqrt (f); }

    bool te (const mx::DiagnosiPista& d, const std::string& tipus)
    {
        return std::any_of (d.incidencies.begin(), d.incidencies.end(), [&] (const mx::Incidencia& i) { return i.id.rfind (tipus + ":", 0) == 0; });
    }
    const mx::Incidencia* troba (const mx::DiagnosiPista& d, const std::string& tipus)
    {
        for (const auto& i : d.incidencies) if (i.id.rfind (tipus + ":", 0) == 0) return &i;
        return nullptr;
    }
    mx::Identificacio idDe (mx::Instrument i)
    {
        mx::Identificacio r;
        r.instrument = i;
        r.confianca = mx::Confianca::Alta;
        return r;
    }

    void proves123()
    {
        using namespace mibase;
        prova ("1.23 Mix Assistant: quin instrument és pel nom", []
        {
            auto i = mx::identificaPerNom ("Veu principal");
            comprova (i.instrument == mx::Instrument::Veu && i.confianca == mx::Confianca::Alta, "«Veu principal» és la veu");
            comprova (mx::identificaPerNom ("Lead Vocal").instrument == mx::Instrument::Veu, "«Lead Vocal» és la veu (el «lead» no mana)");
            comprova (mx::identificaPerNom ("Kick In").instrument == mx::Instrument::Bombo, "«Kick In» és el bombo");
            comprova (mx::identificaPerNom ("OH L").instrument == mx::Instrument::Plats, "«OH L» són els plats");
            comprova (mx::identificaPerNom ("Baix DI").instrument == mx::Instrument::Baix, "«Baix DI» és el baix");
            comprova (mx::identificaPerNom ("Guitarra acústica").instrument == mx::Instrument::GuitarraAcustica, "acústica amb accent");
            comprova (mx::identificaPerNom ("Cors 2").instrument == mx::Instrument::Cors, "«Cors 2» són cors");
            i = mx::identificaPerNom ("Gtr2");
            comprova (i.instrument == mx::Instrument::GuitarraElectrica && i.confianca == mx::Confianca::Mitjana && i.calPreguntar(),
                      "«Gtr2»: guitarra, però no se sap si és elèctrica: es pregunta");
            comprova (mx::identificaPerNom ("Guitarra dist").confianca == mx::Confianca::Alta, "«Guitarra dist» és elèctrica segur");
            i = mx::identificaPerNom ("Pista 7");
            comprova (i.instrument == mx::Instrument::Desconegut && i.calPreguntar(), "«Pista 7»: no se sap, es pregunta");
            for (int k = 1; k < mx::numInstruments; ++k)
                if (mx::instrumentDeClau (mx::clauInstrument ((mx::Instrument) k)) != (mx::Instrument) k) comprova (false, "la clau torna l'instrument");
            comprova (mx::instrumentDeClau ("res") == mx::Instrument::Desconegut, "una clau desconeguda no és res");
        });

        prova ("1.23 Mix Assistant: tipus de pista, el que tria en Roger i que es desa", []
        {
            Projecte p;
            auto& a = edicio::afegeixPista (p, "Pista 3", "midi");
            a.instrument = "sinte:subbass";
            comprova (mx::identifica (a).instrument == mx::Instrument::Baix && ! mx::identifica (a).calPreguntar(), "el SUB & BASS és un baix");
            a.instrument = "gm:0";
            comprova (mx::identifica (a).instrument == mx::Instrument::Piano, "el programa GM 0 és un piano");
            auto& k = edicio::afegeixPista (p, "Bateria", "midi");
            k.instrument = instrumentKit;
            comprova (mx::identifica (k).instrument == mx::Instrument::Bateria, "un kit és la bateria");
            auto& g = edicio::afegeixPista (p, "Pista 9", "audio");
            g.mixInstrument = "cors";
            const auto r = mx::identifica (g);
            comprova (r.instrument == mx::Instrument::Cors && r.triatPerTu && ! r.calPreguntar(), "el que tria en Roger mana i no es torna a preguntar");
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_123";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            const auto f = dir / "Canco.mibase";
            comprova (disc::desa (f, p, "2026-11-28T10:00:00", "2026-11-28 100000", false).correcte, "es desa");
            const auto l = disc::llegeix (f);
            comprova (l && l->pistes.size() == 3 && l->pistes[2].mixInstrument == "cors", "l'instrument triat torna en obrir");
            comprova (l && l->pistes[0].mixInstrument.empty(), "les altres queden buides (projectes antics igual)");
        });

        prova ("1.23 Mix Assistant: el so tot sol no dona confiança alta", []
        {
            // un «bombo»: cops greus curts
            std::vector<float> s ((size_t) 48000 * 8, 0.0f);
            for (size_t c = 0; c + 12000 < s.size(); c += 24000)
                for (int i = 0; i < 9000; ++i)
                    s[c + (size_t) i] = 0.8f * std::exp (-i / 1500.0f) * (float) std::sin (2.0 * 3.14159265 * 55.0 * i / 48000.0);
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            Projecte p;
            auto& t = edicio::afegeixPista (p, "Pista 1", "audio");
            const auto r = mx::identifica (t, &a);
            comprova (r.instrument == mx::Instrument::Bombo, "sembla un bombo");
            comprova (r.calPreguntar(), "però es pregunta");
        });

        prova ("1.23 Mix Assistant: analitza una pista", []
        {
            const auto s = sorollAmb (rosa);
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            comprova (a.valid && a.esMono, "vàlida i mono");
            comprova (a.parteActiva > 0.95f, "sona tota l'estona");
            comprova (std::abs (a.nivellDb.size() * a.salt - a.segons) < 0.2, "un valor cada finestra");
            comprova (a.bandesDb.size() == a.nivellDb.size(), "bandes al llarg del temps (per a la fase 2)");
            comprova (a.saturacions == 0, "el soroll no satura");
            comprova (a.rangDinamicDb < 4.0f, "el soroll és constant");
            const std::vector<float> zero ((size_t) 48000 * 3, 0.0f);
            const auto b = mx::analitzaPista (zero.data(), zero.data(), zero.size(), 48000.0);
            comprova (! b.valid, "el silenci no es pot analitzar");
            const auto d = mx::diagnosticaPista ("t1", "Res", b, idDe (mx::Instrument::Veu));
            comprova (d.incidencies.empty() && ! d.resum.empty(), "i ho diu");
        });

        prova ("1.23 Mix Assistant: excés de mitjos baixos a una veu", []
        {
            const auto s = sorollAmb ([] (double f) { return rosa (f) * (f > 220 && f < 420 ? 5.0 : 1.0) * (f > 6000 ? 0.3 : 1.0); });
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            const auto d = mx::diagnosticaPista ("t1", "Veu", a, idDe (mx::Instrument::Veu));
            const auto* i = troba (d, "fang");
            comprova (i != nullptr, "ho veu");
            comprova (i != nullptr && i->proposta.tipus == mx::Proposta::Campana && i->proposta.guany < 0.0f
                      && i->proposta.freq > 200.0f && i->proposta.freq < 450.0f && i->proposta.eina == "EQ Pro",
                      "proposa una retallada amb l'EQ Pro on toca");
            comprova (i != nullptr && ! i->detalls.empty() && ! i->perque.empty() && ! i->solucio.empty(), "explica per què i ho detalla");
            comprova (! te (d, "fase") && ! te (d, "saturacio"), "res de fase ni saturació");
            const auto d2 = mx::diagnosticaPista ("t1", "Veu", a, idDe (mx::Instrument::Veu));
            comprova (d2.incidencies.size() == d.incidencies.size() && d2.incidencies.front().id == d.incidencies.front().id,
                      "les incidències tenen la mateixa identitat cada vegada");
            mx::Identificacio no;
            const auto d3 = mx::diagnosticaPista ("t1", "Pista", a, no);
            comprova (! te (d3, "fang") && ! te (d3, "tall"), "si no se sap l'instrument, no es proposen correccions de to");
        });

        prova ("1.23 Mix Assistant: greus que sobren a una veu", []
        {
            const auto s = sorollAmb ([] (double f) { return rosa (f) * (f < 60 ? 8.0 : 1.0) * (f > 6000 ? 0.3 : 1.0); });
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            const auto d = mx::diagnosticaPista ("t1", "Veu", a, idDe (mx::Instrument::Veu));
            const auto* i = troba (d, "tall");
            comprova (i != nullptr && i->proposta.tipus == mx::Proposta::PassaAlts && i->proposta.freq == 80.0f, "proposa un passa-alts a 80 Hz");
        });

        prova ("1.23 Mix Assistant: fase", []
        {
            const auto s = sorollAmb (rosa, 3);
            std::vector<float> r (s.size());
            for (size_t i = 0; i < s.size(); ++i) r[i] = -0.9f * s[i];
            const auto a = mx::analitzaPista (s.data(), r.data(), s.size(), 48000.0);
            comprova (a.perduaMonoDb > 15.0f, "en mono gairebé desapareix");
            const auto d = mx::diagnosticaPista ("t2", "Gtr", a, idDe (mx::Instrument::GuitarraElectrica));
            const auto* i = troba (d, "fase");
            comprova (i != nullptr && i->prioritat == mx::Prioritat::Alta, "problema de fase important");
        });

        prova ("1.23 Mix Assistant: ressonància que hi és sempre", []
        {
            auto s = sorollAmb ([] (double f) { return rosa (f) * (f > 6000 ? 0.3 : 1.0); }, 5);
            float pic = 0.0f;
            for (size_t i = 0; i < s.size(); ++i)
            {
                s[i] += 0.06f * (float) std::sin (2.0 * 3.14159265 * 1200.0 * (double) i / 48000.0);
                pic = std::max (pic, std::abs (s[i]));
            }
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            const bool hiEs = std::any_of (a.ressonancies.begin(), a.ressonancies.end(), [] (const mx::Ressonancia& r)
                                           { return std::abs (r.freq - 1200.0f) < 25.0f && r.persistencia > 0.8f; });
            comprova (hiEs, "troba el pic de 1,2 kHz i que hi és sempre");
            const auto d = mx::diagnosticaPista ("t3", "Acústica", a, idDe (mx::Instrument::GuitarraAcustica));
            const auto* i = troba (d, "ressonancia");
            comprova (i != nullptr && std::abs (i->proposta.freq - 1200.0f) < 25.0f && i->proposta.q >= 1.5f && i->proposta.guany < 0.0f,
                      "proposa una retallada estreta a 1,2 kHz");
            const auto b = sorollAmb ([] (double f) { return rosa (f) * (f > 6000 ? 0.3 : 1.0); }, 5);
            const auto ab = mx::analitzaPista (b.data(), b.data(), b.size(), 48000.0);
            const auto db = mx::diagnosticaPista ("t3", "Acústica", ab, idDe (mx::Instrument::GuitarraAcustica));
            comprova (! te (db, "ressonancia"), "sense el pic, no n'hi ha");
        });

        prova ("1.23 Mix Assistant: esses fortes a la veu", []
        {
            const auto fosc = sorollAmb ([] (double f) { return rosa (f) * (f > 4000 ? 0.05 : 1.0); }, 7);
            const auto ess = sorollAmb ([] (double f) { return f > 6000 && f < 8000 ? 1.0 : 0.0; }, 8);
            std::vector<float> s (fosc.size());
            for (size_t i = 0; i < s.size(); ++i)
            {
                const bool esa = (i % 48000) < 6000;
                s[i] = fosc[i] + (esa ? 1.5f * ess[i] : 0.0f);
            }
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            comprova (a.sibilancia > 0.05f && a.freqSibilancia > 5500.0f && a.freqSibilancia < 8500.0f, "troba les esses i on són");
            comprova (! a.momentsSibilancia.empty(), "i en quins moments");
            const std::vector<mx::Tram> trams { { "Estrofa", 0.0, 6.0 }, { "Tornada", 6.0, 12.0 } };
            const auto d = mx::diagnosticaPista ("t1", "Veu", a, idDe (mx::Instrument::Veu), trams);
            const auto* i = troba (d, "sibilancia");
            comprova (i != nullptr && i->proposta.tipus == mx::Proposta::DeEsser && i->proposta.eina == "Dinàmica (de-esser)", "proposa el de-esser");
            comprova (i != nullptr && i->on.find ("Estrofa") != std::string::npos, "diu a quina part passa");
            const auto dg = mx::diagnosticaPista ("t1", "Plats", a, idDe (mx::Instrument::Plats));
            comprova (! te (dg, "sibilancia"), "als plats no es busquen esses");
        });

        prova ("1.23 Mix Assistant: dinàmica i diferències entre parts", []
        {
            auto s = sorollAmb ([] (double f) { return rosa (f) * (f > 6000 ? 0.3 : 1.0); }, 9);
            for (size_t i = 0; i < s.size(); ++i) if ((i / 48000) % 2 == 1) s[i] *= 0.1f;   // un segon fort, un segon fluix (−20 dB)
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            comprova (a.rangDinamicDb > 15.0f, "hi ha molta diferència");
            const auto d = mx::diagnosticaPista ("t1", "Veu", a, idDe (mx::Instrument::Veu));
            const auto* i = troba (d, "dinamica");
            comprova (i != nullptr && i->proposta.tipus == mx::Proposta::Compressor && i->proposta.ratio > 1.0f, "proposa compressió");

            auto t = sorollAmb ([] (double f) { return rosa (f) * (f > 6000 ? 0.3 : 1.0); }, 10);
            for (size_t k = 0; k < t.size() / 2; ++k) t[k] *= 0.15f;   // la primera meitat, 16 dB més fluixa
            const auto b = mx::analitzaPista (t.data(), t.data(), t.size(), 48000.0);
            const std::vector<mx::Tram> trams { { "Estrofa", 0.0, 5.4 }, { "Tornada", 5.5, 11.0 } };
            const auto e = mx::diagnosticaPista ("t1", "Gtr", b, idDe (mx::Instrument::GuitarraElectrica), trams);
            const auto* j = troba (e, "parts");
            comprova (j != nullptr && j->queVeig.find ("Tornada") != std::string::npos && j->prioritat == mx::Prioritat::Baixa,
                      "veu que la tornada puja molt (com a detall: pot ser volgut)");
        });

        prova ("1.23 Mix Assistant: gravació saturada", []
        {
            std::vector<float> s ((size_t) 48000 * 6);
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = std::clamp (0.9f * (float) std::sin (2.0 * 3.14159265 * (180.0 + (double) (i / 24000) * 37.0) * (double) i / 48000.0), -0.5f, 0.5f);
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            comprova (a.saturacions > 20 && ! a.momentsSaturacio.empty(), "veu els trossos plans");
            const auto d = mx::diagnosticaPista ("t1", "Gtr", a, idDe (mx::Instrument::GuitarraElectrica));
            comprova (te (d, "saturacio") && troba (d, "saturacio")->prioritat == mx::Prioritat::Alta, "a una gravació, és important");
            const auto e = mx::diagnosticaPista ("t1", "Sinte", a, idDe (mx::Instrument::Sinte), {}, false);
            comprova (! te (e, "saturacio"), "a un sinte (no gravat) no es diu");
        });
    }
}
