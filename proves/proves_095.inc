// 0.95: perfils de màster i l'assistent «Ajusta al perfil».
namespace
{
    void proves095()
    {
        using namespace mibase;
        prova ("0.95 perfils: CD Master configurable, -0,3 dBTP; sense presets de Carmen 113", []
        {
            using namespace mibase::rack;
            Rack r (Tipus::Master);
            r.posaEstat ({});
            int cd = -1;
            for (int k = 0; k < (int) loudness::objectius().size(); ++k) if (loudness::objectius()[(size_t) k].configurable) cd = k;
            comprova (cd > 0 && loudness::objectius()[(size_t) cd].truePeak > -0.35f, "hi ha el perfil CD (-0,3 dBTP)");
            r.posaObjectiu (cd); r.posaLufsPersonal (-12.0f);
            comprova (std::abs (r.lufsObjectiu() + 12.0f) < 0.01f && std::abs (r.truePeakObjectiu() + 0.3f) < 0.01f, "el CD fa servir els LUFS triats");
            Rack c (Tipus::Master); c.posaEstat (r.estat());
            comprova (std::abs (c.lufsPersonal() + 12.0f) < 0.01f && c.objectiu() == cd, "es desa amb el rack");
            r.posaObjectiu (0);
            comprova (r.lufsObjectiu() == 0.0f, "«Cap»: sense objectiu");
            bool cap = true;
            for (auto& p : presetsFabrica (Tipus::Master)) cap = cap && p.nom.find ("Carmen") == std::string::npos;
            comprova (cap, "cap preset de Carmen 113");
        });

        prova ("0.95 posaGuanyLimitador: el macro Loudness segueix el guany (i s'estén si cal)", []
        {
            using namespace mibase::rack;
            Rack r (Tipus::Master);
            r.posaEstat ({});
            posaGuanyLimitador (r, 6.0f);
            comprova (std::abs (guanyLimitador (r) - 6.0f) < 0.05f && std::abs (r.macro (7) - 0.5f) < 0.02f, "6 dB: el macro a la meitat");
            posaGuanyLimitador (r, 16.0f);
            comprova (std::abs (guanyLimitador (r) - 16.0f) < 0.05f && r.macro (7) > 0.99f, "16 dB (fora del rang): el macro al màxim");
            r.posaMacro (7, 0.0f);
            comprova (guanyLimitador (r) < 0.1f, "i el macro el torna a baixar");
            posaSostre (r, -1.5f);
            Rack s (Tipus::Master);
            comprova (limitadorFinal (s) < 0, "sense mòduls, sense limitador");
        });

        prova ("0.95 Ajusta al perfil: arriba als LUFS de l'objectiu i s'atura si cal massa limitador", []
        {
            using namespace mibase::rack;
            auto simula = [] (Rack& r, AssistentLoudness& a, float lufs, float tp, double segonsMax)
            {
                r.prepara (48000, 480);
                a.comenca (r, lufs, tp);
                auto s = soroll094 (48000, segonsMax, -24);
                for (size_t o = 0; o + 4800 <= s.L.size() && a.actiu(); o += 4800)
                {
                    for (int k = 0; k < 10; ++k) r.processa (s.L.data() + o + (size_t) k * 480, s.R.data() + o + (size_t) k * 480, 480);
                    a.actualitza (r);
                }
                return a.estat;
            };
            Rack r (Tipus::Master);
            r.posaEstat ({});
            AssistentLoudness a;
            const auto e = simula (r, a, -14.0f, -1.0f, 150.0);
            comprova (e == AssistentLoudness::Estat::Fet, "s'acaba (estat " + std::to_string ((int) e) + ", voltes " + std::to_string (a.iteracions) + ")");
            // comprovació independent: 10 s més pel màster ajustat
            auto s = soroll094 (48000, 14, -24, 999u);
            processa094 (r, s, 480);
            const auto m = loudness::analitza (s.L.data() + 48000 * 4, s.R.data() + 48000 * 4, s.L.size() - 48000 * 4, 48000);
            comprova (std::abs (m.integrated + 14.0f) < 0.8f, "el màster és a -14 LUFS (" + std::to_string (m.integrated) + ")");
            comprova (m.truePeakMax <= -0.8f, "i el true peak sota -1 dBTP (" + std::to_string (m.truePeakMax) + ")");
            Rack q (Tipus::Master);
            q.posaEstat ({});
            AssistentLoudness b;
            const auto e2 = simula (q, b, -4.0f, -1.0f, 150.0);
            comprova (e2 == AssistentLoudness::Estat::Massa || e2 == AssistentLoudness::Estat::NoArriba, "-4 LUFS: no ho força (estat " + std::to_string ((int) e2) + ")");
            Rack buit (Tipus::Master);
            AssistentLoudness c;
            c.comenca (buit, -14.0f, -1.0f);
            comprova (c.estat == AssistentLoudness::Estat::SenseLimitador, "sense limitador, ho diu");
        });
    }
}
