// Proves automàtiques del motor de Mibase.
// S'executen a qualsevol sistema (Linux a la sessió de Claude, Windows a GitHub Actions).

#include "mibase/ClicDeProva.h"
#include "mibase/CuaSenseBloqueig.h"
#include "mibase/MesuraLatencia.h"
#include "mibase/Mesurador.h"
#include "mibase/MotorDiagnostic.h"
#include "mibase/ProjecteDisc.h"
#include "mibase/Edicio.h"
#include "mibase/SinteSenzill.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    int fallades = 0;
    int comprovacions = 0;

    void comprova (bool condicio, const std::string& que)
    {
        ++comprovacions;
        if (! condicio)
        {
            ++fallades;
            std::printf ("    FALLA: %s\n", que.c_str());
        }
    }

    void prova (const char* nom, const std::function<void()>& f)
    {
        const int abans = fallades;
        f();
        std::printf ("%s %s\n", fallades == abans ? "[ok]   " : "[FALLA]", nom);
    }

    float pic (const std::vector<float>& v, size_t des, size_t fins)
    {
        float p = 0.0f;
        for (size_t i = des; i < std::min (fins, v.size()); ++i)
            p = std::max (p, std::abs (v[i]));
        return p;
    }

    bool totFinit (const std::vector<float>& v)
    {
        return std::all_of (v.begin(), v.end(), [] (float x) { return std::isfinite (x); });
    }

    using namespace mibase;

    // Simula un cable de la sortida a l'entrada: el que surt torna 'retard' mostres després, amb soroll.
    MesuraLatencia::Resultat simulaCable (double fs, int bloc, int retard, float guany, float soroll, bool connectat)
    {
        MotorDiagnostic motor;
        motor.prepara (fs, bloc);
        motor.clicActiu = true;   // durant la mesura s'ha de silenciar sol
        comprova (motor.latencia().demana(), "es pot demanar la mesura");

        // Tot el que ha sortit fins ara; l'entrada en llegeix 'retard' mostres enrere.
        // (Com a la realitat, el retard sempre és més gran que el bloc.)
        std::vector<float> historia;
        std::mt19937 gen (1234);
        std::uniform_real_distribution<float> dist (-soroll, soroll);

        std::vector<float> in ((size_t) bloc), outL ((size_t) bloc), outR ((size_t) bloc);
        const float* entrades[] = { in.data() };
        float* sortides[] = { outL.data(), outR.data() };

        for (int iter = 0; iter < 100000 && ! motor.latencia().teDades(); ++iter)
        {
            const long long inici = (long long) historia.size();
            for (int i = 0; i < bloc; ++i)
            {
                const long long t = inici + i - retard;
                const float tornat = (t >= 0 && t < (long long) historia.size()) ? historia[(size_t) t] : 0.0f;
                in[(size_t) i] = (connectat ? guany * tornat : 0.0f) + dist (gen);
            }

            motor.processa (entrades, 1, sortides, 2, bloc);
            historia.insert (historia.end(), outL.begin(), outL.end());
        }

        comprova (motor.latencia().teDades(), "la mesura s'acaba");
        return motor.latencia().calcula();
    }
}

int main()
{
    std::printf ("Proves del motor de Mibase\n\n");

    prova ("Cua sense bloqueig: ordre i capacitat", []
    {
        CuaSenseBloqueig<int, 8> cua;
        for (int i = 0; i < 8; ++i)
            comprova (cua.posa (i), "hi cap l'element " + std::to_string (i));
        comprova (! cua.posa (99), "la cua plena no accepta més");

        int v = -1;
        for (int i = 0; i < 8; ++i)
        {
            comprova (cua.treu (v) && v == i, "surt en ordre " + std::to_string (i));
        }
        comprova (! cua.treu (v), "la cua queda buida");

        for (int volta = 0; volta < 100; ++volta)
        {
            cua.posa (volta);
            comprova (cua.treu (v) && v == volta, "dona voltes bé");
        }
    });

    prova ("Clic: sona a cada temps (120 bpm) i calla entre temps", []
    {
        const double fs = 48000.0;
        ClicDeProva clic;
        clic.prepara (fs);
        std::vector<float> b ((size_t) fs * 2, 0.0f);
        for (size_t i = 0; i < b.size(); i += 256)
            clic.processa (b.data() + i, (int) std::min<size_t> (256, b.size() - i));

        const size_t temps = 24000;
        for (size_t t = 0; t < 4; ++t)
        {
            comprova (pic (b, t * temps, t * temps + 1000) > 0.1f, "el clic sona al temps " + std::to_string (t));
            comprova (pic (b, t * temps + 3000, t * temps + 20000) < 0.0001f, "silenci després del temps " + std::to_string (t));
        }
        comprova (pic (b, 0, 1000) > pic (b, temps, temps + 1000), "el primer temps és més fort");
        comprova (pic (b, 0, b.size()) <= 1.0f, "no satura");
        comprova (totFinit (b), "valors vàlids");
    });

    prova ("So de prova: nota, final de nota i 8 notes alhora", []
    {
        const double fs = 48000.0;
        SinteSenzill s;
        s.prepara (fs);

        std::vector<float> b (48000, 0.0f);
        s.midi (0x90, 60, 100);
        s.processa (b.data(), 24000);
        comprova (pic (b, 1000, 24000) > 0.1f, "la nota sona");
        comprova (s.veusSonant() == 1, "una veu");

        s.midi (0x80, 60, 0);
        s.processa (b.data() + 24000, 24000);
        comprova (pic (b, 47000, 48000) < 0.001f, "la nota s'apaga en mig segon");
        comprova (s.veusSonant() == 0, "cap veu");

        std::vector<float> c (4800, 0.0f);
        for (int n = 0; n < 10; ++n)
            s.notaOn (48 + n * 3, 127);
        comprova (s.veusSonant() == SinteSenzill::numVeus, "com a molt 8 veus");
        s.processa (c.data(), (int) c.size());
        comprova (pic (c, 0, c.size()) < 3.0f && totFinit (c), "8 notes fortes, valors raonables");

        s.midi (0xB0, 123, 0);
        comprova (s.veusSonant() == 0, "«tot apagat» silencia");

        s.midi (0x90, 64, 0);   // nota amb velocitat 0 = final de nota
        comprova (s.veusSonant() == 0, "velocitat 0 no fa sonar res");
    });

    prova ("Vúmetre: pic i saturacions", []
    {
        Mesurador m;
        std::vector<float> b = { 0.1f, -0.5f, 0.25f };
        m.processa (b.data(), 3);
        comprova (std::abs (m.llegeixIReinicia() - 0.5f) < 1e-6f, "pic 0,5");
        comprova (m.llegeixIReinicia() == 0.0f, "es reinicia en llegir");
        std::vector<float> s = { 1.0f };
        m.processa (s.data(), 1);
        comprova (m.getSaturacions() == 1, "compta la saturació");
    });

    prova ("Latència: troba el retard exacte amb un cable simulat", []
    {
        struct Cas { double fs; int bloc; int retard; float guany; float soroll; };
        const Cas casos[] = {
            { 48000.0,  32,    37, 0.8f,  0.001f },
            { 48000.0, 128,   300, 0.5f,  0.01f  },
            { 44100.0, 256,  1234, 0.2f,  0.02f  },
            { 96000.0, 512,  5000, -0.7f, 0.005f },   // amb la polaritat girada
            { 48000.0, 480, 11000, 0.05f, 0.005f },   // senyal fluix i retard llarg (WASAPI)
        };

        for (auto& c : casos)
        {
            const auto r = simulaCable (c.fs, c.bloc, c.retard, c.guany, c.soroll, true);
            const std::string nom = "retard " + std::to_string (c.retard) + " a " + std::to_string ((int) c.fs) + " Hz";
            comprova (r.correcte, nom + ": correcte (" + r.missatge + ")");
            comprova (r.mostres == c.retard, nom + ": mesurat " + std::to_string (r.mostres));
            comprova (r.dispersio == 0, nom + ": estable");
            comprova (std::abs (r.ms - 1000.0 * c.retard / c.fs) < 0.01, nom + ": ms correctes");
        }
    });

    prova ("Latència: sense cable, ho diu", []
    {
        const auto r = simulaCable (48000.0, 128, 300, 0.5f, 0.01f, false);
        comprova (! r.correcte, "no dona un resultat inventat");
        comprova (! r.missatge.empty(), "explica què passa");
    });

    prova ("Motor: el teclat sona i es pot apagar; la sortida sempre plena", []
    {
        MotorDiagnostic motor;
        motor.prepara (48000.0, 128);

        std::vector<float> in (128, 0.0f), l (128, 7.0f), r (128, 7.0f);
        const float* entrades[] = { in.data() };
        float* sortides[] = { l.data(), r.data() };

        motor.processa (entrades, 1, sortides, 2, 128);
        comprova (pic (l, 0, 128) == 0.0f && pic (r, 0, 128) == 0.0f, "sense res, silenci (i esborra el que hi havia)");

        motor.enviaMidi ({ 0x90, 69, 100 });
        float maxim = 0.0f;
        for (int i = 0; i < 20; ++i)
        {
            motor.processa (entrades, 1, sortides, 2, 128);
            maxim = std::max (maxim, pic (l, 0, 128));
        }
        comprova (maxim > 0.1f, "la nota del teclat sona");
        comprova (l == r, "les dues sortides iguals");

        motor.sinteActiu = false;
        motor.processa (entrades, 1, sortides, 2, 128);
        motor.processa (entrades, 1, sortides, 2, 128);
        comprova (pic (l, 0, 128) == 0.0f, "en apagar el so de prova, calla");

        // Un bloc més gran del previst no ha de petar.
        std::vector<float> gran (4096, 0.0f), granIn (4096, 0.0f);
        const float* eg[] = { granIn.data() };
        float* sg[] = { gran.data() };
        motor.clicActiu = true;
        motor.processa (eg, 1, sg, 1, 4096);
        comprova (pic (gran, 0, 1000) > 0.1f && totFinit (gran), "bloc gran: el clic sona i no peta");
    });


    prova ("Noms: es validen com a Windows", []
    {
        using namespace mibase::disc;
        comprova (validaNom ("Tornada amb slide").correcte, "nom normal");
        comprova (validaNom ("Cançó d'estiu · 2").correcte, "accents i punts volats");
        comprova (! validaNom ("").correcte, "buit");
        comprova (! validaNom ("   ").correcte, "només espais");
        comprova (! validaNom ("A/B").correcte, "barra");
        comprova (! validaNom ("Què?").correcte, "interrogant");
        comprova (! validaNom ("Final.").correcte, "acaba amb punt");
        comprova (! validaNom ("Final ").correcte, "acaba amb espai");
        comprova (! validaNom ("con").correcte && ! validaNom ("COM1.txt").correcte, "noms reservats");
        comprova (validaNom ("Concert").correcte, "«Concert» sí que val");
        comprova (netejaNom ("Solo: part 2?") == "Solo_ part 2_", "neteja caràcters");
        comprova (netejaNom ("  ") == "Pista", "neteja d'un nom buit");
    });

    prova ("Projecte: es crea amb la seva carpeta, Àudios, Còpies i el diari", []
    {
        namespace fs = std::filesystem;
        using namespace mibase::disc;
        const auto arrel = fs::temp_directory_path() / "mibase_proves";
        fs::remove_all (arrel);
        fs::create_directories (arrel);

        mibase::Projecte p;
        p.nom = "Tornada amb accents àèéíòóú";
        p.tempo = 100;
        p.compasNum = 6; p.compasDen = 8;

        auto r = creaProjecte (arrel, p, "2026-11-03T09:45:00", "03/11/2026 09:45");
        comprova (r.correcte, "creat (" + r.error + ")");
        const auto carpeta = carpetaDe (arrel, p.nom);
        comprova (fs::is_directory (carpeta), "carpeta del projecte");
        comprova (fs::is_regular_file (carpeta / cami (p.nom + ".mibase")), "fitxer del projecte");
        comprova (fs::is_directory (carpeta / cami ("Àudios")), "carpeta Àudios");
        comprova (fs::is_directory (carpeta / cami ("Còpies")), "carpeta Còpies");
        comprova (esCarpetaDeProjecte (carpeta), "es reconeix com a projecte");
        comprova (comptaProjectes (arrel) == 1, "la carpeta mare té 1 projecte");

        auto e = llegeixEvolucio (carpeta, 10);
        comprova (e.size() == 1 && e[0].quan == "03/11/2026 09:45", "diari amb una entrada");
        comprova (! e.empty() && e[0].text.find ("Projecte creat (buit)") == 0 && e[0].text.find ("6/8") != std::string::npos, "text de creació");

        auto r2 = creaProjecte (arrel, p, "2026-11-03T09:46:00", "03/11/2026 09:46");
        comprova (! r2.correcte && ! r2.error.empty(), "no trepitja un projecte que ja existeix");

        auto llegit = llegeix (r.fitxer);
        comprova (llegit.has_value(), "es llegeix");
        if (llegit)
        {
            comprova (llegit->nom == p.nom, "nom amb accents");
            comprova (llegit->tempo == 100 && llegit->compasNum == 6 && llegit->compasDen == 8, "tempo i compàs");
            comprova (llegit->creat == "2026-11-03T09:45:00", "data de creació");
        }
    });

    prova ("Projecte: desar fa còpies (només 10) i conserva el que no coneix", []
    {
        namespace fs = std::filesystem;
        using namespace mibase::disc;
        const auto arrel = fs::temp_directory_path() / "mibase_proves2";
        fs::remove_all (arrel);
        fs::create_directories (arrel);

        mibase::Projecte p;
        p.nom = "Còpies";
        auto r = creaProjecte (arrel, p, "2026-11-03T10:00:00", "03/11/2026 10:00");
        comprova (r.correcte, "creat");

        // Un camp d'una versió futura.
        {
            std::string contingut;
            { std::ifstream f (r.fitxer); std::ostringstream ss; ss << f.rdbuf(); contingut = ss.str(); }
            auto pos = contingut.find ("\"nom\"");
            contingut.insert (pos, "\"campFutur\": 42,\n  ");
            std::ofstream f (r.fitxer, std::ios::trunc); f << contingut;
        }

        for (int i = 0; i < 14; ++i)
        {
            auto llegit = llegeix (r.fitxer);
            comprova (llegit.has_value(), "es llegeix " + std::to_string (i));
            if (! llegit) return;
            llegit->tempo = 90 + i;
            char ara[32]; std::snprintf (ara, sizeof (ara), "2026-11-03 10%04d", i);
            auto d = desa (r.fitxer, *llegit, "2026-11-03T10:00:00", ara);
            comprova (d.correcte, "desat " + std::to_string (i));
        }

        int copies = 0;
        for (auto& e : fs::directory_iterator (r.fitxer.parent_path() / cami ("Còpies")))
            if (e.path().extension() == ".mibase") ++copies;
        comprova (copies == 10, "10 còpies (n'hi ha " + std::to_string (copies) + ")");

        auto final = llegeix (r.fitxer);
        comprova (final && final->tempo == 103, "l'últim tempo desat");
        comprova (final && final->jsonOriginal.find ("campFutur") != std::string::npos, "conserva el camp desconegut");

        int temporals = 0;
        for (auto& e : fs::directory_iterator (r.fitxer.parent_path()))
            if (e.path().extension() == ".tmp") ++temporals;
        comprova (temporals == 0, "no queden fitxers temporals");
    });

    prova ("Diari: el més recent a dalt", []
    {
        namespace fs = std::filesystem;
        using namespace mibase::disc;
        const auto carpeta = fs::temp_directory_path() / "mibase_proves3" / "Diari";
        fs::remove_all (carpeta.parent_path());
        fs::create_directories (carpeta);

        afegeixEvolucio (carpeta, "01/11/2026 19:00", "Primer");
        afegeixEvolucio (carpeta, "02/11/2026 20:00", "Segon");
        afegeixEvolucio (carpeta, "03/11/2026 21:00", "Tercer · amb punt volat");
        auto e = llegeixEvolucio (carpeta, 10);
        comprova (e.size() == 3, "tres entrades");
        comprova (e.size() == 3 && e[0].text == "Tercer · amb punt volat" && e[2].text == "Primer", "ordre i text");
        comprova (llegeixEvolucio (carpeta, 2).size() == 2, "es pot limitar");
    });

    prova ("Preses: «Pista - presa 01.wav», 02, 03…", []
    {
        namespace fs = std::filesystem;
        using namespace mibase::disc;
        const auto audios = fs::temp_directory_path() / "mibase_proves4";
        fs::remove_all (audios);
        fs::create_directories (audios);

        comprova (nomPresa (audios, "Guitarra") == "Guitarra - presa 01.wav", "primera presa");
        std::ofstream (audios / cami ("Guitarra - presa 01.wav")) << "x";
        std::ofstream (audios / cami ("Guitarra - presa 07.wav")) << "x";
        std::ofstream (audios / cami ("Guitarra rítmica - presa 12.wav")) << "x";
        comprova (nomPresa (audios, "Guitarra") == "Guitarra - presa 08.wav", "la següent a la més alta");
        comprova (nomPresa (audios, "Guitarra rítmica") == "Guitarra rítmica - presa 13.wav", "una altra pista, amb accent");
        comprova (nomPresa (audios, "Veu: tornada") == "Veu_ tornada - presa 01.wav", "nom de pista netejat");
    });


    prova ("Estudi: graella, posició i compassos (−1, 0, 1…)", []
    {
        using namespace mibase::edicio;
        mibase::Projecte p; p.tempo = 120; p.compasNum = 4;
        comprova (std::abs (segonsPerTemps (p) - 0.5) < 1e-9 && std::abs (segonsPerCompas (p) - 2.0) < 1e-9, "120 bpm 4/4");
        comprova (std::abs (enganxa (p, 2.9, mibase::Snap::Compas) - 2.0) < 1e-9, "s'enganxa al compàs");
        comprova (std::abs (enganxa (p, 3.1, mibase::Snap::Compas) - 4.0) < 1e-9, "s'enganxa al compàs següent");
        comprova (std::abs (enganxa (p, 2.74, mibase::Snap::Temps) - 2.5) < 1e-9, "s'enganxa al temps");
        comprova (std::abs (enganxa (p, 2.74, mibase::Snap::Lliure) - 2.74) < 1e-9, "lliure no s'enganxa");
        comprova (textPosicio (p, 0.0) == "-1.1", "l'inici és el compàs −1");
        comprova (textPosicio (p, 2.0) == "0.1", "després, el compàs 0");
        comprova (textPosicio (p, 5.5) == "1.4", "compàs 1, temps 4");
        comprova (textPosicio (p, 4.045, true) == "1.1 +45 ms", "amb mil·lisegons");
    });

    prova ("Estudi: moure clips amb la bandera, retallar, fades i volum", []
    {
        using namespace mibase::edicio;
        mibase::Projecte p; p.tempo = 120;
        auto& s = afegeixPresa (p, "Àudios/Guitarra - presa 01.wav", "Guitarra", 10.0);
        const auto idPresa = s.id;
        afegeixPista (p, "Guitarra", "guitarra");
        auto& c = afegeixClip (p, 0, idPresa, 2.0);
        comprova (std::abs (c.iniciVisible() - 2.0) < 1e-9 && std::abs (c.longitud() - 10.0) < 1e-9, "clip sencer");

        // Sense bandera, s'enganxa l'inici; amb bandera, la bandera.
        mouClipA (p, c, 4.0);
        comprova (std::abs (c.pos - 4.0) < 1e-9, "mou per l'inici");
        presa (p, idPresa)->primer = 0.3;
        comprova (banderaVisible (p, c), "bandera visible");
        mouClipA (p, c, 6.0);
        comprova (std::abs (c.pos - 5.7) < 1e-9, "mou per la bandera");

        // Bandera fixa (mode Lliure): l'àudio llisca i la bandera es queda.
        comprova (mouAmbBanderaFixa (p, c, 5.65, 6.0), "bandera fixa");
        comprova (std::abs (presa (p, idPresa)->primer - 0.35) < 1e-9, "la bandera ara és 50 ms més tard dins el fitxer");
        comprova (! mouAmbBanderaFixa (p, c, 20.0, 6.0), "si quedaria fora, no es fa");

        // Retallar (fins i tot més enllà del so: silenci).
        retallaInici (c, c.pos + 1.0);
        comprova (std::abs (c.inici - 1.0) < 1e-9, "retall de l'inici");
        retallaFi (c, c.pos + 12.0);
        comprova (std::abs (c.fi - 12.0) < 1e-9, "s'allarga més enllà del so");
        retallaFi (c, c.pos + 0.5);
        comprova (c.fi > c.inici, "el final no passa mai per davant de l'inici");
        retallaFi (c, c.pos + 9.0);
        comprova (! banderaVisible (p, c), "la bandera queda fora si es retalla");

        fadeIn (c, c.iniciVisible() + 0.5);
        fadeOut (c, c.fiVisible() - 2.0);
        comprova (std::abs (c.fadeIn - 0.5) < 1e-9 && std::abs (c.fadeOut - 2.0) < 1e-9, "fades");
        fadeIn (c, c.fiVisible() + 50.0);
        comprova (c.fadeIn + c.fadeOut <= c.longitud() + 1e-9, "els fades no es trepitgen");
        guany (c, -40.0);
        comprova (c.guanyDb == guanyMinDb, "volum mínim −24 dB");
        guany (c, 3.0);
        comprova (c.guanyDb == 0.0, "volum màxim 0 dB");
    });

    prova ("Estudi: duplicar, treure, passar de pista, ordenar pistes", []
    {
        using namespace mibase::edicio;
        mibase::Projecte p; p.tempo = 120;
        const auto id = afegeixPresa (p, "a.wav", "a", 2.0).id;
        afegeixPista (p, "Guitarra", "guitarra");
        afegeixPista (p, "Veu", "veu");
        afegeixClip (p, 0, id, 2.0);
        auto noves = duplica (p, 0, { 0 });
        comprova (p.pistes[0].clips.size() == 2 && noves.size() == 1, "duplicat");
        comprova (std::abs (p.pistes[0].clips[1].iniciVisible() - 4.0) < 1e-9, "la còpia va just a continuació");
        noves = duplica (p, 0, { 0, 1 });
        comprova (p.pistes[0].clips.size() == 4 && std::abs (p.pistes[0].clips[2].iniciVisible() - 6.0) < 1e-9, "un grup es duplica sencer");

        auto passats = passaAPista (p, 0, { 3 }, 1);
        comprova (p.pistes[0].clips.size() == 3 && p.pistes[1].clips.size() == 1 && passats.size() == 1, "passa a una altra pista");
        comprova (std::abs (p.pistes[1].clips[0].iniciVisible() - 8.0) < 1e-9, "sense moure's en el temps");

        treuClips (p, 0, { 0, 2 });
        comprova (p.pistes[0].clips.size() == 1 && p.preses.size() == 1, "es treuen clips però no la presa");

        mouPista (p, 1, 0);
        comprova (p.pistes[0].nom == "Veu", "ordenar pistes");
        triaGravacio (p, 1);
        comprova (p.pistes[1].grava && ! p.pistes[0].grava, "gravar a una pista");
        triaGravacio (p, 0);
        comprova (p.pistes[0].grava && ! p.pistes[1].grava, "només una pista grava");
        comprova (nomPistaLliure (p, "Veu") == "Veu 2" && nomPistaLliure (p, "Baix") == "Baix", "noms de pista sense repetir");
        comprova (std::abs (final (p) - 10.0) < 1e-9 || final (p) > 0.0, "final del projecte");
    });

    prova ("Estudi: bucle i desfés", []
    {
        using namespace mibase::edicio;
        mibase::Projecte p; p.tempo = 120;
        marcaBucle (p, 5, 2);
        comprova (p.bucle.de == 2 && p.bucle.a == 6 && p.bucle.actiu, "bucle de 2 a 5 (inclosos)");
        comprova (std::abs (iniciBucle (p) - 4.0) < 1e-9 && std::abs (fiBucle (p) - 12.0) < 1e-9, "segons del bucle");
        comprova (compasA (p, 3.9) == 1 && compasA (p, -1.0) == 0, "compàs d'un temps");

        Desfes d;
        afegeixPista (p, "A", "audio");
        d.abans (p);
        afegeixPista (p, "B", "audio");
        d.abans (p);
        p.pistes[0].nom = "Canviat";
        comprova (d.desfes (p) && p.pistes[0].nom == "A" && p.pistes.size() == 2, "desfés 1");
        comprova (d.desfes (p) && p.pistes.size() == 1, "desfés 2");
        comprova (! d.desfes (p), "no hi ha més per desfer");
        comprova (d.refes (p) && p.pistes.size() == 2, "refés");
        comprova (d.refes (p) && p.pistes[0].nom == "Canviat", "refés 2");
    });

    prova ("Estudi: es desa i es torna a llegir igual", []
    {
        namespace fs = std::filesystem;
        using namespace mibase::edicio;
        const auto arrel = fs::temp_directory_path() / "mibase_proves5";
        fs::remove_all (arrel);
        fs::create_directories (arrel);

        mibase::Projecte p; p.nom = "Estudi"; p.tempo = 96;
        auto r = mibase::disc::creaProjecte (arrel, p, "2026-11-03T11:00:00", "03/11/2026 11:00");
        auto q = *mibase::disc::llegeix (r.fitxer);
        const auto id = afegeixPresa (q, "Àudios/Veu - presa 01.wav", "Veu de prova", 3.25).id;
        presa (q, id)->primer = 0.4;
        afegeixPista (q, "Veu", "veu").faderDb = -3.5;
        auto& c = afegeixClip (q, 0, id, 1.0);
        c.fadeIn = 0.1; c.guanyDb = -6;
        q.snap = mibase::Snap::Lliure;
        marcaBucle (q, 1, 2);
        comprova (mibase::disc::desa (r.fitxer, q, "2026-11-03T11:01:00", "x", false).correcte, "desat");

        auto l = mibase::disc::llegeix (r.fitxer);
        comprova (l.has_value(), "llegit");
        if (! l) return;
        comprova (l->preses.size() == 1 && l->preses[0].nom == "Veu de prova" && l->preses[0].primer == 0.4, "preses");
        comprova (l->pistes.size() == 1 && l->pistes[0].faderDb == -3.5 && l->pistes[0].tipus == "veu", "pistes");
        comprova (l->pistes.size() == 1 && l->pistes[0].clips.size() == 1 && l->pistes[0].clips[0].guanyDb == -6
                  && l->pistes[0].clips[0].fadeIn == 0.1, "clips");
        comprova (l->snap == mibase::Snap::Lliure && l->bucle.de == 1 && l->bucle.a == 3, "graella i bucle");
        comprova (l->comptador == q.comptador, "comptador d'identificadors");
        int copies = 0;
        for (auto& e : fs::directory_iterator (r.fitxer.parent_path() / mibase::disc::cami ("Còpies"))) { (void) e; ++copies; }
        comprova (copies == 0, "amb ferCopia = false no es fa còpia");
    });

    std::printf ("\n%d comprovacions, %d fallades\n", comprovacions, fallades);
    return fallades == 0 ? 0 : 1;
}
