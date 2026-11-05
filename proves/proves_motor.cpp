// Proves automàtiques del motor de Mibase.
// S'executen a qualsevol sistema (Linux a la sessió de Claude, Windows a GitHub Actions).

#include "mibase/ClicDeProva.h"
#include "mibase/CuaSenseBloqueig.h"
#include "mibase/MesuraLatencia.h"
#include "mibase/Mesurador.h"
#include "mibase/MotorDiagnostic.h"
#include "mibase/ProjecteDisc.h"
#include "mibase/Edicio.h"
#include "mibase/Gravadora.h"
#include "mibase/MotorEstudi.h"
#include "mibase/Wav.h"
#include "mibase/SinteSenzill.h"
#include "mibase/Midi.h"
#include "mibase/SinteIntern.h"
#include "mibase/Harmonia.h"
#include "mibase/PresetsPads.h"
#include "mibase/TeclatAcords.h"

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

    /** Instrument de prova: apunta els missatges que rep (amb la mostra absoluta) i fa so mentre té notes enceses. */
    struct InstrumentProva : Instrument
    {
        struct Rebut { long long mostra; int estat, d1, d2; };
        std::vector<Rebut> rebuts;
        long long mostra = 0;
        int enceses = 0, apagaTots = 0;
        void prepara (double, int) override { rebuts.reserve (100000); }
        void processa (const EventInstrument* e, int n, float* L, float* R, int m) noexcept override
        {
            for (int i = 0; i < n; ++i)
            {
                rebuts.push_back ({ mostra + e[i].offset, e[i].estat, e[i].dada1, e[i].dada2 });
                const int t = e[i].estat & 0xF0;
                if (t == 0x90 && e[i].dada2 > 0) ++enceses;
                else if (t == 0x80 || t == 0x90) --enceses;
            }
            for (int i = 0; i < m; ++i) { L[i] = enceses > 0 ? 0.1f : 0.0f; R[i] = L[i]; }
            mostra += m;
        }
        void apagaTot() noexcept override { ++apagaTots; enceses = 0; }
        bool fentSo() const noexcept override { return true; }   // sempre es processa: així el comptador de mostres és exacte
        int ons() const { int k = 0; for (auto& r : rebuts) if ((r.estat & 0xF0) == 0x90 && r.d2 > 0) ++k; return k; }
        int offs() const { int k = 0; for (auto& r : rebuts) if ((r.estat & 0xF0) == 0x80 || ((r.estat & 0xF0) == 0x90 && r.d2 == 0)) ++k; return k; }
    };
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


    prova ("Claqueta: quatre sons, volum i accent", []
    {
        for (int so = 0; so < ClicDeProva::numSons; ++so)
        {
            ClicDeProva clic;
            clic.prepara (48000.0);
            clic.setSo (so);
            clic.setTempsPerCompas (4);
            std::vector<float> b (96000, 0.0f);
            clic.processa (b.data(), (int) b.size());
            const std::string nom = "so " + std::to_string (so);
            comprova (totFinit (b), nom + ": valors vàlids");
            comprova (pic (b, 0, 3000) > 0.05f, nom + ": sona el primer temps");
            comprova (pic (b, 24000, 27000) > 0.03f, nom + ": sona el segon temps");
            comprova (pic (b, 0, 3000) > pic (b, 24000, 27000), nom + ": el primer temps és més fort");
            comprova (pic (b, 0, b.size()) <= 1.0f, nom + ": no satura");
            comprova (pic (b, 12000, 20000) < 0.001f, nom + ": silenci entre temps");

            ClicDeProva fluix;
            fluix.prepara (48000.0);
            fluix.setSo (so);
            fluix.setVolum (0.5f);
            std::vector<float> c (3000, 0.0f);
            fluix.processa (c.data(), (int) c.size());
            comprova (std::abs (pic (c, 0, c.size()) - 0.5f * pic (b, 0, 3000)) < 0.02f, nom + ": el volum funciona");
        }
    });

    prova ("Eines: llapis, tisores, cola i colors de pista", []
    {
        using namespace mibase::edicio;
        mibase::Projecte p; p.tempo = 120;
        const auto id = afegeixPresa (p, "a.wav", "a", 8.0).id;
        afegeixPista (p, "A", "audio");
        afegeixPista (p, "B", "audio");
        comprova (p.pistes[0].color != p.pistes[1].color, "pistes seguides, colors diferents");

        // Llapis.
        const int f = creaFinestra (p, 1, 4.0, 2.0);
        comprova (f == 0 && p.pistes[1].clips[0].presa.empty() && std::abs (p.pistes[1].clips[0].iniciVisible() - 2.0) < 1e-9
                  && std::abs (p.pistes[1].clips[0].longitud() - 2.0) < 1e-9, "finestra buida de 2 a 4");

        // Tisores.
        auto& c = afegeixClip (p, 0, id, 0.0);
        c.fadeIn = 0.5; c.fadeOut = 0.5;
        comprova (! talla (p, 0, 0, 0.0) && ! talla (p, 0, 0, 8.0), "no es talla a les vores");
        comprova (talla (p, 0, 0, 3.0), "tallat a 3 s");
        auto& cl = p.pistes[0].clips;
        comprova (cl.size() == 2 && std::abs (cl[0].fiVisible() - 3.0) < 1e-9 && std::abs (cl[1].iniciVisible() - 3.0) < 1e-9, "dos trossos");
        comprova (cl[0].fadeIn == 0.5 && cl[0].fadeOut == 0.0 && cl[1].fadeIn == 0.0 && cl[1].fadeOut == 0.5, "els fades queden a fora");

        // Cola: els dos trossos tornen a ser un (sense fitxer nou).
        auto r = enganxaClips (p, 0, 1, 0);
        comprova (r.fet && ! r.calConsolidar && cl.size() == 1 && std::abs (cl[0].longitud() - 8.0) < 1e-9, "cola de dos trossos contigus");

        // Amb un forat entre mig, cal un fitxer nou.
        talla (p, 0, 0, 3.0);
        cl[1].pos += 1.0;
        r = enganxaClips (p, 0, 0, 1);
        comprova (! r.fet && r.calConsolidar && cl.size() == 2, "amb forat, cal consolidar");

        // Preses diferents: cal consolidar.
        const auto id2 = afegeixPresa (p, "b.wav", "b", 2.0).id;
        afegeixClip (p, 1, id2, 4.0);
        r = enganxaClips (p, 1, 0, 1);
        comprova (r.fet && p.pistes[1].clips.size() == 1, "finestra buida + àudio just al costat: s'ajunten");
        comprova (std::abs (p.pistes[1].clips[0].iniciVisible() - 2.0) < 1e-9 && std::abs (p.pistes[1].clips[0].fiVisible() - 6.0) < 1e-9,
                  "l'àudio s'allarga amb silenci");

        // Dues finestres buides.
        creaFinestra (p, 1, 10.0, 11.0);
        creaFinestra (p, 1, 13.0, 14.0);
        r = enganxaClips (p, 1, 1, 2);
        comprova (r.fet && std::abs (p.pistes[1].clips[1].longitud() - 4.0) < 1e-9, "dues finestres buides en fan una");
    });

    prova ("Projecte: nova versió i exportar (recopilar) amb només els àudios que calen", []
    {
        namespace fs = std::filesystem;
        using namespace mibase::edicio;
        const auto arrel = fs::temp_directory_path() / "mibase_proves6";
        fs::remove_all (arrel);
        fs::create_directories (arrel / "desti");

        mibase::Projecte p; p.nom = "Cançó";
        auto r = mibase::disc::creaProjecte (arrel, p, "2026-11-03T12:00:00", "03/11/2026 12:00");
        auto q = *mibase::disc::llegeix (r.fitxer);
        const auto carpeta = r.fitxer.parent_path();
        std::ofstream (carpeta / mibase::disc::cami ("Àudios") / mibase::disc::cami ("Usada - presa 01.wav")) << "u";
        std::ofstream (carpeta / mibase::disc::cami ("Àudios") / mibase::disc::cami ("Vella - presa 01.wav")) << "v";
        const auto usada = afegeixPresa (q, "Àudios/Usada - presa 01.wav", "usada", 1.0).id;
        afegeixPresa (q, "Àudios/Vella - presa 01.wav", "vella", 1.0);
        afegeixPista (q, "Veu", "audio").canals = 2;
        afegeixClip (q, 0, usada, 0.0).mut = true;
        q.clicSo = 2; q.clicVolum = 0.7;
        mibase::disc::desa (r.fitxer, q, "x", "x", false);

        auto v = mibase::disc::novaVersio (r.fitxer, q, "Cançó v2", "2026-11-03T12:01:00", "03/11/2026 12:01");
        comprova (v.correcte && fs::exists (v.fitxer), "nova versió creada");
        auto lv = mibase::disc::llegeix (v.fitxer);
        comprova (lv && lv->nom == "Cançó v2" && lv->pistes.size() == 1, "la versió té el mateix contingut i el nom nou");
        comprova (! mibase::disc::novaVersio (r.fitxer, q, "Cançó v2", "x", "x").correcte, "no trepitja una versió que ja hi és");
        comprova (mibase::disc::llegeixEvolucio (carpeta, 1)[0].text.find ("Nova versió") == 0, "s'anota al diari");

        auto e = mibase::disc::recopila (carpeta, q, arrel / "desti", "2026-11-03T12:02:00", "03/11/2026 12:02");
        comprova (e.correcte && e.avisos.empty(), "recopilat (" + e.error + ")");
        const auto nova = arrel / "desti" / mibase::disc::cami ("Cançó");
        comprova (fs::exists (nova / mibase::disc::cami ("Àudios") / mibase::disc::cami ("Usada - presa 01.wav")), "hi ha l'àudio que es fa servir");
        comprova (! fs::exists (nova / mibase::disc::cami ("Àudios") / mibase::disc::cami ("Vella - presa 01.wav")), "no hi ha l'àudio obsolet");
        auto le = mibase::disc::llegeix (e.fitxer);
        comprova (le && le->preses.size() == 1 && le->pistes[0].canals == 2 && le->pistes[0].clips[0].mut, "el projecte recopilat està sanejat i complet");
        comprova (le && le->clicSo == 2 && std::abs (le->clicVolum - 0.7) < 1e-9, "la claqueta es desa");
        comprova (! mibase::disc::recopila (carpeta, q, arrel / "desti", "x", "x").correcte, "no trepitja una carpeta que ja hi és");
    });


    // ======================= 0.5: sonar, gravar i exportar =======================
    using namespace mibase::edicio;
    namespace fs5 = std::filesystem;
    const auto arrel5 = fs5::temp_directory_path() / mibase::disc::cami ("mibase-proves-0.5 àèí");

    prova ("0.6: copiar i enganxar (trepitjant i inserint), buidar un tros, moure de pista", [&]
    {
        using mibase::edicio::RefClip;
        Projecte q;
        q.tempo = 120.0;   // 2 s per compàs
        afegeixPista (q, "A", "audio");
        afegeixPista (q, "B", "audio");
        afegeixPista (q, "C", "audio");
        const auto s1 = afegeixPresa (q, "Àudios/a.wav", "a", 8.0).id;
        afegeixClip (q, 0, s1, 0.0);           // A: 0..8
        afegeixClip (q, 1, s1, 2.0).fi = 2.0;  // B: 2..4

        auto r = mibase::edicio::copiaRang (q, 0, 1, 1.0, 3.0);
        comprova (std::abs (r.durada - 2.0) < 1e-9 && r.clips.size() == 2, "el tros té els dos clips retallats");
        comprova (std::abs (r.clips[0].second.iniciVisible() - 0.0) < 1e-9 && std::abs (r.clips[0].second.fiVisible() - 2.0) < 1e-9
                  && std::abs (r.clips[1].second.iniciVisible() - 1.0) < 1e-9 && std::abs (r.clips[1].second.fiVisible() - 2.0) < 1e-9,
                  "les posicions són relatives al començament del tros");

        auto c = mibase::edicio::copiaClips (q, { { 1, 0 } });
        comprova (std::abs (c.durada - 2.0) < 1e-9 && c.clips.size() == 1 && c.clips[0].first == 1, "copia el clip sencer amb la seva pista");

        auto q2 = q;
        auto nous = mibase::edicio::enganxaRetall (q2, c, 5.0, false);
        comprova (nous.size() == 1 && nous[0].pista == 1 && std::abs (q2.pistes[1].clips[(size_t) nous[0].clip].iniciVisible() - 5.0) < 1e-9,
                  "enganxa al cursor, a la mateixa pista");
        nous = mibase::edicio::enganxaRetall (q2, mibase::edicio::copiaRang (q, 0, 0, 4.0, 6.0), 1.0, false);
        const auto& pa = q2.pistes[0].clips;
        comprova (pa.size() == 3, "trepitjant: el clip de sota es parteix (" + std::to_string (pa.size()) + ")");
        bool forat = true;
        for (size_t k = 0; k + 1 < pa.size(); ++k)
            if (pa[k].iniciVisible() < 3.0 - 1e-9 && pa[k].fiVisible() > 1.0 + 1e-9) forat = false;
        comprova (forat, "i sota el tros enganxat no hi queda res");

        auto q3 = q;
        nous = mibase::edicio::enganxaRetall (q3, c, 2.0, true);
        comprova (q3.pistes[0].clips.size() == 2 && std::abs (q3.pistes[0].clips[1].iniciVisible() - 4.0) < 1e-9
                  && std::abs (q3.pistes[0].clips[1].fiVisible() - 10.0) < 1e-9, "inserint: a totes les pistes, el que hi ha després s'aparta");
        comprova (std::abs (q3.pistes[1].clips[0].iniciVisible() - 4.0) < 1e-9 && std::abs (q3.pistes[1].clips[1].iniciVisible() - 2.0) < 1e-9,
                  "el clip de B s'aparta i el nou ocupa el seu lloc");

        auto q4 = q;
        mibase::edicio::buidaRang (q4, 0, 3.0, 5.0);
        comprova (q4.pistes[0].clips.size() == 2 && std::abs (q4.pistes[0].clips[0].fiVisible() - 3.0) < 1e-9
                  && std::abs (q4.pistes[0].clips[1].iniciVisible() - 5.0) < 1e-9, "buidar un tros del mig parteix el clip");

        auto q5 = q;
        auto moguts = mibase::edicio::mouDePista (q5, { { 0, 0 }, { 1, 0 } }, 5);
        comprova (moguts.size() == 2 && moguts[0].pista + moguts[1].pista == 3 && q5.pistes[0].clips.empty(), "es mou tan avall com es pot (una pista)");
        auto dup = mibase::edicio::duplicaTriats (q5, moguts);
        comprova (dup.size() == 2 && q5.pistes[1].clips.size() == 2 && q5.pistes[2].clips.size() == 2, "duplica a cada pista");
        mibase::edicio::treuTriats (q5, dup);
        comprova (q5.pistes[1].clips.size() == 1 && q5.pistes[2].clips.size() == 1, "treu de diverses pistes");
    });
    fs5::remove_all (arrel5);
    fs5::create_directories (arrel5);

    auto audioConstant = [] (double fs, double segons, float valor, int canals = 1)
    {
        auto a = std::make_shared<AudioPresa>();
        a->fs = fs;
        a->canals = canals;
        a->longitud = (long long) std::llround (fs * segons);
        for (int c = 0; c < canals; ++c) a->dades[c].assign ((size_t) a->longitud, c == 0 ? valor : -valor);
        return a;
    };

    prova ("WAV: escriure i tornar a llegir (24 bits, estèreo, accents al camí)", [&]
    {
        EscriptorWav w;
        const auto f = arrel5 / mibase::disc::cami ("prova ç.wav");
        comprova (w.obre (f, 2, 44100.0), "s'obre");
        std::vector<float> d;
        for (int i = 0; i < 1001; ++i) { d.push_back ((float) i / 1001.0f); d.push_back (-0.25f); }
        comprova (w.escriu (d.data(), 1001), "s'escriu");
        comprova (w.tanca(), "es tanca");
        auto r = llegeixWav (f);
        comprova (r.correcte && r.canals == 2 && std::abs (r.fs - 44100.0) < 0.5 && r.frames() == 1001, "capçalera correcta");
        float err = 0.0f;
        for (size_t i = 0; i < d.size(); ++i) err = std::max (err, std::abs (d[i] - r.mostres[i]));
        comprova (err < 1.0e-5f, "les mostres hi són (error " + std::to_string (err) + ")");
    });

    prova ("Pla: silenci, solo, clips sense àudio, canal de la taula", [&]
    {
        Projecte q;
        afegeixPista (q, "A", "audio");
        afegeixPista (q, "B", "audio");
        afegeixPista (q, "C", "audio");
        const auto s1 = afegeixPresa (q, "Àudios/a.wav", "a", 1.0).id;
        afegeixClip (q, 0, s1, 0.0);
        afegeixClip (q, 1, s1, 0.0).mut = true;
        afegeixClip (q, 2, "res", 0.0);
        q.pistes[1].solo = true;
        q.pistes[2].solo = true;
        q.pistes[2].mut = true;
        MapaAudios m { { s1, audioConstant (48000.0, 1.0, 0.5f) } };
        auto pla = construeixPla (q, m);
        comprova (pla->pistes.size() == 3, "tres pistes");
        comprova (! pla->pistes[0].sona && pla->pistes[1].sona && ! pla->pistes[2].sona, "solo i silenci");
        comprova (pla->pistes[0].clips.size() == 1 && pla->pistes[1].clips.empty() && pla->pistes[2].clips.empty(),
                  "els clips silenciats i sense àudio no hi són");
        comprova (pla->audios.size() == 1, "l'àudio es guarda una vegada");
        comprova (slotDePista (q.pistes[0]) != slotDePista (q.pistes[1]), "cada pista el seu canal");
    });

    prova ("Mesclador: lloc, volum, pan, fade i freqüència diferent", [&]
    {
        Projecte q;
        afegeixPista (q, "A", "audio");
        const auto s1 = afegeixPresa (q, "Àudios/a.wav", "a", 1.0).id;
        afegeixClip (q, 0, s1, 1.0);
        q.clic = false;
        MapaAudios m { { s1, audioConstant (48000.0, 1.0, 0.5f) } };

        auto sona = [&] (const Projecte& pr, double fsSortida, std::vector<float>& L, std::vector<float>& R)
        {
            auto pla = construeixPla (pr, m);
            Mesclador mx;
            mx.prepara (fsSortida, 512);
            const int total = (int) (fsSortida * 2.5);
            L.assign ((size_t) total, 0.0f);
            R.assign ((size_t) total, 0.0f);
            for (int p = 0; p < total; p += 512)
                mx.processa (*pla, p, std::min (512, total - p), L.data() + p, R.data() + p, false, nullptr);
        };

        std::vector<float> L, R;
        sona (q, 48000.0, L, R);
        comprova (pic (L, 0, 48000) == 0.0f, "res abans del clip");
        comprova (std::abs (L[60000] - 0.5f) < 1.0e-6f && std::abs (R[60000] - 0.5f) < 1.0e-6f, "el clip sona a les dues bandes");
        comprova (pic (L, 96001, L.size()) == 0.0f, "res després del clip");
        comprova (std::abs (L[47999]) == 0.0f && std::abs (L[48000] - 0.5f) < 1.0e-6f, "comença a la mostra exacta");

        auto q2 = q;
        q2.pistes[0].faderDb = -6.0;
        q2.pistes[0].pan = -1.0;
        sona (q2, 48000.0, L, R);
        comprova (std::abs (L[60000] - 0.5f * 0.50119f) < 1.0e-4f, "el fader baixa 6 dB");
        comprova (pic (R, 0, R.size()) == 0.0f, "tot a l'esquerra: la dreta no sona");

        auto q3 = q;
        q3.pistes[0].clips[0].fadeIn = 0.5;
        sona (q3, 48000.0, L, R);
        comprova (std::abs (L[48000]) < 0.01f && std::abs (L[48000 + 12000] - 0.25f) < 0.01f && std::abs (L[48000 + 30000] - 0.5f) < 1.0e-6f,
                  "el fade d'entrada puja de 0 al volum");

        sona (q, 44100.0, L, R);
        comprova (pic (L, 0, 44099) == 0.0f && std::abs (L[44100 + 1000] - 0.5f) < 1.0e-5f && pic (L, 88202, L.size()) == 0.0f,
                  "un àudio de 48 kHz sona al seu lloc amb la targeta a 44,1 kHz");

        auto q4 = q;
        q4.pistes[0].canals = 2;
        m[s1] = audioConstant (48000.0, 1.0, 0.5f, 2);
        sona (q4, 48000.0, L, R);
        comprova (std::abs (L[60000] - 0.5f) < 1.0e-6f && std::abs (R[60000] + 0.5f) < 1.0e-6f, "pista estèreo: cada canal al seu costat");
        q4.pistes[0].canals = 1;
        sona (q4, 48000.0, L, R);
        comprova (std::abs (L[60000]) < 1.0e-6f, "pista mono amb àudio estèreo: es barregen els canals");
    });

    prova ("Clips un damunt de l'altre: sona el de dalt", [&]
    {
        Projecte q;
        q.clic = false;
        afegeixPista (q, "A", "audio");
        const auto s1 = afegeixPresa (q, "Àudios/a.wav", "a", 4.0).id;
        const auto s2 = afegeixPresa (q, "Àudios/b.wav", "b", 1.0).id;
        afegeixClip (q, 0, s1, 0.0);
        afegeixClip (q, 0, s2, 1.0);
        MapaAudios m { { s1, audioConstant (48000.0, 4.0, 0.25f) }, { s2, audioConstant (48000.0, 1.0, 0.5f) } };
        auto sona = [&]
        {
            auto pla = construeixPla (q, m);
            Mesclador mx;
            mx.prepara (48000.0, 1000);
            std::vector<float> L (48000 * 4, 0.0f), R (48000 * 4, 0.0f);
            for (int p = 0; p < 48000 * 4; p += 1000)
                mx.processa (*pla, p, 1000, L.data() + p, R.data() + p, false, nullptr);
            return L;
        };
        auto L = sona();
        comprova (std::abs (L[24000] - 0.25f) < 1.0e-6f && std::abs (L[72000] - 0.5f) < 1.0e-6f && std::abs (L[120000] - 0.25f) < 1.0e-6f,
                  "abans i després sona el de sota; al mig, només el de dalt (sense sumar)");
        q.pistes[0].clips[1].mut = true;
        L = sona();
        comprova (std::abs (L[72000] - 0.25f) < 1.0e-6f, "si el de dalt està silenciat, torna a sonar el de sota");
    });

    prova ("Claqueta enganxada als temps (blocs de mida rara)", [&]
    {
        Projecte q;
        q.tempo = 120.0;
        q.compasNum = 3;
        auto pla = construeixPla (q, {});
        Mesclador mx;
        mx.prepara (48000.0, 333);
        const int total = 24000 * 7;
        std::vector<float> L ((size_t) total, 0.0f), R ((size_t) total, 0.0f);
        for (int p = 0; p < total; p += 333)
            mx.processa (*pla, p, std::min (333, total - p), L.data() + p, R.data() + p, true, nullptr);
        std::vector<int> cops;
        for (int i = 0; i < total; ++i)
            if (L[(size_t) i] != 0.0f && (i == 0 || L[(size_t) i - 1] == 0.0f) && (cops.empty() || i - cops.back() > 5000))
                cops.push_back (i);
        bool llocs = cops.size() == 7;
        for (size_t k = 0; llocs && k < cops.size(); ++k)
            llocs = std::abs (cops[k] - (int) k * 24000) <= 1;
        comprova (llocs, "set cops, un a cada temps (" + std::to_string (cops.size()) + ")");
        comprova (pic (L, 0, 2000) > pic (L, 24000, 26000) * 1.2f && pic (L, 72000, 74000) > pic (L, 24000, 26000) * 1.2f,
                  "el primer temps de cada compàs (de 3) és més fort");
    });

    prova ("Motor: sona on toca, situa i bucle", [&]
    {
        Projecte q;
        q.clic = false;
        afegeixPista (q, "A", "audio");
        const auto s1 = afegeixPresa (q, "Àudios/a.wav", "a", 1.0).id;
        afegeixClip (q, 0, s1, 1.0);
        MapaAudios m { { s1, audioConstant (48000.0, 1.0, 0.5f) } };

        MotorEstudi motor;
        motor.prepara (48000.0, 256);
        motor.publica (construeixPla (q, m));
        std::vector<float> o0 (256), o1 (256);
        float* sort[2] { o0.data(), o1.data() };
        auto bloc = [&]
        {
            std::fill (o0.begin(), o0.end(), 0.0f);
            std::fill (o1.begin(), o1.end(), 0.0f);
            motor.processa (nullptr, 0, sort, 2, 256);
        };

        bloc();
        comprova (motor.getEstat() == MotorEstudi::Estat::Aturat && pic (o0, 0, 256) == 0.0f, "aturat no sona");
        motor.reprodueix (1.0);
        bloc();
        comprova (motor.getEstat() == MotorEstudi::Estat::Sonant && std::abs (o0[0] - 0.5f) < 1.0e-6f, "comença a sonar al punt demanat");
        comprova (std::abs (motor.getPosicio() - (1.0 + 256.0 / 48000.0)) < 1.0e-9, "la posició avança");
        motor.situa (3.0);
        bloc();
        comprova (pic (o0, 0, 256) == 0.0f && motor.getPosicio() > 3.0, "situa salta");
        comprova (motor.vuSortida (0).llegeixIReinicia() > 0.4f, "el vúmetre de la sortida s'ha mogut");
        motor.atura();
        bloc();
        comprova (motor.getEstat() == MotorEstudi::Estat::Aturat, "s'atura");

        // Bucle del compàs 0 (de 0 a 2 s a 120 bpm, 4/4).
        q.bucle = { true, true, 0, 1 };
        motor.publica (construeixPla (q, m));
        motor.reprodueix (1.5);
        for (int i = 0; i < 400; ++i) bloc();   // uns 2,1 s
        comprova (motor.getPosicio() < 2.0 && motor.getPosicio() >= 0.0, "amb el bucle encès torna a començar");
        motor.atura();
        bloc();
        for (int i = 0; i < 10; ++i) { motor.publica (construeixPla (q, m)); motor.recull(); }
        bloc();
        motor.recull();
        comprova (true, "publicar i recollir molts plans no peta");
    });

    prova ("Gravar: compte d'entrada, entrada triada i fitxer", [&]
    {
        Projecte q;
        q.clic = true;
        auto pla = construeixPla (q, {});
        MotorEstudi motor;
        motor.prepara (48000.0, 480);
        motor.setMapaEntrades ({ 0, 1 });
        motor.publica (std::move (pla));

        const auto f = arrel5 / mibase::disc::cami ("Guitarra - presa 01.wav");
        comprova (motor.gravadora().comenca (f, 1, 48000.0), "la gravadora s'engega");
        motor.grava (0.0, 0.5, 1, 1);

        std::vector<float> e0 (480, 0.9f), e1 (480), o0 (480), o1 (480);
        const float* ent[2] { e0.data(), e1.data() };
        float* sort[2] { o0.data(), o1.data() };
        long long n = 0;
        bool comptava = false, gravava = false;
        for (int b = 0; b < 100; ++b)   // 1 s
        {
            for (int i = 0; i < 480; ++i) e1[(size_t) i] = (float) ((n + i) % 1000) / 1000.0f - 0.5f;
            std::fill (o0.begin(), o0.end(), 0.0f);
            motor.processa (ent, 2, sort, 2, 480);
            n += 480;
            if (b == 10) comptava = motor.getEstat() == MotorEstudi::Estat::Comptant;
            if (b == 80) gravava = motor.getEstat() == MotorEstudi::Estat::Gravant;
        }
        comprova (comptava && gravava, "primer compta i després grava");
        comprova (motor.vuEntrada (1).llegeixIReinicia() > 0.4f, "el vúmetre de l'entrada 2 es mou");
        motor.atura();
        motor.processa (ent, 2, sort, 2, 480);
        comprova (motor.getEstat() == MotorEstudi::Estat::Aturat, "s'atura");
        const auto r = motor.gravadora().acaba();
        comprova (r.correcte && r.frames == 48000 && r.perdudes == 0, "s'ha gravat 1 s sencer (" + std::to_string (r.frames) + ")");
        auto w = llegeixWav (f);
        bool igual = w.correcte && w.frames() == 48000;
        for (long long i = 0; igual && i < 48000; ++i)
            igual = std::abs (w.mostres[(size_t) i] - ((float) (i % 1000) / 1000.0f - 0.5f)) < 1.0e-5f;
        comprova (igual, "el fitxer té exactament el que entrava per l'entrada 2");
    });

    prova ("Gravació a la pista: bandera, finestra i retard", [&]
    {
        Projecte q;
        afegeixPista (q, "Guitarra", "audio");
        const int c = mibase::edicio::afegeixGravacio (q, 0, "Àudios/Guitarra - presa 01.wav", "Guitarra - presa 01", 6.0, 2.0, 4.0, 0.010);
        comprova (c == 0 && q.preses.size() == 1, "s'afegeix la presa i el clip");
        const auto& k = q.pistes[0].clips[0];
        comprova (std::abs (k.pos - 1.990) < 1.0e-9 && std::abs (k.iniciVisible() - 4.0) < 1.0e-9 && std::abs (k.fiVisible() - 7.990) < 1.0e-9,
                  "es mou enrere el retard i la finestra comença al cursor");
        comprova (std::abs (q.preses[0].primer - 2.010) < 1.0e-9, "la bandera és al cursor");
        comprova (mibase::edicio::afegeixGravacio (q, 0, "x.wav", "x", 1.5, 2.0, 4.0, 0.0) == -1 && q.preses.size() == 1,
                  "si s'atura durant el compte, no queda res");
    });

    prova ("Exportar la mescla", [&]
    {
        Projecte q;
        afegeixPista (q, "A", "audio");
        afegeixPista (q, "B", "audio");
        const auto s1 = afegeixPresa (q, "Àudios/a.wav", "a", 1.0).id;
        afegeixClip (q, 0, s1, 2.0);
        afegeixClip (q, 1, s1, 2.5);
        q.pistes[1].pan = 1.0;
        auto tros = trosPerExportar (q);
        comprova (std::abs (tros.first - 2.0) < 1.0e-9 && std::abs (tros.second - 3.5) < 1.0e-9, "del primer clip al darrer");
        q.pistes[1].mut = true;
        comprova (std::abs (trosPerExportar (q).second - 3.0) < 1.0e-9, "les pistes silenciades no compten");
        q.pistes[1].mut = false;

        MapaAudios m { { s1, audioConstant (48000.0, 1.0, 0.25f) } };
        std::string error;
        const auto f = arrel5 / mibase::disc::cami ("Cançó - mescla.wav");
        comprova (exportaMescla (*construeixPla (q, m), 48000.0, tros.first, tros.second, f, error), "s'exporta (" + error + ")");
        auto w = llegeixWav (f);
        comprova (w.correcte && w.canals == 2 && w.frames() == 72000, "estèreo i de la llargada justa");
        comprova (w.correcte && std::abs (w.mostres[2 * 12000] - 0.25f) < 1.0e-4f && std::abs (w.mostres[2 * 12000 + 1] - 0.25f) < 1.0e-4f,
                  "al principi només sona A (al mig)");
        comprova (w.correcte && std::abs (w.mostres[2 * 36000] - 0.25f) < 1.0e-4f && std::abs (w.mostres[2 * 36000 + 1] - 0.5f) < 1.0e-4f,
                  "després s'hi suma B (a la dreta)");
        q.bucle = { true, true, 1, 2 };
        comprova (std::abs (trosPerExportar (q).first - 2.0) < 1.0e-9 && std::abs (trosPerExportar (q).second - 4.0) < 1.0e-9, "amb el bucle encès, el bucle");
    });

    prova ("Projecte al disc: pan, entrada i volum de sortida", [&]
    {
        Projecte nou;
        nou.nom = "Pan";
        auto r = mibase::disc::creaProjecte (arrel5, nou, "x", "x");
        auto q = *mibase::disc::llegeix (r.fitxer);
        afegeixPista (q, "A", "audio");
        q.pistes[0].pan = -0.4;
        q.pistes[0].entrada = 1;
        q.masterDb = -3.0;
        q.pistes[0].rgb = 0xff112233;
        mibase::disc::desa (r.fitxer, q, "x", "x", false);
        auto l = mibase::disc::llegeix (r.fitxer);
        comprova (l && std::abs (l->pistes[0].pan + 0.4) < 1.0e-9 && l->pistes[0].entrada == 1 && std::abs (l->masterDb + 3.0) < 1.0e-9
                  && l->pistes[0].rgb == 0xff112233,
                  "es desen i es llegeixen");
    });
    fs5::remove_all (arrel5);


    // ======================================================================
    // MIDI i piano roll (0.7)
    // ======================================================================
    prova ("MIDI: noms de les notes, tecles negres i graella", []
    {
        comprova (midi::nomNota (60) == "C4" && midi::nomNota (69) == "A4" && midi::nomNota (61) == "C#4" && midi::nomNota (0) == "C-1", "noms (do central = C4)");
        comprova (midi::esNegra (61) && ! midi::esNegra (60) && midi::esNegra (70) && ! midi::esNegra (64), "tecles negres");
        comprova (midi::enganxa (500, 480) == 480 && midi::enganxa (730, 480) == 960 && midi::enganxa (731, 0) == 731, "enganxa a la graella");
        comprova (midi::enganxaAvall (959, 480) == 480, "enganxa cap avall");
        comprova (midi::valorsGraella()[3].ticks == ticksPerNegra && midi::valorsGraella()[8].ticks == ticksPerNegra / 3, "negra i corxera de treset");
    });

    prova ("MIDI: afegir, moure, durada, quantitzar, transposar", []
    {
        Clip c = midi::clipNou (0.0, 2.0);
        comprova (c.midi && c.notes.empty(), "clip MIDI buit");
        midi::afegeix (c, { 960, 480, 64, 100, 0 });
        midi::afegeix (c, { 0, 480, 60, 90, 0 });
        const int k = midi::afegeix (c, { 480, 480, 62, 80, 0 });
        comprova (c.notes.size() == 3 && c.notes[0].to == 60 && c.notes[1].to == 62 && c.notes[2].to == 64 && k == 1, "ordenades per inici");

        std::vector<int> t { 2 };
        midi::mou (c, t, -1000, 3);
        comprova (c.notes[(size_t) t[0]].ini == 960 - 960 && c.notes[(size_t) t[0]].to == 67, "es mou (i no passa del començament)");
        comprova (t.size() == 1 && c.notes[(size_t) t[0]].to == 67, "la selecció segueix la nota després d'ordenar");

        std::vector<int> totes { 0, 1, 2 };
        midi::mou (c, totes, 0, 100);
        int maxim = 0; for (auto& n : c.notes) maxim = std::max (maxim, n.to);
        comprova (maxim == 127, "el grup no passa de 127 (s'atura tot junt)");
        midi::mou (c, totes, 0, -(maxim - 67));

        Clip q = midi::clipNou (0.0, 4.0);
        midi::afegeix (q, { 470, 300, 60, 100, 0 });
        midi::afegeix (q, { 1000, 300, 62, 100, 0 });
        std::vector<int> s2 { 0, 1 };
        midi::quantitza (q, s2, 480);
        comprova (q.notes[0].ini == 480 && q.notes[1].ini == 960 && q.notes[0].dur == 300, "quantitza l'inici (la durada es manté)");
        midi::quantitza (q, s2, 480, 1.0, true);
        comprova (q.notes[0].dur == 480, "quantitza també el final");
        q.notes[0].ini = 500;
        midi::quantitza (q, s2, 480, 0.5);
        comprova (q.notes[0].ini == 490, "força del 50 %: la meitat del camí");

        std::vector<int> u { 0 };
        midi::canviaDurada (q, u, 100);
        comprova (q.notes[0].dur == 580, "allarga");
        midi::canviaDurada (q, u, -10000);
        comprova (q.notes[0].dur == 1, "no queda mai a zero");
        q.notes[0].dur = 480; q.notes[0].ini = 480;
        midi::canviaDurada (q, u, -240, true);
        comprova (q.notes[(size_t) u[0]].ini == 240 && q.notes[(size_t) u[0]].dur == 720, "allarga per l'inici (el final no es mou)");

        std::vector<int> v { 0, 1 };
        midi::transposa (q, v, 12);
        comprova (q.notes[0].to == 72 && q.notes[1].to == 74, "transposa una octava");
        midi::velocitat (q, v, 200);
        comprova (q.notes[0].vel == 127, "velocitat limitada a 127");
        midi::sumaVelocitat (q, v, -500);
        comprova (q.notes[0].vel == 1, "velocitat mínima 1");
    });

    prova ("MIDI: duplicar, copiar, enganxar, notes dins un requadre i trepitjades", []
    {
        Clip c = midi::clipNou (0.0, 8.0);
        midi::afegeix (c, { 0, 400, 60, 100, 0 });
        midi::afegeix (c, { 480, 400, 64, 100, 0 });
        auto d = midi::duplica (c, { 0, 1 }, 960);
        comprova (c.notes.size() == 4 && d.size() == 2 && c.notes[(size_t) d[0]].ini == 960 && c.notes[(size_t) d[1]].ini == 1440,
                  "duplica just després, arrodonit a la graella");
        auto r = midi::copia (c, { 1, 2 });
        comprova (r.notes.size() == 2 && r.notes[0].ini == 0 && r.notes[1].ini == 480, "copia (relatiu a la primera)");
        auto e = midi::enganxa (c, r, 3840);
        comprova (c.notes.size() == 6 && c.notes[(size_t) e[0]].ini == 3840, "enganxa on es demana");
        auto dins = midi::notesDins (c, 0, 960, 60, 60);
        comprova (dins.size() == 1 && c.notes[(size_t) dins[0]].to == 60, "notes dins un requadre");

        Clip t = midi::clipNou (0.0, 4.0);
        midi::afegeix (t, { 0, 960, 60, 100, 0 });
        midi::afegeix (t, { 480, 960, 60, 100, 0 });
        midi::afegeix (t, { 480, 100, 62, 100, 0 });
        auto tr = midi::trepitjades (t.notes);
        comprova (tr[0] && tr[2] == false && std::count (tr.begin(), tr.end(), true) == 2, "dues notes iguals que es trepitgen");
        midi::esborra (t, { 0, 2 });
        comprova (t.notes.size() == 1 && t.notes[0].ini == 480 && t.notes[0].to == 60, "esborra");
    });

    prova ("MIDI: clips (llapis, tisores, cola, moure de pista, enganxar i canvi de tempo)", []
    {
        Projecte p;
        p.tempo = 120.0;
        afegeixPista (p, "Àudio", "audio");
        auto& m = afegeixPista (p, "Piano", "midi");
        m.instrument = "intern:piano";
        const int k = edicio::creaFinestra (p, 1, 2.0, 4.0);
        auto& c = p.pistes[1].clips[(size_t) k];
        comprova (c.midi, "el llapis a una pista MIDI fa un clip MIDI");
        midi::afegeix (c, { 0, 480, 60, 100, 0 });          // 2,0 s
        midi::afegeix (c, { 1920, 480, 62, 100, 0 });       // 3,0 s
        comprova (std::abs (midi::segonsAbs (p, c, 1920) - 3.0) < 1e-9, "ticks a segons (120 bpm: 960 ticks = 0,5 s)");

        comprova (edicio::talla (p, 1, 0, 3.0) && p.pistes[1].clips.size() == 2 && p.pistes[1].clips[1].notes.size() == 2,
                  "les tisores parteixen la finestra (les notes es conserven a les dues meitats)");
        auto rc = edicio::enganxaClips (p, 1, 0, 1);
        comprova (rc.fet && p.pistes[1].clips.size() == 1 && p.pistes[1].clips[0].notes.size() == 2, "la cola torna a ajuntar les notes");

        auto res = edicio::mouDePista (p, { { 1, 0 } }, -1);
        comprova (res.size() == 1 && res[0].pista == 1 && p.pistes[0].clips.empty(), "un clip MIDI no passa a una pista d'àudio");

        auto ret = edicio::copiaClips (p, { { 1, 0 } });
        auto nous = edicio::enganxaRetall (p, ret, 10.0, false);
        comprova (nous.size() == 1 && p.pistes[1].clips.size() == 2 && p.pistes[1].clips[1].midi, "copiar i enganxar clips MIDI");

        // Canvi de tempo: la nota de 3,0 s (compàs intern 1, temps 3) queda al mateix compàs i temps.
        const double abans = edicio::segonsPerCompas (p);
        const auto& c0 = p.pistes[1].clips[0];
        const double compasAbans = midi::segonsAbs (p, c0, 1920) / abans;
        midi::canviaTempo (p, 60.0);
        const double compasDespres = midi::segonsAbs (p, p.pistes[1].clips[0], 1920) / edicio::segonsPerCompas (p);
        comprova (std::abs (compasAbans - compasDespres) < 1e-9 && std::abs (p.pistes[1].clips[0].pos - 4.0) < 1e-9,
                  "amb el tempo nou les notes queden al mateix compàs");

        Clip llarg = midi::clipNou (0.0, 0.5);
        midi::afegeix (llarg, { 0, 4 * 960, 60, 100, 0 });
        midi::encabeix (p, llarg);
        comprova (std::abs (llarg.fi - 4.0) < 1e-9, "la finestra s'allarga perquè hi càpiga la nota");
        comprova (midi::soIntern ("intern:baix") == 4 && midi::soIntern ("vst3:x") == -1 && midi::nomInstrument ("intern:pad") == "Pad", "sons interns");
    });

    prova ("MIDI: es desa i es llegeix (notes, controladors, instrument)", []
    {
        namespace fs = std::filesystem;
        const auto arrel = fs::temp_directory_path() / "mibase_proves_midi";
        fs::remove_all (arrel);
        fs::create_directories (arrel);
        Projecte nou;
        nou.nom = "Midi";
        auto r = mibase::disc::creaProjecte (arrel, nou, "x", "x");
        auto q = *mibase::disc::llegeix (r.fitxer);
        auto& t = afegeixPista (q, "Piano", "midi");
        t.instrument = "intern:epiano";
        Clip c = midi::clipNou (1.0, 3.0);
        midi::afegeix (c, { 0, 480, 60, 99, 0 });
        midi::afegeix (c, { 960, 240, 67, 30, 9 });
        c.events.push_back ({ 100, 0xB0, 64, 127, 0 });
        c.events.push_back ({ 200, 0xE0, 0, 12000, 0 });
        q.pistes[0].clips.push_back (c);
        mibase::disc::desa (r.fitxer, q, "x", "x", false);
        auto l = mibase::disc::llegeix (r.fitxer);
        comprova (l && l->pistes.size() == 1 && l->pistes[0].tipus == "midi" && l->pistes[0].instrument == "intern:epiano", "pista i instrument");
        comprova (l && l->pistes[0].clips.size() == 1 && l->pistes[0].clips[0].midi && l->pistes[0].clips[0].notes == c.notes, "les notes, iguals");
        comprova (l && l->pistes[0].clips[0].events == c.events, "els controladors i el pitch bend, iguals");
        fs::remove_all (arrel);
    });

    prova ("MIDI: el pla (notes en segons, retallades a la finestra i tapades per un clip de sobre)", []
    {
        Projecte p;
        p.tempo = 120.0;
        auto& t = afegeixPista (p, "Piano", "midi");
        t.instrument = "intern:piano";
        Clip a = midi::clipNou (0.0, 2.0);
        midi::afegeix (a, { 0, 480, 60, 100, 0 });       // 0,0 – 0,25 s
        midi::afegeix (a, { 1920, 9600, 62, 100, 0 });   // 1,0 s, molt llarga: es talla al final de la finestra (2,0 s)
        midi::afegeix (a, { 3000, 100, 64, 100, 0 });    // 1,5625 s: queda tapada pel clip B
        Clip b = midi::clipNou (1.5, 1.6);
        p.pistes[0].clips = { a, b };
        auto inst = std::make_shared<InstrumentProva>();
        auto pla = construeixPla (p, {}, [&] (const Pista&) { return inst; });
        const auto& pp = pla->pistes[0];
        comprova (pp.instrument == inst && pp.notes.size() == 2, "dues notes sonen (la tapada no)");
        comprova (pp.notes.size() == 2 && std::abs (pp.notes[0].fi - 0.25) < 1e-9 && std::abs (pp.notes[1].ini - 1.0) < 1e-9
                  && std::abs (pp.notes[1].fi - 1.5) < 1e-9, "acaben on toca (la llarga, on comença el clip que la tapa)");
        auto plaSense = construeixPla (p, {});
        comprova (plaSense->pistes[0].notes.empty(), "sense instrument, no sona");
    });

    prova ("MIDI: el mesclador envia les notes a la mostra justa i no en deixa cap de penjada", []
    {
        const double fs = 48000.0;
        Projecte p;
        p.tempo = 120.0;
        auto& t = afegeixPista (p, "Piano", "midi");
        t.instrument = "intern:piano";
        Clip a = midi::clipNou (0.0, 4.0);
        midi::afegeix (a, { 960, 960, 60, 100, 0 });      // 0,5 s a 1,0 s
        midi::afegeix (a, { 1920, 4800, 64, 90, 0 });     // 1,0 s a 3,5 s
        a.events.push_back ({ 960, 0xB0, 64, 127, 0 });
        p.pistes[0].clips = { a };
        auto inst = std::make_shared<InstrumentProva>();
        inst->prepara (fs, 256);
        auto pla = construeixPla (p, {}, [&] (const Pista&) { return inst; });

        Mesclador m;
        m.prepara (fs, 256);
        std::vector<float> L (256), R (256);
        for (long long pos = 0; pos < (long long) (2.0 * fs); pos += 256)
            m.processa (*pla, pos, 256, L.data(), R.data(), false, nullptr);
        bool onBe = false, offBe = false, ccBe = false;
        for (auto& r : inst->rebuts)
        {
            if ((r.estat & 0xF0) == 0x90 && r.d1 == 60 && r.mostra == 24000) onBe = true;
            if ((r.estat & 0xF0) == 0x80 && r.d1 == 60 && r.mostra == 48000) offBe = true;
            if ((r.estat & 0xF0) == 0xB0 && r.d1 == 64 && r.mostra == 24000) ccBe = true;
        }
        comprova (onBe && offBe && ccBe, "note-on, note-off i controlador a la mostra exacta");
        comprova (m.notesEnceses (slotDePista (p.pistes[0])) == 1, "la nota llarga encara sona");

        // Aturar: s'apaga tot.
        m.apagaNotes();
        m.processa (*pla, 0, 256, L.data(), R.data(), false, nullptr, false);
        comprova (inst->apagaTots >= 1 && m.notesEnceses (slotDePista (p.pistes[0])) == 0, "en aturar, no queda cap nota encesa");

        // Silenciada: no hi entra cap nota nova.
        const int onsAbans = inst->ons();
        auto pla2 = construeixPla ([&] { auto q = p; q.pistes[0].mut = true; return q; }(), {}, [&] (const Pista&) { return inst; });
        m.reinicia();
        for (long long pos = 0; pos < (long long) (2.0 * fs); pos += 256)
            m.processa (*pla2, pos, 256, L.data(), R.data(), false, nullptr);
        comprova (inst->ons() == onsAbans, "pista silenciada: cap nota nova");

        // Canvi de pla amb un altre instrument: el vell s'apaga.
        m.reinicia();
        for (long long pos = 0; pos < (long long) (1.2 * fs); pos += 256)
            m.processa (*pla, pos, 256, L.data(), R.data(), false, nullptr);
        auto altre = std::make_shared<InstrumentProva>();
        auto pla3 = construeixPla (p, {}, [&] (const Pista&) { return altre; });
        const int apagatsAbans = inst->apagaTots;
        m.canviDePla (pla.get(), pla3.get());
        comprova (inst->apagaTots == apagatsAbans + 1 && m.notesEnceses (slotDePista (p.pistes[0])) == 0, "l'instrument que se'n va s'apaga");
        m.canviDePla (pla3.get(), pla3.get());
        comprova (altre->apagaTots == 0, "el mateix instrument no s'apaga en publicar un pla nou");

        // Mateixa nota dues vegades seguides (trepitjades): sempre una apagada abans de tornar-la a encendre.
        Projecte p2 = p;
        p2.pistes[0].clips[0].notes = { { 0, 960, 60, 100, 0 }, { 480, 960, 60, 100, 0 } };
        auto i2 = std::make_shared<InstrumentProva>();
        auto pl = construeixPla (p2, {}, [&] (const Pista&) { return i2; });
        Mesclador m2;
        m2.prepara (fs, 512);
        std::vector<float> L2 (512), R2 (512);
        for (long long pos = 0; pos < (long long) (2.0 * fs); pos += 512)
            m2.processa (*pl, pos, 512, L2.data(), R2.data(), false, nullptr);
        int sonant = 0, maxim = 0;
        for (auto& r : i2->rebuts)
        {
            if ((r.estat & 0xF0) == 0x90) ++sonant;
            else if ((r.estat & 0xF0) == 0x80) --sonant;
            maxim = std::max (maxim, sonant);
        }
        comprova (sonant == 0 && maxim == 1 && i2->ons() == 2, "notes trepitjades: cap queda penjada");
    });

    prova ("MIDI: el motor de l'estudi (sonar, aturar, bucle i teclat en directe)", []
    {
        const double fs = 48000.0;
        const int bloc = 256;
        Projecte p;
        p.tempo = 120.0;
        auto& t = afegeixPista (p, "Piano", "midi");
        t.instrument = "intern:piano";
        Clip a = midi::clipNou (0.0, 4.0);
        midi::afegeix (a, { 0, 7680, 60, 100, 0 });      // tota la finestra
        midi::afegeix (a, { 960, 480, 67, 100, 0 });
        p.pistes[0].clips = { a };
        p.bucle = { true, true, 0, 1 };                    // el primer compàs (2 s)
        auto inst = std::make_shared<InstrumentProva>();
        inst->prepara (fs, 2048);
        MotorEstudi motor;
        motor.prepara (fs, 2048);
        motor.publica (construeixPla (p, {}, [&] (const Pista&) { return inst; }));
        std::vector<float> outL (bloc), outR (bloc);
        float* sortides[] = { outL.data(), outR.data() };
        auto corre = [&] (double segons)
        {
            for (long long k = 0; k < (long long) (segons * fs / bloc); ++k)
            {
                std::fill (outL.begin(), outL.end(), 0.0f);
                std::fill (outR.begin(), outR.end(), 0.0f);
                motor.processa (nullptr, 0, sortides, 2, bloc);
                motor.recull();
            }
        };
        motor.reprodueix (0.0);
        corre (5.0);   // dues voltes i mitja de bucle
        comprova (inst->ons() >= 6, "el bucle torna a tocar les notes");
        comprova (inst->enceses <= 2, "mai més de dues notes alhora (les del bucle s'apaguen en tornar)");
        motor.atura();
        corre (0.1);
        comprova (motor.getEstat() == MotorEstudi::Estat::Aturat && inst->enceses == 0, "aturat: cap nota penjada");

        // Teclat en directe, amb el transport aturat.
        const int slot = slotDePista (p.pistes[0]);
        comprova (! motor.enviaTeclat (0x90, 72, 100), "sense pista triada, el teclat no va a l'estudi");
        motor.setSlotTeclat (slot);
        comprova (motor.enviaTeclat (0x90, 72, 100), "amb pista triada, sí");
        motor.setSlotTeclat (-1);                          // es canvia de pista amb la tecla premuda
        comprova (motor.enviaTeclat (0x80, 72, 0), "l'apagada va a la mateixa pista que l'encesa");
        corre (0.05);
        bool rebut = false; for (auto& r : inst->rebuts) if (r.d1 == 72 && (r.estat & 0xF0) == 0x80) rebut = true;
        comprova (rebut && inst->enceses == 0, "el teclat sona aturat i s'apaga");
        comprova (! motor.enviaTeclat (0x90, 40, 100), "sense pista, l'encesa va al Diagnòstic");
        motor.setSlotTeclat (slot);
        comprova (! motor.enviaTeclat (0x80, 40, 0), "i l'apagada també hi va, encara que ara hi hagi pista triada");
        motor.setSlotTeclat (-1);

        motor.enviaDirecte ({ slot, 0x90, 50, 90 });
        corre (0.02);
        comprova (inst->enceses == 1, "una nota des del piano roll");
        motor.enviaDirecte ({ slot, 0x80, 50, 0 });
        corre (0.02);
        comprova (inst->enceses == 0, "i s'apaga");

        // Treure la pista mentre sona: no queda res encès.
        motor.reprodueix (0.0);
        corre (0.6);
        Projecte buit = p;
        buit.pistes.clear();
        motor.publica (construeixPla (buit, {}));
        corre (0.05);
        comprova (inst->enceses == 0, "esborrar la pista mentre sona l'apaga");
        motor.atura();
        corre (0.05);
    });

    prova ("MIDI: gravar el teclat", []
    {
        const double fs = 48000.0;
        MotorEstudi motor;
        motor.prepara (fs, 2048);
        Projecte p;
        auto& t = afegeixPista (p, "Piano", "midi");
        t.instrument = "intern:piano";
        auto inst = std::make_shared<InstrumentProva>();
        motor.publica (construeixPla (p, {}, [&] (const Pista&) { return inst; }));
        const int slot = slotDePista (p.pistes[0]);
        motor.setSlotTeclat (slot);
        motor.setSlotGravaMidi (slot);
        std::vector<float> L (480), R (480);
        float* s[] = { L.data(), R.data() };
        motor.grava (0.0, 0.0, 0, 1);
        for (int k = 0; k < 100; ++k) motor.processa (nullptr, 0, s, 2, 480);   // 1 s
        motor.enviaTeclat (0x90, 60, 100);
        for (int k = 0; k < 50; ++k) motor.processa (nullptr, 0, s, 2, 480);
        motor.enviaTeclat (0x80, 60, 0);
        motor.processa (nullptr, 0, s, 2, 480);
        MotorEstudi::MidiGravat g;
        std::vector<MotorEstudi::MidiGravat> v;
        while (motor.treuMidiGravat (g)) v.push_back (g);
        comprova (v.size() == 2 && std::abs (v[0].t - 1.0) < 0.011 && std::abs (v[1].t - 1.5) < 0.011, "es grava amb el temps de la línia de temps");
    });

    prova ("Sinte intern: sona, s'apaga, pedal i tots els sons", []
    {
        const double fs = 48000.0;
        for (int so = 0; so < (int) midi::sonsInterns().size(); ++so)
        {
            SinteIntern s (so);
            s.prepara (fs, 512);
            std::vector<float> L ((size_t) fs * 5), R ((size_t) fs * 5);
            EventInstrument on { 0, 0x90, 60, 110 };
            s.processa (&on, 1, L.data(), R.data(), (int) fs);
            EventInstrument off { 0, 0x80, 60, 0 };
            s.processa (&off, 1, L.data() + (size_t) fs, R.data() + (size_t) fs, (int) fs);
            for (int k = 2; k < 5; ++k)
                s.processa (nullptr, 0, L.data() + (size_t) (k * fs), R.data() + (size_t) (k * fs), (int) fs);
            const auto nom = std::string (midi::sonsInterns()[(size_t) so].nom);
            comprova (totFinit (L) && totFinit (R), nom + ": sense valors estranys");
            comprova (pic (L, 0, 24000) > 0.02f, nom + ": sona");
            comprova (pic (L, 0, L.size()) < 1.0f, nom + ": no satura");
            comprova (pic (L, (size_t) (4.5 * fs), L.size()) < 0.0005f && ! s.fentSo(), nom + ": s'apaga del tot");
        }
        SinteIntern s (2);   // orgue: sosté mentre la tecla és premuda
        s.prepara (fs, 512);
        std::vector<float> L (48000), R (48000);
        EventInstrument ev[] = { { 0, 0xB0, 64, 127 }, { 0, 0x90, 60, 100 }, { 100, 0x80, 60, 0 } };
        s.processa (ev, 3, L.data(), R.data(), 24000);
        s.processa (nullptr, 0, L.data() + 24000, R.data() + 24000, 24000);
        comprova (pic (L, 40000, 48000) > 0.02f, "amb el pedal, la nota continua després d'amollar la tecla");
        EventInstrument peu { 0, 0xB0, 64, 0 };
        s.processa (&peu, 1, L.data(), R.data(), 24000);
        s.processa (nullptr, 0, L.data(), R.data(), 24000);
        comprova (! s.fentSo(), "sense pedal, s'apaga");
        EventInstrument tres[] = { { 0, 0x90, 60, 100 }, { 0, 0x90, 64, 100 }, { 0, 0x90, 67, 100 } };
        s.processa (tres, 3, L.data(), R.data(), 4800);
        comprova (s.veusSonant() == 3, "tres notes, tres veus");
        s.apagaTot();
        s.processa (nullptr, 0, L.data(), R.data(), 4800);
        comprova (! s.fentSo(), "apagaTot: silenci de seguida");
        comprova (pic (L, 2400, 4800) < 0.001f, "sense clic (s'apaga amb una caiguda curta)");
        SinteIntern gran (0);
        gran.prepara (fs, 512);
        std::vector<EventInstrument> acordGran;
        for (int n : harmonia::acord (48, 10, 0, harmonia::Voicing::Tancat, 2, true)) acordGran.push_back ({ 0, 0x90, (std::uint8_t) n, 127 });
        gran.processa (acordGran.data(), (int) acordGran.size(), L.data(), R.data(), 24000);
        comprova (pic (L, 0, 24000) <= 1.0f && pic (L, 0, 24000) > 0.3f, "un acord de 11 notes fort no passa d'1 (limitador suau)");
    });

    prova ("MIDI: exportar la mescla amb el sinte intern", []
    {
        namespace fs = std::filesystem;
        Projecte p;
        p.tempo = 120.0;
        auto& t = afegeixPista (p, "Piano", "midi");
        t.instrument = "intern:piano";
        Clip a = midi::clipNou (0.0, 2.0);
        midi::afegeix (a, { 0, 960, 60, 100, 0 });
        midi::afegeix (a, { 960, 960, 64, 100, 0 });
        p.pistes[0].clips = { a };
        const auto tros = trosPerExportar (p);
        comprova (std::abs (tros.first) < 1e-9 && tros.second > 1.0 && tros.second <= 3.0, "el tros va de la primera nota a l'última (més la cua)");
        auto pla = construeixPla (p, {}, [] (const Pista& x)
        {
            auto s = std::make_shared<SinteIntern> (std::max (0, midi::soIntern (x.instrument)));
            s->prepara (48000.0, 4096);
            return s;
        });
        const auto desti = fs::temp_directory_path() / "mibase_midi_mescla.wav";
        std::string error;
        comprova (exportaMescla (*pla, 48000.0, tros.first, tros.second, desti, error), "s'exporta");
        auto w = llegeixWav (desti);
        float maxim = 0.0f;
        for (float x : w.mostres) maxim = std::max (maxim, std::abs (x));
        comprova (w.correcte && maxim > 0.02f, "el WAV té el so del sinte");
        fs::remove (desti);
    });


    // ======================================================================
    // Harmonia (0.8)
    // ======================================================================
    prova ("Harmonia: escales, noms i ajust a l'escala", []
    {
        using namespace harmonia;
        comprova (escales().size() == 13, "13 escales");
        const Tonalitat am { 9, 1 };   // La menor natural
        comprova (nomTonalitat (am) == "A Menor natural", "nom de la tonalitat");
        comprova (aEscala (60, am) && aEscala (69, am) && ! aEscala (61, am) && ! aEscala (68, am), "notes de l'escala");
        comprova (ajusta (61, am) == 60 && ajusta (61, am, +1) == 62 && ajusta (68, am) == 67, "ajusta a la més propera (empat: la de sota o la direcció)");
        comprova (ajusta (66, am) == 65 || ajusta (66, am) == 67, "F# a La menor va a F o G");
        comprova (grau (60, am, 1) == 62 && grau (60, am, -1) == 59 && grau (64, am, 2) == 67, "graus de l'escala");
        comprova (nomTo (10, { 5, 0 }) == "Bb" && nomTo (10, { 4, 0 }) == "A#", "bemolls a les tonalitats que en porten");
        const Tonalitat blues { 4, 12 };
        comprova (aEscala (64 + 6, blues) && ! aEscala (64 + 4, blues), "blues de E: té la blue note i no la tercera major");
        comprova (escalaPerId ("dorica") == 4 && escalaPerId ("x") == -1, "escales per id");
    });

    prova ("Harmonia: acords, inversions i voicings", []
    {
        using namespace harmonia;
        comprova (tipusAcords().size() == 18, "18 tipus d'acord");
        comprova (acord (60, 0) == std::vector<int> ({ 60, 64, 67 }), "C");
        comprova (acord (57, 1) == std::vector<int> ({ 57, 60, 64 }), "Am");
        comprova (acord (60, 0, 1) == std::vector<int> ({ 64, 67, 72 }), "C, primera inversió");
        comprova (acord (60, 0, 2) == std::vector<int> ({ 67, 72, 76 }), "C, segona inversió");
        comprova (acord (60, 8, 0, Voicing::Obert) == std::vector<int> ({ 55, 60, 64, 70 }), "C7 obert (drop 2)");
        comprova (acord (60, 0, 0, Voicing::Tancat, 2).size() == 6 && acord (60, 0, 0, Voicing::Tancat, 1, true).front() == 48,
                  "dues octaves i baix");
        comprova (nomAcord (9, 1) == "Am" && nomAcord (0, 0, 4) == "C/E" && nomAcord (7, 8) == "G7", "noms");
    });

    prova ("Harmonia: acords de la tonalitat i reconèixer acords", []
    {
        using namespace harmonia;
        auto c = acordsDeTonalitat ({ 0, 0 });
        comprova (c.size() == 7 && c[0].nom == "C" && c[1].nom == "Dm" && c[4].nom == "G" && c[5].nom == "Am" && c[6].nom == "Bdim",
                  "C major: C Dm Em F G Am Bdim");
        comprova (c[5].roma == "vi" && c[6].roma == "vii°" && c[4].roma == "V", "xifres romanes");
        auto s = acordsDeTonalitat ({ 0, 0 }, true);
        comprova (s[0].nom == "Cmaj7" && s[4].nom == "G7" && s[6].nom == "Bm7b5", "amb setenes");
        auto am = acordsDeTonalitat ({ 9, 1 });
        comprova (am[0].nom == "Am" && am[2].nom == "C" && am[5].nom == "F" && am[6].nom == "G", "La menor: Am … C … F G");
        auto pm = acordsDeTonalitat ({ 9, 11 });
        comprova (pm[0].nom == "Am", "la pentatònica menor fa servir la menor");
        auto hm = acordsDeTonalitat ({ 9, 2 });
        comprova (hm[4].nom == "E", "menor harmònica: el V és major");

        comprova (reconeix ({ 57, 60, 64 }).nom == "Am", "Am");
        comprova (reconeix ({ 52, 60, 67 }).nom == "C/E", "C amb la E al baix");
        comprova (reconeix ({ 55, 59, 62, 65 }).nom == "G7", "G7");
        comprova (reconeix ({ 60, 64 }).nom == "C", "dues notes: C (sense quinta)");
        comprova (! reconeix ({ 60 }).trobat && ! reconeix ({}).trobat, "una nota no és un acord");
    });

    prova ("Harmonia: detectar la tonalitat i notes que sonen", []
    {
        using namespace harmonia;
        Projecte p;
        p.tempo = 120.0;
        auto& t = afegeixPista (p, "Piano", "midi");
        t.instrument = "intern:piano";
        Clip c = midi::clipNou (0.0, 16.0);
        // Am – F – C – G, i una melodia de La menor.
        const int acords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 55, 60, 64 }, { 55, 59, 62 } };
        for (int k = 0; k < 4; ++k)
            for (int n : acords[k]) midi::afegeix (c, { k * 3840LL, 3840, n, 90, 0 });
        for (int k = 0; k < 8; ++k) midi::afegeix (c, { k * 960LL, 960, (k % 2 == 0) ? 69 : 72, 90, 0 });
        p.pistes[0].clips.push_back (c);
        auto d = detecta (p);
        comprova (d.fiable && d.tonalitat.to == 9 && d.tonalitat.escala == 1, "detecta La menor (" + nomTonalitat (d.tonalitat) + ")");
        comprova (tonalitatEfectiva (p).to == 9, "en automàtic, fa servir la detectada");
        p.tonalitatAuto = false;
        p.tonalitat = { 2, 4 };
        comprova (tonalitatEfectiva (p).to == 2 && tonalitatEfectiva (p).escala == 4, "a mà, la triada");
        auto ara = notesSonant (p, 2.25);   // dins el segon acord (F) i la segona nota de la melodia
        comprova (reconeix (ara).trobat && reconeix (ara).nom.substr (0, 1) == "F", "l'acord que sona ara: " + reconeix (ara).nom);
        Projecte buit;
        comprova (! detecta (buit).fiable, "sense notes no es pot detectar");
    });

    prova ("Harmonia: moviment mínim, progressions i acords sobre les notes", []
    {
        using namespace harmonia;
        auto g = enllaca ({ 60, 64, 67 }, acord (65, 0));   // de C a F: millor C-F-A (segona inversió)
        comprova (g == std::vector<int> ({ 60, 65, 69 }), "C → F amb el mínim moviment");
        auto pr = generaProgressio ({ 9, 1 }, 1, 8, false, Voicing::Tancat, true, 4);
        comprova (pr.size() == 8, "8 compassos");
        comprova (reconeix (pr[0]).nom.substr (0, 2) == "Am" && reconeix (pr[1]).nom.substr (0, 1) == "F" && reconeix (pr[2]).nom.substr (0, 1) == "C",
                  "Balada a La menor: Am F C G (amb la relativa major)");
        auto pop = acordsProgressio ({ 9, 1 }, 0, false);
        comprova (pop[0].nom == "C" && pop[1].nom == "G" && pop[2].nom == "Am" && pop[3].nom == "F", "Pop a La menor: C G Am F");
        auto and_ = acordsProgressio ({ 0, 0 }, 8, false);
        comprova (and_[0].nom == "Am" && and_[1].nom == "G" && and_[3].nom == "E", "Andalusa a Do major (la relativa menor): Am G F E");
        comprova (relativaMajor ({ 2, 4 }).to == 0, "la major de Re dòric és Do");
        int salt = 0;
        for (size_t k = 1; k < pr.size(); ++k)
        {
            int s0 = 0, s1 = 0;
            for (int x : pr[k - 1]) s0 += x;
            for (int x : pr[k]) s1 += x;
            salt = std::max (salt, std::abs (s1 - s0) / (int) pr[k].size());
        }
        comprova (salt <= 5, "amb moviment mínim, cap acord salta lluny");
        auto blues = generaProgressio ({ 4, 0 }, 10, 12, true, Voicing::Tancat, false);
        comprova (blues.size() == 12 && reconeix (blues[4]).nom.substr (0, 1) == "A", "blues de 12 en E (el compàs 5, A)");

        Clip c = midi::clipNou (0.0, 4.0);
        auto noves = afegeixAcord (c, acord (60, 0), 0, 1920, 90);
        comprova (c.notes.size() == 3 && noves.size() == 3, "afegeix un acord");
        std::vector<int> totes { 0, 1, 2 };
        inverteix (c, totes, true);
        comprova (c.notes[0].to == 64 && c.notes[2].to == 72, "inversió amunt");
        inverteix (c, totes, false);
        comprova (c.notes[0].to == 60 && c.notes[2].to == 67, "i avall");

        Clip d = midi::clipNou (0.0, 4.0);
        midi::afegeix (d, { 0, 960, 57, 100, 0 });     // A
        midi::afegeix (d, { 960, 960, 53, 100, 0 });   // F
        auto r = ferAcords (d, { 0, 1 }, { 9, 1 }, 0, true);
        std::vector<int> primer, segon;
        for (auto& n : d.notes) (n.ini == 0 ? primer : segon).push_back (n.to);
        comprova (reconeix (primer).nom == "Am" && reconeix (segon).nom == "F", "cada nota es converteix en l'acord de la tonalitat");
        comprova (r.size() == 4, "s'hi afegeixen dues notes per acord");

        Clip e = midi::clipNou (0.0, 8.0);
        afegeixAcord (e, acord (60, 0), 0, 960, 90);
        afegeixAcord (e, acord (65, 0, 0), 960, 960, 90);   // F en posició fonamental: salta
        std::vector<int> t2 { 0, 1, 2, 3, 4, 5 };
        enllacaTriades (e, t2);
        std::vector<int> f;
        for (auto& n : e.notes) if (n.ini == 960) f.push_back (n.to);
        comprova (f == std::vector<int> ({ 60, 65, 69 }), "enllaça els acords triats");
    });

    prova ("Harmonia: es desa la tonalitat, el mode i els canvis", []
    {
        namespace fs = std::filesystem;
        const auto arrel = fs::temp_directory_path() / "mibase_proves_harmonia";
        fs::remove_all (arrel);
        fs::create_directories (arrel);
        Projecte nou;
        nou.nom = "Ton";
        auto r = mibase::disc::creaProjecte (arrel, nou, "x", "x");
        auto q = *mibase::disc::llegeix (r.fitxer);
        comprova (q.tonalitatAuto && q.modeEscala == ModeEscala::Visual, "per defecte, automàtica i visual");
        q.tonalitatAuto = false;
        q.tonalitat = { 7, 7 };
        q.modeEscala = ModeEscala::Bloquejat;
        q.ressaltaAcord = false;
        q.canvisTonalitat.push_back ({ 8, { 2, 0 } });
        mibase::disc::desa (r.fitxer, q, "x", "x", false);
        auto l = mibase::disc::llegeix (r.fitxer);
        comprova (l && ! l->tonalitatAuto && l->tonalitat == Tonalitat { 7, 7 } && l->modeEscala == ModeEscala::Bloquejat && ! l->ressaltaAcord,
                  "tonalitat, mode i acord");
        comprova (l && l->canvisTonalitat.size() == 1 && l->canvisTonalitat[0].compas == 8 && l->canvisTonalitat[0].tonalitat == Tonalitat { 2, 0 },
                  "els canvis de tonalitat (per a més endavant) es conserven");
        fs::remove_all (arrel);
    });


    // ======================================================================
    // Pads d'acords (0.9)
    // ======================================================================
    prova ("Pads: llegir i escriure noms d'acord", []
    {
        using namespace harmonia;
        auto a = llegeixAcord ("Ebmaj7");
        comprova (a.arrel == 3 && a.tipus == 0 && a.tensions == TMaj7 && nomPad (a) == "D#maj7", "Ebmaj7 (" + nomPad (a) + ")");
        comprova (nomPad (llegeixAcord ("C#m7")) == "C#m7", "C#m7");
        comprova (nomPad (llegeixAcord ("Bb/D")) == "A#/D", "Bb/D amb la D al baix");
        auto g = llegeixAcord ("G7(b9)");
        comprova (g.tensions == (T7 | Tb9) && nomPad (g) == "G7/b9", "G7(b9) → G7/b9");
        comprova (nomPad (llegeixAcord ("F#m7b5")) == "F#m7b5" && nomPad (llegeixAcord ("Dsus4")) == "Dsus4", "m7b5 i sus4");
        comprova (nomPad (llegeixAcord ("E9")) == "E9" && nomPad (llegeixAcord ("Fmaj7/9")) == "Fmaj9" && nomPad (llegeixAcord ("Cadd9")) == "Cadd9", "novenes");
        comprova (nomPad (llegeixAcord ("A♭7")) == "G#7", "el bemoll ♭ també");
        comprova (llegeixAcord ("Hola").buit() && llegeixAcord ("Cxyz").buit() && llegeixAcord ("").buit(), "el que no s'entén, buit");
        comprova (notesPad (llegeixAcord ("Am")) == std::vector<int> ({ 57, 60, 64 }), "notes d'Am (A3 C4 E4)");
        comprova (notesPad (llegeixAcord ("C/E")).front() == 52, "C/E: la E al baix");
        comprova (notesPad (llegeixAcord ("G7/b9")).size() == 5, "G7/b9: 5 notes");
        comprova (reconeix (notesPad (llegeixAcord ("Dm7"))).nom == "Dm7", "el que sona es reconeix com el mateix acord");
        // Tots els noms que fa nomPad es tornen a llegir igual.
        bool tot = true;
        for (int r = 0; r < 12; ++r)
            for (int tp = 0; tp < numTipusPad; ++tp)
                for (unsigned t : { 0u, (unsigned) T7, (unsigned) TMaj7, (unsigned) (T7 | T9), (unsigned) (T7 | Tb9), (unsigned) T13, (unsigned) T9 })
                {
                    AcordPad x { r, tp, t, -1 };
                    if (notesPad (llegeixAcord (nomPad (x))) != notesPad (x)) { tot = false; std::printf ("    %s\n", nomPad (x).c_str()); }
                }
        comprova (tot, "anada i tornada dels noms (mateixes notes)");
    });

    prova ("Pads: els 50 presets", []
    {
        using namespace harmonia;
        comprova (pads::presets().size() == 50, "50 presets");
        bool tots = true;
        for (int i = 0; i < (int) pads::presets().size(); ++i)
        {
            const auto p = pads::padsDePreset (i);
            if ((int) p.size() != pads::numPads) tots = false;
            for (int k = 0; k < pads::numPads; ++k)
                if (llegeixAcord (p[(size_t) k]).buit()) { tots = false; std::printf ("    no s'entén: %s\n", p[(size_t) k].c_str()); }
        }
        comprova (tots, "tots els acords dels presets s'entenen");
        const auto c = pads::padsDePreset (0);
        // Creep: G B C Cm G Em Am D → blanques G B C Cm Em Am D (sense repetir), negres els de color de Sol major.
        comprova (c.size() == 12 && c[0] == "G" && c[2] == "B" && c[4] == "C" && c[5] == "Cm" && c[7] == "Em" && c[9] == "Am" && c[11] == "D",
                  "Creep: la seqüència a les blanques, en ordre i sense repetir");
        comprova (c[1] == "D7", "Creep: a la C#, el V7 (D7)");
        bool negresPlenes = true;
        for (int i = 0; i < (int) pads::presets().size(); ++i)
        {
            const auto p = pads::padsDePreset (i);
            for (int k = 0; k < 12; ++k) if (p[(size_t) k].empty()) negresPlenes = false;
        }
        comprova (negresPlenes, "cap preset deixa pads buits");
    });

    prova ("Pads com un piano (0.10): 12, blanques i negres", []
    {
        using namespace harmonia;
        comprova (pads::numPads == 12 && pads::esNegre (1) && pads::esNegre (10) && ! pads::esNegre (4) && ! pads::esNegre (5), "disposició");
        const auto t = pads::padsDeTonalitat ({ 9, 1 });
        comprova (t.size() == 12 && t[0] == "Am" && t[2] == "Bdim" && t[4] == "C" && t[5] == "Dm" && t[7] == "Em" && t[9] == "F" && t[11] == "G",
                  "La menor: graus i a vii a les blanques");
        comprova (t[1] == "E7" && t[6] == "D" && t[10] == "A#", "La menor: negres E7, D, Bb");
        const auto m = pads::padsDeTonalitat ({ 0, 0 });
        comprova (m[0] == "C" && m[2] == "Dm" && m[11] == "Bdim" && m[1] == "G7" && m[6] == "Fm" && m[8] == "G#" && m[10] == "A#",
                  "Do major: C Dm … Bdim; negres G7, Dm7, Fm, Ab, Bb");
        // Adaptar els 16 de la 0.9.
        std::vector<std::string> vells (16, "Am");
        comprova (pads::adapta (vells, "Creep") == pads::padsDePreset (0), "16 pads d'un preset: es refan amb la disposició nova");
        comprova (pads::adapta (vells, "Acords de la tonalitat").empty(), "16 pads de la tonalitat: tornen a ser de la tonalitat");
        vells[0] = "F"; vells[7] = "E7";
        const auto a = pads::adapta (vells, "");
        comprova (a.size() == 12 && a[0] == "F" && a[1] == "E7", "16 pads fets a mà: es conserven els primers");
    });

    prova ("Pads: inversions (0.10)", []
    {
        using namespace harmonia;
        auto c = llegeixAcord ("C");
        comprova (notesPad (c) == std::vector<int> ({ 60, 64, 67 }), "C en posició fonamental");
        c.inversio = 1;
        comprova (notesPad (c) == std::vector<int> ({ 64, 67, 72 }) && textPad (c) == "C^1" && nomPad (c) == "C", "1a inversió (E G C)");
        c.inversio = 2;
        comprova (notesPad (c) == std::vector<int> ({ 67, 72, 76 }), "2a inversió (G C E)");
        c.inversio = -1;
        comprova (notesPad (c) == std::vector<int> ({ 55, 60, 64 }), "-1: la de dalt baixa (G C E, avall)");
        comprova (llegeixAcord ("Am7^1").inversio == 1 && llegeixAcord ("Am7^1").tensions == T7 && llegeixAcord ("G/B^-2").inversio == -2
                  && llegeixAcord ("G/B^-2").baix == 11, "llegir «^n»");
        comprova (llegeixAcord ("C^9").inversio == maxInversio && llegeixAcord ("C^x").buit(), "límits i errors");
        auto b = llegeixAcord ("C/E^1");
        comprova (notesPad (b).front() == 52 && notesPad (b).size() == 4, "amb nota greu: la greu continua a baix");
    });

    prova ("Teclat d'acords (0.10): una tecla, un acord", []
    {
        TeclatAcords t;
        t.setAcord (0, { 60, 64, 67 });      // C
        t.setAcord (9, { 57, 60, 64 });      // Am
        std::vector<std::array<int, 3>> v;
        auto envia = [&v] (std::uint8_t e, std::uint8_t a, std::uint8_t b) { v.push_back ({ e, a, b }); };
        t.tradueix (0x90, 48, 100, true, envia);   // C3 → C
        comprova (v.size() == 3 && v[0] == std::array<int, 3> { 0x90, 60, 100 } && v[2][1] == 67, "la tecla C (de qualsevol octava) toca C");
        v.clear();
        t.tradueix (0x90, 69, 90, true, envia);    // A4 → Am: C i E ja sonen
        comprova (v.size() == 1 && v[0][1] == 57, "Am amb C sonant: només s'encén la A");
        v.clear();
        t.tradueix (0x80, 48, 0, true, envia);     // amollo C: G s'apaga; C i E no (els manté Am)
        comprova (v.size() == 1 && v[0] == std::array<int, 3> { 0x80, 67, 0 }, "amollar C: només s'apaga la G");
        v.clear();
        t.setAcord (9, { 50, 53, 57 });            // canvio el pad mentre sona
        t.tradueix (0x80, 69, 0, false, envia);    // i desactivo: s'apaga el que va encendre
        comprova (v.size() == 3 && t.teclesAmbAcord() == 0, "l'apagada apaga el que es va encendre (encara que el pad hagi canviat)");
        v.clear();
        t.tradueix (0x90, 62, 80, false, envia);   // desactivat: passa tal qual
        t.tradueix (0x80, 62, 0, true, envia);     // i l'apagada també (encara que ara estigui actiu)
        comprova (v.size() == 2 && v[0][1] == 62 && v[1] == std::array<int, 3> { 0x80, 62, 0 }, "desactivat: les notes passen");
        v.clear();
        t.tradueix (0x90, 61, 80, true, envia);    // pad buit (C#): res
        t.tradueix (0x80, 61, 0, true, envia);
        comprova (v.empty(), "pad buit: no sona res");
        t.tradueix (0x91, 60, 70, true, envia);
        t.tradueix (0x91, 60, 70, true, envia);    // tornada a prémer sense amollar
        t.tradueix (0x81, 60, 0, true, envia);
        int enceses = 0, apagades = 0;
        for (auto& x : v) { if ((x[0] & 0xF0) == 0x90) ++enceses; else if ((x[0] & 0xF0) == 0x80) ++apagades; }
        comprova (enceses == 6 && apagades == 6 && (v[0][0] & 0x0F) == 1, "prémer dos cops sense amollar: no queda res penjat (i es manté el canal)");
        v.clear();
        t.tradueix (0x90, 60, 70, true, envia);
        t.tradueix (0xB0, 123, 0, true, envia);
        t.tradueix (0x80, 60, 0, true, envia);
        comprova (t.teclesAmbAcord() == 0 && v.back()[0] == 0xB0, "«tot apagat» ho buida");
        t.tradueix (0xE0, 0, 64, true, envia);
        comprova (v.back()[0] == 0xE0, "els altres missatges passen");
    });

    prova ("Carpetes de pistes (0.11)", []
    {
        namespace ed = mibase::edicio;
        Projecte p;
        for (auto* n : { "Bombo", "Guitarra", "Caixa", "Veu", "Charles" }) ed::afegeixPista (p, n, "audio");
        const auto nom = [&p] (int i) { return p.pistes[(size_t) i].nom; };
        const auto id = ed::creaCarpeta (p, "Bateria", { 0, 2, 4 });
        comprova (p.carpetes.size() == 1 && ed::carpeta (p, id) != nullptr && ed::carpeta (p, id)->nom == "Bateria", "carpeta creada");
        comprova (nom (0) == "Bombo" && nom (1) == "Caixa" && nom (2) == "Charles" && nom (3) == "Guitarra" && nom (4) == "Veu",
                  "les pistes de la carpeta queden seguides, en el seu ordre");
        comprova (ed::pistesDeCarpeta (p, id) == std::vector<int> ({ 0, 1, 2 }), "pistes de la carpeta");
        const int on = ed::posaACarpeta (p, 4, id);
        comprova (on == 3 && nom (3) == "Veu" && ed::pistesDeCarpeta (p, id).size() == 4, "posar-ne una altra: va al final de la carpeta");
        ed::posaACarpeta (p, 3, "");
        comprova (p.pistes[3].carpeta.empty() && ed::pistesDeCarpeta (p, id).size() == 3, "treure-la de la carpeta");
        comprova (ed::filesVisibles (p) == std::vector<int> ({ 1, 1, 1, 1, 1 }), "oberta: totes visibles");
        ed::carpeta (p, id)->plegada = true;
        comprova (ed::filesVisibles (p) == std::vector<int> ({ 2, 0, 0, 1, 1 }), "plegada: una fila de carpeta");
        ed::carpeta (p, id)->amagada = true;
        comprova (ed::filesVisibles (p) == std::vector<int> ({ 0, 0, 0, 1, 1 }), "amagada: no es veu");
        comprova (ed::posaACarpeta (p, 0, "f999") == 0 && p.pistes[0].carpeta == id, "una carpeta que no existeix: res");

        namespace fs = std::filesystem;
        const auto arrel = fs::temp_directory_path() / "mibase_proves_carpetes";
        fs::remove_all (arrel);
        fs::create_directories (arrel);
        Projecte nou;
        nou.nom = "Carp";
        auto r = mibase::disc::creaProjecte (arrel, nou, "x", "x");
        mibase::disc::desa (r.fitxer, p, "x", "x", false);
        auto l = mibase::disc::llegeix (r.fitxer);
        comprova (l && l->carpetes.size() == 1 && l->carpetes[0].nom == "Bateria" && l->carpetes[0].plegada && l->carpetes[0].amagada
                  && l->pistes[1].carpeta == id && l->pistes[4].carpeta.empty(), "es desen i es llegeixen");
        ed::esborraCarpeta (p, id);
        comprova (p.carpetes.empty() && p.pistes.size() == 5 && p.pistes[0].carpeta.empty(), "esborrar la carpeta no esborra les pistes");
        fs::remove_all (arrel);
    });

    std::printf ("\n%d comprovacions, %d fallades\n", comprovacions, fallades);
    return fallades == 0 ? 0 : 1;
}
