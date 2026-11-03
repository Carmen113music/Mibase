// Proves automàtiques del motor de Mibase.
// S'executen a qualsevol sistema (Linux a la sessió de Claude, Windows a GitHub Actions).

#include "mibase/ClicDeProva.h"
#include "mibase/CuaSenseBloqueig.h"
#include "mibase/MesuraLatencia.h"
#include "mibase/Mesurador.h"
#include "mibase/MotorDiagnostic.h"
#include "mibase/SinteSenzill.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
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

    std::printf ("\n%d comprovacions, %d fallades\n", comprovacions, fallades);
    return fallades == 0 ? 0 : 1;
}
