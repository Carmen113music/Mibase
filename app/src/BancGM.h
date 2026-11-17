#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <mibase/GeneralMidi.h>
#include "Text.h"

/** 0.31: el banc de sons General MIDI (un .sf2). Es busca a Documents\Mibase\SoundFonts (primer FluidR3_GM.sf2; si no,
    el primer .sf2 que hi hagi) i, si no n'hi ha cap, a la carpeta «SoundFonts» del costat del programa.
    Si no es troba, es pot baixar el FluidR3 GM (148 MB, llicència MIT) de GitHub. */
namespace bancgm
{
    inline juce::File carpeta()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("SoundFonts");
    }

    inline const char* nomFluid() { return "FluidR3_GM.sf2"; }

    inline juce::File fitxer()
    {
        for (const auto& dir : { carpeta(),
                                 juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getChildFile ("SoundFonts") })
        {
            if (! dir.isDirectory()) continue;
            if (auto f = dir.getChildFile (nomFluid()); f.existsAsFile() && f.getSize() > 1000000) return f;
            auto v = dir.findChildFiles (juce::File::findFiles, false, "*.sf2");
            v.sort();
            for (auto& f : v) if (f.getSize() > 100000) return f;
        }
        return {};
    }

    inline bool hiEs() { return fitxer().existsAsFile(); }

    /** El banc llegit (un sol cop; si canvia el fitxer, es torna a llegir). nullptr si no n'hi ha. Fil de la interfície. */
    inline std::shared_ptr<mibase::gm::Banc> banc()
    {
        static std::shared_ptr<mibase::gm::Banc> b;
        static juce::String clau;
        const auto f = fitxer();
        if (! f.existsAsFile()) return nullptr;
        const auto k = f.getFullPathName() + "|" + juce::String (f.getLastModificationTime().toMilliseconds());
        if (b == nullptr || k != clau)
        {
            // es llegeix amb JUCE (camins amb accents a Windows) i el banc es fa des de la memòria
            juce::MemoryBlock dades;
            b = f.loadFileAsData (dades) ? mibase::gm::llegeixMemoria (dades.getData(), (int) dades.getSize()) : nullptr;
            clau = k;
        }
        return b;
    }

    /** Signatura del banc (canvia si se'n posa un altre): per saber quan cal tornar a fer els instruments. */
    inline std::string signatura()
    {
        const auto f = fitxer();
        return f.existsAsFile() ? (f.getFullPathName() + "|" + juce::String (f.getLastModificationTime().toMilliseconds())).toStdString() : std::string();
    }

    /** Submenú amb les 16 famílies i els 128 sons. Ids: base + programa. */
    inline juce::PopupMenu menu (int base, int actual)
    {
        juce::PopupMenu m;
        for (int f = 0; f < 16; ++f)
        {
            juce::PopupMenu sub;
            for (int p = f * 8; p < f * 8 + 8; ++p)
                sub.addItem (base + p, juce::String (p + 1) + "  " + juce::String::fromUTF8 (mibase::gm::nomPrograma (p)), true, p == actual);
            m.addSubMenu (juce::String::fromUTF8 (mibase::gm::nomFamilia (f)), sub, true, juce::Image(), actual >= f * 8 && actual < f * 8 + 8);
        }
        return m;
    }

    /** Qui s'ha d'avisar quan s'acaba de baixar el banc (per tornar a fer sonar les pistes) i on es diu com va. */
    inline std::function<void()>& enBaixat() { static std::function<void()> f; return f; }
    inline std::function<void (const juce::String&)>& enProgres() { static std::function<void (const juce::String&)> f; return f; }

    /** La baixada (n'hi ha com a molt una alhora). */
    class Baixada : private juce::URL::DownloadTaskListener
    {
    public:
        static Baixada& una() { static Baixada b; return b; }
        bool enMarxa() const { return tasca != nullptr; }

        void comenca()
        {
            if (tasca != nullptr) return;
            carpeta().createDirectory();
            temporal = carpeta().getChildFile (juce::String (nomFluid()) + ".part");
            temporal.deleteFile();
            ultim = -1;
            avisa ("Baixant el banc de sons General MIDI (148 MB)…"_ca);
            tasca = juce::URL ("https://github.com/pianobooster/fluid-soundfont/releases/download/v3.1/FluidR3_GM.sf2")
                        .downloadToFile (temporal, juce::URL::DownloadTaskOptions().withListener (this));
            if (tasca == nullptr) temporal.deleteFile();
            if (tasca == nullptr) avisa ("No s'ha pogut començar a baixar el banc General MIDI (hi ha connexió?)."_ca);
        }

    private:
        void progress (juce::URL::DownloadTask*, juce::int64 baixat, juce::int64 total) override
        {
            const int pc = total > 0 ? (int) (100 * baixat / total) : (int) (baixat / 1480000);
            if (pc / 5 == ultim / 5) return;
            ultim = pc;
            juce::MessageManager::callAsync ([pc] { if (enProgres()) enProgres() ("Baixant el banc General MIDI: "_ca + juce::String (juce::jmin (pc, 99)) + " %"); });
        }

        void finished (juce::URL::DownloadTask* t, bool ok) override
        {
            const bool bo = ok && ! t->hadError() && temporal.getSize() > 100000000;
            juce::MessageManager::callAsync ([this, bo]
            {
                tasca.reset();
                if (! bo)
                {
                    temporal.deleteFile();
                    avisa ("No s'ha pogut baixar el banc General MIDI. Mira la connexió i torna-ho a provar, o posa un .sf2 a "_ca
                           + carpeta().getFullPathName());
                    return;
                }
                const auto desti = carpeta().getChildFile (nomFluid());
                desti.deleteFile();
                temporal.moveFileTo (desti);
                avisa ("Banc General MIDI a punt: "_ca + desti.getFullPathName());
                if (enBaixat()) enBaixat()();
            });
        }

        static void avisa (const juce::String& s) { if (enProgres()) enProgres() (s); }

        std::unique_ptr<juce::URL::DownloadTask> tasca;
        juce::File temporal;
        int ultim = -1;
    };

    /** Abans de posar un so General MIDI: si no hi ha banc, pregunta si es baixa. «fet» es crida igualment (el canvi
        d'instrument es fa; sonarà quan hi hagi el banc). */
    inline void assegura (juce::Component* sobre, std::function<void()> fet)
    {
        if (fet) fet();
        if (hiEs() || Baixada::una().enMarxa()) return;
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Cal el banc de sons General MIDI"_ca,
            "Els sons General MIDI surten d'un banc de sons (SoundFont). Mibase pot baixar el FluidR3 GM (148 MB, llicència MIT, "
            "de GitHub) a:\n"_ca + carpeta().getFullPathName() + "\n\nMentre baixa pots continuar treballant. Si en tens un altre (.sf2), "
            "també el pots posar en aquesta carpeta."_ca,
            "Baixa'l"_ca, "Ara no"_ca, sobre != nullptr ? sobre->getTopLevelComponent() : nullptr, juce::ModalCallbackFunction::create ([] (int r) { if (r == 1) Baixada::una().comenca(); }));
    }
}
