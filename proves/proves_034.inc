// 0.34: busos a baix de tot a la taula de treball, enviaments només a retorns, cap clip als busos.
namespace
{
    void proves034()
    {
        using namespace mibase::edicio;
        prova ("busos: sempre a baix, enviaments només a retorns, sense clips", []
        {
            Projecte q;
            afegeixPista (q, "A", "audio");
            const auto grup = afegeixBus (q, "GRUP", false).id;
            const auto fx = afegeixBus (q, "REVERB", true).id;
            afegeixPista (q, "B", "audio");
            comprova (q.pistes.size() == 4 && q.pistes[1].nom == "B" && esBus (q.pistes[2]) && esBus (q.pistes[3]), "la pista nova va abans dels busos");
            mouPista (q, 0, 3);
            comprova (! esBus (q.pistes[1]) && q.pistes[1].nom == "A", "una pista no baixa per sota dels busos");
            mouPista (q, 3, 0);
            comprova (esBus (q.pistes[2]) && esBus (q.pistes[3]) && q.pistes[2].nom == "REVERB", "un bus no puja per sobre de les pistes");
            comprova (afegeixEnviament (q, 0, grup) < 0, "no s'envia a un grup");
            comprova (afegeixEnviament (q, 0, fx) == 0, "sí a un retorn");
            comprova (! destiEnviament (q, 0, 0, grup), "ni canviant la destinació");
            const auto vis = filesVisibles (q);
            comprova (vis.size() == 4 && vis[2] == 1 && vis[3] == 1, "els busos es veuen a la taula de treball");
            Clip c;
            comprova (! midi::compatible (q.pistes[2], c) && midi::compatible (q.pistes[0], c), "als busos no hi van clips");
            // un fitxer amb els busos barrejats s'obre amb els busos a baix
            namespace fs = std::filesystem;
            const auto dir = fs::temp_directory_path() / "mibase_prova_busos034";
            fs::remove_all (dir);
            fs::create_directories (dir);
            std::rotate (q.pistes.begin(), q.pistes.begin() + 2, q.pistes.end());
            comprova (esBus (q.pistes[0]), "(barrejat abans de desar)");
            comprova (disc::desa (dir / "p.mibase", q, "2026-11-12T21:00:00", "2026-11-12 210000", false).correcte, "desa");
            const auto llegit = disc::llegeix (dir / "p.mibase");
            comprova (llegit.has_value() && ! esBus (llegit->pistes[0]) && ! esBus (llegit->pistes[1]) && esBus (llegit->pistes[2]) && esBus (llegit->pistes[3]),
                      "en obrir-lo, els busos a baix");
            fs::remove_all (dir);
        });

        prova ("res abans del zero (0.35)", []
        {
            Projecte q;
            afegeixPista (q, "A", "audio");
            const auto s1 = afegeixPresa (q, "Àudios/a.wav", "a", 4.0).id;
            afegeixClip (q, 0, s1, -1.5);
            afegeixClip (q, 0, s1, 6.0);
            q.pistes[0].clips[1].inici = 1.0;   // visible des de 7 s
            comprova (resAbansDelZero (q) == 1, "en mou un");
            comprova (q.pistes[0].clips[0].iniciVisible() == 0.0 && q.pistes[0].clips[1].iniciVisible() == 7.0, "el primer al zero, l'altre on era");
            comprova (resAbansDelZero (q) == 0, "la segona vegada, res");
        });
    }
}
