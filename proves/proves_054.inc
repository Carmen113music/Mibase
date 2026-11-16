// 0.54: la vista segueix el capçal lliscant (sense salts de pàgina) i es queda amb el capçal al mig.
namespace
{
    void proves054()
    {
        using namespace mibase;
        prova ("0.54 seguiment del capçal: llisca suau fins a deixar-lo al mig", []
        {
            SeguimentCapcal s;
            const double w = 10.0, dt = 1.0 / 30.0, T = 8.0;
            double inici = 0.0, capcal = 1.0;
            bool quiet = true, sempreDins = true, suau = true;
            double comencaA = -1.0, vAnt = 0.0, maxSalt = 0.0;
            for (int i = 0; i < 30 * 40; ++i)
            {
                capcal += dt;   // 1 unitat per segon
                const double nou = s.pas (capcal, inici, w, dt, T);
                const double v = (nou - inici) / dt;
                if (comencaA < 0.0 && capcal - inici < 0.9 * w - 0.05 && nou != inici) quiet = false;
                if (comencaA < 0.0 && nou != inici) comencaA = capcal - inici;
                maxSalt = std::max (maxSalt, std::abs (v - vAnt));
                if (std::abs (v - vAnt) > 0.25) suau = false;
                vAnt = v;
                inici = nou;
                if (capcal < inici || capcal > inici + w) sempreDins = false;
            }
            comprova (quiet, "no es mou fins que el capçal arriba a la vora");
            comprova (comencaA >= 0.89 * w && comencaA <= 0.92 * w, "comença a la vora dreta");
            comprova (sempreDins, "el capçal no surt mai de la pantalla");
            comprova (suau, "la velocitat canvia de mica en mica (sense salts)");
            comprova (std::abs ((capcal - inici) / w - 0.5) < 0.01, "al final, el capçal és al mig");
            comprova (std::abs (vAnt - 1.0) < 0.01, "i la vista va a la mateixa velocitat que la música");
            std::printf ("        (salt màxim de velocitat per tic: %.3f)\n", maxSalt);
        });

        prova ("0.54 seguiment del capçal: arriba al mig en uns «tempsCentrar» segons", []
        {
            SeguimentCapcal s;
            const double w = 10.0, dt = 1.0 / 30.0, T = 8.0;
            double inici = 0.0, capcal = 0.0, tComenca = -1.0, tMig = -1.0;
            for (int i = 0; i < 30 * 60; ++i)
            {
                capcal += dt;
                const double nou = s.pas (capcal, inici, w, dt, T);
                if (tComenca < 0.0 && nou != inici) tComenca = i * dt;
                inici = nou;
                if (tComenca >= 0.0 && tMig < 0.0 && std::abs ((capcal - inici) / w - 0.5) < 0.03) tMig = i * dt - tComenca;
            }
            comprova (tMig > 0.5 * T && tMig < 1.6 * T, "de la vora al mig, entre la meitat i una mica més del temps demanat");
            std::printf ("        (de la vora al mig: %.1f s, demanat %.1f s)\n", tMig, T);
        });

        prova ("0.54 seguiment del capçal: bucles, salts i vista moguda a mà", []
        {
            const double w = 10.0, dt = 1.0 / 30.0, T = 8.0;
            {
                // bucle dins la pantalla: la vista no es mou mai
                SeguimentCapcal s; double inici = 0.0, capcal = 2.0; bool quiet = true;
                for (int i = 0; i < 30 * 20; ++i)
                {
                    capcal += dt; if (capcal >= 6.0) capcal = 2.0;
                    const double nou = s.pas (capcal, inici, w, dt, T);
                    if (nou != inici) quiet = false;
                    inici = nou;
                }
                comprova (quiet, "un bucle que cap a la pantalla no mou la vista");
            }
            {
                // seguint, un bucle que torna enrere fora de la pantalla: es veu sencer i la vista es queda quieta
                SeguimentCapcal s; double inici = 0.0, capcal = 0.0, iniciAbans = -1.0; bool quieta = true, esVeu = true;
                for (int i = 0; i < 30 * 60; ++i)
                {
                    capcal += dt; if (capcal >= 20.0) capcal = 16.0;
                    inici = s.pas (capcal, inici, w, dt, T);
                    if (i > 30 * 40)
                    {
                        if (iniciAbans >= 0.0 && inici != iniciAbans) quieta = false;
                        if (inici > 16.0 || inici + w < 20.0) esVeu = false;
                        iniciAbans = inici;
                    }
                }
                comprova (quieta && esVeu, "un bucle que hi cap acaba quiet i sencer a la pantalla");
            }
            {
                // salt lluny (situar o bucle fora de la pantalla): el capçal queda al mig de seguida
                SeguimentCapcal s; double inici = 0.0, capcal = 1.0;
                inici = s.pas (capcal, inici, w, dt, T);
                capcal = 50.0;
                inici = s.pas (capcal, inici, w, dt, T);
                comprova (std::abs (capcal - inici - 0.5 * w) < 1e-9, "un salt lluny el deixa al mig");
            }
            {
                // moure la vista a mà mentre sona: el seguiment s'atura fins que es reinicia
                SeguimentCapcal s; double inici = 0.0, capcal = 0.0;
                for (int i = 0; i < 30 * 12; ++i) { capcal += dt; inici = s.pas (capcal, inici, w, dt, T); }
                comprova (s.actiu(), "seguint");
                inici += 3.0;   // a mà
                double nou = s.pas (capcal + dt, inici, w, dt, T);
                comprova (s.pausat() && nou == inici, "pausat en moure-la a mà");
                for (int i = 0; i < 30 * 30; ++i) { capcal += dt; nou = s.pas (capcal, inici, w, dt, T); }
                comprova (nou == inici, "i no la torna a moure");
                s.reinicia();
                comprova (! s.pausat() && ! s.actiu(), "en tornar a començar, torna a seguir");
            }
            {
                // amb ticks (unitats grans) també va igual
                SeguimentCapcal s; const double tw = 7680.0 * 2.5; double inici = 0.0, capcal = 0.0, vel = 1920.0;
                for (int i = 0; i < 30 * 40; ++i) { capcal += vel * dt; inici = s.pas (capcal, inici, tw, dt, T); }
                comprova (std::abs ((capcal - inici) / tw - 0.5) < 0.01, "també en ticks");
            }
        });
    }
}
