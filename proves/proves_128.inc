// 1.28: Mix Assistant: el que s'aplica ha de fer marxar el problema (o es pot reforçar); els pads no tenen «ressonàncies».
namespace
{
    /** Aplica (encesa) totes les correccions d'EQ preparables i torna el so passat per l'EQ del Mix Assistant. */
    std::vector<float> aplicaTot (const std::vector<float>& s, const mx::DiagnosiPista& d, const mx::AnalisiPista& a, mibase::Projecte& p)
    {
        for (const auto& i : d.incidencies) if (mx::espotPreparar (i)) mx::posaCorreccio (p, 0, mx::preparaCorreccio (i, &a), true);
        for (const auto& x : p.pistes[0].inserts) if (x.plugin == mibase::eq::idPlugin) return perEfecte<mibase::eq::Equalitzador> (s, x.estat);
        return s;
    }

    void proves128()
    {
        using namespace mibase;
        prova ("1.28 Mix Assistant: un pad (instrument virtual) no té ressonàncies", []
        {
            auto s = sorollAmb ([] (double f) { return rosa (f) * (f > 6000 ? 0.3 : 1.0); }, 61);
            for (size_t i = 0; i < s.size(); ++i)   // un acord mantingut (les notes sempre hi són)
                for (double f : { 523.25, 659.26, 783.99 }) s[i] += 0.05f * (float) std::sin (2.0 * 3.14159265 * f * (double) i / 48000.0);
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            comprova (! a.ressonancies.empty(), "l'anàlisi hi veu pics fixos (les notes)");
            comprova (! te (mx::diagnosticaPista ("t1", "Pad", a, idDe (mx::Instrument::Pad), {}, false), "ressonancia"), "però a un pad no són ressonàncies");
            comprova (! te (mx::diagnosticaPista ("t1", "Sinte", a, idDe (mx::Instrument::GuitarraElectrica), {}, false), "ressonancia"),
                      "i a un instrument virtual (sense sala ni micro), tampoc");
        });

        prova ("1.28 Mix Assistant: el que s'aplica fa marxar el problema (i si no, es reforça)", []
        {
            auto s = sorollAmb ([] (double f) { return rosa (f) * (f < 60 ? 3.0 : 1.0) * (f > 6000 ? 0.3 : 1.0); }, 62);
            for (size_t i = 0; i < s.size(); ++i) s[i] += 0.012f * (float) std::sin (2.0 * 3.14159265 * 1300.0 * (double) i / 48000.0);
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            const auto d = mx::diagnosticaPista ("t1", "Gtr", a, idDe (mx::Instrument::GuitarraAcustica));
            comprova (te (d, "ressonancia") && te (d, "tall"), "una ressonància i greus que sobren");
            Projecte p;
            edicio::afegeixPista (p, "Gtr", "audio");
            const auto s2 = aplicaTot (s, d, a, p);
            const auto a2 = mx::analitzaPista (s2.data(), s2.data(), s2.size(), 48000.0);
            const auto d2 = mx::diagnosticaPista ("t1", "Gtr", a2, idDe (mx::Instrument::GuitarraAcustica));
            comprova (! te (d2, "ressonancia") && ! te (d2, "tall"), "amb les correccions aplicades, ja no hi són");

            // un pic enorme: una correcció no n'hi ha prou, però reforçant-la sí
            auto r = sorollAmb ([] (double f) { return rosa (f) * (f > 6000 ? 0.3 : 1.0); }, 63);
            for (size_t i = 0; i < r.size(); ++i) r[i] += 0.06f * (float) std::sin (2.0 * 3.14159265 * 1300.0 * (double) i / 48000.0);
            auto b = mx::analitzaPista (r.data(), r.data(), r.size(), 48000.0);
            auto db = mx::diagnosticaPista ("t1", "Gtr", b, idDe (mx::Instrument::GuitarraAcustica));
            const auto* i1 = troba (db, "ressonancia");
            comprova (i1 != nullptr, "un pic molt gran");
            Projecte q;
            edicio::afegeixPista (q, "Gtr", "audio");
            const auto lloc = mx::posaCorreccio (q, 0, mx::preparaCorreccio (*i1, &b), true);
            int rondes = 0;
            for (; rondes < 4; ++rondes)
            {
                const auto r2 = perEfecte<eq::Equalitzador> (r, q.pistes[0].inserts[0].estat);
                b = mx::analitzaPista (r2.data(), r2.data(), r2.size(), 48000.0);
                db = mx::diagnosticaPista ("t1", "Gtr", b, idDe (mx::Instrument::GuitarraAcustica));
                const auto* i = troba (db, "ressonancia");
                if (i == nullptr) break;
                comprova (i->id == i1->id, "és el mateix problema (mateixa identitat)");
                mx::reforcaCorreccio (q, lloc, mx::preparaCorreccio (*i, &b));
            }
            comprova (rondes <= 2, "en una o dues passades de «Reforça-la», ja no hi és");
            comprova (q.pistes[0].inserts.size() == 1, "sempre a la mateixa banda del mateix EQ");
            // si el que falta és a una altra freqüència, una banda nova al mateix EQ
            Projecte z;
            edicio::afegeixPista (z, "Gtr", "audio");
            const auto l2 = mx::posaCorreccio (z, 0, correccioEq (1600.0f, -5.0f), true);
            int nova = -1;
            comprova (mx::reforcaCorreccio (z, l2, correccioEq (4000.0f, -3.0f), &nova) && nova >= 0 && nova != l2.banda
                      && bandesUsades (z.pistes[0].inserts[0].estat) == 2, "a 4 kHz: banda nova (no es toca la d'1,6 kHz)");
            int nova2 = -1;
            mx::reforcaCorreccio (z, l2, correccioEq (1700.0f, -2.0f), &nova2);
            eq::Equalitzador e;
            e.posaEstat (z.pistes[0].inserts[0].estat);
            comprova (nova2 == -1 && std::abs (e.banda (l2.banda).guany + 7.0f) < 0.01f, "a prop: la mateixa banda, més retallada (−5 −2 = −7 dB)");
        });
    }
}
