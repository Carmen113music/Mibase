#include "FinestraFont.h"
#include "BancGM.h"
#include "Controls.h"
#include "Dibuix.h"
#include "EscGlobal.h"
#include "EstatEstudi.h"
#include "FinestraFlotant.h"
#include "Guitarres.h"
#include "ServeiAudio.h"
#include "SinteUI.h"
#include "Tema.h"
#include "Text.h"

#include <mibase/Edicio.h>
#include <mibase/FontInspiracio.h>
#include <mibase/GeneralMidi.h>
#include <mibase/Harmonia.h>

/* 0.97: Font d'inspiració. Tot es fa amb el teclat (fletxes, Espai, Enter, Supr, Ctrl+Z) i, si cal, amb el ratolí
   (clic tria i fa sonar, arrossegar la vora dreta canvia la durada, clic dret: durades i esborrar). La idea es desa
   dins el projecte cada cop que canvia. */
namespace
{
    using namespace mibase;
    using namespace mibase::font;
    namespace ed = mibase::edicio;

    const juce::Colour colorFont { 0xff5fd4c4 };   // aigua
    juce::Colour colorNivell (Nivell n)
    {
        return n == Nivell::Habitual ? juce::Colour (0xff7fd67a) : n == Nivell::Color ? juce::Colour (0xff6fb8ff) : juce::Colour (0xffd88aff);
    }
    juce::String txt (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

    class PanellFont : public juce::Component, private juce::Timer, private juce::ChangeListener
    {
    public:
        PanellFont (EstatEstudi& e, ServeiAudio& s, std::function<void()> aturaCanco)
            : estat (e), servei (s), aturaLaCanco (std::move (aturaCanco))
        {
            setWantsKeyboardFocus (true);
            idea = llegeixIdea (estat.projecte.fontInspiracio);
            if (idea.acords.empty()) afegeix (idea, mesProbable (tonalitat(), {}, 0), tempsCompas());

            for (auto* c : std::initializer_list<juce::Component*> { &btTonalitat, &selInstr, &btSons, &selPatroPiano, &selPatroGuitarra,
                                                                      &btPlay, &btBucle, &btDesfes, &btEnvia })
            {
                c->setWantsKeyboardFocus (false);
                c->setMouseClickGrabsKeyboardFocus (false);
                addAndMakeVisible (c);
            }
            btTonalitat.setTooltip ("Tonalitat dels suggeriments (la de la cançó o una de triada aquí). Canviar-la no toca els acords que ja hi ha"_ca);
            btTonalitat.onClick = [this] { menuTonalitat(); };
            btSons.onClick = [this] { menuSons(); };   // 1.5
            actualitzaTextSons();
            selInstr.setTriat ((int) idea.instrument);
            selInstr.onCanvi = [this] (int i) { idea.instrument = (Instr) i; posaInstrument(); mostraPatrons(); canviaSo(); desa(); };
            selPatroPiano.setTriat (idea.patroPiano);
            selPatroGuitarra.setTriat (idea.patroGuitarra);
            selPatroPiano.onCanvi = [this] (int i) { idea.patroPiano = i; canviaSo(); desa(); };
            selPatroGuitarra.onCanvi = [this] (int i) { idea.patroGuitarra = i; canviaSo(); desa(); };
            selPatroPiano.setTooltip ("Patró de piano: s'aplica a tots els acords"_ca);
            selPatroGuitarra.setTooltip ("Patró de guitarra: s'aplica a tots els acords"_ca);
            btPlay.setTooltip ("Toca la fila des del principi (Espai). Maj+Espai: des de l'acord triat"_ca);
            btPlay.onClick = [this] { commutaPlay (false); grabKeyboardFocus(); };
            btBucle.setTooltip ("En bucle: quan s'acaba, torna a començar"_ca);
            btBucle.setLlum (colorFont);
            btBucle.setClickingTogglesState (true);
            btBucle.setToggleState (idea.bucle, juce::dontSendNotification);
            btBucle.onClick = [this] { idea.bucle = btBucle.getToggleState(); canviaSo(); desa(); grabKeyboardFocus(); };
            btDesfes.setTooltip ("Desfés el darrer canvi de la fila (Ctrl+Z)"_ca);
            btDesfes.onClick = [this] { desfes(); grabKeyboardFocus(); };
            btEnvia.setTooltip ("Posa aquests acords a la pista d'Acords de la cançó, a partir del compàs on és el capçal"_ca);
            btEnvia.onClick = [this] { envia(); };

            mostraPatrons();
            posaInstrument();
            triaLloc (0, false);
            estat.addChangeListener (this);
            setSize (1060, 390);
            startTimerHz (30);
        }

        ~PanellFont() override
        {
            estat.removeChangeListener (this);
            servei.font().atura();
        }

        void visibilityChanged() override { if (isShowing()) juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellFont> (this)] { if (sp != nullptr) sp->grabKeyboardFocus(); }); }
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent&) override { arrossega = -1; }
        void mouseMove (const juce::MouseEvent& e) override
        {
            setMouseCursor (voraA (e.position) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        }
        bool keyPressed (const juce::KeyPress& k) override;
        void paint (juce::Graphics& g) override;
        void resized() override;

    private:
        EstatEstudi& estat;
        ServeiAudio& servei;
        std::function<void()> aturaLaCanco;
        Idea idea;
        int sel = 0;
        std::vector<Suggeriment> llista;      // les alternatives del lloc triat (no canvia mentre s'hi navega)
        int cursor = 0;
        std::vector<Idea> passos;             // desfer
        bool explorant = false;               // ja s'ha desat el «com era» d'aquesta exploració amb amunt/avall
        int sona = 0;                         // 0 res, 1 la fila, 2 l'escolta d'un acord
        double desplac = 0.0;                 // scroll horitzontal (px)
        int arrossega = -1;                   // vora que s'arrossega
        double tempsArrossega = 0.0;
        std::string bancSonant;

        Boto btTonalitat { {}, std::nullopt, Boto::Estil::Secundari };
        Segmentat selInstr { { "Piano", "Guitarra" } };
        Segmentat selPatroPiano { { "Arpegi ondulat", "Arpegi Alberti", "Pop a negres", "Rock a corxeres" } };
        Segmentat selPatroGuitarra { { "Arpegi", "Travis picking", "Rasgueig pop", "Indie rock" } };
        Boto btPlay { {}, dibuix::Icona::Play, Boto::Estil::Secundari };
        Boto btBucle { "Bucle", dibuix::Icona::Bucle, Boto::Estil::Tecla };
        Boto btDesfes { {}, dibuix::Icona::Desfes, Boto::Estil::Secundari };
        Boto btEnvia { "ENVIA A LA PISTA D'ACORDS"_ca, std::nullopt, Boto::Estil::Primari };
        Boto btSons { {}, dibuix::Icona::Ajustos, Boto::Estil::Secundari };   // 1.5: quin so fan el piano i la guitarra
        juce::Rectangle<int> zonaCap, zonaFila, zonaPeu;

        // ---- mides ----
        int tempsCompas() const { return juce::jmax (1, estat.projecte.compasNum); }
        double pxTemps() const { return juce::jlimit (22.0, 60.0, 190.0 / tempsCompas()); }
        float xDe (double temps) const { return (float) (zonaFila.getX() + 24 + temps * pxTemps() - desplac); }
        juce::Rectangle<float> rectAcord (int i) const
        {
            const float x0 = xDe (inici (idea, i)), x1 = xDe (inici (idea, i) + (i < (int) idea.acords.size() ? idea.acords[(size_t) i].temps : (double) tempsCompas()));
            const float y = (float) zonaFila.getY() + 70.0f;
            return { x0 + 2.0f, y, juce::jmax (8.0f, x1 - x0 - 4.0f), 96.0f };
        }
        int voraA (juce::Point<float> p) const
        {
            for (int i = 0; i < (int) idea.acords.size(); ++i)
            {
                const auto r = rectAcord (i);
                if (std::abs (p.x - r.getRight()) < 6.0f && p.y >= r.getY() && p.y <= r.getBottom()) return i;
            }
            return -1;
        }

        Tonalitat tonalitat() const { return idea.tePropia ? idea.tonalitat : estat.projecte.tonalitat; }

        // ---- canvis ----
        void guarda() { passos.push_back (idea); if (passos.size() > 200) passos.erase (passos.begin()); }
        void desa()
        {
            estat.projecte.fontInspiracio = textIdea (idea);
            if (estat.onCanviat) estat.onCanviat ({});
            repaint();
        }
        void desfes()
        {
            if (passos.empty()) return;
            idea = passos.back();
            passos.pop_back();
            explorant = false;
            selInstr.setTriat ((int) idea.instrument);
            selPatroPiano.setTriat (idea.patroPiano);
            selPatroGuitarra.setTriat (idea.patroGuitarra);
            btBucle.setToggleState (idea.bucle, juce::dontSendNotification);
            mostraPatrons();
            posaInstrument();
            triaLloc (juce::jlimit (0, juce::jmax (0, (int) idea.acords.size() - 1), sel), true);
            canviaSo();
            desa();
        }

        /** Tria el lloc «i» i calcula les seves alternatives. */
        void triaLloc (int i, bool escolta)
        {
            sel = juce::jlimit (0, juce::jmax (0, (int) idea.acords.size() - 1), i);
            explorant = false;
            recalculaLlista();
            assegura();
            if (escolta) escoltaTriat();
            repaint();
        }
        void recalculaLlista()
        {
            llista = suggeriments (tonalitat(), idea.acords, sel);
            const std::string ara = sel < (int) idea.acords.size() ? idea.acords[(size_t) sel].acord : std::string();
            cursor = -1;
            for (int k = 0; k < (int) llista.size(); ++k) if (llista[(size_t) k].acord == ara) { cursor = k; break; }
            if (cursor < 0) { llista.insert (llista.begin(), { ara, Nivell::Color, 0.0 }); cursor = 0; }
            btTonalitat.setButtonText ("Tonalitat: "_ca + txt (harmonia::nomTonalitat (tonalitat())) + (idea.tePropia ? juce::String() : " (cançó)"_ca));
        }
        void prova (int pas)
        {
            if (idea.acords.empty() || llista.empty()) return;
            const int c = juce::jlimit (0, (int) llista.size() - 1, cursor + pas);
            if (c == cursor) return;
            if (! explorant) { guarda(); explorant = true; }
            cursor = c;
            idea.acords[(size_t) sel].acord = llista[(size_t) c].acord;
            canviaSo();
            escoltaTriat();
            desa();
        }
        void seguent()
        {
            if (sel + 1 < (int) idea.acords.size()) { triaLloc (sel + 1, true); return; }
            guarda();
            afegeix (idea, mesProbable (tonalitat(), idea.acords, (int) idea.acords.size()), tempsCompas());
            triaLloc ((int) idea.acords.size() - 1, false);
            canviaSo();
            escoltaTriat();
            desa();
        }
        void esborra()
        {
            if (idea.acords.size() <= 1) return;   // sempre en queda un
            guarda();
            treu (idea, sel);
            triaLloc (sel >= (int) idea.acords.size() ? sel - 1 : sel, false);
            canviaSo();
            desa();
        }
        void mou (double d)
        {
            Idea abans = idea;
            if (! mouInici (idea, sel, d)) return;
            passos.push_back (abans);
            explorant = false;
            canviaSo();
            desa();
        }
        void canviaLlarg (double d)
        {
            Idea abans = idea;
            if (! canviaDurada (idea, sel, d, tempsCompas())) return;
            passos.push_back (abans);
            explorant = false;
            canviaSo();
            desa();
        }
        void posaDurada (int i, double temps)
        {
            if (i < 0 || i >= (int) idea.acords.size()) return;
            guarda();
            idea.acords[(size_t) i].temps = juce::jmax (tempsMinim, temps);
            canviaSo();
            desa();
        }
        void assegura()
        {
            if (zonaFila.isEmpty()) return;
            const auto r = rectAcord (sel);
            if (r.getRight() > (float) zonaFila.getRight() - 60.0f) desplac += r.getRight() - ((float) zonaFila.getRight() - 60.0f);
            if (r.getX() < (float) zonaFila.getX() + 24.0f) desplac -= ((float) zonaFila.getX() + 24.0f) - r.getX();
            desplac = juce::jmax (0.0, desplac);
        }

        // ---- so ----
        void posaInstrument()
        {
            auto banc = bancgm::banc();
            if (banc == nullptr)
            {
                bancgm::assegura (this, {});
                servei.font().posaInstrument (nullptr);
                bancSonant.clear();
                return;
            }
            // 1.5: el so que has triat amb el botó de configuració (si és una guitarra .sf2 que ja no hi és, el de sempre)
            const auto so = soTriat (idea.instrument);
            if (gm::RefSf2 ref; gm::llegeixRef (so, ref))
                if (auto b = guitarres::banc (ref.fitxer))
                {
                    const std::string clau = so + "|" + guitarres::signatura (ref.fitxer);
                    if (clau == bancSonant) return;
                    bancSonant = clau;
                    servei.font().posaInstrument (std::make_shared<gm::SinteGM> (b, ref.programa, ref.banc));
                    return;
                }
            int prog = idea.instrument == Instr::Guitarra ? programaGuitarra : programaPiano;
            if (so.rfind ("gm:", 0) == 0) prog = juce::jlimit (0, 127, juce::String (so.substr (3)).getIntValue());
            const std::string clau = bancgm::signatura() + "#" + std::to_string (prog);
            if (clau == bancSonant) return;
            bancSonant = clau;
            servei.font().posaInstrument (std::make_shared<gm::SinteGM> (banc, prog));
        }

        // ---- 1.5: quin so fa cada instrument (es recorda per a tots els projectes) ----
        static const char* clauAjust (Instr i) { return i == Instr::Guitarra ? "fontSoGuitarra" : "fontSoPiano"; }
        std::string soTriat (Instr i)
        {
            const auto v = servei.getAjustos().getValue (clauAjust (i)).toStdString();
            return v.empty() ? "gm:" + std::to_string (i == Instr::Guitarra ? programaGuitarra : programaPiano) : v;
        }
        juce::String nomSo (Instr i)
        {
            const auto so = soTriat (i);
            if (gm::RefSf2 r; gm::llegeixRef (so, r)) return juce::String::fromUTF8 (r.nom.c_str());
            const int p = so.rfind ("gm:", 0) == 0 ? juce::jlimit (0, 127, juce::String (so.substr (3)).getIntValue()) : 0;
            return juce::String::fromUTF8 (gm::nomPrograma (p));
        }
        void posaSo (Instr i, const std::string& so)
        {
            servei.getAjustos().setValue (clauAjust (i), juce::String (so));
            servei.getAjustos().saveIfNeeded();
            bancSonant.clear();
            posaInstrument();
            actualitzaTextSons();
        }
        void actualitzaTextSons()
        {
            btSons.setTooltip ("Quin so fan el piano i la guitarra (es recorda per a tots els projectes).\n"_ca
                               + "Piano: "_ca + nomSo (Instr::Piano) + "\n" + "Guitarra: "_ca + nomSo (Instr::Guitarra));
            selInstr.setTooltip ("Amb què sona: "_ca + nomSo (Instr::Piano) + " o "_ca + nomSo (Instr::Guitarra) + ". El botó del costat canvia els sons."_ca);
        }
        void menuSons()
        {
            const auto sp = soTriat (Instr::Piano), sg = soTriat (Instr::Guitarra);
            auto prog = [] (const std::string& so) { return so.rfind ("gm:", 0) == 0 ? juce::String (so.substr (3)).getIntValue() : -1; };
            juce::PopupMenu m;
            m.addSectionHeader ("Piano: "_ca + nomSo (Instr::Piano));
            m.addSubMenu ("So del piano (General MIDI)"_ca, bancgm::menu (1000, prog (sp)));
            m.addSeparator();
            m.addSectionHeader ("Guitarra: "_ca + nomSo (Instr::Guitarra));
            m.addSubMenu ("So de la guitarra (General MIDI)"_ca, bancgm::menu (2000, prog (sg)));
            m.addSubMenu ("Les meves guitarres (.sf2)"_ca, guitarres::menu (3000, sg));
            m.addSeparator();
            m.addItem (9000, "Torna als sons de sempre (piano de cua i guitarra de niló)"_ca);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btSons), [this] (int r)
            {
                if (r >= 1000 && r < 1128) posaSo (Instr::Piano, "gm:" + std::to_string (r - 1000));
                else if (r >= 2000 && r < 2128) posaSo (Instr::Guitarra, "gm:" + std::to_string (r - 2000));
                else if (r >= 3000 && r < 9000)
                {
                    const auto so = guitarres::tria (r - 3000);
                    if (! so.empty()) posaSo (Instr::Guitarra, so);
                }
                else if (r == 9000)
                {
                    servei.getAjustos().removeValue ("fontSoPiano");
                    servei.getAjustos().removeValue ("fontSoGuitarra");
                    posaSo (Instr::Piano, "gm:" + std::to_string (programaPiano));
                    posaSo (Instr::Guitarra, "gm:" + std::to_string (programaGuitarra));
                }
                if (r > 0) canviaSo();
            });
        }
        long long ticksTemps() const { return font::ticksTemps (estat.projecte.compasDen); }
        double tempo() const { return estat.projecte.tempo; }
        void preparaSo()
        {
            servei.parellFont.store (estat.projecte.sortidaMaster);
            if (bancSonant.empty()) posaInstrument();
        }
        std::shared_ptr<const Programa> programaFila() const
        {
            return programa (notes (idea, estat.projecte.compasNum, estat.projecte.compasDen, tempo()),
                             juce::jmax (1LL, (long long) std::llround (durada (idea) * (double) ticksTemps())), idea.bucle, tempo());
        }
        /** Quan canvia alguna cosa: si la fila sona, sona la nova des d'on era (sense tallar). */
        void canviaSo()
        {
            if (sona == 1) servei.font().toca (programaFila(), false);
        }
        void escoltaTriat()
        {
            if (sona == 1 || idea.acords.empty()) return;   // mentre sona la fila, el canvi se sent quan hi arriba
            preparaSo();
            const auto& k = idea.acords[(size_t) sel];
            const long long llarg = (long long) std::llround (k.temps * (double) ticksTemps());
            servei.font().toca (programa (notes (idea, estat.projecte.compasNum, estat.projecte.compasDen, tempo(), sel), llarg, false, tempo()), true);
            sona = 2;
        }
        void commutaPlay (bool desDelTriat)
        {
            if (sona == 1) { servei.font().atura(); sona = 0; repaint(); return; }
            if (aturaLaCanco) aturaLaCanco();
            preparaSo();
            const double desDe = desDelTriat ? inici (idea, sel) * (double) ticksTemps() : 0.0;
            servei.font().toca (programaFila(), true, desDe);
            sona = 1;
            repaint();
        }
        void timerCallback() override
        {
            if (sona != 0 && ! servei.font().sonant()) { sona = 0; repaint(); }
            btPlay.setIcona (sona == 1 ? dibuix::Icona::Atura : dibuix::Icona::Play);
            btDesfes.setEnabled (! passos.empty());
            if (sona == 1) repaint (zonaFila);
        }
        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            // la tonalitat de la cançó (o el compàs) pot haver canviat: els suggeriments es tornen a fer
            if (! idea.tePropia) { recalculaLlista(); repaint(); }
        }

        void mostraPatrons()
        {
            selPatroPiano.setVisible (idea.instrument == Instr::Piano);
            selPatroGuitarra.setVisible (idea.instrument == Instr::Guitarra);
        }

        void menuTonalitat()
        {
            juce::PopupMenu m;
            m.addItem (1, "La de la cançó: "_ca + txt (harmonia::nomTonalitat (estat.projecte.tonalitat)), true, ! idea.tePropia);
            const int major = juce::jmax (0, harmonia::escalaPerId ("major")), menor = juce::jmax (0, harmonia::escalaPerId ("menor"));
            juce::PopupMenu ma, mi;
            for (int to = 0; to < 12; ++to)
            {
                const Tonalitat a { to, major }, b { to, menor };
                ma.addItem (100 + to, txt (harmonia::nomTonalitat (a)), true, idea.tePropia && idea.tonalitat == a);
                mi.addItem (200 + to, txt (harmonia::nomTonalitat (b)), true, idea.tePropia && idea.tonalitat == b);
            }
            m.addSubMenu ("Major", ma);
            m.addSubMenu ("Menor", mi);
            juce::Component::SafePointer<PanellFont> jo (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btTonalitat), [jo, major, menor] (int r)
            {
                if (jo == nullptr || r == 0) return;
                if (r == 1) jo->idea.tePropia = false;
                else { jo->idea.tePropia = true; jo->idea.tonalitat = { r % 100, r >= 200 ? menor : major }; }
                jo->recalculaLlista();   // els acords no es toquen
                jo->desa();
                jo->grabKeyboardFocus();
            });
        }

        void menuAcord (int i)
        {
            juce::PopupMenu m;
            const int tc = tempsCompas();
            const auto& k = idea.acords[(size_t) i];
            m.addItem (1, "Mig compàs"_ca, true, std::abs (k.temps - tc * 0.5) < 1e-6);
            m.addItem (2, "Un compàs"_ca, true, std::abs (k.temps - tc) < 1e-6);
            m.addItem (3, "Dos compassos"_ca, true, std::abs (k.temps - tc * 2.0) < 1e-6);
            m.addItem (4, "Quatre compassos"_ca, true, std::abs (k.temps - tc * 4.0) < 1e-6);
            m.addSeparator();
            m.addItem (9, "Esborra l'acord (Supr)"_ca, idea.acords.size() > 1);
            juce::Component::SafePointer<PanellFont> jo (this);
            m.showMenuAsync ({}, [jo, i, tc] (int r)
            {
                if (jo == nullptr || r == 0) return;
                if (r == 9) { jo->sel = i; jo->esborra(); }
                else jo->posaDurada (i, tc * (r == 1 ? 0.5 : r == 2 ? 1.0 : r == 3 ? 2.0 : 4.0));
                jo->grabKeyboardFocus();
            });
        }

        // ---- enviar a la pista d'Acords ----
        void envia()
        {
            auto& p = estat.projecte;
            const double c0 = juce::jmax (1.0, std::floor (ed::compasDeSegons (p, estat.capcal) + 1e-9));
            const double tc = (double) tempsCompas();
            const double s0 = ed::segonsDeCompas (p, c0), s1 = ed::segonsDeCompas (p, c0 + durada (idea) / tc);
            bool trepitja = false;
            if (const int ia = ed::pistaAcords (p); ia >= 0)
                for (auto& c : p.pistes[(size_t) ia].clips)
                    if (c.esAcord() && c.iniciVisible() < s1 - 1e-9 && c.fiVisible() > s0 + 1e-9) trepitja = true;
            const juce::String on = "a partir del compàs "_ca + juce::String (ed::numCompas ((int) c0));
            if (! trepitja) { fesEnvia (c0); return; }
            juce::Component::SafePointer<PanellFont> jo (this);
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Ja hi ha acords en aquest tros"_ca,
                "La pista d'Acords ja en té "_ca + on + ". Els vols substituir pels de la Font d'inspiració? (Es pot desfer amb Ctrl+Z.)"_ca,
                "Substitueix"_ca, "Cancel·la"_ca, this, juce::ModalCallbackFunction::create ([jo, c0] (int r) { if (jo != nullptr && r == 1) jo->fesEnvia (c0); }));
        }
        void fesEnvia (double c0)
        {
            auto& p = estat.projecte;
            const double tc = (double) tempsCompas();
            estat.desfes.abans (p);
            int posats = 0;
            for (int i = 0; i < (int) idea.acords.size(); ++i)
            {
                const auto& k = idea.acords[(size_t) i];
                const double a = ed::segonsDeCompas (p, c0 + inici (idea, i) / tc), b = ed::segonsDeCompas (p, c0 + (inici (idea, i) + k.temps) / tc);
                if (k.acord.empty())
                {
                    // el silenci: el que hi hagués en aquest tros de la pista d'Acords se'n va
                    if (const int ia = ed::pistaAcords (p); ia >= 0)
                    {
                        std::vector<Clip> resta;
                        for (auto& c : p.pistes[(size_t) ia].clips)
                        {
                            const double x = c.iniciVisible(), y = c.fiVisible();
                            if (y <= a + 1e-9 || x >= b - 1e-9) { resta.push_back (c); continue; }
                            if (x < a) { auto e = c; e.fi = a - e.pos; resta.push_back (e); }
                            if (y > b) { auto d = c; d.inici = b - d.pos; resta.push_back (d); }
                        }
                        p.pistes[(size_t) ia].clips = std::move (resta);
                    }
                    continue;
                }
                if (ed::posaAcord (p, a, b, k.acord) >= 0) ++posats;
            }
            // la tonalitat triada aquí passa a la cançó si la cançó no en tenia cap de triada
            juce::String extra;
            if (idea.tePropia && p.tonalitatAuto && ! (p.tonalitat == idea.tonalitat))
            {
                p.tonalitatAuto = false;
                p.tonalitat = idea.tonalitat;
                extra = " · tonalitat: "_ca + txt (harmonia::nomTonalitat (idea.tonalitat));
            }
            const auto text = "Font d'inspiració: "_ca + juce::String (posats) + (posats == 1 ? " acord" : " acords")
                            + " a la pista d'Acords des del compàs "_ca + juce::String (ed::numCompas ((int) c0)) + extra;
            estat.recalculaFiles();
            estat.canviat (text);
            estat.info = text;
            estat.avisa();
        }
    };

    bool PanellFont::keyPressed (const juce::KeyPress& k)
    {
        const auto m = k.getModifiers();
        const int c = k.getKeyCode();
        const bool ctrl = m.isCommandDown() || m.isCtrlDown(), maj = m.isShiftDown();
        if (c == juce::KeyPress::leftKey || c == juce::KeyPress::rightKey)
        {
            const int dir = c == juce::KeyPress::rightKey ? 1 : -1;
            if (ctrl) { mou (dir * (maj ? 1.0 : 0.5)); return true; }
            if (maj) { canviaLlarg (dir * 0.5); return true; }
            if (dir > 0) seguent(); else if (sel > 0) triaLloc (sel - 1, true);
            return true;
        }
        if (c == juce::KeyPress::upKey)   { prova (+1); return true; }
        if (c == juce::KeyPress::downKey) { prova (-1); return true; }
        if (c == juce::KeyPress::spaceKey) { commutaPlay (maj); return true; }
        if (c == juce::KeyPress::returnKey) { if (sona != 1) { sona = 0; escoltaTriat(); } return true; }
        if (c == juce::KeyPress::deleteKey || c == juce::KeyPress::backspaceKey) { esborra(); return true; }
        if (ctrl && (c == 'Z' || c == 'z')) { desfes(); return true; }
        if (c == juce::KeyPress::F8Key)   // 1.16: F8 (la F9 ara és la taula de mescles)
        {
            if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) w->closeButtonPressed();   // es tanca fora d'aquí
            return true;
        }
        if (c == juce::KeyPress::homeKey) { triaLloc (0, true); return true; }
        if (c == juce::KeyPress::endKey) { triaLloc ((int) idea.acords.size() - 1, true); return true; }
        return false;   // la resta (Esc…) va a la finestra
    }

    void PanellFont::mouseDown (const juce::MouseEvent& e)
    {
        grabKeyboardFocus();
        if (! zonaFila.contains (e.getPosition())) return;
        if (const int v = voraA (e.position); v >= 0 && ! e.mods.isPopupMenu())
        {
            guarda();
            arrossega = v;
            tempsArrossega = idea.acords[(size_t) v].temps;
            return;
        }
        for (int i = 0; i < (int) idea.acords.size(); ++i)
            if (rectAcord (i).expanded (2.0f, 30.0f).contains (e.position))
            {
                if (e.mods.isPopupMenu()) { triaLloc (i, false); menuAcord (i); }
                else triaLloc (i, true);
                return;
            }
        // el «+» del final
        if (rectAcord ((int) idea.acords.size()).contains (e.position)) { sel = (int) idea.acords.size() - 1; seguent(); }
    }

    void PanellFont::mouseDrag (const juce::MouseEvent& e)
    {
        if (arrossega < 0) return;
        const double d = (double) e.getDistanceFromDragStartX() / pxTemps();
        const double t = juce::jmax (tempsMinim, std::round ((tempsArrossega + d) * 2.0) / 2.0);
        if (std::abs (t - idea.acords[(size_t) arrossega].temps) < 1e-9) return;
        idea.acords[(size_t) arrossega].temps = t;
        canviaSo();
        desa();
    }

    void PanellFont::resized()
    {
        auto b = getLocalBounds();
        zonaCap = b.removeFromTop (92);
        zonaPeu = b.removeFromBottom (64);
        zonaFila = b.reduced (16, 0);
        auto c = zonaCap.reduced (18, 0);
        c.removeFromTop (46);
        auto f = c.removeFromTop (34);
        btTonalitat.setBounds (f.removeFromLeft (250));
        f.removeFromLeft (14);
        selInstr.setBounds (f.removeFromLeft (selInstr.ampladaIdeal() + 10));
        f.removeFromLeft (4);
        btSons.setBounds (f.removeFromLeft (34));   // 1.5
        f.removeFromLeft (10);
        const auto rp = f.removeFromLeft (juce::jmax (selPatroPiano.ampladaIdeal(), selPatroGuitarra.ampladaIdeal()) + 10);
        selPatroPiano.setBounds (rp);
        selPatroGuitarra.setBounds (rp);
        auto p = zonaPeu.reduced (18, 14);
        btEnvia.setBounds (p.removeFromRight (btEnvia.ampladaIdeal() + 16));
        btPlay.setBounds (p.removeFromLeft (44));
        p.removeFromLeft (6);
        btBucle.setBounds (p.removeFromLeft (96));
        p.removeFromLeft (6);
        btDesfes.setBounds (p.removeFromLeft (40));
        assegura();
    }

    void PanellFont::paint (juce::Graphics& g)
    {
        using namespace sinteui;
        // fons: laca negra i capçalera de carboni, com els sintes
        g.drawImageAt (textura (Textura::Laca, getWidth(), getHeight()), 0, 0);
        g.drawImageAt (textura (Textura::Carboni, getWidth(), zonaCap.getHeight()), 0, 0);
        g.setColour (colorFont.withAlpha (0.8f));
        g.fillRect (0, zonaCap.getBottom() - 2, getWidth(), 2);
        g.setFont (dibuix::lletra (20.0f, true));
        g.setColour (colorFont);
        textEspaiat (g, "FONT D'INSPIRACIÓ"_ca, juce::Rectangle<float> (20.0f, 10.0f, 400.0f, 30.0f), juce::Justification::centredLeft, 3.0f);
        g.setFont (dibuix::lletra (11.5f));
        g.setColour (tema::suau);
        g.drawText ("De cap idea a una progressió: fletxes per triar i provar, Espai per escoltar-la."_ca,
                    juce::Rectangle<int> (330, 14, getWidth() - 350, 24), juce::Justification::centredRight);

        // ---- la fila ----
        g.saveState();
        g.reduceClipRegion (zonaFila);
        const int tc = tempsCompas();
        const double total = durada (idea) + tc;
        const float yRegla = (float) zonaFila.getY() + 14.0f;
        for (int t = 0; t <= (int) std::ceil (total); ++t)
        {
            const float x = xDe (t);
            const bool compas = t % tc == 0;
            g.setColour (compas ? tema::linia2 : tema::linia);
            g.fillRect (x, yRegla + (compas ? 0.0f : 12.0f), 1.0f, compas ? 22.0f : 8.0f);
            if (compas)
            {
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (11.0f, true));
                g.drawText (juce::String (t / tc + 1), juce::Rectangle<float> (x + 4.0f, yRegla - 2.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
            }
        }
        const double posTemps = sona == 1 ? servei.font().posicio() / (double) ticksTemps() : -1.0;
        for (int i = 0; i < (int) idea.acords.size(); ++i)
        {
            const auto& k = idea.acords[(size_t) i];
            auto r = rectAcord (i);
            const bool triat = i == sel;
            const bool sonant = posTemps >= inici (idea, i) && posTemps < inici (idea, i) + k.temps;
            Nivell niv = Nivell::Habitual;
            for (auto& s : suggeriments (tonalitat(), idea.acords, i)) if (s.acord == k.acord) { niv = s.nivell; break; }
            juce::Colour fons = k.acord.empty() ? juce::Colour (0xff1c1c22) : juce::Colour (0xff262a31);
            if (sonant) fons = fons.interpolatedWith (colorFont, 0.35f);
            g.setGradientFill (juce::ColourGradient (fons.brighter (0.15f), r.getX(), r.getY(), fons.darker (0.25f), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 7.0f);
            if (triat)
            {
                g.setColour (tema::accent.withAlpha (0.25f));
                g.drawRoundedRectangle (r.expanded (3.0f), 9.0f, 4.0f);
            }
            g.setColour (triat ? tema::accent : sonant ? colorFont : tema::linia2);
            g.drawRoundedRectangle (r, 7.0f, triat ? 2.0f : 1.0f);
            // nom
            g.setColour (k.acord.empty() ? tema::suau : tema::text);
            g.setFont (dibuix::lletra (r.getWidth() < 60.0f ? 17.0f : 26.0f, true));
            g.drawFittedText (k.acord.empty() ? juce::String ("—") : txt (k.acord), r.withTrimmedBottom (26.0f).toNearestInt(), juce::Justification::centred, 1);
            // durada
            const double comp = k.temps / tc;
            auto enter = [] (double x) { return std::abs (x - std::round (x)) < 1e-6; };
            auto num = [enter] (double x) { return enter (x) ? juce::String ((int) std::round (x)) : juce::String (x, 1); };
            juce::String d = enter (comp) ? num (comp) + (std::abs (comp - 1.0) < 1e-6 ? " compàs"_ca : " compassos"_ca)
                           : std::abs (comp - 0.5) < 1e-6 ? juce::String::fromUTF8 ("\xc2\xbd") + " compàs"_ca
                           : num (k.temps) + " temps";
            if (r.getWidth() < 60.0f) d = num (k.temps);
            g.setFont (dibuix::lletra (10.5f));
            g.setColour (tema::suau);
            g.drawText (d, r.removeFromBottom (24.0f).toNearestInt(), juce::Justification::centred);
            // tipus d'acord: un punt de color (habitual, color, atrevit)
            if (! k.acord.empty())
            {
                g.setColour (colorNivell (niv));
                g.fillEllipse (r.getRight() - 13.0f, r.getY() + 7.0f, 6.0f, 6.0f);
            }
            // les alternatives veïnes del triat: amunt la següent, avall l'anterior
            if (triat)
            {
                g.setFont (dibuix::lletra (13.0f, true));
                const auto rr = rectAcord (i);
                if (cursor + 1 < (int) llista.size())
                {
                    const auto& s = llista[(size_t) cursor + 1];
                    g.setColour (colorNivell (s.nivell).withAlpha (0.55f));
                    g.drawText (juce::String::fromUTF8 ("\xe2\x96\xb2 ") + (s.acord.empty() ? "silenci"_ca : txt (s.acord)),
                                juce::Rectangle<float> (rr.getX() - 20.0f, rr.getY() - 30.0f, rr.getWidth() + 40.0f, 22.0f).toNearestInt(), juce::Justification::centred);
                }
                if (cursor > 0)
                {
                    const auto& s = llista[(size_t) cursor - 1];
                    g.setColour (colorNivell (s.nivell).withAlpha (0.55f));
                    g.drawText (juce::String::fromUTF8 ("\xe2\x96\xbc ") + (s.acord.empty() ? "silenci"_ca : txt (s.acord)),
                                juce::Rectangle<float> (rr.getX() - 20.0f, rr.getBottom() + 8.0f, rr.getWidth() + 40.0f, 22.0f).toNearestInt(), juce::Justification::centred);
                }
            }
        }
        // el «+» del final
        {
            const auto r = rectAcord ((int) idea.acords.size());
            g.setColour (tema::linia2);
            const float dash[] = { 5.0f, 4.0f };
            juce::Path pth; pth.addRoundedRectangle (r, 7.0f);
            juce::PathStrokeType (1.0f).createDashedStroke (pth, pth, dash, 2);
            g.fillPath (pth);
            g.setFont (dibuix::lletra (26.0f));
            g.drawText ("+", r.toNearestInt(), juce::Justification::centred);
        }
        // el capçal
        if (posTemps >= 0.0)
        {
            const float x = xDe (posTemps);
            g.setColour (colorFont);
            g.fillRect (x - 1.0f, yRegla, 2.0f, (float) zonaFila.getBottom() - yRegla - 6.0f);
        }
        g.restoreState();

        // ---- peu: les tecles i la llegenda dels colors ----
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (zonaPeu);
        auto t = zonaPeu.reduced (18, 14);
        t.removeFromLeft (44 + 6 + 96 + 6 + 40 + 20);
        t.removeFromRight (btEnvia.ampladaIdeal() + 32);
        g.setFont (dibuix::lletra (11.0f));
        g.setColour (tema::suau);
        auto l1 = t.removeFromTop (t.getHeight() / 2);
        g.drawText (juce::String::fromUTF8 ("\xe2\x86\x90 \xe2\x86\x92 tria (\xe2\x86\x92 al final: un acord nou)  \xc2\xb7  \xe2\x86\x91 \xe2\x86\x93 alternatives  \xc2\xb7  "
                                            "Ctrl \xe2\x86\x90 \xe2\x86\x92 mou l'entrada \xc2\xbd temps (amb Maj, 1 temps)  \xc2\xb7  Maj \xe2\x86\x90 \xe2\x86\x92 durada"),
                    l1, juce::Justification::centredLeft);
        auto l2 = t;
        g.drawText (juce::String::fromUTF8 ("Espai toca / para  \xc2\xb7  Enter escolta  \xc2\xb7  Supr esborra  \xc2\xb7  Ctrl+Z desfà  \xc2\xb7  "),
                    l2.removeFromLeft (360), juce::Justification::centredLeft);
        for (auto [n, s] : { std::pair { Nivell::Habitual, "habitual" }, std::pair { Nivell::Color, "color" }, std::pair { Nivell::Atrevit, "atrevit" } })
        {
            g.setColour (colorNivell (n));
            g.fillEllipse ((float) l2.getX(), (float) l2.getCentreY() - 3.0f, 6.0f, 6.0f);
            l2.removeFromLeft (10);
            g.setColour (tema::suau);
            g.drawText (s, l2.removeFromLeft (62), juce::Justification::centredLeft);
        }
    }

    class FinestraFont : public juce::DocumentWindow
    {
    public:
        FinestraFont (PanellFont* p, std::function<void()> enTancar)
            : juce::DocumentWindow ("Font d'inspiració"_ca, juce::Colour (0xff121316), juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (p, true);
            setResizable (true, false);
            setResizeLimits (820, 360, 2400, 520);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { if (t) t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraFont (EstatEstudi& estat, ServeiAudio& servei,
                                                        std::function<void()> aturaCanco, std::function<void()> enTancar)
{
    auto f = std::make_unique<FinestraFont> (new PanellFont (estat, servei, std::move (aturaCanco)), std::move (enTancar));
    quedaSobreMibase (*f);
    return f;
}
