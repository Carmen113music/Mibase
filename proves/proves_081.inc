// 0.81: preses alternatives (takes), comping, gravació en bucle, Print i Tune en sec.
#include "mibase/Edicio.h"
#include "mibase/Projecte.h"
#include "mibase/ProjecteDisc.h"
#include "mibase/MotorEstudi.h"
#include "mibase/Harmonitzador.h"
#include <filesystem>
namespace
{
    void proves081()
    {
        using namespace mibase;
        using namespace mibase::edicio;
        constexpr double fs = 48000.0;
        auto prop = [] (double a, double b, double e = 1.0e-6) { return std::abs (a - b) < e; };

        // Pista d'àudio amb una gravació a «on» que dura «dur» (sense compte d'entrada ni retard).
        auto grava = [] (Projecte& p, int pista, double on, double dur, const std::string& nom)
        {
            const int c = afegeixGravacio (p, pista, "Àudios/" + nom + ".wav", nom, dur, on, on, 0.0);
            return organitzaTakes (p, pista, { c });
        };
        // Comprova que dues preses no sonen mai al mateix temps (fora del crossfade).
        auto exclusiu = [] (const std::vector<std::vector<std::pair<double, double>>>& z)
        {
            for (size_t a = 0; a < z.size(); ++a)
                for (size_t b = a + 1; b < z.size(); ++b)
                    for (auto x : z[a]) for (auto y : z[b])
                        if (std::min (x.second, y.second) - std::max (x.first, y.first) > 1.0e-6) return false;
            return true;
        };
        auto suma = [] (const std::vector<std::pair<double, double>>& v) { double s = 0; for (auto z : v) s += z.second - z.first; return s; };

        prova ("0.81 Preses: una gravació sola, i clips de seccions diferents, no són preses", [&]
        {
            Projecte p;
            afegeixPista (p, "Veu", "audio");
            comprova (! grava (p, 0, 0.0, 4.0, "a"), "una gravació sola: pista normal");
            comprova (! grava (p, 0, 10.0, 4.0, "b"), "una altra a la tornada (no es trepitgen): tampoc");
            comprova (p.pistes[0].takes.empty() && p.pistes[0].clips.size() == 2, "dos clips normals, cap presa");
            comprova (prop (p.pistes[0].clips[0].iniciVisible(), 0.0) && prop (p.pistes[0].clips[1].iniciVisible(), 10.0), "al seu lloc");
        });

        prova ("0.81 Preses: gravar sobre el mateix tros en fa preses i el comp és la més nova", [&]
        {
            Projecte p;
            afegeixPista (p, "Veu", "audio");
            grava (p, 0, 0.0, 8.0, "a");
            grava (p, 0, 20.0, 4.0, "tornada");   // una altra secció: no hi ha de participar
            comprova (grava (p, 0, 2.0, 4.0, "b"), "la segona trepitja la primera: preses");
            const auto& t = p.pistes[0];
            comprova (t.takes.size() == 2, "dues preses (" + std::to_string (t.takes.size()) + ")");
            comprova (t.takesObertes, "les subpistes s'obren soles");
            comprova (prop (t.takes[0].iniciVisible(), 0.0) && prop (t.takes[0].fiVisible(), 8.0), "la presa 1 és sencera (0-8 s)");
            comprova (prop (t.takes[1].iniciVisible(), 2.0) && prop (t.takes[1].fiVisible(), 6.0), "la presa 2 és sencera (2-6 s)");
            const auto z = zonesTakes (p, 0);
            comprova (exclusiu (z), "mai dues preses alhora");
            comprova (prop (suma (z[1]), 4.0, 0.011), "la 2 sona de 2 a 6 s");
            comprova (prop (suma (z[0]), 4.0, 0.011), "la 1 sona la resta (0-2 i 6-8)");
            bool tornada = false;
            for (auto& c : t.clips) if (prop (c.iniciVisible(), 20.0) && prop (c.fiVisible(), 24.0) && takeDe (t, c) < 0) tornada = true;
            comprova (tornada, "el clip de la tornada queda intacte i no és cap presa");
            comprova (grava (p, 0, 3.0, 2.0, "c") && p.pistes[0].takes.size() == 3, "una tercera: tres preses");
        });

        prova ("0.81 Comping: triar trossos de preses diferents, substituir i treure", [&]
        {
            Projecte p;
            afegeixPista (p, "Veu", "audio");
            grava (p, 0, 0.0, 8.0, "a");
            grava (p, 0, 0.0, 8.0, "b");
            grava (p, 0, 0.0, 8.0, "c");
            comprova (p.pistes[0].takes.size() == 3, "tres preses");
            // compassos 1-4 de la 1, 4-6 de la 2, 6-8 de la 3
            compTake (p, 0, 0, 0.0, 4.0);
            compTake (p, 0, 1, 4.0, 6.0);
            compTake (p, 0, 2, 6.0, 8.0);
            auto z = zonesTakes (p, 0);
            comprova (exclusiu (z), "mai dues preses alhora");
            comprova (prop (suma (z[0]), 4.0, 0.011) && prop (suma (z[1]), 2.0, 0.011) && prop (suma (z[2]), 2.0, 0.011), "4 + 2 + 2 s");
            // la 2 al tros de la 1: la 1 perd aquest tros, la resta intacta
            compTake (p, 0, 1, 1.0, 2.0);
            z = zonesTakes (p, 0);
            comprova (exclusiu (z), "després de substituir, tampoc");
            comprova (prop (suma (z[0]), 3.0, 0.011) && prop (suma (z[1]), 3.0, 0.011) && prop (suma (z[2]), 2.0, 0.011), "3 + 3 + 2 s");
            // crossfades: cada canvi té un fade curt i els clips no tenen més de 10 ms de trepitjada
            for (auto& c : p.pistes[0].clips) comprova (c.fadeIn <= crossfadeComp + 1e-9 && c.fadeOut <= crossfadeComp + 1e-9, "fades curts");
            // trossos petits
            comprova (compTake (p, 0, 2, 3.0, 3.05), "es pot triar un tros de 50 ms");
            // treure un tros: queda sense presa
            buidaRang (p, 0, 6.5, 7.0);
            z = zonesTakes (p, 0);
            comprova (prop (suma (z[2]), 1.55, 0.02), "treure un tros del comp el deixa buit (" + std::to_string (suma (z[2])) + ")");
            // la presa sencera
            comprova (compTakeSencera (p, 0, 1), "triar una presa sencera");
            z = zonesTakes (p, 0);
            comprova (prop (suma (z[1]), 8.0, 0.011) && suma (z[0]) < 1e-6 && suma (z[2]) < 1e-6, "ara sona només la 2");
            int deLa2 = 0;
            for (auto& c : p.pistes[0].clips) if (takeDe (p.pistes[0], c) == 1) ++deLa2;
            comprova (deLa2 == 1, "els trossos de la mateixa presa s'ajunten en un clip (" + std::to_string (deLa2) + ")");
            // les preses no canvien mai
            comprova (prop (p.pistes[0].takes[0].iniciVisible(), 0.0) && prop (p.pistes[0].takes[0].fiVisible(), 8.0), "les preses originals, intactes");
        });

        prova ("0.81 Comping: treure una presa i desar/llegir el projecte", [&]
        {
            Projecte p;
            p.nom = "Takes";
            afegeixPista (p, "Veu", "audio");
            grava (p, 0, 0.0, 8.0, "a");
            grava (p, 0, 0.0, 8.0, "b");
            grava (p, 0, 0.0, 8.0, "c");
            compTake (p, 0, 0, 0.0, 3.0);
            p.pistes[0].takesObertes = false;
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_081";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            const auto f = dir / "Takes.mibase";
            const auto r = disc::desa (f, p, "2026-11-22T10:00:00", "2026-11-22 100000", false);
            comprova (r.correcte, "es desa");
            const auto l = disc::llegeix (f);
            comprova (l.has_value(), "es llegeix");
            if (! l) return;
            Projecte q = *l;
            comprova (q.pistes.size() == 1 && q.pistes[0].takes.size() == 3, "les tres preses tornen");
            comprova (! q.pistes[0].takesObertes, "plegades com estaven");
            comprova (q.pistes[0].clips.size() == p.pistes[0].clips.size(), "el comp torna igual");
            comprova (q.pistes[0].takes[2].presa == p.pistes[0].takes[2].presa && prop (q.pistes[0].takes[2].pos, p.pistes[0].takes[2].pos), "mateixa presa al mateix lloc");
            treuTake (q, 0, 0);
            comprova (q.pistes[0].takes.size() == 2, "treure'n una: en queden dues");
            for (auto& c : q.pistes[0].clips) comprova (takeDe (q.pistes[0], c) >= 0, "el comp ja no en té res");
            treuTake (q, 0, 0);
            comprova (q.pistes[0].takes.empty() && ! q.pistes[0].clips.empty(), "amb una de sola, pista normal (el que sonava continua)");
            std::filesystem::remove_all (dir);
        });

        prova ("0.81 Bucle: cada volta és una presa al lloc del bucle", [&]
        {
            Projecte p;
            afegeixPista (p, "Veu", "audio");
            // bucle de 4 a 8 s; es grava des de 2 s (pre-roll fins a 4) i fa 3 voltes i mitja (la mitja és curta: 0,3 s)
            const double ini = 4.0, L = 4.0, lat = 0.005;
            const double durada = 2.0 + 3 * L + 0.3;   // fitxer des de 2 s
            const int c = afegeixGravacio (p, 0, "Àudios/bucle.wav", "bucle", durada, 2.0, 4.0, lat);
            const auto v = afegeixVoltes (p, 0, c, ini, 2.0 + L, L, 3, lat);
            comprova (v.size() == 3, "3 voltes guardades (la de 0,3 s no): " + std::to_string (v.size()));
            const auto& t = p.pistes[0];
            for (size_t k = 0; k < v.size(); ++k)
            {
                const auto& x = t.clips[(size_t) v[k]];
                comprova (prop (x.fiVisible(), ini + L, 1e-9), "volta " + std::to_string (k + 1) + " acaba al final del bucle");
                if (k > 0) comprova (prop (x.iniciVisible(), ini, 1e-9) && prop (x.longitud(), L, 1e-9), "i comença a l'inici del bucle");
            }
            // la volta 2 surt del fitxer just després de la 1 (amb el retard corregit)
            comprova (prop (t.clips[(size_t) v[1]].inici, 2.0 + L + lat, 1e-9), "volta 2: el tros bo del fitxer");
            comprova (organitzaTakes (p, 0, v) && t.takes.size() == 3, "són preses");
            const auto z = zonesTakes (p, 0);
            comprova (exclusiu (z) && prop (suma (z[2]), L, 0.011), "sona la darrera volta");
        });

        prova ("0.81 Bucle: el motor torna a l'inici mentre grava i compta les voltes", [&]
        {
            Projecte p;
            p.tempo = 120.0;
            p.clic = false;
            auto& t = afegeixPista (p, "Veu", "audio");
            t.grava = true; t.entrada = 0;
            marcaBucle (p, 1, 1);
            p.bucle.actiu = true;
            const double ib = iniciBucle (p), fb = fiBucle (p);
            MotorEstudi motor;
            motor.prepara (fs, 512);
            motor.setMapaEntrades ({ 0 });
            motor.publica (construeixPla (p, {}, {}));
            const auto f = std::filesystem::temp_directory_path() / "mibase_081_bucle.wav";
            comprova (motor.gravadora().comenca (f, 1, fs), "gravadora en marxa");
            motor.grava (ib, ib, 0, 1);
            const int bloc = 256;
            std::vector<float> in (bloc, 0.1f), L (bloc), R (bloc);
            const float* ent[] = { in.data() };
            float* s[] = { L.data(), R.data() };
            const long long total = (long long) ((fb - ib) * 2.5 * fs);
            double maxPos = 0.0;
            for (long long k = 0; k < total; k += bloc)
            {
                motor.processa (ent, 1, s, 2, bloc);
                motor.recull();
                maxPos = std::max (maxPos, motor.getPosicio());
            }
            motor.atura();
            motor.processa (ent, 1, s, 2, bloc);
            const auto r = motor.gravadora().acaba();
            std::filesystem::remove (f);
            comprova (maxPos <= fb + 1e-6, "el capçal no passa del final del bucle");
            comprova (motor.getVoltes() == 2, "2 voltes (" + std::to_string (motor.getVoltes()) + ")");
            comprova (prop (motor.getPrimeraVolta(), fb - ib, 1.0 / fs), "la primera volta, al final del bucle dins el fitxer");
            comprova (prop (motor.getLlargVolta(), fb - ib, 1.0 / fs) && prop (motor.getIniciVolta(), ib, 1.0 / fs), "llargada i inici del bucle");
            comprova (r.frames >= total - bloc, "el fitxer és continu (" + std::to_string (r.frames) + " frames)");
        });

        prova ("0.81 Comp: els canvis de presa no fan clic", [&]
        {
            // dues preses amb sinus en contrafase: un tall sec faria un salt de 2x l'amplitud
            Projecte p;
            p.clic = false;
            afegeixPista (p, "Veu", "audio");
            grava (p, 0, 0.0, 2.0, "a");
            grava (p, 0, 0.0, 2.0, "b");
            compTake (p, 0, 0, 0.0, 1.0);
            const auto& t = p.pistes[0];
            auto sinus = [] (double fase)
            {
                auto a = std::make_shared<AudioPresa>();
                a->fs = fs; a->canals = 1; a->longitud = (long long) (2 * fs);
                a->dades[0].resize ((size_t) a->longitud);
                for (long long i = 0; i < a->longitud; ++i) a->dades[0][(size_t) i] = (float) (0.5 * std::sin (2 * 3.141592653589793 * 220.0 * i / fs + fase));
                return a;
            };
            MapaAudios m { { t.takes[0].presa, sinus (0.0) }, { t.takes[1].presa, sinus (3.141592653589793) } };
            auto pla = construeixPla (p, m);
            Mesclador mx;
            mx.prepara (fs, 512);
            std::vector<float> L ((size_t) (2 * fs)), R ((size_t) (2 * fs));
            for (int q = 0; q + 512 <= (int) L.size(); q += 512) mx.processa (*pla, q, 512, L.data() + q, R.data() + q, false, nullptr, true);
            float salt = 0.0f, pic = 0.0f;
            const int c0 = (int) (0.9 * fs), c1 = (int) (1.1 * fs);
            for (int i = c0; i < c1; ++i) { salt = std::max (salt, std::abs (L[(size_t) i] - L[(size_t) i - 1])); pic = std::max (pic, std::abs (L[(size_t) i])); }
            const float pas = (float) (0.5 * 2 * 3.141592653589793 * 220.0 / fs);   // el pendent màxim del sinus
            std::printf ("        salt màxim al canvi %.4f (pendent del sinus %.4f), pic %.3f\n", salt, pas, pic);
            comprova (salt < pas * 1.6f, "sense salt al canvi de presa");
            comprova (pic > 0.4f && pic < 0.55f, "el so hi és i no se suma");
        });

        prova ("0.81 Print Comp: pista nova a sota, amb els mateixos efectes, i l'original silenciada", [&]
        {
            Projecte p;
            auto& t = afegeixPista (p, "Veu", "audio");
            t.faderDb = -3.0; t.pan = 0.2;
            Insert x; x.id = "i90"; x.plugin = "intern:reverb"; x.nom = "Reverb"; t.inserts.push_back (x);
            Insert v; v.id = "i91"; v.plugin = pluginVeu; t.inserts.push_back (v);
            Automatitzacio a; a.id = "a92"; a.objectiu = "insert:i90:mix"; a.punts.push_back ({}); t.autos.push_back (a);
            afegeixPista (p, "Baix", "audio");
            grava (p, 0, 0.0, 8.0, "a");
            grava (p, 0, 0.0, 8.0, "b");
            const auto& pr = afegeixPresa (p, "Àudios/Print - Veu.wav", "Print - Veu", 8.5);
            const int n = creaPistaPrint (p, 0, pr.id, -0.25, 0.25, 8.25);
            comprova (n == 1 && p.pistes.size() == 3, "pista nova just a sota");
            const auto& o = p.pistes[0];
            const auto& q = p.pistes[1];
            comprova (q.nom == "Print - Veu", "es diu «Print - Veu»");
            comprova (o.mut && ! q.mut, "l'original, silenciada; el Print sona");
            comprova (o.takes.size() == 2 && ! o.clips.empty(), "l'original conserva les preses i el comp");
            comprova (q.takes.empty() && q.clips.size() == 1 && q.clips[0].presa == pr.id, "el Print és un clip d'àudio propi");
            comprova (prop (q.clips[0].iniciVisible(), 0.0) && prop (q.clips[0].fiVisible(), 8.0), "al mateix lloc");
            comprova (q.inserts.size() == 1 && q.inserts[0].plugin == "intern:reverb" && q.inserts[0].id != "i90", "la Reverb, amb un id nou (sense el Vocal Editor)");
            comprova (q.autos.size() == 1 && q.autos[0].objectiu == "insert:" + q.inserts[0].id + ":mix", "l'automatització segueix el seu efecte");
            comprova (prop (q.faderDb, -3.0) && prop (q.pan, 0.2) && q.id != o.id, "mateix volum i panorama");
            comprova (p.pistes[2].nom == "Baix", "les altres pistes, al seu lloc");
        });

        prova ("0.81 Tune: en directe passa en sec i sense retard", [&]
        {
            tune::Tune tu;
            tu.prepara (fs, 512);
            const int lat = tu.latencia();
            comprova (lat > 100, "normalment retarda (" + std::to_string (lat) + " mostres)");
            tu.posaDirecte (true);
            comprova (tu.latencia() == 0, "en directe, cap retard");
            std::vector<float> L (512), R (512);
            for (int i = 0; i < 512; ++i) L[(size_t) i] = R[(size_t) i] = (float) std::sin (0.03 * i);
            const auto L0 = L;
            tu.processa (L.data(), R.data(), 512);
            bool igual = true;
            for (int i = 0; i < 512; ++i) if (L[(size_t) i] != L0[(size_t) i]) igual = false;
            comprova (igual, "el so passa tal qual");
            tu.posaDirecte (false);
            comprova (tu.latencia() == lat, "en tornar a reproduir, torna a afinar (i a retardar)");
        });
    }
}
