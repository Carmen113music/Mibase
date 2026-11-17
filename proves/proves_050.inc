// 0.50: biblioteca de patrons de bateria (150 ritmes, capes, variacions i velocitats «humanes»).
namespace
{
    void proves050()
    {
        using namespace mibase;
        namespace bt = mibase::bateria;
        prova ("0.50 patrons de bateria: 150, de 4 compassos de 16 caselles, amb símbols coneguts", []
        {
            const auto& v = bt::patronsBat();
            comprova (v.size() == 150, "n'hi ha 150");
            int dolents = 0, senseBase = 0;
            for (size_t i = 0; i < v.size(); ++i)
            {
                if (v[i].num != (int) i + 1) ++dolents;
                for (auto& [et, tira] : v[i].files)
                {
                    const std::string s = tira;
                    if (s.size() != (size_t) bt::llargPatro || s.find_first_not_of ("xXog-") != std::string::npos) ++dolents;
                }
                const auto g = bt::graella (v[i]);
                if ((bt::capesDe (g) & (bt::CapaBombo | bt::CapaCaixa)) == 0) ++senseBase;
            }
            comprova (dolents == 0, "numeració i tires bones");
            comprova (senseBase == 0, "tots tenen bombo o caixa");
            comprova (bt::patroBat (1) != nullptr && std::string (bt::patroBat (1)->nom) == "Back in Black Groove", "el primer és el Back in Black");
            comprova (bt::patroBat (151) == nullptr, "el 151 no existeix");
        });

        prova ("0.50 patrons de bateria: capes i notes a les files de la pista", []
        {
            const auto files = bt::filesInternes();
            const auto& p = *bt::patroBat (1);
            const long long tpc = 4 * ticksPerNegra;
            const auto tot = bt::notesDePatro (p, bt::Variacio::Original, bt::CapaTot, 4, tpc, files, 0);
            const auto hh = bt::notesDePatro (p, bt::Variacio::Original, bt::CapaHihat, 4, tpc, files, 0);
            const auto kk = bt::notesDePatro (p, bt::Variacio::Original, bt::CapaBombo, 4, tpc, files, 0);
            const auto sn = bt::notesDePatro (p, bt::Variacio::Original, bt::CapaCaixa, 4, tpc, files, 0);
            comprova (tot.size() == hh.size() + kk.size() + sn.size(), "tot = hihat + bombo + caixa");
            comprova (std::all_of (hh.begin(), hh.end(), [] (const Nota& n) { return n.to == 42 || n.to == 46; }), "el hihat només toca 42 i 46");
            comprova (std::count_if (hh.begin(), hh.end(), [] (const Nota& n) { return n.to == 46; }) == 1, "una sola obertura (final del c.4)");
            comprova (std::all_of (kk.begin(), kk.end(), [] (const Nota& n) { return n.to == 36; }), "el bombo és la 36");
            comprova (kk.size() == 12 && kk[0].ini == 0 && kk[1].ini == tpc / 2, "bombo: 3 per compàs, al 1 i al 3");
            comprova (bt::notesDePatro (p, bt::Variacio::Original, bt::CapaToms, 4, tpc, files, 0).empty(), "no té toms");
            // 8 compassos: el patró es repeteix
            const auto vuit = bt::notesDePatro (p, bt::Variacio::Original, bt::CapaBombo, 8, tpc, files, 0);
            comprova (vuit.size() == 24 && vuit[12].ini == 4 * tpc && vuit[12].vel == vuit[0].vel, "8 compassos = dues voltes iguals");
            // pista sense ride: el ride va al hihat (i la campana, a l'obert); res no es perd
            std::vector<FilaBateria> poques;
            for (auto& f : files) if (f.tipus == "kick" || f.tipus == "snare" || f.tipus == "hihat" || f.tipus == "hihat-obert") poques.push_back (f);
            const auto& ride = *bt::patroBat (11);
            const auto r = bt::notesDePatro (ride, bt::Variacio::Original, bt::CapaPlats, 1, tpc, poques, 0);
            comprova (r.size() == 8 && r[0].to == 46 && r[1].to == 42, "sense fila de ride: campana a l'obert, la resta al tancat");
            // toms sense files de tom: a la caixa
            const auto t = bt::notesDePatro (*bt::patroBat (149), bt::Variacio::Original, bt::CapaToms, 1, tpc, poques, 0);
            comprova (! t.empty() && std::all_of (t.begin(), t.end(), [] (const Nota& n) { return n.to == 38; }), "sense toms: van a la caixa, un sol cop per casella");
            comprova (t.size() == 8, "T2 i FT a l'uníson queden en un cop");
        });

        prova ("0.50 patrons de bateria: velocitats com les d'un bateria", []
        {
            const auto files = bt::filesInternes();
            const long long tpc = 4 * ticksPerNegra;
            auto vel = [&] (int num, bt::Variacio v, int pista, int pas) { const auto& p = *bt::patroBat (num); return bt::velocitatPatro (p, bt::graella (p, v), v, pista, pas); };
            const auto O = bt::Variacio::Original;
            // hihat a corxeres: el temps, més fort que el contratemps
            comprova (vel (1, O, bt::PHH, 0) > vel (1, O, bt::PHH, 2) + 10, "hihat: negra > contratemps");
            // hihat a semicorxeres (patró 4): 1 > & > e, a
            comprova (vel (4, O, bt::PHH, 0) > vel (4, O, bt::PHH, 2) && vel (4, O, bt::PHH, 2) > vel (4, O, bt::PHH, 1) + 8, "semicorxeres: 1 > & > e");
            // bombo: el primer temps mana; el primer d'un doble, més fluix (patró 5: X--X---X--XX----)
            comprova (vel (5, O, bt::PKK, 0) > vel (5, O, bt::PKK, 3) + 10, "bombo: el primer temps és el més fort");
            comprova (vel (5, O, bt::PKK, 10) < vel (5, O, bt::PKK, 11), "doble de bombo: fluix-fort");
            // caixa: backbeat fort, fantasmes molt fluixes (patró 12: ----X--g-g--X--g)
            comprova (vel (12, O, bt::PSN, 4) >= 108, "backbeat fort");
            comprova (vel (12, O, bt::PSN, 7) >= 36 && vel (12, O, bt::PSN, 7) <= 44, "nota fantasma entre 36 i 44");
            comprova (vel (12, bt::Variacio::Estrofa, bt::PSN, 7) <= 44, "les fantasmes no les toca l'escala de la variació");
            // redoble del final (patró 10, c.4: --X---X---X-XXXX): creix fins al final
            const int a = vel (10, O, bt::PSN, 48 + 12), b = vel (10, O, bt::PSN, 48 + 13), c = vel (10, O, bt::PSN, 48 + 15);
            comprova (a < b + 6 && a < c - 20 && c >= 112, "redoble: comença fluix i acaba fort");
            // un ostinato de goliat a semicorxeres (130) no és un redoble: no creix sense parar, mana l'accent
            comprova (vel (130, O, bt::PFT, 0) > vel (130, O, bt::PFT, 1) + 6 && std::abs (vel (130, O, bt::PFT, 0) - vel (130, O, bt::PFT, 48)) <= 8, "ostinato de goliat: accents, no crescendo");
            comprova (vel (130, O, bt::PSN, 60) + 20 < vel (130, O, bt::PSN, 63), "i el redoble de caixa del final sí que creix");
            // no és una màquina: compassos iguals, velocitats una mica diferents; però sempre el mateix resultat
            int diferents = 0;
            for (int s = 0; s < 16; s += 2) if (vel (1, O, bt::PHH, s) != vel (1, O, bt::PHH, 16 + s)) ++diferents;
            comprova (diferents >= 3, "el segon compàs no és calcat al primer");
            const auto& p = *bt::patroBat (1);
            comprova (bt::notesDePatro (p, O, bt::CapaTot, 4, tpc, files, 0) == bt::notesDePatro (p, O, bt::CapaTot, 4, tpc, files, 0), "dues vegades, el mateix");
            // tot dins de 1..127 a tots els patrons i variacions
            int fora = 0;
            for (auto& q : bt::patronsBat())
                for (int v = 0; v < bt::numVariacions; ++v)
                    for (auto& n : bt::notesDePatro (q, (bt::Variacio) v, bt::CapaTot, 4, tpc, files, 0))
                        if (n.vel < 1 || n.vel > 127 || n.ini < 0 || n.ini >= 4 * tpc) ++fora;
            comprova (fora == 0, "velocitats i temps dins de marges a les 900 combinacions");
        });

        prova ("0.50 patrons de bateria: variacions per a estrofa, pre-tornada, tornada i pont", []
        {
            const auto& p = *bt::patroBat (1);
            const auto o = bt::graella (p);
            auto compas = [] (const std::string& s, int b) { return s.substr ((size_t) (b * 16), 16); };
            // sense tall: el quart compàs és el segon
            const auto st = bt::graella (p, bt::Variacio::SenseTall);
            comprova (compas (st.p[bt::PSN], 3) == compas (o.p[bt::PSN], 1) && compas (st.p[bt::PHH], 3) == compas (o.p[bt::PHH], 1), "sense tall: c.4 = c.2");
            comprova (compas (st.p[bt::PSN], 0) == compas (o.p[bt::PSN], 0), "la resta no canvia");
            // estrofa: sense crash, hihat tancat (només l'obertura del final)
            const auto& teen = *bt::patroBat (3);   // hihat obert constant
            const auto es = bt::graella (teen, bt::Variacio::Estrofa);
            comprova (std::count (es.p[bt::PHH].begin(), es.p[bt::PHH].end(), 'o') == 1 && es.p[bt::PHH][62] == 'o', "estrofa: hihat tancat i una obertura al final");
            comprova (es.p[bt::PCR].find_first_not_of ('-') == std::string::npos, "estrofa: sense crash");
            const auto er = bt::graella (*bt::patroBat (11), bt::Variacio::Estrofa);   // patró de ride
            comprova (er.p[bt::PRD].find_first_not_of ('-') == std::string::npos && er.p[bt::PHH][0] == 'x', "estrofa: el ride baixa al hihat");
            // tornada: ride amb campana a les negres, crash i bombo a l'entrada dels compassos 1 i 3
            const auto to = bt::graella (p, bt::Variacio::Tornada);
            comprova (to.p[bt::PCR][0] == 'X' && to.p[bt::PCR][32] == 'X' && to.p[bt::PKK][32] == 'X', "tornada: crash i bombo al c.1 i al c.3");
            comprova (to.p[bt::PRD][4] == 'X' && to.p[bt::PRD][2] == 'x' && to.p[bt::PRD][0] == '-' && to.p[bt::PHH][2] == '-', "tornada: el hihat passa al ride");
            comprova (to.p[bt::PHH][60] == 'o', "tornada: l'obertura del tall es queda");
            comprova (to.p[bt::PSN] == o.p[bt::PSN], "tornada: la caixa no es toca");
            // pre-tornada: caixa a les negres al c.3, redoble al c.4 que creix
            const auto pr = bt::graella (p, bt::Variacio::PreTornada);
            comprova (compas (pr.p[bt::PSN], 2) == "x---X---x---X---", "pre-tornada: caixa a totes les negres al c.3");
            comprova (compas (pr.p[bt::PSN], 3) == "x-x-x-x-xxxxxxxX" && compas (pr.p[bt::PKK], 3) == "X---X---X---X---", "pre-tornada: redoble i bombo a negres al c.4");
            comprova (bt::velocitatPatro (p, pr, bt::Variacio::PreTornada, bt::PSN, 48) + 30 < bt::velocitatPatro (p, pr, bt::Variacio::PreTornada, bt::PSN, 63), "el redoble puja");
            // pont: mig temps (caixa al 3) i el tall de l'original al final
            const auto po = bt::graella (p, bt::Variacio::Pont);
            comprova (compas (po.p[bt::PSN], 0) == "--------X-------", "pont: caixa només al tercer temps");
            comprova (po.p[bt::PSN].substr (56) == o.p[bt::PSN].substr (56) && po.p[bt::PHH].substr (56) == o.p[bt::PHH].substr (56), "pont: acaba amb el tall de l'original");
            // un patró que ja és a mig temps (9): el pont l'espaia encara més
            const auto& mig = *bt::patroBat (9);
            const auto pm = bt::graella (mig, bt::Variacio::Pont);
            comprova (compas (pm.p[bt::PHH], 0) == "x---x---x---x---" && compas (pm.p[bt::PSN], 0) == compas (bt::graella (mig).p[bt::PSN], 0), "ja a mig temps: plats a negres, caixa igual");
            // cada variació és diferent de l'original a (gairebé) tots els patrons
            int iguals = 0;
            for (auto& q : bt::patronsBat())
                for (int v = 1; v < bt::numVariacions; ++v)
                    if (bt::graella (q, (bt::Variacio) v) == bt::graella (q)) ++iguals;
            comprova (iguals <= 15, "les variacions canvien alguna cosa (menys als patrons que ja no tenen tall o ja porten crash)");
            std::printf ("        (variacions iguals a l'original: %d de 750)\n", iguals);
        });

        prova ("0.50 patrons de bateria: posar només unes capes respecta la resta", []
        {
            const auto files = bt::filesInternes();
            const long long tpc = 4 * ticksPerNegra;
            Clip c;
            c.midi = true;
            // primer, tot el patró 2 a 8 compassos
            const int n1 = bt::posaPatro (c, *bt::patroBat (2), bt::Variacio::Original, bt::CapaTot, files, tpc, 0, 8 * tpc);
            comprova (n1 == (int) c.notes.size() && n1 > 0, "posa el patró sencer");
            const auto bombos = std::count_if (c.notes.begin(), c.notes.end(), [] (const Nota& n) { return n.to == 36; });
            const auto caixes = std::count_if (c.notes.begin(), c.notes.end(), [] (const Nota& n) { return n.to == 38; });
            // ara, només el hihat d'un altre patró (4, semicorxeres) als compassos 5–8
            bt::posaPatro (c, *bt::patroBat (4), bt::Variacio::Original, bt::CapaHihat, files, tpc, 4 * tpc, 8 * tpc);
            comprova (std::count_if (c.notes.begin(), c.notes.end(), [] (const Nota& n) { return n.to == 36; }) == bombos, "el bombo no es toca");
            comprova (std::count_if (c.notes.begin(), c.notes.end(), [] (const Nota& n) { return n.to == 38; }) == caixes, "la caixa no es toca");
            const auto hh1 = std::count_if (c.notes.begin(), c.notes.end(), [&] (const Nota& n) { return (n.to == 42 || n.to == 46) && n.ini < 4 * tpc; });
            const auto hh2 = std::count_if (c.notes.begin(), c.notes.end(), [&] (const Nota& n) { return (n.to == 42 || n.to == 46) && n.ini >= 4 * tpc; });
            comprova (hh1 == 32 && hh2 == 61, "hihat: corxeres als quatre primers, semicorxeres (i l'obertura) als altres");
            // una nota d'una fila que no és de cap capa (percussió) no es treu mai
            Nota perc; perc.ini = 10; perc.dur = 60; perc.to = 56; perc.vel = 90;
            c.notes.push_back (perc);
            bt::posaPatro (c, *bt::patroBat (2), bt::Variacio::Original, bt::CapaTot, files, tpc, 0, 8 * tpc);
            comprova (std::count_if (c.notes.begin(), c.notes.end(), [] (const Nota& n) { return n.to == 56; }) == 1, "la percussió es queda");
            comprova ((int) c.notes.size() == n1 + 1, "tornar a posar el patró sencer no duplica res");
            comprova (std::is_sorted (c.notes.begin(), c.notes.end(), [] (const Nota& a, const Nota& b) { return a.ini < b.ini; }), "les notes queden ordenades");
            // tros que no és múltiple de compàs: res fora del tros
            Clip d; d.midi = true;
            bt::posaPatro (d, *bt::patroBat (2), bt::Variacio::Original, bt::CapaTot, files, tpc, tpc, tpc + tpc / 2);
            comprova (! d.notes.empty() && d.notes.front().ini == tpc && d.notes.back().ini < tpc + tpc / 2, "només dins del tros demanat");
            comprova (bt::posaPatro (d, *bt::patroBat (2), bt::Variacio::Original, 0, files, tpc, 0, tpc) == 0, "sense capes, no fa res");
        });
    }
}
