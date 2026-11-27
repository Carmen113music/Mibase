// 1.18: banderoles d'inici i final de la cançó.
namespace
{
    void proves118()
    {
        using namespace mibase;
        using namespace mibase::edicio;
        prova ("1.18 Inici i final de la cançó: automàtics, fixats i desats", []
        {
            Projecte p;
            afegeixPista (p, "Guitarra", "audio");
            const auto id = afegeixPresa (p, "Àudios/a.wav", "a", 10.0).id;
            afegeixClip (p, 0, id, 4.0);
            auto [a, b] = limitsCanco (p);
            comprova (std::abs (a - 4.0) < 1e-9 && std::abs (b - 14.0) < 1e-9, "automàtic: del primer clip al darrer");
            p.iniciCanco = 2.0;
            p.fiCanco = 20.0;
            std::tie (a, b) = limitsCanco (p);
            comprova (std::abs (a - 2.0) < 1e-9 && std::abs (b - 20.0) < 1e-9, "amb banderoles: el que diuen");
            p.bucle = { true, true, 1.0, 2.0 };
            std::tie (a, b) = limitsCanco (p);
            comprova (std::abs (a - 2.0) < 1e-9, "el bucle no canvia la cançó");
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_118";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            const auto f = dir / "Canco.mibase";
            comprova (disc::desa (f, p, "2026-11-27T10:00:00", "2026-11-27 100000", false).correcte, "es desa");
            const auto l = disc::llegeix (f);
            comprova (l && std::abs (l->iniciCanco - 2.0) < 1e-9 && std::abs (l->fiCanco - 20.0) < 1e-9, "les banderoles tornen");
            Projecte buit;
            std::tie (a, b) = limitsCanco (buit);
            comprova (a == 0.0 && b == 0.0, "sense res, no hi ha cançó");
        });
    }
}
