// 0.62: EQ dinàmica (per banda; so propi o sidechain).
namespace
{
    namespace p062
    {
        double nivellOut (mibase::eq::Equalitzador& e, double f, double fs)
        {
            return p060::nivellA (e.anellSortida(), f, fs);
        }
        std::shared_ptr<mibase::eq::Equalitzador> eqAmb (const mibase::eq::Banda& b, double fs)
        {
            auto e = std::make_shared<mibase::eq::Equalitzador>();
            e->prepara (fs, 512);
            e->posaBanda (0, b);
            e->posaCaptura (true);
            return e;
        }
        mibase::eq::Banda dinamica (float freq, float llindar, float ratio, float rang)
        {
            mibase::eq::Banda b;
            b.usada = true; b.tipus = mibase::eq::Bell; b.freq = freq; b.guany = 0.0f; b.q = 1.5f;
            b.dinamica = true; b.llindar = llindar; b.ratio = ratio; b.rang = rang; b.atac = 5.0f; b.retorn = 80.0f;
            return b;
        }
    }

    void proves062()
    {
        using namespace mibase;

        prova ("0.62 EQ dinàmica: baixa la zona quan sona fort, i no toca el que sona fluix o fora de la banda", []
        {
            const double fs = 48000.0, f = 32.0 * fs / 4096.0 * 8.0;   // ~3 kHz, al centre d'un bin
            auto e = p062::eqAmb (p062::dinamica ((float) f, -30.0f, 4.0f, -12.0f), fs);
            auto L = p060::sinus (48000, f, fs, 0.5), R = L;                 // fort (-6 dB)
            eqp::passa (*e, L, R, 256);
            double fort = p062::nivellOut (*e, f, fs);
            std::printf ("        (fort: %.1f dB a la sortida, la dinàmica mou %.1f dB)\n", fort, e->dinamicaAra (0));
            comprova (std::abs (fort - (-6.02 - 12.0)) < 1.0, "fort: baixa els 12 dB del rang");
            comprova (e->dinamicaAra (0) < -11.0f, "es veu quant mou");

            e = p062::eqAmb (p062::dinamica ((float) f, -30.0f, 4.0f, -12.0f), fs);
            L = p060::sinus (48000, f, fs, 0.01); R = L;                     // fluix (-40 dB)
            eqp::passa (*e, L, R, 256);
            comprova (std::abs (p062::nivellOut (*e, f, fs) - (-40.0)) < 0.3, "fluix: no es toca");

            const double fBaix = 26.0 * fs / 4096.0;                          // ~300 Hz, fora de la banda
            e = p062::eqAmb (p062::dinamica ((float) f, -30.0f, 4.0f, -12.0f), fs);
            L = p060::sinus (48000, fBaix, fs, 0.5); R = L;
            eqp::passa (*e, L, R, 256);
            comprova (std::abs (p062::nivellOut (*e, fBaix, fs) + 6.02) < 0.5 && e->dinamicaAra (0) > -1.5f, "fort però fora de la zona: no es toca");

            // rang positiu: puja
            e = p062::eqAmb (p062::dinamica ((float) f, -30.0f, 4.0f, 6.0f), fs);
            L = p060::sinus (48000, f, fs, 0.25); R = L;
            eqp::passa (*e, L, R, 256);
            comprova (std::abs (p062::nivellOut (*e, f, fs) - (-12.04 + 6.0)) < 1.0, "rang positiu: puja la zona quan sona fort");
        });

        prova ("0.62 EQ dinàmica: amb el sidechain, manda el so de l'altra pista", []
        {
            const double fs = 48000.0, f = 256.0 * fs / 4096.0;
            auto b = p062::dinamica ((float) f, -30.0f, 4.0f, -10.0f);
            b.sidechain = true;
            auto e = p062::eqAmb (b, fs);
            auto L = p060::sinus (48000, f, fs, 0.05), R = L;                // la pista: fluixa (-26 dB)
            const auto sc = p060::sinus (48000, f, fs, 0.5);                  // el sidechain: fort
            for (size_t i = 0; i < L.size(); i += 256)
            {
                const int m = (int) std::min<size_t> (256, L.size() - i);
                e->posaSidechain (sc.data() + i, sc.data() + i);
                e->processa (L.data() + i, R.data() + i, m);
            }
            const double amb = p062::nivellOut (*e, f, fs);
            std::printf ("        (amb sidechain fort: %.1f dB)\n", amb);
            comprova (std::abs (amb - (20.0 * std::log10 (0.05) - 10.0)) < 1.0, "baixa encara que la pista soni fluixa");
            comprova (e->sidechainConnectat(), "sap que hi ha sidechain");
            // sense sidechain connectat, fa servir el so propi (fluix: no es toca)
            L = p060::sinus (48000, f, fs, 0.01); R = L;
            eqp::passa (*e, L, R, 256);
            comprova (std::abs (p062::nivellOut (*e, f, fs) - 20.0 * std::log10 (0.01)) < 0.5, "sense sidechain: el so propi (fluix: no es toca)");
        });

        prova ("0.62 EQ dinàmica: atac i retorn sense clics, i es desa", []
        {
            const double fs = 48000.0, f = 3000.0;
            auto e = p062::eqAmb (p062::dinamica ((float) f, -30.0f, 4.0f, -12.0f), fs);
            // fort mig segon i fluix després
            std::vector<float> L (48000), R;
            const auto s = p060::sinus (48000, f, fs, 1.0);
            for (size_t i = 0; i < L.size(); ++i) L[i] = (float) (s[i] * (i < 24000 ? 0.5 : 0.02));
            R = L;
            eqp::passa (*e, L, R, 128);
            // un sinus net de 3 kHz ja dona ~0,06 (la tercera diferència); un clic la faria pujar molt
            const double a1 = eqp::aspresa (L, 0, 24000), a2 = eqp::aspresa (L, 24100, 48000);
            std::printf ("        (aspresa: %.3f i %.3f; un sinus net: %.3f)\n", a1, a2, std::pow (2.0 * 3.14159265 * f / fs, 3.0));
            comprova (a1 < 0.09 && a2 < 0.09, "sense clics mentre es mou");
            comprova (e->dinamicaAra (0) > -0.5f, "quan torna a sonar fluix, la banda torna a lloc (retorn)");

            auto b = p062::dinamica (2500.0f, -18.0f, 6.0f, -9.0f);
            b.sidechain = true; b.atac = 2.5f; b.retorn = 300.0f;
            e->posaBanda (3, b);
            const auto st = e->estat();
            eq::Equalitzador e2;
            e2.posaEstat (st);
            comprova (e2.banda (3) == e->banda (3) && e2.banda (0) == e->banda (0), "l'estat desa la dinàmica");
            eq::Equalitzador e3;
            e3.posaEstat ("mibase-eq=1;out=0;byp=0;b2=0,1000,3,1,1,0,1");
            comprova (e3.banda (2).usada && ! e3.banda (2).dinamica, "els estats de la 0.59 es llegeixen (sense dinàmica)");
            auto hp = b; hp.tipus = eq::HighPass;
            e->posaBanda (4, hp);
            comprova (! e->banda (4).dinamica, "un passa-alts no pot ser dinàmic");
        });

        prova ("0.62 EQ dinàmica: consum", []
        {
            const double fs = 48000.0;
            auto e = std::make_shared<eq::Equalitzador>();
            e->prepara (fs, 256);
            for (int k = 0; k < eq::maxBandes; ++k)
            {
                auto b = p062::dinamica ((float) (40.0 * std::pow (2.0, k * 0.4)), -40.0f, 3.0f, k % 2 ? 6.0f : -6.0f);
                e->posaBanda (k, b);
            }
            auto L = eqp::soroll (48000 * 5, 3), R = eqp::soroll (48000 * 5, 4);
            const auto t0 = std::chrono::steady_clock::now();
            eqp::passa (*e, L, R, 256);
            const double seg = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("        (24 bandes dinàmiques: %.1f %% d'un nucli)\n", 100.0 * seg / 5.0);
            comprova (seg / 5.0 < 0.35, "consum raonable amb 24 bandes dinàmiques");
        });
    }
}
