#pragma once
// 0.43: guitarres en SoundFont (.sf2). Els fitxers van a Documents\\Mibase\\SoundFonts\\Guitarres (no al repositori:
// són grans i d'altres autors). Cada pista tria un so d'un fitxer; el banc només es carrega quan una pista el fa sonar.
#include "BancGM.h"
#include <map>

namespace guitarres
{
    inline juce::File carpeta() { return bancgm::carpeta().getChildFile ("Guitarres"); }

    /** Nom per ensenyar: «FS-Jaguar-I5-HedSound.sf2» → «Jaguar». */
    inline juce::String nomGuitarra (const juce::String& fitxer)
    {
        auto n = juce::File::createLegalFileName (fitxer).upToLastOccurrenceOf (".", false, false).replaceCharacters ("_-", "  ");
        for (const char* fora : { "HedSound", "Hedsound", " I5", "FS ", " O ", "EGuitarFSBS" })
            n = n.replace (fora, " ");
        while (n.isNotEmpty() && juce::CharacterFunctions::isDigit (n.getLastCharacter())) n = n.dropLastCharacters (1);   // la data del final
        n = n.trim();
        while (n.contains ("  ")) n = n.replace ("  ", " ");
        return n.isEmpty() ? fitxer : n;
    }

    /** D'un instrument «sf2:…», el nom de la guitarra («Jaguar») o el del so («-chorus»). Buit si no ho és. */
    inline juce::String part (const std::string& instrument, bool so)
    {
        mibase::gm::RefSf2 r;
        if (! mibase::gm::llegeixRef (instrument, r)) return {};
        const auto nom = juce::String::fromUTF8 (r.nom.c_str());
        const auto sep = juce::String::fromUTF8 (" \xc2\xb7 ");
        if (! nom.contains (sep)) return so ? nom : nomGuitarra (juce::String::fromUTF8 (r.fitxer.c_str()));
        return so ? nom.fromFirstOccurrenceOf (sep, false, false) : nom.upToFirstOccurrenceOf (sep, false, false);
    }

    struct Entrada { juce::String fitxer; mibase::gm::InfoPreset preset; };
    inline std::vector<Entrada>& darreraLlista() { static std::vector<Entrada> v; return v; }

    /** Submenú amb una guitarra per fitxer i els seus sons. Ids: base + posició a darreraLlista(); base + 4999 = obre la carpeta. */
    inline juce::PopupMenu menu (int base, const std::string& actual)
    {
        juce::PopupMenu m;
        auto& llista = darreraLlista();
        llista.clear();
        mibase::gm::RefSf2 ara;
        const bool teRef = mibase::gm::llegeixRef (actual, ara);
        auto fitxers = carpeta().findChildFiles (juce::File::findFiles, false, "*.sf2");
        fitxers.sort();
        for (auto& f : fitxers)
        {
            juce::PopupMenu sub;
            bool aquest = false;
            for (auto& p : mibase::gm::presetsDeFitxer (f.getFullPathName().toStdString()))
            {
                auto nom = juce::String::fromUTF8 (p.nom.c_str()).trim();
                if (nom.startsWithChar ('^') || nom.containsIgnoreCase ("Fret Noise")) continue;   // separadors i sorolls
                while (nom.startsWithChar ('-')) nom = nom.substring (1).trim();
                const bool triat = teRef && ara.fitxer == f.getFileName().toStdString() && ara.banc == p.banc && ara.programa == p.programa;
                aquest = aquest || triat;
                auto q = p;
                q.nom = nom.toStdString();
                sub.addItem (base + (int) llista.size(), nom, true, triat);
                llista.push_back ({ f.getFileName(), q });
            }
            if (sub.getNumItems() > 0) m.addSubMenu (nomGuitarra (f.getFileName()), sub, true, juce::Image(), aquest);
        }
        if (llista.empty())
            m.addItem (-1, juce::String::fromUTF8 ("Encara no hi ha cap guitarra: posa els .sf2 a Documents\\Mibase\\SoundFonts\\Guitarres"), false);
        m.addSeparator();
        m.addItem (base + 4999, juce::String::fromUTF8 ("Obre la carpeta de les guitarres"));
        return m;
    }

    /** L'instrument («sf2:…») de la tria «index» del darrer menú; buit si no és un so (o si era «obre la carpeta», que ja la obre). */
    inline std::string tria (int index)
    {
        if (index == 4999) { carpeta().createDirectory(); carpeta().revealToUser(); return {}; }
        const auto& l = darreraLlista();
        if (index < 0 || index >= (int) l.size()) return {};
        mibase::gm::RefSf2 r;
        r.fitxer = l[(size_t) index].fitxer.toStdString();
        r.banc = l[(size_t) index].preset.banc;
        r.programa = l[(size_t) index].preset.programa;
        r.nom = (nomGuitarra (l[(size_t) index].fitxer) + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String::fromUTF8 (l[(size_t) index].preset.nom.c_str())).toStdString();
        return mibase::gm::instrumentSf2 (r);
    }

    inline std::string signatura (const std::string& fitxer)
    {
        const auto f = carpeta().getChildFile (juce::String::fromUTF8 (fitxer.c_str()));
        return f.existsAsFile() ? (f.getFullPathName() + "|" + juce::String (f.getLastModificationTime().toMilliseconds())).toStdString() : std::string();
    }

    /** El banc d'un fitxer, llegit un sol cop (i tornat a llegir si el fitxer canvia). nullptr si no hi és. Fil de la interfície. */
    inline std::shared_ptr<mibase::gm::Banc> banc (const std::string& fitxer)
    {
        struct Cau { std::shared_ptr<mibase::gm::Banc> b; std::string sig; };
        static std::map<std::string, Cau> cau;
        const auto sig = signatura (fitxer);
        if (sig.empty()) return nullptr;
        auto& c = cau[fitxer];
        if (c.b == nullptr || c.sig != sig)
        {
            juce::MemoryBlock dades;
            const auto f = carpeta().getChildFile (juce::String::fromUTF8 (fitxer.c_str()));
            c.b = f.loadFileAsData (dades) ? mibase::gm::llegeixMemoria (dades.getData(), (int) dades.getSize()) : nullptr;
            c.sig = sig;
        }
        return c.b;
    }
}
