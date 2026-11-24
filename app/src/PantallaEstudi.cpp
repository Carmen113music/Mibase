#include "PantallaEstudi.h"
#include <mibase/DeteccioAcords.h>
#include <mibase/ContextMusical.h>
#include <array>
#include "FinestraFlotant.h"
#include "DialegExporta.h"
#include "Tancaments.h"
#include "BancGM.h"
#include "EscGlobal.h"
#include "Inserts.h"

using namespace mibase;
namespace ed = mibase::edicio;

namespace
{
    constexpr int alcadaBarra = 58;
    constexpr int alcadaRegla = 34;
    constexpr int gruixBarres = 10;

    /** Fader petit horitzontal per al volum de la claqueta (0 a 150 %). */
    class FaderPetit : public juce::Component
    {
    public:
        std::function<void (double)> onCanvi;
        double valor = 1.0;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto text = r.removeFromRight (48.0f);
            auto carril = r.reduced (8.0f, 0.0f).withSizeKeepingCentre (r.getWidth() - 16.0f, 6.0f);
            vintage::ranura (g, carril);
            g.setColour (tema::linia2);
            const float x100 = carril.getX() + carril.getWidth() / 1.5f;
            g.fillRect (juce::Rectangle<float> (x100 - 0.5f, r.getY() + 2.0f, 1.0f, 5.0f));
            const float x = carril.getX() + carril.getWidth() * (float) (valor / 1.5);
            vintage::capFader (g, juce::Rectangle<float> (x - 7.0f, r.getCentreY() - 11.0f, 14.0f, 22.0f), vintage::ledGroc, false);
            g.setColour (tema::textMig);
            g.setFont (dibuix::mono (12.0f));
            g.drawText (juce::String (juce::roundToInt (valor * 100.0)) + " %", text, juce::Justification::centredRight);
        }

        void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const float ample = (float) getWidth() - 48.0f - 16.0f;
            valor = juce::jlimit (0.0, 1.5, (double) (e.position.x - 8.0f) / ample * 1.5);
            if (std::abs (valor - 1.0) < 0.04) valor = 1.0;
            repaint();
            if (onCanvi) onCanvi (valor);
        }
        void mouseDoubleClick (const juce::MouseEvent&) override { valor = 1.0; repaint(); if (onCanvi) onCanvi (valor); }
    };

    /** Finestreta de la claqueta: encesa, volum, so i «escolta». */
    class PanellClic : public juce::Component
    {
    public:
        PanellClic (bool encesa, double volum, int so, std::function<void (bool, double, int)> canvi, std::function<void (bool)> escolta)
            : onCanvi (std::move (canvi)), onEscolta (std::move (escolta))
        {
            addAndMakeVisible (btEncesa);
            btEncesa.setLlum (vintage::ledGroc);
            btEncesa.setToggleState (encesa, juce::dontSendNotification);
            btEncesa.onClick = [this] { btEncesa.setToggleState (! btEncesa.getToggleState(), juce::dontSendNotification); avisa(); };

            addAndMakeVisible (volum_);
            volum_.valor = volum;
            volum_.onCanvi = [this] (double) { avisa(); };

            addAndMakeVisible (sons);
            sons.setTriat (so);
            sons.setLlum (vintage::ledGroc);
            sons.onCanvi = [this] (int) { avisa(); };

            addAndMakeVisible (btEscolta);
            btEscolta.setLlum (vintage::ledVerd);
            btEscolta.onClick = [this]
            {
                btEscolta.setToggleState (! btEscolta.getToggleState(), juce::dontSendNotification);
                if (onEscolta) onEscolta (btEscolta.getToggleState());
            };
            setSize (380, 214);
        }

        ~PanellClic() override { if (onEscolta) onEscolta (false); }

        void paint (juce::Graphics& g) override
        {
            vintage::panell (g, getLocalBounds().toFloat(), 8.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (15.0f, true));
            g.drawText (juce::String::fromUTF8 ("Claqueta"), 16, 10, 200, 26, juce::Justification::centredLeft);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.0f));
            g.drawText ("Volum", 16, volum_.getY() - 18, 200, 16, juce::Justification::centredLeft);
            g.drawText ("So", 16, sons.getY() - 18, 200, 16, juce::Justification::centredLeft);
        }

        void resized() override
        {
            btEncesa.setBounds (getWidth() - 16 - 100, 10, 100, 40);
            volum_.setBounds (16, 70, getWidth() - 32, 28);
            sons.setBounds (16, 124, getWidth() - 32, 40);
            btEscolta.setBounds (16, 172, 150, 34);
        }

    private:
        void avisa() { if (onCanvi) onCanvi (btEncesa.getToggleState(), volum_.valor, sons.getTriat()); }

        std::function<void (bool, double, int)> onCanvi;
        std::function<void (bool)> onEscolta;
        Boto btEncesa { juce::String::fromUTF8 ("Encesa"), std::nullopt, Boto::Estil::Tecla };
        FaderPetit volum_;
        Segmentat sons { { "Digital", "Fusta", juce::String::fromUTF8 ("Analògic"), "Campana" } };
        Boto btEscolta { "Escolta-la", dibuix::Icona::Play, Boto::Estil::Tecla };
    };
}

//==============================================================================
PantallaEstudi::PantallaEstudi (const juce::File& f, const Projecte& p, juce::PropertiesFile& a, Historial& h, ServeiAudio& s, Precarrega* precarrega)
    : ajustos (a), historial (h), servei (s)
{
    setWantsKeyboardFocus (true);
    formats.registerBasicFormats();

    estat.projecte = p;
    estat.fitxer = f;
    estat.carpeta = f.getParentDirectory();
    estat.pxPerSegon = ajustos.getDoubleValue ("estudi.zoom", 48.0);
    estat.altPista = juce::jmax (VistaPistes::alcadaMinima, ajustos.getIntValue ("estudi.altPista", 80));
    if (! estat.projecte.pistes.empty()) estat.pistaTriada = 0;
    if (auto* t = estat.pista (estat.pistaTriada)) idTriadaAbans = t->id;   // 0.72
    // 0.78: «Implementa a pistes reals» / «Treu la cadena» des de la finestra del Vocal Studio
    plugins.onAccioRack = [this] (const std::string& idInsert, int accio) -> int
    {
        int pista = -1;
        const mibase::Insert* in = nullptr;
        for (int i = 0; i < estat.numPistes() && pista < 0; ++i)
            for (const auto& x : estat.projecte.pistes[(size_t) i].inserts)
                if (x.id == idInsert) { pista = i; in = &x; }
        if (pista < 0 || in == nullptr) return 0;
        if (accio == 0) return (int) rack::busosDesplegats (estat.projecte, pista).size();
        if (accio == 3) return estat.projecte.pistes[(size_t) pista].solo ? 1 : 0;   // 0.79: el botó S del Vocal Studio
        if (accio == 4)
        {
            estat.desfes.abans (estat.projecte);
            auto& t = estat.projecte.pistes[(size_t) pista];
            t.solo = ! t.solo;
            estat.canviat();
            return t.solo ? 1 : 0;
        }
        plugins.desaEstats (estat.projecte);   // el que hi ha ara al Vocal Studio viu
        const auto abans = estat.projecte;
        const juce::String nom = dibuix::utf8 (estat.projecte.pistes[(size_t) pista].nom);
        if (accio == 1)
        {
            const auto r = rack::desplega (estat.projecte, pista, idInsert);
            if (! r.correcte)
            {
                // 0.83: que es vegi per què no fa res
                estat.projecte = abans;
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "A pistes reals",
                    dibuix::utf8 (r.error) + "\n\n«A pistes reals» converteix els retorns en paral·lel del Vocal Studio (els camins de sota "
                    "la cadena principal) en busos de veritat. La cadena principal es queda dins el Vocal Studio. Afegeix-hi un retorn "
                    "(o tria un preset amb camins en paral·lel) i torna-ho a provar."_ca);
                return 0;
            }
            estat.desfes.abans (abans);
            // 0.83: la vista baixa fins a la carpeta nova (és al final, amb els busos)
            for (int i = 0; i < (int) estat.projecte.pistes.size(); ++i)
                if (estat.projecte.pistes[(size_t) i].nom.rfind (std::string (rack::prefixDesplega) + nom.toStdString(), 0) == 0) { estat.desplY = juce::jmax (0, estat.yPista (i) + estat.desplY - 20); break; }
            // el Vocal Studio viu: ara només amb la cadena principal
            for (const auto& t : estat.projecte.pistes)
                for (const auto& x : t.inserts)
                    if (x.id == idInsert)
                        if (auto* rk = dynamic_cast<rack::Rack*> (plugins.parametres (x))) rk->posaEstat (x.estat);
            estat.canviat ("Vocal Studio de «"_ca + nom + "» a pistes reals: "_ca + juce::String (r.busos) + " busos"_ca);
            estat.info = "Fet: "_ca + juce::String (r.busos) + " busos «VS · "_ca + nom + " · …» a la carpeta «VS · "_ca + nom + "». Per treure-ho: «Treu la cadena» o Desfés."_ca;
            estat.avisa();
            return r.busos;
        }
        estat.desfes.abans (abans);
        const int n = rack::treuDesplegament (estat.projecte, pista);
        estat.canviat ("Treta la cadena «VS · "_ca + nom + "» ("_ca + juce::String (n) + " busos)"_ca);
        return n;
    };
    estat.addChangeListener (this);
    estat.onCanviat = [this] (const juce::String& evolucio)
    {
        if (evolucio.isNotEmpty())
            disc::afegeixEvolucio (dibuix::cami (estat.carpeta), dibuix::araText().toStdString(), evolucio.toStdString());
        calDesar = true;
        startTimer (700);
    };

    // 0.28.1: els àudios ja s'han llegit en segon pla (mentre es veia la finestra de càrrega); si no, ara
    for (auto& presa : estat.projecte.preses)
    {
        if (precarrega != nullptr)
            if (auto it = precarrega->find (presa.id); it != precarrega->end() && it->second.ona != nullptr)
            {
                estat.ones[presa.id] = it->second.ona;
                audioPreses[presa.id] = it->second.audio;
                continue;
            }
        carregaOna (presa);
    }
    if (progresGlobal()) progresGlobal() ("Preparant el Vocal Editor…"_ca, 0.88);
    sincronitzaVeu();   // 0.22: les preses corregides es preparen encara que no s'obri el Vocal Editor

    estat.nomsEntrades = servei.getNomsEntrades();
    estat.onCapcalMogut = [this] (double t) { if (estat.transport == 1) { plugins.buidaCues(); servei.estudi().situa (t); } };
    rellotge.tic = [this] { tic(); };
    rellotge.startTimerHz (30);

    ampleEsquerra = ajustos.getIntValue ("estudi.ampleEsquerra", 132);
    ampleNoms = ajustos.getIntValue ("estudi.ampleNoms3", 244);   // 0.41: més ampla, perquè hi càpiguen «Ac» i E/S
    amplePanell = ajustos.getIntValue ("estudi.amplePanell", 270);

    // Caixa d'eines.
    struct E { dibuix::Icona icona; const char* ajuda; };
    const E llista[] = {
        { dibuix::Icona::Fletxa2, "Fletxa (1): tria, mou, retalla, fades i volum" },
        { dibuix::Icona::Llapis,  "Llapis (2): fa una finestra nova (arrossegant, o d'un compàs)" },
        { dibuix::Icona::Goma,    "Goma (3): esborra clips" },
        { dibuix::Icona::Tisores, "Tisores (4): talla un clip on cliques" },
        { dibuix::Icona::Cola,    "Cola (5): ajunta dos clips en una sola finestra" },
        { dibuix::Icona::Silenci, "X (6): silencia o fa sonar un clip" },
        { dibuix::Icona::Elastic, "Elastic Audio (7): estira el clip per les vores o mou el temps amb marcadors, sense canviar el to" },
    };
    for (int i = 0; i < 7; ++i)
    {
        auto* b = eines.add (new Boto ({}, llista[i].icona, Boto::Estil::Tecla));
        b->setLlum (vintage::ledTaronja);
        b->setTooltip (juce::String::fromUTF8 (llista[i].ajuda));
        b->onClick = [this, i] { triaEina ((Eina) i); };
        addAndMakeVisible (b);
    }

    for (auto* b : { &btClic, &btClicMenu, &btTempo, &btDesfes, &btRefes, &btRequadre, &btInsereix })
        addAndMakeVisible (b);
    addAndMakeVisible (btColor);
    estat.modeRang = ajustos.getBoolValue ("estudi.modeRang", false);
    estat.insereix = ajustos.getBoolValue ("estudi.insereix", false);
    btRequadre.setLlum (vintage::ledTaronja);
    btRequadre.onClick = [this] { canviaModeRang (! estat.modeRang); };
    btInsereix.setLlum (vintage::ledTaronja);
    btInsereix.onClick = [this] { canviaInsereix (! estat.insereix); };
    btColor.setTooltip ("Color dels clips triats o, si no n'hi ha, de la pista"_ca);
    btColor.onClick = [this] { triaColor(); };
    addAndMakeVisible (snap);

    btClic.setLlum (vintage::ledGroc);
    btClic.setTooltip ("Claqueta encesa o apagada"_ca);
    btClic.onClick = [this]
    {
        estat.projecte.clic = ! estat.projecte.clic;
        estat.avisa();
        calDesar = true;
        startTimer (700);
    };
    btClicMenu.setTooltip ("Volum i so de la claqueta"_ca);
    btClicMenu.onClick = [this] { menuClaqueta(); };
    btTempo.setTooltip ("Canvia el tempo"_ca);
    btTempo.onClick = [this]
    {
        // 0.27: tempo i compàs
        juce::PopupMenu m;
        m.addItem (1, "Tempo… ("_ca + juce::String ((int) std::round (estat.projecte.tempo)) + " bpm)");
        m.addSeparator();
        m.addSectionHeader ("Compàs"_ca);
        static const int compassos[][2] = { { 2, 4 }, { 3, 4 }, { 4, 4 }, { 5, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 } };
        for (int i = 0; i < 8; ++i)
            m.addItem (10 + i, juce::String (compassos[i][0]) + "/" + juce::String (compassos[i][1]), true,
                       estat.projecte.compasNum == compassos[i][0] && estat.projecte.compasDen == compassos[i][1]);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btTempo), [this] (int r)
        {
            if (r == 1) { canviaTempo(); return; }
            if (r < 10 || r > 17) return;
            const int num = compassos[r - 10][0], den = compassos[r - 10][1];
            if (num == estat.projecte.compasNum && den == estat.projecte.compasDen) return;
            estat.desfes.abans (estat.projecte);
            estat.projecte.compasNum = num;
            estat.projecte.compasDen = den;
            servei.motor().tempsPerCompasClic = num;
            estat.canviat ("Compàs: "_ca + juce::String (num) + "/" + juce::String (den));
        });
    };
    snap.setTriat ((int) estat.projecte.snap);
    snap.onCanvi = [this] (int i) { estat.projecte.snap = (Snap) i; estat.avisa(); calDesar = true; startTimer (700); };
    addAndMakeVisible (btGraella);
    btGraella.setTooltip ("Subdivisió de la graella de la taula de treball (per moure i retallar clips amb «Graella»)"_ca);
    btGraella.onClick = [this] { menuGraella(); };
    btDesfes.setTooltip ("Desfés (Ctrl+Z)"_ca);
    btDesfes.onClick = [this] { desfes (false); };
    btRefes.setTooltip ("Refés (Ctrl+Y)"_ca);
    btRefes.onClick = [this] { desfes (true); };

    // Seccions.
    for (auto* sec : std::initializer_list<SeccioEstudi*> { &esquerra, &capcaleres, &regla, &pistes, &mescles, &panell })
    {
        addAndMakeVisible (sec);
        sec->onRoda = [this] (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) { roda (e, w); };
    }
    mescles.onRoda = nullptr;
    capcaleres.onCanviaNom = [this] (int i) { canviaNom (i); };
    mescles.onCanviaNom = [this] (int i) { canviaNom (i); };
    mescles.onBusNou = [this] (bool fx, int pista, bool comSortida) { busNou (fx, pista, comSortida); };
    capcaleres.onPistaNova = [this] (const juce::String& tipus, int canals) { pistaNova (tipus, canals); };
    pistes.onPistaNova = [this] (const juce::String& tipus, int canals) { pistaNova (tipus, canals); };
    pistes.onEscoltaNotes = [this] (const std::vector<int>& notes)
    {
        // 0.37: escoltar un acord de la pista d'Acords: sona per la primera pista que segueix els acords
        // (o la MIDI triada, o la primera MIDI que hi hagi)
        int on = -1;
        for (int i = 0; i < estat.numPistes() && on < 0; ++i)
            if (estat.projecte.pistes[(size_t) i].segueixAcords && estat.projecte.pistes[(size_t) i].tipus == "midi") on = i;
        if (on < 0 && estat.pista (estat.pistaTriada) != nullptr && estat.pista (estat.pistaTriada)->tipus == "midi") on = estat.pistaTriada;
        for (int i = 0; i < estat.numPistes() && on < 0; ++i)
            if (estat.projecte.pistes[(size_t) i].tipus == "midi") on = i;
        if (on < 0) { estat.info = "Per escoltar l'acord cal una pista MIDI"_ca; estat.avisa(); return; }
        const int slot = slotDePista (estat.projecte.pistes[(size_t) on]);
        for (int n : notes) servei.estudi().enviaDirecte ({ slot, (std::uint8_t) 0x90, (std::uint8_t) juce::jlimit (0, 127, n), (std::uint8_t) 92 });
        juce::Component::SafePointer<PantallaEstudi> jo (this);
        juce::Timer::callAfterDelay (900, [jo, slot, notes]
        {
            if (jo == nullptr) return;
            for (int n : notes) jo->servei.estudi().enviaDirecte ({ slot, (std::uint8_t) 0x80, (std::uint8_t) juce::jlimit (0, 127, n), (std::uint8_t) 0 });
        });
    };
    pistes.onImporta = [this] (const juce::StringArray& fitxers, int pista, double on) { importa (fitxers, pista, on); };
    pistes.onDemanaImportar = [this] (int pista, double on) { demanaImportar (pista, on); };
    pistes.onConsolida = [this] (int pista, int clipA, int clipB) { consolida (pista, clipA, clipB); };
    pistes.onConsolidaComp = [this] (int pista) { consolidaComp (pista); };   // 0.81
    pistes.onPrint = [this] (int pista) { printPista (pista); };              // 0.82
    pistes.onObreMidi = [this] (int pista, int clip) { obrePianoRoll (pista, clip); };
    pistes.onVistaMoguda = [this] { actualitzaBarres(); estat.avisa(); };   // 0.54
    pistes.onObreVeu = [this] (int pista, int clip) { obreVeu (pista, clip); };
    inserts::obreVeu() = [this] (int pista) { obreVeuDePista (pista); };
    // 0.56: automatització i biblioteca d'IR
    addChildComponent (editorAuto);
    plugins.sidechain = &serveiSc;
    editorAuto.onTanca = [this] { tancaAutomatitzacio(); };
    editorAuto.onTeclaNoUsada = [this] (const juce::KeyPress& k) { return keyPressed (k); };
    inserts::obreAutomatitzacio() = [this] (int pista, const std::string& idInsert)
    {
        obreAutomatitzacio (pista);
        juce::MessageManager::callAsync ([this, idInsert] { editorAuto.menuLinia (&editorAuto, idInsert); });
    };
    mibase::conv::posaArrel (dibuix::cami (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("IR")));
    informePlugins() = [this] { return plugins.informe (estat.projecte); };
    addChildComponent (editorVeu);
    editorVeu.onTanca = [this] { tancaVeu(); };
    editorVeu.onCommutaAB = [this] { commutaVeuAB(); };
    editorVeu.onDesfes = [this] (bool refer) { desfes (refer); };
    editorVeu.onSolo = [this] { commutaSoloEditor(); };
    // 0.26: samplejador
    addChildComponent (editorSampler);
    editorSampler.onTanca = [this] { tancaSampler(); };
    editorSampler.onCarrega = [this] { demanaSample(); };
    editorSampler.onClip = [this] (int p, int c) { clipAlSampler (p, c); };
    editorSampler.onABiblioteca = [this] { samplerABiblioteca(); };
    editorSampler.onFitxer = [this] (const juce::File& fitxerSample) { carregaSample (estat.sampler.pista, fitxerSample); };
    editorSampler.onNota = [this] (std::uint8_t e, std::uint8_t n, std::uint8_t v)
    {
        for (auto& t : estat.projecte.pistes)
            if (t.id == estat.sampler.pista) { servei.estudi().enviaDirecte ({ slotDePista (t), e, n, v }); return; }
    };
    editorSampler.onEscolta = [this] (double seg)
    {
        if (auto* sm = instruments.samplejador (estat.sampler.pista)) { if (seg >= 0.0) sm->escolta (seg); else sm->aturaEscolta(); }
    };
    editorSampler.posicioSo = [this]() -> double
    {
        auto* sm = instruments.samplejador (estat.sampler.pista);
        const auto* pr = sm != nullptr ? sm->preparada() : nullptr;
        if (pr == nullptr) return -1.0;
        const double pos = sm->posicioActual();
        if (pos < 0.0 || pr->m.fs <= 0.0) return -1.0;
        const double seg = pos / (pr->m.fs * pr->factorTemps);
        return pr->girada ? pr->duradaOriginal - seg : seg;
    };
    editorSampler.mostra = [this]() -> std::shared_ptr<const mibase::sampler::Mostra>
    {
        for (auto& t : estat.projecte.pistes)
            if (t.id == estat.sampler.pista && mibase::esSampler (t) && ! t.sampler.fitxer.empty())
                return Instruments::mostraSample (Instruments::fitxerSample (estat.carpeta, t.sampler));
        return nullptr;
    };
    editorSampler.onTeclaNoUsada = [this] (const juce::KeyPress& k) { return keyPressed (k); };
    {
        // 0.31: el banc General MIDI es baixa en segon pla: el progrés a la barra d'estat i, quan acaba, les pistes GM ja sonen.
        juce::Component::SafePointer<PantallaEstudi> jo (this);
        bancgm::enProgres() = [jo] (const juce::String& t) { if (jo == nullptr) return; jo->estat.info = t; jo->estat.avisa(); };
        bancgm::enBaixat() = [jo] { if (jo == nullptr) return; jo->historial.afegeix ("Banc General MIDI baixat"_ca); jo->publicaPla(); };
    }
    estat.sampler.altConsola = ajustos.getIntValue ("sampler.altConsola", 0);
    editorSampler.onAltConsola = [this] (int alt) { ajustos.setValue ("sampler.altConsola", alt); };
    editorSampler.onGrava = [this] { gravaSampler(); };
    editorSampler.entrades = [this] { return servei.getNomsEntrades(); };
    editorSampler.gravant = [this] { return estat.transport == 2 && gravacio.perSampler; };
    // 0.27: panell de la dreta amb pestanyes (clip / biblioteca)
    addAndMakeVisible (pestDreta);
    addChildComponent (biblioteca);
    addChildComponent (pool);   // 0.85
    dreta = juce::jlimit (0, 2, ajustos.getIntValue ("estudi.dretaPestanya", ajustos.getBoolValue ("estudi.dretaBiblioteca", false) ? 1 : 0));
    pestDreta.setTriat (dreta);
    pestDreta.onCanvi = [this] (int i) { dreta = i; ajustos.setValue ("estudi.dretaPestanya", dreta); colocaSeccions(); };
    pool.fitxerDe = [this] (const mibase::Presa& s) { return fitxerPresa (s); };
    pool.audioDe = [this] (const std::string& id) -> const mibase::AudioPresa*
    {
        auto it = audioPreses.find (id);
        return it != audioPreses.end() ? it->second.get() : nullptr;
    };
    pool.onEscolta = [this] (const std::string& id) { escoltaPresa (id); };
    pool.onLocalitza = [this] (const std::string& id) { localitzaPresa (id); };
    pool.onCanviaNom = [this] (const std::string& id) { canviaNomPresa (id); };
    pool.onBusca = [this] (const std::string& id) { buscaPresa (id); };
    pool.onTreu = [this] (const std::string& id) { aLaPaperera ({ id }); };
    pool.onTreuSobrants = [this]
    {
        const auto ids = ed::presesSenseUs (estat.projecte);
        if (ids.empty()) return;
        juce::Component::SafePointer<PantallaEstudi> jo (this);
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Treu els àudios que sobren"_ca,
            juce::String ((int) ids.size()) + " àudios no es fan servir enlloc (ni a la taula ni a les preses). Van a la paperera del projecte "
            "(carpeta Àudios\\Paperera) i es pot desfer amb Ctrl+Z."_ca, "Treu-los"_ca, "Cancel·la"_ca, nullptr,
            juce::ModalCallbackFunction::create ([jo, ids] (int r) { if (jo != nullptr && r == 1) jo->aLaPaperera (ids); }));
    };
    pool.onBuidaPaperera = [this] { buidaPaperera(); };
    pool.onImporta = [this] { importaAlPool(); };
    pistes.onPresa = [this] (const std::string& id, int fila, double t) { deixaPresa (id, fila, t); };
    escGlobal() = [this] { tancaTot(); };
    // 0.46: amb una finestra a part al davant (guitarra virtual, sinte, plugin…), les tecles del transport també van
    teclaGlobal() = [this] (const juce::KeyPress& k)
    {
        if (k.getModifiers().isAnyModifierKeyDown()) return false;
        const int c = k.getKeyCode();
        const bool transport = c == juce::KeyPress::spaceKey || c == 'L' || c == 'l' || esNumpad0 (k)
                            || c == juce::KeyPress::homeKey || c == juce::KeyPress::returnKey;
        return transport && keyPressed (k);
    };
    biblioteca.onInfo = [this] (const juce::String& t) { estat.info = t; estat.avisa(); };
    biblioteca.onEscolta = [this] (const juce::File& fs)
    {
        // 0.40: tornar a clicar el sample que està sonant l'atura (n'hi ha de molt llargs).
        const auto ara = juce::Time::getMillisecondCounter();
        if (fs == sampleEscoltat && ara < sampleEscoltatFins)
        {
            servei.escoltaSo (nullptr);
            sampleEscoltat = juce::File();
            return;
        }
        auto mo = kits::llegeix (fs, 30.0);
        if (mo == nullptr || mo->L.empty()) { biblioteca.marcaNoObre (fs); return; }   // 0.42: no es pot obrir: fora de la llista
        {
            sampleEscoltat = fs;
            sampleEscoltatFins = ara + (juce::uint32) (1000.0 * (double) mo->L.size() / juce::jmax (1.0, mo->fs));
            servei.escoltaSo (mo);
        }
    };
    biblioteca.onObreAlSampler = [this] (const juce::File& fs)
    {
        if (estat.sampler.obert) carregaSample (estat.sampler.pista, fs);
        else pistaSamplerNova (fs);
    };
    pistes.onSample = [this] (const juce::File& fs, int fila, double t) { deixaSample (fs, fila, t); };
    pistes.onEnviaSampler = [this] (int pista, int clip, bool nomesBib) { enviaAlSampler (pista, clip, nomesBib); };
    pistes.arrossegaFora = [this] (juce::Point<int> q) { return editorSampler.isShowing() && editorSampler.getScreenBounds().contains (q); };
    panell.onDuplica = [this] { duplica(); };
    panell.onTreu = [this] { treu(); };
    panell.onElastic = [this] (juce::Component& b) { menuElastic (b); };   // 0.86
    panell.onAcords = [this]   // 0.87
    {
        if (estat.triats.size() != 1) return;
        if (const auto* c = estat.clip (estat.triats[0]))
            if (const auto* s = ed::presa (estat.projecte, c->presa)) treuAcords (*c, dibuix::utf8 (s->nom));
    };
    pistes.onAcordsDeClip = [this] (int pista, int clip)   // 0.87: un clip arrossegat a la pista d'Acords
    {
        if (const auto* c = estat.clip ({ pista, clip }))
            if (const auto* s = ed::presa (estat.projecte, c->presa)) treuAcords (*c, dibuix::utf8 (s->nom));
    };

    // Carpetes de pistes (0.11).
    addChildComponent (carpetes);
    addChildComponent (taulaEstructura);
    taulaEstructura.onTram = [this] { return servei.estudi().getTramRuta(); };
    addChildComponent (divCarpetes);
    estat.panellCarpetes = ajustos.getBoolValue ("estudi.panellCarpetes", false);
    ampleCarpetes = juce::jlimit (130, 400, ajustos.getIntValue ("estudi.ampleCarpetes", 190));
    btCarpetes.setLlum (vintage::ledGroc);
    btCarpetes.setTooltip ("Carpetes de pistes: mostra o amaga el panell per agrupar les pistes"_ca);
    btCarpetes.onClick = [this]
    {
        estat.panellCarpetes = ! estat.panellCarpetes;
        ajustos.setValue ("estudi.panellCarpetes", estat.panellCarpetes);
        colocaSeccions();
    };
    addAndMakeVisible (btCarpetes);
    carpetes.onCarpetaNova = [this] (std::vector<int> v) { demanaNomCarpeta (std::move (v), {}); };
    carpetes.onCanviaNom = [this] (std::string id) { demanaNomCarpeta ({}, id); };
    divCarpetes.setTooltip ("Arrossega per fer més ample o més estret. Doble clic: mida inicial."_ca);
    divCarpetes.onComenca = [this] { ampleInici = carpetes.getWidth(); };
    divCarpetes.onArrossega = [this] (int dx) { ampleCarpetes = juce::jlimit (130, 400, ampleInici + dx); colocaSeccions(); };
    divCarpetes.onDobleClic = [this] { ampleCarpetes = 190; colocaSeccions(); divCarpetes.onAcaba(); };
    divCarpetes.onAcaba = [this] { ajustos.setValue ("estudi.ampleCarpetes", ampleCarpetes); ajustos.saveIfNeeded(); };

    for (auto* d : { &divEsquerra, &divNoms, &divPanell })
    {
        addAndMakeVisible (d);
        d->setTooltip ("Arrossega per fer més ample o més estret. Doble clic: mida inicial."_ca);
        d->onAcaba = [this]
        {
            ajustos.setValue ("estudi.ampleEsquerra", ampleEsquerra);
            ajustos.setValue ("estudi.ampleEsquerraEditor", ampleEsquerraEditor);
            ajustos.setValue ("estudi.ampleNoms3", ampleNoms);
            ajustos.setValue ("estudi.amplePanell", amplePanell);
            ajustos.saveIfNeeded();
        };
    }
    divEsquerra.onComenca = [this] { ampleInici = esquerra.getWidth(); };
    divEsquerra.onArrossega = [this] (int dx)
    {
        if (esquerra.ambEditor()) ampleEsquerraEditor = juce::jlimit (230, 420, ampleInici + dx);
        else ampleEsquerra = juce::jlimit (110, 260, ampleInici + dx);
        colocaSeccions();
    };
    divEsquerra.onDobleClic = [this] { if (esquerra.ambEditor()) ampleEsquerraEditor = 270; else ampleEsquerra = 132; colocaSeccions(); divEsquerra.onAcaba(); };
    divNoms.onComenca = [this] { ampleInici = capcaleres.getWidth(); };
    divNoms.onArrossega = [this] (int dx) { ampleNoms = juce::jmax (120, ampleInici + dx); colocaSeccions(); };
    divNoms.onDobleClic = [this] { ampleNoms = 244; colocaSeccions(); divNoms.onAcaba(); };
    divPanell.onComenca = [this] { ampleInici = panell.getWidth(); };
    divPanell.onArrossega = [this] (int dx) { amplePanell = juce::jmax (200, ampleInici - dx); colocaSeccions(); };
    divPanell.onDobleClic = [this] { amplePanell = 270; colocaSeccions(); divPanell.onAcaba(); };

    // Piano Roll: a baix (amb un separador per fer-lo més alt) o en una finestra a part.
    addChildComponent (zonaEditors);
    esquerra.setPanellEditor (&panellEditor);
    esquerra.opcions.onObreKits = [this] { obreKits(); };
    esquerra.opcions.onObreSampler = [this] { obreSampler (estat.pistaTriada); };
    // 0.88: el SUB & BASS (finestra, teclat de pantalla, automatització i desar els ajustos)
    esquerra.opcions.sinte = [this] (const std::string& id)
    {
        if (auto s = instruments.sinte (id)) return s;
        publicaPla();   // la pista l'acaba d'agafar: que el pla el creï
        return instruments.sinte (id);
    };
    esquerra.opcions.onDirecte = [this] (const std::string& id, std::uint8_t e, std::uint8_t d1, std::uint8_t d2)
    {
        if (auto* t = estat.pista (mibase::edicio::pistaPerId (estat.projecte, id))) servei.estudi().enviaDirecte ({ slotDePista (*t), e, d1, d2 });
    };
    panellHarmonia.onObreSubBass = [this] (const std::string& id) { esquerra.opcions.obreSubBass (id); };
    plugins.instrumentIntern = [this] (const std::string& id) -> mibase::Parametritzable* { return instruments.sinte (id).get(); };
    plugins.desaInterns = [this] (mibase::Projecte& p) { return instruments.desaEstats (p); };
    panellHarmonia.onObreSampler = [this]
    {
        if (auto* pr = zonaEditors.actiu()) obreSampler (pr->pistaDeLEditor());
    };
    panellBateria.onObreKits = [this] { obreKits(); };
    panellHarmonia.onObreKits = [this] { obreKits(); };
    capcaleres.onAutomatitzacio = [this] (int pista) { obreAutomatitzacio (pista); };
    capcaleres.onConsolidaComp = [this] (int pista) { consolidaComp (pista); };   // 0.81
    capcaleres.onPrint = [this] (int pista) { printPista (pista); };              // 0.82
    capcaleres.onObreInstrument = [this] (int fila)
    {
        auto* t = estat.pista (fila);
        if (t == nullptr) return;
        if (mibase::esKit (*t)) obreKits();
        else if (mibase::esSampler (*t)) obreSampler (fila);
        else esquerra.opcions.obreInstrument();   // la pista ja és la triada
    };
    esquerra.opcions.teclats = [this] { return servei.getTeclatsMidi(); };
    capcaleres.teclats = [this] { return servei.getTeclatsMidi(); };
    esquerra.opcions.setPlugins (&plugins);
    mescles.plugins = &plugins;
    esquerra.opcions.sortides = [this]
    {
        juce::StringArray noms;
        if (auto* d = servei.dispositius().getCurrentAudioDevice())
        {
            const auto tots = d->getOutputChannelNames();
            const auto actives = d->getActiveOutputChannels();
            for (int i = 0; i < tots.size(); ++i)   // el motor rep només les actives, en ordre
                if (actives[i]) noms.add (tots[i]);
        }
        return noms;
    };
    focusEditor.f = [this] { if (! estat.focusEditor) { estat.focusEditor = true; estat.avisa(); } };
    focusPistes.f = [this] { if (estat.focusEditor && estat.pianoRoll) { estat.focusEditor = false; estat.avisa(); } };
    // 0.27: el teclat (Z, Maj+Z, M…) actua on és el ratolí, sense haver de clicar
    focusEditor.entra = [this] (juce::Component* c)
    {
        if (! potAgafarFocus() || c == nullptr) return;
        juce::Component* ed = dynamic_cast<PianoRoll*> (c);
        if (ed == nullptr) ed = c->findParentComponentOfClass<PianoRoll>();
        if (ed == nullptr) ed = dynamic_cast<EditorVeu*> (c) != nullptr ? c : c->findParentComponentOfClass<EditorVeu>();
        if (ed == nullptr) ed = dynamic_cast<EditorSampler*> (c) != nullptr ? c : c->findParentComponentOfClass<EditorSampler>();
        if (ed != nullptr && ! ed->hasKeyboardFocus (true)) ed->grabKeyboardFocus();
        // 0.40: el panell de l'esquerra no canvia només de passar-hi el ratolí (era massa aleatori): cal clicar.
    };
    focusPistes.entra = [this] (juce::Component*)
    {
        if (! potAgafarFocus()) return;
        if (! pistes.hasKeyboardFocus (true)) pistes.grabKeyboardFocus();
    };
    editorVeu.addMouseListener (&focusEditor, true);
    editorSampler.addMouseListener (&focusEditor, true);
    zonaEditors.addMouseListener (&focusEditor, true);
    pistes.addMouseListener (&focusPistes, true);
    capcaleres.addMouseListener (&focusPistes, true);
    ampleEsquerraEditor = ajustos.getIntValue ("estudi.ampleEsquerraEditor", 270);
    addChildComponent (divPianoRoll);
    altPianoRoll = ajustos.getIntValue ("estudi.altPianoRoll", 430);
    divPianoRoll.setTooltip ("Arrossega per fer el Piano Roll més alt o més baix. Doble clic: mida inicial."_ca);
    divPianoRoll.onComenca = [this] { altInici = estat.veu.obert ? editorVeu.getHeight() : estat.sampler.obert ? editorSampler.getHeight()
                                                 : editorAuto.isVisible() ? editorAuto.getHeight() : zonaEditors.getHeight(); };
    divPianoRoll.onArrossega = [this] (int dy)
    {
        altPianoRoll = juce::jmax (160, altInici - dy - (estat.veu.obert || estat.sampler.obert ? 0 : (juce::jmax (1, zonaEditors.quants()) - 1) * 250));
        colocaSeccions();
    };
    divPianoRoll.onDobleClic = [this] { altPianoRoll = 430; colocaSeccions(); divPianoRoll.onAcaba(); };
    divPianoRoll.onAcaba = [this] { ajustos.setValue ("estudi.altPianoRoll", altPianoRoll); ajustos.saveIfNeeded(); };

    for (auto* b : { &barraV, &barraH })
    {
        addAndMakeVisible (b);
        b->addListener (this);
        b->setAutoHide (false);
        b->setColour (juce::ScrollBar::thumbColourId, tema::linia2);
    }

    ultimaCopia = 0;   // el primer desament de la sessió fa còpia; després, com a molt una cada 10 minuts
    triaEina (Eina::Fletxa);
    if (progresGlobal()) progresGlobal() ("Obrint els plugins i els instruments…"_ca, 0.92);
    changeListenerCallback (nullptr);
}

PantallaEstudi::~PantallaEstudi()
{
    inserts::obreVeu() = nullptr;
    inserts::obreAutomatitzacio() = nullptr;
    escGlobal() = nullptr;
    teclaGlobal() = nullptr;
    informePlugins() = nullptr;
    rellotge.stopTimer();
    zonaEditors.removeMouseListener (&focusEditor);
    editorVeu.removeMouseListener (&focusEditor);
    editorSampler.removeMouseListener (&focusEditor);
    pistes.removeMouseListener (&focusPistes);
    capcaleres.removeMouseListener (&focusPistes);
    if (finestraPianoRoll != nullptr)
    {
        ajustos.setValue ("pianoRoll.finestra", finestraPianoRoll->getWindowStateAsString());
        finestraPianoRoll->clearContentComponent();
        finestraPianoRoll.reset();
    }
    auto& motor = servei.estudi();
    motor.setSlotTeclat (-1);
    motor.setSlotGravaMidi (-1);
    if (estat.transport != 0 || motor.getEstat() != MotorEstudi::Estat::Aturat)
    {
        motor.atura();
        for (int i = 0; i < 50 && motor.getEstat() != MotorEstudi::Estat::Aturat; ++i)
            juce::Thread::sleep (10);
    }
    if (motor.gravadora().activa())
        acabaGravacio();
    if (gravacio.midi && gravacio.pista >= 0)
        acabaGravacioMidi();
    motor.publica (std::make_unique<Pla>());   // ja no ha de sonar res d'aquest projecte
    previsualitzaClic (false);
    estat.removeChangeListener (this);
    if (plugins.desaEstats (estat.projecte)) calDesar = true;   // 0.19: ajustos dels plugins
    if (calDesar) desaAra();
    ajustos.setValue ("estudi.zoom", estat.pxPerSegon);
    ajustos.setValue ("estudi.altPista", estat.altPista);
    ajustos.saveIfNeeded();
}

void PantallaEstudi::desaAra()
{
    stopTimer();
    const auto ara = juce::Time::currentTimeMillis();
    const bool ferCopia = ara - ultimaCopia > 10 * 60 * 1000;
    plugins.desaEstats (estat.projecte);
    const auto r = disc::desa (dibuix::cami (estat.fitxer), estat.projecte, dibuix::araIso().toStdString(),
                               dibuix::araPerNom().toStdString(), ferCopia);
    if (r.correcte)
    {
        calDesar = false;
        if (ferCopia) ultimaCopia = ara;
        if (auto llegit = disc::llegeix (dibuix::cami (estat.fitxer)))
            estat.projecte.jsonOriginal = llegit->jsonOriginal;
    }
    else
    {
        estat.info = "No s'ha pogut desar: "_ca + dibuix::utf8 (r.error);
        historial.afegeix ("No s'ha pogut desar el projecte: "_ca + dibuix::utf8 (r.error));
        estat.avisa();
    }
}

namespace
{
    /** 0.73: la tonalitat i els acords de la pista d'Acords, per a l'Harmonitzador del Vocal Studio. */
    void publicaContextMusical (const Projecte& p)
    {
        auto& cm = ContextMusical::global();
        const auto t = harmonia::tonalitatEfectiva (p);
        int mascara = 0;
        for (int pc : harmonia::notesEscala (t)) mascara |= 1 << (((pc - t.to) % 12 + 12) % 12);
        cm.tonica.store (t.to);
        if (mascara != 0) cm.mascaraEscala.store (mascara);
        std::vector<ContextMusical::Tram> trams;
        for (const auto& pista : p.pistes)
            if (esAcords (pista))
                for (const auto& c : pista.clips)
                {
                    if (! c.esAcord() || c.mut) continue;
                    int m = 0;
                    for (int n : harmonia::notesPad (harmonia::llegeixAcord (c.acord))) m |= 1 << (((n % 12) + 12) % 12);
                    if (m != 0) trams.push_back ({ c.iniciVisible(), c.fiVisible(), m, -1 });
                }
        std::sort (trams.begin(), trams.end(), [] (const auto& a, const auto& b) { return a.de < b.de; });
        static std::vector<std::array<double, 3>> abans;
        std::vector<std::array<double, 3>> ara;
        for (const auto& x : trams) ara.push_back ({ x.de, x.a, (double) x.mascara });
        if (ara != abans) { cm.publicaAcords (trams); abans = std::move (ara); }
    }
}

void PantallaEstudi::changeListenerCallback (juce::ChangeBroadcaster*)
{
    revisaArmament();   // 0.72
    publicaContextMusical (estat.projecte);   // 0.73
    // Els editors només canvien de clip amb doble clic (obrePianoRoll), no en triar un clip (0.10).
    // El teclat MIDI de fora toca la pista triada (si és MIDI) o la que grava.
    {
        int slot = -1;
        if (gravacio.midi && gravacio.pista >= 0) { if (auto* t = estat.pista (gravacio.pista)) slot = slotDePista (*t); }
        else if (auto* t = estat.pista (estat.pistaTriada); t != nullptr && mibase::esMidi (*t)) slot = slotDePista (*t);
        servei.estudi().setSlotTeclat (slot);
        // Teclats assignats a pistes concretes; i el de la pista triada (si en té un d'assignat, només aquell la toca).
        std::vector<std::pair<juce::String, int>> mapa;
        for (auto& t : estat.projecte.pistes)
            if (mibase::esMidi (t) && ! t.entradaMidi.empty()) mapa.push_back ({ juce::String (t.entradaMidi), slotDePista (t) });
        juce::String teclatTriada;
        if (auto* t = estat.pista (estat.pistaTriada); t != nullptr && mibase::esMidi (*t)) teclatTriada = juce::String (t->entradaMidi);
        servei.setMapaTeclats (mapa, teclatTriada);

        // Chord Pad (0.10): el de l'editor actiu (o el primer que el tingui obert) fa que el teclat de la seva pista toqui acords.
        int slotPads = -1;
        std::array<std::vector<int>, 12> acords;
        for (int k = -1; k < zonaEditors.quants() && slotPads < 0; ++k)
        {
            auto* pr = zonaEditors.editor (k < 0 ? estat.editorActiu : k);
            if (pr == nullptr || ! pr->chordPadActiu()) continue;
            if (auto* t = estat.pista (pr->pistaDeLEditor())) { slotPads = slotDePista (*t); acords = pr->acordsTeclat(); }
        }
        servei.setChordPadTeclat (slotPads, acords);

        // 0.20: bateria amb els pads per ordre de files (la triada, o la que grava).
        {
            int slotBat = -1;
            std::vector<int> notesFiles;
            const auto* tb = gravacio.midi && gravacio.pista >= 0 ? estat.pista (gravacio.pista) : estat.pista (estat.pistaTriada);
            if (tb != nullptr && mibase::esKit (*tb) && tb->padsPerOrdre)
            {
                slotBat = slotDePista (*tb);
                for (auto& f : tb->files) notesFiles.push_back (f.nota);
            }
            servei.setMapaBateria (slotBat, notesFiles);
        }

        // 0.17: el teclat respecta l'escala (assistit o bloquejat).
        {
            const auto t = harmonia::tonalitatEfectiva (estat.projecte);
            std::array<bool, 12> esc {};
            for (int n = 0; n < 12; ++n) esc[(size_t) n] = harmonia::aEscala (n, t);
            // 0.20: una pista que toca lliure (o la bateria) no fa cas de l'escala.
            const auto* tt = estat.pista (estat.pistaTriada);
            const bool lliure = tt != nullptr && (! tt->escalaTeclat || mibase::esKit (*tt));
            servei.setEscalaTeclat (lliure ? 0 : (int) (tt != nullptr ? mibase::modeEscalaDe (estat.projecte, *tt) : estat.projecte.modeEscala), esc);
        }
    }
    {
        const bool volVisible = estat.pianoRoll && finestraPianoRoll == nullptr && ! estat.foraTreball();
        if (volVisible != zonaEditors.isVisible() && finestraPianoRoll == nullptr) colocaSeccions();
        else if (esquerra.ambEditor() != (esquerra.getWidth() == ampleEsquerraEditor)) colocaSeccions();
    }

    const auto& p = estat.projecte;
    btTempo.setButtonText (juce::String ((int) std::round (p.tempo)) + " bpm · "_ca + juce::String (p.compasNum) + "/" + juce::String (p.compasDen));
    btClic.setToggleState (p.clic, juce::dontSendNotification);
    btRequadre.setToggleState (estat.modeRang, juce::dontSendNotification);
    btRequadre.setTooltip (estat.modeRang ? "Requadre: tria només el que queda dins (encès). Clica per triar clips sencers."_ca
                                          : "Requadre: tria els clips sencers que toca (apagat). Clica per triar només el que queda dins."_ca);
    btInsereix.setToggleState (estat.insereix, juce::dontSendNotification);
    btInsereix.setTooltip (estat.insereix ? "Enganxar inserint: aparta el que hi ha després del capçal (encès). Tecla Insert."_ca
                                          : "Enganxar trepitjant el que hi ha (apagat). Clica o Insert per inserir."_ca);
    if (! estat.triats.empty())
    {
        if (auto* c = estat.clip (estat.triats[0])) btColor.setColor (estat.colorClip (estat.triats[0].pista, *c));
    }
    else if (auto* t = estat.pista (estat.pistaTriada))
        btColor.setColor (EstatEstudi::colorPista (*t));
    btDesfes.setEnabled (estat.desfes.potDesfer());
    btRefes.setEnabled (estat.desfes.potRefer());
    for (int i = 0; i < eines.size(); ++i)
        eines[i]->setToggleState ((int) estat.eina == i, juce::dontSendNotification);
    if (snap.getTriat() != (int) p.snap) snap.setTriat ((int) p.snap);
    {
        juce::String nom = "1/4";
        for (auto& v : midi::valorsGraella()) if (v.ticks == p.graella) nom = juce::String::fromUTF8 (v.nom);
        if (btGraella.getButtonText() != nom) btGraella.setButtonText (nom);
    }

    if (mescles.isVisible() != estat.vistaMescles || taulaEstructura.isVisible() != estat.vistaEstructura)
        colocaSeccions();
    actualitzaBarres();
    repaint (zonaBarra);
    sincronitzaVeu();
    sincronitzaElastic();   // 0.86
    seguixSoloEditor();
    publicaPla();
}

void PantallaEstudi::triaEina (Eina e)
{
    estat.eina = e;
    // 0.31: la fletxa, el llapis i la goma també valen per al Piano Roll i el Drumpad oberts
    if ((int) e <= 2)
        for (int i = 0; i < zonaEditors.quants(); ++i)
            if (auto* pr = zonaEditors.editor (i)) pr->posaEina ((int) e);
    if (e != Eina::Fletxa && (estat.triats.size() > 1 || estat.rang.actiu)) estat.capTriat();
    estat.avisa();
}

//==============================================================================
void PantallaEstudi::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);
    vintage::panell (g, zonaBarra.toFloat());

    // Calaix de la caixa d'eines.
    auto calaix = zonaEines.toFloat().expanded (5.0f, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (calaix, 7.0f);

    if (! zonaInfo.isEmpty())
    {
        g.setColour (estat.info.isNotEmpty() ? tema::accent : tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        const auto text = estat.info.isNotEmpty() ? estat.info
                        : juce::String (estat.numPistes()) + (estat.numPistes() == 1 ? " pista" : " pistes")
                          + " · Espai: play/atura · 0 (teclat numèric): grava · Ctrl+roda: zoom · Alt+roda: alçada"_ca;
        g.drawFittedText (text, zonaInfo, juce::Justification::centredLeft, 1, 0.8f);
    }

    if (! estat.foraTreball())
    {
        auto titol = [&] (juce::Rectangle<int> r, const juce::String& t)
        {
            vintage::panell (g, r.toFloat());
            g.setColour (tema::linia);
            g.fillRect (r.getRight() - 1, r.getY(), 1, r.getHeight());
            dibuix::pintaEtiquetaSeccio (g, t, r.withTrimmedLeft (14));
        };
        titol (zonaTitolNoms, "Pistes");
    }
}

void PantallaEstudi::colocaSeccions()
{
    auto r = getLocalBounds().withTrimmedTop (alcadaBarra);
    const bool m = estat.foraTreball();

    const bool ambEditor = estat.pianoRoll && ! m && ! esquerra.acordsTriada();   // amb un editor obert, la columna esquerra té el panell d'harmonia (0.37: no amb la pista d'Acords triada)
    int esq = ambEditor ? ampleEsquerraEditor : ampleEsquerra, noms = ampleNoms, pan = amplePanell;
    const bool ambCarpetes = estat.panellCarpetes && ! m;
    int carp = ambCarpetes ? ampleCarpetes : 0;
    int sobra = esq + (m ? 0 : noms + carp) + pan + 320 - r.getWidth();
    auto encongeix = [&sobra] (int& v, int minim) { if (sobra > 0) { const int t = juce::jmin (sobra, v - minim); if (t > 0) { v -= t; sobra -= t; } } };
    encongeix (pan, 200);
    if (! m) encongeix (noms, 140);
    encongeix (esq, ambEditor ? 230 : 110);
    if (ambCarpetes) encongeix (carp, 120);

    esquerra.setBounds (r.removeFromLeft (esq));
    {
        auto zDreta = r.removeFromRight (pan);
        pestDreta.setBounds (zDreta.removeFromTop (34).reduced (8, 4));
        panell.setBounds (zDreta);
        biblioteca.setBounds (zDreta);
        pool.setBounds (zDreta);
    }
    // 0.26: el samplejador, a baix (sense tocar el panell de la dreta, on anirà la biblioteca)
    const bool samplerBaix = estat.sampler.obert && ! m && ! estat.veu.obert;
    editorSampler.setVisible (samplerBaix);
    if (samplerBaix)
    {
        const int alt = juce::jlimit (330, juce::jmax (330, r.getHeight() - 140), juce::jmax (altPianoRoll, 470));
        auto zona = r.removeFromBottom (alt);
        editorSampler.setBounds (zona.withTrimmedTop (4));
        divPianoRoll.setBounds (zona.getX(), zona.getY() - 3, zona.getWidth(), 8);
    }

    // Piano Roll a baix (si és obert i no és en una finestra a part).
    const bool veuBaix = estat.veu.obert && ! m;   // 0.22: o el Vocal Editor, al mateix lloc
    // 0.56: o l'editor d'automatització
    const bool autoBaix = estat.automat.obert && ! m && ! veuBaix && ! samplerBaix && editorAuto.getParentComponent() != nullptr;
    editorAuto.setVisible (autoBaix);
    if (autoBaix)
    {
        const int alt = juce::jlimit (200, juce::jmax (200, r.getHeight() - 140), juce::jmax (altPianoRoll, 300));
        auto zona = r.removeFromBottom (alt);
        editorAuto.setBounds (zona.withTrimmedTop (4));
        divPianoRoll.setBounds (zona.getX(), zona.getY() - 3, zona.getWidth(), 8);
    }
    const bool prBaix = estat.pianoRoll && finestraPianoRoll == nullptr && ! m && ! veuBaix && ! samplerBaix && ! autoBaix;
    divPianoRoll.setVisible (prBaix || veuBaix || samplerBaix || autoBaix);
    if (finestraPianoRoll == nullptr) zonaEditors.setVisible (prBaix);
    editorVeu.setVisible (veuBaix);
    if (veuBaix)
    {
        const int alt = juce::jlimit (300, juce::jmax (300, r.getHeight() - 140), juce::jmax (altPianoRoll, 380));
        auto zona = r.removeFromBottom (alt);
        editorVeu.setBounds (zona.withTrimmedTop (4));
        divPianoRoll.setBounds (zona.getX(), zona.getY() - 3, zona.getWidth(), 8);
    }
    if (prBaix)
    {
        // Amb diversos editors, la zona creix (cada un ha de tenir lloc per treballar); les pistes en conserven una mica.
        const int n = juce::jmax (1, zonaEditors.quants());
        const int alt = juce::jlimit (160, juce::jmax (160, r.getHeight() - 140), altPianoRoll + (n - 1) * 250);
        auto zona = r.removeFromBottom (alt);
        zonaEditors.setBounds (zona.withTrimmedTop (4));
        divPianoRoll.setBounds (zona.getX(), zona.getY() - 3, zona.getWidth(), 8);
    }
    const int baixSeccions = r.getBottom();

    mescles.setVisible (estat.vistaMescles);
    taulaEstructura.setVisible (estat.vistaEstructura);
    panell.setVisible (! estat.vistaEstructura && dreta == 0);
    biblioteca.setVisible (! estat.vistaEstructura && dreta == 1);
    pool.setVisible (! estat.vistaEstructura && dreta == 2);
    pestDreta.setVisible (! estat.vistaEstructura);
    for (auto* c : std::initializer_list<juce::Component*> { &capcaleres, &regla, &pistes, &barraH, &barraV, &divNoms, &btCarpetes })
        c->setVisible (! m);
    carpetes.setVisible (ambCarpetes);
    divCarpetes.setVisible (ambCarpetes);
    btCarpetes.setToggleState (estat.panellCarpetes, juce::dontSendNotification);

    if (m)
    {
        mescles.setBounds (r);
        // La taula d'estructura també ocupa el lloc del panell d'info (hi van les parts per arrossegar).
        taulaEstructura.ampleDreta = panell.getWidth();
        taulaEstructura.setBounds (r.withRight (panell.getRight()));
        zonaTitolNoms = {};
    }
    else
    {
        if (ambCarpetes)
        {
            carpetes.setBounds (r.removeFromLeft (carp).withTrimmedBottom (gruixBarres));
            divCarpetes.setBounds (carpetes.getRight() - 4, alcadaBarra, 8, baixSeccions - alcadaBarra);
        }
        auto colNoms = r.removeFromLeft (noms);
        auto temps = r;
        zonaTitolNoms = colNoms.removeFromTop (alcadaRegla);
        btCarpetes.setBounds (zonaTitolNoms.getRight() - 34, zonaTitolNoms.getY() + 1, 30, zonaTitolNoms.getHeight() - 2);
        colNoms.removeFromBottom (gruixBarres);
        capcaleres.setBounds (colNoms);
        regla.setBounds (temps.removeFromTop (alcadaRegla));
        barraH.setBounds (temps.removeFromBottom (gruixBarres).withTrimmedRight (gruixBarres));
        barraV.setBounds (temps.removeFromRight (gruixBarres));
        pistes.setBounds (temps);
        divNoms.setBounds (capcaleres.getRight() - 4, alcadaBarra, 8, baixSeccions - alcadaBarra);
    }

    divEsquerra.setBounds (esquerra.getRight() - 4, alcadaBarra, 8, getHeight() - alcadaBarra);
    divPanell.setBounds (panell.getX() - 4, alcadaBarra, 8, getHeight() - alcadaBarra);
    for (auto* d : { &divEsquerra, &divNoms, &divPanell, &divPianoRoll, &divCarpetes }) d->toFront (false);

    actualitzaBarres();
    repaint();
}

void PantallaEstudi::resized()
{
    zonaBarra = getLocalBounds().removeFromTop (alcadaBarra);
    auto b = zonaBarra.reduced (14, 8);

    zonaEines = b.removeFromLeft (7 * 40 + 6 * 3);
    {
        auto e = zonaEines;
        for (auto* t : eines)
        {
            t->setBounds (e.removeFromLeft (40));
            e.removeFromLeft (3);
        }
    }
    b.removeFromLeft (20);
    btClic.setBounds (b.removeFromLeft (44));
    b.removeFromLeft (2);
    btClicMenu.setBounds (b.removeFromLeft (26));
    b.removeFromLeft (6);
    btTempo.setBounds (b.removeFromLeft (btTempo.ampladaIdeal() + 10));
    b.removeFromLeft (16);
    snap.setBounds (b.removeFromLeft (snap.ampladaIdeal() + 10));
    b.removeFromLeft (4);
    btGraella.setBounds (b.removeFromLeft (58));
    b.removeFromLeft (4);
    btRequadre.setBounds (b.removeFromLeft (40));
    b.removeFromLeft (3);
    btInsereix.setBounds (b.removeFromLeft (40));
    b.removeFromLeft (16);
    btColor.setBounds (b.removeFromLeft (44));
    b.removeFromLeft (16);
    btDesfes.setBounds (b.removeFromLeft (40));
    b.removeFromLeft (3);
    btRefes.setBounds (b.removeFromLeft (40));
    b.removeFromLeft (16);
    zonaInfo = b;

    colocaSeccions();
}

//==============================================================================
void PantallaEstudi::actualitzaBarres()
{
    if (pistes.getWidth() <= 0) return;
    const double visibles = pistes.getWidth() / estat.pxPerSegon;
    const double total = juce::jmax (ed::final (estat.projecte) + 30.0, estat.iniciVista + visibles, 120.0);
    barraH.setRangeLimits (0.0, total, juce::dontSendNotification);
    barraH.setCurrentRange (estat.iniciVista, visibles, juce::dontSendNotification);

    const int contingut = estat.altTotal() + estat.altPista + 60;
    const int maxim = juce::jmax (0, contingut - pistes.getHeight());
    if (estat.desplY > maxim) estat.desplY = maxim;
    barraV.setRangeLimits (0.0, juce::jmax (contingut, pistes.getHeight()), juce::dontSendNotification);
    barraV.setCurrentRange (estat.desplY, pistes.getHeight(), juce::dontSendNotification);
}

void PantallaEstudi::scrollBarMoved (juce::ScrollBar* barra, double inici)
{
    if (barra == &barraH) estat.iniciVista = juce::jmax (0.0, inici);
    else estat.desplY = juce::jmax (0, (int) inici);
    estat.avisa();
}

void PantallaEstudi::roda (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaY) >= std::abs (w.deltaX) ? w.deltaY : w.deltaX;
    if (std::abs (d) < 1.0e-6f) return;

    if (e.mods.isCommandDown())
    {
        // Zoom del temps a partir del punt del ratolí, que després queda centrat a la pantalla.
        // Si el ratolí no s'ha mogut des de l'últim zoom, es continua fent zoom sobre el mateix punt.
        const auto ara = juce::Time::currentTimeMillis();
        if (e.getScreenPosition() != posZoom || ara - tempsZoom > 2000)
            anclaZoom = estat.t ((float) e.getEventRelativeTo (&pistes).x);
        posZoom = e.getScreenPosition();
        tempsZoom = ara;
        estat.pxPerSegon = juce::jlimit (2.0, 800000.0, estat.pxPerSegon * (d > 0.0f ? 1.35 : 1.0 / 1.35));   // cada pas de roda, el mateix
        const double meitat = (pistes.getWidth() / 2.0) / estat.pxPerSegon;
        estat.iniciVista = juce::jmax (0.0, anclaZoom - meitat);   // mai abans del començament
    }
    else if (e.mods.isAltDown())
    {
        estat.altPista = juce::jlimit (VistaPistes::alcadaMinima, 260, estat.altPista + juce::roundToInt (d * 60.0f));
    }
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        estat.iniciVista = juce::jmax (0.0, estat.iniciVista - d * 300.0 / estat.pxPerSegon);
    }
    else
    {
        estat.desplY = juce::jmax (0, estat.desplY - juce::roundToInt (d * 200.0f));
    }
    actualitzaBarres();
    estat.avisa();
}

//==============================================================================
void PantallaEstudi::menuClaqueta()
{
    auto panellClic = std::make_unique<PanellClic> (estat.projecte.clic, estat.projecte.clicVolum, estat.projecte.clicSo,
        [this] (bool encesa, double volum, int so)
        {
            estat.projecte.clic = encesa;
            estat.projecte.clicVolum = volum;
            estat.projecte.clicSo = so;
            auto& m = servei.motor();
            m.volumClic = (float) volum;
            m.soClic = so;
            estat.avisa();
            calDesar = true;
            startTimer (700);
        },
        [this] (bool escolta) { previsualitzaClic (escolta); });
    juce::CallOutBox::launchAsynchronously (std::move (panellClic), btClicMenu.getScreenBounds(), nullptr);
}

void PantallaEstudi::previsualitzaClic (bool encesa)
{
    auto& m = servei.motor();
    m.tempoClic = estat.projecte.tempo;
    m.soClic = estat.projecte.clicSo;
    m.volumClic = (float) estat.projecte.clicVolum;
    m.tempsPerCompasClic = estat.projecte.compasNum;
    m.clicActiu = encesa;
}

//==============================================================================
void PantallaEstudi::transport (int accio)
{
    if (accio == 3) { atura(); return; }
    if (accio == 4) { if (estat.transport == 0) reprodueix(); return; }
    if (accio == 5) { gravaContextual(); return; }   // 0.72: el botó fa el mateix que el 0 del teclat numèric
    if (estat.transport >= 2) return;   // mentre es grava, el capçal no es toca

    double t = estat.capcal;
    const double c = ed::compasDeSegons (estat.projecte, t);   // 0.28: amb el mapa de tempo
    switch (accio)
    {
        case 0: t = 0.0; estat.iniciVista = 0.0; break;
        case 1: t = ed::segonsDeCompas (estat.projecte, juce::jmax (0.0, std::ceil (c - 1e-6) - 1.0)); break;
        case 2: t = ed::segonsDeCompas (estat.projecte, std::floor (c + 1e-6) + 1.0); break;
        default: break;
    }
    estat.mouCapcal (t, true);
    // Que el capçal es vegi.
    const double visibles = pistes.getWidth() / estat.pxPerSegon;
    if (estat.capcal < estat.iniciVista || estat.capcal > estat.iniciVista + visibles * 0.9)
        estat.iniciVista = juce::jmax (0.0, estat.capcal - visibles * 0.25);
    seguiment.reinicia();          // 0.54: no és moure la vista a mà
    zonaEditors.reiniciaSeguiment();
    actualitzaBarres();
    estat.avisa();
}

//==============================================================================
double PantallaEstudi::segonsPerCentrar() const
{
    // 0.54: el capçal va de la vora al mig en uns 4 compassos
    return juce::jlimit (2.0, 20.0, 4.0 * (double) mibase::midi::ticksPerCompas (estat.projecte) * mibase::midi::segonsPerTick (estat.projecte));
}

void PantallaEstudi::publicaPla()
{
    instruments.neteja (estat.projecte);
    instruments.carpeta = estat.carpeta;
    plugins.neteja (estat.projecte);
    RellotgePlugins::global().tempo.store (estat.projecte.tempo > 1.0 ? estat.projecte.tempo : 120.0);
    mibase::subbass::tempo().store (estat.projecte.tempo > 1.0 ? estat.projecte.tempo : 120.0);   // 0.88: LFO amb el tempo
    RellotgePlugins::global().num.store (estat.projecte.compasNum);
    RellotgePlugins::global().den.store (estat.projecte.compasDen);
    const double fs = servei.estudi().getFs();
    auto interns = instruments.proveidor (fs, 2048);
    const auto errorAbans = plugins.getUltimError();
    servei.estudi().publica (construeixPla (estat.projecte, audioPreses,
        [this, fs, interns] (const mibase::Pista& t) { return GestorPlugins::esVst3 (t.instrument) ? plugins.instrument (t, fs, 2048) : interns (t); },
        [this, fs] (const mibase::Insert& x) { return plugins.efecte (x, fs, 2048); }, &mapaSo));
    if (instruments.avis.isNotEmpty())
    {
        historial.afegeix (instruments.avis);
        estat.info = instruments.avis;
        instruments.avis.clear();
    }
    if (plugins.getUltimError() != errorAbans && plugins.getUltimError().isNotEmpty())
    {
        historial.afegeix (plugins.getUltimError());
        estat.info = plugins.getUltimError();
    }
}

void PantallaEstudi::avisDeTargeta (const juce::String& titol, const juce::String& text)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, titol, text);
}

void PantallaEstudi::reprodueix()
{
    if (! servei.enMarxa())
    {
        avisDeTargeta ("No pot sonar", "La targeta d'àudio està aturada. Obre Fitxer → Preferències i tria-la."_ca);
        return;
    }
    publicaPla();
    iniciPlay = estat.capcal;
    plugins.buidaCues();   // 0.27.2: que no soni la cua dels efectes de l'última vegada
    servei.estudi().reprodueix (estat.capcal);
    estat.transport = 1;
    estatEsperat = 1;
    esperantMotor = true;
    esperaExacte = false;
    tempsOrdre = juce::Time::currentTimeMillis();
    estat.avisa();
}

void PantallaEstudi::atura()
{
    if (estat.transport == 0)
    {
        // Ja aturat: a l'inici.
        transport (0);
        return;
    }
    const int abans = estat.transport;
    if (gravacio.midi) gravacio.fins = juce::jmax (gravacio.cursor, servei.estudi().getPosicio());   // fins on s'ha gravat
    servei.estudi().atura();
    plugins.buidaCues();   // 0.27.2: en aturar, fora les cues de les reverbs i els delays
    estat.transport = 0;
    estatEsperat = 0;
    esperantMotor = true;
    esperaExacte = false;
    tempsOrdre = juce::Time::currentTimeMillis();
    // Torna on havies començat (per tornar-ho a escoltar o a gravar).
    estat.capcal = iniciPlay;   // 0.72: gravant des d'aturat, iniciPlay és el punt de gravació; amb Punch In, on es va fer play
    (void) abans;
    estat.avisa();
}

void PantallaEstudi::grava (double cursor)
{
    const int pista = pistaArmada();
    if (pista < 0)
    {
        estat.info = "Per gravar, encén el botó vermell d'una pista"_ca;
        estat.avisa();
        return;
    }
    if (! servei.enMarxa())
    {
        avisDeTargeta ("No es pot gravar", "La targeta d'àudio està aturada. Obre Fitxer → Preferències i tria-la."_ca);
        return;
    }

    const auto& t = estat.projecte.pistes[(size_t) pista];
    auto& motor = servei.estudi();

    if (mibase::esMidi (t))
    {
        // Pista MIDI: es grava el que es toca al teclat (no la targeta).
        publicaPla();
        gravacio.pista = pista;
        gravacio.midi = true;
        gravacio.missatges.clear();
        gravacio.punch = false;
        gravacio.cursor = cursor;
        gravacio.desDe = iniciPreroll (cursor);   // 0.72: dos compassos de pre-roll
        gravacio.retard = 0.0;
        gravacio.fitxer = juce::File();
        MotorEstudi::MidiGravat buit;
        while (motor.treuMidiGravat (buit)) {}
        const int slot = slotDePista (t);
        motor.setSlotTeclat (slot);
        motor.setSlotGravaMidi (slot);
        plugins.buidaCues();
        motor.grava (gravacio.desDe, gravacio.cursor, 0, 1);
        estat.cursorGravacio = gravacio.cursor;
        estat.transport = 2;
        estatEsperat = 2;
        esperantMotor = true;
        esperaExacte = false;
        iniciPlay = cursor;
        tempsOrdre = juce::Time::currentTimeMillis();
        estat.info = "Pre-roll de 2 compassos… (toca el teclat MIDI)"_ca;
        estat.avisa();
        return;
    }

    estat.nomsEntrades = servei.getNomsEntrades();
    const auto& noms = estat.nomsEntrades;
    const int darrera = t.entrada + (t.canals == 2 ? 1 : 0);
    if (darrera >= noms.size() || noms[t.entrada].isEmpty() || noms[darrera].isEmpty())
    {
        avisDeTargeta ("No es pot gravar",
                       "La pista «"_ca + dibuix::utf8 (t.nom) + "» grava de l'entrada "_ca + EstatEstudi::textEntrada (t).substring (1)
                       + ", que no està activa.\n\nTria'n una altra (botó dret al nom de la pista → Entrada per gravar) o activa-la a Fitxer → Preferències."_ca);
        return;
    }

    auto carpetaAudios = audios();
    carpetaAudios.createDirectory();
    const auto nomFitxer = dibuix::utf8 (disc::nomPresa (dibuix::cami (carpetaAudios), t.nom));
    gravacio.fitxer = carpetaAudios.getChildFile (nomFitxer);
    if (! motor.gravadora().comenca (dibuix::cami (gravacio.fitxer), t.canals, motor.getFs()))
    {
        avisDeTargeta ("No es pot gravar", "No s'ha pogut crear el fitxer:\n"_ca + gravacio.fitxer.getFullPathName());
        return;
    }

    // 0.81: amb el bucle encès es grava en bucle (cada volta, una presa): si el capçal és fora del bucle, comença a l'inici
    bool enBucle = false;
    {
        const auto& b = estat.projecte.bucle;
        if (b.marcat && b.actiu && b.a > b.de && ed::estructuraActiva (estat.projecte) == nullptr)
        {
            const double bi = ed::iniciBucle (estat.projecte), bf = ed::fiBucle (estat.projecte);
            if (cursor < bi - 1.0e-6 || cursor >= bf - 0.05) cursor = bi;
            enBucle = true;
        }
    }
    publicaPla();
    gravacio.pista = pista;
    gravacio.punch = false;
    gravacio.cursor = cursor;
    gravacio.desDe = iniciPreroll (cursor);   // 0.72: dos compassos de pre-roll (també es graven: marge per recuperar)
    gravacio.retard = servei.retardGravacio (gravacio.retardMesurat);
    plugins.buidaCues();
    motor.grava (gravacio.desDe, gravacio.cursor, t.entrada, t.canals);
    estat.cursorGravacio = gravacio.cursor;
    estat.transport = 2;
    estatEsperat = 2;
    esperantMotor = true;
    esperaExacte = false;
    iniciPlay = cursor;
    tempsOrdre = juce::Time::currentTimeMillis();
    estat.info = enBucle ? "Pre-roll de 2 compassos… Grava en bucle: cada volta serà una presa"_ca : "Pre-roll de 2 compassos…"_ca;
    estat.avisa();
}

//==============================================================================
// 0.72: gravació intuïtiva (Smart Record). Una sola tecla: el 0 del teclat numèric.
//   Aturat + pista armada          → 2 compassos de pre-roll i grava des del capçal (o des del clip buit preparat)
//   Sonant + pista armada          → Punch In ara mateix (sense aturar), amb pregravació al davant del fitxer
//   Gravant (o en el pre-roll)     → Punch Out: deixa de gravar i continua sonant; la pista continua armada
//   Sense pista armada             → no fa res (no arma mai cap pista)
//==============================================================================
bool PantallaEstudi::esNumpad0 (const juce::KeyPress& k) const
{
    if (k.getModifiers().isCommandDown() || k.getModifiers().isAltDown()) return false;
    const int c = k.getKeyCode();
    if (c == juce::KeyPress::numberPad0) return true;
    // Per si el sistema dona el codi del «0» normal: només val si la tecla de baix és la del teclat numèric.
    return c == '0' && juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::numberPad0) && ! juce::KeyPress::isKeyCurrentlyDown ('0');
}

int PantallaEstudi::pistaArmada() const
{
    for (int i = 0; i < estat.numPistes(); ++i)
        if (estat.projecte.pistes[(size_t) i].grava && ! mibase::esBus (estat.projecte.pistes[(size_t) i])) return i;
    return -1;
}

double PantallaEstudi::iniciPreroll (double cursor) const
{
    // Dos compassos enrere (amb el mapa de tempo); pot quedar abans del zero (compàs de compte).
    double t = cursor;
    for (int i = 0; i < 2; ++i)
        t -= ed::segonsPerCompasA (estat.projecte, juce::jmax (0.0, t - 1e-6));
    return t;
}

int PantallaEstudi::clipPerGravar (int pista) const
{
    const auto* t = estat.pista (pista);
    if (t == nullptr) return -1;
    auto buit = [] (const Clip& c) { return c.presa.empty() && (! c.midi || (c.notes.empty() && c.events.empty())); };
    // 1) Un clip buit triat d'aquesta pista.
    for (auto& r : estat.triats)
        if (r.pista == pista && r.clip >= 0 && r.clip < (int) t->clips.size() && buit (t->clips[(size_t) r.clip])) return r.clip;
    // 2) El darrer clip buit fet amb Ctrl + arrossegar, si encara hi és (al mateix lloc).
    const auto& cp = estat.clipPreparat;
    if (cp.pista == t->id)
        for (int k = (int) t->clips.size() - 1; k >= 0; --k)
        {
            const auto& c = t->clips[(size_t) k];
            if (buit (c) && std::abs (c.iniciVisible() - cp.ini) < 1e-6 && std::abs (c.fiVisible() - cp.fi) < 1e-6) return k;
        }
    return -1;
}

void PantallaEstudi::gravaContextual()
{
    if (esperantMotor) return;   // l'ordre anterior encara no s'ha fet: una pulsació, una acció
    if (estat.transport >= 2) { punchOut(); return; }
    const int pista = pistaArmada();
    if (pista < 0)
    {
        estat.info = "Per gravar, arma una pista amb el seu botó vermell i prem 0 (teclat numèric)"_ca;
        estat.avisa();
        return;
    }
    if (estat.transport == 1) { punchIn (pista); return; }

    // Aturat: des del clip buit preparat (si n'hi ha) o des del capçal.
    double cursor = estat.capcal;
    const int c = clipPerGravar (pista);
    if (c >= 0)
        cursor = estat.projecte.pistes[(size_t) pista].clips[(size_t) c].iniciVisible();
    grava (cursor);
    if (c >= 0 && estat.transport == 2) estat.clipPreparat = {};   // ja s'hi grava (si no ha pogut començar, es queda preparat)
}

void PantallaEstudi::punchIn (int pista)
{
    if (! servei.enMarxa()) return;
    const auto& t = estat.projecte.pistes[(size_t) pista];
    auto& motor = servei.estudi();
    if (mibase::esMidi (t))
    {
        gravacio = {};
        gravacio.pista = pista;
        gravacio.midi = true;
        gravacio.punch = true;
        gravacio.cursor = estat.capcal;
        MotorEstudi::MidiGravat buit;
        while (motor.treuMidiGravat (buit)) {}
        const int slot = slotDePista (t);
        motor.setSlotTeclat (slot);
        motor.setSlotGravaMidi (slot);
        motor.entra (-1, 1);
    }
    else
    {
        estat.nomsEntrades = servei.getNomsEntrades();
        const auto& noms = estat.nomsEntrades;
        const int darrera = t.entrada + (t.canals == 2 ? 1 : 0);
        if (darrera >= noms.size() || noms[t.entrada].isEmpty() || noms[darrera].isEmpty())
        {
            estat.info = "La pista grava d'una entrada que no està activa (botó dret al nom → Entrada per gravar)"_ca;
            estat.avisa();
            return;
        }
        auto carpetaAudios = audios();
        carpetaAudios.createDirectory();
        const auto fitxer = carpetaAudios.getChildFile (dibuix::utf8 (disc::nomPresa (dibuix::cami (carpetaAudios), t.nom)));
        if (! motor.gravadora().comenca (dibuix::cami (fitxer), t.canals, motor.getFs()))
        {
            avisDeTargeta ("No es pot gravar", "No s'ha pogut crear el fitxer:\n"_ca + fitxer.getFullPathName());
            return;
        }
        gravacio = {};
        gravacio.pista = pista;
        gravacio.punch = true;
        gravacio.fitxer = fitxer;
        gravacio.cursor = estat.capcal;
        gravacio.desDe = estat.capcal;
        gravacio.retard = servei.retardGravacio (gravacio.retardMesurat);
        motor.entra (t.entrada, t.canals);
    }
    estat.cursorGravacio = estat.capcal;
    estatEsperat = 3;
    esperaExacte = true;
    esperantMotor = true;
    tempsOrdre = juce::Time::currentTimeMillis();
}

void PantallaEstudi::punchOut()
{
    servei.estudi().surt();
    estatEsperat = 1;
    esperaExacte = true;
    esperantMotor = true;
    tempsOrdre = juce::Time::currentTimeMillis();
    estat.info = "Punch Out: continua sonant (la pista continua armada)"_ca;
    estat.avisa();
}

void PantallaEstudi::revisaArmament()
{
    // Canviar de pista desarma l'anterior i no arma la nova. Mentre es grava, s'espera a acabar (no es talla la presa).
    auto& motor = servei.estudi();
    {
        // La pregravació del Punch In: l'entrada de la pista d'àudio armada.
        const int a = pistaArmada();
        const auto* t = estat.pista (a);
        if (t != nullptr && ! mibase::esMidi (*t)) motor.setPregravacio (t->entrada, t->canals);
        else motor.setPregravacio (-1, 1);
    }
    const auto* tri = estat.pista (estat.pistaTriada);
    const std::string id = tri != nullptr ? tri->id : std::string();
    if (id == idTriadaAbans) return;
    if (estat.transport >= 2 || esperantMotor || motor.gravadora().activa() || (gravacio.midi && gravacio.pista >= 0)) return;
    idTriadaAbans = id;
    bool canvi = false;
    for (auto& t : estat.projecte.pistes)
        if (t.grava && t.id != id) { t.grava = false; canvi = true; }
    if (canvi)
    {
        calDesar = true;
        startTimer (700);
        revisaArmament();   // la pregravació ja no és d'aquella pista
    }
}

void PantallaEstudi::acabaGravacio()
{
    auto& motor = servei.estudi();
    const auto r = motor.gravadora().acaba();
    const double fs = motor.getFs();
    const auto fitxer = gravacio.fitxer;
    const int pista = gravacio.pista;
    gravacio.pista = -1;
    if (estat.pista (pista) == nullptr)
    {
        fitxer.deleteFile();
        return;
    }
    if (! r.correcte)
    {
        fitxer.deleteFile();
        avisDeTargeta ("La gravació ha fallat", "No s'ha pogut escriure l'àudio al disc."_ca);
        return;
    }

    if (gravacio.perSampler)
    {
        // 0.28: un sample nou dins el samplejador (el retard es corregeix amb l'inici del fragment)
        gravacio.perSampler = false;
        auto& t = estat.projecte.pistes[(size_t) pista];
        if (r.frames <= 0 || ! mibase::esSampler (t)) { fitxer.deleteFile(); return; }
        estat.desfes.abans (estat.projecte);
        auto& p = t.sampler;
        p.fitxer = ("Samples/" + fitxer.getFileName()).toStdString();
        p.nom = fitxer.getFileNameWithoutExtension().toStdString();
        p.inici = juce::jlimit (0.0, (double) r.frames / fs, gravacio.retard);
        p.fi = -1.0;
        p.bucleDe = p.bucleA = -1.0;
        p.speed = 1.0; p.reversa = false; p.bpm = 0.0;
        if (auto mo = Instruments::mostraSample (fitxer)) { const int n = mibase::sampler::detectaRoot (*mo); p.root = n >= 0 ? n : 60; }
        estat.sampler.pxPerSeg = 0.0;
        estat.info = "Gravat al samplejador: "_ca + fitxer.getFileName() + " (" + juce::String ((double) r.frames / fs, 1) + " s)";
        historial.afegeix ("Sample gravat: "_ca + fitxer.getFileName());
        estat.canviat ("Samplejador: gravat «"_ca + fitxer.getFileName() + "»"_ca);
        return;
    }

    const auto copia = estat.projecte;
    const auto nomPista = dibuix::utf8 (estat.projecte.pistes[(size_t) pista].nom);
    const auto rel = (juce::String::fromUTF8 (disc::carpetaAudios) + "/" + fitxer.getFileName()).toStdString();
    const int clip = ed::afegeixGravacio (estat.projecte, pista, rel, fitxer.getFileNameWithoutExtension().toStdString(),
                                          (double) r.frames / fs, gravacio.desDe, gravacio.cursor, gravacio.retard);
    if (clip < 0)
    {
        fitxer.deleteFile();
        estat.info = "No s'ha gravat res (s'ha aturat durant el compte)"_ca;
        estat.avisa();
        return;
    }

    // 0.81: en bucle, cada volta és una presa; i si trepitja altres gravacions del mateix tros, totes passen a ser preses
    std::vector<int> nous { clip };
    const int voltes = motor.getVoltes();
    if (voltes > 0)
        nous = ed::afegeixVoltes (estat.projecte, pista, clip, motor.getIniciVolta(), motor.getPrimeraVolta(), motor.getLlargVolta(),
                                  voltes, gravacio.retard);
    const auto takesAbans = estat.projecte.pistes[(size_t) pista].takes.size();
    const bool ambTakes = ed::organitzaTakes (estat.projecte, pista, nous);
    const auto takesAra = estat.projecte.pistes[(size_t) pista].takes.size();

    estat.desfes.abans (copia);
    carregaOna (estat.projecte.preses.back());
    if (ambTakes) estat.capTriat();
    else estat.triaClip (pista, clip);
    const auto retard = juce::String (gravacio.retard * 1000.0, 1).replaceCharacter ('.', ',') + " ms";
    estat.info = "Gravat: "_ca + fitxer.getFileName() + " (retard corregit: " + retard + (gravacio.retardMesurat ? ", mesurat)" : ", el que diu la targeta)");
    historial.afegeix ("Gravada la presa «"_ca + fitxer.getFileName() + "» (" + juce::String ((double) r.frames / fs, 1) + " s, retard corregit "
                       + retard + ")");
    if (ambTakes && takesAra > takesAbans)
    {
        estat.info += " · La pista té "_ca + juce::String ((int) takesAra) + " preses: arrossega sobre una presa per triar-ne el tros"_ca;
        historial.afegeix ("Preses a la pista «"_ca + nomPista + "»: "_ca + juce::String ((int) takesAra)
                           + (voltes > 0 ? " (gravació en bucle, " + juce::String ((int) nous.size()) + " voltes)" : juce::String()));
    }
    estat.canviat ("Gravada «"_ca + fitxer.getFileName() + "» a la pista «"_ca + nomPista + "»"_ca
                   + (ambTakes ? " · "_ca + juce::String ((int) takesAra) + " preses" : juce::String()));
    if (r.perdudes > 0)
        avisDeTargeta ("Compte: s'han perdut mostres",
                       "El disc no ha anat prou de pressa i s'han perdut "_ca + juce::String (r.perdudes) + " mostres de la gravació.");
}

/** 0.62.1: el que fa el play abans de sonar (pla nou i plugins a zero), però sense fer sonar res. En Roger ha vist
    que, tornant a Mibase des d'una altra finestra, a vegades els instruments no sonen fins que es fa play. */
void PantallaEstudi::refrescaSo()
{
    if (estat.transport != 0 || ! servei.enMarxa()) return;
    publicaPla();
    plugins.buidaCues();
    historial.afegeix ("So refrescat en tornar a Mibase"_ca);
}

void PantallaEstudi::commutaRetardMinim()
{
    retardMinimBoto = ! retardMinimBoto;
    servei.estudi().mesclador.setRetardMinim (retardMinimActiu());
    estat.info = retardMinimBoto ? "Retard mínim encès: els plugins que retarden s'apaguen a la pista que toques o graves, als busos i al màster"_ca
                                 : "Retard mínim apagat: tots els plugins sonen i el retard es compensa"_ca;
    estat.avisa();
}

void PantallaEstudi::menuRetardMinim (juce::Component& on)
{
    // 0.69: un retard petit gairebé no es nota tocant: per defecte, RM només apaga els plugins de més de 5 ms
    const int ara = ajustos.getIntValue ("estudi.rmMinimMs", 5);
    juce::PopupMenu m;
    m.addSectionHeader ("Retard mínim: apaga els plugins que retarden més de…"_ca);
    for (int ms : { 0, 2, 5, 10, 20 })
        m.addItem (ms + 1, ms == 0 ? "Qualsevol retard"_ca : juce::String (ms) + " ms", true, ms == ara);
    m.addSeparator();
    m.addItem (100, "Perquè un plugin no s'apagui mai (un ampli de guitarra…): botó dret sobre l'efecte"_ca, false);
    juce::Component::SafePointer<PantallaEstudi> jo (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&on), [jo] (int r)
    {
        if (jo == nullptr || r < 1 || r > 21) return;
        jo->ajustos.setValue ("estudi.rmMinimMs", r - 1);
        jo->estat.info = "Retard mínim: apaga els plugins de més de "_ca + juce::String (r - 1) + " ms";
        jo->estat.avisa();
    });
}

void PantallaEstudi::tic()
{
    servei.estudi().mesclador.setRetardMinim (retardMinimActiu());   // 0.68: s'encén sol mentre es grava
    servei.estudi().mesclador.setLlindarRM ((int) (ajustos.getIntValue ("estudi.rmMinimMs", 5) * 0.001 * servei.estudi().getFs()));   // 0.69
    {
        const bool davant = juce::Process::isForegroundProcess();
        if (davant && ! eraDavant) refrescaSo();
        eraDavant = davant;
        // 0.64: si una pista ha donat valors no vàlids (NaN…), el mesclador l'ha silenciada: posa els efectes a zero i anota-ho
        auto& sonda = servei.estudi().mesclador.sonda;
        if (const auto nv = sonda.noValids.load(); nv != noValidsVistos)
        {
            noValidsVistos = nv;
            const int slot = sonda.slotNoValid.load();
            juce::String qui = slot == -2 ? juce::String ("el màster"_ca) : "una pista"_ca;
            for (const auto& t : estat.projecte.pistes)
                if (slot >= 0 && t.id.size() > 1 && std::atoi (t.id.c_str() + 1) % 128 == slot) qui = "la pista «"_ca + juce::String::fromUTF8 (t.nom.c_str()) + "»"_ca;
            plugins.buidaCues();
            historial.afegeix ("So recuperat: "_ca + qui + " donava valors no vàlids (silenciada un moment i efectes a zero)"_ca);
        }
    }
    {
    }
    auto& motor = servei.estudi();
    motor.recull();
    if (ed::teCanvisTempo (estat.projecte))   // 0.28: els plugins (delays a tempo…) saben el tempo d'on sona
    {
        const auto tr = ed::tramA (estat.projecte, estat.capcal);
        RellotgePlugins::global().tempo.store (tr.tempo);
        RellotgePlugins::global().num.store (tr.num);
        RellotgePlugins::global().den.store (tr.den);
    }

    // Estat del motor.
    const int ara = (int) motor.getEstat();
    if (esperantMotor)
    {
        const bool fet = esperaExacte ? ara == estatEsperat : (estatEsperat == 0 ? ara == 0 : ara != 0);
        if (fet)
        {
            esperantMotor = false;
            if (esperaExacte)
            {
                // 0.72: el fil d'àudio ha fet el Punch In / Punch Out
                esperaExacte = false;
                estat.transport = ara;
                if (ara == 3)
                {
                    gravacio.desDe = motor.getIniciFitxer();
                    gravacio.cursor = estat.cursorGravacio = motor.getPuntGravacio();
                    estat.info = "Gravant (Punch In)… Torna a prémer 0 per sortir"_ca;
                }
                estat.avisa();
            }
        }
        else if (juce::Time::currentTimeMillis() - tempsOrdre > 1500)
        {
            esperantMotor = false;
            if (esperaExacte) { esperaExacte = false; estat.transport = ara; }   // 0.72: no s'ha pogut (ja no sonava…)
            else if (estatEsperat != 0)
            {
                estat.transport = 0;
                avisDeTargeta ("La targeta no respon", "S'ha demanat sonar però la targeta d'àudio no ha respost. Mira Fitxer → Preferències."_ca);
            }
            estat.avisa();
        }
    }
    else if (ara != estat.transport)
    {
        if (ara == 3 && estat.transport == 2) estat.info = "Gravant…"_ca;
        if (ara == 0 && estat.transport == 1) estat.capcal = iniciPlay;   // la targeta s'ha aturat sola
        estat.transport = ara;
        estat.avisa();
    }
    // 0.72: aturat, o sonant després d'un Punch Out: la presa es tanca i queda a la pista (la pista continua armada)
    if (estat.transport <= 1 && ! esperantMotor && motor.gravadora().activa())
        acabaGravacio();
    if (gravacio.midi && gravacio.pista >= 0)
    {
        recullMidiGravat();
        if (estat.transport <= 1 && ! esperantMotor)
        {
            if (estat.transport == 1) gravacio.fins = juce::jmax (gravacio.cursor, motor.getFiGravacio());
            acabaGravacioMidi();
        }
    }

    // Capçal: mentre sona, el del motor (i la vista el segueix).
    // 0.54: sense salts de pàgina: quan arriba a la vora, la vista llisca suau fins a deixar el capçal al mig
    // (mibase::SeguimentCapcal). Si es mou la vista a mà, deixa de seguir fins al pròxim play.
    const float xAbans = estat.x (estat.capcal);
    const double araMs = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (0.001, 0.25, (araMs - darrerTicMs) / 1000.0);
    darrerTicMs = araMs;
    if (estat.transport != 0 && transportAbans == 0) seguiment.reinicia();
    transportAbans = estat.transport;
    if (estat.transport != 0 && ! esperantMotor)
    {
        estat.capcal = motor.getPosicio();
        const double visibles = pistes.getWidth() / estat.pxPerSegon;
        const double nou = seguiment.pas (estat.capcal, estat.iniciVista, visibles, dt, segonsPerCentrar());
        if (nou != estat.iniciVista)
        {
            estat.iniciVista = nou;
            actualitzaBarres();
            estat.avisa();
        }
        else
        {
            const float xAra = estat.x (estat.capcal);
            const int x0 = (int) juce::jmin (xAbans, xAra) - 6, x1 = (int) juce::jmax (xAbans, xAra) + 6;
            if (estat.transport == 3) pistes.repaint();
            else pistes.repaint (x0, 0, x1 - x0, pistes.getHeight());
            regla.repaint (x0, 0, x1 - x0, regla.getHeight());
        }
        zonaEditors.ticTransport();
        if (taulaEstructura.isVisible()) taulaEstructura.repaint();
    }
    recullVeu();
    recullElastic();   // 0.86
    if (editorVeu.isVisible()) editorVeu.tic();
    if (editorSampler.isVisible()) editorSampler.tic();
    if (editorAuto.isVisible()) editorAuto.tic();
    editorAuto.ticGravacio();   // 0.57: grava l'automatització encara que l'editor estigui tancat


    // Vúmetres: el pic de cada canal, amb caiguda suau.
    auto cau = [] (float& v, float nou) { v = juce::jmax (nou, v * 0.82f); if (v < 1.0e-4f) v = 0.0f; };
    bool canvi = false;
    for (int i = 0; i < maxSlots * 2; ++i)
    {
        const float abans = estat.nivells[(size_t) i];
        cau (estat.nivells[(size_t) i], motor.vuPista (i / 2, i % 2).llegeixIReinicia());
        canvi = canvi || std::abs (abans - estat.nivells[(size_t) i]) > 1.0e-6f;
    }
    for (int e = 0; e < 32; ++e)
    {
        const float abans = estat.nivellsEntrada[(size_t) e];
        cau (estat.nivellsEntrada[(size_t) e], motor.vuEntrada (e).llegeixIReinicia());
        canvi = canvi || std::abs (abans - estat.nivellsEntrada[(size_t) e]) > 1.0e-6f;
    }
    for (int c = 0; c < 2; ++c)
    {
        const float abans = estat.nivellsSortida[(size_t) c];
        cau (estat.nivellsSortida[(size_t) c], motor.vuSortida (c).llegeixIReinicia());
        canvi = canvi || std::abs (abans - estat.nivellsSortida[(size_t) c]) > 1.0e-6f;
    }
    if (canvi)
    {
        if (capcaleres.isVisible()) capcaleres.repaint (capcaleres.getWidth() - 20, 0, 20, capcaleres.getHeight());
        esquerra.repaint();
        if (mescles.isVisible()) mescles.repaint();
    }

    // De tant en tant, les entrades de la targeta (per si s'ha canviat al Diagnòstic).
    static int comptador = 0;
    if (++comptador % 30 == 0)
    {
        auto noms = servei.getNomsEntrades();
        if (noms != estat.nomsEntrades) { estat.nomsEntrades = noms; capcaleres.repaint(); }
    }
}

void PantallaEstudi::exportaMescla()
{
    // 0.83: primer la finestra d'opcions
    desaAra();
    dialegExporta = std::make_unique<DialegExporta> (estat.projecte, estat.pistaTriada, [this] (const DialegExporta::Resultat& r)
    {
        const bool unSol = r.que == DialegExporta::Tot || r.que == DialegExporta::Triades;
        const auto nomProj = dibuix::utf8 (estat.projecte.nom);
        if (unSol)
        {
            const auto proposta = estat.carpeta.getChildFile (nomProj + (r.que == DialegExporta::Tot ? " - mescla.wav" : " - pistes triades.wav"));
            selector = std::make_unique<juce::FileChooser> ("Exporta (WAV estèreo)"_ca, proposta, "*.wav");
            selector->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                       | juce::FileBrowserComponent::warnAboutOverwriting,
                                   [this, r] (const juce::FileChooser& fc)
            {
                auto desti = fc.getResult();
                if (desti == juce::File()) return;
                if (! desti.hasFileExtension ("wav")) desti = desti.withFileExtension ("wav");
                fesExportacio (r, desti);
            });
        }
        else
        {
            selector = std::make_unique<juce::FileChooser> ("Tria la carpeta on es desaran els fitxers"_ca, estat.carpeta);
            selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                   [this, r] (const juce::FileChooser& fc)
            {
                const auto carpeta = fc.getResult();
                if (carpeta == juce::File()) return;
                fesExportacio (r, carpeta);
            });
        }
    });
    DialegExporta::mostra (*this, dialegExporta);
}

void PantallaEstudi::fesExportacio (const DialegExporta::Resultat& r, const juce::File& desti)
{
    instruments.desaEstats (estat.projecte);   // 0.88: els ajustos d'ara del SUB & BASS
    const auto& p = estat.projecte;
    const double fs = servei.estudi().getFs() > 0.0 ? servei.estudi().getFs() : 48000.0;
    const auto nomProj = dibuix::utf8 (p.nom);
    auto netejaNom = [] (juce::String n) { return juce::File::createLegalFileName (n).trim(); };

    // Les feines: cada una, unes pistes (buit = tot) i un fitxer
    struct Feina { std::vector<int> pistes; juce::File fitxer; juce::String nom; };
    std::vector<Feina> feines;
    if (r.que == DialegExporta::Tot) feines.push_back ({ {}, desti, "la mescla" });
    else if (r.que == DialegExporta::Triades) feines.push_back ({ r.pistes, desti, "les pistes triades" });
    else if (r.que == DialegExporta::UnPerPista)
        for (int i : r.pistes)
        {
            const auto nom = dibuix::utf8 (p.pistes[(size_t) i].nom);
            feines.push_back ({ { i }, desti.getChildFile (netejaNom (nomProj + " - " + nom) + ".wav"), "«" + nom + "»" });
        }
    else
        for (const auto& id : r.carpetes)
        {
            std::vector<int> v;
            for (int i = 0; i < (int) p.pistes.size(); ++i)
                if (p.pistes[(size_t) i].carpeta == id && ! mibase::esBus (p.pistes[(size_t) i]) && ! mibase::esAcords (p.pistes[(size_t) i])) v.push_back (i);
            juce::String nom = id;
            for (const auto& c : p.carpetes) if (c.id == id) nom = dibuix::utf8 (c.nom);
            if (! v.empty()) feines.push_back ({ v, desti.getChildFile (netejaNom (nomProj + " - " + nom) + ".wav"), "la carpeta «" + nom + "»" });
        }

    // El tros: el mateix per a tots els fitxers (així els stems queden alineats i amb la mateixa llargada)
    std::vector<int> totes;
    if (r.que != DialegExporta::Tot) for (auto& f : feines) totes.insert (totes.end(), f.pistes.begin(), f.pistes.end());
    std::pair<double, double> tros;
    mibase::projectePerExportar (p, r.opcions, totes, tros);
    if (tros.second <= tros.first || feines.empty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Exporta", "No hi ha res que soni per exportar en aquest tros."_ca);
        return;
    }
    const bool variosFitxers = feines.size() > 1 || r.que == DialegExporta::UnPerPista || r.que == DialegExporta::UnPerCarpeta;
    const double cua = 4.0;   // reverbs, delays i notes que s'apaguen

    juce::StringArray fets, errors;
    juce::MouseCursor::showWaitCursor();
    sincronitzaElastic (true);   // 0.86: els sons estirats, tots fets abans d'exportar
    for (auto& f : feines)
    {
        tancaments::pas ("exportant " + f.nom + ": preparant");
        std::pair<double, double> trosFeina;
        auto q = mibase::projectePerExportar (p, r.opcions, f.pistes, trosFeina);
        const auto solo = ed::estatSolo (q);
        std::vector<std::string> sonen;   // només es creen els instruments de les pistes que sonen
        for (size_t i = 0; i < q.pistes.size(); ++i) if (solo[i] != 0) sonen.push_back (q.pistes[i].id);
        auto interns = Instruments::perExportar (fs, 4096, estat.carpeta);
        std::string error;
        bool fet = false;
        {
            auto pla = construeixPla (q, audioPreses,
                [this, fs, interns, sonen] (const mibase::Pista& t) -> std::shared_ptr<mibase::Instrument>
                {
                    if (std::find (sonen.begin(), sonen.end(), t.id) == sonen.end()) return nullptr;
                    return GestorPlugins::esVst3 (t.instrument) ? plugins.instrumentNou (t, fs, 4096, GestorPlugins::escalfamentVst3) : interns (t);
                },
                [this, fs] (const mibase::Insert& x) { return x.actiu ? plugins.efecteNou (x, fs, 4096) : nullptr; },
                &mapaSo);
            tancaments::pas ("exportant " + f.nom + ": renderitzant");
            fet = mibase::exportaPla (*pla, fs, tros.first, tros.second, r.bits, cua, dibuix::cami (f.fitxer), error, {}, ! variosFitxers);
            tancaments::pas ("exportant " + f.nom + ": tancant les còpies dels plugins");
        }
        tancaments::pas ("exportació acabada");
        if (fet) fets.add (f.fitxer.getFileName());
        else errors.add (f.nom + ": " + dibuix::utf8 (error));
    }
    juce::MouseCursor::hideWaitCursor();

    const juce::String que = r.opcions.tros == mibase::OpcionsExport::Bucle ? "el bucle"
                           : r.opcions.tros == mibase::OpcionsExport::Part ? "una part"
                           : r.opcions.estructura.empty() ? "tota la cançó (lineal)" : "tota la cançó (estructura)";
    const juce::String com = juce::String (r.bits) + " bits" + (r.opcions.efectes ? ", amb efectes" : ", sense efectes")
                           + (r.opcions.master ? ", amb màster" : ", sense màster");
    for (auto& f : fets)
    {
        disc::afegeixEvolucio (dibuix::cami (estat.carpeta), dibuix::araText().toStdString(), ("Exportat " + f + " (" + que + ", " + com + ")").toStdString());
        historial.afegeix ("Exportat "_ca + f + " (" + que + ", " + com + ")");
    }
    juce::String text;
    if (! fets.isEmpty())
        text << "S'ha exportat " << que << " (" << com << ")"
             << (fets.size() == 1 ? juce::String (":\n") + (variosFitxers ? desti.getChildFile (fets[0]) : desti).getFullPathName()
                                  : juce::String (": ") + juce::String (fets.size()) + " fitxers a\n" + desti.getFullPathName());
    if (! errors.isEmpty()) text << (text.isEmpty() ? "" : "\n\n") << "No s'ha pogut exportar:\n" << errors.joinIntoString ("\n");
    juce::AlertWindow::showMessageBoxAsync (errors.isEmpty() ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                                            "Exporta", text);
}

std::pair<juce::String, juce::String> PantallaEstudi::textPosicio() const
{
    const double t = estat.capcal;
    const int min = (int) (t / 60.0);
    const double seg = t - min * 60.0;
    return { dibuix::utf8 (ed::textPosicio (estat.projecte, t)),
             juce::String::formatted ("%02d:%06.3f", min, seg) };
}

void PantallaEstudi::obreKits()
{
    if (finestraKits != nullptr) { finestraKits->toFront (true); return; }
    finestraKits = std::make_unique<FinestraKits> (estat, servei, [this] { finestraKits.reset(); });
    quedaSobreMibase (*finestraKits);   // 0.70
}

void PantallaEstudi::pistaNova (const juce::String& tipus, int canals)
{
    if (tipus == "bus" || tipus == "fx") { busNou (tipus == "fx", -1, false); return; }   // 0.36: des del menú de pista nova
    estat.desfes.abans (estat.projecte);
    if (tipus == "acords")
    {
        // 0.36: la pista d'Acords (una, a dalt)
        estat.pistaTriada = ed::creaPistaAcords (estat.projecte);
        estat.capTriat();
        estat.canviat ("Pista d'acords"_ca);
        return;
    }
    if (tipus == "bateria")
    {
        // 0.20: pista de bateria amb el kit intern (o el darrer que s'ha fet servir) i un clip de 4 compassos amb un ritme bàsic.
        const auto nom = ed::nomPistaLliure (estat.projecte, "Bateria");
        auto& t = ed::afegeixPista (estat.projecte, nom, "midi");   // 0.39: pista d'instrument amb el kit de bateria
        t.canals = 2;
        t.instrument = mibase::instrumentKit;
        kits::posaKit (t, kits::ultimKit());
        estat.pistaTriada = ed::pistaPerId (estat.projecte, t.id);   // 0.34: les pistes noves van abans dels busos
        estat.capTriat();
        estat.info = "Pista amb kit de bateria: fes un clip amb el llapis (2) i fes-hi doble clic per obrir el Drumpad"_ca;
        estat.canviat ("Pista nova: «"_ca + dibuix::utf8 (nom) + "» (bateria)"_ca);
        return;
    }
    if (tipus == "sampler")
    {
        // 0.26: pista d'instrument amb el samplejador; s'obre l'editor per carregar-hi un sample.
        const auto nom = ed::nomPistaLliure (estat.projecte, "Sampler");
        auto& t = ed::afegeixPista (estat.projecte, nom, "midi");
        t.canals = 2;
        t.instrument = mibase::instrumentSampler;
        estat.pistaTriada = ed::pistaPerId (estat.projecte, t.id);   // 0.34: les pistes noves van abans dels busos
        estat.capTriat();
        estat.canviat ("Pista nova: «"_ca + dibuix::utf8 (nom) + "» (samplejador)"_ca);
        obreSampler (estat.pistaTriada);
        estat.info = "Samplejador: carrega un sample (o arrossega'l a l'ona) i toca'l amb el teclat"_ca;
        estat.avisa();
        return;
    }
    if (tipus == "midi")
    {
        const auto nom = ed::nomPistaLliure (estat.projecte, "Instrument");   // 0.65: abans «Piano»
        auto& t = ed::afegeixPista (estat.projecte, nom, "midi");
        t.canals = 2;
        t.instrument = mibase::lead::idInstrument;   // 0.89: el sinte bàsic ja no hi és
        estat.pistaTriada = ed::pistaPerId (estat.projecte, t.id);   // 0.34: les pistes noves van abans dels busos
        estat.capTriat();
        estat.info = "Pista d'instrument: fes un clip amb el llapis (2) i dibuixa-hi notes, o grava amb el teclat MIDI"_ca;
        estat.canviat ("Pista nova: «"_ca + dibuix::utf8 (nom) + "» (instrument MIDI)"_ca);
        return;
    }
    const auto nom = ed::nomPistaLliure (estat.projecte, (canals == 2 ? juce::String::fromUTF8 ("Àudio estèreo") : juce::String::fromUTF8 ("Àudio")).toStdString());
    auto& t = ed::afegeixPista (estat.projecte, nom, tipus.toStdString());
    t.canals = canals;
    estat.pistaTriada = ed::pistaPerId (estat.projecte, t.id);   // 0.34: les pistes noves van abans dels busos
    estat.capTriat();
    estat.canviat ("Pista nova: «"_ca + dibuix::utf8 (nom) + "» ("_ca + (canals == 2 ? "estèreo" : "mono") + ")"_ca);
}

void PantallaEstudi::busNou (bool fx, int pista, bool comSortida)
{
    // 0.33: nom proposat «Grup 1», «Retorn 1»… (el primer que no hi sigui).
    juce::String proposat;
    for (int n = 1; n < 100 && proposat.isEmpty(); ++n)
    {
        const auto nom = juce::String (fx ? "Retorn " : "Grup ") + juce::String (n);
        bool hiEs = false;
        for (auto& t : estat.projecte.pistes) hiEs = hiEs || dibuix::utf8 (t.nom) == nom;
        if (! hiEs) proposat = nom;
    }
    dialeg = std::make_unique<DialegText> (fx ? "Retorn d'efectes nou" : "Bus de grup nou",
        fx ? "Un efecte compartit: les pistes hi envien la quantitat que volen. Posa-hi el plugin al 100 % d'efecte."_ca
           : "Ajunta pistes per tractar-les totes alhora: tria'l com a sortida de cada pista."_ca,
        "Crea'l"_ca,
        [] (const juce::String& t) -> juce::String { return t.trim().isEmpty() ? juce::String ("Escriu un nom.") : juce::String(); },
        [this, fx, pista, comSortida] (const juce::String& t)
        {
            estat.desfes.abans (estat.projecte);
            const auto id = ed::afegeixBus (estat.projecte, t.trim().toStdString(), fx).id;
            if (estat.pista (pista) != nullptr)
            {
                if (comSortida) ed::posaSortida (estat.projecte, pista, id);
                else ed::afegeixEnviament (estat.projecte, pista, id);
            }
            estat.canviat ((fx ? "Retorn d'efectes nou: «"_ca : "Bus de grup nou: «"_ca) + t.trim() + "»"_ca);
        }, proposat);
    DialegText::mostra (*this, dialeg);
}

void PantallaEstudi::canviaNom (int pista)
{
    if (estat.pista (pista) == nullptr) return;
    const auto actual = dibuix::utf8 (estat.projecte.pistes[(size_t) pista].nom);
    dialeg = std::make_unique<DialegText> ("Nom de la pista", "Els àudios que ja hi ha conserven el seu nom."_ca, "Canvia'l"_ca,
        [] (const juce::String& t) -> juce::String { return t.trim().isEmpty() ? juce::String ("Escriu un nom.") : juce::String(); },
        [this, pista, actual] (const juce::String& t)
        {
            if (estat.pista (pista) == nullptr || t.trim() == actual) return;
            estat.desfes.abans (estat.projecte);
            estat.projecte.pistes[(size_t) pista].nom = t.trim().toStdString();
            estat.canviat ("Pista «"_ca + actual + "» ara es diu «"_ca + t.trim() + "»"_ca);
        }, actual);
    DialegText::mostra (*this, dialeg);
}

void PantallaEstudi::menuGraella()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Graella de la taula de treball");
    const auto& vals = midi::valorsGraella();
    for (int i = 0; i < (int) vals.size(); ++i)
        if (vals[(size_t) i].ticks > 0) m.addItem (100 + i, juce::String::fromUTF8 (vals[(size_t) i].nom), true, vals[(size_t) i].ticks == estat.projecte.graella);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btGraella), [this] (int r)
    {
        const auto& v = midi::valorsGraella();
        if (r < 100 || r - 100 >= (int) v.size()) return;
        estat.projecte.graella = v[(size_t) (r - 100)].ticks;
        estat.projecte.snap = Snap::Temps;   // triar una subdivisió engega el mode «Graella»
        estat.info = "Graella: "_ca + juce::String::fromUTF8 (v[(size_t) (r - 100)].nom);
        estat.avisa();
        calDesar = true;
        startTimer (700);
    });
}

void PantallaEstudi::canviaTempo()
{
    dialeg = std::make_unique<DialegText> ("Tempo", "En bpm (de 30 a 300). Els clips d'àudio no es mouen; els MIDI segueixen els compassos."_ca, "Canvia'l"_ca,
        [] (const juce::String& t) -> juce::String
        {
            const auto v = t.trim().replaceCharacter (',', '.');
            if (! v.containsOnly ("0123456789.")) return "Escriu només el número."_ca;
            const double bpm = v.getDoubleValue();
            return (bpm < 30.0 || bpm > 300.0) ? juce::String ("Ha de ser entre 30 i 300.") : juce::String();
        },
        [this] (const juce::String& t)
        {
            const double bpm = t.trim().replaceCharacter (',', '.').getDoubleValue();
            if (std::abs (bpm - estat.projecte.tempo) < 0.001) return;
            estat.desfes.abans (estat.projecte);
            const double abans = estat.projecte.tempo;
            midi::canviaTempo (estat.projecte, bpm);   // els clips MIDI queden als mateixos compassos
            servei.motor().tempoClic = bpm;
            estat.canviat ("Tempo de "_ca + juce::String (abans, 0) + " a " + juce::String (bpm, 0) + " bpm");
        }, juce::String (estat.projecte.tempo, 0));
    DialegText::mostra (*this, dialeg);
}

void PantallaEstudi::novaVersio()
{
    desaAra();
    const auto actual = dibuix::utf8 (estat.projecte.nom);
    const auto carpeta = estat.carpeta;
    dialeg = std::make_unique<DialegText> ("Nova versió a partir d'aquí"_ca,
        "Es desa una còpia del projecte tal com és ara, a la mateixa carpeta (comparteix els àudios). Després s'obre la versió nova."_ca,
        "Crea-la"_ca,
        [carpeta] (const juce::String& t) -> juce::String
        {
            const auto v = disc::validaNom (t.trim().toStdString());
            if (! v.correcte) return dibuix::utf8 (v.missatge);
            if (carpeta.getChildFile (t.trim() + ".mibase").exists()) return "Ja hi ha una versió amb aquest nom."_ca;
            return {};
        },
        [this, actual] (const juce::String& t)
        {
            const auto r = disc::novaVersio (dibuix::cami (estat.fitxer), estat.projecte, t.trim().toStdString(),
                                             dibuix::araIso().toStdString(), dibuix::araText().toStdString());
            if (! r.correcte)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No s'ha creat la versió"_ca, dibuix::utf8 (r.error));
                return;
            }
            historial.afegeix ("Nova versió: "_ca + t.trim() + " (a partir de " + actual + ")");
            const auto f = dibuix::fitxer (r.fitxer);
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PantallaEstudi> (this), f] { if (sp && sp->onObre) sp->onObre (f); });
        }, actual + " v2");
    DialegText::mostra (*this, dialeg);
}

void PantallaEstudi::exportaProjecte()
{
    desaAra();
    selector = std::make_unique<juce::FileChooser> ("On vols exportar el projecte? (s'hi crearà una carpeta)"_ca,
                                                    juce::File::getSpecialLocation (juce::File::userDesktopDirectory));
    selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                           [this] (const juce::FileChooser& fc)
    {
        const auto desti = fc.getResult();
        if (! desti.isDirectory()) return;
        const auto r = disc::recopila (dibuix::cami (estat.carpeta), estat.projecte, dibuix::cami (desti),
                                       dibuix::araIso().toStdString(), dibuix::araText().toStdString());
        if (! r.correcte)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No s'ha pogut exportar"_ca, dibuix::utf8 (r.error));
            return;
        }
        juce::String text = "S'ha creat la carpeta amb el projecte i només els àudios que fa servir:\n"_ca + dibuix::fitxer (r.fitxer).getParentDirectory().getFullPathName();
        for (auto& a : r.avisos) text << "\n" << dibuix::utf8 (a);
        disc::afegeixEvolucio (dibuix::cami (estat.carpeta), dibuix::araText().toStdString(),
                               ("Exportat a " + dibuix::fitxer (r.fitxer).getParentDirectory().getFullPathName()).toStdString());
        historial.afegeix ("Projecte exportat a "_ca + dibuix::fitxer (r.fitxer).getParentDirectory().getFullPathName());
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Projecte exportat", text);
        dibuix::fitxer (r.fitxer).revealToUser();
    });
}

void PantallaEstudi::importaDeMenu()
{
    demanaImportar (estat.pistaTriada, juce::jmax (0.0, ed::enganxa (estat.projecte, estat.capcal, estat.projecte.snap)));
}

void PantallaEstudi::demanaImportar (int pista, double on)
{
    selector = std::make_unique<juce::FileChooser> ("Importa àudio"_ca, juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                    "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                               | juce::FileBrowserComponent::canSelectMultipleItems,
                           [this, pista, on] (const juce::FileChooser& fc)
    {
        juce::StringArray fitxers;
        for (auto& f : fc.getResults()) fitxers.add (f.getFullPathName());
        if (! fitxers.isEmpty()) importa (fitxers, pista, on);
    });
}

juce::File PantallaEstudi::audios() const
{
    return estat.carpeta.getChildFile (juce::String::fromUTF8 (disc::carpetaAudios));
}

juce::File PantallaEstudi::fitxerPresa (const Presa& s) const
{
    return dibuix::fitxer (disc::camiRelatiu (dibuix::cami (estat.carpeta), s.fitxer));
}

void PantallaEstudi::carregaOna (const Presa& s)
{
    auto r = llegeixPresa (formats, fitxerPresa (s), progresOna, nullptr);
    if (r.ona == nullptr)
    {
        estat.ones.erase (s.id);
        audioPreses.erase (s.id);
        return;
    }
    estat.ones[s.id] = std::move (r.ona);
    audioPreses[s.id] = std::move (r.audio);
}

bool PantallaEstudi::importaUn (const juce::File& f, int& pista, double on, double& final, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> lector (formats.createReaderFor (f));
    if (lector == nullptr || lector->sampleRate <= 0.0 || lector->lengthInSamples <= 0)
    {
        error = "«"_ca + f.getFileName() + "» no és un àudio que Mibase sàpiga llegir."_ca;
        return false;
    }

    if (pista < 0 || pista >= estat.numPistes())
    {
        auto base = f.getFileNameWithoutExtension().substring (0, 40).trim();
        if (base.isEmpty()) base = juce::String::fromUTF8 ("Àudio");
        const auto nom = ed::nomPistaLliure (estat.projecte, base.toStdString());
        auto& nova = ed::afegeixPista (estat.projecte, nom, "audio");
        nova.canals = lector->numChannels > 1 ? 2 : 1;
        pista = ed::pistaPerId (estat.projecte, nova.id);   // 0.34
    }

    auto& t = estat.projecte.pistes[(size_t) pista];
    const double durada = (double) lector->lengthInSamples / lector->sampleRate;
    lector.reset();
    const auto idPresa = copiaAlPool (f, dibuix::utf8 (t.nom), error);   // 0.85: la mateixa còpia que fa el Pool
    if (idPresa.empty()) return false;
    const auto nomDesti = juce::File (fitxerPresa (*ed::presa (estat.projecte, idPresa))).getFileName();
    ed::afegeixClip (estat.projecte, pista, idPresa, on);
    final = on + durada;
    disc::afegeixEvolucio (dibuix::cami (estat.carpeta), dibuix::araText().toStdString(),
                           ("Importat «"_ca + f.getFileName() + "» a la pista «"_ca + dibuix::utf8 (t.nom) + "» ("_ca + nomDesti + ")").toStdString());
    return true;
}

void PantallaEstudi::importa (const juce::StringArray& fitxers, int pista, double on)
{
    const auto copia = estat.projecte;
    juce::StringArray errors;
    int fets = 0;
    double posicio = juce::jmax (0.0, on);
    int ultimaPista = pista;

    for (auto& cami : fitxers)
    {
        int p = pista;
        double final = posicio;
        juce::String error;
        if (importaUn (juce::File (cami), p, posicio, final, error))
        {
            ++fets;
            ultimaPista = p;
            if (pista >= 0)
            {
                const double compas = ed::segonsPerCompas (estat.projecte);
                posicio = std::ceil (final / compas - 1e-6) * compas;
            }
        }
        else
            errors.add (error);
    }

    if (fets > 0)
    {
        estat.desfes.abans (copia);
        estat.pistaTriada = ultimaPista;
        estat.capTriat();
        estat.canviat();
        historial.afegeix ("Importats "_ca + juce::String (fets) + " fitxers d'àudio.");
    }
    if (! errors.isEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Algun fitxer no s'ha pogut importar", errors.joinIntoString ("\n"));
}

bool PantallaEstudi::renderitzaPista (const mibase::Pista& t, bool ambEfectes, double ini, double fi, int canals, const juce::File& desti,
                                      double minFi, long long& frames, std::string& error, double escalfa, float* pic)
{
    tancaments::pas ("renderitzant la pista «" + dibuix::utf8 (t.nom) + "»");
    desti.deleteFile();   // (si es torna a provar: juce afegiria al final del fitxer vell)
    // 0.81/0.82: la pista sola a un fitxer. Sense fader, panorama, enviaments, busos ni màster.
    // ambEfectes = false: en sec (només el Vocal Editor, que ja és la veu corregida); true: amb els efectes i l'instrument.
    const double fs = servei.estudi().getFs() > 0.0 ? servei.estudi().getFs() : 48000.0;
    instruments.desaEstats (estat.projecte);   // 0.88
    mibase::Projecte q = estat.projecte;
    auto sola = t;
    if (! ambEfectes)
        sola.inserts.erase (std::remove_if (sola.inserts.begin(), sola.inserts.end(), [] (const mibase::Insert& x) { return ! mibase::esInsertVeu (x); }),
                            sola.inserts.end());
    // l'automatització dels efectes i de l'instrument sí (és part del so); la de volum i panorama, no
    sola.autos.erase (std::remove_if (sola.autos.begin(), sola.autos.end(), [ambEfectes] (const mibase::Automatitzacio& a)
                                      { return ! ambEfectes || a.objectiu == "volum" || a.objectiu == "pan"; }), sola.autos.end());
    sola.enviaments.clear(); sola.sortida.clear(); sola.carpeta.clear();
    for (auto& x : sola.inserts) x.sidechain.clear();   // les altres pistes no hi són
    sola.mut = sola.solo = sola.grava = false;
    sola.faderDb = 0.0; sola.pan = 0.0;
    q.pistes = { sola };
    q.carpetes.clear();
    q.insertsMaster.clear();
    q.masterDb = 0.0;
    q.clic = false;
    q.estructuraActiva.clear();
    q.bucle.actiu = false;

    const auto temporal = desti.getSiblingFile (desti.getFileName() + ".tmp.wav");
    sincronitzaElastic (true);   // 0.86
    auto interns = Instruments::perExportar (fs, 4096, estat.carpeta);
    bool fet = mibase::exportaMescla (*construeixPla (q, audioPreses,
                                          [this, fs, interns, escalfa] (const mibase::Pista& x) { return GestorPlugins::esVst3 (x.instrument) ? plugins.instrumentNou (x, fs, 4096, escalfa) : interns (x); },
                                          [this, fs, ambEfectes] (const mibase::Insert& x) { return ambEfectes && x.actiu ? plugins.efecteNou (x, fs, 4096) : nullptr; },
                                          &mapaSo),
                                      fs, ini, fi, dibuix::cami (temporal), error);
    frames = 0;
    if (fet)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> lector (fm.createReaderFor (temporal));
        fet = lector != nullptr && lector->lengthInSamples > 0;
        if (fet)
        {
            frames = lector->lengthInSamples;
            juce::AudioBuffer<float> b ((int) lector->numChannels, (int) frames);
            lector->read (&b, 0, (int) frames, 0, true, true);
            lector.reset();
            if (pic != nullptr) *pic = b.getMagnitude (0, (int) frames);
            // la cua dels efectes (reverb, delay): es talla on ja no se sent, però mai abans de «minFi»
            const auto minim = (long long) std::ceil ((minFi - ini) * fs);
            long long ultim = frames;
            while (ultim > minim && ultim > 1)
            {
                bool silenci = true;
                for (int ch = 0; ch < b.getNumChannels() && silenci; ++ch) silenci = std::abs (b.getSample (ch, (int) ultim - 1)) < 3.0e-5f;
                if (! silenci) break;
                --ultim;
            }
            frames = juce::jmin (frames, ultim + (long long) (0.05 * fs));
            std::unique_ptr<juce::OutputStream> sortida = desti.createOutputStream();
            juce::WavAudioFormat wav;
            auto escriptor = sortida != nullptr ? wav.createWriterFor (sortida, juce::AudioFormatWriterOptions{}
                                                                                .withSampleRate (fs).withNumChannels (canals).withBitsPerSample (24))
                                                : nullptr;
            fet = escriptor != nullptr && escriptor->writeFromAudioSampleBuffer (b, 0, (int) frames);
            if (! fet) error = "No s'ha pogut escriure el fitxer nou.";
        }
        else error = "No s'ha pogut llegir el que s'havia renderitzat.";
    }
    temporal.deleteFile();
    if (! fet) desti.deleteFile();   // res a mitges
    tancaments::pas ("render de la pista acabat");
    return fet;
}

void PantallaEstudi::consolidaComp (int pista)
{
    // 0.81: el comp (els trossos triats de les preses) en un àudio nou, en sec, en una pista amb els mateixos efectes.
    const auto* t = estat.pista (pista);
    if (t == nullptr || mibase::esMidi (*t) || mibase::esBus (*t) || mibase::esAcords (*t)) return;
    const auto rg = ed::rangComp (*t);
    if (rg.second - rg.first < ed::longitudMinima)
    {
        estat.info = "Consolida el comp: la pista no té res que soni"_ca;
        estat.avisa();
        return;
    }
    const double fs = servei.estudi().getFs() > 0.0 ? servei.estudi().getFs() : 48000.0;
    const double marge = juce::jmin (0.25, rg.first);   // una mica de silenci al davant (el clip comença on toca)
    const double ini = rg.first - marge, fi = rg.second + 0.02;
    auto carpetaAudios = audios();
    carpetaAudios.createDirectory();
    const auto nomDesti = dibuix::utf8 (disc::nomPresa (dibuix::cami (carpetaAudios), "Comp - " + t->nom));
    const auto desti = carpetaAudios.getChildFile (nomDesti);
    long long frames = 0;
    std::string error;
    juce::MouseCursor::showWaitCursor();
    const bool fet = renderitzaPista (*t, false, ini, fi, t->canals == 2 ? 2 : 1, desti, fi, frames, error, 0.0, nullptr);
    juce::MouseCursor::hideWaitCursor();
    if (! fet)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Consolida el comp", "No s'ha pogut fer:\n"_ca + dibuix::utf8 (error));
        return;
    }
    estat.desfes.abans (estat.projecte);   // un sol pas: pista nova + original silenciada
    const auto nomPista = t->nom;
    const double durada = (double) frames / fs;
    const auto idPresa = ed::afegeixPresa (estat.projecte, (juce::String::fromUTF8 (disc::carpetaAudios) + "/" + nomDesti).toStdString(),
                                           desti.getFileNameWithoutExtension().toStdString(), durada).id;
    const int nova = ed::creaPistaComp (estat.projecte, pista, idPresa, ini, marge, juce::jmin (durada, fi - ini));
    carregaOna (*ed::presa (estat.projecte, idPresa));
    estat.recalculaFiles();
    estat.pistaTriada = nova;
    estat.capTriat();
    estat.info = "Comp consolidat: «Comp - "_ca + dibuix::utf8 (nomPista) + "» (l'original queda silenciada; desfés amb Ctrl+Z)"_ca;
    historial.afegeix ("Comp consolidat de «"_ca + dibuix::utf8 (nomPista) + "»: "_ca + nomDesti);
    estat.canviat ("Comp consolidat de «"_ca + dibuix::utf8 (nomPista) + "» → «Comp - "_ca + dibuix::utf8 (nomPista) + "»"_ca);
}

void PantallaEstudi::printPista (int pista)
{
    // 0.82: el so de la pista amb tots els seus efectes (i l'instrument, si és MIDI) en un àudio nou; la pista nova no té
    // cap efecte, però sí el mateix fader, panorama, sortida i enviaments. L'original queda silenciada.
    const auto* t = estat.pista (pista);
    if (t == nullptr || mibase::esBus (*t) || mibase::esAcords (*t)) return;
    const auto rg = ed::rangClips (*t);
    if (rg.second - rg.first < ed::longitudMinima)
    {
        estat.info = "Print: la pista no té res que soni"_ca;
        estat.avisa();
        return;
    }
    const double fs = servei.estudi().getFs() > 0.0 ? servei.estudi().getFs() : 48000.0;
    const double marge = juce::jmin (0.25, rg.first);
    const double ini = rg.first - marge;
    const double fi = rg.second + (mibase::esMidi (*t) ? 1.0 : 0.02);   // les notes MIDI s'apaguen amb una mica de cua
    const double cua = 8.0;                                                // reverbs i delays: fins a 8 s (es talla el silenci)
    auto carpetaAudios = audios();
    carpetaAudios.createDirectory();
    const auto nomDesti = dibuix::utf8 (disc::nomPresa (dibuix::cami (carpetaAudios), "Print - " + t->nom));
    const auto desti = carpetaAudios.getChildFile (nomDesti);
    long long frames = 0;
    std::string error;
    desaAra();
    juce::MouseCursor::showWaitCursor();
    // 0.83: un instrument VST3 (Kontakt…) es deixa carregar uns segons; si tot i així surt en silenci, es torna a provar
    // esperant més (les llibreries grans triguen)
    const bool vst3 = GestorPlugins::esVst3 (t->instrument);
    float pic = 0.0f;
    bool fet = renderitzaPista (*t, true, ini, fi + cua, 2, desti, fi, frames, error, vst3 ? GestorPlugins::escalfamentVst3 : 0.0, &pic);
    if (fet && vst3 && pic < 1.0e-4f)
    {
        historial.afegeix ("Print: l'instrument ha sortit en silenci; es torna a provar esperant que carregui més estona"_ca);
        fet = renderitzaPista (*t, true, ini, fi + cua, 2, desti, fi, frames, error, 20.0, &pic);
    }
    juce::MouseCursor::hideWaitCursor();
    if (fet && vst3 && pic < 1.0e-4f)
    {
        desti.deleteFile();
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Print",
            "L'instrument («"_ca + dibuix::utf8 (t->nom) + "») ha sortit en silenci fins i tot esperant 20 segons que carregués. No s'ha creat res.\n\n"
            "Copia el diagnòstic i passa'l a Claude."_ca);
        return;
    }
    if (! fet)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Print", "No s'ha pogut fer el Print:\n"_ca + dibuix::utf8 (error));
        return;
    }
    estat.desfes.abans (estat.projecte);   // un sol pas: pista nova + original silenciada
    const auto nomPista = t->nom;
    bool ambSidechain = false;
    for (const auto& x : t->inserts) ambSidechain |= ! x.sidechain.empty();
    const double durada = (double) frames / fs;
    const auto idPresa = ed::afegeixPresa (estat.projecte, (juce::String::fromUTF8 (disc::carpetaAudios) + "/" + nomDesti).toStdString(),
                                           desti.getFileNameWithoutExtension().toStdString(), durada).id;
    const int nova = ed::creaPistaPrint (estat.projecte, pista, idPresa, ini, marge, durada);
    carregaOna (*ed::presa (estat.projecte, idPresa));
    estat.recalculaFiles();
    estat.pistaTriada = nova;
    estat.capTriat();
    estat.info = "Print: «Print - "_ca + dibuix::utf8 (nomPista) + "» amb els efectes aplicats (l'original queda silenciada; desfés amb Ctrl+Z)"_ca
               + (ambSidechain ? juce::String (" · Compte: el sidechain d'altres pistes no s'aplica al Print") : juce::String());
    historial.afegeix ("Print de «"_ca + dibuix::utf8 (nomPista) + "»: "_ca + nomDesti);
    estat.canviat ("Print de «"_ca + dibuix::utf8 (nomPista) + "» → «Print - "_ca + dibuix::utf8 (nomPista) + "» (amb efectes)"_ca);
}

void PantallaEstudi::consolida (int pista, int ia, int ib)
{
    // Ajunta dos clips en un fitxer nou: es llegeix el que sona de cada un (amb fades i volum), es barreja i es desa.
    auto* t = estat.pista (pista);
    if (t == nullptr) return;
    const auto a = t->clips[(size_t) ia], b = t->clips[(size_t) ib];

    double sr = 0.0;
    int canals = 1;
    for (const auto* c : { &a, &b })
    {
        if (c->presa.empty()) continue;
        auto it = estat.ones.find (c->presa);
        if (it == estat.ones.end() || it->second == nullptr || it->second->lector == nullptr)
        {
            estat.info = "No es troba l'àudio d'un dels clips"_ca;
            estat.avisa();
            return;
        }
        if (sr > 0.0 && std::abs (sr - it->second->sr) > 0.5)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No es poden enganxar",
                                                    "Els dos àudios tenen freqüències de mostreig diferents."_ca);
            return;
        }
        sr = it->second->sr;
        canals = juce::jmax (canals, it->second->canals > 1 ? 2 : 1);
    }
    if (sr <= 0.0) return;

    const double ini = juce::jmin (a.iniciVisible(), b.iniciVisible());
    const double fi = juce::jmax (a.fiVisible(), b.fiVisible());
    const int total = (int) std::ceil ((fi - ini) * sr);
    juce::AudioBuffer<float> mescla (canals, total);
    mescla.clear();

    sincronitzaElastic (true);   // 0.86
    for (const auto* c : { &a, &b })
    {
        if (c->presa.empty() || c->mut) continue;
        if (mibase::elastic::actiu (*c))
        {
            // 0.86: un clip amb Elastic Audio: es llegeix el so estirat (en temps del clip).
            const bool deVeu = mibase::veuActiva (*t) && serveiVeu.corregides().count (c->presa) > 0;
            auto it = mapaSo.find (mibase::elastic::clau (*c, deVeu));
            if (it != mapaSo.end() && it->second != nullptr && std::abs (it->second->fs - sr) < 0.5)
            {
                const auto& so = *it->second;
                const double guany = std::pow (10.0, c->guanyDb / 20.0);
                const double fa = juce::jmax (c->inici, 0.0), fb = juce::jmin (c->fi, so.durada());
                for (long long i = (long long) std::round (fa * sr); i < (long long) std::round (fb * sr); ++i)
                {
                    const double tf = (double) i / sr;
                    double env = 1.0;
                    if (c->fadeIn > 0.0 && tf < c->inici + c->fadeIn) env = juce::jmax (0.0, (tf - c->inici) / c->fadeIn);
                    if (c->fadeOut > 0.0 && tf > c->fi - c->fadeOut) env = juce::jmin (env, juce::jmax (0.0, (c->fi - tf) / c->fadeOut));
                    const int k = (int) std::round ((c->pos + tf - ini) * sr);
                    if (k < 0 || k >= total || i >= so.longitud) continue;
                    for (int ch = 0; ch < canals; ++ch)
                        mescla.addSample (ch, k, (float) (so.dades[juce::jmin (ch, so.canals - 1)][(size_t) i] * guany * env));
                }
                continue;
            }
        }
        auto& ona = *estat.ones[c->presa];
        const double guany = std::pow (10.0, c->guanyDb / 20.0);
        // Tros del fitxer que sona: [max(inici, 0), min(fi, durada)].
        const double fa = juce::jmax (c->inici, 0.0), fb = juce::jmin (c->fi, (double) ona.longitud / sr);
        if (fb <= fa) continue;
        const auto s0 = (juce::int64) std::round (fa * sr);
        const int n = (int) std::round ((fb - fa) * sr);
        juce::AudioBuffer<float> llegit ((int) juce::jmax (1u, ona.lector->numChannels), n);
        ona.lector->read (&llegit, 0, n, s0, true, true);
        const int desti0 = (int) std::round ((c->pos + fa - ini) * sr);
        for (int i = 0; i < n; ++i)
        {
            const double tf = fa + (double) i / sr;
            double env = 1.0;
            if (c->fadeIn > 0.0 && tf < c->inici + c->fadeIn) env = juce::jmax (0.0, (tf - c->inici) / c->fadeIn);
            if (c->fadeOut > 0.0 && tf > c->fi - c->fadeOut) env = juce::jmin (env, juce::jmax (0.0, (c->fi - tf) / c->fadeOut));
            const int k = desti0 + i;
            if (k < 0 || k >= total) continue;
            for (int ch = 0; ch < canals; ++ch)
            {
                const int origen = juce::jmin (ch, llegit.getNumChannels() - 1);
                mescla.addSample (ch, k, (float) (llegit.getSample (origen, i) * guany * env));
            }
        }
    }

    auto carpetaAudios = audios();
    carpetaAudios.createDirectory();
    const auto nomDesti = dibuix::utf8 (disc::nomPresa (dibuix::cami (carpetaAudios), t->nom));
    const auto desti = carpetaAudios.getChildFile (nomDesti);
    {
        std::unique_ptr<juce::OutputStream> sortida = desti.createOutputStream();
        juce::WavAudioFormat wav;
        auto escriptor = sortida != nullptr ? wav.createWriterFor (sortida, juce::AudioFormatWriterOptions{}
                                                                            .withSampleRate (sr).withNumChannels (canals).withBitsPerSample (24))
                                            : nullptr;
        if (escriptor == nullptr || ! escriptor->writeFromAudioSampleBuffer (mescla, 0, total))
        {
            escriptor.reset();
            desti.deleteFile();
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No es poden enganxar", "No s'ha pogut escriure el fitxer nou.");
            return;
        }
    }

    estat.desfes.abans (estat.projecte);
    const auto* sa = ed::presa (estat.projecte, a.presa);
    const auto* sb = ed::presa (estat.projecte, b.presa);
    const auto nomPresa = juce::String ("Enganxat: ") + (sa ? dibuix::utf8 (sa->nom) : juce::String ("buida")) + " + "
                        + (sb ? dibuix::utf8 (sb->nom) : juce::String ("buida"));
    const auto idPresa = ed::afegeixPresa (estat.projecte, (juce::String::fromUTF8 (disc::carpetaAudios) + "/" + nomDesti).toStdString(),
                                           nomPresa.toStdString(), (double) total / sr).id;
    ed::treuClips (estat.projecte, pista, { ia, ib });
    ed::afegeixClip (estat.projecte, pista, idPresa, ini);
    carregaOna (*ed::presa (estat.projecte, idPresa));
    estat.triaClip (pista, (int) estat.projecte.pistes[(size_t) pista].clips.size() - 1);
    estat.info = "Enganxats en un fitxer nou: "_ca + nomDesti;
    estat.canviat ("Enganxats dos clips a «"_ca + dibuix::utf8 (estat.projecte.pistes[(size_t) pista].nom) + "» en un fitxer nou ("_ca + nomDesti + ")");
}

//==============================================================================
void PantallaEstudi::duplica()
{
    if (estat.triats.empty()) return;
    estat.desfes.abans (estat.projecte);
    estat.triats = ed::duplicaTriats (estat.projecte, estat.triats);
    estat.canviat();
}

void PantallaEstudi::treu()
{
    if (estat.rang.actiu)
    {
        // Buida el tros triat.
        estat.desfes.abans (estat.projecte);
        for (int i = estat.rang.pistaDe; i <= estat.rang.pistaFins; ++i)
            ed::buidaRang (estat.projecte, i, estat.rang.t0, estat.rang.t1);
        estat.info = "Tros buidat";
        estat.capTriat();
        estat.canviat();
        return;
    }
    if (estat.triats.empty()) return;
    estat.desfes.abans (estat.projecte);
    ed::treuTriats (estat.projecte, estat.triats);
    estat.capTriat();
    estat.canviat();
}

void PantallaEstudi::copia (bool retalla)
{
    if (estat.rang.actiu)
        estat.portaretalls = ed::copiaRang (estat.projecte, estat.rang.pistaDe, estat.rang.pistaFins, estat.rang.t0, estat.rang.t1);
    else if (! estat.triats.empty())
        estat.portaretalls = ed::copiaClips (estat.projecte, estat.triats);
    else
    {
        estat.info = "No hi ha res triat per copiar";
        estat.avisa();
        return;
    }
    const auto n = (int) estat.portaretalls.clips.size();
    estat.info = (retalla ? "Retallat: "_ca : "Copiat: "_ca) + juce::String (n) + (n == 1 ? " clip" : " clips")
               + " · Ctrl+V l'enganxa al capçal"_ca;
    if (retalla) treu();
    else estat.avisa();
}

void PantallaEstudi::enganxa()
{
    if (estat.portaretalls.buit())
    {
        estat.info = "No hi ha res copiat (Ctrl+C)";
        estat.avisa();
        return;
    }
    estat.desfes.abans (estat.projecte);
    estat.triats = ed::enganxaRetall (estat.projecte, estat.portaretalls, estat.capcal, estat.insereix);
    estat.rang.actiu = false;
    if (! estat.triats.empty()) estat.pistaTriada = estat.triats.back().pista;
    estat.info = estat.insereix ? "Enganxat inserint: el que hi havia s'ha apartat"_ca : "Enganxat trepitjant el que hi havia"_ca;
    estat.canviat();
}

void PantallaEstudi::canviaModeRang (bool rang)
{
    estat.modeRang = rang;
    ajustos.setValue ("estudi.modeRang", rang);
    estat.info = rang ? "Requadre: només el que queda dins"_ca : "Requadre: clips sencers"_ca;
    estat.avisa();
}

void PantallaEstudi::canviaInsereix (bool ins)
{
    estat.insereix = ins;
    ajustos.setValue ("estudi.insereix", ins);
    estat.info = ins ? "Enganxar: insereix (aparta el que hi ha després del capçal, a totes les pistes)"_ca
                     : "Enganxar: trepitja el que hi ha"_ca;
    estat.avisa();
}

void PantallaEstudi::silenciaTriat()
{
    estat.desfes.abans (estat.projecte);
    if (! estat.triats.empty())
    {
        const bool nou = ! estat.clip (estat.triats[0])->mut;
        for (auto& r : estat.triats) if (auto* c = estat.clip (r)) c->mut = nou;
        estat.info = nou ? "Silenciat" : "Sonant";
    }
    else if (const auto v = estat.indexsTriades(); v.size() > 1)
    {
        // Diverses pistes triades (o una carpeta): totes alhora.
        bool totes = true;
        for (int i : v) totes &= estat.projecte.pistes[(size_t) i].mut;
        for (int i : v) estat.projecte.pistes[(size_t) i].mut = ! totes;
        estat.info = ! totes ? "Pistes silenciades" : "Pistes sonant";
    }
    else if (auto* t = estat.pista (estat.pistaTriada))
    {
        t->mut = ! t->mut;
        estat.info = t->mut ? "Pista silenciada" : "Pista sonant";
    }
    else return;
    estat.canviat();
}

void PantallaEstudi::demanaNomCarpeta (std::vector<int> v, const std::string& idACanviar)
{
    auto* existent = ed::carpeta (estat.projecte, idACanviar);
    const auto inicial = existent != nullptr ? dibuix::utf8 (existent->nom)
                                             : "Carpeta "_ca + juce::String ((int) estat.projecte.carpetes.size() + 1);
    dialeg = std::make_unique<DialegText> (existent != nullptr ? "Canvia el nom de la carpeta"_ca : "Carpeta nova"_ca,
        existent != nullptr ? juce::String() : (v.empty() ? "Després hi podràs arrossegar pistes."_ca
                                                          : juce::String ((int) v.size()) + (v.size() == 1 ? " pista hi anirà."_ca : " pistes hi aniran."_ca)),
        existent != nullptr ? "Canvia"_ca : "Crea"_ca,
        [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom"_ca : juce::String(); },
        [this, v, idACanviar] (const juce::String& t)
        {
            estat.desfes.abans (estat.projecte);
            const auto nom = t.trim().toStdString();
            if (auto* c = ed::carpeta (estat.projecte, idACanviar)) c->nom = nom;
            else
            {
                ed::creaCarpeta (estat.projecte, nom, v);
                estat.panellCarpetes = true;
                colocaSeccions();
            }
            estat.canviat ("Carpeta «"_ca + t.trim() + "»");
        }, inicial);
    DialegText::mostra (*this, dialeg);
}

void PantallaEstudi::fesCrossfade()
{
    // 0.14: X amb un tros triat (requadre «només el tros») on dos clips d'àudio se sobreposen.
    auto copia = estat.projecte;
    int fets = 0;
    for (int i = estat.rang.pistaDe; i <= estat.rang.pistaFins; ++i)
        if (ed::crossfade (copia, i, estat.rang.t0, estat.rang.t1)) ++fets;
    if (fets == 0)
    {
        estat.info = "Per fer un crossfade, tria (en Lliure) el tros on se sobreposen dos clips d'àudio i prem X"_ca;
        estat.avisa();
        return;
    }
    const int ms = juce::roundToInt ((estat.rang.t1 - estat.rang.t0) * 1000.0);
    estat.desfes.abans (estat.projecte);
    estat.projecte = std::move (copia);
    estat.rang = {};
    estat.canviat ("Crossfade de "_ca + juce::String (ms) + " ms");
}

void PantallaEstudi::commutaBucle()
{
    auto& b = estat.projecte.bucle;
    estat.desfes.abans (estat.projecte);
    if (! b.marcat)
    {
        const int c = ed::compasA (estat.projecte, estat.capcal);
        ed::marcaBucle (estat.projecte, c, c + 3);
        estat.projecte.bucle.marcat = estat.projecte.bucle.actiu = true;
    }
    else b.actiu = ! b.actiu;
    estat.info = b.actiu ? "Bucle encès"_ca : "Bucle apagat"_ca;
    estat.canviat();
}

void PantallaEstudi::zoomA (double a, double b, int pistaDe, int pistaFins)
{
    if (b <= a || pistes.getWidth() <= 0) return;
    estat.pxPerSegon = juce::jlimit (2.0, 800000.0, pistes.getWidth() * 0.94 / (b - a));
    estat.iniciVista = juce::jmax (0.0, a - (pistes.getWidth() * 0.03) / estat.pxPerSegon);
    const int n = juce::jmax (1, pistaFins - pistaDe + 1);
    estat.altPista = juce::jlimit (VistaPistes::alcadaMinima, 260, (pistes.getHeight() - 8) / n);
    estat.desplY = 0;
    estat.desplY = juce::jmax (0, estat.yPista (pistaDe));
    actualitzaBarres();
    estat.avisa();
}

void PantallaEstudi::zoomTot()
{
    const double fi = juce::jmax (ed::final (estat.projecte), ed::segonsPerCompas (estat.projecte) * 4.0);
    zoomA (0.0, fi, 0, estat.numPistes() - 1);
    estat.info = "Tot el projecte";
}

void PantallaEstudi::zoomTriat()
{
    if (estat.rang.actiu)
    {
        zoomA (estat.rang.t0, estat.rang.t1, estat.rang.pistaDe, estat.rang.pistaFins);
        return;
    }
    if (estat.triats.empty()) { estat.info = "No hi ha res triat"; estat.avisa(); return; }
    double a = 1e300, b = -1e300;
    int pa = estat.numPistes(), pb = 0;
    for (auto& r : estat.triats)
        if (auto* c = estat.clip (r))
        {
            a = juce::jmin (a, c->iniciVisible());
            b = juce::jmax (b, c->fiVisible());
            pa = juce::jmin (pa, r.pista);
            pb = juce::jmax (pb, r.pista);
        }
    zoomA (a, b, pa, pb);
}

void PantallaEstudi::triaColor()
{
    const bool clips = ! estat.triats.empty();
    if (! clips && estat.pista (estat.pistaTriada) == nullptr)
    {
        estat.info = "Tria una pista o uns clips per pintar-los";
        estat.avisa();
        return;
    }
    const auto actual = clips ? estat.colorClip (estat.triats[0].pista, *estat.clip (estat.triats[0]))
                              : EstatEstudi::colorPista (*estat.pista (estat.pistaTriada));
    const auto que = clips ? (estat.triats.size() == 1 ? juce::String ("el clip triat") : juce::String (estat.triats.size()) + " clips triats")
                           : "la pista «"_ca + dibuix::utf8 (estat.pista (estat.pistaTriada)->nom) + "»"_ca;
    auto primer = std::make_shared<bool> (true);
    auto quins = estat.triats;
    const int pista = estat.pistaTriada;
    auto panellColor = std::make_unique<PanellColor> (actual, que, [this, primer, quins, pista, clips] (juce::Colour c)
    {
        if (*primer) { estat.desfes.abans (estat.projecte); *primer = false; }   // un sol Desfés per a tota la finestreta
        if (clips) { for (auto& r : quins) if (auto* k = estat.clip (r)) k->rgb = c.getARGB(); }
        else if (auto* t = estat.pista (pista)) t->rgb = c.getARGB();
        estat.canviat();
    });
    juce::CallOutBox::launchAsynchronously (std::move (panellColor), btColor.getScreenBounds(), nullptr);
}

void PantallaEstudi::desfes (bool refer)
{
    const bool fet = refer ? estat.desfes.refes (estat.projecte) : estat.desfes.desfes (estat.projecte);
    if (! fet) return;
    plugins.restauraRacks (estat.projecte);   // 0.78: el Vocal Studio viu, com el del projecte
    for (auto& s : estat.projecte.preses)
        if (estat.ones.find (s.id) == estat.ones.end())
            carregaOna (s);
    estat.info = refer ? "Refet" : "Desfet";
    estat.canviat();
}

bool PantallaEstudi::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const int codi = k.getKeyCode();

    const bool cap = ! mods.isAnyModifierKeyDown();
    auto lletra = [codi] (char c) { return codi == c || codi == c + 32; };
    if (codi == juce::KeyPress::deleteKey || codi == juce::KeyPress::backspaceKey) { treu(); return true; }
    if (mods.isCommandDown() && lletra ('C')) { copia (false); return true; }
    if (mods.isCommandDown() && lletra ('X')) { copia (true); return true; }
    if (cap && lletra ('X') && estat.rang.actiu) { fesCrossfade(); return true; }
    if (mods.isCommandDown() && lletra ('V')) { enganxa(); return true; }
    if (codi == juce::KeyPress::insertKey) { canviaInsereix (! estat.insereix); return true; }
    // 0.24: / i * del teclat numèric (a Linux arriben com a «/» i «*»).
    // 0.27: a Windows amb el teclat espanyol, la «/» del teclat numèric arriba amb el codi de la tecla «-» (la del mateix
    // lloc al teclat principal) però amb el caràcter «/»: per això es mira el caràcter.
    const auto car = k.getTextCharacter();
    if (! mods.isCommandDown() && (codi == juce::KeyPress::numberPadDivide || codi == '/' || car == '/')) { commutaBucle(); return true; }
    if (! mods.isCommandDown() && (codi == juce::KeyPress::numberPadMultiply || codi == '*' || car == '*')) { commutaSoloEditor(); return true; }
    if (cap && lletra ('M')) { silenciaTriat(); return true; }
    if (mods.isCommandDown() && lletra ('G')) { demanaNomCarpeta (estat.indexsTriades(), {}); return true; }
    if (cap && lletra ('L')) { commutaBucle(); return true; }
    if (cap && lletra ('P')) { mostraPianoRoll (! estat.pianoRoll); return true; }
    if (cap && lletra ('A'))   // 0.56: l'editor d'automatització de la pista triada
    {
        if (estat.automat.obert) tancaAutomatitzacio();
        else if (estat.pista (estat.pistaTriada) != nullptr) obreAutomatitzacio (estat.pistaTriada);
        return true;
    }
    if (! mods.isCommandDown() && ! mods.isAltDown() && lletra ('Z'))
    {
        // 0.12: si el ratolí és sobre un Piano Roll, el zoom és el del Piano Roll.
        auto* sota = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
        auto* pr = sota != nullptr ? (dynamic_cast<PianoRoll*> (sota) != nullptr ? dynamic_cast<PianoRoll*> (sota) : sota->findParentComponentOfClass<PianoRoll>()) : nullptr;
        if (pr != nullptr) return pr->keyPressed (k);
    }
    if (! mods.isCommandDown() && ! mods.isAltDown() && lletra ('Z')) { if (mods.isShiftDown()) zoomTriat(); else zoomTot(); return true; }
    if (cap && lletra ('Q')) { estat.projecte.snap = Snap::Compas; estat.info = "Graella: compàs"_ca; estat.avisa(); calDesar = true; startTimer (700); return true; }
    if (cap && lletra ('W')) { estat.projecte.snap = Snap::Temps; estat.info = "Graella: temps"; estat.avisa(); calDesar = true; startTimer (700); return true; }
    if (cap && lletra ('E')) { estat.projecte.snap = Snap::Lliure; estat.info = "Graella: lliure"; estat.avisa(); calDesar = true; startTimer (700); return true; }
    if (mods.isCommandDown() && (codi == 'D' || codi == 'd')) { duplica(); return true; }
    if (mods.isCommandDown() && (codi == 'Z' || codi == 'z')) { desfes (mods.isShiftDown()); return true; }
    if (mods.isCommandDown() && (codi == 'Y' || codi == 'y')) { desfes (true); return true; }
    // 0.31: Ctrl+A tria tot i Ctrl+S ho deixa sense triar, a la finestra on és el ratolí (Piano Roll / Drumpad o taula de treball).
    // Desar: Ctrl+Maj+S (el projecte ja es desa sol a cada canvi).
    if (mods.isCommandDown() && ! mods.isShiftDown() && (lletra ('A') || lletra ('S')))
    {
        auto* sota = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
        auto* pr = sota != nullptr ? (dynamic_cast<PianoRoll*> (sota) != nullptr ? dynamic_cast<PianoRoll*> (sota) : sota->findParentComponentOfClass<PianoRoll>()) : nullptr;
        if (pr != nullptr) { if (lletra ('A')) pr->triaTot(); else pr->desseleccionaTot(); return true; }
        if (lletra ('A'))
        {
            estat.triats.clear();
            estat.rang.actiu = false;
            for (int p = 0; p < estat.numPistes(); ++p)
                for (int c = 0; c < (int) estat.projecte.pistes[(size_t) p].clips.size(); ++c)
                    estat.triats.push_back ({ p, c });
            estat.info = juce::String ((int) estat.triats.size()) + " clips triats"_ca;
        }
        else { estat.capTriat(); estat.info = "Res triat"_ca; }
        estat.avisa();
        return true;
    }
    if (mods.isCommandDown() && (codi == 'S' || codi == 's')) { desaAra(); estat.info = "Desat"; estat.avisa(); return true; }
    if (mods.isCommandDown() && (codi == 'I' || codi == 'i')) { importaDeMenu(); return true; }
    if (! mods.isAnyModifierKeyDown() && codi >= '1' && codi <= '7') { triaEina ((Eina) (codi - '1')); return true; }
    if (codi == juce::KeyPress::escapeKey) { estat.triantFantasma = -1; estat.capTriat(); triaEina (Eina::Fletxa); tancaTot(); return true; }
    if (codi == juce::KeyPress::homeKey) { transport (0); return true; }
    if (codi == juce::KeyPress::spaceKey) { if (estat.transport == 0) reprodueix(); else atura(); return true; }
    // 0.72: l'única tecla de gravar és el 0 del teclat numèric (la R ja no grava)
    if (esNumpad0 (k))
    {
        const auto ara = juce::Time::currentTimeMillis();
        const bool repeticio = ara - darrerNumpad0 < 350;   // tecla mantinguda o la mateixa pulsació repetida
        darrerNumpad0 = ara;
        if (! repeticio) gravaContextual();
        return true;
    }
    return false;
}

//==============================================================================
// Piano Roll i MIDI
//==============================================================================
void PantallaEstudi::configuraEditor (PianoRoll& pr)
{
    juce::Component::SafePointer<PianoRoll> sp (&pr);
    pr.onDirecte = [this] (int pista, std::uint8_t e, std::uint8_t d1, std::uint8_t d2)
    {
        if (auto* t = estat.pista (pista)) servei.estudi().enviaDirecte ({ slotDePista (*t), e, d1, d2 });
    };
    pr.onDesfes = [this] (bool refer) { desfes (refer); };
    pr.onSolo = [this] { commutaSoloEditor(); };
    pr.onSampleAFila = [this] (int fila, const juce::File& f) { sampleAFila (fila, f); };
    pr.onTeclaNoUsada = [this] (const juce::KeyPress& k) { return keyPressed (k); };
    pr.onDesenganxa = [this]
    {
        const bool desenganxa = finestraPianoRoll == nullptr;
        ajustos.setValue ("pianoRoll.desenganxat", desenganxa);
        desenganxaPianoRoll (desenganxa);
    };
    juce::Component::SafePointer<PantallaEstudi> jo (this);
    pr.onTanca = [jo, sp]
    {
        if (sp == nullptr) return;
        const int i = sp->getIndex();
        juce::MessageManager::callAsync ([jo, i] { if (jo != nullptr) jo->tancaEditor (i); });   // no es pot esborrar l'editor des de dins seu
    };
    pr.onVistaCanviada = [this] { zonaEditors.repintaTots(); };
    pr.onMenuInstrument = [this] (juce::Component& b) { panellHarmonia.menuInstrument (b); };   // 0.38
    pr.onMenuSo = [this] (juce::Component& b) { panellHarmonia.menuSo (b); };
    pr.onActiu = [this] { zonaEditors.repintaTots(); esquerra.repaint(); estat.avisa(); };
}

void PantallaEstudi::tancaEditor (int index)
{
    if (index < 0 || index >= (int) estat.editors.size()) return;
    estat.editors.erase (estat.editors.begin() + index);
    estat.editorActiu = juce::jlimit (0, juce::jmax (0, (int) estat.editors.size() - 1), estat.editorActiu >= index ? estat.editorActiu - 1 : estat.editorActiu);
    zonaEditors.sincronitza();
    if (estat.editors.empty()) mostraPianoRoll (false);
    else { colocaSeccions(); estat.avisa(); }
}

void PantallaEstudi::obrePianoRoll (int pista, int clip)
{
    auto* c = estat.clip ({ pista, clip });
    if (c == nullptr || ! c->midi) return;
    estat.automat.obert = false;   // 0.58: l'editor d'automatització deixa el lloc
    estat.focusEditor = true;
    estat.triaClip (pista, clip);
    int i = estat.editorDe ({ pista, clip });
    if (i < 0)
    {
        // Un editor no bloquejat (primer l'actiu); si tots ho estan, un de nou a sota.
        if (estat.editorActiu < (int) estat.editors.size() && ! estat.editors[(size_t) estat.editorActiu].bloquejat) i = estat.editorActiu;
        for (int k = 0; k < (int) estat.editors.size() && i < 0; ++k)
            if (! estat.editors[(size_t) k].bloquejat) i = k;
        if (i < 0)
        {
            if ((int) estat.editors.size() >= EstatEstudi::maxEditors)
            {
                estat.info = "Ja hi ha 3 editors bloquejats: desbloqueja'n un per obrir aquest clip"_ca;
                estat.avisa();
                return;
            }
            estat.editors.push_back ({});
            i = (int) estat.editors.size() - 1;
        }
        estat.editorMostra (i, { pista, clip });
    }
    estat.editorActiu = i;
    zonaEditors.sincronitza();
    mostraPianoRoll (true);
    if (auto* pr = zonaEditors.editor (i))
    {
        pr->actualitza();
        pr->grabKeyboardFocus();
    }
    estat.avisa();
}

void PantallaEstudi::mostraPianoRoll (bool visible)
{
    if (visible && estat.veu.obert) estat.veu.obert = false;   // 0.22: comparteixen el lloc de baix
    if (visible) estat.automat.obert = false;                     // 0.58
    if (visible && estat.sampler.obert) estat.sampler.obert = false;   // 0.26
    // 0.31: el clip a obrir: el MIDI triat; si no n'hi ha, el de la pista triada (MIDI o bateria: així P obre també el Drumpad)
    // on és el capçal, o el primer.
    mibase::edicio::RefClip quin;
    if (estat.triats.size() == 1)
        if (auto* c = estat.clip (estat.triats[0]); c != nullptr && c->midi) quin = estat.triats[0];
    if (quin.pista < 0)
        if (auto* t = estat.pista (estat.pistaTriada); t != nullptr && mibase::esMidi (*t))
        {
            for (int c = 0; c < (int) t->clips.size(); ++c)
            {
                const auto& k = t->clips[(size_t) c];
                if (! k.midi) continue;
                if (quin.pista < 0 || (estat.capcal >= k.iniciVisible() && estat.capcal < k.fiVisible())) quin = { estat.pistaTriada, c };
                if (estat.capcal >= k.iniciVisible() && estat.capcal < k.fiVisible()) break;
            }
        }
    if (visible && quin.pista < 0)
        if (auto* t = estat.pista (estat.pistaTriada); t != nullptr && mibase::esMidi (*t)
            && std::none_of (t->clips.begin(), t->clips.end(), [] (const mibase::Clip& k) { return k.midi; }))
        {
            // la pista encara no té cap clip: un d'un compàs on és el capçal, per poder-hi tocar o dibuixar ja
            estat.desfes.abans (estat.projecte);
            const double a = juce::jmax (0.0, estat.capcal);
            const int nou = ed::creaFinestra (estat.projecte, estat.pistaTriada, a, a + ed::segonsPerCompasA (estat.projecte, a));
            quin = { estat.pistaTriada, nou };
            estat.canviat ("Clip nou d'un compàs"_ca);
        }
    if (visible && estat.editors.empty())
    {
        estat.editors.push_back ({});
        if (quin.pista >= 0) estat.editorMostra (0, quin);
        estat.editorActiu = 0;
        zonaEditors.sincronitza();
    }
    else if (visible && quin.pista >= 0 && estat.editorActiu >= 0 && estat.editorActiu < (int) estat.editors.size()
             && ! estat.editors[(size_t) estat.editorActiu].bloquejat)
        estat.editorMostra (estat.editorActiu, quin);
    estat.pianoRoll = visible;
    if (visible && finestraPianoRoll == nullptr && ajustos.getBoolValue ("pianoRoll.desenganxat", false))
        desenganxaPianoRoll (true);
    else if (! visible && finestraPianoRoll != nullptr)
        desenganxaPianoRoll (false);
    colocaSeccions();
    estat.avisa();
    if (! visible) grabKeyboardFocus();
}

void PantallaEstudi::desenganxaPianoRoll (bool desenganxa)
{
    if (desenganxa)
    {
        if (finestraPianoRoll != nullptr) return;
        removeChildComponent (&zonaEditors);
        zonaEditors.setVisible (true);
        zonaEditors.setDesenganxat (true);
        juce::Component::SafePointer<PantallaEstudi> jo (this);
        finestraPianoRoll = std::make_unique<FinestraPianoRoll> (zonaEditors, [jo]
        {
            // En tancar la finestra, torna a baix (més tard: no es pot esborrar la finestra des de dins seu).
            juce::MessageManager::callAsync ([jo]
            {
                if (jo == nullptr) return;
                jo->ajustos.setValue ("pianoRoll.desenganxat", false);
                jo->desenganxaPianoRoll (false);
            });
        });
        const auto estatFinestra = ajustos.getValue ("pianoRoll.finestra");
        if (estatFinestra.isNotEmpty()) finestraPianoRoll->restoreWindowStateFromString (estatFinestra);
        estat.pianoRoll = true;
        historial.afegeix ("Piano Roll en una finestra a part"_ca);
    }
    else
    {
        if (finestraPianoRoll == nullptr) return;
        ajustos.setValue ("pianoRoll.finestra", finestraPianoRoll->getWindowStateAsString());
        finestraPianoRoll->clearContentComponent();
        finestraPianoRoll.reset();
        addChildComponent (zonaEditors);
        zonaEditors.setDesenganxat (false);
    }
    ajustos.saveIfNeeded();
    colocaSeccions();
    estat.avisa();
    if (auto* pr = zonaEditors.actiu()) pr->grabKeyboardFocus();
}

void PantallaEstudi::recullMidiGravat()
{
    MotorEstudi::MidiGravat m;
    while (servei.estudi().treuMidiGravat (m))
        gravacio.missatges.push_back (m);
}

void PantallaEstudi::acabaGravacioMidi()
{
    auto& motor = servei.estudi();
    recullMidiGravat();
    motor.setSlotGravaMidi (-1);
    const int pista = gravacio.pista;
    const double cursorRec = gravacio.cursor;
    double fins = juce::jmax (gravacio.fins, cursorRec);
    gravacio.pista = -1;
    gravacio.midi = false;
    gravacio.fins = 0.0;
    auto missatges = std::move (gravacio.missatges);
    gravacio.missatges.clear();
    estat.info.clear();

    auto* t = estat.pista (pista);
    if (t == nullptr || ! mibase::esMidi (*t)) return;

    // Notes: cada encesa amb la seva apagada (les que encara sonaven en aturar, s'acaben on s'ha aturat).
    const auto& p = estat.projecte;
    const double spt = midi::segonsPerTick (p);
    const double tick0 = midi::ticks (p, cursorRec);   // 0.28: amb el mapa de tempo
    auto tick = [&] (double tt) { return ed::teCanvisTempo (p) ? (long long) std::llround (midi::ticks (p, tt) - tick0)
                                                                : (long long) std::llround ((tt - cursorRec) / spt); };
    Clip c = midi::clipNou (cursorRec, cursorRec + ed::segonsPerCompasA (p, cursorRec));
    std::array<double, 16 * 128> obertes;
    std::array<int, 16 * 128> velocitats {};
    obertes.fill (-1.0);
    for (auto& m : missatges) fins = juce::jmax (fins, m.t);
    auto tanca = [&] (int canal, int to, double fi)
    {
        const size_t k = (size_t) (canal * 128 + to);
        if (obertes[k] < 0.0) return;
        Nota n;
        n.ini = tick (obertes[k]);
        n.dur = juce::jmax (1LL, tick (fi) - n.ini);
        n.to = to;
        n.vel = juce::jlimit (1, 127, velocitats[k]);
        n.canal = canal;
        c.notes.push_back (n);
        obertes[k] = -1.0;
    };
    for (auto& m : missatges)
    {
        const int tipus = m.estat & 0xF0, canal = m.estat & 0x0F;
        if (tipus == 0x90 && m.dada2 > 0)
        {
            tanca (canal, m.dada1 & 127, m.t);
            obertes[(size_t) (canal * 128 + (m.dada1 & 127))] = m.t;
            velocitats[(size_t) (canal * 128 + (m.dada1 & 127))] = m.dada2;
        }
        else if (tipus == 0x80 || tipus == 0x90)
            tanca (canal, m.dada1 & 127, m.t);
        else if (tipus == 0xB0 || tipus == 0xE0 || tipus == 0xD0 || tipus == 0xA0 || tipus == 0xC0)
        {
            EventMidi e;
            e.tick = tick (m.t);
            e.tipus = tipus;
            e.canal = canal;
            if (tipus == 0xE0) e.valor = (m.dada1 & 127) | ((m.dada2 & 127) << 7);
            else if (tipus == 0xD0 || tipus == 0xC0) e.valor = m.dada1;
            else { e.num = m.dada1; e.valor = m.dada2; }
            c.events.push_back (e);
        }
    }
    for (int k = 0; k < 16 * 128; ++k)
        if (obertes[(size_t) k] >= 0.0) tanca (k / 128, k % 128, fins);

    if (c.notes.empty())
    {
        estat.info = "No s'ha gravat cap nota"_ca;
        estat.avisa();
        return;
    }
    std::vector<int> cap;
    midi::ordena (c.notes, cap);
    c.fi = juce::jmax (c.fi, fins - cursorRec);
    midi::encabeix (p, c);
    c.inici = 0.0;   // el que s'ha tocat durant el compte queda amagat (no s'esborra)

    estat.desfes.abans (estat.projecte);
    auto& clips = estat.projecte.pistes[(size_t) pista].clips;
    clips.push_back (c);
    const int idx = (int) clips.size() - 1;
    const auto nomPista = dibuix::utf8 (estat.projecte.pistes[(size_t) pista].nom);
    const int quantes = (int) c.notes.size();
    estat.info = "Gravades "_ca + juce::String (quantes) + (quantes == 1 ? " nota" : " notes") + " a «"_ca + nomPista + "»"_ca;
    historial.afegeix (estat.info);
    estat.canviat ("Gravat MIDI ("_ca + juce::String (quantes) + " notes) a la pista «"_ca + nomPista + "»"_ca);
    obrePianoRoll (pista, idx);
}

//==============================================================================
// 0.22: Vocal Editor
//==============================================================================
void PantallaEstudi::obreVeu (int pista, int clip)
{
    auto* t = estat.pista (pista);
    auto* c = estat.clip ({ pista, clip });
    if (t == nullptr || c == nullptr || c->midi || c->presa.empty() || mibase::esMidi (*t)) return;
    estat.automat.obert = false;   // 0.58
    if (audioPreses.find (c->presa) == audioPreses.end())
    {
        estat.info = "Aquest clip no té l'àudio carregat"_ca;
        estat.avisa();
        return;
    }
    if (! mibase::teVeu (*t))
    {
        // Es posa el Vocal Editor al primer insert de la pista (es pot desfer).
        estat.desfes.abans (estat.projecte);
        mibase::Insert x;
        x.id = "i" + std::to_string (++estat.projecte.comptador);
        x.plugin = mibase::pluginVeu;
        x.nom = "Vocal Editor";
        t->inserts.insert (t->inserts.begin(), x);
        estat.canviat ("Vocal Editor a la pista "_ca + dibuix::utf8 (t->nom));
        t = estat.pista (pista);
        c = estat.clip ({ pista, clip });
        if (t == nullptr || c == nullptr) return;
    }
    if (estat.pianoRoll) mostraPianoRoll (false);
    estat.sampler.obert = false;
    const bool mateixa = estat.veu.obert && estat.veu.presa == c->presa;
    estat.veu.obert = true;
    estat.veu.pista = pista;
    estat.veu.clip = clip;
    estat.veu.presa = c->presa;
    if (! mateixa)
    {
        estat.veu.triades.clear();
        editorVeu.novaPresa();
    }
    estat.triaClip (pista, clip);
    colocaSeccions();
    sincronitzaVeu();
    if (mibase::elastic::actiu (*c))   // 0.86
        estat.info = "Aquest clip té Elastic Audio: el Vocal Editor mostra la veu sense estirar (el so corregit sí que s'estira)"_ca;
    estat.avisa();
    editorVeu.grabKeyboardFocus();
}

void PantallaEstudi::obreVeuDePista (int pista)
{
    auto* t = estat.pista (pista);
    if (t == nullptr) return;
    for (auto& r : estat.triats)
        if (r.pista == pista)
            if (auto* c = estat.clip (r); c != nullptr && ! c->presa.empty()) { obreVeu (pista, r.clip); return; }
    for (int k = 0; k < (int) t->clips.size(); ++k)
        if (! t->clips[(size_t) k].presa.empty()) { obreVeu (pista, k); return; }
    estat.info = "Aquesta pista encara no té cap gravació per editar"_ca;
    estat.avisa();
}

void PantallaEstudi::tancaVeu()
{
    estat.veu.obert = false;
    colocaSeccions();
    estat.avisa();
    grabKeyboardFocus();
}

void PantallaEstudi::commutaVeuAB()
{
    auto* t = estat.pista (estat.veu.pista);
    if (t == nullptr) return;
    for (int k = 0; k < (int) t->inserts.size(); ++k)
        if (mibase::esInsertVeu (t->inserts[(size_t) k]))
        {
            inserts::commuta (estat, &plugins, estat.veu.pista, k);
            estat.info = mibase::veuActiva (estat.projecte.pistes[(size_t) estat.veu.pista]) ? "Sona la veu corregida"_ca : "Sona la veu original"_ca;
            estat.avisa();
            return;
        }
}

void PantallaEstudi::sincronitzaVeu()
{
    for (auto& t : estat.projecte.pistes)
    {
        if (mibase::esMidi (t) || ! mibase::teVeu (t)) continue;
        for (auto& c : t.clips)
        {
            if (c.presa.empty()) continue;
            auto* s = mibase::edicio::presa (estat.projecte, c.presa);
            auto it = audioPreses.find (c.presa);
            if (s == nullptr || it == audioPreses.end()) continue;
            // Només les preses que ja s'han obert al Vocal Editor (o la que s'hi obre ara).
            if (! s->veu.analitzada && ! (estat.veu.obert && estat.veu.presa == s->id)) continue;
            serveiVeu.actualitza (s->id, it->second, juce::File (fitxerPresa (*s).getFullPathName() + ".veu"), s->veu);
        }
    }
}

void PantallaEstudi::recullVeu()
{
    std::vector<std::string> fetes;
    if (! serveiVeu.recull (fetes)) return;
    bool notesNoves = false;
    for (auto& id : fetes)
    {
        auto* s = mibase::edicio::presa (estat.projecte, id);
        auto a = serveiVeu.analisi (id);
        if (s == nullptr || a == nullptr || s->veu.analitzada) continue;
        s->veu.notes = mibase::veu::detectaNotes (*a);
        s->veu.analitzada = true;
        historial.afegeix ("Vocal Editor: "_ca + juce::String ((int) s->veu.notes.size()) + " notes a "_ca + dibuix::utf8 (s->nom));
        notesNoves = true;
    }
    sincronitzaElastic();   // 0.86: el so corregit ha canviat (el mapa del pla el porta)
    if (notesNoves) estat.canviat ("Vocal Editor: notes detectades"_ca);
    else publicaPla();
    editorVeu.repaint();
}

//==============================================================================
// 0.24: solo dels editors
//==============================================================================
std::string PantallaEstudi::pistaEditor() const
{
    if (estat.veu.obert && estat.veu.pista >= 0 && estat.veu.pista < (int) estat.projecte.pistes.size())
        return estat.projecte.pistes[(size_t) estat.veu.pista].id;
    if (estat.pianoRoll && estat.editorActiu >= 0 && estat.editorActiu < (int) estat.editors.size())
        return estat.editors[(size_t) estat.editorActiu].idPista;
    return {};
}

void PantallaEstudi::restauraSolos()
{
    for (auto& [id, solo] : estat.soloEditor.abans)
        for (auto& t : estat.projecte.pistes)
            if (t.id == id) t.solo = solo;
    estat.soloEditor = {};
}

void PantallaEstudi::commutaSoloEditor()
{
    auto& s = estat.soloEditor;
    if (s.actiu)
    {
        restauraSolos();
        estat.info = "Solo de l'editor apagat: les pistes tornen com estaven"_ca;
        estat.avisa();
        return;
    }
    const auto id = pistaEditor();
    if (id.empty())
    {
        estat.info = "Obre un editor (Piano Roll, Drumpad o Vocal Editor) per fer-ne solo"_ca;
        estat.avisa();
        return;
    }
    s.abans.clear();
    for (auto& t : estat.projecte.pistes) s.abans.push_back ({ t.id, t.solo });
    for (auto& t : estat.projecte.pistes) t.solo = t.id == id;
    s.pista = id;
    s.actiu = true;
    for (auto& t : estat.projecte.pistes) if (t.id == id) estat.info = "Solo de l'editor: "_ca + dibuix::utf8 (t.nom);
    estat.avisa();
}

void PantallaEstudi::seguixSoloEditor()
{
    auto& s = estat.soloEditor;
    if (! s.actiu) return;
    const auto id = pistaEditor();
    if (id.empty()) { restauraSolos(); return; }   // l'editor s'ha tancat
    if (id == s.pista) return;
    for (auto& t : estat.projecte.pistes) t.solo = t.id == id;   // l'editor ha canviat de pista: el solo la segueix
    s.pista = id;
}

//==============================================================================
// 0.26: samplejador
//==============================================================================
void PantallaEstudi::obreSampler (int pista)
{
    auto* t = estat.pista (pista);
    if (t == nullptr || ! mibase::esSampler (*t)) return;
    estat.automat.obert = false;   // 0.58
    if (estat.pianoRoll) mostraPianoRoll (false);
    estat.veu.obert = false;
    if (estat.sampler.pista != t->id) { estat.sampler.pxPerSeg = 0.0; estat.sampler.iniciVista = 0.0; estat.sampler.capcal = 0.0; }
    estat.sampler.obert = true;
    estat.sampler.pista = t->id;
    estat.pistaTriada = pista;
    colocaSeccions();
    estat.avisa();
    editorSampler.grabKeyboardFocus();
}

void PantallaEstudi::obreAutomatitzacio (int pista)
{
    if (estat.pista (pista) == nullptr) return;
    if (estat.veu.obert) tancaVeu();            // 0.58: un sol editor a baix
    if (estat.sampler.obert) tancaSampler();
    editorAuto.obre (pista);
    colocaSeccions();
    estat.avisa();
    editorAuto.grabKeyboardFocus();
}

void PantallaEstudi::tancaAutomatitzacio()
{
    estat.automat.obert = false;
    colocaSeccions();
    estat.avisa();
    grabKeyboardFocus();
}

void PantallaEstudi::tancaSampler()
{
    if (auto* sm = instruments.samplejador (estat.sampler.pista)) sm->aturaEscolta();
    estat.sampler.obert = false;
    colocaSeccions();
    estat.avisa();
    grabKeyboardFocus();
}

void PantallaEstudi::demanaSample()
{
    const auto id = estat.sampler.pista;
    selector = std::make_unique<juce::FileChooser> ("Carrega un sample"_ca, juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                    "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                           [this, id] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile()) carregaSample (id, f);
    });
}

void PantallaEstudi::carregaSample (const std::string& idPista, const juce::File& origen)
{
    mibase::Pista* t = nullptr;
    for (auto& q : estat.projecte.pistes) if (q.id == idPista && mibase::esSampler (q)) t = &q;
    if (t == nullptr) return;
    // Es copia a «Samples» dins del projecte (els que no són WAV es converteixen a WAV de 24 bits). L'original no es toca.
    const auto carpetaSamples = estat.carpeta.getChildFile ("Samples");
    carpetaSamples.createDirectory();
    const auto base = juce::File::createLegalFileName (origen.getFileNameWithoutExtension());
    auto desti = carpetaSamples.getChildFile (base + ".wav");
    for (int k = 2; desti.existsAsFile(); ++k)
    {
        if (origen.hasFileExtension ("wav") && desti.getSize() == origen.getSize()) break;   // ja hi és
        desti = carpetaSamples.getChildFile (base + " " + juce::String (k) + ".wav");
    }
    juce::String error;
    if (! desti.existsAsFile())
    {
        if (origen.hasFileExtension ("wav"))
        {
            if (! origen.copyFileTo (desti)) error = "No s'ha pogut copiar el fitxer"_ca;
        }
        else
        {
            std::unique_ptr<juce::AudioFormatReader> r (kits::formats().createReaderFor (origen));
            if (r == nullptr) error = "No sé llegir aquest fitxer"_ca;
            else
            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::OutputStream> sortida = desti.createOutputStream();
                auto w = sortida != nullptr ? wav.createWriterFor (sortida, juce::AudioFormatWriterOptions{}
                                                                       .withSampleRate (r->sampleRate)
                                                                       .withNumChannels ((int) juce::jmin (2u, r->numChannels))
                                                                       .withBitsPerSample (24))
                                            : nullptr;
                if (w == nullptr || ! w->writeFromAudioReader (*r, 0, -1)) error = "No s'ha pogut escriure el WAV"_ca;
            }
        }
    }
    if (error.isNotEmpty() || Instruments::mostraSample (desti) == nullptr)
    {
        if (error.isEmpty()) error = "El fitxer no té àudio o no es pot llegir"_ca;
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No s'ha pogut carregar el sample"_ca, error + "\n" + origen.getFullPathName());
        return;
    }
    estat.desfes.abans (estat.projecte);
    auto& p = t->sampler;
    p.fitxer = ("Samples/" + desti.getFileName()).toStdString();
    p.nom = desti.getFileNameWithoutExtension().toStdString();
    p.inici = 0.0; p.fi = -1.0;
    p.bucleDe = p.bucleA = -1.0;
    p.fadeIn = 0.0;
    p.bpm = 0.0;
    p.speed = 1.0;
    p.reversa = false;
    estat.sampler.pxPerSeg = 0.0;
    // 0.26.1: la nota del sample es busca sola i es posa com a Root (així, aquella tecla el toca tal qual).
    const int root = mibase::sampler::detectaRoot (*Instruments::mostraSample (desti));
    p.root = root >= 0 ? root : 60;
    estat.info = "Sample carregat: "_ca + desti.getFileName()
                 + (root >= 0 ? ". És un "_ca + EditorSampler::nomNota (root) + ": aquesta tecla el toca tal qual"_ca
                              : ". No té una nota clara: el toca la C4"_ca);
    estat.canviat ("Samplejador: «"_ca + desti.getFileName() + "» a la pista «"_ca + dibuix::utf8 (t->nom) + "»"_ca);
}

//==============================================================================
// 0.27: biblioteca de samples
//==============================================================================
int PantallaEstudi::pistaSamplerNova (const juce::File& f)
{
    const auto nom = ed::nomPistaLliure (estat.projecte, f.getFileNameWithoutExtension().toStdString());
    estat.desfes.abans (estat.projecte);
    auto& t = ed::afegeixPista (estat.projecte, nom, "midi");
    t.canals = 2;
    t.instrument = mibase::instrumentSampler;
    const auto id = t.id;
    estat.pistaTriada = ed::pistaPerId (estat.projecte, t.id);   // 0.34: les pistes noves van abans dels busos
    estat.capTriat();
    estat.canviat ("Pista nova: «"_ca + dibuix::utf8 (nom) + "» (samplejador)"_ca);
    obreSampler (estat.pistaTriada);
    carregaSample (id, f);
    return estat.pistaTriada;
}

void PantallaEstudi::deixaSample (const juce::File& f, int fila, double temps)
{
    if (! f.existsAsFile()) return;
    auto* t = fila >= 0 ? estat.pista (fila) : nullptr;
    if (t == nullptr) { pistaSamplerNova (f); return; }                   // sota les pistes: pista Samplejador nova
    if (mibase::esSampler (*t)) { obreSampler (fila); carregaSample (t->id, f); return; }
    if (mibase::esMidi (*t))
    {
        estat.info = "Aquesta pista és d'instrument: deixa'l en una pista d'àudio (clip), en una de samplejador o a sota de les pistes (pista nova)"_ca;
        estat.avisa();
        return;
    }
    importa (juce::StringArray (f.getFullPathName()), fila, temps);       // pista d'àudio: un clip
}

void PantallaEstudi::sampleAFila (int fila, const juce::File& f)
{
    auto* pr = zonaEditors.actiu();
    auto* t = pr != nullptr ? estat.pista (pr->pistaDeLEditor()) : nullptr;
    if (t == nullptr || fila < 0 || fila >= (int) t->files.size()) return;
    // El so es copia a la carpeta dels kits («Biblioteca/…»), d'on la bateria llegeix els seus sons.
    const auto carpetaKits = kits::carpeta().getChildFile ("Biblioteca");
    auto desti = carpetaKits.getChildFile (juce::File::createLegalFileName (f.getFileNameWithoutExtension()) + ".wav");
    if (! desti.existsAsFile() || desti.getSize() != f.getSize())
    {
        if (desti.existsAsFile())
            desti = carpetaKits.getChildFile (juce::String::fromUTF8 (mibase::biblioteca::nomLliure (carpetaKits.getFullPathName().toStdString(),
                                                                       juce::File::createLegalFileName (f.getFileNameWithoutExtension()).toStdString(), ".wav").c_str()));
        if (auto e = samples::copiaComWav (f, desti); e.isNotEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No s'ha pogut posar el sample"_ca, e);
            return;
        }
    }
    estat.desfes.abans (estat.projecte);
    auto& fl = t->files[(size_t) fila];
    fl.so = ("Biblioteca/" + desti.getFileName()).toStdString();
    fl.nom = f.getFileNameWithoutExtension().toStdString();
    estat.canviat ("Drumpad: «"_ca + f.getFileNameWithoutExtension() + "» a la fila "_ca + juce::String (fila + 1));
    if (auto mo = kits::llegeix (desti)) servei.escoltaSo (mo);
}

//==============================================================================
// 0.27: Esc, estiguis on estiguis: tanca les finestres i els editors i torna a la taula de treball.
void PantallaEstudi::tancaTot()
{
    plugins.tancaFinestres();
    esquerra.opcions.tancaFinestresSinte();
    finestraKits.reset();
    if (estat.sampler.obert) tancaSampler();
    if (estat.veu.obert) tancaVeu();
    if (estat.pianoRoll) mostraPianoRoll (false);
    estat.vistaMescles = estat.vistaEstructura = false;
    estat.info = "Esc: tot tancat (taula de treball)"_ca;
    estat.avisa();
    if (auto* arrel = getTopLevelComponent()) arrel->toFront (true);
    grabKeyboardFocus();
}

/** 0.27: es pot passar el teclat a on és el ratolí? No si s'està escrivint (un camp de text o una finestreta) o si
    Mibase no és la finestra de davant. */
bool PantallaEstudi::potAgafarFocus() const
{
    if (! juce::Process::isForegroundProcess()) return false;
    if (dialeg != nullptr && dialeg->isVisible()) return false;
    if (juce::Component::getCurrentlyModalComponent() != nullptr) return false;
    auto* f = juce::Component::getCurrentlyFocusedComponent();
    if (dynamic_cast<juce::TextEditor*> (f) != nullptr) return false;
    if (f != nullptr && f->getTopLevelComponent() != getTopLevelComponent()) return false;   // una finestra a part (plugin, Piano Roll desenganxat…)
    return true;
}

//==============================================================================
// 0.28: «Envia al samplejador»: el tros d'un clip d'àudio (o el del requadre) es converteix en un sample nou de la
// biblioteca (còpia: l'original no es toca) i, si cal, en una pista Samplejador nova.
juce::File PantallaEstudi::escriuTros (int pista, int clip, const juce::File& dir)
{
    auto* t = estat.pista (pista);
    if (t == nullptr || clip < 0 || clip >= (int) t->clips.size()) return {};
    const auto c = t->clips[(size_t) clip];
    const auto* s = ed::presa (estat.projecte, c.presa);
    if (s == nullptr) return {};
    auto it = audioPreses.find (s->id);
    if (it == audioPreses.end() || it->second == nullptr) return {};
    const auto& au = *it->second;
    // de la línia de temps a dins del fitxer
    double a = c.iniciVisible(), b = c.fiVisible();
    if (estat.rang.actiu && pista >= estat.rang.pistaDe && pista <= estat.rang.pistaFins)
    {
        a = juce::jmax (a, estat.rang.t0);
        b = juce::jmin (b, estat.rang.t1);
    }
    const auto i0 = (juce::int64) std::llround ((a - c.pos) * au.fs), i1 = (juce::int64) std::llround ((b - c.pos) * au.fs);
    const auto ia = juce::jlimit ((juce::int64) 0, (juce::int64) au.longitud, i0), ib = juce::jlimit (ia, (juce::int64) au.longitud, i1);
    if (ib - ia < (juce::int64) (0.01 * au.fs))
    {
        estat.info = "El tros és massa curt"_ca;
        estat.avisa();
        return {};
    }
    dir.createDirectory();
    const auto base = juce::File::createLegalFileName (dibuix::utf8 (s->nom) + " tros").toStdString();
    const auto rel = mibase::biblioteca::nomLliure (dir.getFullPathName().toStdString(), base, ".wav");
    const auto desti = dir.getChildFile (juce::String::fromUTF8 (rel.c_str()));
    juce::AudioBuffer<float> buf (au.canals, (int) (ib - ia));
    for (int ch = 0; ch < au.canals; ++ch)
        std::copy (au.dades[ch].begin() + ia, au.dades[ch].begin() + ib, buf.getWritePointer (ch));
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> sortida = desti.createOutputStream();
    auto w = sortida != nullptr ? wav.createWriterFor (sortida, juce::AudioFormatWriterOptions{}.withSampleRate (au.fs).withNumChannels (au.canals).withBitsPerSample (24))
                                : nullptr;
    if (w == nullptr || ! w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples()))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No s'ha pogut crear el sample"_ca, desti.getFullPathName());
        return {};
    }
    return desti;
}

void PantallaEstudi::enviaAlSampler (int pista, int clip, bool nomesBiblioteca)
{
    const auto desti = escriuTros (pista, clip, biblioteca.carpeta());
    if (desti == juce::File()) return;
    biblioteca.nouFitxer (desti);   // l'afegeix a la biblioteca (amb el tipus, la nota…), en segon pla
    historial.afegeix ("Sample nou a la biblioteca: "_ca + desti.getFileName());
    if (nomesBiblioteca)
    {
        estat.info = "«"_ca + desti.getFileName() + "» és a la biblioteca"_ca;
        estat.avisa();
        return;
    }
    pistaSamplerNova (desti);
}

// 0.29: un clip d'àudio (o el tros del requadre) arrossegat a sobre del samplejador: n'és el sample nou
void PantallaEstudi::clipAlSampler (int pista, int clip)
{
    const auto f = escriuTros (pista, clip, estat.carpeta.getChildFile ("Samples"));
    if (f == juce::File()) return;
    carregaSample (estat.sampler.pista, f);
}

// 0.29: el sample del samplejador obert, a la biblioteca (còpia)
void PantallaEstudi::samplerABiblioteca()
{
    for (auto& t : estat.projecte.pistes)
        if (t.id == estat.sampler.pista && mibase::esSampler (t))
        {
            if (t.sampler.fitxer.empty()) { estat.info = "El samplejador no té cap sample"_ca; estat.avisa(); return; }
            const auto f = Instruments::fitxerSample (estat.carpeta, t.sampler);
            if (! f.existsAsFile()) { estat.info = "No trobo el fitxer del sample"_ca; estat.avisa(); return; }
            if (f.isAChildOf (biblioteca.carpeta())) { estat.info = "Aquest sample ja és a la biblioteca"_ca; estat.avisa(); return; }
            biblioteca.afegeix ({ f });
            estat.info = "«"_ca + f.getFileName() + "» s'afegeix a la biblioteca"_ca;
            estat.avisa();
            return;
        }
}

//==============================================================================
// 0.28: gravar dins el samplejador: com una presa (amb la gravadora del motor), però el fitxer va a Samples\ i el
// carrega el samplejador (no fa cap clip).
void PantallaEstudi::gravaSampler()
{
    if (estat.transport == 2) { atura(); return; }
    int pista = -1;
    for (int i = 0; i < estat.numPistes(); ++i)
        if (estat.projecte.pistes[(size_t) i].id == estat.sampler.pista && mibase::esSampler (estat.projecte.pistes[(size_t) i])) pista = i;
    if (pista < 0) return;
    if (! servei.enMarxa())
    {
        avisDeTargeta ("No es pot gravar", "La targeta d'àudio està aturada. Obre Fitxer → Preferències i tria-la."_ca);
        return;
    }
    if (estat.transport != 0) atura();
    const auto& t = estat.projecte.pistes[(size_t) pista];
    const int entrada = t.sampler.entradaGrava, canals = t.sampler.canalsGrava;
    estat.nomsEntrades = servei.getNomsEntrades();
    const auto& noms = estat.nomsEntrades;
    const int darrera = entrada + (canals == 2 ? 1 : 0);
    if (darrera >= noms.size() || noms[entrada].isEmpty() || noms[darrera].isEmpty())
    {
        avisDeTargeta ("No es pot gravar", "El samplejador grava de l'entrada E"_ca + juce::String (entrada + 1)
                       + ", que no està activa. Tria'n una altra (botó del costat de «Grava») o activa-la a Fitxer → Preferències."_ca);
        return;
    }
    auto& motor = servei.estudi();
    const auto dir = estat.carpeta.getChildFile ("Samples");
    dir.createDirectory();
    const auto rel = mibase::biblioteca::nomLliure (dir.getFullPathName().toStdString(),
                                                    juce::File::createLegalFileName (dibuix::utf8 (t.nom) + " gravat").toStdString(), ".wav");
    gravacio.fitxer = dir.getChildFile (juce::String::fromUTF8 (rel.c_str()));
    if (! motor.gravadora().comenca (dibuix::cami (gravacio.fitxer), canals, motor.getFs()))
    {
        avisDeTargeta ("No es pot gravar", "No s'ha pogut crear el fitxer:\n"_ca + gravacio.fitxer.getFullPathName());
        return;
    }
    publicaPla();
    gravacio.pista = pista;
    gravacio.midi = false;
    gravacio.perSampler = true;
    gravacio.cursor = gravacio.desDe = estat.capcal;   // sense compte d'entrada
    gravacio.retard = servei.retardGravacio (gravacio.retardMesurat);
    plugins.buidaCues();
    motor.grava (gravacio.desDe, gravacio.cursor, entrada, canals);
    estat.cursorGravacio = gravacio.cursor;
    estat.transport = 2;
    estatEsperat = 2;
    esperantMotor = true;
    tempsOrdre = juce::Time::currentTimeMillis();
    estat.info = "Gravant al samplejador… (torna a clicar «Grava» o atura per acabar)"_ca;
    estat.avisa();
}

// ---------------------------------------------------------------------------------------------------------------------
// 0.85: el Pool

std::string PantallaEstudi::copiaAlPool (const juce::File& f, const juce::String& nomBase, juce::String& error)
{
    // L'àudio es copia (o es converteix a WAV) a la carpeta Àudios del projecte i passa a ser una presa (sense clip).
    std::unique_ptr<juce::AudioFormatReader> lector (formats.createReaderFor (f));
    if (lector == nullptr || lector->sampleRate <= 0.0 || lector->lengthInSamples <= 0)
    {
        error = "«"_ca + f.getFileName() + "» no és un àudio que Mibase sàpiga llegir."_ca;
        return {};
    }
    auto carpetaAudios = audios();
    carpetaAudios.createDirectory();
    const auto nomDesti = dibuix::utf8 (disc::nomPresa (dibuix::cami (carpetaAudios), nomBase.toStdString()));
    const auto desti = carpetaAudios.getChildFile (nomDesti);
    if (f.hasFileExtension ("wav"))
    {
        if (! f.copyFileTo (desti))
        {
            error = "No s'ha pogut copiar «"_ca + f.getFileName() + "» a Àudios."_ca;
            return {};
        }
    }
    else
    {
        std::unique_ptr<juce::OutputStream> sortida = desti.createOutputStream();
        juce::WavAudioFormat wav;
        auto escriptor = sortida != nullptr ? wav.createWriterFor (sortida, juce::AudioFormatWriterOptions{}
                                                                            .withSampleRate (lector->sampleRate)
                                                                            .withNumChannels ((int) lector->numChannels)
                                                                            .withBitsPerSample (24))
                                            : nullptr;
        if (escriptor == nullptr || ! escriptor->writeFromAudioReader (*lector, 0, -1))
        {
            escriptor.reset();
            desti.deleteFile();
            error = "No s'ha pogut convertir «"_ca + f.getFileName() + "» a WAV."_ca;
            return {};
        }
    }
    const double durada = (double) lector->lengthInSamples / lector->sampleRate;
    const auto fitxerRelatiu = (juce::String::fromUTF8 (disc::carpetaAudios) + "/" + nomDesti).toStdString();
    const auto id = ed::afegeixPresa (estat.projecte, fitxerRelatiu, f.getFileName().toStdString(), durada).id;
    carregaOna (*ed::presa (estat.projecte, id));
    return id;
}

void PantallaEstudi::escoltaPresa (const std::string& id)
{
    const auto ara = juce::Time::getMillisecondCounter();
    if (id == presaEscoltada && ara < presaEscoltadaFins)   // tornar-hi a clicar l'atura
    {
        servei.escoltaSo (nullptr);
        presaEscoltada.clear();
        pool.escoltant.clear();
        pool.repaint();
        return;
    }
    const auto* s = ed::presa (estat.projecte, id);
    if (s == nullptr) return;
    auto mo = kits::llegeix (fitxerPresa (*s), 180.0);   // fins a 3 minuts
    if (mo == nullptr || mo->L.empty()) { estat.info = "No es pot obrir aquest àudio"_ca; estat.avisa(); return; }
    presaEscoltada = id;
    presaEscoltadaFins = ara + (juce::uint32) (1000.0 * (double) mo->L.size() / juce::jmax (1.0, mo->fs));
    pool.escoltant = id;
    servei.escoltaSo (mo);
    pool.repaint();
    juce::Component::SafePointer<PantallaEstudi> jo (this);
    juce::Timer::callAfterDelay ((int) (presaEscoltadaFins - ara) + 50, [jo, id]
    {
        if (jo != nullptr && jo->pool.escoltant == id && juce::Time::getMillisecondCounter() >= jo->presaEscoltadaFins)
        { jo->pool.escoltant.clear(); jo->pool.repaint(); }
    });
}

void PantallaEstudi::localitzaPresa (const std::string& id)
{
    const auto llocs = ed::llocsPresa (estat.projecte, id);
    const auto us = ed::usPresa (estat.projecte, id);
    if (llocs.empty())
    {
        estat.info = us.takes > 0 ? "Aquest àudio només és a les preses (subpistes) d'una pista"_ca
                                  : "Aquest àudio no és a la taula: arrossega'l des del Pool per posar-lo"_ca;
        if (us.takes > 0)
            for (int i = 0; i < estat.numPistes(); ++i)
                for (const auto& k : estat.projecte.pistes[(size_t) i].takes)
                    if (k.presa == id) { estat.pistaTriada = i; estat.projecte.pistes[(size_t) i].takesObertes = true; }
        estat.avisa();
        pistes.repaint();
        return;
    }
    // es trien tots els clips on surt i la vista va al primer
    estat.triats = llocs;
    estat.rang.actiu = false;
    estat.pistaTriada = llocs[0].pista;
    const auto& c = estat.projecte.pistes[(size_t) llocs[0].pista].clips[(size_t) llocs[0].clip];
    const double ampleSeg = pistes.getWidth() / juce::jmax (1.0, estat.pxPerSegon);
    if (c.iniciVisible() < estat.iniciVista || c.iniciVisible() > estat.iniciVista + ampleSeg * 0.9)
        estat.iniciVista = juce::jmax (0.0, c.iniciVisible() - ampleSeg * 0.15);
    const int y = estat.yPista (llocs[0].pista);
    if (y < estat.altPista || y + estat.altPista > pistes.getHeight())
        estat.desplY = juce::jmax (0, y + estat.desplY - estat.altPista * 2);
    estat.info = llocs.size() == 1 ? "Aquest àudio surt en 1 clip (triat a la taula)"_ca
                                   : "Aquest àudio surt en "_ca + juce::String ((int) llocs.size()) + " clips (tots triats a la taula)";
    estat.avisa();
    repaint();
}

void PantallaEstudi::canviaNomPresa (const std::string& id)
{
    const auto* s = ed::presa (estat.projecte, id);
    if (s == nullptr) return;
    const auto vell = fitxerPresa (*s);
    juce::Component::SafePointer<PantallaEstudi> jo (this);
    dialeg = std::make_unique<DialegText> ("Canvia el nom de l'àudio"_ca,
        "Es canvia el nom del fitxer a la carpeta Àudios i el projecte el continua trobant."_ca, "Canvia"_ca,
        [] (const juce::String& t) { return juce::File::createLegalFileName (t.trim()).isEmpty() ? "Escriu un nom"_ca : juce::String(); },
        [jo, id, vell] (const juce::String& t)
        {
            if (jo == nullptr) return;
            auto& e = jo->estat;
            const auto base = juce::File::createLegalFileName (t.trim());
            auto nou = vell.getSiblingFile (base + vell.getFileExtension());
            for (int k = 2; nou.exists() && nou != vell; ++k) nou = vell.getSiblingFile (base + " " + juce::String (k) + vell.getFileExtension());
            if (nou != vell && ! vell.moveFileTo (nou))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Canvia el nom"_ca,
                                                        "No s'ha pogut canviar el nom del fitxer (potser és obert en un altre programa)."_ca);
                return;
            }
            const auto relatiu = nou.getRelativePathFrom (e.carpeta).replaceCharacter ('\\', '/').toStdString();
            const auto nom = t.trim().toStdString();
            // el fitxer ha canviat al disc: totes les còpies de desfer també ho han de saber
            ed::canviaFitxerPresa (e.projecte, id, relatiu, nom);
            e.desfes.perTots ([&] (mibase::Projecte& q) { ed::canviaFitxerPresa (q, id, relatiu, nom); });
            jo->historial.afegeix ("Pool: «"_ca + vell.getFileName() + "» ara es diu «" + nou.getFileName() + "»");
            e.canviat ("Canviat el nom de l'àudio «"_ca + vell.getFileName() + "» a «" + nou.getFileName() + "»");
            jo->desaAra();
        },
        dibuix::utf8 (s->nom).upToLastOccurrenceOf (".", false, false).isEmpty() ? dibuix::utf8 (s->nom) : dibuix::utf8 (s->nom).upToLastOccurrenceOf (".", false, false));
    DialegText::mostra (*this, dialeg);
}

void PantallaEstudi::buscaPresa (const std::string& id)
{
    const auto* s = ed::presa (estat.projecte, id);
    if (s == nullptr) return;
    const auto nom = dibuix::utf8 (s->nom);
    selector = std::make_unique<juce::FileChooser> ("On és «"_ca + nom + "»?", estat.carpeta, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, id, nom] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        // es copia a Àudios (el projecte continua tenint-ho tot a la seva carpeta) i la presa hi apunta
        juce::String error;
        const auto idTemp = copiaAlPool (f, nom.upToLastOccurrenceOf (".", false, false).isEmpty() ? nom : nom.upToLastOccurrenceOf (".", false, false), error);
        if (idTemp.empty()) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Busca el fitxer"_ca, error); return; }
        const auto* t = ed::presa (estat.projecte, idTemp);
        const auto fitxer = t->fitxer;
        const double durada = t->durada;
        estat.projecte.preses.erase (std::remove_if (estat.projecte.preses.begin(), estat.projecte.preses.end(),
                                                     [&idTemp] (const mibase::Presa& x) { return x.id == idTemp; }), estat.projecte.preses.end());
        estat.ones.erase (idTemp);
        audioPreses.erase (idTemp);
        ed::canviaFitxerPresa (estat.projecte, id, fitxer);
        if (auto* p = ed::presa (estat.projecte, id)) p->durada = durada;
        estat.desfes.perTots ([&] (mibase::Projecte& q) { ed::canviaFitxerPresa (q, id, fitxer); });
        carregaOna (*ed::presa (estat.projecte, id));
        publicaPla();
        historial.afegeix ("Pool: «"_ca + nom + "» retrobat a " + f.getFullPathName());
        estat.canviat ("Retrobat l'àudio «"_ca + nom + "»");
    });
}

void PantallaEstudi::aLaPaperera (const std::vector<std::string>& ids)
{
    // Els àudios que no es fan servir surten del projecte i el fitxer va a Àudios/Paperera. Es pot desfer: les còpies de
    // desfer apunten a la paperera, i l'àudio continua carregat.
    auto copia = estat.projecte;
    int fets = 0;
    const auto pap = paperera();
    pap.createDirectory();
    for (const auto& id : ids)
    {
        const auto* s = ed::presa (estat.projecte, id);
        if (s == nullptr || ed::usPresa (estat.projecte, id).enUs()) continue;
        const auto f = fitxerPresa (*s);
        std::string relatiu = s->fitxer;
        if (f.existsAsFile())
        {
            auto desti = pap.getChildFile (f.getFileName());
            for (int k = 2; desti.exists(); ++k) desti = pap.getChildFile (f.getFileNameWithoutExtension() + " " + juce::String (k) + f.getFileExtension());
            if (! f.moveFileTo (desti)) continue;
            relatiu = desti.getRelativePathFrom (estat.carpeta).replaceCharacter ('\\', '/').toStdString();
        }
        ed::canviaFitxerPresa (copia, id, relatiu);
        estat.desfes.perTots ([&] (mibase::Projecte& q) { ed::canviaFitxerPresa (q, id, relatiu); });
        ed::treuPresa (estat.projecte, id);
        ++fets;
    }
    if (fets == 0) return;
    estat.desfes.abans (copia);
    estat.canviat ("Pool: "_ca + juce::String (fets) + (fets == 1 ? " àudio a la paperera"_ca : " àudios a la paperera"_ca));
    estat.info = (fets == 1 ? "1 àudio a la paperera del projecte"_ca : juce::String (fets) + " àudios a la paperera del projecte"_ca) + " (Ctrl+Z ho desfà)";
    estat.avisa();
    desaAra();
}

void PantallaEstudi::buidaPaperera()
{
    const auto pap = paperera();
    juce::Array<juce::File> fitxers;
    for (const auto& f : pap.findChildFiles (juce::File::findFiles, false))
    {
        bool usat = false;   // si un desfer l'ha tornat al projecte, no
        for (const auto& s : estat.projecte.preses) usat |= fitxerPresa (s) == f;
        if (! usat) fitxers.add (f);
    }
    if (fitxers.isEmpty()) { estat.info = "La paperera del projecte és buida"_ca; estat.avisa(); return; }
    juce::Component::SafePointer<PantallaEstudi> jo (this);
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Buida la paperera"_ca,
        "S'esborraran del disc "_ca + juce::String (fitxers.size()) + " fitxers de la paperera del projecte. Això no es pot desfer."_ca,
        "Esborra'ls"_ca, "Cancel·la"_ca, nullptr, juce::ModalCallbackFunction::create ([jo, fitxers] (int r)
        {
            if (jo == nullptr || r != 1) return;
            int n = 0;
            for (auto& f : fitxers) if (f.deleteFile()) ++n;
            jo->historial.afegeix ("Pool: paperera buidada ("_ca + juce::String (n) + " fitxers)");
            jo->estat.info = "Paperera buidada: "_ca + juce::String (n) + " fitxers esborrats";
            jo->estat.avisa();
        }));
}

void PantallaEstudi::importaAlPool()
{
    selector = std::make_unique<juce::FileChooser> ("Importa àudios al Pool"_ca, juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                    "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                               | juce::FileBrowserComponent::canSelectMultipleItems,
                           [this] (const juce::FileChooser& fc)
    {
        if (fc.getResults().isEmpty()) return;
        const auto copia = estat.projecte;
        juce::StringArray errors;
        int fets = 0;
        for (auto& f : fc.getResults())
        {
            juce::String error;
            if (copiaAlPool (f, f.getFileNameWithoutExtension(), error).empty()) errors.add (error);
            else ++fets;
        }
        if (fets > 0)
        {
            estat.desfes.abans (copia);
            estat.canviat ("Pool: importats "_ca + juce::String (fets) + " àudios");
        }
        if (! errors.isEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Importa al Pool"_ca, errors.joinIntoString ("\n"));
    });
}

void PantallaEstudi::deixaPresa (const std::string& id, int fila, double temps)
{
    // Un àudio del Pool deixat a la taula: un clip nou del mateix fitxer (no es copia res)
    const auto* s = ed::presa (estat.projecte, id);
    if (s == nullptr) return;
    auto* t = fila >= 0 ? estat.pista (fila) : nullptr;
    if (t != nullptr && mibase::esAcords (*t))   // 0.87: a la pista d'Acords: se n'escolten els acords (el clip no s'hi posa)
    {
        mibase::Clip c;
        c.presa = id;
        c.pos = juce::jmax (0.0, temps);
        c.fi = s->durada;
        treuAcords (c, dibuix::utf8 (s->nom));
        return;
    }
    if (t != nullptr && (mibase::esMidi (*t) || mibase::esBus (*t) || mibase::esAcords (*t)))
    {
        estat.info = "Deixa'l en una pista d'àudio o a sota de les pistes (pista nova)"_ca;
        estat.avisa();
        return;
    }
    estat.desfes.abans (estat.projecte);
    if (t == nullptr)
    {
        auto base = dibuix::utf8 (s->nom).upToLastOccurrenceOf (".", false, false);
        if (base.isEmpty()) base = dibuix::utf8 (s->nom);
        const auto nom = ed::nomPistaLliure (estat.projecte, base.substring (0, 40).toStdString());
        auto& nova = ed::afegeixPista (estat.projecte, nom, "audio");
        auto it = audioPreses.find (id);
        nova.canals = it != audioPreses.end() && it->second != nullptr && it->second->canals == 2 ? 2 : 1;
        fila = ed::pistaPerId (estat.projecte, nova.id);
    }
    ed::afegeixClip (estat.projecte, fila, id, juce::jmax (0.0, temps));
    estat.recalculaFiles();
    estat.triaClip (fila, (int) estat.projecte.pistes[(size_t) fila].clips.size() - 1);
    estat.canviat ("Pool: «"_ca + dibuix::utf8 (ed::presa (estat.projecte, id)->nom) + "» posat a la taula");
}

//==============================================================================
// 0.86: Elastic Audio
//==============================================================================
void PantallaEstudi::sincronitzaElastic (bool araMateix)
{
    namespace el = mibase::elastic;
    const auto& corr = serveiVeu.corregides();
    mapaSo = corr;
    std::set<std::string> vius;
    std::map<std::string, std::string> nou;
    for (const auto& t : estat.projecte.pistes)
    {
        if (mibase::esMidi (t)) continue;
        const bool veu = mibase::veuActiva (t);
        for (size_t k = 0; k < t.clips.size(); ++k)
        {
            const auto& c = t.clips[k];
            if (! el::actiu (c) || c.presa.empty()) continue;
            // Amb el Vocal Editor encès, s'estira la veu corregida (com fa el motor en triar-ne la clau).
            std::shared_ptr<const mibase::AudioPresa> font;
            bool deVeu = false;
            if (auto ic = corr.find (c.presa); veu && ic != corr.end() && ic->second != nullptr) { font = ic->second; deVeu = true; }
            else if (auto ia = audioPreses.find (c.presa); ia != audioPreses.end()) font = ia->second;
            if (font == nullptr) continue;
            const auto clau = el::clau (c, deVeu);
            const auto lloc = t.id + "#" + std::to_string (k);
            vius.insert (clau);
            if (araMateix) serveiElastic.ara (clau, font, c.warp, c.elastic);
            else serveiElastic.demana (clau, font, c.warp, c.elastic);
            if (auto so = serveiElastic.fet (clau, font)) { mapaSo[clau] = so; nou[lloc] = clau; }
            else if (auto ic = clauDeClip.find (lloc); ic != clauDeClip.end())
                if (auto vell = serveiElastic.fet (ic->second))
                {
                    // Mentre es calcula el nou, sona el d'abans d'aquest clip (no l'original, que aniria a un altre temps).
                    mapaSo[clau] = vell;
                    nou[lloc] = ic->second;
                    vius.insert (ic->second);
                }
        }
    }
    clauDeClip = std::move (nou);
    serveiElastic.oblida (vius);
}

void PantallaEstudi::recullElastic()
{
    if (! serveiElastic.recull()) return;
    sincronitzaElastic();
    publicaPla();
}

void PantallaEstudi::menuElastic (juce::Component& boto)
{
    namespace el = mibase::elastic;
    if (estat.triats.size() != 1) return;
    const auto ref = estat.triats[0];
    const auto* c = estat.clip (ref);
    if (c == nullptr || c->midi || c->presa.empty()) return;
    const bool ambMarcadors = ! el::marcadors (*c).empty();

    juce::PopupMenu m, alg, cops;
    for (int i = 0; i < 3; ++i) alg.addItem (10 + i, juce::String::fromUTF8 (el::nomMode (i)), true, c->elastic == i);
    cops.addItem (20, "Només els cops forts"_ca);
    cops.addItem (21, "Normal");
    cops.addItem (22, "Tots (també els fluixos)"_ca);
    m.addSectionHeader ("Elastic Audio");
    m.addSubMenu ("Algorisme: "_ca + juce::String::fromUTF8 (el::nomMode (c->elastic)), alg);
    m.addSeparator();
    m.addSubMenu ("Marcadors als cops (detecta els atacs)"_ca, cops);
    m.addItem (2, "Quantitza els marcadors a la graella"_ca, ambMarcadors);
    m.addItem (3, "Quantitza al 50 % (més natural)"_ca, ambMarcadors);
    m.addSeparator();
    m.addItem (4, "Treu els marcadors (es queda l'estirament del clip)"_ca, ambMarcadors);
    m.addItem (5, "Restaura l'àudio original"_ca, el::actiu (*c));
    if (estat.eina != Eina::Elastic)
    {
        m.addSeparator();
        m.addItem (6, "Agafa l'eina Elastic Audio   (7)"_ca);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&boto), [this, ref] (int r)
    {
        auto* k = estat.clip (ref);
        if (r <= 0 || k == nullptr || k->presa.empty()) return;
        if (r == 6) { triaEina (Eina::Elastic); return; }
        const auto* s = ed::presa (estat.projecte, k->presa);
        if (s == nullptr) return;
        auto copia = estat.projecte;
        juce::String text;
        if (r >= 10 && r <= 12)
        {
            if (k->elastic == r - 10) return;
            k->elastic = r - 10;
            text = "Elastic Audio: algorisme "_ca + juce::String::fromUTF8 (el::nomMode (k->elastic));
        }
        else if (r >= 20 && r <= 22)
        {
            auto ia = audioPreses.find (k->presa);
            if (ia == audioPreses.end() || ia->second == nullptr) { estat.info = "Aquest clip no té l'àudio carregat"_ca; estat.avisa(); return; }
            const auto trobats = el::cops (*ia->second, r == 20 ? 0.2 : r == 21 ? 0.5 : 0.85);
            const int n = el::marcadorsAlsCops (*k, s->durada, trobats);
            if (n == 0) { estat.projecte = std::move (copia); estat.info = "No s'hi ha trobat cap cop nou"_ca; estat.avisa(); return; }
            text = "Elastic Audio: "_ca + juce::String (n) + (n == 1 ? " marcador" : " marcadors") + " als cops";
        }
        else if (r == 2 || r == 3)
        {
            const int n = el::quantitza (*k, estat.projecte, r == 2 ? 1.0 : 0.5);
            if (n == 0) { estat.info = "Els marcadors ja són a la graella"_ca; estat.avisa(); return; }
            text = "Elastic Audio: "_ca + juce::String (n) + (n == 1 ? " marcador quantitzat" : " marcadors quantitzats") + (r == 3 ? " (50 %)" : "");
        }
        else if (r == 4) { el::treuMarcadors (*k, s->durada); text = "Elastic Audio: marcadors trets"_ca; }
        else if (r == 5) { el::restaura (*k); text = "Elastic Audio: àudio original"_ca; }
        else return;
        estat.desfes.abans (copia);
        estat.canviat (text);
    });
}

//==============================================================================
// 0.87: acords d'un àudio
//==============================================================================
void PantallaEstudi::treuAcords (const mibase::Clip& c0, const juce::String& nom)
{
    auto c = c0;
    auto ia = audioPreses.find (c.presa);
    if (c.midi || c.presa.empty() || ia == audioPreses.end() || ia->second == nullptr)
    {
        estat.info = "Aquest clip no té l'àudio carregat"_ca;
        estat.avisa();
        return;
    }
    juce::MouseCursor::showWaitCursor();
    std::shared_ptr<const mibase::AudioPresa> so = ia->second;
    if (mibase::elastic::actiu (c))
    {
        // Amb Elastic Audio s'escolta el so estirat (tal com sona a la línia de temps).
        so = mibase::elastic::renderitza (*so, c.warp, c.elastic);
        c.warp.clear();
    }
    const auto trobats = mibase::acordsAudio::detecta (estat.projecte, c, *so);
    juce::MouseCursor::hideWaitCursor();
    if (trobats.empty())
    {
        estat.info = "No s'hi ha trobat cap acord (silenci o massa poc clar)"_ca;
        estat.avisa();
        return;
    }
    estat.desfes.abans (estat.projecte);
    int posats = 0, dubtosos = 0;
    for (auto& a : trobats)
    {
        const int i = ed::posaAcord (estat.projecte, a.ini, a.fi, a.nom);
        if (i < 0) continue;
        ++posats;
        if (a.dubtos)
        {
            estat.projecte.pistes[(size_t) ed::pistaAcords (estat.projecte)].clips[(size_t) i].dubtos = true;
            ++dubtosos;
        }
    }
    const auto text = "Acords de «"_ca + nom + "»: " + juce::String (posats) + (posats == 1 ? " acord" : " acords")
                    + (dubtosos > 0 ? ", "_ca + juce::String (dubtosos) + " per revisar (amb «?»)"_ca : juce::String());
    estat.recalculaFiles();
    estat.canviat (text);
    estat.info = text;
    estat.avisa();
}
