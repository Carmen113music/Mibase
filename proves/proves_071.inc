// 0.71: el bucle toca les notes que cauen just al seu inici; escoltar l'entrada mentre es grava.
namespace
{
    void proves071()
    {
        using namespace mibase;
        prova ("0.71 Bucle: les notes just a l'inici del bucle sonen a cada volta", []
        {
            const double fs = 48000.0;
            int fallades = 0, casos = 0;
            for (double tempo : { 120.0, 97.0, 133.0, 89.5, 141.0 })
                for (int de : { 1, 2, 3, 5 })
                    for (double posClip : { 0.0, 0.37, 1.0 / 3.0 })
                    {
                        Projecte p;
                        p.tempo = tempo;
                        auto& t = edicio::afegeixPista (p, "Piano", "midi");
                        t.instrument = "intern:piano";
                        p.bucle = { true, true, (double) de, (double) de + 1 };
                        const double ini = edicio::iniciBucle (p), fi = edicio::fiBucle (p);
                        // un clip que comença abans del bucle i té una nota just al seu començament
                        Clip a = midi::clipNou (posClip, fi + 1.0);
                        const long long tick = std::llround ((ini - posClip) / midi::segonsPerTick (p));
                        midi::afegeix (a, { tick, 240, 60, 100, 0 });
                        p.pistes[0].clips = { a };
                        auto inst = std::make_shared<InstrumentProva>();
                        inst->prepara (fs, 2048);
                        MotorEstudi motor;
                        motor.prepara (fs, 2048);
                        motor.publica (construeixPla (p, {}, [&] (const Pista&) { return inst; }));
                        std::vector<float> L (256), R (256);
                        float* s[] = { L.data(), R.data() };
                        motor.reprodueix (ini);
                        const int blocs = (int) ((fi - ini) * 3.5 * fs / 256);
                        for (int k = 0; k < blocs; ++k) { motor.processa (nullptr, 0, s, 2, 256); motor.recull(); }
                        ++casos;
                        if (inst->ons() < 4)
                        {
                            ++fallades;
                            if (fallades < 4) std::printf ("        [!] tempo %.1f, bucle %d, clip %.2f: %d de 4\n", tempo, de, posClip, inst->ons());
                        }
                        if (inst->apagaTots > 0 && fallades < 4) std::printf ("        (apagaTot %d cops)\n", inst->apagaTots);
                    }
            std::printf ("        %d casos, %d amb notes perdudes\n", casos, fallades);
            comprova (fallades == 0, "la nota de l'inici sona a totes les voltes");
        });
    }
}
namespace
{
    void proves071b()
    {
        using namespace mibase;
        prova ("0.71 Escoltar-se: la pista armada fa sonar l'entrada (aturat i gravant)", []
        {
            const double fs = 48000.0;
            const int bloc = 256;
            auto mesura = [&] (bool armada, bool escolta, bool fesGrava, int entradaPista)
            {
                Projecte p;
                auto& t = edicio::afegeixPista (p, "Guitarra", "audio");
                t.grava = armada; t.escolta = escolta; t.entrada = entradaPista; t.canals = 1;
                MotorEstudi motor;
                motor.prepara (fs, 2048);
                motor.setMapaEntrades ({ 0, 1 });
                motor.publica (construeixPla (p, {}, {}));
                std::vector<float> in1 (bloc), in2 (bloc, 0.0f), L (bloc), R (bloc);
                for (int i = 0; i < bloc; ++i) in1[(size_t) i] = 0.25f * (float) std::sin (0.05 * i);
                const float* ent[] = { in1.data(), in2.data() };
                float* s[] = { L.data(), R.data() };
                if (fesGrava) motor.grava (0.0, 0.0, entradaPista, 1);
                float pic = 0.0f;
                for (int k = 0; k < 200; ++k)
                {
                    std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
                    motor.processa (ent, 2, s, 2, bloc);
                    motor.recull();
                    if (k > 100) for (int i = 0; i < bloc; ++i) pic = std::max (pic, std::max (std::fabs (L[(size_t) i]), std::fabs (R[(size_t) i])));
                }
                if (fesGrava) motor.atura();
                return pic;
            };
            const float aturada = mesura (true, true, false, 0), noArmada = mesura (false, true, false, 0), sense = mesura (true, false, false, 0);
            const float altra = mesura (true, true, false, 1), gravant = mesura (true, true, true, 0);
            std::printf ("        armada %.3f · sense armar %.3f · escolta apagada %.3f · altra entrada %.3f · gravant %.3f\n", aturada, noArmada, sense, altra, gravant);
            comprova (aturada > 0.1f, "armada (aturat): se sent la guitarra");
            comprova (gravant > 0.1f, "gravant: també se sent");
            comprova (noArmada < 1e-6f, "sense el botó de gravar, no");
            comprova (sense < 1e-6f, "amb «escolta» apagada (si es fa servir el monitor de la targeta), no");
            comprova (altra < 1e-6f, "només l'entrada de la pista");
        });
    }
}
