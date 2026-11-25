// 1.01: Pista de referència (escolta, volum igualat, inici, desar) i EQ matching.
namespace
{
    std::shared_ptr<mibase::rack::Referencia> ref101 (double fs, double segons, std::function<float (double t)> f, const std::string& nom = "Ref")
    {
        auto r = std::make_shared<mibase::rack::Referencia>();
        r->nom = nom; r->cami = "C:/refs/" + nom + ".wav"; r->fs = fs;
        const size_t n = (size_t) (fs * segons);
        r->L.resize (n); r->R.resize (n);
        for (size_t i = 0; i < n; ++i) r->L[i] = r->R[i] = f ((double) i / fs);
        r->analisi = mibase::diagnosi::analitza (r->L.data(), r->R.data(), n, fs);
        return r;
    }

    void proves101()
    {
        using namespace mibase;
        using namespace mibase::rack;
        prova ("1.01 Referència: s'escolta (amb canvi de freqüència de mostreig), es calla sense música i comença on toca", []
        {
            Rack r (Tipus::Master);
            r.posaEstat ({});
            r.posaReferencia (ref101 (44100, 5, [] (double t) { return (float) (0.25 * std::sin (2.0 * 3.14159265358979 * 440.0 * t)); }));
            r.posaIgualaRef (false);
            r.posaEscolta (EscoltaReferencia);
            r.prepara (48000, 480);
            auto s = soroll094 (48000, 2, -20);
            processa094 (r, s, 480);
            int creuaments = 0;
            for (size_t i = 72000; i + 1 < 96000; ++i) if ((s.L[i] < 0) != (s.L[i + 1] < 0)) ++creuaments;
            comprova (std::abs (pic (s.L, 72000, 96000) - 0.25f) < 0.01f, "sona la referència al seu volum (" + std::to_string (pic (s.L, 72000, 96000)) + ")");
            comprova (std::abs (creuaments / 2.0 / 0.5 - 440.0) < 5.0, "a 440 Hz (de 44,1 a 48 kHz): " + std::to_string (creuaments));
            auto silenci = sinus094 (48000, 3, 1000, -200);
            processa094 (r, silenci, 480);
            comprova (pic (silenci.L, 48000 * 2, 48000 * 3) < 1e-6f, "sense música, la referència calla");
            Rack q (Tipus::Master);
            q.posaEstat ({});
            q.posaReferencia (ref101 (48000, 6, [] (double t) { return t < 3.0 ? 0.1f : 0.3f; }));
            q.posaIgualaRef (false);
            q.posaIniciRef (4.0f);
            q.posaEscolta (EscoltaReferencia);
            q.prepara (48000, 480);
            auto z = soroll094 (48000, 0.5, -30);
            processa094 (q, z, 480);
            comprova (std::abs (z.L[20000] - 0.3f) < 0.01f && q.posicioRef() > 4.0f, "comença al segon 4");
        });

        prova ("1.01 Referència: volum igualat amb el màster; Original i Màster com sempre", []
        {
            Rack r (Tipus::Master);
            r.posaEstat ({});
            r.posaReferencia (ref101 (48000, 20, [] (double t) { return (float) (0.5 * std::sin (2.0 * 3.14159265358979 * 300.0 * t)); }));
            r.posaEscolta (EscoltaReferencia);
            r.prepara (48000, 480);
            auto s = soroll094 (48000, 16, -24);
            processa094 (r, s, 480);
            const float lRef = loudness::analitza (s.L.data() + 48000 * 12, s.R.data() + 48000 * 12, 48000 * 4, 48000).integrated;
            const float lMaster = r.loudnessSortida()->integrated.load();
            comprova (std::abs (lRef - lMaster) < 1.5f, "la referència sona com el màster (" + std::to_string (lRef) + " / " + std::to_string (lMaster) + ")");
            comprova (r.guanyRefDb() < -5.0f, "per fer-ho, l'abaixa (" + std::to_string (r.guanyRefDb()) + " dB)");
            r.posaOriginal (true);
            comprova (r.escolta() == EscoltaOriginal && r.original(), "posaOriginal continua funcionant");
        });

        prova ("1.01 Referències: es desen amb el rack i un preset no les esborra", []
        {
            Rack r (Tipus::Master);
            r.posaEstat ({});
            r.referencies = { { "Coldplay - Yellow", "C:/m/yellow.mp3" }, { "Izal", "C:/m/izal.wav" } };
            r.refActiva = 1;
            r.posaIniciRef (42.0f);
            Rack c (Tipus::Master);
            c.posaEstat (r.estat());
            comprova (c.referencies.size() == 2 && c.referencies[1].cami == "C:/m/izal.wav" && c.refActiva == 1 && std::abs (c.iniciRef() - 42.0f) < 0.01f, "es recuperen");
            c.posaEstat (presetsFabrica (Tipus::Master)[3].estat);
            comprova (c.referencies.size() == 2 && c.refActiva == 1, "carregar un preset les manté");
            Rack v (Tipus::Master);
            v.posaEstat ({});
            comprova (v.estat().find ("referencies") == std::string::npos, "sense referències no s'escriu res (els presets segueixen igual)");
        });

        prova ("1.01 EQ matching: acosta el màster a la referència, no es duplica i es pot treure", []
        {
            using namespace mibase::diagnosi;
            std::array<float, numBandes> z {}, brillant {};
            brillant[21] = brillant[22] = brillant[23] = 4.0f;   // el màster té +4 dB a l'octava dels 5 kHz
            auto m = mescla100 (brillant), rf = mescla100 (z);
            const auto am = analitza (m.L.data(), m.R.data(), m.L.size(), 48000), ar = analitza (rf.L.data(), rf.R.data(), rf.L.size(), 48000);
            const auto d = diferencia (am, ar);
            comprova (std::abs (d[7] + 4.0f) < 0.8f && std::abs (d[3]) < 0.8f, "diferència: -4 dB als 5 kHz (" + std::to_string (d[7]) + ")");
            Rack r (Tipus::Master);
            r.posaEstat ({});
            aplicaMatching (r, am, ar, 1.0f);
            int im = -1;
            for (int i = 0; i < r.numModuls(); ++i) if (r.modul (i)->clau == "eqmatch") im = i;
            comprova (im == 1 && r.modul (0)->clau == "eqm", "EQ matching just després de l'EQ de màster");
            auto* q = dynamic_cast<eq::Equalitzador*> (r.modul (im)->fx.get());
            comprova (std::abs (q->banda (7).guany + 4.0f) < 0.8f && std::abs (q->banda (7).freq - 5000.0f) < 1.0f && std::abs (q->banda (3).guany) < 0.8f, "campana de -4 dB als 5 kHz");
            aplicaMatching (r, ar, ar, 1.0f);   // tornat a analitzar: el màster ja s'assembla a la referència
            comprova (std::abs (q->banda (7).guany + 4.0f) < 0.8f, "tornar-lo a aplicar no el duplica");
            aplicaMatching (r, am, ar, 0.5f);
            comprova (std::abs (q->banda (7).guany + 4.0f) < 0.8f, "a mitja quantitat, amb el màster sense corregir, també -4 (la meitat de -8)");
            aplicaMatching (r, am, ar, 0.0f);
            bool hi = false; for (int i = 0; i < r.numModuls(); ++i) hi = hi || r.modul (i)->clau == "eqmatch";
            comprova (! hi, "quantitat 0: el treu");
        });

        prova ("1.01 ANALYZE amb referència: compara amb la seva corba", []
        {
            using namespace mibase::diagnosi;
            std::array<float, numBandes> fang {};
            fang[9] = fang[10] = 8.0f;
            auto m = mescla100 (fang);
            const auto a = analitza (m.L.data(), m.R.data(), m.L.size(), 48000);
            Context ctx;
            ctx.ambReferencia = true; ctx.corbaRef = a.corba(); ctx.nomRef = "Una de fosca";
            const auto inf = diagnostica (a, nullptr, {}, ctx);
            comprova (! te100 (inf, "Acumulació de greus-mitjans (fang)"), "si la referència també en té, no és un problema");
            comprova (inf.confiancaPerque.find ("Una de fosca") != std::string::npos, "diu amb quina referència ho compara");
        });
    }
}
