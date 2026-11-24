// 0.88: Mibase SUB & BASS (sinte de baixos).
namespace
{
    struct SonSB { std::vector<float> L, R; };

    // Fa sonar un sinte: «events» (mostra absoluta, estat, d1, d2) i «total» mostres a 48 kHz, en blocs de 256.
    SonSB sonaSB (mibase::subbass::SubBass& s, std::vector<std::array<int, 4>> ev, int total)
    {
        SonSB r;
        r.L.assign ((size_t) total, 0.0f);
        r.R.assign ((size_t) total, 0.0f);
        for (int a = 0; a < total; a += 256)
        {
            const int n = std::min (256, total - a);
            std::vector<mibase::EventInstrument> e;
            for (auto& x : ev)
                if (x[0] >= a && x[0] < a + n) e.push_back ({ x[0] - a, (std::uint8_t) x[1], (std::uint8_t) x[2], (std::uint8_t) x[3] });
            s.processa (e.data(), (int) e.size(), r.L.data() + a, r.R.data() + a, n);
        }
        return r;
    }
    double hzSB (const std::vector<float>& v, int de, int fins)
    {
        double mitj = 0.0;
        for (int i = de; i < fins; ++i) mitj += v[(size_t) i];
        mitj /= (fins - de);
        int primer = -1, darrer = -1, n = 0;
        for (int i = de + 1; i < fins; ++i)
            if (v[(size_t) i - 1] - mitj < 0.0 && v[(size_t) i] - mitj >= 0.0) { if (primer < 0) primer = i; darrer = i; ++n; }
        return n < 2 ? 0.0 : (n - 1) * 48000.0 / (darrer - primer);
    }
    float picSB (const std::vector<float>& v, int de, int fins)
    {
        float p = 0.0f;
        for (int i = de; i < fins; ++i) p = std::max (p, std::abs (v[(size_t) i]));
        return p;
    }
    double brillSB (const std::vector<float>& v, int de, int fins)
    {
        double d = 0.0, e = 0.0;
        for (int i = de + 1; i < fins; ++i) { const double x = v[(size_t) i] - v[(size_t) i - 1]; d += x * x; e += (double) v[(size_t) i] * v[(size_t) i]; }
        return e > 0.0 ? d / e : 0.0;
    }

    void proves088()
    {
        using namespace mibase::subbass;
        prova ("0.88 SUB & BASS: presets de fàbrica", []
        {
            const auto& v = presetsFabrica();
            comprova (v.size() >= 100, "almenys 100 presets (" + std::to_string (v.size()) + ")");
            std::set<std::string> estats, noms;
            std::map<std::string, int> perCat;
            for (const auto& p : v) { estats.insert (p.estat); noms.insert (p.nom); ++perCat[p.categoria]; }
            comprova (estats.size() == v.size(), "cada preset és un so diferent");
            comprova (noms.size() == v.size(), "cap nom repetit");
            for (const auto& c : categories()) comprova (perCat[c] >= 5, "la categoria «" + c + "» té presets");
            comprova (perCat.size() == categories().size(), "cap preset fora de les categories");
            SubBass s;
            s.posaEstat (v[20].estat);
            comprova (s.estatSo() == v[20].estat, "un preset es carrega igual que es desa");
        });

        prova ("0.88 SUB & BASS: tots els presets sonen, sense NaN i sense passar de 0 dBFS", []
        {
            int muts = 0, dolents = 0;
            for (const auto& p : presetsFabrica())
            {
                SubBass s;
                s.posaEstat (p.estat);
                s.prepara (48000.0, 256);
                auto r = sonaSB (s, { { 0, 0x90, 45, 110 }, { 14400, 0x80, 45, 0 } }, 19200);
                bool nan = false;
                for (float x : r.L) if (! std::isfinite (x)) nan = true;
                const float pic = std::max (picSB (r.L, 0, 19200), picSB (r.R, 0, 19200));
                if (pic < 0.01f) { ++muts; std::printf ("    (mut: %s)\n", p.nom.c_str()); }
                if (nan || pic > 1.0f) { ++dolents; std::printf ("    (dolent: %s pic %.2f)\n", p.nom.c_str(), pic); }
            }
            comprova (muts == 0, "cap preset mut");
            comprova (dolents == 0, "cap preset amb NaN ni per sobre de 0 dBFS");
        });

        prova ("0.88 SUB & BASS: afinació, sub, pitch bend i nota a la mostra exacta", []
        {
            SubBass s;
            s.posaEstat (presetsFabrica()[0].estat);   // Sub pur (sinus)
            s.prepara (48000.0, 256);
            auto r = sonaSB (s, { { 1000, 0x90, 45, 100 } }, 48000);
            comprova (picSB (r.L, 0, 1000) < 1e-6f, "res abans de la nota");
            comprova (picSB (r.L, 1000, 1400) > 0.01f, "la nota comença on toca");
            const double hz = hzSB (r.L, 12000, 48000);
            comprova (std::abs (hz - 110.0) < 0.5, "A2 = 110 Hz (" + std::to_string (hz) + ")");
            // només el sub (-1 octava)
            SubBass b;
            b.posa (O1Niv, 0.0f); b.posa (SubNiv, 1.0f); b.posa (SubOna, 0.0f); b.posa (FTall, 18000.0f); b.posa (FEnv, 0.0f);
            b.prepara (48000.0, 256);
            auto rb = sonaSB (b, { { 0, 0x90, 45, 100 } }, 48000);
            const double hs = hzSB (rb.L, 12000, 48000);
            comprova (std::abs (hs - 55.0) < 0.5, "el sub sona una octava avall (" + std::to_string (hs) + ")");
            // pitch bend a dalt de tot amb un rang de 2 semitons
            SubBass c;
            c.posaEstat (presetsFabrica()[0].estat);
            c.prepara (48000.0, 256);
            auto rc = sonaSB (c, { { 0, 0xE0, 127, 127 }, { 10, 0x90, 45, 100 } }, 48000);
            const double hb = hzSB (rc.L, 12000, 48000);
            comprova (std::abs (hb - 110.0 * std::pow (2.0, 2.0 / 12.0)) < 0.8, "pitch bend +2 st (" + std::to_string (hb) + ")");
        });

        prova ("0.88 SUB & BASS: el filtre i l'envolupant canvien el so", []
        {
            auto brillantor = [] (float tall)
            {
                SubBass s;
                s.posa (FTall, tall); s.posa (FEnv, 0.0f); s.posa (FKey, 0.0f); s.posa (SubNiv, 0.0f);
                s.prepara (48000.0, 256);
                auto r = sonaSB (s, { { 0, 0x90, 45, 100 } }, 24000);
                return brillSB (r.L, 6000, 24000);
            };
            comprova (brillantor (5000.0f) > brillantor (200.0f) * 3.0, "el filtre obert és molt més brillant que el tancat");
            for (int model = 0; model < 4; ++model)
            {
                SubBass s;
                s.posa (FModel, (float) model); s.posa (FRes, 1.0f); s.posa (FTall, 600.0f); s.posa (FEnv, 0.0f);
                s.prepara (48000.0, 256);
                auto r = sonaSB (s, { { 0, 0x90, 45, 100 } }, 24000);
                bool ok = true;
                for (float x : r.L) if (! std::isfinite (x)) ok = false;
                comprova (ok && picSB (r.L, 0, 24000) <= 1.0f, std::string ("filtre ") + opcions (FModel)[(size_t) model] + " estable amb ressonància al màxim");
            }
            // envolupant: s'apaga després de deixar anar la nota
            SubBass s;
            s.posa (AR, 50.0f);
            s.prepara (48000.0, 256);
            auto r = sonaSB (s, { { 0, 0x90, 45, 100 }, { 4800, 0x80, 45, 0 } }, 24000);
            comprova (picSB (r.L, 20000, 24000) < 1e-3f, "la nota s'acaba després de l'alliberament");
            comprova (! s.fentSo(), "i el sinte ja no fa so");
        });

        prova ("0.88 SUB & BASS: mono, legato amb glide i poli", []
        {
            SubBass s;
            s.posaEstat (presetsFabrica()[0].estat);
            s.posa (Mode, 1.0f); s.posa (Glide, 300.0f);
            s.prepara (48000.0, 256);
            // A2 i, lligada, A3: el to llisca
            auto r = sonaSB (s, { { 0, 0x90, 45, 100 }, { 24000, 0x90, 57, 100 }, { 24100, 0x80, 45, 0 } }, 96000);
            const double abans = hzSB (r.L, 12000, 24000), mig = hzSB (r.L, 25000, 30000), despres = hzSB (r.L, 72000, 96000);
            comprova (std::abs (abans - 110.0) < 1.0 && std::abs (despres - 220.0) < 1.5, "legato: de 110 a 220 Hz");
            comprova (mig > 115.0 && mig < 215.0, "legato: el to hi arriba a poc a poc (" + std::to_string (mig) + ")");
            SubBass p;
            p.posa (Mode, 2.0f);
            p.prepara (48000.0, 256);
            sonaSB (p, { { 0, 0x90, 45, 100 }, { 0, 0x90, 52, 100 }, { 0, 0x90, 57, 100 } }, 4800);
            comprova (p.veusActives() == 3, "poli: tres notes, tres veus");
            SubBass m;
            m.prepara (48000.0, 256);
            sonaSB (m, { { 0, 0x90, 45, 100 }, { 0, 0x90, 52, 100 }, { 0, 0x90, 57, 100 } }, 4800);
            comprova (m.veusActives() == 1, "mono: una sola veu");
            comprova (m.notaSona (57) && m.notaSona (45), "es veuen les tecles premudes");
            m.apagaTot();
            sonaSB (m, {}, 4800);
            comprova (! m.fentSo(), "apagaTot ho calla de seguida");
        });

        prova ("0.88 SUB & BASS: MIDI learn, CC i automatització", []
        {
            SubBass s;
            s.prepara (48000.0, 256);
            s.aprenCC (FRes);
            sonaSB (s, { { 0, 0xB0, 20, 127 } }, 256);
            comprova (s.ccDeParam (FRes) == 20 && s.aprenentCC() < 0, "aprèn el CC 20 per a la ressonància");
            comprova (std::abs (s.valor (FRes) - 1.0f) < 1e-4f, "i el CC ja la mou");
            comprova (s.ccDeParam (FTall) == 74, "CC 74 = tall, de fàbrica");
            SubBass c;
            c.posaEstat (s.estat());
            comprova (c.ccDeParam (FRes) == 20, "les assignacions es desen amb el so");
            c.posaEstat (presetsFabrica()[3].estat);
            comprova (c.ccDeParam (FRes) == 20, "carregar un preset no esborra les assignacions");
            s.aprenCC (FTall);
            sonaSB (s, { { 0, 0xB0, 1, 64 } }, 256);
            comprova (s.aprenentCC() == FTall, "la roda (CC 1) no es pot assignar");
            const int ix = s.indexParametre ("cut");
            comprova (ix == FTall && s.numParametres() == numParams, "paràmetres automatitzables per id");
            s.posaParametre (ix, 0.0f);
            comprova (std::abs (s.valor (FTall) - 20.0f) < 0.01f, "posaParametre (0..1) mou el valor real");
            comprova (s.textParametre (ix, 1.0f) == "18,0 kHz", "text amb unitats");
        });

        prova ("0.88 SUB & BASS: atzar, variació i previsualització", []
        {
            const auto a = aleatori (1), b = aleatori (2);
            comprova (a != b && a == aleatori (1), "l'atzar canvia amb la llavor i és repetible");
            int muts = 0;
            for (unsigned k = 0; k < 20; ++k)
            {
                SubBass s;
                s.posaEstat (aleatori (k));
                s.prepara (48000.0, 256);
                auto r = sonaSB (s, { { 0, 0x90, 40, 110 } }, 9600);
                if (picSB (r.L, 0, 9600) < 0.01f) ++muts;
            }
            comprova (muts == 0, "els sons a l'atzar sonen");
            const auto v = varia (presetsFabrica()[15].estat, 7, 0.5f);
            comprova (v != presetsFabrica()[15].estat, "variar canvia el so");
            float ona[256];
            const float pic = previsualitza (presetsFabrica()[0].estat, ona, 256);
            comprova (pic > 0.1f, "la previsualització dona una ona");
            comprova (ona[0] > -0.05f && ona[1] > ona[0] - 0.01f, "l'ona comença pujant pel zero");
        });

        prova ("0.88 SUB & BASS: els ajustos es desen amb el projecte", []
        {
            namespace fs = std::filesystem;
            const auto dir = fs::temp_directory_path() / "mibase_prova088";
            fs::remove_all (dir);
            fs::create_directories (dir);
            mibase::Projecte p;
            p.nom = "Baixos";
            mibase::Pista t;
            t.id = "t1"; t.nom = "Baix"; t.tipus = "midi"; t.instrument = idInstrument;
            SubBass s;
            s.posaEstat (presetsFabrica()[25].estat);
            s.nomPreset = presetsFabrica()[25].nom;
            s.assignaCC (21, FRes);
            t.estatInstrument = s.estat();
            p.pistes.push_back (t);
            const auto r = mibase::disc::creaProjecte (dir, p, "2026-11-23T12:00:00", "23/11/2026 12:00");
            comprova (r.correcte, "es crea el projecte");
            auto llegit = mibase::disc::llegeix (r.fitxer);
            comprova (llegit.has_value() && llegit->pistes.size() == 1 && esSubBass (llegit->pistes[0]), "es llegeix la pista del SUB & BASS");
            if (llegit.has_value() && ! llegit->pistes.empty())
            {
                SubBass c;
                c.posaEstat (llegit->pistes[0].estatInstrument);
                comprova (c.estatSo() == s.estatSo(), "el so és el mateix");
                comprova (c.nomPreset == presetsFabrica()[25].nom && c.ccDeParam (FRes) == 21, "i el nom del preset i el CC també");
            }
            fs::remove_all (dir);
        });

        prova ("0.88 SUB & BASS: consum", []
        {
            SubBass s;
            s.posaEstat (presetsFabrica()[36].estat);   // Reese ample (unison)
            s.posa (Mode, 2.0f);
            s.prepara (48000.0, 256);
            const auto t0 = std::chrono::steady_clock::now();
            sonaSB (s, { { 0, 0x90, 40, 100 }, { 0, 0x90, 47, 100 }, { 0, 0x90, 52, 100 } }, 48000);
            const double seg = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("    (1 s de tres notes amb unison de 4: %.1f ms)\n", seg * 1000.0);
            comprova (seg < 0.5, "molt per sota del temps real");
        });
    }
}
