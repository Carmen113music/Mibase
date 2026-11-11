// 0.31: favorits de la biblioteca, kits desats amb les files i esborrar notes amb els seus controladors.
namespace
{
    void proves031()
    {
        prova ("biblioteca: favorits (es desen i es combinen amb els tipus)", []
        {
            namespace fs = std::filesystem;
            const auto dir = fs::temp_directory_path() / "mibase_prova_favorits";
            fs::remove_all (dir);
            fs::create_directories (dir);
            biblioteca::Index idx;
            auto afegeix = [&] (const char* f, const char* cat, bool fav)
            {
                biblioteca::Sample s;
                s.fitxer = f; s.nom = f; s.categoria = cat; s.favorit = fav;
                idx.samples.push_back (s);
            };
            afegeix ("k1.wav", "Bombo", true);
            afegeix ("k2.wav", "Bombo", false);
            afegeix ("g1.wav", "Guitarra", true);
            afegeix ("s1.wav", "Sintetitzador", true);
            afegeix ("v1.wav", "Veu", true);
            comprova (biblioteca::desa (dir, idx), "desa l'índex");
            const auto llegit = biblioteca::llegeix (dir);
            int favs = 0;
            for (auto& s : llegit.samples) favs += s.favorit ? 1 : 0;
            comprova (llegit.samples.size() == 5 && favs == 4, "els favorits es conserven en desar i llegir");
            const auto o = biblioteca::Ordre::Nom;
            comprova (biblioteca::filtra (llegit, "", std::vector<std::string> { "Bombo" }, {}, o, false, true).size() == 1, "Bombo + Favorits");
            comprova (biblioteca::filtra (llegit, "", std::vector<std::string> { "Guitarra", "Sintetitzador" }, {}, o, false, true).size() == 2, "Guitarres + Sintes + Favorits");
            comprova (biblioteca::filtra (llegit, "", std::vector<std::string> {}, {}, o, false, true).size() == 4, "només Favorits");
            comprova (biblioteca::filtra (llegit, "", std::vector<std::string> { "Bombo" }, {}, o, false, false).size() == 2, "sense Favorits, tots els bombos");
            fs::remove_all (dir);
        });

        prova ("kits: desar un kit amb les files (notes i ajustos)", []
        {
            namespace fs = std::filesystem;
            const auto arrel = fs::temp_directory_path() / "mibase_prova_kitdesat";
            fs::remove_all (arrel);
            const auto dir = arrel / "El meu kit";
            fs::create_directories (dir);
            for (const char* f : { "Bombo.wav", "Caixa.wav" }) std::ofstream (dir / f) << "x";
            bateria::Kit k;
            k.nom = "El meu kit";
            k.carpeta = dir.string();
            k.sons = { { "Bombo.wav", "Bombo", "kick", "" }, { "Caixa.wav", "Caixa", "snare", "" } };
            FilaBateria a; a.tipus = "kick"; a.so = "Bombo.wav"; a.nom = "Bombo"; a.nota = 35; a.volumDb = -3.0; a.pan = -0.25; a.afina = 2.0; a.decai = 0.5; a.talla = 2;
            FilaBateria b; b.tipus = "snare"; b.so = "Caixa.wav"; b.nom = "Caixa"; b.nota = 40; b.mut = true;
            k.files = { b, a };   // l'ordre de la pista (la caixa primer)
            comprova (bateria::desaKit (k), "desa el kit");
            const auto llegit = bateria::llegeixKit (dir.string());
            comprova (llegit.has_value() && llegit->files.size() == 2, "llegeix les files");
            if (! llegit) return;
            const auto files = bateria::filesDeKit (*llegit, {});
            comprova (files.size() == 2 && files[0].tipus == "snare" && files[1].tipus == "kick", "mateix ordre que la pista");
            comprova (files[1].so == "El meu kit/Bombo.wav" && files[1].nota == 35 && files[1].volumDb == -3.0 && files[1].pan == -0.25
                      && files[1].afina == 2.0 && files[1].decai == 0.5 && files[1].talla == 2, "notes i ajustos iguals");
            comprova (files[0].mut && files[0].nota == 40, "el silenci i la nota de la caixa");
            fs::remove_all (arrel);
        });

        prova ("piano roll: esborrar notes treu els seus controladors", []
        {
            Clip c;
            c.midi = true;
            c.notes = { { 0, 960, 60, 100, 0 }, { 0, 960, 64, 100, 0 }, { 1920, 960, 67, 100, 0 } };
            c.events = { { 100, 0xB0, 11, 80, 0 }, { 2000, 0xB0, 1, 50, 0 }, { 2100, 0xE0, 0, 9000, 0 }, { 2200, 0xA0, 67, 30, 0 },
                         { 1500, 0xB0, 1, 10, 0 }, { 0, 0xC0, 5, 0, 0 } };
            midi::esborra (c, { 2 });   // la nota sola: se'n van la modulació, el pitch bend i l'aftertouch del seu tros
            comprova (c.notes.size() == 2 && c.events.size() == 3, "els controladors de la nota esborrada se'n van");
            midi::esborra (c, { 1 });   // una nota d'un acord: l'expressió es queda (l'altra nota encara hi sona)
            comprova (c.events.size() == 3, "si una altra nota hi sona, es queden");
            midi::esborra (c, { 0 });   // ja no hi queda cap nota: només el canvi de programa
            comprova (c.notes.empty() && c.events.size() == 1 && c.events[0].tipus == 0xC0, "sense notes, cap controlador");
        });
    }
}
