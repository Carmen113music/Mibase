// 0.36: pista d'Acords.
namespace
{
    void proves036()
    {
        using namespace mibase::edicio;
        prova ("pista d'Acords: una, a dalt, blocs que es trepitgen bé", []
        {
            Projecte q;
            afegeixPista (q, "A", "audio");
            afegeixBus (q, "GRUP", false);
            comprova (pistaAcords (q) < 0, "al principi no n'hi ha");
            comprova (posaAcord (q, 0.0, 4.0, "G") >= 0, "posa G (crea la pista)");
            comprova (pistaAcords (q) == 0 && q.pistes.size() == 3, "la pista d'Acords va a dalt");
            comprova (creaPistaAcords (q) == 0 && q.pistes.size() == 3, "no se'n fa una altra");
            afegeixPista (q, "B", "midi");
            comprova (esAcords (q.pistes[0]) && q.pistes[2].nom == "B" && esBus (q.pistes[3]), "les pistes noves, entre els acords i els busos");
            posaAcord (q, 4.0, 8.0, "D");
            posaAcord (q, 8.0, 12.0, "Am");
            posaAcord (q, 12.0, 16.0, "G");
            comprova (acordA (q, 0.5) == "G" && acordA (q, 5.0) == "D" && acordA (q, 11.9) == "Am" && acordA (q, 16.5).empty(), "quin acord sona a cada moment");
            // un acord al mig d'un altre el parteix
            posaAcord (q, 9.0, 10.0, "C/E");
            const auto& cl = q.pistes[0].clips;
            comprova (cl.size() == 6 && acordA (q, 8.5) == "Am" && acordA (q, 9.5) == "C/E" && acordA (q, 10.5) == "Am", "parteix l'Am");
            // un acord que en tapa dos
            posaAcord (q, 3.0, 13.0, "Em");
            comprova (acordA (q, 2.0) == "G" && acordA (q, 5.0) == "Em" && acordA (q, 12.9) == "Em" && acordA (q, 13.5) == "G", "en tapa uns quants");
            comprova (posaAcord (q, 20.0, 24.0, "xyz") < 0 && acordA (q, 21.0).empty(), "un nom que no és un acord, no");
            // no es mou de dalt; ningú hi passa per sobre
            mouPista (q, 0, 2);
            comprova (esAcords (q.pistes[0]), "la pista d'Acords no es mou");
            mouPista (q, 2, 0);
            comprova (esAcords (q.pistes[0]) && q.pistes[1].nom == "B", "ningú hi passa per sobre");
            // els blocs només van a la pista d'Acords
            comprova (midi::compatible (q.pistes[0], cl[0]) && ! midi::compatible (q.pistes[1], cl[0]), "un acord no va a una pista normal");
            Clip audio;
            comprova (! midi::compatible (q.pistes[0], audio), "un clip normal no va a la pista d'Acords");
            // es desa i es torna a llegir
            namespace fs = std::filesystem;
            const auto dir = fs::temp_directory_path() / "mibase_prova_acords036";
            fs::remove_all (dir);
            fs::create_directories (dir);
            std::rotate (q.pistes.begin(), q.pistes.begin() + 1, q.pistes.end());   // (barrejat a propòsit)
            comprova (disc::desa (dir / "p.mibase", q, "2026-11-13T09:00:00", "2026-11-13 090000", false).correcte, "desa");
            const auto llegit = disc::llegeix (dir / "p.mibase");
            comprova (llegit.has_value() && esAcords (llegit->pistes[0]) && acordA (*llegit, 9.5) == "Em" && acordA (*llegit, 14.0) == "G",
                      "en obrir-lo, els acords hi són i la pista torna a dalt");
            fs::remove_all (dir);
        });
    }
}
