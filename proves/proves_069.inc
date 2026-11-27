// 0.69: Limitador (Mibase); «Retard mínim» amb mínim i plugins sempre encesos.
#include "mibase/Limitador.h"
namespace
{
    void proves069()
    {
        using namespace mibase;
        auto sinus = [] (float amp, float hz, int n)
        {
            std::vector<float> v ((size_t) n);
            for (int i = 0; i < n; ++i) v[(size_t) i] = amp * (float) std::sin (2.0 * 3.141592653589793 * hz * i / 48000.0);
            return v;
        };
        auto passa = [] (limitador::Limitador& d, std::vector<float>& L, std::vector<float>& R)
        {
            for (int i = 0; i < (int) L.size(); i += 256) d.processa (L.data() + i, R.data() + i, std::min (256, (int) L.size() - i));
        };
        auto pic = [] (const std::vector<float>& v, int a, int b) { float m = 0; for (int i = a; i < b; ++i) m = std::max (m, std::fabs (v[(size_t) i])); return m; };

        prova ("0.69 Limitador: cap pic passa del sostre, amb tots els estils", [&]
        {
            for (int e = 0; e < limitador::numEstils; ++e)
            {
                limitador::Limitador d;
                d.prepara (48000.0, 256);
                d.posa (limitador::EstilP, (float) e); d.posa (limitador::Gain, 12); d.posa (limitador::Ceiling, -1);
                // cops: sinus fort amb pics de transitoris
                auto L = sinus (0.5f, 110.0f, 96000), R = sinus (0.4f, 165.0f, 96000);
                for (int k = 0; k < 96000; k += 4800) { L[(size_t) k] = 1.0f; R[(size_t) k + 3] = -0.95f; }
                passa (d, L, R);
                const float sostre = std::pow (10.0f, -1.0f / 20.0f);
                const bool finit = std::all_of (L.begin(), L.end(), [] (float x) { return std::isfinite (x); });
                std::printf ("        %s: pic %.4f (sostre %.4f), reducció %.1f dB, retallades de seguretat %u\n", limitador::nomEstil (e), std::max (pic (L, 0, 96000), pic (R, 0, 96000)), sostre,
                             d.mesures().reduccio.load(), (unsigned) d.mesures().retallades.load());
                comprova (d.mesures().retallades.load() < 20, std::string (limitador::nomEstil (e)) + ": és el limitador qui ho fa (gairebé sense la seguretat)");
                comprova (finit && pic (L, 0, 96000) <= sostre + 1e-5f && pic (R, 0, 96000) <= sostre + 1e-5f,
                          std::string (limitador::nomEstil (e)) + ": no passa del sostre");
            }
        });

        prova ("0.69 Limitador: latència fixa, so net quan no cal limitar, i igualat", [&]
        {
            limitador::Limitador d;
            d.prepara (48000.0, 256);
            comprova (d.latencia() == 228, "mira endavant una mica menys de 5 ms (1.12: 240 − 12 mostres a 48 kHz)");
            d.posa (limitador::EstilP, limitador::Transparent);   // l'estil no canvia la latència
            comprova (d.latencia() == 228, "la latència no depèn de l'estil");
            // un so fluix amb +6 dB: surt el mateix, 6 dB més fort i retardat 240 mostres
            d.posa (limitador::Gain, 6);
            auto L = sinus (0.1f, 440.0f, 48000), R = L;
            const auto orig = L;
            passa (d, L, R);
            const float g = std::pow (10.0f, 6.0f / 20.0f);
            float err = 0;
            for (int i = 24000; i < 48000; ++i) err = std::max (err, std::fabs (L[(size_t) i] - orig[(size_t) i - 228] * g));
            comprova (err < 1e-3f, "sense limitar: el mateix so, més fort i retardat la latència");
            // igualat: el mateix volum que l'original
            limitador::Limitador q;
            q.prepara (48000.0, 256);
            q.posa (limitador::Gain, 10); q.posa (limitador::Igualat, 1);
            auto L2 = sinus (0.05f, 440.0f, 48000), R2 = L2;
            passa (q, L2, R2);
            comprova (std::fabs (pic (L2, 24000, 48000) - 0.05f) < 0.002f, "escolta igualat: queda al volum d'entrada");
        });

        prova ("0.69 Limitador: LUFS, estat i presets", [&]
        {
            limitador::Limitador d;
            d.prepara (48000.0, 256);
            d.posa (limitador::Ceiling, 0);
            auto L = sinus (0.1f, 1000.0f, 48000 * 4), R = L;   // -20 dBFS als dos costats = -20 LUFS (un sol costat seria -23)
            passa (d, L, R);
            const float l = d.mesures().lufsCurt.load();
            std::printf ("        sinus 1 kHz a -20 dBFS: %.2f LUFS\n", l);
            comprova (std::fabs (l + 20.0f) < 0.7f, "mesura LUFS (sinus 1 kHz a -20 dBFS als dos costats = -20 LUFS)");
            limitador::Limitador a;
            a.posa (limitador::EstilP, limitador::Punch); a.posa (limitador::Gain, 7.5f);
            limitador::Limitador b;
            b.posaEstat (a.estat());
            comprova (b.estat() == a.estat(), "l'estat es desa i es llegeix igual");
            const auto& v = limitador::presetsFabrica();
            std::printf ("        (%zu presets)\n", v.size());
            std::set<std::string> noms;
            bool be = true;
            for (const auto& p : v)
            {
                noms.insert (p.nom);
                limitador::Limitador x;
                x.posaEstat (p.estat);
                be = be && x.estat() == p.estat && ! p.descripcio.empty()
                     && std::find (limitador::categories().begin(), limitador::categories().end(), p.categoria) != limitador::categories().end();
            }
            comprova (v.size() >= 20 && noms.size() == v.size() && be, "presets vàlids i sense noms repetits");
        });

        prova ("0.69 RM: mínim en ms i plugins «sempre encesos»", [&]
        {
            using namespace mibase::edicio;
            auto audio = []
            {
                auto a = std::make_shared<AudioPresa>();
                a->fs = 48000.0; a->canals = 1; a->longitud = 48000;
                a->dades[0].assign (48000, 0.0f);
                std::fill (a->dades[0].begin() + 4800, a->dades[0].end(), 0.5f);
                return a;
            };
            Projecte q;
            q.clic = false;
            const auto presa = afegeixPresa (q, "Àudios/a.wav", "a", 1.0).id;
            afegeixPista (q, "P", "audio");
            afegeixClip (q, 0, presa, 0.0);
            q.pistes[0].inserts.push_back ({ "i1", "prova", "Prova", true, "" });
            q.pistes[0].grava = true;
            auto mesura = [&] (int llindar)
            {
                MapaAudios m { { presa, audio() } };
                auto pla = construeixPla (q, m, {}, [] (const Insert&) -> std::shared_ptr<Efecte> { auto e = std::make_shared<EfecteRetard068>(); e->lat = 100; return e; });
                for (auto& t : pla->pistes) for (auto& e : t.inserts) e->prepara (48000.0, 512);
                Mesclador mx;
                mx.prepara (48000.0, 512);
                mx.setRetardMinim (true);
                mx.setLlindarRM (llindar);
                std::vector<float> L (8192), R (8192);
                for (int p = 0; p < 8192; p += 512) mx.processa (*pla, p, 512, L.data() + p, R.data() + p, false, nullptr, true);
                return std::make_pair (mx.sonda.pdcMaxim.load(), mx.sonda.pdcSaltats.load());
            };
            auto r = mesura (0);
            comprova (r.first == 0 && r.second == 1, "sense mínim: el plugin de 100 mostres s'apaga");
            r = mesura (240);
            comprova (r.first == 100 && r.second == 0, "amb mínim de 5 ms (240 mostres): el de 100 mostres es queda");
            q.pistes[0].inserts[0].encesAmbRM = true;
            r = mesura (0);
            comprova (r.first == 100 && r.second == 0, "«sempre encès amb RM»: no s'apaga mai");
            // es desa
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_069";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            comprova (disc::desa (dir / "p.mibase", q, "2026-11-19T18:00:00", "x", false).correcte, "desat");
            const auto l = disc::llegeix (dir / "p.mibase");
            comprova (l.has_value() && l->pistes[0].inserts[0].encesAmbRM, "«sempre encès» es torna a llegir");
            std::filesystem::remove_all (dir);
        });
    }
}
