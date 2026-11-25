// 0.98: el bucle amb compensació de retard (PDC): les pistes que llegeixen avançat també tornen a l'inici del bucle.
#include "mibase/Guitarra.h"
namespace
{
    struct InstrumentRetard098 : InstrumentProva
    {
        int retard = 0;
        int latencia() const noexcept override { return retard; }
    };

    /** Un projecte amb la pista d'Acords i una pista que la segueix amb un patró; bucle de 2 compassos. */
    Projecte projecte098 (double tempo, double de, const char* patro)
    {
        Projecte p;
        p.tempo = tempo;
        const char* ac[] = { "G", "D", "Em", "C", "G", "D" };
        for (int b = 0; b < 6; ++b) edicio::posaAcord (p, edicio::segonsDeCompas (p, 1 + b), edicio::segonsDeCompas (p, 2 + b), ac[b]);
        edicio::afegeixPista (p, "Guitarra", "midi");
        for (auto& q : p.pistes)
            if (q.tipus == "midi")
            {
                q.instrument = "intern:piano";
                q.segueixAcords = true;
                Clip c = midi::clipNou (edicio::segonsDeCompas (p, 1), edicio::segonsDeCompas (p, 7));
                guitarra::posaBloc (c, guitarra::blocDe (guitarra::patro (patro), 0, 6 * 3840, 0.0, 7, 90));
                q.clips = { c };
            }
        guitarra::actualitza (p);
        p.bucle = { true, true, de, de + 2.0 };
        return p;
    }

    /** Les notes que rep l'instrument a cada volta, en mostres des de l'inici de la volta (tal com sortiran, és a dir,
        sumant-hi el retard de l'instrument). */
    std::vector<std::vector<long long>> voltes098 (const Projecte& p, int retard, int bloc, int voltes)
    {
        const double fs = 48000.0;
        auto inst = std::make_shared<InstrumentRetard098>();
        inst->retard = retard;
        inst->prepara (fs, 2048);
        MotorEstudi motor;
        motor.prepara (fs, 2048);
        motor.publica (construeixPla (p, {}, [&] (const Pista&) { return inst; }));
        const double ini = edicio::iniciBucle (p), fi = edicio::fiBucle (p);
        const long long volta = std::llround ((fi - ini) * fs);
        std::vector<float> L ((size_t) bloc), R ((size_t) bloc);
        float* s[] = { L.data(), R.data() };
        motor.reprodueix (ini);
        const int blocs = (int) (volta * (voltes + 0.5) / bloc);
        for (int k = 0; k < blocs; ++k) { motor.processa (nullptr, 0, s, 2, bloc); motor.recull(); }
        std::vector<std::vector<long long>> r ((size_t) voltes + 1);
        for (auto& x : inst->rebuts)
            if ((x.estat & 0xF0) == 0x90 && x.d2 > 0)
            {
                const long long t = x.mostra + retard;   // quan se sent
                const long long v = t / volta;
                if (v < (long long) r.size()) r[(size_t) v].push_back (t % volta);
            }
        return r;
    }

    void proves098()
    {
        using namespace mibase;
        prova ("0.98 Bucle amb plugins que retarden (PDC): el primer temps sona a totes les voltes", []
        {
            int fallades = 0, casos = 0;
            for (const char* patro : { "m01", "f01", "pa01" })
                for (double tempo : { 120.0, 97.0 })
                    for (double de : { 2.0, 3.0 })
                        for (int retard : { 64, 300, 1500, 5000 })
                            for (int bloc : { 128, 512 })
                            {
                                const auto p = projecte098 (tempo, de, patro);
                                const auto sense = voltes098 (p, 0, bloc, 3);
                                const auto amb = voltes098 (p, retard, bloc, 3);
                                ++casos;
                                bool be = true;
                                // de la segona volta en endavant, les mateixes notes i al mateix moment que sense retard
                                for (int v = 1; v <= 2; ++v)
                                {
                                    if (amb[(size_t) v].size() != sense[(size_t) v].size()) { be = false; break; }
                                    for (size_t k = 0; k < amb[(size_t) v].size(); ++k)
                                        if (std::llabs (amb[(size_t) v][k] - sense[(size_t) v][k]) > 2) be = false;
                                }
                                // i la nota del primer temps hi és (cap volta comença muda)
                                for (int v = 1; v <= 2; ++v)
                                    if (amb[(size_t) v].empty() || sense[(size_t) v].empty() || amb[(size_t) v].front() > sense[(size_t) v].front() + 2) be = false;
                                if (! be)
                                {
                                    ++fallades;
                                    if (fallades < 4)
                                        std::printf ("        [!] %s %.0f bpm, bucle %.0f, retard %d, bloc %d: volta 2 %zu notes (sense retard %zu), primera a %lld\n",
                                                     patro, tempo, de, retard, bloc, amb[1].size(), sense[1].size(), amb[1].empty() ? -1LL : amb[1].front());
                                }
                            }
            std::printf ("        %d casos, %d amb notes perdudes o mogudes\n", casos, fallades);
            comprova (fallades == 0, "amb retard, cada volta sona igual que sense retard");
        });

        prova ("0.98 Play amb un plugin que retarda: la nota just al punt de play sona", []
        {
            const auto p = projecte098 (120.0, 2.0, "m01");
            const auto amb = voltes098 (p, 1500, 256, 1);
            comprova (! amb[0].empty() && amb[0].front() <= 1500 + 2, "la primera nota sona (com a molt, tard el que retarda el plugin)");
        });
    }
}
