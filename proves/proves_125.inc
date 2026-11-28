// 1.25: Mix Assistant (fase 3): correccions preparades amb efectes interns, seguiment entre anàlisis i versions de mescla.
#include "mibase/Dinamica.h"
#include "mibase/Equalitzador.h"
#include "mibase/Estereo.h"

namespace
{
    /** Passa el so (mono a L i R) per un efecte intern amb aquest estat. */
    template <typename Efecte>
    std::vector<float> perEfecte (const std::vector<float>& s, const std::string& estat)
    {
        Efecte e;
        e.posaEstat (estat);
        e.prepara (48000.0, 512);
        std::vector<float> L (s), R (s);
        for (size_t k = 0; k < s.size(); k += 512)
        {
            const int n = (int) std::min<size_t> (512, s.size() - k);
            e.processa (L.data() + k, R.data() + k, n);
        }
        return L;
    }

    void proves125()
    {
        using namespace mibase;
        prova ("1.25 Mix Assistant: preparar una EQ i comprovar que el problema baixa", []
        {
            const auto s = sorollAmb ([] (double f) { return rosa (f) * (f > 220 && f < 420 ? 5.0 : 1.0) * (f > 6000 ? 0.3 : 1.0); }, 41);
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            const auto d = mx::diagnosticaPista ("t1", "Veu", a, idDe (mx::Instrument::Veu));
            const auto* i = troba (d, "fang");
            comprova (i != nullptr && mx::espotPreparar (*i), "l'excés de mitjos baixos es pot preparar");
            const auto c = mx::preparaCorreccio (*i, &a);
            comprova (c.valida() && c.plugin == eq::idPlugin && c.pista == "t1" && ! c.resum.empty(), "és una EQ Pro per a la pista");
            eq::Equalitzador e;
            e.posaEstat (c.estat);
            const auto b = e.banda (0);
            comprova (b.usada && b.tipus == eq::Bell && std::abs (b.freq - i->proposta.freq) < 1.0f && std::abs (b.guany - i->proposta.guany) < 0.01f,
                      "amb la banda que diu la proposta");
            const auto s2 = perEfecte<eq::Equalitzador> (s, c.estat);
            const auto a2 = mx::analitzaPista (s2.data(), s2.data(), s2.size(), 48000.0);
            const auto d2 = mx::diagnosticaPista ("t1", "Veu", a2, idDe (mx::Instrument::Veu));
            const auto ev = mx::compara (d.incidencies, d2.incidencies);
            const auto it = std::find_if (ev.begin(), ev.end(), [&] (const mx::Evolucio& x) { return x.id == i->id; });
            comprova (it != ev.end() && (it->estat == mx::Evolucio::Solucionat || it->estat == mx::Evolucio::Millorat),
                      "en tornar a analitzar, el problema ha millorat o ja no hi és");
        });

        prova ("1.25 Mix Assistant: passa-alts, de-esser, compressor i mono als greus", []
        {
            mx::Incidencia i;
            i.pista = "t1";
            i.proposta.tipus = mx::Proposta::PassaAlts;
            i.proposta.freq = 80.0f;
            auto c = mx::preparaCorreccio (i);
            eq::Equalitzador e;
            e.posaEstat (c.estat);
            comprova (e.banda (0).tipus == eq::HighPass && std::abs (e.banda (0).freq - 80.0f) < 0.5f, "passa-alts a 80 Hz");

            i.proposta = {};
            i.proposta.tipus = mx::Proposta::DeEsser;
            i.proposta.freq = 7000.0f;
            i.proposta.reduccioDb = 4.0f;
            c = mx::preparaCorreccio (i);
            dinamica::Dinamica dd;
            dd.posaEstat (c.estat);
            comprova (c.plugin == dinamica::idPlugin && (int) dd.valor (dinamica::ModeP) == dinamica::DeEsser
                      && dd.valor (dinamica::FreqDeEsser) > 5000.0f && dd.valor (dinamica::FreqDeEsser) < 7000.0f, "de-esser una mica per sota de les esses");

            i.proposta = {};
            i.proposta.tipus = mx::Proposta::MonoGreus;
            i.proposta.freq = 120.0f;
            c = mx::preparaCorreccio (i);
            estereo::Estereo es;
            es.posaEstat (c.estat);
            comprova (c.plugin == estereo::idPlugin && std::abs (es.valor (estereo::MonoGreus) - 120.0f) < 0.5f, "mono als greus a 120 Hz");

            for (auto t : { mx::Proposta::Fader, mx::Proposta::Panorama, mx::Proposta::Revisa, mx::Proposta::Cap })
            {
                i.proposta = {};
                i.proposta.tipus = t;
                if (mx::espotPreparar (i) || mx::preparaCorreccio (i).valida()) comprova (false, "fader, panorama i «revisa» no es preparen");
            }
            i.proposta.tipus = mx::Proposta::Campana;
            i.deMescla = true;
            comprova (! mx::espotPreparar (i), "una cosa de la mescla sencera no és d'una pista");
        });

        prova ("1.25 Mix Assistant: el compressor preparat redueix la diferència de volum", []
        {
            auto s = sorollAmb ([] (double f) { return rosa (f) * (f > 6000 ? 0.3 : 1.0); }, 42);
            for (size_t k = 0; k < s.size(); ++k) if ((k / 48000) % 2 == 1) s[k] *= 0.1f;
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            const auto d = mx::diagnosticaPista ("t1", "Veu", a, idDe (mx::Instrument::Veu), {}, true, 0.0f);
            const auto* i = troba (d, "dinamica");
            comprova (i != nullptr, "hi ha massa diferència");
            const auto c = mx::preparaCorreccio (*i, &a, 0.0f);
            dinamica::Dinamica dd;
            dd.posaEstat (c.estat);
            comprova ((int) dd.valor (dinamica::ModeP) == dinamica::Compressor && dd.valor (dinamica::Llindar) < a.nivellFortDb, "compressor amb el llindar per sota de les parts fortes");
            const auto s2 = perEfecte<dinamica::Dinamica> (s, c.estat);
            const auto a2 = mx::analitzaPista (s2.data(), s2.data(), s2.size(), 48000.0);
            comprova (a2.rangDinamicDb < a.rangDinamicDb - 2.0f, "la diferència entre fort i fluix baixa");
            // amb el fader a −6 dB, el so analitzat era 6 dB més fluix: el llindar (abans del fader) puja 6 dB
            const auto c6 = mx::preparaCorreccio (*i, &a, -6.0f);
            dinamica::Dinamica d6;
            d6.posaEstat (c6.estat);
            comprova (std::abs ((d6.valor (dinamica::Llindar) - dd.valor (dinamica::Llindar)) - 6.0f) < 0.1f, "el llindar compta el fader");
        });

        prova ("1.25 Mix Assistant: seguiment entre dues anàlisis", []
        {
            auto inc = [] (const std::string& id, float g) { mx::Incidencia i; i.id = id; i.pista = "t1"; i.titol = id; i.gravetat = g; return i; };
            const auto ev = mx::compara ({ inc ("a", 0.6f), inc ("b", 0.6f), inc ("c", 0.6f), inc ("d", 0.5f) },
                                         { inc ("b", 0.4f), inc ("c", 0.62f), inc ("d", 0.7f), inc ("e", 0.5f) });
            auto estat = [&] (const std::string& id) { for (auto& e : ev) if (e.id == id) return e.estat; return mx::Evolucio::Continua; };
            comprova (estat ("a") == mx::Evolucio::Solucionat && estat ("b") == mx::Evolucio::Millorat && estat ("c") == mx::Evolucio::Continua
                      && estat ("d") == mx::Evolucio::Empitjorat && estat ("e") == mx::Evolucio::Nou && ev.size() == 5,
                      "solucionat, millorat, continua, empitjorat i nou");
        });

        prova ("1.25 Versions de mescla: desar, recuperar i que tornin en obrir", []
        {
            Projecte p;
            auto& g = edicio::afegeixPista (p, "Guitarra", "audio");
            g.faderDb = -3.0; g.pan = -0.5;
            g.inserts.push_back ({ "i1", eq::idPlugin, "EQ Pro", true, "mibase-eq=x", {}, false });
            const auto idR = edicio::afegeixPista (p, "Reverb", "fx").id;   // (g ja no val: el vector ha crescut)
            p.pistes[0].enviaments.push_back ({ "v1", idR, -10.0, 0.0, true, false, 1 });
            const auto v = edicio::fotoMescla (p, "La meva", "28/11 12:00");
            p.pistes[0].faderDb = 2.0; p.pistes[0].pan = 0.7; p.pistes[0].inserts.clear(); p.pistes[0].enviaments.clear(); p.masterDb = -4.0;
            comprova (edicio::recuperaMescla (p, v) == 2, "recupera les pistes");
            comprova (p.pistes[0].faderDb == -3.0 && p.pistes[0].pan == -0.5 && p.pistes[0].inserts.size() == 1
                      && p.pistes[0].inserts[0].estat == "mibase-eq=x" && p.pistes[0].enviaments.size() == 1 && p.masterDb == 0.0,
                      "volum, panorama, efectes (amb ajustos), enviaments i màster");
            p.versionsMescla.push_back (v);
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_125";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            comprova (disc::desa (dir / "C.mibase", p, "2026-11-28T12:00:00", "2026-11-28 120000", false).correcte, "es desa");
            const auto l = disc::llegeix (dir / "C.mibase");
            comprova (l && l->versionsMescla.size() == 1 && l->versionsMescla[0].nom == "La meva" && l->versionsMescla[0].pistes.size() == 2
                      && l->versionsMescla[0].pistes[0].inserts.size() == 1 && l->versionsMescla[0].pistes[0].enviaments.size() == 1,
                      "les versions tornen en obrir");
            comprova (l && l->pistes[0].enviaments.size() == 1 && l->pistes[0].enviaments[0].nivellDb == -10.0, "els enviaments de les pistes, igual que abans");
            // un bus que ja no hi és: l'enviament no es recupera
            Projecte q = p;
            q.pistes.erase (q.pistes.begin() + 1);
            edicio::recuperaMescla (q, v);
            comprova (q.pistes[0].enviaments.empty(), "enviaments a busos esborrats, fora");
        });
    }
}
