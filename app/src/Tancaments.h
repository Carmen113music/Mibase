#pragma once
// 0.83: si Mibase es tanca de cop, deixa escrit què estava fent i on (la pila de crides, amb el nom del mòdul: per
// exemple un plugin). A la propera arrencada surt a l'historial, i el diagnòstic ho ensenya sencer.
#include <juce_core/juce_core.h>
#include <cstring>

namespace tancaments
{
    inline juce::File carpeta()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase");
    }
    inline juce::File fitxer() { return carpeta().getChildFile ("darrer-tancament.txt"); }

    namespace detall
    {
        // Text fix (sense demanar memòria) perquè el gestor del tancament el pugui llegir encara que tot vagi malament.
        inline char pas[512] = "arrencant";
        inline char versio[32] = "";
    }

    /** Què està fent Mibase ara (es crida abans de les feines delicades: exportar, Print, obrir plugins…). */
    inline void pas (const juce::String& que)
    {
        const auto t = juce::Time::getCurrentTime().formatted ("%H:%M:%S ") + que;
        std::strncpy (detall::pas, t.toRawUTF8(), sizeof (detall::pas) - 1);
        detall::pas[sizeof (detall::pas) - 1] = 0;
    }

    inline void gestor (void*)
    {
        juce::String t;
        t << "Mibase " << detall::versio << " es va tancar de cop el " << juce::Time::getCurrentTime().formatted ("%d/%m/%Y a les %H:%M:%S") << "\n"
          << "Estava fent: " << juce::String::fromUTF8 (detall::pas) << "\n\n"
          << "Pila de crides (el mòdul de cada línia diu de qui és el codi: Mibase.exe, un plugin…):\n"
          << juce::SystemStats::getStackBacktrace();
        carpeta().createDirectory();
        fitxer().replaceWithText (t, false, false, "\r\n");
    }

    inline void instal_la (const char* versioText)
    {
        std::strncpy (detall::versio, versioText, sizeof (detall::versio) - 1);
        juce::SystemStats::setApplicationCrashHandler (gestor);
    }

    /** El text del darrer tancament inesperat (buit si no n'hi ha cap) i quan va ser. */
    inline juce::String darrer() { return fitxer().existsAsFile() ? fitxer().loadFileAsString() : juce::String(); }

    /** Si n'hi ha un de nou des de la darrera arrencada: true (un cop). */
    inline bool nou()
    {
        const auto f = fitxer();
        if (! f.existsAsFile()) return false;
        const auto marca = carpeta().getChildFile ("darrer-tancament.vist");
        if (marca.existsAsFile() && marca.getLastModificationTime() >= f.getLastModificationTime()) return false;
        marca.replaceWithText ("vist");
        return true;
    }
}
