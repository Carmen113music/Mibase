#pragma once
// 0.40: editor de rasgueigs. A cada pas es tria el cop i com es toca; el resultat es pot desar com a rasgueig propi.
// S'inclou després de PianoRoll.h (fa servir CaixaNumero).
#include "Controls.h"
#include "Kits.h"
#include <mibase/Guitarra.h>

namespace rasgueigs
{
    /** Documents\Mibase\Rasgueigs.json: els rasgueigs que ha fet l'usuari. */
    inline juce::File fitxer() { return kits::carpeta().getParentDirectory().getChildFile ("Rasgueigs.json"); }
    inline void carrega()
    {
        const auto f = fitxer();
        if (f.existsAsFile()) mibase::guitarra::posaPropis (mibase::guitarra::llegeixPropis (f.loadFileAsString().toStdString()));
    }
    inline bool desa()
    {
        return fitxer().replaceWithText (juce::String::fromUTF8 (mibase::guitarra::textPropis (mibase::guitarra::propis()).c_str()));
    }
}

/** 0.53: demana el nom en desar (a sobre de l'editor): Retorn = desa, Esc = cancel·la. */
class PanellNomRasgueig : public juce::Component
{
public:
    std::function<void (const juce::String&)> onDesa;
    std::function<void()> onCancella;
    PanellNomRasgueig (const juce::String& titolPanell, const juce::String& nomInicial) : titol (titolPanell)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &text, &btDesa, &btCancella }) addAndMakeVisible (c);
        text.setFont (dibuix::lletra (14.0f));
        text.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.45f));
        text.setColour (juce::TextEditor::textColourId, tema::text);
        text.setColour (juce::TextEditor::outlineColourId, tema::linia2);
        text.setColour (juce::TextEditor::focusedOutlineColourId, tema::accent);
        text.setText (nomInicial, false);
        text.onReturnKey = [this] { accepta(); };
        text.onEscapeKey = [this] { if (onCancella) onCancella(); };
        btDesa.onClick = [this] { accepta(); };
        btCancella.onClick = [this] { if (onCancella) onCancella(); };
    }
    void comenca() { text.grabKeyboardFocus(); text.selectAll(); }
    void resized() override
    {
        caixa = getLocalBounds().withSizeKeepingCentre (juce::jmin (420, getWidth() - 20), 134);
        auto z = caixa.reduced (14);
        z.removeFromTop (22);
        text.setBounds (z.removeFromTop (30));
        z.removeFromTop (8);
        auto b = z.removeFromTop (30);
        btDesa.setBounds (b.removeFromRight (90)); b.removeFromRight (6);
        btCancella.setBounds (b.removeFromRight (100));
        avisZona = b;
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));
        g.setColour (tema::superficie);
        g.fillRoundedRectangle (caixa.toFloat(), 8.0f);
        g.setColour (tema::accent);
        g.drawRoundedRectangle (caixa.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (13.0f, true));
        g.drawFittedText (titol, caixa.reduced (14, 10).removeFromTop (18), juce::Justification::centredLeft, 1, 0.8f);
        if (avis.isNotEmpty()) { g.setColour (tema::guitarra); g.setFont (dibuix::lletra (11.5f)); g.drawFittedText (avis, avisZona, juce::Justification::centredLeft, 2, 0.8f); }
    }
private:
    void accepta()
    {
        const auto t = text.getText().trim();
        if (t.isEmpty()) { avis = juce::String::fromUTF8 ("Posa-li un nom."); repaint(); return; }
        if (onDesa) onDesa (t);
    }
    juce::String titol, avis;
    juce::TextEditor text;
    Boto btDesa { "Desa", std::nullopt, Boto::Estil::Primari };
    Boto btCancella { juce::String::fromUTF8 ("Cancel·la"), std::nullopt, Boto::Estil::Secundari };
    juce::Rectangle<int> caixa, avisZona;
};

class EditorRasgueig : public juce::Component, private juce::Timer
{
public:
    using Pas = mibase::guitarra::Pas;
    using Cop = mibase::guitarra::Cop;
    static constexpr int maxPassos = 32, ampleNoms = 86, ampleCasella = 24, altFila = 26, numFiles = 9;

    std::function<void (const mibase::guitarra::Patro&)> onEscolta, onDesa;
    std::function<void (const std::string& id)> onEsborra;
    std::function<void()> onAtura;      // 0.49.1: talla l'escolta
    double msPerTick = 0.52;            // 0.49.1: per saber quant dura una volta del bucle (el posa qui obre l'editor)
    ~EditorRasgueig() override { stopTimer(); if (onAtura) onAtura(); }
    mibase::guitarra::Forma forma = mibase::guitarra::Forma::Oberts;   // 0.49: la forma d'acord amb què es desa

    explicit EditorRasgueig (const mibase::guitarra::Patro* inicial)
    {
        const bool propi = inicial != nullptr && inicial->id.rfind ("propi:", 0) == 0;
        idOriginal = propi ? inicial->id : std::string();
        variacioDe = propi ? inicial->de : std::string();   // 0.53
        if (inicial != nullptr && ! inicial->passos.empty())
        {
            passos = inicial->passos;
            if ((int) passos.size() > maxPassos) passos.resize ((size_t) maxPassos);
            compas = inicial->compas;
            selPas.setTriat (inicial->pasTicks == 240 ? 1 : 0);
            nom.setText (juce::String::fromUTF8 (inicial->nom.c_str()) + (propi ? juce::String() : juce::String::fromUTF8 (" (meu)")), false);
        }
        else
        {
            passos = mibase::guitarra::llegeixPassos ("D . D U . U D U");
            nom.setText ("El meu rasgueig", false);
        }
        numPassos.setValor ((int) passos.size());
        for (auto* c : std::initializer_list<juce::Component*> { &nom, &selPas, &numPassos, &btEscolta, &btBucle, &btAtura, &btDesa, &btEsborra }) addAndMakeVisible (c);
        nom.setFont (dibuix::lletra (14.0f));
        nom.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.4f));
        nom.setColour (juce::TextEditor::textColourId, tema::text);
        nom.setColour (juce::TextEditor::outlineColourId, tema::linia2);
        selPas.setLlum (vintage::ledTaronja);
        selPas.setTooltip (juce::String::fromUTF8 ("Quant dura cada pas"));
        pasAnterior = selPas.getTriat();
        selPas.onCanvi = [this] (int quin)
        {
            // 0.41: els mateixos compassos amb el doble (o la meitat) de caselles
            if (quin == pasAnterior) return;
            std::vector<Pas> nou;
            if (quin == 1)
                for (auto& p : passos) { nou.push_back (p); nou.push_back (Pas()); }
            else
            {
                bool perdut = false;
                for (size_t i = 0; i < passos.size(); ++i)
                    if (i % 2 == 0) nou.push_back (passos[i]);
                    else perdut = perdut || passos[i].cop != Cop::Res;
                avis = perdut ? juce::String::fromUTF8 ("En passar a corxeres s'han perdut els cops que queien entremig.") : juce::String();
            }
            if ((int) nou.size() > maxPassos) nou.resize ((size_t) maxPassos);
            if (nou.size() < 2) nou.resize (2);
            passos = std::move (nou);
            pasAnterior = quin;
            numPassos.setValor ((int) passos.size());
            setSize (ampleTotal(), getHeight());
            canviat();
            repaint();
        };
        numPassos.onCanvi = [this] (int n)
        {
            passos.resize ((size_t) juce::jlimit (2, maxPassos, n));
            setSize (ampleTotal(), getHeight());
            canviat();
            repaint();
        };
        // 0.49.1: minicontrol: una vegada, en bucle, o parar.
        btEscolta.setTooltip (juce::String::fromUTF8 ("Escolta'l una vegada"));
        btBucle.setTooltip (juce::String::fromUTF8 ("Escolta'l en bucle: el que canviïs se sent a la volta següent"));
        btAtura.setTooltip (juce::String::fromUTF8 ("Para l'escolta"));
        btBucle.setLlum (tema::accent);
        btEscolta.onClick = [this] { posaBucle (false); sona(); };
        btBucle.onClick = [this] { posaBucle (! bucle); if (! bucle && onAtura) onAtura(); };
        btAtura.onClick = [this] { posaBucle (false); if (onAtura) onAtura(); };
        btDesa.setTooltip (juce::String::fromUTF8 ("Desa'l com a patró nou o com a variació d'un patró (surt a la columna de variacions)"));
        btDesa.onClick = [this] { menuDesa(); };   // 0.53
        btEsborra.setVisible (propi);
        btEsborra.onClick = [this]
        {
            if (onEsborra && ! idOriginal.empty()) onEsborra (idOriginal);
            idOriginal.clear();
            btEsborra.setVisible (false);
            avis = juce::String::fromUTF8 ("Esborrat dels teus rasgueigs.");
            repaint();
        };
        setSize (ampleTotal(), 12 + 30 + 10 + 16 + numFiles * altFila + 8 + 18 + 8 + 30 + 10);
    }

    mibase::guitarra::Patro patro() const
    {
        return mibase::guitarra::fesPropi (nom.getText().trim().toStdString(), compas, selPas.getTriat() == 1 ? 240 : 480, passos, forma, variacioDe);
    }

    /** 0.53: el patró triat a l'editor (per oferir «Desa com a variació de…»). */
    void posaArrel (const std::string& id, const std::string& nomPatro) { arrel = id; nomArrel = nomPatro; }

private:
    std::string arrel, nomArrel, variacioDe;
    std::unique_ptr<PanellNomRasgueig> panellNom;

    static juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

    /** 0.84: «nom», o «nom 2», «nom 3»… si ja hi ha un patró meu amb aquest nom. */
    juce::String nomLliure (const juce::String& base) const
    {
        auto existeix = [] (const juce::String& n) { return mibase::guitarra::patro ("propi:" + n.toStdString()) != nullptr; };
        if (base.isEmpty() || ! existeix (base)) return base;
        for (int k = 2; k < 1000; ++k)
            if (! existeix (base + " " + juce::String (k))) return base + " " + juce::String (k);
        return base;
    }

    /** 0.53: en desar, es tria si és un patró nou o una variació d'un patró (el triat, el d'abans o qualsevol altre). */
    void menuDesa()
    {
        namespace gt = mibase::guitarra;
        juce::PopupMenu m;
        // 0.84: guardar els canvis al mateix patró (si és dels meus), o fer-ne un de nou sense tocar aquest
        const auto* propi = idOriginal.empty() ? nullptr : gt::patro (idOriginal);
        if (propi != nullptr) m.addItem (4, juce::String::fromUTF8 ("Desa els canvis a «") + u8 (propi->nom) + juce::String::fromUTF8 ("»"));
        m.addItem (1, juce::String::fromUTF8 ("Desa com a patró nou (a «Els meus rasgueigs»)…"));
        if (! arrel.empty()) m.addItem (2, juce::String::fromUTF8 ("Desa com a variació de «") + u8 (nomArrel) + juce::String::fromUTF8 ("»…"));
        const auto* abans = variacioDe.empty() || variacioDe == arrel ? nullptr : gt::patro (variacioDe);
        if (abans != nullptr) m.addItem (3, juce::String::fromUTF8 ("Desa com a variació de «") + u8 (abans->nom) + juce::String::fromUTF8 ("»…"));
        // qualsevol altre patró, per grups
        std::vector<const gt::Patro*> tots;
        std::vector<std::string> grups;
        for (auto& p : gt::patrons()) { tots.push_back (&p); if (std::find (grups.begin(), grups.end(), p.grup) == grups.end()) grups.push_back (p.grup); }
        juce::PopupMenu altres;
        for (auto& gr : grups)
        {
            juce::PopupMenu sub;
            for (size_t i = 0; i < tots.size(); ++i) if (tots[i]->grup == gr) sub.addItem (1000 + (int) i, u8 (tots[i]->nom));
            altres.addSubMenu (u8 (gr), sub);
        }
        m.addSubMenu (juce::String::fromUTF8 ("Desa com a variació d'un altre patró"), altres);
        juce::Component::SafePointer<EditorRasgueig> jo (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btDesa), [jo, tots] (int r)
        {
            if (jo == nullptr || r <= 0) return;
            if (r == 4)
            {
                if (const auto* q = mibase::guitarra::patro (jo->idOriginal))
                {
                    jo->nom.setText (u8 (q->nom), false);
                    jo->variacioDe = q->de;
                    std::string nomDe;
                    if (const auto* d = q->de.empty() ? nullptr : mibase::guitarra::patro (q->de)) nomDe = d->nom;
                    jo->desaAra (nomDe);
                }
            }
            else if (r == 1) jo->demanaNom ({}, {});
            else if (r == 2) jo->demanaNom (jo->arrel, jo->nomArrel);
            else if (r == 3) { if (const auto* p = mibase::guitarra::patro (jo->variacioDe)) jo->demanaNom (p->id, p->nom); }
            else if (r >= 1000 && r - 1000 < (int) tots.size()) jo->demanaNom (tots[(size_t) (r - 1000)]->id, tots[(size_t) (r - 1000)]->nom);
        });
    }

    void demanaNom (const std::string& de, const std::string& nomDe)
    {
        const auto titol = de.empty() ? juce::String::fromUTF8 ("Nom del patró nou")
                                      : juce::String::fromUTF8 ("Nom de la variació de «") + u8 (nomDe) + juce::String::fromUTF8 ("»");
        auto suggerit = nom.getText().trim();
        if (! de.empty() && (suggerit.isEmpty() || suggerit == u8 (nomDe) + juce::String::fromUTF8 (" (meu)"))) suggerit = u8 (nomDe) + juce::String::fromUTF8 (" (la meva)");
        suggerit = nomLliure (suggerit);   // 0.84: mai el d'un patró que ja existeix (si no, el sobreescriuria)
        panellNom = std::make_unique<PanellNomRasgueig> (titol, suggerit);
        juce::Component::SafePointer<EditorRasgueig> jo (this);
        panellNom->onCancella = [jo] { if (jo != nullptr) juce::MessageManager::callAsync ([jo] { if (jo != nullptr) jo->panellNom.reset(); }); };
        panellNom->onDesa = [jo, de, nomDe] (const juce::String& t)
        {
            if (jo == nullptr) return;
            jo->nom.setText (jo->nomLliure (t.trim()), false);   // 0.84: si el nom ja hi és, s'hi afegeix un número
            jo->variacioDe = de;
            jo->desaAra (nomDe);
            juce::MessageManager::callAsync ([jo] { if (jo != nullptr) jo->panellNom.reset(); });
        };
        addAndMakeVisible (*panellNom);
        panellNom->setBounds (getLocalBounds());
        panellNom->comenca();
    }

    void desaAra (const std::string& nomDe)
    {
        const auto p = patro();
        if (onDesa) onDesa (p);
        // 0.84: desar amb un altre nom fa un patró NOU i l'original es queda (abans s'esborrava: només quedava l'últim)
        idOriginal = p.id;
        btEsborra.setVisible (true);
        avis = p.de.empty() ? juce::String::fromUTF8 ("Desat a «Els meus rasgueigs».")
                            : juce::String::fromUTF8 ("Desat com a variació de «") + u8 (nomDe) + juce::String::fromUTF8 ("»: surt a la columna de variacions.");
        repaint();
    }

public:

    void resized() override
    {
        auto f = juce::Rectangle<int> (10, 12, getWidth() - 20, 30);
        numPassos.setBounds (f.removeFromRight (96)); f.removeFromRight (8);
        selPas.setBounds (f.removeFromRight (selPas.ampladaIdeal())); f.removeFromRight (8);
        nom.setBounds (f);
        auto b = juce::Rectangle<int> (10, getHeight() - 40, getWidth() - 20, 30);
        btEscolta.setBounds (b.removeFromLeft (78)); b.removeFromLeft (6);
        btBucle.setBounds (b.removeFromLeft (78)); b.removeFromLeft (6);
        btAtura.setBounds (b.removeFromLeft (78));
        btEsborra.setBounds (b.removeFromRight (84)); b.removeFromRight (6);
        btDesa.setBounds (b.removeFromRight (84));
        if (panellNom != nullptr) panellNom->setBounds (getLocalBounds());   // 0.53
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::lateral);
        static const char* const noms[numFiles] = { "Cop", "Accent", "Fluix", "Palm mute", "Slap", "Curt", "Cordes", "Dit", "Dit lliure" };
        const auto z = graella();
        const int perTemps = selPas.getTriat() == 1 ? 4 : 2;
        for (int i = 0; i < (int) passos.size(); ++i)
        {
            const auto cap = juce::Rectangle<int> (z.getX() + i * ampleCasella, z.getY() - 16, ampleCasella, 16);
            g.setColour (i % perTemps == 0 ? tema::text : tema::suau);
            g.setFont (dibuix::mono (10.0f));
            g.drawText (i % perTemps == 0 ? juce::String (i / perTemps + 1) : juce::String::fromUTF8 ("\xc2\xb7"), cap, juce::Justification::centred);
        }
        for (int f = 0; f < numFiles; ++f)
        {
            g.setColour (tema::textMig);
            g.setFont (dibuix::lletra (11.5f));
            g.drawText (juce::String::fromUTF8 (noms[f]), 10, z.getY() + f * altFila, ampleNoms - 8, altFila, juce::Justification::centredLeft);
            for (int i = 0; i < (int) passos.size(); ++i)
            {
                const auto c = casella (i, f).toFloat().reduced (1.5f);
                const auto t = text (passos[(size_t) i], f);
                const bool ences = t.isNotEmpty();
                g.setColour (f == 0 ? (ences ? tema::accent.withAlpha (0.85f) : juce::Colours::black.withAlpha (0.45f))
                                    : (ences ? vintage::ledGroc.withAlpha (0.8f) : juce::Colours::black.withAlpha (i % perTemps == 0 ? 0.45f : 0.3f)));
                g.fillRoundedRectangle (c, 3.0f);
                if (! ences) continue;
                g.setColour (juce::Colours::black);
                g.setFont (f == 0 ? dibuix::lletra (15.0f, true) : dibuix::lletra (11.0f, true));
                g.drawText (t, c, juce::Justification::centred, false);
            }
        }
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.0f));
        g.drawText (avis.isNotEmpty() ? avis : juce::String::fromUTF8 ("Clica per canviar; arrossega de costat per repetir. «Dit lliure»: fonamental + 5a, 6a o 7a."),
                    10, z.getBottom() + 8, getWidth() - 20, 18, juce::Justification::centredLeft, true);
    }

    // 0.49: arrossegant de costat, les caselles de la mateixa fila queden com la primera que s'ha clicat.
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto z = graella();
        if (filaGest < 0 || pasGest < 0 || pasGest >= (int) passos.size()) return;
        const int i = juce::jlimit (0, (int) passos.size() - 1, (e.x - z.getX()) / ampleCasella);
        if (i == ultimGest) return;
        using namespace mibase::guitarra;
        const Pas model = passos[(size_t) pasGest];
        for (int k = juce::jmin (i, ultimGest); k <= juce::jmax (i, ultimGest); ++k)
        {
            if (k == pasGest) continue;
            auto& p = passos[(size_t) k];
            if (filaGest == 0) { p = Pas(); p.cop = model.cop; }
            else
            {
                if (p.cop == Cop::Res || p.cop == Cop::Silenci) { if (text (model, filaGest).isEmpty()) continue; p.cop = Cop::Avall; }
                switch (filaGest)
                {
                    case 1: p.accent = model.accent; break;
                    case 2: p.suau = model.suau; break;
                    case 3: case 4: p.tec = model.tec; break;
                    case 5: p.curt = model.curt; break;
                    case 6: p.abast = model.abast; break;
                    case 7: p.dit = model.dit; break;
                    default: p.lliure = model.lliure; break;
                }
            }
            const auto net = llegeixPassos (textPassos ({ p }));
            if (net.size() == 1) p = net[0];
        }
        ultimGest = i;
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (filaGest >= 0) canviat();   // 0.49.1: se sent en deixar anar (un clic o tot el que s'ha pintat arrossegant)
        filaGest = pasGest = -1;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto z = graella();
        filaGest = pasGest = -1;
        if (! z.contains (e.getPosition())) return;
        const int i = (e.x - z.getX()) / ampleCasella, f = (e.y - z.getY()) / altFila;
        if (i < 0 || i >= (int) passos.size() || f < 0 || f >= numFiles) return;
        filaGest = f; pasGest = ultimGest = i;
        auto& p = passos[(size_t) i];
        const bool enrere = e.mods.isPopupMenu();
        using namespace mibase::guitarra;
        auto cal = [&] { if (p.cop == Cop::Res || p.cop == Cop::Silenci) p.cop = Cop::Avall; };   // un detall demana un cop
        switch (f)
        {
            case 0:
            {
                static constexpr Cop ordre[] = { Cop::Res, Cop::Avall, Cop::Amunt, Cop::MutAvall, Cop::MutAmunt, Cop::Silenci };
                int k = 0;
                for (int j = 0; j < 6; ++j) if (ordre[j] == p.cop) k = j;
                p = Pas();
                p.cop = ordre[(k + (enrere ? 5 : 1)) % 6];
                break;
            }
            case 1: cal(); p.accent = ! p.accent; break;
            case 2: cal(); p.suau = ! p.suau; if (p.suau) p.tec = Tecnica::Obert; break;
            case 3: cal(); p.tec = p.tec == Tecnica::Palm ? Tecnica::Obert : Tecnica::Palm; break;
            case 4: cal(); p.tec = p.tec == Tecnica::Slap ? Tecnica::Obert : Tecnica::Slap; break;
            case 5: cal(); p.curt = ! p.curt; break;
            case 6: cal(); p.abast = (Abast) (((int) p.abast + (enrere ? 2 : 1)) % 3); break;
            case 7: cal(); p.dit = (Dit) (((int) p.dit + (enrere ? 9 : 1)) % 10); break;
            default:
            {
                cal();
                static constexpr int ordre[] = { 0, 5, 6, 7 };
                int k = 0;
                for (int j = 0; j < 4; ++j) if (ordre[j] == p.lliure) k = j;
                p.lliure = ordre[(k + (enrere ? 3 : 1)) % 4];
                if (p.lliure != 0) { p.dit = Dit::Cap; if (p.tec == Tecnica::Slap) p.tec = Tecnica::Obert; if (p.cop == Cop::MutAvall || p.cop == Cop::MutAmunt) p.cop = Cop::Avall; }
                break;
            }
        }
        // El que el text d'un rasgueig no pot dir no es queda a mitges: es passa pel mateix camí que quan es desa.
        const auto net = llegeixPassos (textPassos ({ p }));
        if (net.size() == 1) p = net[0];
        avis.clear();
        repaint();
    }

private:
    int ampleTotal() const { return juce::jmax (470, 10 + ampleNoms + (int) passos.size() * ampleCasella + 12); }
    juce::Rectangle<int> graella() const { return { 10 + ampleNoms, 12 + 30 + 10 + 16, (int) passos.size() * ampleCasella, numFiles * altFila }; }
    juce::Rectangle<int> casella (int i, int f) const { const auto z = graella(); return { z.getX() + i * ampleCasella, z.getY() + f * altFila, ampleCasella, altFila }; }
    static juce::String text (const Pas& p, int fila)
    {
        using namespace mibase::guitarra;
        const bool dit = p.dit != Dit::Cap;
        switch (fila)
        {
            case 0:
                switch (p.cop)
                {
                    case Cop::Avall:    return juce::String::fromUTF8 ("\xe2\x86\x93");
                    case Cop::Amunt:    return juce::String::fromUTF8 ("\xe2\x86\x91");
                    case Cop::MutAvall: return juce::String::fromUTF8 ("x\xe2\x86\x93");
                    case Cop::MutAmunt: return juce::String::fromUTF8 ("x\xe2\x86\x91");
                    case Cop::Silenci:  return juce::String::fromUTF8 ("\xe2\x80\x93");
                    case Cop::Res:      return {};
                }
                return {};
            case 1: return p.accent ? ">" : "";
            case 2: return p.suau && ! dit ? "f" : "";
            case 3: return p.tec == Tecnica::Palm && ! dit ? "P" : "";
            case 4: return p.tec == Tecnica::Slap && ! dit ? "S" : "";
            case 5: return p.curt && ! dit ? "!" : "";
            case 6: return dit ? "" : p.abast == Abast::Baix ? "greu" : p.abast == Abast::Aguts ? "ag." : "";
            case 7:
            {
                static const char* const noms[] = { "", "p", "p2", "i", "m", "a", "pl", "p+", "tap", "r" };
                return noms[(int) p.dit];
            }
            default: return p.lliure == 5 ? "5a" : p.lliure == 6 ? "6a" : p.lliure == 7 ? "7a" : "";
        }
    }

    std::vector<Pas> passos;
    std::string idOriginal, compas = "4/4";
    int pasAnterior = 0;
    int filaGest = -1, pasGest = -1, ultimGest = -1;   // 0.49: per pintar arrossegant
    juce::TextEditor nom;
    Segmentat selPas { { "Corxeres", "Semicorxeres" } };
    CaixaNumero numPassos { "Passos", 2, maxPassos, 8 };
    Boto btEscolta { "Escolta", std::nullopt, Boto::Estil::Secundari };
    Boto btBucle { "Bucle", std::nullopt, Boto::Estil::Tecla };
    Boto btAtura { "Para", std::nullopt, Boto::Estil::Secundari };
    bool bucle = false;
    void sona() { if (onEscolta) onEscolta (patro()); }
    int msVolta() const { return juce::jmax (150, juce::roundToInt ((double) passos.size() * (selPas.getTriat() == 1 ? 240.0 : 480.0) * msPerTick)); }
    void posaBucle (bool si)
    {
        bucle = si;
        btBucle.setToggleState (si, juce::dontSendNotification);
        if (si) { sona(); startTimer (msVolta()); } else stopTimer();
    }
    // Cada canvi se sent: si el bucle roda, a la volta següent (sense tallar-lo); si no, de seguida i una vegada.
    void canviat()
    {
        if (bucle) { if (getTimerInterval() != msVolta()) startTimer (msVolta()); }
        else sona();
    }
    void timerCallback() override { sona(); }
    Boto btDesa { "Desa", std::nullopt, Boto::Estil::Primari };
    Boto btEsborra { "Esborra", std::nullopt, Boto::Estil::Secundari };
    juce::String avis;
};
