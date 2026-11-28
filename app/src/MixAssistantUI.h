#pragma once
// 1.23: el MIX ASSISTANT (fase 1). La finestra pròpia (F7, es pot deixar a una segona pantalla; l'Esc no la tanca) i la
// pestanya «Mix» del panell de la dreta. Mostra el diagnòstic de cada pista; encara no canvia res de la mescla.
#include "Controls.h"
#include "EstatEstudi.h"
#include "mibase/MixAssistant.h"
#include "mibase/MixCoach.h"
#include <set>

/** El que sap el Mix Assistant ara (no es desa al projecte: es torna a analitzar; sí que es desa l'instrument triat). */
struct ModelMix : public juce::ChangeBroadcaster
{
    struct Fila
    {
        std::string id;
        juce::String nom;
        juce::Colour color;
        mibase::mix::Identificacio ident;
        mibase::mix::DiagnosiPista diag;
        std::shared_ptr<mibase::mix::AnalisiPista> analisi;
        bool analitzada = false;
        juce::String motiu;   // per què no s'ha analitzat («silenciada», «buida»…)
    };
    std::vector<Fila> files;
    // 1.24: la mescla sencera (conflictes, equilibri, to de la mescla) i el Balanç proposat
    std::vector<mibase::mix::Incidencia> globals;
    std::vector<mibase::mix::CanviBalanc> balanc;
    bool escoltantBalanc = false;   // el Balanç proposat sona ara (B)
    bool creacio = false;           // mode Creació: només el que val la pena arreglar a la gravació
    // 1.25: correccions (incidència → pista i efecte que s'hi ha posat) i seguiment entre anàlisis
    struct Correccio { mibase::mix::LlocCorreccio lloc; bool aplicada = false; int analisi = -1; std::vector<int> bandesExtra; };   // 1.28: bandes afegides en reforçar-la   // 1.26: banda de l'EQ únic (o efecte); 1.28: anàlisi en què s'ha aplicat
    int numAnalisi = 0;   // 1.28: quantes anàlisis s'han acabat (per saber si un problema continua després d'aplicar-ne la correcció)
    std::map<std::string, Correccio> correccions;
    inline static const std::string idHistorial { "__historial__" };   // 1.26 (amb «:<pista>», el d'una pista)
    // 1.27: el Mix Coach (la conversa de la sessió)
    struct Missatge { bool meu = false; juce::String text; std::vector<mibase::mix::coach::Enllac> enllacos; std::vector<std::string> suggeriments; };
    std::vector<Missatge> conversa;
    mibase::mix::coach::Conversa estatCoach;
    std::vector<mibase::mix::Evolucio> seguiment;
    /** Totes les incidències d'ara (per comparar-les amb les de la propera anàlisi). */
    std::vector<mibase::mix::Incidencia> totesCopia() const
    {
        std::vector<mibase::mix::Incidencia> v;
        for (const auto& f : files) for (const auto& i : f.diag.incidencies) v.push_back (i);
        for (const auto& i : globals) v.push_back (i);
        return v;
    }
    inline static const std::string idMescla { "__mescla__" };
    bool analitzant = false, fet = false;
    juce::String progres, quan;
    double fraccio = 0.0;

    const Fila* fila (const std::string& id) const
    {
        for (const auto& f : files) if (f.id == id) return &f;
        return nullptr;
    }
    bool visible (const mibase::mix::Incidencia& i) const { return ! creacio || i.origen; }
    int compta (mibase::mix::Prioritat p) const
    {
        int n = 0;
        for (const auto& [f, i] : totes()) n += i->prioritat == p;
        return n;
    }
    /** Les d'una pista: les seves i les de la mescla on surt (conflictes, equilibri…). */
    std::vector<const mibase::mix::Incidencia*> dePista (const std::string& id) const
    {
        std::vector<const mibase::mix::Incidencia*> v;
        if (const auto* f = fila (id)) for (const auto& i : f->diag.incidencies) if (visible (i)) v.push_back (&i);
        for (const auto& i : globals)
            if (visible (i) && (i.pista == id || std::find (i.pistes.begin(), i.pistes.end(), id) != i.pistes.end())) v.push_back (&i);
        std::stable_sort (v.begin(), v.end(), [] (auto* a, auto* b) { return a->gravetat > b->gravetat; });
        return v;
    }
    std::vector<const mibase::mix::Incidencia*> deMescla() const
    {
        std::vector<const mibase::mix::Incidencia*> v;
        for (const auto& i : globals) if (visible (i)) v.push_back (&i);
        return v;
    }
    int perPreguntar() const
    {
        int n = 0;
        for (const auto& f : files) n += f.analitzada && f.ident.calPreguntar();
        return n;
    }
    int analitzades() const
    {
        int n = 0;
        for (const auto& f : files) n += f.analitzada;
        return n;
    }
    /** Totes les incidències, de la més greu a la menys. */
    /** Totes les incidències, de la més greu a la menys (la fila és nul·la si és de la mescla sencera). */
    std::vector<std::pair<const Fila*, const mibase::mix::Incidencia*>> totes() const
    {
        std::vector<std::pair<const Fila*, const mibase::mix::Incidencia*>> v;
        for (const auto& f : files) for (const auto& i : f.diag.incidencies) if (visible (i)) v.push_back ({ &f, &i });
        for (const auto& i : globals) if (visible (i)) v.push_back ({ i.pista.empty() ? nullptr : fila (i.pista), &i });   // 1.24
        std::stable_sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.second->gravetat > b.second->gravetat; });
        return v;
    }
    void avisa() { sendSynchronousChangeMessage(); }
};

namespace mixui
{
    namespace mx = mibase::mix;

    inline juce::String u (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
    inline juce::Colour colorPrioritat (mx::Prioritat p)
    {
        return p == mx::Prioritat::Alta ? tema::error : p == mx::Prioritat::Mitjana ? tema::accent : tema::suau;
    }
    inline juce::String nomConfianca (mx::Confianca c)
    {
        return c == mx::Confianca::Alta ? "alta"_ca : c == mx::Confianca::Mitjana ? "mitjana"_ca : "baixa (escolta-ho abans)"_ca;
    }

    /** Text amb salts de línia: alçada i dibuix. */
    inline juce::TextLayout disposa (const juce::String& t, const juce::Font& f, juce::Colour c, float w)
    {
        juce::AttributedString a;
        a.setWordWrap (juce::AttributedString::byWord);
        a.append (t, f, c);
        juce::TextLayout l;
        l.createLayout (a, juce::jmax (10.0f, w));
        return l;
    }
    /** Escriu el text a partir de y i torna la y de sota (g pot ser nul: només mesura). */
    inline int escriu (juce::Graphics* g, const juce::String& t, const juce::Font& f, juce::Colour c, int x, int y, int w, int separacio = 6)
    {
        if (t.isEmpty()) return y;
        auto l = disposa (t, f, c, (float) w);
        if (g != nullptr) l.draw (*g, juce::Rectangle<float> ((float) x, (float) y, (float) w, l.getHeight()));
        return y + (int) std::ceil (l.getHeight()) + separacio;
    }
    /** Un botó dibuixat (els de dins de les llistes llargues). */
    inline void pintaBoto (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, bool primari, bool sobre)
    {
        auto rf = r.toFloat().reduced (0.5f);
        g.setColour (primari ? (sobre ? tema::accent.brighter (0.15f) : tema::accent) : (sobre ? tema::sobre.brighter (0.1f) : tema::superficie2));
        g.fillRoundedRectangle (rf, 6.0f);
        if (! primari) { g.setColour (tema::linia2); g.drawRoundedRectangle (rf, 6.0f, 1.0f); }
        g.setColour (primari ? juce::Colours::black : tema::text);
        g.setFont (dibuix::lletra (12.5f, primari));
        g.drawText (t, r, juce::Justification::centred);
    }
    inline int ampleBoto (const juce::String& t)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (dibuix::lletra (12.5f, true), t, 0, 0);
        return (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth()) + 24;
    }
    /** Els tres comptadors (importants, a revisar, detalls). */
    inline void pintaComptadors (juce::Graphics& g, juce::Rectangle<int> r, int alta, int mitjana, int baixa, bool petit = false)
    {
        const juce::String t[] = { juce::String (alta) + (alta == 1 ? " important"_ca : " importants"_ca),
                                   juce::String (mitjana) + " a revisar"_ca, juce::String (baixa) + (baixa == 1 ? " detall"_ca : " detalls"_ca) };
        const juce::Colour c[] = { tema::error, tema::accent, tema::suau };
        const int n[] = { alta, mitjana, baixa };
        int x = r.getX();
        for (int k = 0; k < 3; ++k)
        {
            const int w = ampleBoto (t[k]) + (petit ? 0 : 8);
            if (x + w > r.getRight()) break;
            auto rr = juce::Rectangle<int> (x, r.getY(), w, r.getHeight()).toFloat().reduced (0.5f);
            g.setColour (n[k] > 0 ? c[k].withAlpha (0.16f) : tema::superficie);
            g.fillRoundedRectangle (rr, rr.getHeight() / 2.0f);
            g.setColour (n[k] > 0 ? c[k] : tema::linia2);
            g.drawRoundedRectangle (rr, rr.getHeight() / 2.0f, 1.0f);
            g.fillEllipse (rr.getX() + 9.0f, rr.getCentreY() - 3.5f, 7.0f, 7.0f);
            g.setColour (n[k] > 0 ? tema::text : tema::suau);
            g.setFont (dibuix::lletra (petit ? 11.5f : 12.5f, n[k] > 0));
            g.drawText (t[k], rr.toNearestInt().withTrimmedLeft (20), juce::Justification::centredLeft);
            x += w + 6;
        }
    }
}

/** La part de la dreta de la finestra: el diagnòstic d'una pista, de la mescla sencera o de tot, per ordre d'importància. */
class LlistaMix : public juce::Component
{
public:
    explicit LlistaMix (ModelMix& m) : model (m) { setRepaintsOnMouseActivity (true); }

    std::function<void (const std::string&)> onTriaInstrument;                          // obre el menú d'instruments
    std::function<void (const std::string&, mibase::mix::Instrument)> onConfirma;      // «Sí, és això»
    std::function<void (const std::string&)> onVesAPista, onMostraPista;
    std::function<void()> onEscoltaBalanc, onAcceptaBalanc, onDescartaBalanc;          // 1.24
    /** 1.25: «prepara», «escolta», «obre», «aplica», «rebutja» d'una incidència. */
    std::function<void (const std::string& accio, const std::string& incidencia)> onCorreccio;
    /** 1.25: com està la correcció d'una incidència: 0 cap, 1 preparada (apagada, A), 2 preparada i sonant (B), 3 aplicada;
        1.28: 4 aplicada però, en tornar a analitzar, encara hi és. */
    std::function<int (const std::string& incidencia)> estatCorreccio;
    /** 1.26: l'historial del Mix Assistant (del projecte), el més nou a dalt. */
    std::function<std::vector<mibase::Projecte::EntradaMix>()> historial;

    void mostra (const std::string& idPista) { pista = idPista; refes(); }
    const std::string& pistaMostrada() const noexcept { return pista; }

    void refes()
    {
        const int alt = mesura (getWidth());
        setSize (getWidth(), juce::jmax (alt, getParentHeight()));
        repaint();
    }

    void paint (juce::Graphics& g) override { dibuixa (&g, getWidth()); }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto ara = zonaA (e.getPosition());
        if (ara != sobre) { sobre = ara; repaint(); }
        setMouseCursor (ara >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { if (sobre >= 0) { sobre = -1; repaint(); } }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int z = zonaA (e.getPosition());
        if (z < 0) return;
        auto zona = zones[(size_t) z];   // còpia: les accions poden refer la llista
        switch (zona.accio)
        {
            case Zona::Detalls:
                if (oberts.count (zona.clau) > 0) oberts.erase (zona.clau); else oberts.insert (zona.clau);
                refes();
                break;
            case Zona::Instrument: if (onTriaInstrument) onTriaInstrument (zona.clau); break;
            case Zona::Confirma: if (onConfirma) onConfirma (zona.clau, zona.inst); break;
            case Zona::VesAPista: if (onVesAPista) onVesAPista (zona.clau); break;
            case Zona::MostraPista: if (onMostraPista) onMostraPista (zona.clau); break;
            case Zona::EscoltaBalanc: if (onEscoltaBalanc) onEscoltaBalanc(); break;
            case Zona::AcceptaBalanc: if (onAcceptaBalanc) onAcceptaBalanc(); break;
            case Zona::DescartaBalanc: if (onDescartaBalanc) onDescartaBalanc(); break;
            case Zona::Correccio: if (onCorreccio) onCorreccio (zona.accio2, zona.clau); break;
        }
    }

private:
    struct Zona
    {
        enum Accio { Detalls, Instrument, Confirma, VesAPista, MostraPista, EscoltaBalanc, AcceptaBalanc, DescartaBalanc, Correccio } accio;
        juce::Rectangle<int> r;
        std::string clau;
        mibase::mix::Instrument inst = mibase::mix::Instrument::Desconegut;
        std::string accio2;   // 1.25: quina acció de la correcció
    };
    ModelMix& model;
    std::string pista;                // buit: totes; ModelMix::idMescla: la mescla sencera
    std::set<std::string> oberts;     // incidències (i «mesures:<pista>») amb els detalls oberts
    std::vector<Zona> zones;
    int sobre = -1;

    int zonaA (juce::Point<int> p) const
    {
        for (size_t i = 0; i < zones.size(); ++i) if (zones[i].r.contains (p)) return (int) i;
        return -1;
    }
    int mesura (int w) { return dibuixa (nullptr, w); }

    juce::Rectangle<int> boto (juce::Graphics* g, int x, int y, const juce::String& t, bool primari, Zona z)
    {
        juce::Rectangle<int> r (x, y, mixui::ampleBoto (t), 28);
        z.r = r;
        if (g != nullptr) mixui::pintaBoto (*g, r, t, primari, sobre == (int) zones.size());
        zones.push_back (z);
        return r;
    }

    /** Dibuixa (o només mesura, amb g nul) i torna l'alçada. */
    int dibuixa (juce::Graphics* g, int w)
    {
        namespace mx = mibase::mix;
        using mixui::u;
        zones.clear();
        const int m = 16, W = juce::jmax (200, w - 2 * m);
        int y = 14;
        if (! model.fet)
        {
            y = mixui::escriu (g, model.analitzant ? "Analitzant… Cada pista es renderitza per separat, sense reproduir-la i sense tocar el so original. Al final, la mescla sencera."_ca
                                                   : "Encara no s'ha analitzat. Prem «Analitza la mescla»: Mibase escolta cada pista per separat (sense reproduir-la) i la mescla sencera, i et diu què hi veu, per què pot ser un problema i què proposa.\n\nNo canvia res de la mescla si no ho acceptes tu."_ca,
                               dibuix::lletra (14.0f), tema::textMig, m, y, W);
            return y + 20;
        }
        if (model.creacio)
            y = mixui::escriu (g, "Mode Creació: només et dic el que val la pena arreglar mentre encara graves (saturació, gravacions fluixes, fase, soroll, ressonàncies molt marcades)."_ca,
                               dibuix::lletra (12.5f), tema::accent, m, y, W, 12);
        if (pista.empty())
        {
            y = mixui::escriu (g, "Totes les pistes"_ca, dibuix::lletra (18.0f, true), tema::text, m, y, W, 4);
            y = mixui::escriu (g, "De la més important a la menys. Clica el nom d'una pista per veure-la sola."_ca, dibuix::lletra (12.5f), tema::suau, m, y, W, 12);
            if (const int n = model.perPreguntar(); n > 0)
            {
                juce::String noms;
                for (const auto& f : model.files) if (f.analitzada && f.ident.calPreguntar()) noms << (noms.isEmpty() ? "" : ", ") << f.nom;
                y = caixaPregunta (g, m, y, W, (n == 1 ? "Hi ha una pista que no sé quin instrument és: "_ca : juce::String (n) + " pistes no sé quin instrument són: "_ca)
                                   + noms + ". Si m'ho dius, el diagnòstic serà més precís."_ca, {});
            }
            if (! model.seguiment.empty()) y = caixaSeguiment (g, m, y, W);   // 1.25
            if (! model.creacio && ! model.balanc.empty()) y = caixaBalanc (g, m, y, W);   // 1.24.1: també aquí
            const auto v = model.totes();
            if (v.empty()) y = mixui::escriu (g, "No hi veig res important."_ca, dibuix::lletra (14.0f), tema::textMig, m, y, W);
            for (const auto& [f, i] : v) y = targeta (g, f, *i, m, y, W, true);
            return y + 16;
        }
        if (pista.rfind (ModelMix::idHistorial, 0) == 0)   // 1.26
        {
            const std::string filtre = pista.size() > ModelMix::idHistorial.size() + 1 ? pista.substr (ModelMix::idHistorial.size() + 1) : std::string();
            const auto* fp = filtre.empty() ? nullptr : model.fila (filtre);
            y = mixui::escriu (g, fp != nullptr ? "Historial de «"_ca + fp->nom + "»" : "Historial del Mix Assistant"_ca, dibuix::lletra (18.0f, true), tema::text, m, y, W, 4);
            y = mixui::escriu (g, "Què s'ha preparat, aplicat o tret, i les versions de mescla. És per consultar: si ho canvies a mà a l'efecte, aquí continua el que es va fer."_ca,
                               dibuix::lletra (12.5f), tema::suau, m, y, W, 12);
            if (fp != nullptr)
            {
                Zona z { Zona::MostraPista, {}, ModelMix::idHistorial };
                boto (g, m, y, "Tot l'historial"_ca, false, z);
                y += 40;
            }
            const auto h = historial ? historial() : std::vector<mibase::Projecte::EntradaMix>();
            int n = 0;
            for (auto it = h.rbegin(); it != h.rend(); ++it)
            {
                if (! filtre.empty() && it->pista != filtre) continue;
                ++n;
                const auto* fr = model.fila (it->pista);
                juce::String linia = u (it->quan);
                if (fr != nullptr) linia << "  ·  " << fr->nom;
                if (g != nullptr && fr != nullptr) { g->setColour (fr->color); g->fillRoundedRectangle ((float) m, (float) y + 3.0f, 4.0f, 14.0f, 2.0f); }
                y = mixui::escriu (g, linia, dibuix::lletra (12.0f, true), tema::textMig, m + 10, y, W - 10, 2);
                y = mixui::escriu (g, u (it->text), dibuix::lletra (13.0f), tema::text, m + 10, y, W - 10, 10);
            }
            if (n == 0) y = mixui::escriu (g, "Encara no hi ha res."_ca, dibuix::lletra (13.5f), tema::textMig, m, y, W);
            return y + 16;
        }
        if (pista == ModelMix::idMescla)
        {
            y = mixui::escriu (g, "La mescla i l'equilibri"_ca, dibuix::lletra (18.0f, true), tema::text, m, y, W, 4);
            y = mixui::escriu (g, "Com sonen les pistes juntes: qui tapa qui, l'equilibri de volums i panorames, i el to de la mescla sencera (sense el màster)."_ca,
                               dibuix::lletra (12.5f), tema::suau, m, y, W, 12);
            if (! model.creacio && ! model.balanc.empty()) y = caixaBalanc (g, m, y, W);
            else if (! model.creacio)
            {
                // 1.24.1: dir per què no hi ha Balanç proposat
                int segures = 0;
                juce::String dubtoses;
                for (const auto& f : model.files)
                {
                    if (! f.analitzada) continue;
                    if (f.ident.instrument != mibase::mix::Instrument::Desconegut && f.ident.confianca != mibase::mix::Confianca::Baixa) ++segures;
                    else dubtoses << (dubtoses.isEmpty() ? "" : ", ") << f.nom;
                }
                const juce::String t = segures < 2
                    ? "Balanç proposat: ara no en puc fer cap. Per proposar volums i panorames he de saber quins instruments són (veu, baix, bombo, guitarres…), i "_ca
                      + (dubtoses.isEmpty() ? "no n'hi ha prou."_ca : "no sé segur què són: "_ca + dubtoses + ". Tria-ho a cada pista (els que tenen «?») i es recalcula sol."_ca)
                    : "Balanç proposat: ara no hi ha res a proposar. Els volums i panorames de les pistes que conec estan dins del que és habitual."_ca;
                y = mixui::escriu (g, t, dibuix::lletra (13.0f), tema::textMig, m, y, W, 14);
            }
            const auto v = model.deMescla();
            if (v.empty()) y = mixui::escriu (g, model.creacio ? "En mode Creació no es mira la mescla sencera."_ca : "No hi veig res important a la mescla."_ca,
                                              dibuix::lletra (14.0f), tema::textMig, m, y, W);
            for (const auto* i : v) y = targeta (g, i->pista.empty() ? nullptr : model.fila (i->pista), *i, m, y, W, true);
            return y + 16;
        }
        const auto* f = model.fila (pista);
        if (f == nullptr) return mixui::escriu (nullptr, " ", dibuix::lletra (14.0f), tema::text, m, y, W);
        // capçalera de la pista
        if (g != nullptr) { g->setColour (f->color); g->fillRoundedRectangle ((float) m, (float) y + 4.0f, 6.0f, 22.0f, 3.0f); }
        y = mixui::escriu (g, f->nom, dibuix::lletra (20.0f, true), tema::text, m + 14, y, W - 14, 4);
        if (! f->analitzada)
        {
            y = mixui::escriu (g, f->motiu, dibuix::lletra (13.5f), tema::textMig, m, y + 6, W);
            return y + 16;
        }
        const auto& id = f->ident;
        {
            juce::String t = "Instrument: "_ca + u (mx::nomInstrument (id.instrument)) + "  ·  "_ca + u (id.perque);
            y = mixui::escriu (g, t, dibuix::lletra (13.0f), tema::textMig, m, y, W - 200, 8);
            Zona z { Zona::Instrument, {}, f->id };
            boto (g, m, y, "Canvia l'instrument"_ca, false, z);
            Zona v { Zona::VesAPista, {}, f->id };
            const int xv = m + mixui::ampleBoto ("Canvia l'instrument"_ca) + 8;
            boto (g, xv, y, "Vés a la pista"_ca, false, v);
            Zona hh { Zona::MostraPista, {}, ModelMix::idHistorial + ":" + f->id };   // 1.26
            boto (g, xv + mixui::ampleBoto ("Vés a la pista"_ca) + 8, y, "Historial de la pista"_ca, false, hh);
            y += 40;
        }
        if (id.calPreguntar())
        {
            const bool sembla = id.instrument != mx::Instrument::Desconegut;
            y = caixaPregunta (g, m, y, W, sembla ? "No n'estic segur: em sembla que és «"_ca + u (mx::nomInstrument (id.instrument)) + "». És correcte?"_ca
                                                  : "No sé quin instrument és. Digues-m'ho i el diagnòstic serà més precís."_ca,
                               sembla ? f->id : std::string(), id.instrument);
        }
        const auto v = model.dePista (f->id);
        juce::String resum;
        if (model.creacio) resum = v.empty() ? "Per a la gravació, res a dir."_ca : juce::String ((int) v.size()) + (v.size() == 1 ? " cosa de la gravació."_ca : " coses de la gravació."_ca);
        else
        {
            // 1.24: també compten els conflictes i l'equilibri on surt la pista
            int alta = 0, mitjana = 0;
            for (const auto* i : v) { alta += i->prioritat == mx::Prioritat::Alta; mitjana += i->prioritat == mx::Prioritat::Mitjana; }
            if (v.empty()) resum = "No hi veig res important."_ca;
            else if (alta > 0) resum = juce::String (alta) + (alta == 1 ? " cosa important"_ca : " coses importants"_ca) + (mitjana > 0 ? " i "_ca + juce::String (mitjana) + " a revisar."_ca : juce::String ("."));
            else if (mitjana > 0) resum = juce::String (mitjana) + (mitjana == 1 ? " cosa a revisar."_ca : " coses a revisar."_ca);
            else resum = "Només detalls petits."_ca;
            if (id.instrument == mx::Instrument::Desconegut || id.confianca == mx::Confianca::Baixa) resum += " (Sense saber segur quin instrument és, només miro les coses generals.)"_ca;
        }
        y = mixui::escriu (g, resum, dibuix::lletra (14.0f, true), tema::text, m, y + 2, W, 12);
        for (const auto* i : v) y = targeta (g, f, *i, m, y, W, false);
        // les xifres
        {
            const std::string clau = "mesures:" + f->id;
            const bool obert = oberts.count (clau) > 0;
            Zona z { Zona::Detalls, {}, clau };
            boto (g, m, y + 4, obert ? "Amaga les mesures"_ca : "Mostra les mesures"_ca, false, z);
            y += 40;
            if (obert) y = mixui::escriu (g, u (f->diag.mesures), dibuix::mono (12.0f), tema::textMig, m, y, W, 10);
        }
        return y + 16;
    }

    /** 1.25: què ha passat des de l'anàlisi anterior. */
    int caixaSeguiment (juce::Graphics* g, int x, int y, int w)
    {
        namespace mx = mibase::mix;
        using mixui::u;
        const int pad = 14;
        int n[5] {};
        for (const auto& e : model.seguiment) ++n[(int) e.estat];
        juce::String resum = "Des de l'anàlisi anterior: "_ca;
        juce::StringArray parts;
        const char* noms[] = { "solucionades", "han millorat", "continuen", "han empitjorat", "noves" };
        for (int k = 0; k < 5; ++k) if (n[k] > 0) parts.add (juce::String (n[k]) + " " + juce::String::fromUTF8 (noms[k]));
        resum << parts.joinIntoString (", ") << ".";
        juce::String llista;
        int k = 0;
        for (const auto& e : model.seguiment)
        {
            if (e.estat == mx::Evolucio::Continua) continue;
            if (++k > 8) break;
            const auto* f = model.fila (e.pista);
            llista << "· "_ca << u (mx::nomEvolucio (e.estat)) << ": " << u (e.titol) << (f != nullptr ? " (" + f->nom + ")" : juce::String()) << "\n";
        }
        const juce::String nota = "Les mesures no ho són tot: escolta-ho i decideix tu."_ca;
        const int h = pad + mixui::escriu (nullptr, resum, dibuix::lletra (14.0f, true), tema::text, 0, 0, w - 2 * pad, 6)
                    + mixui::escriu (nullptr, llista.trimEnd(), dibuix::lletra (12.5f), tema::textMig, 0, 0, w - 2 * pad, 6)
                    + mixui::escriu (nullptr, nota, dibuix::lletra (12.0f), tema::suau, 0, 0, w - 2 * pad, 0) + pad;
        if (g != nullptr)
        {
            auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            g->setColour (tema::sinte.withAlpha (0.08f));
            g->fillRoundedRectangle (r, 8.0f);
            g->setColour (tema::sinte.withAlpha (0.5f));
            g->drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        }
        int yy = mixui::escriu (g, resum, dibuix::lletra (14.0f, true), tema::text, x + pad, y + pad, w - 2 * pad, 6);
        yy = mixui::escriu (g, llista.trimEnd(), dibuix::lletra (12.5f), tema::textMig, x + pad, yy, w - 2 * pad, 6);
        mixui::escriu (g, nota, dibuix::lletra (12.0f), tema::suau, x + pad, yy, w - 2 * pad, 0);
        return y + h + 14;
    }

    /** 1.24: el Balanç proposat, amb l'A/B. */
    int caixaBalanc (juce::Graphics* g, int x, int y, int w)
    {
        using mixui::u;
        const int pad = 14;
        juce::String text;
        for (const auto& c : model.balanc)
        {
            text << "· "_ca << u (c.nom) << ": ";
            juce::StringArray parts;
            if (c.tocaFader()) parts.add ("fader " + juce::String (c.faderAbans, 1).replaceCharacter ('.', ',') + " → "
                                          + juce::String (c.fader, 1).replaceCharacter ('.', ',') + " dB");
            if (c.tocaPan())
            {
                auto tp = [] (double p) { return std::abs (p) < 0.01 ? "al mig"_ca : juce::String (juce::roundToInt (std::abs (p) * 100.0)) + " % " + (p < 0 ? "E" : "D"); };
                parts.add ("panorama " + tp (c.panAbans) + " → " + tp (c.pan));
            }
            text << parts.joinIntoString (" · ") << "\n";
        }
        const juce::String titol = model.escoltantBalanc ? "Balanç proposat: SONA ARA (B)"_ca : "Balanç proposat"_ca;
        const juce::String expl = model.escoltantBalanc ? "Estàs escoltant la proposta. Torna al teu balanç per comparar-los (A/B), accepta-la o descarta-la."_ca
                                                         : "Escolta-ho abans de decidir: «Escolta'l» canvia els faders i panorames mentre sona, i «Torna al teu» els deixa com eren. No es queda res fins que l'acceptis."_ca;
        const int hT = mixui::escriu (nullptr, titol, dibuix::lletra (15.0f, true), tema::text, 0, 0, w - 2 * pad, 4);
        const int hE = mixui::escriu (nullptr, expl, dibuix::lletra (12.5f), tema::suau, 0, 0, w - 2 * pad, 8);
        const int hL = mixui::escriu (nullptr, text.trimEnd(), dibuix::mono (12.5f), tema::text, 0, 0, w - 2 * pad, 10);
        const int h = pad + hT + hE + hL + 28 + pad;
        if (g != nullptr)
        {
            auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            g->setColour (model.escoltantBalanc ? tema::accent.withAlpha (0.14f) : tema::superficie);
            g->fillRoundedRectangle (r, 8.0f);
            g->setColour (model.escoltantBalanc ? tema::accent : tema::linia2);
            g->drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        }
        int yy = mixui::escriu (g, titol, dibuix::lletra (15.0f, true), model.escoltantBalanc ? tema::accent : tema::text, x + pad, y + pad, w - 2 * pad, 4);
        yy = mixui::escriu (g, expl, dibuix::lletra (12.5f), tema::suau, x + pad, yy, w - 2 * pad, 8);
        yy = mixui::escriu (g, text.trimEnd(), dibuix::mono (12.5f), tema::text, x + pad, yy, w - 2 * pad, 10);
        int bx = x + pad;
        bx = boto (g, bx, yy, model.escoltantBalanc ? "Torna al teu (A)"_ca : "Escolta'l (B)"_ca, ! model.escoltantBalanc, { Zona::EscoltaBalanc, {}, {} }).getRight() + 8;
        bx = boto (g, bx, yy, "Accepta'l"_ca, model.escoltantBalanc, { Zona::AcceptaBalanc, {}, {} }).getRight() + 8;
        boto (g, bx, yy, "Descarta'l"_ca, false, { Zona::DescartaBalanc, {}, {} });
        return y + h + 14;
    }

    int caixaPregunta (juce::Graphics* g, int x, int y, int w, const juce::String& text, const std::string& idPista,
                       mibase::mix::Instrument inst = mibase::mix::Instrument::Desconegut)
    {
        const int pad = 12;
        const int h0 = mixui::escriu (nullptr, text, dibuix::lletra (13.5f), tema::text, 0, 0, w - 2 * pad, 0);
        const bool ambBotons = ! pista.empty() && pista != ModelMix::idMescla;
        const int h = pad + h0 + (ambBotons ? 10 + 28 : 0) + pad;
        if (g != nullptr)
        {
            auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            g->setColour (tema::accent.withAlpha (0.10f));
            g->fillRoundedRectangle (r, 8.0f);
            g->setColour (tema::accent.withAlpha (0.6f));
            g->drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        }
        mixui::escriu (g, text, dibuix::lletra (13.5f), tema::text, x + pad, y + pad, w - 2 * pad, 0);
        if (ambBotons)
        {
            int bx = x + pad;
            const int by = y + pad + h0 + 10;
            if (! idPista.empty())
            {
                Zona c { Zona::Confirma, {}, idPista, inst };
                bx = boto (g, bx, by, "Sí, és això"_ca, true, c).getRight() + 8;
            }
            Zona t { Zona::Instrument, {}, pista };
            boto (g, bx, by, "Tria l'instrument"_ca, false, t);
        }
        return y + h + 14;
    }

    /** Les pistes d'una incidència com a etiquetes clicables (o «La mescla»). Torna l'amplada feta. */
    int etiquetesPistes (juce::Graphics* gg, const ModelMix::Fila* f, const mibase::mix::Incidencia& i, int tx, int yy)
    {
        std::vector<std::string> ids = i.pistes;
        if (ids.empty() && f != nullptr) ids.push_back (f->id);
        int x = tx;
        if (ids.empty())
        {
            if (gg != nullptr)
            {
                gg->setColour (tema::accent);
                gg->setFont (dibuix::lletra (12.5f, true));
                gg->drawText ("La mescla"_ca, x, yy, 120, 16, juce::Justification::centredLeft);
            }
            return 80;
        }
        for (const auto& id : ids)
        {
            const auto* fp = model.fila (id);
            if (fp == nullptr) continue;
            const int wp = mixui::ampleBoto (fp->nom) - 18;
            if (gg != nullptr)
            {
                gg->setColour (fp->color);
                gg->fillEllipse ((float) x, (float) yy + 4.0f, 8.0f, 8.0f);
                gg->setColour (sobre == (int) zones.size() ? tema::accent : tema::textMig);
                gg->setFont (dibuix::lletra (12.5f, true));
                gg->drawText (fp->nom, x + 13, yy, wp + 4, 16, juce::Justification::centredLeft);
            }
            zones.push_back ({ Zona::MostraPista, { x, yy - 2, wp + 20, 20 }, id });
            x += wp + 24;
        }
        return x - tx;
    }

    int targeta (juce::Graphics* g, const ModelMix::Fila* f, const mibase::mix::Incidencia& i, int x, int y, int w, bool ambPista)
    {
        using mixui::u;
        const int pad = 14, iw = w - 2 * pad - 6;
        const bool obert = oberts.count (i.id) > 0;
        const auto col = mixui::colorPrioritat (i.prioritat);
        const int y0 = y;
        // primer es mesura, després es pinta el fons i el text
        auto contingut = [&] (juce::Graphics* gg) -> int
        {
            int yy = y0 + pad;
            const int tx = x + pad + 6;
            const juce::String et = u (mibase::mix::nomPrioritat (i.prioritat));
            const int we = mixui::ampleBoto (et);
            if (gg != nullptr)
            {
                auto re = juce::Rectangle<float> ((float) (x + w - pad - we), (float) yy, (float) we, 20.0f);
                gg->setColour (col.withAlpha (0.18f));
                gg->fillRoundedRectangle (re, 10.0f);
                gg->setColour (col);
                gg->setFont (dibuix::lletra (11.5f, true));
                gg->drawText (et, re.toNearestInt(), juce::Justification::centred);
            }
            yy = mixui::escriu (gg, u (i.titol), dibuix::lletra (15.0f, true), tema::text, tx, yy, iw - we - 10, 3);
            const juce::String on = u (i.on);
            if (ambPista || ! i.pistes.empty())
            {
                const int wp = etiquetesPistes (gg, f, i, tx, yy);
                yy = mixui::escriu (gg, on, dibuix::lletra (12.0f), tema::suau, tx + wp + 6, yy, juce::jmax (80, iw - wp - 6), 6);
            }
            else yy = mixui::escriu (gg, on, dibuix::lletra (12.0f), tema::suau, tx, yy, iw, 6);
            yy = mixui::escriu (gg, u (i.queVeig), dibuix::lletra (13.5f), tema::textMig, tx, yy, iw, 4);
            yy = mixui::escriu (gg, u (i.perque), dibuix::lletra (13.0f), tema::suau, tx, yy, iw, 8);
            yy = mixui::escriu (gg, "Què proposo: "_ca + u (i.solucio), dibuix::lletra (13.5f), tema::text, tx, yy, iw, 8);
            if (! i.altres.empty())
            {
                yy = mixui::escriu (gg, "Altres opcions:"_ca, dibuix::lletra (12.5f, true), tema::textMig, tx, yy, iw, 3);
                for (const auto& a : i.altres) yy = mixui::escriu (gg, "· "_ca + u (a), dibuix::lletra (12.5f), tema::suau, tx + 8, yy, iw - 8, 3);
                yy += 5;
            }
            // 1.25: preparar, escoltar, ajustar, aplicar o rebutjar la correcció
            if (! model.creacio && mibase::mix::espotPreparar (i))
            {
                const int ec = estatCorreccio ? estatCorreccio (i.id) : 0;
                auto bt = [&] (int bx, const juce::String& t, bool primari, const char* accio)
                {
                    Zona z { Zona::Correccio, {}, i.id };
                    z.accio2 = accio;
                    return boto (gg, bx, yy, t, primari, z).getRight() + 8;
                };
                int bx = tx;
                if (ec == 0) bx = bt (bx, "Prepara-la"_ca, true, "prepara");
                else if (ec == 3)
                {
                    if (gg != nullptr)
                    {
                        gg->setColour (tema::sinte);
                        gg->setFont (dibuix::lletra (12.5f, true));
                        gg->drawText ("Aplicada"_ca, bx, yy, 80, 28, juce::Justification::centredLeft);
                    }
                    bx += 80;
                    bx = bt (bx, "Ajusta-la"_ca, false, "obre");
                    bx = bt (bx, "Treu-la"_ca, false, "rebutja");
                }
                else if (ec == 4)   // 1.28: aplicada, però en tornar a analitzar encara hi és
                {
                    bx = bt (bx, "Reforça-la"_ca, true, "reforca");
                    bx = bt (bx, "Ajusta-la"_ca, false, "obre");
                    bx = bt (bx, "Treu-la"_ca, false, "rebutja");
                }
                else
                {
                    bx = bt (bx, ec == 2 ? "Torna a l'original (A)"_ca : "Escolta-la (B)"_ca, ec != 2, "escolta");
                    bx = bt (bx, "Ajusta-la"_ca, false, "obre");
                    bx = bt (bx, "Aplica-la"_ca, ec == 2, "aplica");
                    bx = bt (bx, "Rebutja-la"_ca, false, "rebutja");
                }
                yy += 32;
                if (ec == 4)
                    yy = mixui::escriu (gg, "Ja hi ha una correcció aplicada, però encara ho detecto (la mesura d'ara ja la compta). «Reforça-la» hi afegeix el que encara falta."_ca,
                                        dibuix::lletra (12.0f), tema::accent, tx, yy, iw, 4);
                if (ec == 1 || ec == 2)
                    yy = mixui::escriu (gg, ec == 2 ? "Sona la correcció (B). Compara-la amb l'original (A) i decideix."_ca
                                                    : "Preparada: és a l'efecte del Mix Assistant de la pista (al final dels efectes), encara apagada. No canvia res fins que l'apliquis."_ca,
                                        dibuix::lletra (12.0f), ec == 2 ? tema::accent : tema::suau, tx, yy, iw, 4);
            }
            const bool deBalanc = (i.proposta.tipus == mibase::mix::Proposta::Fader || i.proposta.tipus == mibase::mix::Proposta::Panorama)
                                  && ! model.balanc.empty() && ! model.creacio;
            if (gg != nullptr)
            {
                gg->setColour (tema::suau);
                gg->setFont (dibuix::lletra (11.5f));
                gg->drawText ("Confiança: "_ca + mixui::nomConfianca (i.confianca), tx, yy + 6, iw - 140 - (deBalanc ? 230 : 0), 16, juce::Justification::centredLeft);
            }
            if (deBalanc)   // 1.24.1: escoltar el Balanç proposat des de la mateixa targeta
            {
                const juce::String tb = model.escoltantBalanc ? "Torna al teu balanç (A)"_ca : "Escolta el Balanç proposat (B)"_ca;
                const int wDet = i.detalls.empty() ? 0 : mixui::ampleBoto (obert ? "Amaga els detalls"_ca : "Detalls"_ca) + 8;
                boto (gg, x + w - pad - wDet - mixui::ampleBoto (tb), yy, tb, ! model.escoltantBalanc, { Zona::EscoltaBalanc, {}, {} });
            }
            if (! i.detalls.empty())
            {
                const juce::String tb = obert ? "Amaga els detalls"_ca : "Detalls"_ca;
                Zona z { Zona::Detalls, {}, i.id };
                boto (gg, x + w - pad - mixui::ampleBoto (tb), yy, tb, false, z);
            }
            yy += 34;
            if (obert && ! i.detalls.empty())
            {
                const int hd = mixui::escriu (nullptr, u (i.detalls), dibuix::mono (12.0f), tema::text, 0, 0, iw - 20, 0);
                if (gg != nullptr)
                {
                    gg->setColour (tema::fons);
                    gg->fillRoundedRectangle ((float) tx, (float) yy, (float) iw, (float) hd + 16.0f, 6.0f);
                }
                mixui::escriu (gg, u (i.detalls), dibuix::mono (12.0f), tema::text, tx + 10, yy + 8, iw - 20, 0);
                yy += hd + 24;
            }
            return yy + pad - 6;
        };
        const size_t nz = zones.size();
        const int fi = contingut (nullptr);
        zones.resize (nz);   // es tornen a posar en pintar
        if (g != nullptr)
        {
            auto r = juce::Rectangle<float> ((float) x, (float) y0, (float) w, (float) (fi - y0));
            g->setColour (tema::superficie);
            g->fillRoundedRectangle (r, 8.0f);
            g->setColour (tema::linia);
            g->drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
            g->setColour (col);
            g->fillRoundedRectangle (r.withWidth (5.0f).reduced (0.0f, 6.0f).translated (4.0f, 0.0f), 2.5f);
        }
        contingut (g);
        return fi + 10;
    }
};

/** 1.27: el Mix Coach: la conversa (bombolles, enllaços a les pistes i preguntes per continuar). */
class LlistaXat : public juce::Component
{
public:
    explicit LlistaXat (ModelMix& m) : model (m) { setRepaintsOnMouseActivity (true); }
    std::function<void (const juce::String&)> onPregunta;
    std::function<void (const mibase::mix::coach::Enllac&)> onEnllac;

    void refes() { setSize (getWidth(), juce::jmax (dibuixa (nullptr, getWidth()), getParentHeight())); repaint(); }
    void paint (juce::Graphics& g) override { dibuixa (&g, getWidth()); }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int z = zonaA (e.getPosition());
        if (z != sobre) { sobre = z; repaint(); }
        setMouseCursor (z >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { if (sobre >= 0) { sobre = -1; repaint(); } }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int z = zonaA (e.getPosition());
        if (z < 0) return;
        const auto zona = zones[(size_t) z];
        if (zona.enllac.pista.empty()) { if (onPregunta) onPregunta (zona.text); }
        else if (onEnllac) onEnllac (zona.enllac);
    }

private:
    struct Zona { juce::Rectangle<int> r; juce::String text; mibase::mix::coach::Enllac enllac; };
    ModelMix& model;
    std::vector<Zona> zones;
    int sobre = -1;
    int zonaA (juce::Point<int> p) const { for (size_t i = 0; i < zones.size(); ++i) if (zones[i].r.contains (p)) return (int) i; return -1; }

    int xip (juce::Graphics* g, int x, int y, int maxX, const juce::String& t, bool enllac)
    {
        const int w = juce::jmin (mixui::ampleBoto (t) + 4, maxX - x);
        juce::Rectangle<int> r (x, y, w, 28);
        if (g != nullptr)
        {
            const bool s = sobre == (int) zones.size();
            auto rf = r.toFloat().reduced (0.5f);
            g->setColour (enllac ? (s ? tema::accent.brighter (0.15f) : tema::accent.withAlpha (0.85f)) : (s ? tema::sobre.brighter (0.1f) : tema::superficie2));
            g->fillRoundedRectangle (rf, 14.0f);
            if (! enllac) { g->setColour (tema::linia2); g->drawRoundedRectangle (rf, 14.0f, 1.0f); }
            g->setColour (enllac ? juce::Colours::black : tema::text);
            g->setFont (dibuix::lletra (12.5f, enllac));
            g->drawFittedText (t, r.reduced (10, 0), juce::Justification::centred, 1, 0.8f);
        }
        return w;
    }

    int dibuixa (juce::Graphics* g, int w)
    {
        zones.clear();
        const int m = 16, W = juce::jmax (200, w - 2 * m), bmax = (int) (W * 0.82f);
        int y = 14;
        auto missatges = model.conversa;
        if (missatges.empty())
        {
            ModelMix::Missatge benvinguda;
            benvinguda.text = model.fet ? "Hola! Soc el Mix Coach. Pregunta'm el que vulguis de la mescla: respondré amb el que he mesurat a l'anàlisi, i si no ho sé, t'ho diré."_ca
                                        : "Hola! Soc el Mix Coach. Primer he d'analitzar la mescla (botó «Analitza la mescla», a dalt); després pregunta'm el que vulguis i respondré amb el que hagi mesurat."_ca;
            benvinguda.suggeriments = mibase::mix::coach::preguntesInicials();
            missatges.push_back (benvinguda);
        }
        for (size_t k = 0; k < missatges.size(); ++k)
        {
            const auto& mm = missatges[k];
            const auto l = mixui::disposa (mm.text, dibuix::lletra (13.5f), tema::text, (float) bmax - 24.0f);
            const int hb = (int) std::ceil (l.getHeight()) + 20;
            const int wb = juce::jmin (bmax, (int) std::ceil (l.getWidth()) + 26);
            const int x = mm.meu ? m + W - wb : m;
            if (g != nullptr)
            {
                auto r = juce::Rectangle<float> ((float) x, (float) y, (float) wb, (float) hb);
                g->setColour (mm.meu ? tema::accent.withAlpha (0.16f) : tema::superficie);
                g->fillRoundedRectangle (r, 10.0f);
                g->setColour (mm.meu ? tema::accent.withAlpha (0.5f) : tema::linia);
                g->drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
                l.draw (*g, r.reduced (12.0f, 10.0f));
            }
            y += hb + 6;
            // enllaços i preguntes per continuar: només a la darrera resposta
            const bool darrer = k + 1 == missatges.size();
            if (! mm.meu && darrer)
            {
                int xx = m;
                for (const auto& e : mm.enllacos)
                {
                    const juce::String t = mixui::u (e.text);
                    const int wx = mixui::ampleBoto (t) + 4;
                    if (xx + wx > m + W && xx > m) { xx = m; y += 34; }
                    zones.push_back ({ { xx, y, wx, 28 }, t, e });
                    xip (g, xx, y, m + W, t, true);
                    xx += wx + 6;
                }
                if (! mm.enllacos.empty()) { y += 36; xx = m; }
                for (const auto& sg : mm.suggeriments)
                {
                    const juce::String t = mixui::u (sg);
                    const int wx = mixui::ampleBoto (t) + 4;
                    if (xx + wx > m + W && xx > m) { xx = m; y += 34; }
                    zones.push_back ({ { xx, y, wx, 28 }, t, {} });
                    xip (g, xx, y, m + W, t, false);
                    xx += wx + 6;
                }
                if (! mm.suggeriments.empty()) y += 36;
            }
            y += 6;
        }
        return y + 10;
    }
};

class VistaCoach : public juce::Component
{
public:
    explicit VistaCoach (ModelMix& m) : model (m), xat (m)
    {
        vista.setViewedComponent (&xat, false);
        vista.setScrollBarsShown (true, false);
        vista.setScrollBarThickness (10);
        addAndMakeVisible (vista);
        entrada.setTextToShowWhenEmpty ("Pregunta al Mix Coach (per exemple: per què la veu no sobresurt?)"_ca, tema::suau);
        entrada.setFont (dibuix::lletra (14.0f));
        entrada.setIndents (10, 8);
        entrada.setColour (juce::TextEditor::backgroundColourId, tema::fons);
        entrada.setColour (juce::TextEditor::outlineColourId, tema::linia2);
        entrada.setColour (juce::TextEditor::focusedOutlineColourId, tema::accent);
        entrada.setColour (juce::TextEditor::textColourId, tema::text);
        entrada.onReturnKey = [this] { envia(); };
        addAndMakeVisible (entrada);
        btEnvia.onClick = [this] { envia(); };
        addAndMakeVisible (btEnvia);
        xat.onPregunta = [this] (const juce::String& t) { if (onPregunta) onPregunta (t); };
        xat.onEnllac = [this] (const mibase::mix::coach::Enllac& e) { if (onEnllac) onEnllac (e); };
    }
    std::function<void (const juce::String&)> onPregunta;
    std::function<void (const mibase::mix::coach::Enllac&)> onEnllac;

    void actualitza()
    {
        xat.setSize (vista.getWidth() - vista.getScrollBarThickness(), xat.getHeight());
        xat.refes();
        vista.setViewPosition (0, juce::jmax (0, xat.getHeight() - vista.getHeight()));   // a baix de tot (el més nou)
    }
    void resized() override
    {
        auto r = getLocalBounds();
        auto baix = r.removeFromBottom (58).reduced (14, 10);
        btEnvia.setBounds (baix.removeFromRight (btEnvia.ampladaIdeal()));
        baix.removeFromRight (8);
        entrada.setBounds (baix);
        vista.setBounds (r);
        actualitza();
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (tema::linia);
        g.fillRect (0, getHeight() - 58, getWidth(), 1);
    }
    void focus() { entrada.grabKeyboardFocus(); }

private:
    ModelMix& model;
    juce::Viewport vista;
    LlistaXat xat;
    juce::TextEditor entrada;
    Boto btEnvia { "Pregunta"_ca, std::nullopt, Boto::Estil::Primari };

    void envia()
    {
        const auto t = entrada.getText().trim();
        if (t.isEmpty()) return;
        entrada.clear();
        if (onPregunta) onPregunta (t);
    }
};

/** La finestra del Mix Assistant (el contingut). */
class VistaMixAssistant : public juce::Component, private juce::ChangeListener, private juce::ListBoxModel
{
public:
    explicit VistaMixAssistant (ModelMix& m) : model (m), detall (m)
    {
        setOpaque (true);
        addAndMakeVisible (btAnalitza);
        btAnalitza.setTooltip ("Escolta totes les pistes (renderitzades, sense reproduir-les) i en fa el diagnòstic. No canvia res de la mescla."_ca);
        btAnalitza.onClick = [this] { if (onAnalitza) onAnalitza(); };
        addAndMakeVisible (btVersions);   // 1.25
        btVersions.setTooltip ("Versions de mescla: desa com està ara la mescla (volums, panorames, efectes, enviaments) i torna-hi quan vulguis"_ca);
        btVersions.onClick = [this] { if (onVersions) onVersions (&btVersions); };
        addAndMakeVisible (segMode);   // 1.24
        segMode.setTooltip ("Creació: només el que val la pena arreglar mentre graves. Mescla: tot (conflictes, equilibri, to…)"_ca);
        segMode.onCanvi = [this] (int i) { if (onMode) onMode (i == 0); };
        llista.setModel (this);
        llista.setRowHeight (48);
        llista.setColour (juce::ListBox::backgroundColourId, tema::lateral);
        llista.setOutlineThickness (0);
        addAndMakeVisible (llista);
        vista.setViewedComponent (&detall, false);
        vista.setScrollBarsShown (true, false);
        vista.setScrollBarThickness (10);
        addAndMakeVisible (vista);
        detall.onMostraPista = [this] (const std::string& id) { triaPista (id); };
        addChildComponent (coach);   // 1.27
        model.addChangeListener (this);
        actualitza();
    }
    ~VistaMixAssistant() override { model.removeChangeListener (this); }

    std::function<void()> onAnalitza;
    std::function<void (bool creacio)> onMode;   // 1.24
    std::function<void (juce::Component*)> onVersions;   // 1.25
    inline static const std::string idCoach { "__coach__" };   // 1.27
    LlistaMix& llistaDetall() noexcept { return detall; }

    /** Mostra una pista (buit: totes). */
    void triaPista (const std::string& id)
    {
        int fila = id == ModelMix::idMescla ? 2 : id.rfind (ModelMix::idHistorial, 0) == 0 ? 3 : id == idCoach ? 0 : 1;
        for (size_t i = 0; i < model.files.size(); ++i) if (model.files[i].id == id) fila = (int) i + primeraPista;
        llista.selectRow (fila, false, true);
        if (fila != 0) detall.mostra (fila == 1 ? std::string() : fila == 2 ? ModelMix::idMescla : id);
        vista.setViewPosition (0, 0);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::fons);
        auto r = getLocalBounds();
        auto cap = r.removeFromTop (alcadaCap);
        g.setColour (tema::superficie);
        g.fillRect (cap);
        g.setColour (tema::linia);
        g.fillRect (cap.removeFromBottom (1));
        auto t = cap.reduced (18, 0).withTrimmedRight (btAnalitza.getWidth() + segMode.getWidth() + btVersions.getWidth() + 54);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (20.0f, true));
        g.drawText ("Mix Assistant", t.getX(), 12, t.getWidth(), 26, juce::Justification::centredLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (12.5f));
        juce::String sub;
        if (model.analitzant) sub = model.progres;
        else if (model.fet) sub = juce::String (model.analitzades()) + " pistes analitzades · "_ca + model.quan + " · Si canvies la mescla, torna a analitzar."_ca;
        else sub = "Analitza, diagnostica, explica i proposa. No canvia res de la mescla si no ho acceptes tu."_ca;
        g.drawText (sub, t.getX(), 38, t.getWidth(), 18, juce::Justification::centredLeft);
        if (model.analitzant)
        {
            auto b = juce::Rectangle<float> ((float) t.getX(), 62.0f, (float) juce::jmin (420, t.getWidth()), 6.0f);
            g.setColour (tema::superficie2);
            g.fillRoundedRectangle (b, 3.0f);
            g.setColour (tema::accent);
            g.fillRoundedRectangle (b.withWidth (b.getWidth() * (float) juce::jlimit (0.0, 1.0, model.fraccio)), 3.0f);
        }
        else if (model.fet)
            mixui::pintaComptadors (g, { t.getX(), 60, t.getWidth(), 24 }, model.compta (mibase::mix::Prioritat::Alta),
                                    model.compta (mibase::mix::Prioritat::Mitjana), model.compta (mibase::mix::Prioritat::Baixa));
        g.setColour (tema::linia);
        g.fillRect (llista.getRight(), alcadaCap, 1, getHeight() - alcadaCap);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto cap = r.removeFromTop (alcadaCap);
        btAnalitza.setBounds (cap.removeFromRight (btAnalitza.ampladaIdeal() + 18).withTrimmedRight (18).withSizeKeepingCentre (btAnalitza.ampladaIdeal(), 34));
        segMode.setBounds (cap.removeFromRight (segMode.ampladaIdeal() + 18).withTrimmedRight (18).withSizeKeepingCentre (segMode.ampladaIdeal(), 30));
        btVersions.setBounds (cap.removeFromRight (btVersions.ampladaIdeal() + 18).withTrimmedRight (18).withSizeKeepingCentre (btVersions.ampladaIdeal(), 32));
        llista.setBounds (r.removeFromLeft (juce::jlimit (200, 290, getWidth() / 4)));
        r.removeFromLeft (1);
        vista.setBounds (r);
        coach.setBounds (r);
        detall.setSize (r.getWidth() - vista.getScrollBarThickness(), detall.getHeight());
        detall.refes();
    }

    void actualitza()
    {
        btAnalitza.setEnabled (! model.analitzant);
        btAnalitza.setButtonText (model.analitzant ? "Analitzant…"_ca : model.fet ? "Torna a analitzar"_ca : "Analitza la mescla"_ca);
        segMode.setTriat (model.creacio ? 0 : 1);
        llista.updateContent();
        if (llista.getSelectedRow() < 0) llista.selectRow (1, false, true);
        detall.refes();
        coach.actualitza();
        resized();
        repaint();
    }

private:
    static constexpr int alcadaCap = 92, primeraPista = 4;   // 0 Mix Coach (1.27), 1 totes, 2 la mescla, 3 l'historial (1.26)
    ModelMix& model;
    Boto btAnalitza { "Analitza la mescla"_ca, std::nullopt, Boto::Estil::Primari };
    Segmentat segMode { { "Creació"_ca, "Mescla" } };
    Boto btVersions { "Versions de mescla"_ca };
    juce::ListBox llista;
    juce::Viewport vista;
    LlistaMix detall;
public:
    VistaCoach coach { model };   // 1.27
private:

    void changeListenerCallback (juce::ChangeBroadcaster*) override { actualitza(); }

    int getNumRows() override { return primeraPista + (int) model.files.size(); }
    void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool triada) override
    {
        namespace mx = mibase::mix;
        if (triada) { g.setColour (tema::triat); g.fillRect (0, 0, w, h); g.setColour (tema::accent); g.fillRect (0, 0, 3, h); }
        g.setColour (tema::linia.withAlpha (0.5f));
        g.fillRect (12, h - 1, w - 24, 1);
        if (fila < primeraPista)
        {
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (14.0f, true));
            g.drawText (fila == 0 ? "Mix Coach"_ca : fila == 1 ? "Totes les pistes"_ca : fila == 2 ? "La mescla i l'equilibri"_ca : "Historial"_ca, 16, 6, w - 30, 20, juce::Justification::centredLeft);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.5f));
            juce::String sota = fila == 0 ? "Pregunta-li el que vulguis"_ca : fila == 3 ? "El que ha fet el Mix Assistant"_ca : ! model.fet ? "Encara sense analitzar"_ca
                              : fila == 1 ? "Per ordre d'importància"_ca : "Conflictes, volums, panorames, to"_ca;
            if (fila == 0) g.setColour (tema::accent);
            if (fila == 2 && model.fet && ! model.balanc.empty() && ! model.creacio) { g.setColour (tema::accent); sota = "Hi ha un Balanç proposat"_ca; }
            g.drawText (sota, 16, 26, w - 30, 16, juce::Justification::centredLeft);
            if (fila == 3 || fila == 0) { g.setColour (tema::linia); g.fillRect (12, h - 2, w - 24, fila == 0 ? 1 : 2); }
            return;
        }
        if (fila - primeraPista >= (int) model.files.size()) return;
        const auto& f = model.files[(size_t) (fila - primeraPista)];
        g.setColour (f.color);
        g.fillRoundedRectangle (14.0f, 10.0f, 5.0f, (float) h - 20.0f, 2.5f);
        int ra = 0, rm = 0, rb = 0;
        const auto inc = model.dePista (f.id);
        for (const auto* i : inc) (i->prioritat == mx::Prioritat::Alta ? ra : i->prioritat == mx::Prioritat::Mitjana ? rm : rb)++;
        const int dots = f.analitzada ? 46 : 0;
        g.setColour (f.analitzada ? tema::text : tema::suau);
        g.setFont (dibuix::lletra (13.5f, true));
        g.drawText (f.nom, 28, 6, w - 36 - dots, 20, juce::Justification::centredLeft, true);
        g.setFont (dibuix::lletra (11.5f));
        juce::String sota = f.analitzada ? mixui::u (mx::nomInstrument (f.ident.instrument)) : f.motiu;
        if (f.analitzada && f.ident.calPreguntar()) { g.setColour (tema::accent); sota += "  ?"; }
        else g.setColour (tema::suau);
        g.drawText (sota, 28, 26, w - 36 - dots, 16, juce::Justification::centredLeft, true);
        if (! f.analitzada) return;
        float x = (float) w - 14.0f;
        for (auto [n, c] : { std::pair<int, juce::Colour> { rb, tema::suau }, { rm, tema::accent }, { ra, tema::error } })
        {
            if (n == 0) continue;
            g.setColour (c);
            g.setFont (dibuix::lletra (11.5f, true));
            g.drawText (juce::String (n), (int) x - 12, 16, 12, 16, juce::Justification::centredRight);
            g.fillEllipse (x - 21.0f, 21.0f, 7.0f, 7.0f);
            x -= 26.0f;
        }
        if (inc.empty())
        {
            g.setColour (tema::sinte);
            g.setFont (dibuix::lletra (11.5f, true));
            g.drawText ("Bé"_ca, w - 50, 16, 36, 16, juce::Justification::centredRight);
        }
    }
    void selectedRowsChanged (int fila) override
    {
        if (fila < 0) return;
        coach.setVisible (fila == 0);   // 1.27
        vista.setVisible (fila != 0);
        if (fila == 0) { coach.actualitza(); coach.focus(); return; }
        detall.mostra (fila == 2 ? ModelMix::idMescla : fila == 3 ? ModelMix::idHistorial
                       : fila == 1 || fila - primeraPista >= (int) model.files.size() ? std::string() : model.files[(size_t) (fila - primeraPista)].id);
        vista.setViewPosition (0, 0);
    }
};

/** La pestanya «Mix» del panell de la dreta: el resum i les incidències més importants. */
class PanellMix : public juce::Component, private juce::ChangeListener, private juce::ListBoxModel
{
public:
    explicit PanellMix (ModelMix& m) : model (m)
    {
        for (auto* b : { &btObre, &btAnalitza }) addAndMakeVisible (b);
        btObre.setTooltip ("Obre la finestra del Mix Assistant (F7)"_ca);
        btAnalitza.setTooltip ("Escolta totes les pistes i en fa el diagnòstic (no canvia res de la mescla)"_ca);
        btObre.onClick = [this] { if (onObre) onObre ({}); };
        btAnalitza.onClick = [this] { if (onAnalitza) onAnalitza(); };
        llista.setModel (this);
        llista.setRowHeight (46);
        llista.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        llista.setOutlineThickness (0);
        addAndMakeVisible (llista);
        model.addChangeListener (this);
    }
    ~PanellMix() override { model.removeChangeListener (this); }

    std::function<void (const std::string&)> onObre;   // la pista on ha d'anar (buit: totes)
    std::function<void()> onAnalitza;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (12, 0);
        r.removeFromTop (zonaBotons);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (12.0f));
        juce::String t;
        if (model.analitzant) t = model.progres;
        else if (! model.fet) t = "Encara no s'ha analitzat la mescla."_ca;
        else t = juce::String (model.analitzades()) + " pistes · "_ca + model.quan;
        g.drawText (t, r.removeFromTop (20), juce::Justification::centredLeft, true);
        if (model.fet && ! model.analitzant)
        {
            auto c = r.removeFromTop (28).withTrimmedTop (4);
            mixui::pintaComptadors (g, c, model.compta (mibase::mix::Prioritat::Alta), model.compta (mibase::mix::Prioritat::Mitjana),
                                    model.compta (mibase::mix::Prioritat::Baixa), true);
            if (const int n = model.perPreguntar(); n > 0)
            {
                g.setColour (tema::accent);
                g.setFont (dibuix::lletra (12.0f));
                g.drawText (juce::String (n) + (n == 1 ? " pista per identificar"_ca : " pistes per identificar"_ca), r.removeFromTop (22), juce::Justification::centredLeft);
            }
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 0);
        auto b = r.removeFromTop (zonaBotons).withTrimmedTop (8).withHeight (32);
        btObre.setBounds (b.removeFromLeft (b.getWidth() * 3 / 5));
        b.removeFromLeft (6);
        btAnalitza.setBounds (b);
        int alt = 20 + (model.fet && ! model.analitzant ? 28 + (model.perPreguntar() > 0 ? 22 : 0) : 0) + 8;
        llista.setBounds (getLocalBounds().withTrimmedTop (zonaBotons + alt));
    }

private:
    static constexpr int zonaBotons = 48;
    ModelMix& model;
    Boto btObre { "Obre el Mix Assistant"_ca, std::nullopt, Boto::Estil::Primari };
    Boto btAnalitza { "Analitza"_ca };
    juce::ListBox llista;
    std::vector<std::pair<std::string, const mibase::mix::Incidencia*>> files;

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        files.clear();
        for (const auto& [f, i] : model.totes())
        {
            if (files.size() >= 40) break;
            files.push_back ({ f != nullptr ? f->id : (i->deMescla || i->pistes.empty() ? ModelMix::idMescla : i->pistes.front()), i });
        }
        btAnalitza.setEnabled (! model.analitzant);
        llista.updateContent();
        resized();
        repaint();
    }
    int getNumRows() override { return (int) files.size(); }
    void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool) override
    {
        if (fila < 0 || fila >= (int) files.size()) return;
        const auto* i = files[(size_t) fila].second;
        const auto* f = model.fila (files[(size_t) fila].first);
        g.setColour (mixui::colorPrioritat (i->prioritat));
        g.fillRoundedRectangle (12.0f, 9.0f, 4.0f, (float) h - 18.0f, 2.0f);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (12.5f, true));
        g.drawText (mixui::u (i->titol), 24, 5, w - 32, 18, juce::Justification::centredLeft, true);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawText (f != nullptr ? f->nom : "La mescla"_ca, 24, 23, w - 32, 16, juce::Justification::centredLeft, true);
        g.setColour (tema::linia.withAlpha (0.5f));
        g.fillRect (12, h - 1, w - 24, 1);
    }
    void listBoxItemClicked (int fila, const juce::MouseEvent&) override
    {
        if (fila >= 0 && fila < (int) files.size() && onObre) onObre (files[(size_t) fila].first);
    }
};
