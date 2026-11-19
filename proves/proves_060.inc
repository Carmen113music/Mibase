// 0.60: analitzador d'espectre i escolta d'una banda de l'EQ Pro.
namespace
{
    namespace p060
    {
        std::vector<float> sinus (size_t n, double f, double fs, double amp, double fase = 0.0)
        {
            std::vector<float> v (n);
            for (size_t i = 0; i < n; ++i) v[i] = (float) (amp * std::sin (2.0 * 3.14159265358979323846 * f * (double) i / fs + fase));
            return v;
        }

        /** Nivell (dB) a la freqüència f de les últimes mostres d'un anell. */
        double nivellA (const mibase::espectre::Anell& a, double f, double fs)
        {
            mibase::espectre::Analitzador an (4096);
            std::vector<float> x (4096);
            a.llegeix (x.data(), 4096);
            const auto& db = an.analitza (x.data());
            const int k = (int) std::lround (f / (fs / 4096.0));
            return std::max ({ db[(size_t) k - 1], db[(size_t) k], db[(size_t) k + 1] });
        }
    }

    void proves060()
    {
        using namespace mibase;

        prova ("0.60 Espectre: un sinus a escala plena dona 0 dB; a -20 dB, -20 dB", []
        {
            const double fs = 48000.0;
            espectre::Analitzador an (4096);
            const double fBin = 85.0 * fs / 4096.0;   // just al centre d'un bin
            auto x = p060::sinus (4096, fBin, fs, 1.0);
            auto db = an.analitza (x.data());
            comprova (std::abs (db[85] - 0.0f) < 0.05f, "0 dB al bin del sinus");
            x = p060::sinus (4096, fBin, fs, 0.1);
            db = an.analitza (x.data());
            comprova (std::abs (db[85] + 20.0f) < 0.05f, "-20 dB al bin del sinus");
            comprova (db[300] < -80.0f, "lluny del sinus no hi ha res");
            // entre dos bins: el pitjor cas de la finestra de Hann és -1,42 dB
            x = p060::sinus (4096, fBin + fs / 4096.0 * 0.5, fs, 1.0);
            db = an.analitza (x.data());
            comprova (std::max (db[85], db[86]) > -1.5f, "entre bins, com a molt -1,5 dB");
            comprova (espectre::midaPer (44100.0) == 4096 && espectre::midaPer (96000.0) == 8192, "mida segons la freqüència de mostreig");
        });

        prova ("0.60 Espectre: columnes, inclinació, suavitzat i pics", []
        {
            const double fs = 48000.0;
            espectre::Analitzador an (4096);
            auto x = p060::sinus (4096, 1000.0, fs, 0.5);
            const auto& db = an.analitza (x.data());
            std::vector<float> freqs;
            for (int c = 0; c < 600; ++c) freqs.push_back ((float) (20.0 * std::pow (1000.0, c / 599.0)));
            std::vector<float> col;
            espectre::aColumnes (db, fs, freqs, col);
            comprova (col.size() == freqs.size(), "una dada per columna");
            const int pic = (int) (std::max_element (col.begin(), col.end()) - col.begin());
            comprova (std::abs (freqs[(size_t) pic] - 1000.0f) < 40.0f, "el pic surt a la columna d'1 kHz");
            comprova (col[(size_t) pic] > -8.0f && col[(size_t) pic] < -5.0f, "amb el seu nivell (-6 dB)");
            comprova (std::abs (espectre::inclinacio (2000.0f, 4.5f) - 4.5f) < 1.0e-4f && std::abs (espectre::inclinacio (500.0f, 4.5f) + 4.5f) < 1.0e-4f,
                      "inclinació de 4,5 dB per octava (0 a 1 kHz)");
            const int trobat = espectre::picProper (col, freqs, 1300.0f, 0.5f);
            comprova (trobat == pic, "agafa el pic més proper");
            comprova (espectre::picProper (col, freqs, 100.0f, 0.3f) < 0 || col[(size_t) espectre::picProper (col, freqs, 100.0f, 0.3f)] < -60.0f,
                      "on no hi ha so, no hi ha cap pic de veritat");

            std::vector<float> m { 0.0f, -10.0f }, nou { -60.0f, -60.0f };
            espectre::suavitza (m, nou, espectre::Pic, espectre::Normal, 0.5);
            comprova (std::abs (m[0] + espectre::caigudaPic (espectre::Normal) * 0.5f) < 1.0e-3f, "el pic baixa a poc a poc");
            nou = { 3.0f, 3.0f };
            espectre::suavitza (m, nou, espectre::Pic, espectre::Normal, 0.03);
            comprova (m[0] == 3.0f, "i puja de cop");
            std::vector<float> a { -60.0f };
            for (int i = 0; i < 200; ++i) espectre::suavitza (a, std::vector<float> { -20.0f }, espectre::Mitjana, espectre::Rapida, 0.033);
            comprova (std::abs (a[0] + 20.0f) < 0.1f, "la mitjana arriba al nivell");
            std::vector<float> lent { -60.0f }, rapid { -60.0f };
            espectre::suavitza (lent, { -20.0f }, espectre::Mitjana, espectre::Lenta, 0.05);
            espectre::suavitza (rapid, { -20.0f }, espectre::Mitjana, espectre::Rapida, 0.05);
            comprova (rapid[0] > lent[0], "la ràpida s'hi acosta abans que la lenta");
        });

        prova ("0.60 Espectre: l'anell torna les últimes mostres en ordre (també donant la volta)", []
        {
            static espectre::Anell a;
            std::vector<float> x (1000);
            float k = 0.0f;
            for (int volta = 0; volta < 40; ++volta)
            {
                for (auto& v : x) v = k++;
                a.escriu (x.data(), (int) x.size());
            }
            std::vector<float> d (5000);
            a.llegeix (d.data(), 5000);
            bool ordre = true;
            for (size_t i = 0; i < d.size(); ++i) ordre = ordre && d[i] == k - 5000.0f + (float) i;
            comprova (ordre, "les 5000 últimes, en ordre");
            a.escriuUna (-1.0f);
            float u = 0.0f;
            a.llegeix (&u, 1);
            comprova (u == -1.0f, "una sola mostra");
        });

        prova ("0.60 EQ: la captura copia l'entrada i la sortida (i apagada no fa res)", []
        {
            const double fs = 48000.0, fc = 85.0 * fs / 4096.0;   // al centre d'un bin
            auto e = std::make_shared<eq::Equalitzador>();
            e->prepara (fs, 512);
            eq::Banda b; b.usada = true; b.tipus = eq::Bell; b.freq = 1000.0f; b.guany = -12.0f; b.q = 1.0f;
            e->posaBanda (0, b);
            auto L = p060::sinus (48000, fc, fs, 0.5), R = L;
            eqp::passa (*e, L, R, 256);
            std::vector<float> d (4096);
            e->anellSortida().llegeix (d.data(), 4096);
            bool zeros = true;
            for (float v : d) zeros = zeros && v == 0.0f;
            comprova (zeros, "sense captura, l'anell no es toca");

            e->posaCaptura (true);
            L = p060::sinus (48000, fc, fs, 0.5); R = L;
            eqp::passa (*e, L, R, 256);
            const double in = p060::nivellA (e->anellEntrada(), fc, fs), out = p060::nivellA (e->anellSortida(), fc, fs);
            std::printf ("        (entrada %.2f dB, sortida %.2f dB)\n", in, out);
            comprova (std::abs (in + 6.02) < 0.3, "l'entrada es veu al seu nivell");
            comprova (std::abs ((out - in) + 12.0) < 0.3, "la sortida es veu amb els -12 dB de la banda");
        });

        prova ("0.60 EQ: escoltar una banda deixa sentir només la seva zona, sense clics", []
        {
            const double fs = 48000.0;
            auto e = std::make_shared<eq::Equalitzador>();
            e->prepara (fs, 512);
            eq::Banda b; b.usada = true; b.tipus = eq::Bell; b.freq = 1000.0f; b.guany = 4.0f; b.q = 2.0f;
            e->posaBanda (0, b);
            eq::Banda ls; ls.usada = true; ls.tipus = eq::LowShelf; ls.freq = 150.0f; ls.guany = 3.0f;
            e->posaBanda (1, ls);
            const std::string abans = e->estat();
            e->posaCaptura (true);

            auto senyal = [&] (std::vector<float>& L, std::vector<float>& R)
            {
                const auto a = p060::sinus (L.size(), 100.0, fs, 0.3), c = p060::sinus (L.size(), 1000.0, fs, 0.3);
                for (size_t i = 0; i < L.size(); ++i) L[i] = R[i] = a[i] + c[i];
            };
            std::vector<float> L (48000), R (48000);
            senyal (L, R);
            e->posaSolo (0);
            comprova (e->estat() == abans, "l'escolta no es desa a l'estat");
            eqp::passa (*e, L, R, 128);
            double baix = p060::nivellA (e->anellSortida(), 100.0, fs), mig = p060::nivellA (e->anellSortida(), 1000.0, fs);
            std::printf ("        (Bell solo: 100 Hz %.1f dB, 1 kHz %.1f dB)\n", baix, mig);
            comprova (mig - baix > 20.0, "Bell: la zona d'1 kHz molt per sobre dels 100 Hz");
            comprova (std::abs (mig - (20.0 * std::log10 (0.3) + 4.0)) < 1.0, "i la banda sona amb el seu guany");
            comprova (eqp::aspresa (L, 0, L.size()) < 0.02, "sense clic en començar");

            e->posaSolo (1);
            senyal (L, R);
            eqp::passa (*e, L, R, 128);
            baix = p060::nivellA (e->anellSortida(), 100.0, fs); mig = p060::nivellA (e->anellSortida(), 1000.0, fs);
            std::printf ("        (Low Shelf solo: 100 Hz %.1f dB, 1 kHz %.1f dB)\n", baix, mig);
            comprova (baix - mig > 20.0, "Low Shelf: se sent el que hi ha per sota");

            e->posaSolo (-1);
            senyal (L, R);
            eqp::passa (*e, L, R, 128);
            baix = p060::nivellA (e->anellSortida(), 100.0, fs); mig = p060::nivellA (e->anellSortida(), 1000.0, fs);
            comprova (std::abs (baix - mig) < 6.0, "sense escolta, torna el so de sempre");
            comprova (eqp::aspresa (L, 0, L.size()) < 0.02, "sense clic en acabar");
            comprova (eq::canalOposat (eq::Mid) == eq::Side && eq::canalOposat (eq::Side) == eq::Mid && eq::canalOposat (eq::Esquerre) == eq::Dret
                      && eq::canalOposat (eq::Estereo) == eq::Estereo, "canal germà (Mid ↔ Side, L ↔ R)");
        });
    }
}
