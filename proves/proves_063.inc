// 0.63: presets de l'EQ, Auto Gain i A/B (estat).
namespace
{
    void proves063()
    {
        using namespace mibase;

        prova ("0.63 EQ: més de 100 presets, tots vàlids i amb categoria", []
        {
            const auto& v = eq::presetsFabrica();
            std::printf ("        (%zu presets)\n", v.size());
            comprova (v.size() >= 100, "més de 100");
            std::set<std::string> noms;
            bool be = true, cats = true;
            for (const auto& p : v)
            {
                noms.insert (p.nom);
                eq::Equalitzador e;
                e.posaEstat (p.estat);
                be = be && e.estat() == p.estat && ! p.descripcio.empty();
                cats = cats && std::find (eq::categories().begin(), eq::categories().end(), p.categoria) != eq::categories().end();
                int n = 0;
                for (int k = 0; k < eq::maxBandes; ++k) if (e.banda (k).usada) ++n;
                be = be && n > 0;
            }
            comprova (noms.size() == v.size(), "sense noms repetits");
            comprova (be, "cada preset es llegeix igual que es desa i té bandes i descripció");
            comprova (cats, "totes les categories existeixen");
        });

        prova ("0.63 EQ: Auto Gain compensa la corba (i res si és plana)", []
        {
            std::vector<eq::Banda> b (1);
            comprova (std::abs (eq::guanyAuto (b)) < 1.0e-6, "sense bandes: 0 dB");
            b[0].usada = true; b[0].tipus = eq::LowShelf; b[0].freq = 1000.0f; b[0].guany = 6.0f;   // la meitat de l'espectre +6 dB
            const double g = eq::guanyAuto (b);
            std::printf ("        (shelf +6 dB a 1 kHz: Auto Gain %.2f dB)\n", g);
            comprova (g < -2.0 && g > -5.0, "un shelf de +6 dB a la meitat: uns -4 dB");
            b[0].guany = -6.0f;
            comprova (eq::guanyAuto (b) > 1.5, "i al revés, puja");
            b[0].tipus = eq::Bell; b[0].guany = 3.0f; b[0].q = 4.0f;
            comprova (std::abs (eq::guanyAuto (b)) < 0.6, "una banda estreta gairebé no canvia res");
            eq::Equalitzador e;
            e.posaAutoGain (true);
            const auto st = e.estat();
            eq::Equalitzador e2;
            e2.posaEstat (st);
            comprova (e2.autoGain() && st.find (";ag=1") != std::string::npos, "l'Auto Gain es desa");
        });
    }
}
