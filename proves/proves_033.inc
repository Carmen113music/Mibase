// 0.33: busos de grup, retorns d'efectes i enviaments.
namespace
{
    struct EfecteProva033 : Efecte
    {
        float k = 1.0f;
        int crides = 0;
        void prepara (double, int) override {}
        void processa (float* L, float* R, int n) noexcept override
        {
            ++crides;
            for (int i = 0; i < n; ++i) { L[i] *= k; R[i] *= k; }
        }
    };

    struct So033 { std::vector<float> L, R; float l() const { return L[2000]; } float r() const { return R[2000]; } };

    void proves033()
    {
        using namespace mibase;
        using namespace mibase::edicio;
        auto audio = [] (float valor)
        {
            auto a = std::make_shared<AudioPresa>();
            a->fs = 48000.0; a->canals = 1; a->longitud = 48000;
            a->dades[0].assign (48000, valor);
            return a;
        };
        // Projecte amb «n» pistes d'àudio que sonen a 0,5 tot el primer segon.
        auto projecte = [] (int n, std::string& presa)
        {
            Projecte q;
            q.clic = false;
            presa = afegeixPresa (q, "Àudios/a.wav", "a", 1.0).id;
            for (int i = 0; i < n; ++i)
            {
                afegeixPista (q, "P" + std::to_string (i), "audio");
                afegeixClip (q, i, presa, 0.0);
            }
            return q;
        };
        auto sona = [&] (const Projecte& q, const std::string& presa, const ProveidorEfectes& ef = {}, bool reproduint = true, int mostres = 4096)
        {
            MapaAudios m { { presa, audio (0.5f) } };
            auto pla = construeixPla (q, m, {}, ef);
            Mesclador mx;
            mx.prepara (48000.0, 512);
            So033 s;
            s.L.assign ((size_t) mostres, 0.0f);
            s.R.assign ((size_t) mostres, 0.0f);
            for (int p = 0; p < mostres; p += 512)
                mx.processa (*pla, p, 512, s.L.data() + p, s.R.data() + p, false, nullptr, reproduint);
            return s;
        };
        auto prop = [] (float a, float b) { return std::abs (a - b) < 1.0e-4f; };
        const float m6 = guanyDb (-6.0);

        prova ("busos: tres pistes a un bus de grup, amb fader, mut i efecte", [&]
        {
            std::string s;
            auto q = projecte (3, s);
            const auto bus = afegeixBus (q, "BATERIA", false).id;
            for (int i = 0; i < 3; ++i) comprova (posaSortida (q, i, bus), "la pista " + std::to_string (i) + " va al bus");
            comprova (esBus (q.pistes[3]) && q.pistes[3].tipus == "bus" && q.pistes[3].canals == 2, "el bus és estèreo");
            auto r = sona (q, s);
            comprova (prop (r.l(), 1.5f) && prop (r.r(), 1.5f), "se sumen les tres (" + std::to_string (r.l()) + ")");
            q.pistes[3].faderDb = -6.0;
            r = sona (q, s);
            comprova (prop (r.l(), 1.5f * m6), "el fader del bus les abaixa totes");
            q.pistes[3].pan = -1.0;
            r = sona (q, s);
            comprova (prop (r.l(), 1.5f * m6) && prop (r.r(), 0.0f), "el panorama del bus");
            q.pistes[3].pan = 0.0;
            q.pistes[3].inserts.push_back ({ "i1", "prova", "Prova", true, "" });
            auto e = std::make_shared<EfecteProva033>();
            e->k = 0.5f;
            r = sona (q, s, [&] (const Insert&) { return e; });
            comprova (prop (r.l(), 0.75f * m6), "l'efecte del bus tracta la suma");
            comprova (e->crides == 8, "l'efecte es calcula un sol cop per bloc (" + std::to_string (e->crides) + ")");
            q.pistes[3].mut = true;
            r = sona (q, s);
            comprova (prop (r.l(), 0.0f), "el mut del bus ho calla tot");
        });

        prova ("busos: dos grups en cascada, en qualsevol ordre de la llista", [&]
        {
            std::string s;
            auto q = projecte (2, s);
            const auto b1 = afegeixBus (q, "BATERIA", false).id;
            const auto b2 = afegeixBus (q, "MÚSICA", false).id;
            posaSortida (q, 0, b1);
            posaSortida (q, 2, b2);
            q.pistes[2].faderDb = -6.0;
            q.pistes[3].faderDb = -6.0;
            auto r = sona (q, s);
            comprova (prop (r.l(), 0.5f + 0.5f * m6 * m6), "pista → grup → grup → sortida");
            // MÚSICA primer, BATERIA després, pistes al final (directament a la llista: des de la 0.34 mouPista
            // ja no deixa posar busos a dalt, però un fitxer antic o fet a mà ho podria portar)
            std::rotate (q.pistes.begin(), q.pistes.begin() + 2, q.pistes.end());
            std::swap (q.pistes[0], q.pistes[1]);
            comprova (esBus (q.pistes[0]) && esBus (q.pistes[1]), "els busos són a dalt");
            r = sona (q, s);
            comprova (prop (r.l(), 0.5f + 0.5f * m6 * m6), "l'ordre de la llista no canvia el so");
        });

        prova ("enviaments: una pista a tres retorns i diverses pistes al mateix", [&]
        {
            std::string s;
            auto q = projecte (2, s);
            const auto f1 = afegeixBus (q, "REVERB", true).id, f2 = afegeixBus (q, "DELAY", true).id, f3 = afegeixBus (q, "PARAL", true).id;
            comprova (afegeixEnviament (q, 0, f1) == 0 && afegeixEnviament (q, 0, f2) == 1 && afegeixEnviament (q, 0, f3) == 2, "tres enviaments");
            q.pistes[0].enviaments[1].nivellDb = -6.0;
            q.pistes[1].mut = true;
            auto r = sona (q, s);
            comprova (prop (r.l(), 0.5f + 0.5f + 0.5f * m6 + 0.5f), "el sec i els tres retorns");
            comprova (q.pistes[0].enviaments[0].mode == 1 && q.pistes[0].enviaments[0].nivellDb == 0.0, "per defecte post-fader a 0 dB");
            q.pistes[1].mut = false;
            afegeixEnviament (q, 1, f1);
            q.pistes[2].inserts.push_back ({ "i1", "prova", "Prova", true, "" });
            auto e = std::make_shared<EfecteProva033>();
            e->k = 0.1f;
            r = sona (q, s, [&] (const Insert&) { return e; });
            comprova (prop (r.l(), 1.0f + 0.1f * 1.0f + 0.5f * m6 + 0.5f), "dues pistes sumen al mateix retorn");
            comprova (e->crides == 8, "el retorn es calcula un sol cop per bloc");
        });

        prova ("enviaments: pre-fader, post-fader i post-pan", [&]
        {
            std::string s;
            auto q = projecte (1, s);
            const auto fx = afegeixBus (q, "REVERB", true).id;
            afegeixEnviament (q, 0, fx);
            auto& e = q.pistes[0].enviaments[0];
            q.pistes[1].pan = 0.0;
            // El fader de la pista a baix de tot: només passa el pre-fader.
            q.pistes[0].faderDb = -60.0;
            e.mode = 0;
            comprova (prop (sona (q, s).l(), 0.5f), "pre-fader: no segueix el fader");
            e.mode = 1;
            comprova (prop (sona (q, s).l(), 0.0f), "post-fader: segueix el fader");
            e.mode = 2;
            comprova (prop (sona (q, s).l(), 0.0f), "post-pan: segueix el fader");
            // Fader a -6 i panorama tot a l'esquerra.
            q.pistes[0].faderDb = -6.0;
            q.pistes[0].pan = -1.0;
            q.pistes[1].mut = true;
            const float sec = sona (q, s).l();
            comprova (prop (sec, 0.5f * m6), "amb el retorn callat només se sent la pista");
            q.pistes[1].mut = false;
            q.pistes[0].solo = false;
            auto nomesRetorn = [&]
            {
                auto q2 = q;
                q2.pistes[1].solo = true;   // solo del retorn: només l'efecte
                return sona (q2, s);
            };
            e.mode = 0;
            auto r = nomesRetorn();
            comprova (prop (r.l(), 0.5f) && prop (r.r(), 0.5f), "pre-fader: ni fader ni panorama");
            e.mode = 1;
            r = nomesRetorn();
            comprova (prop (r.l(), 0.5f * m6) && prop (r.r(), 0.5f * m6), "post-fader: el fader sí, el panorama no");
            e.mode = 2;
            r = nomesRetorn();
            comprova (prop (r.l(), 0.5f * m6) && prop (r.r(), 0.0f), "post-pan: fader i panorama");
            // El nivell i el panorama de l'enviament, i el senyal original no canvia.
            e.mode = 1;
            e.nivellDb = -6.0;
            e.pan = 1.0;
            r = nomesRetorn();
            comprova (prop (r.l(), 0.0f) && prop (r.r(), 0.5f * m6 * m6), "nivell i panorama propis de l'enviament");
            q.pistes[1].mut = true;
            r = sona (q, s);
            comprova (prop (r.l(), 0.5f * m6) && prop (r.r(), 0.0f), "moure l'enviament no toca el so de la pista");
        });

        prova ("enviaments: apagat, mut de l'enviament, mut del retorn i mut de la pista", [&]
        {
            std::string s;
            auto q = projecte (1, s);
            const auto fx = afegeixBus (q, "REVERB", true).id;
            afegeixEnviament (q, 0, fx);
            afegeixEnviament (q, 0, fx);
            q.pistes[0].enviaments[1].mode = 0;
            comprova (prop (sona (q, s).l(), 1.5f), "sec i dos enviaments");
            q.pistes[0].enviaments[0].mut = true;
            comprova (prop (sona (q, s).l(), 1.0f), "mut d'un enviament");
            q.pistes[0].enviaments[0].mut = false;
            q.pistes[0].enviaments[1].actiu = false;
            comprova (prop (sona (q, s).l(), 1.0f), "enviament apagat");
            q.pistes[1].mut = true;
            comprova (prop (sona (q, s).l(), 0.5f), "mut del retorn: només el sec");
            q.pistes[1].mut = false;
            q.pistes[0].enviaments[1].actiu = true;
            q.pistes[0].mut = true;
            comprova (prop (sona (q, s).l(), 0.0f), "mut de la pista: també callen els enviaments (fins i tot el pre-fader)");
        });

        prova ("busos: no es poden fer cercles", [&]
        {
            std::string s;
            auto q = projecte (1, s);
            const auto a = afegeixBus (q, "BATERIA", false).id, b = afegeixBus (q, "MÚSICA", false).id, c = afegeixBus (q, "REVERB", true).id;
            comprova (posaSortida (q, 1, b), "BATERIA → MÚSICA");
            comprova (! posaSortida (q, 2, a) && q.pistes[2].sortida.empty(), "MÚSICA → BATERIA es rebutja");
            comprova (! potConnectar (q, a, a), "un bus no pot anar a ell mateix");
            comprova (! potConnectar (q, a, q.pistes[0].id), "una pista normal no és una destinació");
            comprova (afegeixEnviament (q, 2, c) == 0, "MÚSICA envia a REVERB");
            comprova (! posaSortida (q, 3, a), "REVERB → BATERIA tancaria el cercle pels enviaments");
            comprova (afegeixEnviament (q, 3, a) < 0, "i amb un enviament tampoc");
            comprova (posaSortida (q, 2, std::string()), "tornar a la sortida sempre es pot");
            // Un cercle escrit a mà al fitxer: no es penja i sona a la sortida.
            q.pistes[1].sortida = b;
            q.pistes[2].sortida = a;
            posaSortida (q, 0, a);
            auto r = sona (q, s);
            comprova (prop (r.l(), 0.5f), "amb un cercle al fitxer no es penja: la pista va a la sortida");
            q.pistes[2].sortida.clear();
            comprova (prop (sona (q, s).l(), 1.0f), "desfet el cercle, torna a sonar pels busos (grup, grup i retorn)");
        });

        prova ("busos: solo i mut en subgrups i retorns", [&]
        {
            std::string s;
            auto q = projecte (3, s);   // 0 i 1 al grup; 2 sola
            const auto g = afegeixBus (q, "BATERIA", false).id, fx = afegeixBus (q, "REVERB", true).id;
            posaSortida (q, 0, g);
            posaSortida (q, 1, g);
            afegeixEnviament (q, 0, fx);
            comprova (prop (sona (q, s).l(), 2.0f), "tot: tres pistes i un enviament");
            q.pistes[0].solo = true;
            auto e = estatSolo (q);
            comprova (e == std::vector<int> ({ 2, 0, 0, 2, 2 }), "solo d'una pista: ella, el seu grup i el seu retorn");
            comprova (prop (sona (q, s).l(), 1.0f), "se sent amb el seu efecte");
            q.pistes[0].solo = false;
            q.pistes[3].solo = true;
            e = estatSolo (q);
            comprova (e == std::vector<int> ({ 2, 2, 0, 2, 2 }), "solo del grup: les pistes que hi van (i el seu retorn)");
            comprova (prop (sona (q, s).l(), 1.5f), "solo del grup");
            q.pistes[3].solo = false;
            q.pistes[4].solo = true;
            e = estatSolo (q);
            comprova (e == std::vector<int> ({ 1, 0, 0, 0, 2 }), "solo del retorn: la pista només hi envia");
            comprova (prop (sona (q, s).l(), 0.5f), "solo del retorn: només l'efecte");
            q.pistes[0].mut = true;
            comprova (prop (sona (q, s).l(), 0.0f), "el mut passa per davant del solo");
            q.pistes[0].mut = false;
            q.pistes[4].solo = false;
            q.pistes[2].solo = true;
            comprova (prop (sona (q, s).l(), 0.5f), "solo d'una pista sense busos: com sempre");
        });

        prova ("busos: esborrar un bus, aturat, i molts busos alhora", [&]
        {
            std::string s;
            auto q = projecte (2, s);
            const auto g = afegeixBus (q, "BATERIA", false).id, fx = afegeixBus (q, "REVERB", true).id;
            posaSortida (q, 0, g);
            afegeixEnviament (q, 0, fx);
            afegeixEnviament (q, 1, fx);
            q.pistes[3].inserts.push_back ({ "i1", "prova", "Prova", true, "" });
            auto e = std::make_shared<EfecteProva033>();
            sona (q, s, [&] (const Insert&) { return e; }, false);
            comprova (e->crides == 8, "aturat, el retorn es continua calculant (cues de reverb)");
            esborraPista (q, 3);
            comprova (q.pistes[0].enviaments.empty() && q.pistes[1].enviaments.empty(), "esborrar el retorn treu els enviaments");
            esborraPista (q, 2);
            comprova (q.pistes[0].sortida.empty(), "esborrar el grup torna la pista a la sortida");
            comprova (prop (sona (q, s).l(), 1.0f), "i sona com abans");

            // 40 pistes, 32 busos en cadena i 8 enviaments per pista.
            auto gran = projecte (40, s);
            std::vector<std::string> ids;
            for (int i = 0; i < 32; ++i) ids.push_back (afegeixBus (gran, "B" + std::to_string (i), i % 2 == 1).id);
            for (int i = 0; i + 1 < 32; ++i) posaSortida (gran, 40 + i, ids[(size_t) i + 1]);
            for (int i = 0; i < 40; ++i)
            {
                posaSortida (gran, i, ids[(size_t) (i % 32)]);
                gran.pistes[(size_t) i].faderDb = -30.0;
                for (int k = 0; k < 8; ++k)
                    if (const int v = afegeixEnviament (gran, i, ids[(size_t) ((i + k * 3 + 1) % 32)]); v >= 0)
                    {
                        gran.pistes[(size_t) i].enviaments[(size_t) v].mode = k % 3;
                        gran.pistes[(size_t) i].enviaments[(size_t) v].nivellDb = -30.0;
                    }
            }
            const auto t0 = std::chrono::steady_clock::now();
            auto r = sona (gran, s, {}, true, 48000 - 48000 % 512);
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
            bool finit = true;
            for (float v : r.L) finit = finit && std::isfinite (v);
            comprova (finit && r.l() > 0.0f, "40 pistes, 32 busos i 320 enviaments sonen");
            std::printf ("        (un segon de so calculat en %.1f ms)\n", ms);
            comprova (ms < 500.0, "i sobra temps");
        });

        prova ("busos: es desen i es recuperen; un projecte sense busos queda com abans", [&]
        {
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_033";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            std::string s;
            auto q = projecte (2, s);
            q.nom = "Busos";
            comprova (disc::desa (dir / "vell.mibase", q, "2026-11-12T10:00:00", "x", false).correcte, "desat sense busos");
            {
                std::ifstream f (dir / "vell.mibase");
                std::string txt ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
                comprova (txt.find ("enviaments") == std::string::npos && txt.find ("\"sortida\"") == std::string::npos,
                          "un projecte sense busos no porta camps nous");
            }
            auto vell = disc::llegeix (dir / "vell.mibase");
            comprova (vell && vell->pistes.size() == 2 && vell->pistes[0].sortida.empty() && vell->pistes[0].enviaments.empty(),
                      "un projecte d'abans s'obre amb tot a la sortida");
            const auto g = afegeixBus (q, "BATERIA", false).id, fx = afegeixBus (q, "REVERB", true).id;
            posaSortida (q, 0, g);
            afegeixEnviament (q, 0, fx);
            afegeixEnviament (q, 2, fx);
            auto& e = q.pistes[0].enviaments[0];
            e.nivellDb = -7.5; e.pan = -0.25; e.mode = 2; e.mut = true; e.actiu = false;
            q.pistes[3].faderDb = -3.0;
            comprova (disc::desa (dir / "nou.mibase", q, "2026-11-12T10:01:00", "x", false).correcte, "desat amb busos");
            auto n = disc::llegeix (dir / "nou.mibase");
            comprova (n && n->pistes.size() == 4, "es torna a obrir");
            if (n && n->pistes.size() == 4)
            {
                comprova (n->pistes[2].tipus == "bus" && n->pistes[3].tipus == "fx" && n->pistes[2].id == g && n->pistes[3].id == fx, "tipus i ids dels busos");
                comprova (n->pistes[0].sortida == g && n->pistes[1].sortida.empty(), "sortides");
                const auto& x = n->pistes[0].enviaments;
                comprova (x.size() == 1 && x[0].desti == fx && x[0].id == e.id && x[0].nivellDb == -7.5 && x[0].pan == -0.25
                          && x[0].mode == 2 && x[0].mut && ! x[0].actiu, "l'enviament amb tots els seus valors");
                comprova (n->pistes[2].enviaments.size() == 1 && n->pistes[3].faderDb == -3.0, "enviament d'un bus i fader del retorn");
                comprova (afegeixBus (*n, "ALTRE", true).id != fx, "els ids nous no trepitgen els desats");
            }
            std::filesystem::remove_all (dir);
        });
    }
}
