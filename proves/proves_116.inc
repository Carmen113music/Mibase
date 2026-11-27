// 1.16: marcadors F1–F4 (es desen amb el projecte) i enganxar a la pista del requadre.
namespace
{
    void proves116()
    {
        using namespace mibase;
        using namespace mibase::edicio;

        prova ("1.16 Marcadors: es desen i es llegeixen amb el projecte", []
        {
            Projecte p;
            p.nom = "Marques";
            comprova (p.marcadors[0] < 0 && p.marcadors[3] < 0, "un projecte nou no té marcadors");
            p.marcadors[0] = 12.5;
            p.marcadors[2] = 61.25;
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_116";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            const auto f = dir / "Marques.mibase";
            comprova (disc::desa (f, p, "2026-11-27T10:00:00", "2026-11-27 100000", false).correcte, "es desa");
            const auto l = disc::llegeix (f);
            comprova (l.has_value(), "es llegeix");
            if (! l) return;
            comprova (std::abs (l->marcadors[0] - 12.5) < 1e-9 && std::abs (l->marcadors[2] - 61.25) < 1e-9, "els marcadors desats tornen");
            comprova (l->marcadors[1] < 0 && l->marcadors[3] < 0, "els no desats continuen buits");
        });

        prova ("1.16 Enganxar a la pista del requadre (com a Pro Tools)", []
        {
            Projecte p;
            afegeixPista (p, "Guitarra", "audio");
            afegeixPista (p, "Baix", "audio");
            afegeixPista (p, "Veu", "audio");
            afegeixPista (p, "Cors", "audio");
            const auto& pr = afegeixPresa (p, "Àudios/x.wav", "x", 20.0);
            const auto id = pr.id;
            afegeixClip (p, 0, id, 2.0);
            afegeixClip (p, 1, id, 2.0);
            auto r = copiaRang (p, 0, 1, 2.0, 6.0);
            comprova (r.clips.size() == 2, "es copien dues pistes");
            const auto r2 = retallAPista (r, 2);
            comprova (r2.clips.size() == 2 && r2.clips[0].first == 2 && r2.clips[1].first == 3, "van a la pista triada i la de sota");
            const auto nous = enganxaRetall (p, r2, 30.0, false);
            comprova (nous.size() == 2 && nous[0].pista == 2 && nous[1].pista == 3, "s'enganxen a Veu i Cors");
            comprova (std::abs (p.pistes[2].clips.back().iniciVisible() - 30.0) < 1e-9, "a l'inici del requadre, no al capçal");
            comprova (p.pistes[0].clips.size() == 1 && p.pistes[1].clips.size() == 1, "les pistes d'origen no canvien");
            // Massa avall: el que no cap no s'enganxa (no peta)
            const auto r3 = retallAPista (r, 3);
            const auto nous3 = enganxaRetall (p, r3, 50.0, false);
            comprova (nous3.size() == 1 && nous3[0].pista == 3, "el que surt per sota no s'enganxa");
        });
    }
}
