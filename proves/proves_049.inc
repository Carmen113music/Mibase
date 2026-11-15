// 0.49: powerchords, octaves, dit lliure (5a, 6a, 7a) i patrons de rock elèctric.
namespace
{
    void proves049()
    {
        using namespace mibase;
        prova ("0.49 formes: powerchords i octaves de cada fonamental", []
        {
            using guitarra::Forma;
            auto n = [] (const char* ac, Forma f) { return guitarra::notes (harmonia::llegeixAcord (ac), f); };
            comprova (n ("A", Forma::Power) == std::vector<int> { 45, 52 }, "A5 = La2 Mi3 (sense octava)");
            comprova (n ("Am", Forma::Power) == n ("A", Forma::Power), "Am i A donen el mateix powerchord");
            comprova (n ("E", Forma::Power) == std::vector<int> { 40, 47 }, "E5 a l'aire");
            comprova (n ("G", Forma::Power) == std::vector<int> { 43, 50 }, "G5 a la 6a corda, trast 3");
            comprova (guitarra::posicio (harmonia::llegeixAcord ("G"), Forma::Power).text() == "35xxxx", "G5 = 35xxxx");
            comprova (guitarra::posicio (harmonia::llegeixAcord ("C"), Forma::Power).text() == "x35xxx", "C5 = x35xxx");
            comprova (guitarra::posicio (harmonia::llegeixAcord ("C"), Forma::OctavesGreus).text() == "x3x5xx", "octava greu de Do = x3x5xx");
            comprova (guitarra::posicio (harmonia::llegeixAcord ("G"), Forma::OctavesAgudes).text() == "xx5x8x", "octava aguda de Sol = xx5x8x");
            comprova (guitarra::posicio (harmonia::llegeixAcord ("C"), Forma::OctavesAgudes).text() == "xxx5x8", "octava aguda de Do = xxx5x8");
            int dolents = 0;
            for (int arrel = 0; arrel < 12; ++arrel)
            {
                harmonia::AcordPad a = harmonia::llegeixAcord ("C");
                a.arrel = arrel;
                const auto p = guitarra::notes (a, Forma::Power), g = guitarra::notes (a, Forma::OctavesGreus), h = guitarra::notes (a, Forma::OctavesAgudes);
                if (p.size() != 2 || p[0] % 12 != arrel || p[1] - p[0] != 7 || p[0] < 40 || p[0] > 51) ++dolents;
                if (g.size() != 2 || g[0] != p[0] || g[1] - g[0] != 12) ++dolents;
                if (h.size() != 2 || h[0] != p[0] + 12 || h[1] - h[0] != 12) ++dolents;
            }
            comprova (dolents == 0, "les dotze fonamentals: intervals i registre bons");
            comprova (guitarra::notes (harmonia::llegeixAcord ("Am"), Forma::Oberts) == guitarra::notes (harmonia::llegeixAcord ("Am")), "oberts: com sempre");
            for (int f = 0; f < guitarra::numFormes; ++f)
                comprova (guitarra::llegeixForma (guitarra::textForma ((Forma) f)) == (Forma) f, "el text de la forma es torna a llegir");
        });

        prova ("0.49 dit lliure: D5 D6 D7 i el text", []
        {
            const auto v = guitarra::llegeixPassos ("D5 PD6> D7! U6 D X");
            comprova (v.size() == 6 && v[0].lliure == 5 && v[1].lliure == 6 && v[1].accent && v[1].tec == guitarra::Tecnica::Palm
                      && v[2].lliure == 7 && v[2].curt && v[3].lliure == 6 && v[4].lliure == 0, "es llegeix");
            comprova (guitarra::textPassos (v) == "D5 PD6> D7! U6 D X", "es torna a escriure igual: " + guitarra::textPassos (v));
            guitarra::Patro p;
            p.pasTicks = 480;
            p.passos = guitarra::llegeixPassos ("D5 D5 D6 D6 D7 D7 D6 D6");
            guitarra::Opcions o;
            const auto r = guitarra::rasgueja ({ 45, 52, 57 }, p, 3840, o);
            auto a = [&] (long long ini, int to) { for (auto& z : r) if (z.ini >= ini && z.ini < ini + 100 && z.to == to) return true; return false; };
            comprova (r.size() == 16, "dues notes per cop (fonamental i una més): " + std::to_string (r.size()));
            comprova (a (0, 45) && a (0, 52) && ! a (0, 57), "D5: La i Mi, sense l'octava");
            comprova (a (960, 54) && ! a (960, 52), "D6: La i Fa#");
            comprova (a (1920, 55), "D7: La i Sol");
            // amb un acord sense quinta a la segona corda, és un cop normal
            const auto c = guitarra::rasgueja (guitarra::notes (harmonia::llegeixAcord ("C")), p, 480, o);
            comprova (c.size() == guitarra::notes (harmonia::llegeixAcord ("C")).size(), "Do obert: el cop sencer");
        });

        prova ("0.49 patrons de rock: n'hi ha prou, porten forma i sonen", []
        {
            int rock = 0, senseNotes = 0, power = 0, oct = 0;
            for (auto& p : guitarra::patrons())
            {
                if (p.grup.rfind ("Rock elèctric", 0) != 0) continue;
                ++rock;
                if (p.forma == guitarra::Forma::Power) ++power;
                if (p.forma == guitarra::Forma::OctavesAgudes) ++oct;
                guitarra::Opcions o;
                const auto r = guitarra::rasgueja (guitarra::notes (harmonia::llegeixAcord ("A"), p.forma), p, p.pasTicks * (long long) p.passos.size(), o);
                if (r.empty()) ++senseNotes;
                for (auto& z : r) if (z.to < 40 || z.to > 76) ++senseNotes;
                if (guitarra::textPassos (guitarra::llegeixPassos (guitarra::textPassos (p.passos))) != guitarra::textPassos (p.passos)) ++senseNotes;
            }
            comprova (rock >= 50, "50 patrons de rock com a mínim: " + std::to_string (rock));
            comprova (power >= 40 && oct >= 10 && power + oct == rock, "tots porten powerchords o octaves");
            comprova (senseNotes == 0, "tots fan notes dins el registre");
            const auto* folk = guitarra::patro ("folk");
            comprova (folk != nullptr && folk->forma == guitarra::Forma::Oberts, "els de sempre continuen amb acords oberts");
        });

        prova ("0.49 blocs: la forma es desa i mana sobre les notes", []
        {
            using namespace mibase::edicio;
            Projecte q;
            q.tempo = 120.0;
            afegeixPista (q, "Guitarra", "midi");
            Clip c; c.midi = true; c.pos = 0.0; c.inici = 0.0; c.fi = 4.0; c.acordsPropis = true;
            q.pistes[0].clips.push_back (c);
            auto& cl = q.pistes[0].clips[0];
            guitarra::posaAcord (cl, { 0, 7680, "Am" });
            auto b = guitarra::blocDe (guitarra::patro ("r03"), 0, 3840, 0.0, 1, 88);
            comprova (b.forma == "power", "el bloc d'un patró de rock neix amb powerchords");
            guitarra::posaBloc (cl, b);
            auto b2 = guitarra::blocDe (guitarra::patro ("folk"), 3840, 3840, 0.0, 1, 88);
            comprova (b2.forma.empty(), "el d'un patró de sempre, amb oberts");
            guitarra::posaBloc (cl, b2);
            guitarra::actualitza (q);
            bool nomesPower = true, hiHaTercera = false;
            for (auto& z : cl.notes)
            {
                if (z.ini < 3840 && z.to != 45 && z.to != 52) nomesPower = false;
                if (z.ini >= 3840 && z.to % 12 == 0) hiHaTercera = true;
            }
            comprova (! cl.notes.empty() && nomesPower, "primer compàs: només La i Mi");
            comprova (hiHaTercera, "segon compàs: La menor obert (amb el Do)");
            // canviar la forma d'un bloc
            cl.blocs[0].forma = "oct+";
            comprova (guitarra::actualitza (q), "canviar la forma reescriu les notes");
            bool nomesOct = true;
            for (auto& z : cl.notes) if (z.ini < 3840 && z.to != 57 && z.to != 69) nomesOct = false;
            comprova (nomesOct, "ara només La3 i La4");
            // es desa
            const auto dir = std::filesystem::temp_directory_path() / "mibase_prova_049";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            comprova (disc::desa (dir / "p.mibase", q, "2026-11-15T10:00:00", "x", false).correcte, "desat");
            const auto l = disc::llegeix (dir / "p.mibase");
            comprova ((bool) l, "es torna a llegir");
            const Projecte q2 = l ? *l : Projecte();
            comprova (! q2.pistes.empty() && ! q2.pistes[0].clips.empty() && q2.pistes[0].clips[0].blocs.size() == 2
                      && q2.pistes[0].clips[0].blocs[0].forma == "oct+" && q2.pistes[0].clips[0].blocs[1].forma.empty(), "la forma es desa al projecte");
            std::filesystem::remove_all (dir);
            // rasgueigs propis
            const auto pr = guitarra::fesPropi ("Meu", "4/4", 480, guitarra::llegeixPassos ("D5 D6 PD PD"), guitarra::Forma::Power);
            const auto tornat = guitarra::llegeixPropis (guitarra::textPropis ({ pr }));
            comprova (tornat.size() == 1 && tornat[0].forma == guitarra::Forma::Power && guitarra::textPassos (tornat[0].passos) == "D5 D6 PD PD", "el rasgueig propi desa la forma i el dit lliure");
        });
    }
}
