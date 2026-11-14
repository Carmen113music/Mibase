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

class EditorRasgueig : public juce::Component
{
public:
    using Pas = mibase::guitarra::Pas;
    using Cop = mibase::guitarra::Cop;
    static constexpr int maxPassos = 32, ampleNoms = 86, ampleCasella = 24, altFila = 26, numFiles = 8;

    std::function<void (const mibase::guitarra::Patro&)> onEscolta, onDesa;
    std::function<void (const std::string& id)> onEsborra;

    explicit EditorRasgueig (const mibase::guitarra::Patro* inicial)
    {
        const bool propi = inicial != nullptr && inicial->id.rfind ("propi:", 0) == 0;
        idOriginal = propi ? inicial->id : std::string();
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
        for (auto* c : std::initializer_list<juce::Component*> { &nom, &selPas, &numPassos, &btEscolta, &btDesa, &btEsborra }) addAndMakeVisible (c);
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
            repaint();
        };
        numPassos.onCanvi = [this] (int n)
        {
            passos.resize ((size_t) juce::jlimit (2, maxPassos, n));
            setSize (ampleTotal(), getHeight());
            repaint();
        };
        btEscolta.onClick = [this] { if (onEscolta) onEscolta (patro()); };
        btDesa.onClick = [this]
        {
            if (nom.getText().trim().isEmpty()) { avis = juce::String::fromUTF8 ("Posa-li un nom per desar-lo."); repaint(); return; }
            const auto p = patro();
            if (onDesa) onDesa (p);
            if (! idOriginal.empty() && idOriginal != p.id && onEsborra) onEsborra (idOriginal);   // li han canviat el nom
            idOriginal = p.id;
            btEsborra.setVisible (true);
            avis = juce::String::fromUTF8 ("Desat a «Els meus rasgueigs».");
            repaint();
        };
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
        return mibase::guitarra::fesPropi (nom.getText().trim().toStdString(), compas, selPas.getTriat() == 1 ? 240 : 480, passos);
    }

    void resized() override
    {
        auto f = juce::Rectangle<int> (10, 12, getWidth() - 20, 30);
        numPassos.setBounds (f.removeFromRight (96)); f.removeFromRight (8);
        selPas.setBounds (f.removeFromRight (selPas.ampladaIdeal())); f.removeFromRight (8);
        nom.setBounds (f);
        auto b = juce::Rectangle<int> (10, getHeight() - 40, getWidth() - 20, 30);
        btEscolta.setBounds (b.removeFromLeft (100)); b.removeFromLeft (8);
        btDesa.setBounds (b.removeFromLeft (100)); b.removeFromLeft (8);
        btEsborra.setBounds (b.removeFromLeft (100));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::lateral);
        static const char* const noms[numFiles] = { "Cop", "Accent", "Fluix", "Palm mute", "Slap", "Curt", "Cordes", "Dit" };
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
        g.drawText (avis.isNotEmpty() ? avis : juce::String::fromUTF8 ("Clica una casella per canviar-la. «Cop»: avall, amunt, nota morta, silenci o res (deixa sonar)."),
                    10, z.getBottom() + 8, getWidth() - 20, 18, juce::Justification::centredLeft, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto z = graella();
        if (! z.contains (e.getPosition())) return;
        const int i = (e.x - z.getX()) / ampleCasella, f = (e.y - z.getY()) / altFila;
        if (i < 0 || i >= (int) passos.size() || f < 0 || f >= numFiles) return;
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
            default: cal(); p.dit = (Dit) (((int) p.dit + (enrere ? 9 : 1)) % 10); break;
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
            default:
            {
                static const char* const noms[] = { "", "p", "p2", "i", "m", "a", "pl", "p+", "tap", "r" };
                return noms[(int) p.dit];
            }
        }
    }

    std::vector<Pas> passos;
    std::string idOriginal, compas = "4/4";
    int pasAnterior = 0;
    juce::TextEditor nom;
    Segmentat selPas { { "Corxeres", "Semicorxeres" } };
    CaixaNumero numPassos { "Passos", 2, maxPassos, 8 };
    Boto btEscolta { "Escolta", std::nullopt, Boto::Estil::Secundari };
    Boto btDesa { "Desa", std::nullopt, Boto::Estil::Primari };
    Boto btEsborra { "Esborra", std::nullopt, Boto::Estil::Secundari };
    juce::String avis;
};
