// 0.68: compensació del retard dels plugins (PDC) i «Retard mínim».
namespace
{
    /** Un efecte que només retarda el so «lat» mostres (com un plugin amb «lookahead») i ho diu. */
    struct EfecteRetard068 : Efecte
    {
        int lat = 1000;
        std::vector<float> bl, br;
        int w = 0;
        void prepara (double, int) override { bl.assign ((size_t) lat, 0.0f); br.assign ((size_t) lat, 0.0f); w = 0; }
        int latencia() const noexcept override { return lat; }
        void processa (float* L, float* R, int n) noexcept override
        {
            for (int i = 0; i < n; ++i)
            {
                std::swap (L[i], bl[(size_t) w]);
                std::swap (R[i], br[(size_t) w]);
                w = (w + 1) % lat;
            }
        }
    };

    void proves068()
    {
        using namespace mibase;
        using namespace mibase::edicio;
        // presa: silenci fins a la mostra 4800 i després 0,5
        auto audio = []
        {
            auto a = std::make_shared<AudioPresa>();
            a->fs = 48000.0; a->canals = 1; a->longitud = 48000;
            a->dades[0].assign (48000, 0.0f);
            std::fill (a->dades[0].begin() + 4800, a->dades[0].end(), 0.5f);
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
        // On comença el so (primera mostra > 0,1) a la sortida esquerra.
        auto comenca = [&] (const Projecte& q, const std::string& presa, const ProveidorEfectes& ef, bool minim = false, int* maxim = nullptr)
        {
            MapaAudios m { { presa, audio() } };
            auto pla = construeixPla (q, m, {}, ef);
            Mesclador mx;
            mx.prepara (48000.0, 512);
            for (auto* v : { &pla->insertsMaster }) for (auto& e : *v) e->prepara (48000.0, 512);
            for (auto& t : pla->pistes) for (auto& e : t.inserts) e->prepara (48000.0, 512);
            mx.setRetardMinim (minim);
            std::vector<float> L (12288, 0.0f), R (12288, 0.0f);
            for (int p = 0; p < 12288; p += 512) mx.processa (*pla, p, 512, L.data() + p, R.data() + p, false, nullptr, true);
            if (maxim) *maxim = mx.sonda.pdcMaxim.load();
            for (int i = 0; i < (int) L.size(); ++i) if (L[(size_t) i] > 0.1f) return i;
            return -1;
        };
        auto retard = [] (const Insert&) -> std::shared_ptr<Efecte> { return std::make_shared<EfecteRetard068>(); };

        prova ("0.68 PDC: una pista amb un plugin que retarda arriba al seu lloc", [&]
        {
            std::string s;
            auto q = projecte (2, s);
            comprova (comenca (q, s, {}) == 4800, "sense plugins, a la mostra 4800");
            q.pistes[0].inserts.push_back ({ "i1", "prova", "Prova", true, "" });
            q.pistes[1].mut = true;
            int maxim = 0;
            comprova (comenca (q, s, retard, false, &maxim) == 4800, "amb un plugin de 1000 mostres de retard, també a la 4800");
            comprova (maxim == 1000, "el diagnòstic sap que s'avança 1000 mostres");
            q.pistes[1].mut = false;
            q.pistes[0].mut = true;
            comprova (comenca (q, s, retard) == 4800, "la pista sense plugin no es mou");
        });

        prova ("0.68 PDC: busos, màster i «Retard mínim»", [&]
        {
            std::string s;
            auto q = projecte (1, s);
            const auto bus = afegeixBus (q, "GRUP", false).id;
            posaSortida (q, 0, bus);
            q.pistes[1].inserts.push_back ({ "i2", "prova", "Prova", true, "" });   // el bus retarda
            q.insertsMaster.push_back ({ "i3", "prova", "Prova", true, "" });        // i el màster també
            q.pistes[0].inserts.push_back ({ "i1", "prova", "Prova", true, "" });   // i la pista
            int maxim = 0;
            comprova (comenca (q, s, retard, false, &maxim) == 4800 && maxim == 3000, "pista + bus + màster: s'avança 3000 i arriba a la 4800");
            // Retard mínim: la pista que es grava, el bus i el màster, sense els plugins que retarden (i sense avançar)
            q.pistes[0].grava = true;
            comprova (comenca (q, s, retard, true, &maxim) == 4800 && maxim == 0, "amb «Retard mínim», res no retarda ni s'avança");
            // una pista que no es grava conserva els seus plugins
            q.pistes[0].grava = false;
            comprova (comenca (q, s, retard, true, &maxim) == 4800 && maxim == 1000, "una pista que no es grava conserva el seu plugin");
        });
    }
}
