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

// 0.31.1: crossfade entre clips sobreposats: el de sota fa el fade contrari al del de dalt.
namespace
{
    template <typename F>
    void provesCrossfade (F audioConstant)
    {
        using namespace mibase::edicio;
        prova ("crossfade: el clip de sota baixa mentre el de dalt puja", [audioConstant]
        {
            Projecte q;
            afegeixPista (q, "A", "audio");
            q.clic = false;
            const auto s1 = afegeixPresa (q, "Àudios/a.wav", "a", 4.0).id;
            const auto s2 = afegeixPresa (q, "Àudios/b.wav", "b", 4.0).id;
            afegeixClip (q, 0, s1, 0.0);                 // sota: 0..4 s
            afegeixClip (q, 0, s2, 2.0).fadeIn = 1.0;    // dalt: de 2 s, fade in d'1 s
            q.pistes[0].clips[1].fadeOut = 0.5;
            q.pistes[0].clips[1].fi = 1.5;               // dalt s'acaba a 3,5 s amb fade out de 0,5 s (2,0..3,5)
            MapaAudios m;
            m[s1] = audioConstant (48000.0, 4.0, 0.5f);
            m[s2] = audioConstant (48000.0, 4.0, 0.5f);
            auto pla = construeixPla (q, m);
            Mesclador mx;
            mx.prepara (48000.0, 512);
            const int total = 48000 * 5;
            std::vector<float> L ((size_t) total), R ((size_t) total);
            for (int p = 0; p < total; p += 512)
                mx.processa (*pla, p, std::min (512, total - p), L.data() + p, R.data() + p, false, nullptr);
            float maxErr = 0.0f;
            for (int i = 1000; i < 4 * 48000 - 1000; i += 37) maxErr = std::max (maxErr, std::abs (L[(size_t) i] - 0.5f));
            comprova (maxErr < 0.002f, "la suma queda plana durant tots dos fades (error " + std::to_string (maxErr) + ")");
            comprova (std::abs (L[(size_t) (2.5 * 48000)] - 0.5f) < 0.002f, "a mig fade: 50 % i 50 %");
            comprova (pla->pistes[0].clips[0].tapes.size() == 1 && pla->pistes[0].clips[1].tapes.empty(), "només el de sota fa el contrari");
        });
    }
}
