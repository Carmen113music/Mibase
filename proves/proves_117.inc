// 1.17: sortides directes a la targeta (pistes i busos), sense passar pel màster.
namespace
{
    void proves117()
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
        auto projecte = [] (int n, std::string& presa)
        {
            Projecte q;
            q.clic = false;
            presa = afegeixPresa (q, "Àudios/a.wav", "a", 1.0).id;
            for (int i = 0; i < n; ++i) { afegeixPista (q, "P" + std::to_string (i), "audio"); afegeixClip (q, i, presa, 0.0); }
            return q;
        };
        struct Res { float l = 0, r = 0; std::array<float, 8> dir {}; std::uint64_t usats = 0; };
        auto sona = [&] (const Projecte& q, const std::string& presa)
        {
            MapaAudios m { { presa, audio (0.5f) } };
            auto pla = construeixPla (q, m);
            Mesclador mx;
            mx.prepara (48000.0, 512);
            std::vector<float> L (512), R (512);
            Res r;
            for (int p = 0; p < 4096; p += 512)
            {
                std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
                mx.processa (*pla, p, 512, L.data(), R.data(), false, nullptr, true);
            }
            r.l = L[200]; r.r = R[200]; r.usats = mx.usatsDirecte();
            for (int c = 0; c < 8; ++c) r.dir[(size_t) c] = mx.canalDirecte (c)[200];
            return r;
        };
        auto prop = [] (float a, float b) { return std::abs (a - b) < 1.0e-4f; };

        prova ("1.17 Sortida directa: la pista surt pel parell 3/4 i no pel màster", [&]
        {
            std::string s;
            auto q = projecte (2, s);
            comprova (posaSortidaTargeta (q, 0, 2, 2), "la pista 1 va a 3/4");
            const auto r = sona (q, s);
            comprova (prop (r.l, 0.5f) && prop (r.r, 0.5f), "al màster només hi ha la pista 2 (" + std::to_string (r.l) + ")");
            comprova (prop (r.dir[2], 0.5f) && prop (r.dir[3], 0.5f), "a les sortides 3 i 4 hi ha la pista 1");
            comprova (r.usats == ((1u << 2) | (1u << 3)), "només es marquen la 3 i la 4");
        });

        prova ("1.17 Sortida directa mono, i tornar al màster", [&]
        {
            std::string s;
            auto q = projecte (1, s);
            posaSortidaTargeta (q, 0, 5, 1);
            auto r = sona (q, s);
            comprova (prop (r.l, 0.0f) && prop (r.dir[5], 0.5f) && r.usats == (1u << 5), "mono: només la sortida 6");
            comprova (posaSortida (q, 0, ""), "torna al màster");
            comprova (q.pistes[0].sortidaTargeta == -1, "ja no és directa");
            r = sona (q, s);
            comprova (prop (r.l, 0.5f) && r.usats == 0, "torna a sonar pel màster");
        });

        prova ("1.17 Un bus de grup surt directe per la targeta", [&]
        {
            std::string s;
            auto q = projecte (2, s);
            const auto bus = afegeixBus (q, "MONITORS", false).id;
            posaSortida (q, 0, bus);
            posaSortida (q, 1, bus);
            const int ib = pistaPerId (q, bus);
            posaSortidaTargeta (q, ib, 4, 2);
            const auto r = sona (q, s);
            comprova (prop (r.l, 0.0f), "el màster és buit");
            comprova (prop (r.dir[4], 1.0f) && prop (r.dir[5], 1.0f), "les dues pistes surten juntes per la 5/6");
            // anar a un bus treu la sortida directa de la pista
            posaSortidaTargeta (q, 0, 0, 2);
            posaSortida (q, 0, bus);
            comprova (q.pistes[0].sortidaTargeta == -1 && q.pistes[0].sortida == bus, "anar a un bus treu la directa");
        });

        prova ("1.17 Sortida directa: es desa i es llegeix", [&]
        {
            std::string s;
            auto q = projecte (2, s);
            posaSortidaTargeta (q, 1, 6, 1);
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_117";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            const auto f = dir / "Directa.mibase";
            comprova (disc::desa (f, q, "2026-11-27T10:00:00", "2026-11-27 100000", false).correcte, "es desa");
            const auto l = disc::llegeix (f);
            comprova (l && l->pistes[1].sortidaTargeta == 6 && l->pistes[1].canalsTargeta == 1, "torna la sortida 7 mono");
            comprova (l && l->pistes[0].sortidaTargeta == -1, "l'altra continua al màster");
        });
    }
}
