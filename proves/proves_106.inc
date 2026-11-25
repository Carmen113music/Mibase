// 1.6: Guitar Rack fase 2 (IR de pantalla, MIDI learn i presets de pedals i amplis).
#include "mibase/Wav.h"
namespace
{
    void proves106()
    {
        using namespace mibase::guitarrack;
        const double fs = 48000.0;

        prova ("1.6 GUITAR RACK: IR de pantalla (sona com la simulació, es barreja, es desa i es treu)", [fs]
        {
            // la IR: la resposta de la nostra pròpia pantalla
            const auto dir = std::filesystem::temp_directory_path() / "mibase_106";
            std::filesystem::create_directories (dir);
            const auto fitxer = dir / "pantalla.wav";
            {
                GuitarRack r;
                r.buidaPlaca(); r.posa (AmpOn, 0.0f); r.posa (Room, 0.0f);
                r.prepara (fs, 512);
                std::vector<float> L (4800, 0.0f), R (4800, 0.0f);
                L[0] = R[0] = 1.0f;
                for (int i = 0; i < 4800; i += 512) r.processa (L.data() + i, R.data() + i, std::min (512, 4800 - i));
                for (auto& x : L) x *= 0.5f;
                mibase::EscriptorWav w;
                comprova (w.obre (fitxer, 1, fs, 24) && w.escriu (L.data(), 4800) && w.tanca(), "desa la IR");
            }
            const auto in = guitarraGR (fs, 2.0);
            auto so = [&] (bool ambIR, float mescla)
            {
                GuitarRack r;
                r.buidaPlaca(); r.posa (Room, 0.0f);
                if (ambIR) { std::string err; comprova (r.carregaIR (fitxer.string(), err), "carrega la IR: " + err); r.posa (IrMescla, mescla); }
                r.prepara (fs, 256);
                return passaGR (r, in);
            };
            const auto sim = so (false, 0.0f), ir = so (true, 100.0f), mig = so (true, 50.0f);
            const double dSim = 20.0 * std::log10 (rmsGR (sim.L, 4800)), dIr = 20.0 * std::log10 (rmsGR (ir.L, 4800) + 1e-12);
            comprova (finitsGR (ir.L) && finitsGR (mig.L), "finit");
            comprova (std::abs (dSim - dIr) < 3.0, "la IR sona al mateix volum que la simulació: " + std::to_string (dSim) + " / " + std::to_string (dIr) + " dB");
            comprova (rmsGR (mig.L, 4800) > 1e-3, "barrejada també sona");
            GuitarRack r;
            std::string err;
            r.carregaIR (fitxer.string(), err);
            comprova (r.valor (IrOn) > 0.5f && r.nomIR() == "pantalla", "encesa i amb el nom del fitxer");
            GuitarRack s;
            s.posaEstat (r.estat());
            comprova (s.referenciaIR() == r.referenciaIR() && s.valor (IrOn) > 0.5f && ! s.irFalta(), "es desa i es recupera");
            s.treuIR();
            comprova (s.referenciaIR().empty() && s.valor (IrOn) < 0.5f, "es treu");
            GuitarRack t;
            t.posaEstat (r.estat());
            std::filesystem::remove_all (dir);
            GuitarRack u;
            u.posaEstat (r.estat());
            comprova (u.irFalta(), "si el fitxer no hi és, ho sap (i sona la simulació)");
        });

        prova ("1.6 GUITAR RACK: MIDI learn (aprendre, moure, un CC per paràmetre, es desa)", []
        {
            GuitarRack r;
            r.buidaPlaca();
            r.afegeix ("crybaby");
            const int p = paramPedal (0, 0);
            r.aprenMidi (p);
            comprova (r.paramAprenent() == p, "aprenent");
            GuitarRack::ccPerATots (11, 127);
            comprova (r.ccDe (p) == 11 && r.paramAprenent() < 0, "ha après el CC 11");
            comprova (std::abs (r.pedal (0)->valor (0) - 100.0f) < 0.01f, "el pedal a dalt de tot");
            GuitarRack::ccPerATots (11, 0);
            comprova (r.pedal (0)->valor (0) < 0.01f, "el pedal a baix de tot");
            r.aprenMidi (AmpGain);
            r.ccMidi (11, 64);
            comprova (r.ccDe (AmpGain) == 11 && r.ccDe (p) == -1, "un CC mou un sol paràmetre");
            comprova (std::abs (r.valor (AmpGain) - 10.0f * 64.0f / 127.0f) < 0.01f, "el Gain al mig");
            GuitarRack s;
            s.posaEstat (r.estat());
            comprova (s.ccDe (AmpGain) == 11, "es desa amb el projecte");
            s.treuMidi (AmpGain);
            comprova (s.ccDe (AmpGain) == -1 && ! s.ccMidi (11, 30), "es treu");
        });

        prova ("1.6 GUITAR RACK: presets de cada pedal i de cada ampli (de fàbrica i propis)", []
        {
            const auto ts = presetsFabricaPedal ("ts808");
            const auto jc = presetsFabricaAmpli ("jcm800");
            comprova (! ts.empty() && ! jc.empty(), "en surten de les cadenes de fàbrica: " + std::to_string (ts.size()) + " i " + std::to_string (jc.size()));
            GuitarRack r;
            r.buidaPlaca();
            r.afegeix ("ts808");
            r.pedal (0)->posa (0, 9.0f);
            const auto e = estatPedal (*r.pedal (0));
            r.pedal (0)->posa (0, 1.0f);
            posaEstatPedal (*r.pedal (0), e);
            comprova (std::abs (r.pedal (0)->valor (0) - 9.0f) < 1e-4f, "pedal: torna igual");
            r.posaAmpli (indexAmpli ("jcm2000"));
            r.posa (Canal, 1.0f); r.posa (AmpTreble, 8.5f);
            const auto a = estatAmpli (r);
            r.posaAmpli (indexAmpli ("jcm2000"));
            posaEstatAmpli (r, a);
            comprova (r.valor (Canal) > 0.5f && std::abs (r.valor (AmpTreble) - 8.5f) < 1e-4f, "ampli: torna igual");
            const auto f = std::filesystem::temp_directory_path() / "mibase_106_peces.json";
            PresetsPeces pp;
            pp.posa ("p:ts808", "Meu", e);
            pp.posa ("a:jcm2000", "Meu", a);
            comprova (pp.desa (f), "desa");
            PresetsPeces q;
            comprova (q.carrega (f) && q.de ("p:ts808").size() == 1 && q.de ("a:jcm2000")[0].second == a, "carrega");
            comprova (q.esborra ("p:ts808", "Meu") && q.de ("p:ts808").empty(), "esborra");
            std::filesystem::remove (f);
        });
    }
}
