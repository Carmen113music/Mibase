#include "mibase/MixCoach.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace mibase::mix::coach
{
    namespace
    {
        // ---------------- text ----------------
        std::string normalitza (const std::string& s)
        {
            static const std::pair<const char*, char> taula[] = {
                { "à", 'a' }, { "á", 'a' }, { "è", 'e' }, { "é", 'e' }, { "í", 'i' }, { "ï", 'i' }, { "ò", 'o' }, { "ó", 'o' },
                { "ú", 'u' }, { "ü", 'u' }, { "ç", 'c' }, { "ñ", 'n' }, { "À", 'a' }, { "È", 'e' }, { "É", 'e' }, { "Í", 'i' },
                { "Ò", 'o' }, { "Ó", 'o' }, { "Ú", 'u' }, { "Ç", 'c' }, { "Ñ", 'n' }, { "·", 'l' } };
            std::string r = " ";
            for (size_t i = 0; i < s.size();)
            {
                bool fet = false;
                for (const auto& [u, c] : taula)
                {
                    const size_t m = std::char_traits<char>::length (u);
                    if (s.compare (i, m, u) == 0) { r += c; i += m; fet = true; break; }
                }
                if (fet) continue;
                char c = s[i++];
                if (c >= 'A' && c <= 'Z') c = (char) (c + 32);
                r += (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ? c : ' ';
            }
            // espais simples, i un al principi i al final (per buscar paraules senceres amb « paraula »)
            std::string q;
            for (char c : r) if (! (c == ' ' && ! q.empty() && q.back() == ' ')) q += c;
            if (q.back() != ' ') q += ' ';
            return q;
        }
        /** Hi ha alguna d'aquestes arrels? (« veu » = paraula sencera; «sobresurt» = comença així). */
        bool te (const std::string& q, std::initializer_list<const char*> arrels)
        {
            for (const char* a : arrels)
            {
                const std::string s (a);
                if (s.front() == ' ' ? q.find (s) != std::string::npos : q.find (" " + s) != std::string::npos) return true;
            }
            return false;
        }
        std::string num (double v, int d = 1)
        {
            char b[32];
            std::snprintf (b, sizeof b, "%.*f", d, v);
            std::string s (b);
            for (auto& c : s) if (c == '.') c = ',';
            return s;
        }
        std::string cm (const std::string& n) { return "«" + n + "»"; }
        std::string llista (const std::vector<std::string>& v)
        {
            std::string r;
            for (size_t i = 0; i < v.size(); ++i) r += (i == 0 ? "" : i + 1 == v.size() ? " i " : ", ") + v[i];
            return r;
        }

        // ---------------- temes ----------------
        enum Tema { Cap = -1, Resum, Destacar, Tancada, Embolicada, Greus, Dura, Volum, Estereo, Dinamica, Esses,
                    Solucio, Alternatives, Seguiment, Analitza, Referencia, Salutacio, Gracies, Ajuda };

        Tema tema (const std::string& q)
        {
            // l'ordre compta: primer el que és més concret
            if (te (q, { "torna a analitzar", "reanalitz", " analitza ", "analitza la", "analitza-ho", "analitza ho" })) return Analitza;
            if (te (q, { "sonar com", "sona com", "apropar", "aproparme", "acostar", "com a la ", "com els ", "estil de", "produccio de", "com en ", "semblant a" })) return Referencia;
            if (te (q, { "avantatg", "inconvenient", " pros ", "contres", "alternativ", "altres opcions", "altra opcio", "altres maneres", "una altra manera" })) return Alternatives;
            if (te (q, { "ha millorat", "han millorat", "ha quedat", "com queda", "despres dels canvis", "ha funcionat", "ha servit", "millora ara", "ha anat be" })) return Seguiment;
            if (te (q, { "esses", " essa", "sibil", " ss ", "xiulen" })) return Esses;
            if (te (q, { "tota la banda", "tothom", "entra la banda", "entren tots", "entra tot", "quan sonen tots", "embolic", "fang", "bola", "dens",
                         "empastad", "brut", "bruta", "confus", "tapad", "atapeid" })) return Embolicada;
            if (te (q, { "tancad", "apagad", "fosc", "obert", "oberta", "aire", "brillant", "brillantor", "brill", "manta", "llunyan" })) return Tancada;
            if (te (q, { " dur ", " dura ", "estrident", "punxeg", "cansa", "agressiv", "aspr", "xiscl" })) return Dura;
            if (te (q, { "greu", " boom", "retruny", "bomboll", "pesad", " pes ", "sub " })) return Greus;
            if (te (q, { "estereo", "ample", "ampla", " mono", " fase", "panor", " pan ", "costat", "esquerr", "dret", "dreta" })) return Estereo;
            if (te (q, { "compres", "compri", "dinamic", "puja i baixa", "irregular", "constant", "es perden paraules", "paraules" })) return Dinamica;
            if (te (q, { "sobresurt", "sobresurten", "no s ent", "s entengui", "enrere", "darrere", "tapa", "tapen", "clar", "definic", "defini",
                         "s amaga", "amaga", "perd", "no es sent", "no se sent", "destaca", "presenc", "davant", "sense cos" })) return Destacar;
            if (te (q, { " fort", "fluix", "volum", "nivell", "equilibri", "balanc", "massa alt", "massa baix" })) return Volum;
            if (te (q, { "com ho arreglo", "arregl", "soluc", "que hi faig", "que faig", "com puc", "com ho faig", "com ho milloro", "millorar", "milloro",
                         "que faries", "que hi puc fer" })) return Solucio;
            if (te (q, { "que canviaries", "que hi veus", "com esta", "com la veus", "per on comen", "prioritat", "resum", "que falla", "que passa",
                         "que no funciona", "problemes", "que li passa", "com sona" })) return Resum;
            if (te (q, { "hola", "bon dia", "bona tarda", "bones" })) return Salutacio;
            if (te (q, { "graci", "merci", "perfecte", "entes" })) return Gracies;
            if (te (q, { "ajuda", "que et puc", "que puc preguntar", "com funciones", "que saps" })) return Ajuda;
            return Cap;
        }

        /** Les incidències que parlen d'aquest tema (per l'inici de l'id). */
        bool deTema (const Incidencia& i, Tema t)
        {
            auto ve = [&] (std::initializer_list<const char*> p) { for (const char* x : p) if (i.id.rfind (x, 0) == 0) return true; return false; };
            switch (t)
            {
                case Destacar: return ve ({ "conflicte:", "definicio:", "dinamica:", "sibilancia:", "fang:", "parts:" })
                                   || (ve ({ "balanc:" }) && (i.id.find ("veu-enrere") != std::string::npos || i.id.find (":fluix") != std::string::npos
                                                               || i.id.find ("protagonistes") != std::string::npos || i.id.find ("domina") != std::string::npos));
                case Tancada: return ve ({ "mescla:falta:brillantor", "mescla:falta:pres", "mescla:sobra:mitjos baixos", "aguts:", "definicio:", "fang:" });
                case Embolicada: return ve ({ "fang:", "mescla:sobra:mitjos baixos", "mescla:densitat", "conflicte:", "greus:" });
                case Greus: return ve ({ "greus:", "tall:", "mescla:sobra:greus", "mescla:falta:greus", "monogreus:" })
                                   || (ve ({ "conflicte:" }) && i.titol.find ("bombo") != std::string::npos);
                case Dura: return ve ({ "dur:", "mescla:sobra:pres", "sibilancia:", "aguts:", "ressonancia:" });
                case Volum: return ve ({ "balanc:", "parts:" }) && i.proposta.tipus != Proposta::Panorama;
                case Estereo: return ve ({ "fase:", "monogreus:", "mescla:mono" }) || i.proposta.tipus == Proposta::Panorama;
                case Dinamica: return ve ({ "dinamica:", "aixafat:", "parts:" });
                case Esses: return ve ({ "sibilancia:" });
                default: return true;
            }
        }

        // ---------------- pistes ----------------
        struct Paraula { const char* arrel; std::vector<Instrument> inst; };
        const std::vector<Paraula>& paraulesInstrument()
        {
            using I = Instrument;
            static const std::vector<Paraula> v {
                { "veu", { I::Veu } }, { " vocal", { I::Veu } }, { "cantant", { I::Veu } }, { " cors ", { I::Cors } }, { "segones veus", { I::Cors } },
                { " baix", { I::Baix } }, { "bombo", { I::Bombo } }, { " kick", { I::Bombo } }, { "caixa", { I::Caixa } }, { "snare", { I::Caixa } },
                { " toms", { I::Toms } }, { " plats", { I::Plats } }, { "charles", { I::Plats } },
                { "bateria", { I::Bateria, I::Bombo, I::Caixa, I::Toms, I::Plats, I::Percussio } },
                { "guitarr", { I::GuitarraElectrica, I::GuitarraAcustica } }, { " gtr", { I::GuitarraElectrica, I::GuitarraAcustica } },
                { "acustica", { I::GuitarraAcustica } }, { "electrica", { I::GuitarraElectrica } },
                { "teclat", { I::Teclats, I::Piano } }, { "piano", { I::Piano } }, { "orgue", { I::Teclats } }, { "rhodes", { I::Teclats } },
                { "sinte", { I::Sinte } }, { "synth", { I::Sinte } }, { " pad", { I::Pad } }, { "cordes", { I::Corda } }, { "vents", { I::Vents } },
                { "percussio", { I::Percussio } },
            };
            return v;
        }
        /** Les pistes que surten a la pregunta (pel nom o per l'instrument). */
        std::vector<std::string> pistesDe (const std::string& q, const Context& ctx)
        {
            std::vector<std::string> r;
            auto afegeix = [&] (const std::string& id) { if (std::find (r.begin(), r.end(), id) == r.end()) r.push_back (id); };
            // pel nom (sencer, com a paraules)
            for (const auto& p : ctx.pistes)
            {
                const auto n = normalitza (p.nom);
                if (n.size() > 3 && q.find (n) != std::string::npos) afegeix (p.id);
            }
            if (! r.empty()) return r;
            for (const auto& w : paraulesInstrument())
            {
                if (! te (q, { w.arrel })) continue;
                for (const auto& p : ctx.pistes)
                    if (std::find (w.inst.begin(), w.inst.end(), p.ident.instrument) != w.inst.end()) afegeix (p.id);
            }
            return r;
        }
        const PistaMescla* pista (const Context& ctx, const std::string& id)
        {
            for (const auto& p : ctx.pistes) if (p.id == id) return &p;
            return nullptr;
        }
        std::vector<std::string> nomsDe (const Context& ctx, const std::vector<std::string>& ids)
        {
            std::vector<std::string> v;
            for (const auto& id : ids) if (const auto* p = pista (ctx, id)) v.push_back (cm (p->nom));
            return v;
        }
        bool tocaPista (const Incidencia& i, const std::vector<std::string>& ids)
        {
            for (const auto& id : ids)
                if (i.pista == id || std::find (i.pistes.begin(), i.pistes.end(), id) != i.pistes.end()) return true;
            return false;
        }
        bool visible (const Context& ctx, const Incidencia& i) { return ! ctx.creacio || i.origen; }

        /** Explica unes incidències (com a molt n), amb els enllaços. */
        void explica (Resposta& r, Conversa& conv, const std::vector<const Incidencia*>& v, const Context& ctx, size_t n = 3)
        {
            conv.incidencies.clear();
            for (size_t k = 0; k < v.size() && k < n; ++k)
            {
                const auto& i = *v[k];
                std::string p = i.titol;
                if (! i.on.empty() && i.on != "a tota la pista" && i.on != "a tota la cançó") p += " (" + i.on + ")";
                p += ". " + i.queVeig + " " + i.perque + " Què proposo: " + i.solucio;
                r.text += "\n\n" + p;
                conv.incidencies.push_back (i.id);
                const std::string idPista = ! i.pista.empty() ? i.pista : (! i.pistes.empty() ? i.pistes.front() : std::string());
                if (! idPista.empty())
                {
                    const auto* pp = pista (ctx, idPista);
                    r.enllacos.push_back ({ espotPreparar (i) ? "Prepara-la a " + (pp != nullptr ? cm (pp->nom) : std::string ("la pista"))
                                                              : "Vés a " + (pp != nullptr ? cm (pp->nom) : std::string ("la pista")), idPista, i.id });
                }
            }
            if (v.size() > n) r.text += "\n\nN'hi ha " + std::to_string (v.size() - n) + " més, menys importants: les tens a la llista.";
        }

        std::vector<const Incidencia*> cerca (const Context& ctx, const std::vector<std::string>& ids, Tema t)
        {
            std::vector<const Incidencia*> v;
            for (const auto& i : ctx.incidencies)
                if (visible (ctx, i) && (ids.empty() || tocaPista (i, ids)) && (t == Cap || t == Resum || deTema (i, t))) v.push_back (&i);
            std::stable_sort (v.begin(), v.end(), [] (auto* a, auto* b) { return a->gravetat > b->gravetat; });
            return v;
        }

        const char* nomTema (Tema t)
        {
            switch (t)
            {
                case Destacar: return "per què no sobresurt o no s'entén";
                case Tancada: return "si sona tancada o apagada";
                case Embolicada: return "si s'embolica";
                case Greus: return "els greus";
                case Dura: return "si sona dura o estrident";
                case Volum: return "el volum i l'equilibri";
                case Estereo: return "l'estèreo, el panorama i la fase";
                case Dinamica: return "la dinàmica (el que puja i baixa)";
                case Esses: return "les esses";
                default: return "això";
            }
        }

        /** Xifres que expliquen una pista encara que no hi hagi cap problema. */
        std::string xifres (const Context& ctx, const PistaMescla& p)
        {
            std::string r;
            double seg = 0.0;
            const double d = nivellRelatiu (ctx.pistes, { p.id }, &seg);
            if (seg >= 3.0) r += cm (p.nom) + " sona " + num (std::abs (d)) + " dB " + (d >= 0 ? "per sobre" : "per sota") + " de la resta quan toca";
            if (p.analisi != nullptr && p.analisi->valid)
                r += std::string (r.empty() ? "" : "; ") + "té " + num (p.analisi->rangDinamicDb) + " dB de diferència entre les parts fluixes i les fortes";
            return r.empty() ? r : r + ".";
        }

        void suggeriments (Resposta& r, const Context& ctx, Tema t, const std::vector<std::string>& ids)
        {
            std::vector<std::string> s;
            const auto noms = nomsDe (ctx, ids);
            if (! ids.empty() && t != Solucio) s.push_back ("Com ho arreglo?");
            if (! ids.empty() && t != Alternatives) s.push_back ("Quines alternatives hi ha?");
            if (t != Resum) s.push_back ("Què canviaries d'aquesta mescla?");
            if (t != Embolicada) s.push_back ("Per què quan entra tota la banda perdo claredat?");
            if (t != Tancada) s.push_back ("La mescla sona massa tancada?");
            if (! ctx.seguiment.empty() && t != Seguiment) s.push_back ("Ha millorat després dels canvis?");
            if (s.size() > 4) s.resize (4);
            r.suggeriments = s;
        }
    }

    std::vector<std::string> preguntesInicials()
    {
        return { "Què canviaries d'aquesta mescla?", "Per què la meva veu no sobresurt?", "Per què el baix sona sense definició?",
                 "Què passa amb les guitarres?", "La mescla sona massa tancada?", "Per què quan entra tota la banda perdo claredat?" };
    }

    Resposta respon (const std::string& pregunta, const Context& ctx, Conversa& conv)
    {
        Resposta r;
        const auto q = normalitza (pregunta);
        auto t = tema (q);
        auto ids = pistesDe (q, ctx);
        // «i el baix?»: el mateix tema d'abans amb una altra pista; «i com ho arreglo?»: de les mateixes pistes
        const bool continua = te (q, { " i ", " i el", " i la", " i les", " i els" }) && q.size() < 40;
        if (t == Cap && ! ids.empty() && conv.tema >= 0 && continua) t = (Tema) conv.tema;
        if (t == Cap && ! ids.empty()) t = Resum;   // «què passa amb les guitarres?» sense tema concret
        if (ids.empty() && (t == Solucio || t == Alternatives || (continua && t != Cap))) ids = conv.pistes;

        if (t == Salutacio) { r.text = "Hola! Pregunta'm el que vulguis de la mescla: com sona una pista, per què no destaca, com arreglar-ho…"; r.suggeriments = preguntesInicials(); r.suggeriments.resize (4); return r; }
        if (t == Gracies) { r.text = "De res! Quan facis canvis, torna a analitzar i et diré com ha quedat."; r.suggeriments = { "Torna a analitzar", "Què canviaries d'aquesta mescla?" }; return r; }
        if (t == Ajuda)
        {
            r.text = "Entenc preguntes sobre la mescla i les pistes, per exemple sobre la veu, el baix, el bombo, la caixa, les guitarres o els teclats, "
                     "i sobre si sona tancada, embolicada, dura, amb massa greus, el volum, l'estèreo, la dinàmica o les esses. També et puc dir com arreglar-ho "
                     "i quines alternatives hi ha, i si ha millorat després dels canvis. Només responc amb el que he mesurat: no he escoltat res més.";
            r.suggeriments = preguntesInicials();
            return r;
        }
        if (t == Analitza) { r.text = "D'acord: torno a analitzar la mescla."; r.accio = "analitza"; return r; }
        if (! ctx.analitzat)
        {
            r.text = "Encara no he analitzat la mescla, i no et vull respondre a cegues. Prem «Analitza la mescla» (o digues-me «analitza») i després pregunta-m'ho.";
            r.suggeriments = { "Analitza la mescla" };
            r.accio = "";
            return r;
        }

        if (t == Referencia)
        {
            r.text = "No he escoltat aquesta producció: no la tinc ni la puc analitzar, així que no et puc dir com s'hi acosta la teva mescla sense fer-ho veure. ";
            std::vector<std::string> fets;
            for (const auto& i : ctx.incidencies) if (i.deMescla && visible (ctx, i)) fets.push_back (i.titol);
            r.text += fets.empty() ? "El que sí que sé és que la teva mescla, comparada amb una mescla de pop/rock d'avui (orientativa), no té grans desviacions de to."
                                   : "El que sí que sé de la teva mescla, comparada amb una mescla de pop/rock d'avui (orientativa): " + llista (fets) + ".";
            r.text += "\n\nPer comparar-te de debò amb una cançó concreta, caldrà carregar-la com a referència (és a la fase 5 del Mix Assistant).";
            suggeriments (r, ctx, t, {});
            return r;
        }

        if (t == Seguiment)
        {
            if (ctx.seguiment.empty()) { r.text = "Encara no puc comparar: torna a analitzar després dels canvis i t'ho diré."; r.suggeriments = { "Torna a analitzar" }; return r; }
            int n[5] {};
            std::vector<std::string> sol, mill, pitj, nous;
            for (const auto& e : ctx.seguiment)
            {
                ++n[(int) e.estat];
                if (e.estat == Evolucio::Solucionat) sol.push_back (e.titol);
                else if (e.estat == Evolucio::Millorat) mill.push_back (e.titol);
                else if (e.estat == Evolucio::Empitjorat) pitj.push_back (e.titol);
                else if (e.estat == Evolucio::Nou) nous.push_back (e.titol);
            }
            r.text = "Des de l'anàlisi anterior: " + std::to_string (n[0]) + " solucionades, " + std::to_string (n[1]) + " han millorat, "
                   + std::to_string (n[2]) + " continuen, " + std::to_string (n[3]) + " han empitjorat i " + std::to_string (n[4]) + " són noves.";
            auto bloc = [&] (const char* t0, std::vector<std::string> v) { if (v.size() > 4) v.resize (4); if (! v.empty()) r.text += "\n\n" + std::string (t0) + llista (v) + "."; };
            bloc ("Solucionat: ", sol);
            bloc ("Ha millorat: ", mill);
            bloc ("Compte, ha empitjorat: ", pitj);
            bloc ("Nou: ", nous);
            r.text += "\n\nRecorda que això són mesures: escolta-ho, que és el que mana.";
            suggeriments (r, ctx, t, {});
            return r;
        }

        if (t == Solucio || t == Alternatives)
        {
            std::vector<const Incidencia*> v;
            for (const auto& id : conv.incidencies)
                for (const auto& i : ctx.incidencies) if (i.id == id) v.push_back (&i);
            if (v.empty() && ! ids.empty()) v = cerca (ctx, ids, Resum);
            if (v.empty())
            {
                r.text = "De què ho vols arreglar? Digues-me la pista o el que no t'agrada (per exemple: «com arreglo la veu?» o «com faig que la mescla soni menys tancada?»).";
                r.entesa = false;
                r.suggeriments = preguntesInicials();
                r.suggeriments.resize (3);
                return r;
            }
            if (v.size() > 3) v.resize (3);
            r.text = t == Solucio ? "Per arreglar-ho, començaria així:" : "Hi ha més d'una manera de fer-ho:";
            conv.incidencies.clear();
            for (const auto* i : v)
            {
                r.text += "\n\n" + i->titol + ". " + (t == Solucio ? "La meva proposta: " + i->solucio : "La primera opció: " + i->solucio);
                if (! i->altres.empty())
                    for (const auto& a : i->altres) r.text += "\n· " + a;
                else if (t == Alternatives)
                    r.text += "\n(Per a aquesta no en tinc d'altres: la més directa és aquesta. Escolta-la abans d'aplicar-la.)";
                conv.incidencies.push_back (i->id);
                const std::string idPista = ! i->pista.empty() ? i->pista : (! i->pistes.empty() ? i->pistes.front() : std::string());
                if (! idPista.empty())
                {
                    const auto* pp = pista (ctx, idPista);
                    r.enllacos.push_back ({ espotPreparar (*i) ? "Prepara-la a " + (pp != nullptr ? cm (pp->nom) : std::string ("la pista"))
                                                               : "Vés a " + (pp != nullptr ? cm (pp->nom) : std::string ("la pista")), idPista, i->id });
                }
            }
            r.text += "\n\nLes que es poden preparar, les pots escoltar abans (A/B) i aplicar només si t'agraden.";
            conv.tema = t;
            suggeriments (r, ctx, t, ids);
            return r;
        }

        if (t == Cap)
        {
            r.entesa = false;
            r.text = "No t'he entès del tot. Puc respondre sobre les pistes (la veu, el baix, les guitarres…) i sobre com sona la mescla "
                     "(tancada, embolicada, dura, greus, volums, estèreo, dinàmica, esses). Prova alguna d'aquestes:";
            r.suggeriments = preguntesInicials();
            r.suggeriments.resize (4);
            return r;
        }

        // ---- un tema (d'unes pistes o de tota la mescla) ----
        conv.tema = t;
        conv.pistes = ids;
        const auto v = cerca (ctx, ids, t);
        const auto noms = nomsDe (ctx, ids);
        if (t == Resum && ids.empty())
        {
            if (v.empty()) r.text = "Ara mateix no hi veig res important a la mescla. Si alguna cosa no t'agrada, digues-me què i ho mirem.";
            else
            {
                r.text = "Si hagués de començar per algun lloc, ho faria per aquí (de més a menys important):";
                explica (r, conv, v, ctx, 3);
            }
            suggeriments (r, ctx, t, {});
            return r;
        }
        if (! ids.empty())
        {
            r.text = "He mirat " + llista (noms) + (t == Resum ? "." : " (" + std::string (nomTema (t)) + ").");
            if (v.empty())
            {
                r.text += " No hi veig cap problema d'això.";
                for (const auto& id : ids)
                    if (const auto* p = pista (ctx, id)) if (const auto x = xifres (ctx, *p); ! x.empty()) r.text += " " + x;
                if (t == Destacar) r.text += "\n\nSi tot i així no la sents prou, pot ser l'arranjament (algun instrument tocant al mateix registre) o simplement el volum: prova de pujar-la 1–2 dB i escolta.";
                else r.text += "\n\nSi tu hi sents alguna cosa, explica-me-la amb altres paraules i ho mirem.";
            }
            else
            {
                if (t == Destacar)
                    for (const auto& id : ids)
                        if (const auto* p = pista (ctx, id)) if (const auto x = xifres (ctx, *p); ! x.empty()) r.text += " " + x;
                explica (r, conv, v, ctx, 3);
            }
        }
        else
        {
            r.text = "He mirat la mescla sencera (" + std::string (nomTema (t)) + "):";
            if (v.empty()) r.text += " no hi veig cap problema clar. Si ho sents en una pista concreta, digues-me quina.";
            else explica (r, conv, v, ctx, 3);
        }
        suggeriments (r, ctx, t, ids);
        return r;
    }
}
