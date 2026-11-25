// 1.10: diversos nuclis. El so ha de ser el mateix, bit a bit, amb un nucli o amb diversos.
#include "mibase/Reverb.h"
#include "mibase/Delay.h"
#include "mibase/Dinamica.h"
#include "mibase/Equalitzador.h"
#include "mibase/Lead.h"
#include "mibase/Neo.h"
#include "mibase/Vintage.h"
#include "mibase/Atmos.h"
#include "mibase/SubBass.h"
#include "mibase/GuitarRack.h"
#include "mibase/Multibanda.h"
#include "mibase/Saturacio.h"
#include "mibase/Doubler.h"
#include "mibase/Harmonitzador.h"
#include "mibase/Limitador.h"
#include <cstring>
#include <map>
namespace
{
    /** Fa sonar 5 s d'un projecte variat (sintes, àudio, busos, enviaments, sidechain) i en torna l'empremta. */
    unsigned long long empremta110 (int bloc, int nuclis)
    {
        using namespace mibase;
        using namespace mibase::edicio;
    const double fs = 48000.0;
        Projecte p;
        p.tempo = 120.0;
    
        // 0: sinte amb notes i reverb
        auto& a = afegeixPista (p, "Sinte", "midi");
        a.instrument = "intern:lead";
        { Clip c = midi::clipNou (0.0, 4.0); for (int k = 0; k < 8; ++k) midi::afegeix (c, { k * 480, 400, 60 + (k * 5) % 12, 100, 0 }); a.clips = { c }; }
        a.inserts.push_back ({ "i1", "intern:reverb", "Reverb", true, "", {} });
        a.pan = -0.4;
        // 1: àudio (serra) amb dinàmica que escolta el sinte (sidechain)
        auto& b = afegeixPista (p, "Guitarra", "audio");
        auto au = std::make_shared<AudioPresa>();
        au->fs = fs; au->canals = 2; au->longitud = (long long) (fs * 4.0);
        for (int c = 0; c < 2; ++c) { au->dades[c].resize ((size_t) au->longitud); for (long long i = 0; i < au->longitud; ++i) au->dades[c][(size_t) i] = 0.3f * (float) std::sin (0.013 * (double) i * (c + 1)) ; }
        const auto presa = afegeixPresa (p, "Àudios/g.wav", "g", 4.0).id;
        afegeixClip (p, 1, presa, 0.25);
        p.pistes[1].faderDb = -3.0;
        {
            Insert x { "i2", "intern:dinamica", "Dinàmica", true, "", {} };
            x.sidechain.push_back ({ p.pistes[0].id, -3.0, 1, true });
            p.pistes[1].inserts.push_back (x);
        }
        // 2: un altre sinte amb delay i EQ, cap a un bus
        auto& c2 = afegeixPista (p, "Sinte 2", "midi");
        c2.instrument = "intern:lead";
        { Clip c = midi::clipNou (0.0, 4.0); for (int k = 0; k < 6; ++k) midi::afegeix (c, { 240 + k * 600, 500, 48 + (k * 7) % 12, 90, 0 }); c2.clips = { c }; }
        p.pistes[2].inserts.push_back ({ "i3", "intern:delay", "Delay", true, "", {} });
        p.pistes[2].inserts.push_back ({ "i4", "intern:eq", "EQ", true, "", {} });
        auto& bus = afegeixBus (p, "Grup", false);
        p.pistes[2].sortida = bus.id;
        // retorn d'efectes amb reverb, amb enviament des de la guitarra
        auto& fx = afegeixBus (p, "Retorn", true);
        fx.inserts.push_back ({ "i5", "intern:reverb", "Reverb", true, "", {} });
        const std::string idFx = fx.id;
        for (int i = 0; i < (int) p.pistes.size(); ++i)
            if (p.pistes[(size_t) i].nom == "Guitarra") afegeixEnviament (p, i, idFx);
        // 3..10: més sintes senzills (perquè hi hagi feina per repartir)
        for (int k = 0; k < 8; ++k)
        {
            auto& t = afegeixPista (p, "S" + std::to_string (k), "midi");
            const char* insts[] = { "intern:neo", "intern:vintage", "intern:atmos", "intern:subbass", "intern:lead", "intern:neo", "intern:vintage", "intern:lead" };
            const char* efs[] = { "intern:guitarrack", "intern:multibanda", "intern:saturacio", "intern:doubler", "intern:harmonitzador", "intern:limitador", "intern:eq", "intern:reverb" };
            t.instrument = insts[k];
            Clip c = midi::clipNou (0.0, 4.0);
            midi::afegeix (c, { k * 120, 1500, 55 + k, 80, 0 });
            midi::afegeix (c, { 1800 + k * 60, 900, 60 + k, 70, 0 });
            t.clips = { c };
            t.inserts.push_back ({ "e" + std::to_string (k), efs[k], "Ef", true, "", {} });
        }
    
        MapaAudios m; m[presa] = au;
        std::map<std::string, std::shared_ptr<Instrument>> insts;
        auto pla = construeixPla (p, m, [&] (const Pista& t) -> std::shared_ptr<Instrument>
        {
            auto& s = insts[t.id];
            if (! s)
            {
                if (t.instrument == "intern:neo") s = std::make_shared<neo::Neo>();
                else if (t.instrument == "intern:vintage") s = std::make_shared<analogic::Vintage>();
                else if (t.instrument == "intern:atmos") s = std::make_shared<atmos::Atmos>();
                else if (t.instrument == "intern:subbass") s = std::make_shared<subbass::SubBass>();
                else s = std::make_shared<lead::Lead>();
                s->prepara (fs, 4096);
            }
            return s;
        }, [&] (const Insert& x) -> std::shared_ptr<Efecte>
        {
            std::shared_ptr<Efecte> e;
            if (x.plugin == "intern:reverb") e = std::make_shared<reverb::Reverb>();
            else if (x.plugin == "intern:delay") e = std::make_shared<delay::Delay>();
            else if (x.plugin == "intern:dinamica") e = std::make_shared<dinamica::Dinamica>();
            else if (x.plugin == "intern:eq") e = std::make_shared<eq::Equalitzador>();
            else if (x.plugin == "intern:guitarrack") e = std::make_shared<guitarrack::GuitarRack>();
            else if (x.plugin == "intern:multibanda") e = std::make_shared<multibanda::Multibanda>();
            else if (x.plugin == "intern:saturacio") e = std::make_shared<saturacio::Saturacio>();
            else if (x.plugin == "intern:doubler") e = std::make_shared<doubler::Doubler>();
            else if (x.plugin == "intern:harmonitzador") e = std::make_shared<harmonitzador::Harmonitzador>();
            else if (x.plugin == "intern:limitador") e = std::make_shared<limitador::Limitador>();
            if (e) e->prepara (fs, 4096);
            return e;
        });
    
    
        Mesclador mes;
        mes.prepara (fs, 2048);
        mes.setNuclis (nuclis);
        std::vector<float> L ((size_t) bloc), R ((size_t) bloc);
        unsigned long long h = 1469598103934665603ull;
        for (long long pos = 0; pos < (long long) (fs * 5.0); pos += bloc)
        {
            std::fill (L.begin(), L.end(), 0.0f);
            std::fill (R.begin(), R.end(), 0.0f);
            mes.processa (*pla, pos, bloc, L.data(), R.data(), false, nullptr, true);
            for (int i = 0; i < bloc; ++i)
                for (float v : { L[(size_t) i], R[(size_t) i] })
                {
                    unsigned int u; std::memcpy (&u, &v, 4);
                    h = (h ^ u) * 1099511628211ull;
                }
        }
        return h;
    }

    void proves110()
    {
        prova ("1.10 nuclis: el so és idèntic (bit a bit) amb un nucli i amb diversos", []
        {
            const int mx = mibase::Mesclador::maxAjudants() + 1;
            for (int bloc : { 64, 100, 256 })
            {
                const auto u = empremta110 (bloc, 1);
                comprova (u != 0, "sona");
                for (int k : { 2, 4 })
                    comprova (empremta110 (bloc, k) == u, "bloc de " + std::to_string (bloc) + " amb " + std::to_string (k)
                              + " nuclis (aquí n'hi ha " + std::to_string (mx) + ")");
            }
        });
        prova ("1.10 nuclis: es pot encendre i apagar mentre sona", []
        {
            mibase::Mesclador m;
            m.prepara (48000.0, 2048);
            comprova (m.getNuclis() == 1, "per defecte, un");
            m.setNuclis (2);
            comprova (m.getNuclis() == std::min (2, mibase::Mesclador::maxAjudants() + 1), "en demana dos");
            m.setNuclis (1);
            comprova (m.getNuclis() == 1, "torna a un");
        });
    }
}
