// 1.3: comprovació del fitxer exportat (silencis, inici i final de cop, clips, true peak, dither).
namespace
{
    std::vector<float> intercalat103 (double fs, double silenciInici, double so, double fade, float amp, double silenciFinal = 0.0)
    {
        std::vector<float> v;
        const auto n0 = (size_t) (silenciInici * fs), n1 = (size_t) (so * fs), nf = (size_t) (fade * fs), n2 = (size_t) (silenciFinal * fs);
        v.assign (2 * n0, 0.0f);
        for (size_t i = 0; i < n1; ++i)
        {
            float g = amp;
            if (nf > 0 && i + nf > n1) g *= (float) (n1 - i) / (float) nf;   // fade de sortida
            const float x = g * (float) std::sin (2.0 * 3.14159265358979 * 220.0 * (double) i / fs);
            v.push_back (x); v.push_back (x);
        }
        v.insert (v.end(), 2 * n2, 0.0f);
        return v;
    }

    void proves103()
    {
        using namespace mibase;
        prova ("1.3 Comprovació d'exportació: silencis, de cop, clips, true peak i dither", []
        {
            const double fs = 48000;
            auto b = intercalat103 (fs, 0.5, 10, 2, 0.3f);
            auto c = comprovaExport (b.data(), (long long) (b.size() / 2), fs, 24);
            comprova (c.avisos.empty() && std::abs (c.silenciInici - 0.5) < 0.01 && ! c.dither, "una cançó ben feta: cap avís (" + std::to_string (c.avisos.size()) + ")");
            auto s = intercalat103 (fs, 3.5, 5, 1, 0.3f);
            comprova (comprovaExport (s.data(), (long long) (s.size() / 2), fs, 24).avisos.size() == 1, "3,5 s de silenci a l'inici: avisa");
            auto cop = intercalat103 (fs, 0, 5, 0, 0.3f);
            auto cc = comprovaExport (cop.data(), (long long) (cop.size() / 2), fs, 16);
            comprova (cc.deCopFinal && cc.avisos.size() >= 1 && cc.dither, "s'acaba de cop: avisa; 16 bits: dither");
            auto fort = intercalat103 (fs, 0, 2, 1, 1.2f);
            for (auto& x : fort) x = std::clamp (x, -1.0f, 1.0f);
            auto cf = comprovaExport (fort.data(), (long long) (fort.size() / 2), fs, 24);
            comprova (cf.clips > 0 && cf.truePeak > -0.1f, "clips i true peak al sostre");
            bool diuClips = false; for (auto& a : cf.avisos) diuClips = diuClips || a.find ("0 dBFS") != std::string::npos;
            comprova (diuClips, "ho diu");
            auto mig = intercalat103 (fs, 0.2, 4, 1, 0.94f);
            auto cm = comprovaExport (mig.data(), (long long) (mig.size() / 2), fs, 24);
            bool streaming = false; for (auto& a : cm.avisos) streaming = streaming || a.find ("streaming") != std::string::npos;
            comprova (streaming && cm.clips == 0, "pic de -0,5 dB: avisa per al streaming, sense clips");
        });

        prova ("1.3 exportaPla omple la comprovació", []
        {
            using namespace mibase::edicio;
            const double fs = 48000.0;
            Projecte p;
            p.clic = false;
            afegeixPista (p, "Veu", "audio");
            afegeixGravacio (p, 0, "Àudios/a.wav", "a", 4.0, 0.0, 0.0, 0.0);
            auto au = std::make_shared<AudioPresa>();
            au->fs = fs; au->canals = 1; au->longitud = (long long) (4 * fs);
            au->dades[0].assign ((size_t) au->longitud, 0.25f);
            MapaAudios mapa { { p.pistes[0].clips[0].presa, au } };
            auto pla = construeixPla (p, mapa);
            ComprovacioExport c;
            std::string error;
            const auto f = std::filesystem::temp_directory_path() / "mibase_103.wav";
            comprova (exportaPla (*pla, fs, 0.0, 4.0, 16, 1.0, f, error, {}, true, &c), "s'exporta: " + error);
            comprova (std::abs (c.segons - 4.05) < 0.1 && c.dither && c.deCopInici, "4 s, amb dither, i comença de cop (0,25 constant) (" + std::to_string (c.segons) + ")");
        });
    }
}
