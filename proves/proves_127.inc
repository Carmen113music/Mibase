// 1.27: Mix Coach (fase 4): entendre la pregunta i respondre només amb les dades de l'anàlisi.
#include "mibase/MixCoach.h"

namespace
{
    void proves127()
    {
        using namespace mibase;
        namespace co = mibase::mix::coach;

        // una veu tapada per una guitarra, i un baix
        const auto fnVeu = [] (double f) { return rosa (f) * (f > 1000 && f < 5000 ? 3.0 : 1.0) * (f < 150 ? 0.1 : 1.0) * (f > 6000 ? 0.3 : 1.0); };
        static const auto veu = escala (sorollAmb (fnVeu, 51), 0.2f);
        static const auto gtr = sorollAmb ([] (double f) { return rosa (f) * (f > 1000 && f < 5000 ? 3.0 : 1.0) * (f > 6000 ? 0.3 : 1.0); }, 52);
        static const auto baix = sorollAmb ([] (double f) { return f < 400 ? rosa (f) : 0.0; }, 53);
        static const auto aV = mx::analitzaPista (veu.data(), veu.data(), veu.size(), 48000.0);
        static const auto aG = mx::analitzaPista (gtr.data(), gtr.data(), gtr.size(), 48000.0);
        static const auto aB = mx::analitzaPista (baix.data(), baix.data(), baix.size(), 48000.0);
        auto context = [] (bool analitzat)
        {
            co::Context c;
            c.analitzat = analitzat;
            c.pistes = { pistaMescla ("t1", "Veu principal", mx::Instrument::Veu, aV), pistaMescla ("t2", "Gtr rítmica", mx::Instrument::GuitarraElectrica, aG),
                         pistaMescla ("t3", "Baix", mx::Instrument::Baix, aB) };
            for (const auto& p : c.pistes)
                for (auto& i : mx::diagnosticaPista (p.id, p.nom, *p.analisi, p.ident).incidencies) c.incidencies.push_back (i);
            for (auto& i : mx::conflictes (c.pistes)) c.incidencies.push_back (i);
            std::vector<mx::CanviBalanc> b;
            for (auto& i : mx::balanc (c.pistes, b)) c.incidencies.push_back (i);
            c.balanc = b;
            return c;
        };

        prova ("1.27 Mix Coach: sense anàlisi no respon a cegues", [&]
        {
            co::Conversa conv;
            const auto r = co::respon ("Per què la meva veu no sobresurt?", context (false), conv);
            comprova (r.text.find ("Encara no he analitzat") != std::string::npos, "demana analitzar primer");
            co::Conversa c2;
            comprova (co::respon ("torna a analitzar", context (false), c2).accio == "analitza", "«torna a analitzar» és una acció");
        });

        prova ("1.27 Mix Coach: per què la veu no sobresurt", [&]
        {
            const auto ctx = context (true);
            co::Conversa conv;
            const auto r = co::respon ("Per què la meva veu no sobresurt?", ctx, conv);
            comprova (r.entesa && r.text.find ("«Veu principal»") != std::string::npos, "parla de la veu");
            comprova (r.text.find ("Gtr rítmica") != std::string::npos, "diu que la guitarra la tapa (dades reals)");
            comprova (r.text.find ("per sota de la resta") != std::string::npos, "diu com sona respecte de la resta");
            comprova (! r.enllacos.empty() && ! conv.incidencies.empty(), "amb enllaços per anar-hi i preparar-ho");
            // «i com ho arreglo?»: de les mateixes coses
            const auto s = co::respon ("I com ho arreglo?", ctx, conv);
            comprova (s.entesa && s.text.find ("La meva proposta") != std::string::npos, "dona la solució del que acabava d'explicar");
            const auto a = co::respon ("quines alternatives hi ha?", ctx, conv);
            comprova (a.text.find ("Sidechain") != std::string::npos || a.text.find ("sidechain") != std::string::npos, "i les alternatives (amb pros i contres)");
        });

        prova ("1.27 Mix Coach: temes, pistes i continuació", [&]
        {
            const auto ctx = context (true);
            co::Conversa conv;
            auto r = co::respon ("Què passa amb les guitarres?", ctx, conv);
            comprova (r.text.find ("«Gtr rítmica»") != std::string::npos, "les guitarres són la «Gtr rítmica»");
            r = co::respon ("Per què el baix sona sense definició?", ctx, conv);
            comprova (r.text.find ("«Baix»") != std::string::npos, "el baix");
            r = co::respon ("Què canviaries d'aquesta mescla?", ctx, conv);
            comprova (r.text.find ("començar") != std::string::npos && ! r.enllacos.empty(), "un resum per on començar");
            r = co::respon ("La mescla sona massa tancada?", ctx, conv);
            comprova (r.entesa && r.text.find ("tancada") != std::string::npos, "entén «tancada»");
            co::Conversa c2;
            co::respon ("la veu està massa enrere?", ctx, c2);
            r = co::respon ("i el baix?", ctx, c2);
            comprova (r.text.find ("«Baix»") != std::string::npos && r.text.find ("sobresurt") != std::string::npos, "«i el baix?» continua el tema");
        });

        prova ("1.27 Mix Coach: honest amb el que no sap", [&]
        {
            const auto ctx = context (true);
            co::Conversa conv;
            auto r = co::respon ("Com puc apropar el so a una producció de Coldplay?", ctx, conv);
            comprova (r.text.find ("No he escoltat") != std::string::npos, "no fa veure que ha escoltat Coldplay");
            r = co::respon ("quin temps farà demà?", ctx, conv);
            comprova (! r.entesa && ! r.suggeriments.empty(), "si no ho entén, ho diu i proposa preguntes");
            r = co::respon ("Ha millorat després dels canvis?", ctx, conv);
            comprova (r.text.find ("torna a analitzar") != std::string::npos, "sense dues anàlisis, no s'inventa la comparació");
            auto ctx2 = ctx;
            ctx2.seguiment = { { mx::Evolucio::Solucionat, "fang:t1", "t1", "Excés de mitjos baixos" } };
            r = co::respon ("Ha millorat després dels canvis?", ctx2, conv);
            comprova (r.text.find ("Excés de mitjos baixos") != std::string::npos && r.text.find ("1 solucionades") != std::string::npos, "amb seguiment, el diu");
        });
    }
}
